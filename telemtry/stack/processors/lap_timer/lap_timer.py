import json
import os
import logging
from time import sleep
import time
import threading
import requests
from kafka import KafkaConsumer
import json

from analysis.sql_utils.db_session import get_db
from analysis.sql_utils.query_builder import QueryBuilder
from stack.ingest.mqtt_handler import MQTTHandler, MQTTTarget
from analysis.sql_utils.models import Classifier, Event


class LapTimerProcessor:
    def __init__(self, session=None):
        self.session = session
        self.event_id: int = None
        self.gate: tuple[tuple[float], tuple[float]]  = None
        self.status: int = None
        
        self.table_specs = QueryBuilder("Nightwatch").get_table_column_specs()
        
        # Initialize Kafka Consumer
        self.consumer = KafkaConsumer(
            'track-mapper',
            bootstrap_servers='kafka:9092',
            auto_offset_reset='latest',
            value_deserializer=lambda x: json.loads(x.decode('utf-8'))
        )
        self.last_point = None

    def _is_intersection(self, line1: tuple[tuple[float, float], tuple[float, float]], line2: tuple[tuple[float, float], tuple[float, float]]) -> bool:
        """
        Check if a line intersects with another line segment 
        """
        # Calculate the denominator of the intersection formula
        denominator = (line1[0][1] - line1[1][1]) * (line2[0][0] - line2[1][0]) - (line1[0][0] - line1[1][0]) * (line2[0][1] - line2[1][1])

        # If the denominator is zero, the lines are parallel
        if denominator == 0:
            return False  # Parallel lines

        # Calculate t and u
        t = ((line1[0][1] - line2[0][1]) * (line2[0][0] - line2[1][0]) - (line1[0][0] - line2[0][0]) * (line2[0][1] - line2[1][1])) / denominator
        u = ((line1[0][1] - line2[0][1]) * (line1[0][0] - line1[1][0]) - (line1[0][0] - line2[0][0]) * (line1[0][1] - line1[1][1])) / denominator

        # Check if t and u are between 0 and 1 (i.e., the intersection occurs within the segments)
        return 0 <= t <= 1 and 0 <= u <= 1
    
    def _is_valid(self, time:int, delta: int):
        if self.event_id == None:
            return False

        last_lap = self.session.query(Classifier.start_time).filter(Classifier.event_id == self.event_id).order_by(Classifier.start_time.desc()).first()
        if not last_lap:
            return True
        return time - last_lap[0] > delta
        
    
    def _record_time(self, time: int):
        if self.event_id == None:
            return
        
        db_obj = {
            "event_id": self.event_id,
            "type": "lap",
            "start_time": time
        }
        
        # Start time
        if self.status != 1:
            event = self.session.query(Event).filter(Event.event_id == self.event_id).first()
            if event:
                event.status = 1
                event.start_time = time

        table_desc = self.table_specs[Classifier.__tablename__]
        QueryBuilder.insert(self.session, 'classifier', Classifier, db_obj, table_desc, commit=True)

        try:
            # requests.post("http://host.docker.internal:5000/webtool/new_lap", data={"time": time})  #! DIDN'T WORK ON PROD
            requests.post("https://lhrelectric.org/webtool/new_lap", data={"time": time})
        except requests.exceptions.ConnectionError:
            logging.error("Could not connect to Flask server")
        logging.info(f"Successfully recorded time {time}")    
    
    def _upload_gates_to_db(self, gates: tuple[tuple[float, float], tuple[float, float]]):
        retries = 5
        for i in range(retries):
            event = self.session.query(Event).filter(Event.event_id == self.event_id).first()
            if event:
                break
            logging.warning(f"Event {self.event_id} not found, retrying...")
            sleep(1)
        else:
            logging.error(f"Event {self.event_id} not found after {retries} retries, cannot upload gates.")
            return

        db_obj = {
            "event_id": self.event_id,
            "type": "gate",
            "notes": f"{gates[0][0]}_{gates[0][1]}_{gates[1][0]}_{gates[1][1]}",
            "start_time": time.time() * 1000
        }
        table_desc = self.table_specs[Classifier.__tablename__]
        QueryBuilder.insert(self.session, 'classifier', Classifier, db_obj, table_desc, commit=True)
        
        logging.info("Published gates to classifier", db_obj)
   
    def run_thread(self):
        """Kafka Consumer Loop"""
        logging.info("Listening to track-mapper topic...")
        for message in self.consumer:
            # Event not properly set
            if not self.event_id or not self.gate:
                continue
            
            # message.value is [lat, lon, timestamp]
            point_data = message.value
            if not point_data or len(point_data) < 3:
                continue
            
            current_point = (point_data[0], point_data[1])
            current_time = point_data[2]
            
            if self.last_point:
                segment = (self.last_point[0], current_point)
                
                if self._is_intersection(self.gate, segment):
                    lap_time = round((self.last_point[1] + current_time) / 2)
                    logging.info(f"Lap detected! Time: {lap_time}")
                    
                    if self._is_valid(lap_time, 5000):
                        logging.info(f"Lap Time is Valid")
                        self._record_time(lap_time)
                    else:
                        logging.warning(f"Lap Time is Not Valid")
            
            self.last_point = (current_point, current_time)
        
        
    def on_message(self, client, userdata, msg):
        try:
            logging.info(f"MESSAGE HAS BEEN RECEIVED AT TOPIC {msg.topic}")
            if msg.topic == 'config/test':
                data = json.loads(msg.payload.decode())
                logging.debug(data)

                gate_changed = self.gate != data['gate']

                # Modify object variables
                self.event_id = data['event_id']
                self.status = data['status']
                self.gate = data['gate']

                if gate_changed:
                    self._upload_gates_to_db(gates=data['gate'])
        except json.JSONDecodeError:
            self.event_id = None
            self.gate = None
            self.status = None
    

        
def run_processor():
    with get_db("Nightwatch") as nightwatch_session, get_db("Angelique") as angelique_session:
        db_sessions = {'Nightwatch': nightwatch_session, 'Angelique': angelique_session}
        with MQTTHandler(name="lap_timer_processor", target=MQTTTarget.get(), db_sessions=db_sessions) as mqtt:
        
            session = db_sessions["Nightwatch"]
            processor = LapTimerProcessor(session=session)
            
            # Processing thread
            t1 = threading.Thread(target=processor.run_thread)
            t1.start()
            
            mqtt.client.on_message = processor.on_message
            mqtt.subscribe("config/test")