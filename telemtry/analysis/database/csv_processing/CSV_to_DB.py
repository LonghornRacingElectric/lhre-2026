import warnings
warnings.filterwarnings("ignore")
import pandas as pd
import pickle
import time
import logging
import datetime
import os
import sys
import json
import argparse
import matplotlib.pyplot as plt
import numpy as np
from concurrent.futures import ThreadPoolExecutor, as_completed, wait
from multiprocessing import cpu_count
from pathlib import Path
from tqdm import tqdm
from queue import Queue
import requests

REALTIME = True

sys.path.append(str(Path(__file__).parents[3]))

from analysis.sql_utils.db_session import get_db, DBTarget
from analysis.sql_utils.models import (
    Packet,
    Event,
    DriveDay,
    Partitions,
)
from analysis.sql_utils.query_builder import QueryBuilder
from stack.ingest.mqtt_handler import MQTTHandler, MQTTTarget
from analysis.database.csv_processing.proto_builders import (
    build_angelique_sensor_data_proto,
    build_orion_sensor_data_proto,
)

class CSVToDB():

    def __init__(self, data_csv_folder = None, mqtt = None, db_session = None, car = "Nightwatch", MQTT_target = MQTTTarget.get()):
        self.db_session = db_session
        self.MQTT_target = MQTT_target
        self.mqtt = mqtt
        # Lazily initialized from DB on first use, then incremented locally.
        self.last_packet = None
        self.car = car
        self._models = QueryBuilder(car)._models
        self._table_models = {model.__tablename__: model for model in self._models.values()}
        if (data_csv_folder != None):
            try:
                self.data_csv_folder = sorted(list(Path(__file__).parent.joinpath("csv_data", data_csv_folder).glob("*.csv")))
            except FileNotFoundError:
                raise FileNotFoundError("NO CSV FILES WERE FOUND UNDER csv_data/\nCREATE csv_data/ and add csv files within the folder.\n")
        else:
            logging.warning("NO CSV FOLDER WAS PASSED\n")

    def event_seperator(self, threshold = 20, speed_filter = False):
        """"
        The class variable is a directory containing csv files from a certain drive day. The goal of this method is to 
        partition the folder into continous segments in which the car is running, outputting a list containing dataframes
        where each dataframe represents a near continous time period in which the car is running. 
        """

        csv_split = []
        df_current = pd.read_csv(self.data_csv_folder[0]) #Runs into atrribute error if data_csv_folder was not specified
        event = [df_current.copy(deep=False)]
        event_storage = Path(__file__).parent.joinpath("event_csv")

        if not os.path.exists(event_storage): 
            os.makedirs(event_storage) 
        
        if (len(self.data_csv_folder) == 1):
            df_current.to_csv(event_storage.joinpath("0.csv"))
            return

        for i in tqdm(range(len(self.data_csv_folder) - 1)):
            df_forw = pd.read_csv(self.data_csv_folder[i+1])

            last_dt_current = df_current.iloc[-1]
            dt_current = pd.to_datetime({"year" : [last_dt_current["Year"] + 2000], 
                "month" : [last_dt_current["Month"]], 
                "day" : [last_dt_current["Day"]], 
                "hour" : [last_dt_current["Hour"]], 
                "minute" : [last_dt_current["Minute"]], 
                "second" : [last_dt_current["Seconds"]], 
                "millisecond" : [last_dt_current["Milliseconds"]]}).astype(int) // 1000000 #gets into ms

            first_dt_forw = df_forw.iloc[0]
            dt_forw = pd.to_datetime({"year" : [first_dt_forw["Year"] + 2000], 
                "month" : [first_dt_forw["Month"]], 
                "day" : [first_dt_forw["Day"]], 
                "hour" : [first_dt_forw["Hour"]], 
                "minute" : [first_dt_forw["Minute"]], 
                "second" : [first_dt_forw["Seconds"]], 
                "millisecond" : [first_dt_forw["Milliseconds"]]}).astype(int) // 1000000 #gets into ms

            if (dt_forw[0] - dt_current[0] < threshold * 1000):
                differences = df_forw.Time - df_forw.Time.shift(fill_value=0)
                # df.time - df.time.shift(fill_value=0)
                df_forw["Time"][0] = df_current["Time"].iloc[-1] + ((dt_forw[0] - dt_current[0])//1000) #Find differing times
                df_forw["Time"] = df_forw["Time"].iloc[0] + differences.cumsum()
                df_current = df_forw
                event.append(df_forw.copy(deep=False))
            else:
                csv_split.append(pd.concat(event, ignore_index=True))
                event = [df_forw.copy(deep=False)]
                df_current = df_forw
                if (i == len(self.data_csv_folder) - 2):
                    csv_split.append(pd.concat(event))

        time_arrays = [np.array(entry["Time"]) for entry in csv_split]
        time_diff = [np.diff(time_array) for time_array in time_arrays]
        delays = [np.where(diff >= threshold)[0] for diff in time_diff]

        #TODO event separation using wheel speeds
        continous_event = []
        for i, (df, delay_indices) in enumerate(zip(csv_split, delays)):
            start_idx = 0
            for delay_idx in delay_indices:
                continous_event.append(df.iloc[start_idx:delay_idx])  
                start_idx = delay_idx 
            continous_event.append(df.iloc[start_idx:])

        del csv_split # Free memory here

        if (speed_filter):
            speed_threshold = 5
            std_threshold = 20
            for i in tqdm(range(len(continous_event) - 1, -1, -1)):
                flw_speed = continous_event[i]["Front Left Wheel Speed"].rolling(5, min_periods = 1).mean() #Smoothing in groups of 10
                frw_speed = continous_event[i]["Front Right Wheel Speed"].rolling(5, min_periods = 1).mean()
                blw_speed = continous_event[i]["Back Left Wheel Speed"].rolling(5, min_periods = 1).mean()
                brw_speed = continous_event[i]["Back Right Wheel Speed"].rolling(5, min_periods = 1).mean()
                time = continous_event[i]["Time"]

                #Gradient of Acceleration
                grad_flw = pd.Series(np.gradient(np.gradient(flw_speed, time), time)).rolling(40, min_periods = 1).std()
                grad_frw = pd.Series(np.gradient(np.gradient(frw_speed, time), time)).rolling(40, min_periods = 1).std()
                grad_blw = pd.Series(np.gradient(np.gradient(blw_speed, time), time)).rolling(40, min_periods = 1).std()
                grad_brw = pd.Series(np.gradient(np.gradient(brw_speed, time), time)).rolling(40, min_periods = 1).std()

                mask = (
                    ((flw_speed > speed_threshold) |
                    (frw_speed > speed_threshold) |
                    (blw_speed > speed_threshold) |
                    (brw_speed > speed_threshold)) &
                    ((grad_flw > std_threshold) |
                    (grad_frw > std_threshold) |
                    (grad_blw > std_threshold) |
                    (grad_brw > std_threshold)))
                
                filter = continous_event[i][mask]
                #continous_event[i] = filter #extract only high values
                if (len(filter.index) == 0 or filter["Time"].iloc[-1] - filter["Time"].iloc[0] < 30):
                    continous_event.pop(i)
        
        for i in range(len(continous_event)):
            continous_event[i].to_csv(event_storage.joinpath(f'{i}.csv'))
      
    def enumerate_packet_id(self, df):       
        """
        Enumerates packet_id from a single DB snapshot at run start, then increments locally.
        This avoids duplicate ids when setup/publish stages run concurrently.
        """
        if self.last_packet is None:
            try:
                packet_model = self._table_models.get("packet")
                if packet_model:
                    result = self.db_session.query(packet_model.packet_id).order_by(packet_model.packet_id.desc()).first()
                    self.last_packet = int(result[0]) if result else 0
                else:
                    self.last_packet = 0
            except (IndexError, TypeError):
                self.last_packet = 0

        pack_id = list(range(self.last_packet + 1, self.last_packet + 1 + len(df.index)))
        self.last_packet += len(df.index)
        return pack_id

    def set_last_packet_id(self, last_packet_id: int):
        """
        Manually sets the last inserted packet_id so the next insert starts at last_packet_id + 1.
        """
        self.last_packet = int(last_packet_id)

    def dataConvert(self, df, table_desc):
        """
        Converts data to the csv to a format that can be added into the database
        """
        convert = {}
        packet_ids = self.enumerate_packet_id(df)
        
        # For Orion, use direct column mapping (CSV columns are already DB field names)
        if self.car == "Orion":
            pg_to_csv = {col: col for col in df.columns}
        else:
            if self.car == "Angelique":
                with open(Path(__file__).parent.joinpath("angelique_pg_to_csv.json"), "r") as pg:
                    pg_to_csv = json.load(pg)
            else:
                with open(Path(__file__).parent.joinpath("pg_to_csv.json"), "r") as pg:
                    pg_to_csv = json.load(pg)

        for table in table_desc:
            convert[table] = {}
            for column, (dtype, ndim) in table_desc[table].items():
                # Angelique logs store GPS as Latitude/Longitude columns.
                if self.car == "Angelique" and column == "gps":
                    lat_col = next((c for c in df.columns if c.lower() in {"latitude", "lat"} or "latitude" in c.lower()), None)
                    lon_col = next((c for c in df.columns if c.lower() in {"longitude", "lon", "lng"} or "longitude" in c.lower()), None)
                    if lat_col and lon_col:
                        lat_vals = df[lat_col].astype(float).to_numpy()
                        lon_vals = df[lon_col].astype(float).to_numpy()
                        convert[table][column] = [[lat, lon] for lat, lon in zip(lat_vals, lon_vals)]
                        continue

                # Construct CSV column name (for Orion: table.column, others: from mapping)
                if self.car == "Orion":
                    csv_col_name = f"{table}.{column}"
                    col_exists = csv_col_name in df.columns
                else:
                    col_exists = column in pg_to_csv
                    csv_col_name = pg_to_csv.get(column, column)

                if column not in {"time", "packet_id", "f_gps", "b_gps"} and col_exists:
                    if ndim:
                        # Handle array columns
                        if self.car == "Orion":
                            # For Orion, arrays are stored as strings in CSV (e.g., "[]" or "[1.0, 2.0]")
                            import ast
                            convert[table][column] = [
                                ast.literal_eval(str(val)) if isinstance(val, str) else val 
                                for val in df[csv_col_name]
                            ]
                        else:
                            convert[table][column] = df[csv_col_name].astype(dtype).to_numpy().tolist()
                    else:
                        convert[table][column] = df[csv_col_name].astype(dtype)
                elif (column == "time"):
                    if self.car == "Orion":
                        # Orion CSV has a standalone 'time' column, not prefixed by table name
                        if 'time' in df:
                            convert[table][column] = df["time"].astype(int)
                        else:
                            csv_col_name = f"{table}.{column}"
                            convert[table][column] = df[csv_col_name].astype(int)
                    else:
                        df.Year += 2000
                        first_dt = df.iloc[0]
                        dt = pd.to_datetime({"year" : [first_dt["Year"]], 
                            "month" : [first_dt["Month"]], 
                            "day" : [first_dt["Day"]], 
                            "hour" : [first_dt["Hour"]], 
                            "minute" : [first_dt["Minute"]], 
                            "second" : [first_dt["Seconds"]], 
                            "millisecond" : [first_dt["Milliseconds"]]}).astype(int) // 1000000 #gets into ms
                        offset = df["Time"][0] * 1000 #get into ms
                        dt_list = ((dt[0] - offset) + (df["Time"] * 1000)).astype(int)
                        convert[table][column] = dt_list
                elif (column == "packet_id"):
                    convert[table][column] = packet_ids
                elif (dtype == "point"):
                    convert[table][column] = [tuple(row) for row in np.column_stack((df["Latitude"], df["Longitude"]))]
                else:
                    convert[table][column] = []

        return convert

    #Each iteration makes a new connection to the database and sends a singular row at a time
    def insert_row_from_csv(self, df, table_desc, num_rows : int):
        """
        Adds data into the database. First adds packet table, then subsequent table using DBHandler insert function. Each iteration
        only inputs a singular row to the database, making this program the least efficient
        """
        if self.car == "Nightwatch":
            raise Exception("This function is not supported for the Nightwatch database. ")
        data = self.dataConvert(df, table_desc=table_desc)
        packets = data["packet"]
        packet_model = self._table_models.get("packet")
        if not packet_model:
            raise ValueError(f"Packet model not found for car {self.car}")

        for i in tqdm(range(num_rows)):
            row_dict = {j : packets[j][i] for j in packets if (len(packets[j]) != 0)}
            self.db_session.add(packet_model(**row_dict))

        for i in tqdm(data.keys()):
            if (i not in {"packet", "event", "classifier", "drive_day", "lut_driver", "lut_location", "lut_car", "lut_event_type", "partitions"}):
                for j in range(num_rows):
                    row_dict = {k : data[i][k][j] for k in data[i] if (len(data[i][k]) != 0)}
                    model = self._table_models.get(i)
                    if model:
                        self.db_session.add(model(**row_dict))
        self.db_session.commit()


    def _insert_chunk_callback(self, future):
        try:
            future.result()
        except Exception as e:
            logging.error(f"An error occurred during insertion: {e}")

    def insert_multi_row_from_csv(self, df, table_desc, amt = 500, rebuild_partitions = True, partition_gap_seconds = 300):
        """
        Inserts data into the database in larger batches determined by the given amt value. 
        """
        # Setup data within method--------------------------------------------------------------------------------
        data = self.dataConvert(df, table_desc=table_desc)
        packets = data["packet"]
        packet_model = self._table_models.get("packet")
        if not packet_model:
            raise ValueError(f"Packet model not found for car {self.car}")

        def insert_chunk(start, end, packets, data):
            with get_db(self.car) as session:
                pack_list = [
                    {j: packets[j][i] for j in packets if len(packets[j]) != 0}
                    for i in range(start, end)
                ]
                QueryBuilder.bulk_insert(session, "packet", packet_model, pack_list, table_desc["packet"], commit=False)

                # Insert into other tables
                for table_name, values in data.items():
                    if not table_name in {"packet", "event", "classifier", "drive_day",
                                    "lut_driver", "lut_location", "lut_car", "lut_event_type", "partitions"}:
                        model = self._table_models.get(table_name)
                        if model:
                            row_list = [
                                {k: values[k][i] for k in values if len(values[k]) != 0}
                                for i in range(start, end)
                            ]
                            QueryBuilder.bulk_insert(session, table_name, model, row_list, table_desc[table_name], commit=False)
                session.commit()


        # Each worker checks out its own DB connection for the duration of a chunk.
        # The SQLAlchemy engine pool allows 15 connections (pool_size 5 + max_overflow 10),
        # so cap workers below that to avoid QueuePool timeouts on high-core machines.
        max_workers = min(cpu_count(), 10)

        futures = []
        with ThreadPoolExecutor(max_workers=max_workers) as executor:
            for j in tqdm(range(0, len(df.index), amt)):
                high = min(j + amt, len(df.index))

                # If too many futures are running, wait for one to finish
                while len(futures) >= max_workers:
                    done, not_done = wait(futures, return_when="FIRST_COMPLETED")
                    for f in done:
                        f.result()
                        futures.remove(f)  # remove finished one
                    futures = list(not_done)  # raise exception if any

                # Submit a new job
                f = executor.submit(insert_chunk, j, high, packets, data)
                f.add_done_callback(self._insert_chunk_callback)
                futures.append(f)

            # Wait for the rest to finish
            for f in as_completed(futures):
                f.result()

        if rebuild_partitions:
            self.csv_event_injection(time_threshold=partition_gap_seconds)

    def stream_data(self, df):
        for chunk in (pd.read_csv(df, chunksize=2000)):
            yield chunk.reset_index(drop = True)

    def event_playback(self, file_path, table_desc, time_adjustment = True, batch_amt = 1, protobuf = True):
        """
        Publishes to database based on time delays from the csv
        """
        tables = []
        if (self.car == "Nightwatch"):
            tables = ['packet', 'dynamics', 'controls', 'pack', 'diagnostics_high', 'diagnostics_low', 'thermal']
        elif (self.car == "Angelique"):
            tables = ['packet', 'dynamics', 'controls', 'pack', 'diagnostics', 'thermal']
        elif (self.car == "Orion"):
            tables = ['packet', 'dynamics', 'controls', 'pack', 'diagnostics_high', 'diagnostics_low', 'thermal', 'board_status']
        else:
            raise Exception("DBTarget not specified")
        
        def publish_row():
            """
            Takes data from a csv and sends data in rows to mqtt for event playback. Data is formatted as a dictionary and published
            to mqtt through small batches of rows
            """
            started = False
            
            
            while not done or not chunk_queue.empty():
                if (not chunk_queue.empty()):
                    differences, row_dict_list = chunk_queue.get()
                    chunk_length = len(differences)

                    if not started:
                        try:
                            self.start_timer(row_dict_list.get("packet")[0]['time'])
                            started = True
                        except Exception as e:
                            logging.error(e)

                    for i in range(chunk_length):
                        time.sleep((float(differences[i])/ 1000))
                        global_progress.update(1)
                        if self.car == "Angelique" and protobuf:
                            msg = build_angelique_sensor_data_proto(row_dict_list, i)
                            self.mqtt.publish('angelique', msg.SerializeToString(), qos=0)
                        elif self.car == "Orion" and protobuf:
                            msg = build_orion_sensor_data_proto(row_dict_list, i)
                            self.mqtt.publish('orion', msg.SerializeToString(), qos=0)
                        elif (i % batch_amt == 0):
                            for table in tables: #Through the different tables
                                end_index = min(i + batch_amt, chunk_length)  # Ensure we don't exceed the length
                                batch_data = row_dict_list.get(table)[i:end_index]
                                self.mqtt.publish(f'{self.car.lower()}/{table}', pickle.dumps(batch_data), qos=0)
                else:
                    time.sleep(0.1)
            if started:
                self.stop_timer()

        def setup_rows(df, table_desc, time_adjustment):
            """"
            Formats the rows in the form of dictionaries to be sent into database
            """
            data = self.dataConvert(df, table_desc=table_desc)
            differences = []
            times = np.array(data["packet"]["time"])
            differences = np.insert(np.diff(times), 0, 0)
            if (time_adjustment):
                data["packet"]["time"] = (current_time[0] + np.cumsum(differences)).tolist()
                current_time[0] =  int(current_time[0] + np.sum(differences))

            row_dict_list = {}
            for i, values in data.items():
                row_list = []
                if (i not in {"event", "classifier", "drive_day", "lut_driver", "lut_location", "lut_car", "lut_event_type", "partitions"}):
                    model = self._table_models.get(i)
                    if model:
                        row_list = [{k : values[k][j] for k in values if (len(values[k]) != 0)} for j in range(len(df.index))]
                    row_dict_list[i] = row_list
            return differences, row_dict_list
    
        with ThreadPoolExecutor(max_workers=1) as setup_executor:
            with ThreadPoolExecutor(max_workers=1) as publish_executor:
                with open(file_path, "r") as f:
                    row_count = sum(1 for _ in f) - 1
                global_progress = tqdm(total = row_count, desc="Row Publishing Progress")
                setup_future = None
                chunk_queue = Queue()
                done = False
                current_time = [int(datetime.datetime.timestamp(datetime.datetime.now())*1000)]
                try:
                    publish_future = publish_executor.submit(publish_row)
                    for df in self.stream_data(file_path):
                        if setup_future:
                            setup_future.result()
                        setup_future = setup_executor.submit(setup_rows, df, table_desc, time_adjustment)

                        differences, row_dict_list = setup_future.result()
                        chunk_queue.put((differences, row_dict_list))
                    done = True
                    if setup_future:
                        setup_future.result()

                    publish_future.result() 
                except KeyboardInterrupt:
                    setup_executor.shutdown(False)
                    publish_executor.shutdown(False)
                    
    def handle_event_start(self):
        # ---- START EVENT ----
        try:
            result = self.db_session.query(Event.event_id).filter(Event.status == 1).order_by(Event.event_id.desc()).first()
            if result is None:
                raise ValueError("No running event found.")
            event_id = result[0]
            logging.info("EVENT ID: %s", event_id)
            if event_id == -1: raise Exception("No event is currently running")
        # Creating a new event
        except (ValueError, TypeError, Exception) as e:
            logging.info("No running event found, creating a new one.")
            
            sample_drive_day = {
                'date': datetime.date.today(),
                'power_limit': None,
            }
            sample_event = {'driver_id': '0', 'location_id': '0', 'event_type': '0', 'car_id': '1', 'car_weight': '', 'tow_angle': '', 'camber': '', 'ride_height': '', 'ackerman_adjustment': '', 'power_limit': '', 'shock_dampening': '', 'torque_limit': '', 'frw_pressure': '', 'flw_pressure': '', 'brw_pressure': '', 'blw_pressure': '', 'day_id': '1'}
            logging.info("http://" + MQTTTarget.get() + ":5000/webtool/create_event/")
            day = DriveDay(**sample_drive_day)
            self.db_session.add(day)
            self.db_session.commit()
            
            #! CHANGE FOR PROD
            # response = requests.post("http://" + MQTTTarget.get() + ":5000/webtool/create_event/", data=sample_event)
            # response = requests.post("http://" + DBTarget.resolve_ip(DBTarget.get(client=True)) + "/webtool/create_event/", data=sample_event)
            response = requests.post("https://lhrelectric.org/webtool/create_event/", data=sample_event)
            
            with MQTTHandler('flask_app') as mqtt:
                mqtt.publish('config/page_sync', "running_event_page")
                
        #! NEED TO CHANGE BASED ON TRACK
        config = {"event_id": 1, "gate": ((30.3870, -97.7253), (30.3869, -97.7252)), "status": 0, "start_packet": 0}
        logging.info(config)
        self.mqtt.publish(f'config/test', json.dumps(config))
                
    def start_timer(self, start_time: int):
        config = {
            "timerRunning": True,
            "timerEventTime": time.time() * 1000 if REALTIME else start_time,
            "timerInternalTime": 0,
        }
        logging.info(f"STARTED TIMER AT {start_time if REALTIME else time.time() * 1000} | (OR {start_time})")
        logging.info(config)
        time.sleep(1)
        logging.info("SENT CONFIG AT EVENT UPDATE SYNC")
        self.mqtt.publish('config/event_update_sync', json.dumps(config, indent=4))
        
    def stop_timer(self):
        config = {
            "timerRunning": False,
            "useInternalTime": True
        }
        logging.info(config)
        self.mqtt.publish('config/event_update_sync', json.dumps(config, indent=4))

    def _rebuild_partitions_in_session(self, session, time_threshold = 300, min_partition_duration_ms = 120000):
        """
        Rebuilds partition windows from packet timestamp gaps.
        """

        query = f"""
            WITH packet_with_prev_time AS (
            SELECT 
                time,
                LAG(time) OVER (ORDER BY time) AS prev_time
            FROM packet
            ),
            event_starts AS (
                SELECT 
                    time AS start_time
                FROM packet_with_prev_time
                WHERE prev_time IS NULL 
                OR time - prev_time > {time_threshold * 1000}
            ),
            event_intervals AS (
                SELECT 
                    start_time,
                    LEAD(start_time) OVER (ORDER BY start_time) AS next_start_time
                FROM event_starts
            )
            SELECT 
                e.start_time,
                COALESCE(e.next_start_time - 1, p.max_time) AS end_time
            FROM event_intervals e
            CROSS JOIN (SELECT MAX(time) AS max_time FROM packet) p;
            """

        events = QueryBuilder.manual_query(query, car=self.car, return_type=list)
        if not events:
            logging.info("No events found in the database.")
            return

        session.query(Partitions).delete()

        inserted = 0
        for start, end in events:
            if end - start > min_partition_duration_ms:
                event = {
                    'start_time' : start,
                    'end_time' : end,
                    'partition_name' : datetime.datetime.fromtimestamp(start / 1000).strftime('%Y-%m-%d %H:%M:%S')
                }
                session.add(Partitions(**event))
                inserted += 1

        session.commit()
        logging.info(f"Inserted {inserted} partitions into the database.")

    def csv_event_injection(self, time_threshold = 300):
        """
        Find start and end times for an event and records into the database. Assumes csv files already input into database. 
        """

        if self.db_session is not None:
            self._rebuild_partitions_in_session(self.db_session, time_threshold=time_threshold)
            return

        with get_db(self.car) as session:
            self._rebuild_partitions_in_session(session, time_threshold=time_threshold)


def is_csv_valid_for_insert(csv_path, df, table_desc, car):
    if df.empty:
        logging.warning("Skipping %s: empty CSV", csv_path)
        return False

    # Build expected DB column set from schema as table.field names
    expected_cols = set()
    for table_name, columns in table_desc.items():
        if table_name in {"controls", "dynamics", "pack", "packet", "diagnostics_high", "diagnostics_low", "thermal"}:
            if isinstance(columns, dict):
                for col, (dtype, ndim) in columns.items():
                    # Exclude special-cased columns and array columns (stored as indexed cols or handled gracefully)
                    if col not in {"time", "packet_id", "f_gps", "b_gps"} and ndim == 0:
                        expected_cols.add(f"{table_name}.{col}")

    df_cols = set(df.columns)
    csv_fields = set(df.columns)

    missing_expected = expected_cols - df_cols

    if missing_expected:
        logging.warning("Missing expected columns in %s: %s", csv_path, missing_expected)
        return False

    return True


def count_csv_data_rows(csv_file: Path) -> int:
    """
    Counts data rows in a CSV file (excluding header).
    """
    with open(csv_file, "r") as f:
        return max(sum(1 for _ in f) - 1, 0)


def find_resume_point(csv_files, starting_packet_id: int, most_recent_packet_id: int):
    """
    Maps a packet_id checkpoint to (csv_index, row_offset_in_csv).

    Assumes the first data row of csv_files[0] was inserted with starting_packet_id.
    """
    if most_recent_packet_id < starting_packet_id:
        return 0, 0

    rows_already_inserted = most_recent_packet_id - starting_packet_id + 1

    for i, csv_file in enumerate(csv_files):
        rows_in_file = count_csv_data_rows(csv_file)
        if rows_already_inserted < rows_in_file:
            return i, rows_already_inserted
        rows_already_inserted -= rows_in_file

    return len(csv_files), 0


if __name__ == '__main__':

    parser = argparse.ArgumentParser(description="CSV Data Ingestion Script")
    parser.add_argument("--car", type=str, default="Orion", help="Target car for ingestion")
    parser.add_argument("--run_mode", type=str, choices=["insert", "playback"], default="playback", help="Run mode: insert or playback")
    parser.add_argument("--resume", action="store_true", help="Enable resume for interrupted ingestion")
    parser.add_argument("--most_recent_packet_id", type=int, default=0, help="Most recent packet_id for resuming")
    parser.add_argument("--starting_packet_id", type=int, default=1, help="Starting packet_id for the current batch")
    parser.add_argument("--log_dir", type=str, default=str(Path(__file__).parents[3].joinpath("logs", "orion")), help="Directory containing CSV logs")

    args = parser.parse_args()

    logging.basicConfig(level=logging.CRITICAL)
    
    car = args.car
    csv_directory = Path(args.log_dir)
    run_mode = args.run_mode

    # Resume settings for interrupted ingestion.
    resume_enabled = args.resume
    most_recent_packet_id = args.most_recent_packet_id
    starting_packet_id_for_batch = args.starting_packet_id

    if not csv_directory.exists() or not any(csv_directory.glob("*.csv")):
        raise FileNotFoundError(f"No CSV files found in directory: {csv_directory}")

    # if not csv_file.exists():
    #     raise FileNotFoundError(f"CSV file not found: {csv_file}")

    with get_db(car) as session:
        with MQTTHandler(name ='event_playback_test', target = MQTTTarget.get()) as mqtt:
            dataSender = CSVToDB(db_session=session, mqtt=mqtt, car=car)

            table_desc = QueryBuilder(car).get_table_column_specs()

            all_csv_files = sorted(csv_directory.glob("*.csv"))

            if run_mode == "playback":
                for csv_file in all_csv_files:
                    logging.info("Starting playback for CSV %s", csv_file)
                    dataSender.event_playback(csv_file, table_desc=table_desc, protobuf=True)
            elif run_mode == "insert":
                start_csv_index = 0
                start_row_offset = 0

                if resume_enabled:
                    start_csv_index, start_row_offset = find_resume_point(
                        all_csv_files,
                        starting_packet_id=starting_packet_id_for_batch,
                        most_recent_packet_id=most_recent_packet_id,
                    )
                    dataSender.set_last_packet_id(most_recent_packet_id)
                    print(
                        f"Resume enabled: starting at csv index {start_csv_index}, "
                        f"row offset {start_row_offset}, next packet_id {most_recent_packet_id + 1}"
                    )

                if start_csv_index >= len(all_csv_files):
                    print("All CSV rows appear to have already been inserted for the configured packet range.")
                    sys.exit(0)

                for idx, csv_file in enumerate(all_csv_files[start_csv_index:], start=start_csv_index):
                    print(f"Inserting data from {csv_file} into the database...")

                    logging.info("Validating CSV %s for insertion", csv_file)
                    try:
                        if idx == start_csv_index and start_row_offset > 0:
                            # Keep header row, skip already-inserted data rows.
                            df = pd.read_csv(csv_file, skiprows=range(1, start_row_offset + 1), low_memory=False)
                        else:
                            df = pd.read_csv(csv_file, low_memory=False)
                    except Exception as e:
                        logging.warning("Skipping %s: could not read CSV (%s)", csv_file, e)
                        continue

                    # Drop rows with no packet timestamp (e.g. truncated final record from logger cutoff).
                    # packet.time is non-nullable, so these rows cannot be inserted.
                    if "time" in df.columns:
                        before = len(df)
                        df = df.dropna(subset=["time"]).reset_index(drop=True)
                        dropped = before - len(df)
                        if dropped:
                            print(f"  Dropped {dropped} row(s) with missing time from {csv_file}")
                    if df.empty:
                        print(f"  No valid rows in {csv_file} after dropping missing-time rows; skipping.")
                        continue

                    if not is_csv_valid_for_insert(csv_file, df, table_desc, car):
                        continue
                    dataSender.insert_multi_row_from_csv(df=df, table_desc=table_desc, amt=500)

