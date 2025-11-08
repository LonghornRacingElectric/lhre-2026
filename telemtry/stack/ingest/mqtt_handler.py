import datetime
import os
import logging
import json
import pickle
import base64
import sys
import time
import numpy as np
import copy
from paho.mqtt import client as mqtt_client
from google.protobuf.json_format import MessageToDict
from pathlib import Path

sys.path.append(str(Path(__file__).parents[2]))

from analysis.sql_utils.db_session import get_db
from analysis.sql_utils.query_builder import QueryBuilder
from stack.ingest.protobuf import template_pb2

from kafka import KafkaProducer

# Determine path to net_configs.json based on execution context
if os.getenv("IN_DOCKER"):
    # Inside docker, path is absolute from the /app directory
    net_config_path = "/app/net_configs.json"
else:
    # For local execution, construct path relative to this file's location
    net_config_path = Path(__file__).parents[2] / "net_configs.json"

with open(net_config_path, "r") as file:
    global_target = json.load(file)

class MQTTTarget:
    @staticmethod
    def get():
        return 'mosquitto' if os.environ.get("IN_DOCKER") else global_target["TARGETS"][global_target["SERVER_TARGET"]]


class MQTTHandler:
    '''
    This class handles MQTT payloads: connecting to MQTT broker and publishing or subscribing to topics
    '''

    def __init__(self, name='python_client', target=None, db_sessions=None, on_message=None, cache_enable = False):
        '''
        :param name:    str         determining name of client to self-report to MQTT broker
        :param target:  MQTTTarget  MQTT target server
        '''
        self.target = target
        self.sessions = db_sessions
        self.client = mqtt_client.Client(name)
        self.client.username_pw_set(name)
        self.client.on_connect = self.on_connect
        self.client.on_disconnect = self.on_disconnect
        self.client.on_message = on_message if on_message else self.on_message
        self.scalar_or_list = lambda val, scalar: val.tolist()[0] if scalar else val.tolist()
        self.cache = []
        self.cache_enable = cache_enable
        self.table_specs = {
            "Nightwatch": QueryBuilder("Nightwatch").get_table_column_specs(),
            "Angelique": QueryBuilder("Angelique").get_table_column_specs()
        }

        # Kafka logic
        self.kafka_producer = KafkaProducer(
            bootstrap_servers='kafka:9092',
            value_serializer=lambda v: json.dumps(v).encode('utf-8')
        )

    @staticmethod
    def on_connect(client: mqtt_client.Client, userdata, flags: dict, rc: int):
        '''
        Function called when MQTT client connects or fails to connect.
        '''
        if rc:
            logging.error(f'Failed to connect to Mosquitto Broker, return code {rc}\n')
        else:
            logging.info(f'\t\t{client._client_id} connected to Mosquitto Broker')

    @staticmethod
    def on_disconnect(client: mqtt_client.Client, userdata, rc: int):
        '''
        Function called when MQTT client disconnects.
        '''
        if rc != 0:
            print(f'Unexpected MQTT disconnection. Return code: {rc}')

    def connect(self, ip=None):
        '''
        Connect to the MQTT broker. Priority in this order: argument --> class variable --> local.

        :param ip:      str indicating IP of MQTT broker

        :return:        mqtt_client.Client object
        '''
        self.client.connect(ip if ip else self.target if self.target else 'mosquitto' if os.getenv('IN_DOCKER') else 'localhost')
        return self.client

    def disconnect(self):
        self.client.disconnect()

    def __enter__(self):
        '''
        Enables 'with MQTTHandler(<name>, MQTTTarget.LOCAL) as mqtt:' logic which auto disconnects created client
        irrespective of errors. Class target preferred to local
        '''
        self.client.connect(self.target if self.target else 'mosquitto' if os.getenv('IN_DOCKER') else 'localhost')
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.client.disconnect()

    def subscribe(self, topic: str = '#'):
        self.client.subscribe(topic)
        logging.info(f"Topic: {topic}")
        self.client.loop_forever()

    def publish(self, *args, **kwargs):
        self.client.publish(*args, **kwargs)

    def on_message(self, client: mqtt_client.Client, userdata, msg):
        # Handle Start & End Event
        if msg.topic == 'config/flask':
            self._flask_handler(msg.payload.decode())
        # Handle Time Handshake
        elif msg.topic == 'config/car':
            os.environ['RTC_START'] = str(datetime.datetime.strptime(msg.payload.decode(), "%Y-%m-%dT%H:%M:%S.%f").timestamp() * 1000)
        # Handle Angelique-style Base64 Encoded Bytes
        elif (freq := msg.topic.rsplit('/')[-1]) in ['h', 'l']:
            self._b64_ingest(msg.payload, freq)
        # Handle Normal Data Ingest
        elif (topic_split := msg.topic.split('/'))[0] == 'data':
            if (topic_split[-1] in {'packet', 'dynamics', 'controls', 'pack', 'diagnostics_high', 'diagnostics_low', 'thermal'}):
                self._data_ingest(msg.payload, topic_split[-1], cache_enable=self.cache_enable, car = "Nightwatch")
            else:
                # Protobuf serialized string sent
                # TODO: this method right now uses flush to wait, which may be slowing us
                self.send_kafka_protobuf(payload=msg.payload)
                self._proto_ingest(payload=msg.payload, cache_enable=self.cache_enable)
        elif (topic_split := msg.topic.split('/'))[0] == 'angelique':
            if topic_split[-1] in self.table_specs["Angelique"]:
                self._data_ingest(msg.payload, topic_split[-1], cache_enable=self.cache_enable, car="Angelique")
            else:
                self._proto_ingest(payload=msg.payload, cache_enable=self.cache_enable, car="Angelique")
        else:
            logging.warning(f'No corresponding topic found for {msg.topic}')

    def send_kafka_protobuf(self, payload: str):
        '''
        This function sends the protobuf encoded message to a Kafka topic.

        :param payload:     str         protobuf encoded payload string
        '''

        # Send the message to Kafka topic 'sensor_data'
        self.kafka_producer.send('sensor_data', value=payload)
        self.kafka_producer.flush()

        
    def _flask_handler(self, payload):
        '''
        This function oversees the decoding and handling of Flask messages usually related to configuration or metadata.

        :param payload:     str         payload string containing configuration information
        '''
        try:
            event_id = json.loads(payload)['event_id']
            logging.info(f'\tNow logging data for event: {event_id}...')
            os.environ['EVENT_ID'] = str(event_id)
        except json.JSONDecodeError:
            if payload == 'end_event':
                event_id = os.environ['EVENT_ID']
                del os.environ['EVENT_ID']
                try:
                    del os.environ['RTC_START']
                    logging.info(f'\tEnding logging for event {event_id}...')
                except KeyError:
                    logging.info(f'\tEnding logging for event {event_id} despite RTC_START never being set...')
            else:
                logging.error(f'\tUnexpected payload received: {payload}')

    def cache_flush(self, car):
        if (len(self.cache) > 0):
            session = self.sessions[car]
            session.bulk_insert_mappings(self.cache[0][0], [item[1] for item in self.cache])
            session.commit()

    def _data_ingest(self, payload: str, table: str, car:str, cache_enable = False):
        '''
        This function oversees the decoding and insertion of simply packaged, fully processed payloads.

        :param payload:     str         pickle or JSON encoded payload con
        :param table:       str         destination table name
        '''
        if not os.getenv('EVENT_ID'):
            logging.error(f'\tAttempt made to send data without an event_id cached.')
        try:
            data_dict = pickle.loads(payload)
            #logging.debug('\tPickle Payload received, likely coming from debug source...')
        except pickle.UnpicklingError:
            data_dict = json.loads(payload.decode().replace("'", '"'))

        session = self.sessions[car]
        model = QueryBuilder(car)._models.get(table.capitalize())

        if model:
            table_desc = self.table_specs[car][model.__tablename__]
            if isinstance(data_dict, list):
                if len(data_dict) > 1:
                    QueryBuilder.bulk_insert(session, table, model, data_dict, table_desc, commit=True)
                else:
                    QueryBuilder.insert(session, table, model, data_dict[0], table_desc, commit=True)
            elif not cache_enable:
                QueryBuilder.insert(session, table, model, data_dict, table_desc, commit=True)
    
    def _proto_ingest(self, payload:str, cache_enable = False, car = "Nightwatch"):
        message_dict = self._proto_decode(payload=payload, car=car)

        if ("time" not in message_dict or "packet_id" not in message_dict):
            raise Exception("time/packet_id MISSING FROM PAYLOAD")
        
        session = self.sessions[car]
        builder = QueryBuilder(car)
        table_specs = self.table_specs[car]






        for table in table_specs.keys():
            model = builder._models.get(table.capitalize())
            if model:
                table_desc = table_specs[model.__tablename__]
                data = {col.name: message_dict[col.name] for col in model.__table__.columns if col.name in message_dict} if table == "packet" else None
                if (data is None):
                    data = ({col.name: message_dict[table][col.name] for col in model.__table__.columns if col.name in message_dict[table]}
                    | {"packet_id": message_dict["packet_id"]}) if table in message_dict else None

                if data is not None:
                    if not cache_enable:
                        QueryBuilder.insert(session, table, model, data, table_desc, commit=False)
                    elif table == 'packet':
                        QueryBuilder.insert(session, table, model, data, table_desc, commit=False)
                    elif table != 'packet':
                        self.cache.append((model, data))
                        if (len(self.cache) == 24):
                            self.cache_flush(car)
                            self.cache.clear()
        session.commit()


    def _b64_ingest(self, payload: str, high_freq: bool):
        '''
        This function oversees the decoding, preprocessing, and insertion of base64 formatted bytes data.

        :param payload:     str         base64 encoded bytes payload string
        :param high_freq:   bool        switch controlling which frequency message to expect
        '''
        if not os.getenv('EVENT_ID'):
            logging.error(f'\tAttempt made to send data without an event_id cached.')

        logging.info(f'\tData received. Inserting to Database now...')
        data_dict = self._base64_decode(payload, high_freq)
        data_dict = self.preprocess_payload(data_dict, high_freq)

        session = self.sessions["Angelique"]
        builder = QueryBuilder("Angelique")
        table_specs = self.table_specs["Angelique"]

        for table in ['packet', 'angelique_dynamics', 'angelique_controls', 'angelique_pack', 'angelique_diagnostics', 'angelique_thermal']:
            model = builder._models.get(table.capitalize())
            if model:
                table_desc = table_specs[model.__tablename__]
                data = {col.name: data_dict[col.name] for col in model.__table__.columns if col.name in data_dict}
                if data:
                    QueryBuilder.insert(session, table, model, data, table_desc, commit=False)
                else:
                    logging.warning(f'\tNo data received for {table}...')
        session.commit()

    @staticmethod
    def _proto_decode(self, payload: str) -> dict:
        logging.info('Data Received via Protobuf')
        row = template_pb2.SensorData()
        row.ParseFromString(payload)
        row = MessageToDict(row, preserving_proto_field_name=True, always_print_fields_with_no_presence=True)
        logging.debug(row)
        return row

    def _base64_decode(self, payload: str, high_freq: bool) -> dict:
        '''
        This function handles the decoding of base64 payloads into a dictionary. Implemented for Angelique, script first
        decrypts base64 payload into string of bytes and converts to bytearray. Then, using the pre-made car configs
        (car_configs/versionXX.json) formatting schema to unpack the data, a final dictionary is populated.

        :param payload:     str         base64 encoded bytes payload string
        :param high_freq:   bool        switch controlling which frequency decoding schema to use from versionXX.json

        :return:            dict        dictionary composed of schema "column" names and corresponding data
        '''
        bytes_data = bytearray(base64.b64decode(payload))

        # Pulls car_config for given version
        with open(f'car_configs/version{bytes_data[0]:02}.json', 'r') as file:
            car_config = json.load(file)['high' if high_freq else 'low']

        output = {}
        for col, desc in car_config.items():
            # Some "columns" are interpreted directly as binary (error switches, etc.)
            if col in ['vcu_flags', 'current_errors', 'latching_faults']:
                # Bytearray fields must be directly interpreted as binary
                output[col] = bin(int.from_bytes(bytes_data[desc['indices'][0]:desc['indices'][1]], byteorder='big'))[2:]
            else:
                # Normal fields have their field pulled from the car_config, make relevant conversions, and set column
                output[col] = self.scalar_or_list(np.frombuffer(bytes_data[desc['indices'][0]:desc['indices'][1]],
                                                                count=np.prod(desc.get('shape', -1)), dtype=desc['type']
                                                           ) * desc.get('multiplier', 1), not bool(desc.get('shape')))
        return output

    def preprocess_payload(self, payload: dict, high_freq=True) -> dict:
        '''
        This function--built for Angelique and unedited since--handles depackage and preprocessing of payload data

        :param payload:     dict        data payload composed of column names and corresponding values
        :param high_freq:   bool        switch to treat payload as high frequency and depackage accordingly

        :return:            dict        processed payload
        '''
        try:
            payload['time'] = int(float(os.environ['RTC_START']) + payload['since_rtc'])
            del payload['since_rtc']
        except KeyError:
            raise KeyError('RTC Start was not set.')
        if 'packet_id' not in payload:
            payload['packet_id'] = next(self.counter)
        if high_freq:
            payload['gps'] = tuple(val / 60 for val in payload['gps'])
            payload['vcu_flags_json'] = {
                'inverter_on': bool(int(payload['vcu_flags'][0])),
                'r2d_buzzer_on': bool(int(payload['vcu_flags'][1])),
                'brake_light_on': bool(int(payload['vcu_flags'][2])),
                'drs_on': bool(int(payload['vcu_flags'][3])),
                'apps_fault': bool(int(payload['vcu_flags'][4])),
                'bse_fault': bool(int(payload['vcu_flags'][5])),
                'stompp_fault': bool(int(payload['vcu_flags'][6])),
                'steering_fault': bool(int(payload['vcu_flags'][7]))
            }

            # IMU Acceleration
            payload['body1_accel'] = payload['imu_accel'][:3]
            payload['body2_accel'] = payload['imu_accel'][3:6]
            payload['body3_accel'] = payload['imu_accel'][6:9]
            payload['flw_accel'] = payload['imu_accel'][9:12]
            payload['frw_accel'] = payload['imu_accel'][12:15]
            payload['blw_accel'] = payload['imu_accel'][15:18]
            payload['brw_accel'] = payload['imu_accel'][18:21]
            del payload['imu_accel']

            # IMU Gyro
            payload['body1_gyro'] = payload['imu_gyro'][:3]
            payload['body2_gyro'] = payload['imu_gyro'][3:6]
            payload['body3_gyro'] = payload['imu_gyro'][6:9]
            del payload['imu_gyro']

            # Wheel Speed
            payload['flw_speed'] = payload['wheel_speed'][0]
            payload['frw_speed'] = payload['wheel_speed'][1]
            payload['blw_speed'] = payload['wheel_speed'][2]
            payload['brw_speed'] = payload['wheel_speed'][3]
            del payload['wheel_speed']

        return payload


def main():
    '''
    This is the runner script for the subscribe-side MQTT script which uploads data to the database.
    Whether to use safe, unsafe, or connection pool DBHandler is determined by DB_CONN_TYPE environment variable
    and defaults to unsafe. To use a connection pool, set it to the desired connection pool size.

    Options:
        1   Runs MQTT Ingest server with safe DBHandler
        2   Runs MQTT Ingest server with unsafe DBHandler
        3+  Runs MQTT Ingest server with unsafe DBHandler using connection pool of size of arg
    '''
    try:
        conn_type = int(os.getenv('DB_CONN_TYPE', 2))
        if not 0 < conn_type < 11:
            raise ValueError
    except ValueError:
        raise ValueError('DB_CONN_TYPE must be an integer 1-10.')

    with get_db("Nightwatch") as nightwatch_session, get_db("Angelique") as angelique_session:
        db_sessions = {'Nightwatch': nightwatch_session, 'Angelique': angelique_session}
        with MQTTHandler('ingest', db_sessions=db_sessions) as mqtt:
                mqtt.subscribe(topic='#')


if __name__ == '__main__':
    logging.basicConfig(level=os.getenv('LOGLEVEL', 'DEBUG'))
    if logging.root.level == logging.DEBUG:
        time.sleep(3)
        logging.debug('-' * 40 + '\n\n\t\tYOU ARE IN DEBUGGING MODE\n\n ' + '-' * 50)
        os.environ['RTC_START'] = "-99999"
        os.environ['EVENT_ID'] = "-99999"
    main()