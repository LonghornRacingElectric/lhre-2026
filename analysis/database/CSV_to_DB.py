import warnings
warnings.filterwarnings("ignore")
import pandas as pd
import pickle
import time
import logging
import datetime
import os
import psycopg
import sys
import json
import matplotlib.pyplot as plt
import numpy as np
from concurrent.futures import ThreadPoolExecutor
from multiprocessing import cpu_count
from pathlib import Path
from tqdm import tqdm
from psycopg.types.json import Jsonb
from queue import Queue

sys.path.append(str(Path(__file__).parents[2]))

from analysis.sql_utils.db_handler import get_table_column_specs
from analysis.sql_utils.db_handler import DBHandler, DBTarget
from stack.ingest.mqtt_handler import MQTTHandler, MQTTTarget

class CSVToDB():

    def __init__(self, data_csv_folder, mqtt, db_handler = None, MQQT_target = MQTTTarget.LOCAL):
        self.db_handler = db_handler
        self.MQTT_target = MQQT_target
        self.mqtt = mqtt
        self.last_packet = 0
        try:
            self.data_csv_folder = sorted(list(Path.cwd().joinpath("csv_data", data_csv_folder).glob("*.csv")))
        except FileNotFoundError:
            raise FileNotFoundError("NO CSV FILES WERE FOUND UNDER csv_data/\nCREATE csv_data/ and add csv files within the folder.\n")

    def event_seperator(self, threshold = 20, speed_filter = False):
        """"
        The class variable is a directory containing csv files from a certain drive day. The goal of this method is to 
        partition the folder into continous segments in which the car is running, outputting a list containing dataframes
        where each dataframe represents a near continous time period in which the car is running. 
        """
        csv_split = []
        df_current = pd.read_csv(self.data_csv_folder[0])
        event = [df_current.copy(deep=False)]
        
        if (len(self.data_csv_folder) == 1):
            event_storage = Path.cwd().joinpath("event_csv")
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
        
        event_storage = Path.cwd().joinpath("event_csv")
        if not os.path.exists(event_storage): 
            os.makedirs(event_storage) 
        for i in range(len(continous_event)):
            continous_event[i].to_csv(event_storage.joinpath(f'{i}.csv'))
      
    def enumerate_packet_id(self, data_csv):       
        """
        Enumerates the packet_id from the last known packet_id. Uses the csv with car data to determine amount of packet_id to add
        """
        try: 
            last_packet = max(DBHandler.simple_select("SELECT packet_id FROM packet ORDER BY packet_id DESC LIMIT 1", 
                                                      target=DBTarget.LOCAL, user='electric', handler=self.db_handler)[0][0], 
                                                      self.last_packet)
        except IndexError:
            last_packet = self.last_packet

        df = data_csv
        pack_id = list(range(last_packet + 1, last_packet+1+len(df.index)))
        self.last_packet = last_packet + len(df.index)
        return pack_id

    def dataConvert(self, df, table_desc):
        """
        Converts data to the csv to a format that can be added into the database
        """
        convert = {}
        packet_ids = self.enumerate_packet_id(df)
        with open(Path.cwd().joinpath("analysis").joinpath("database").joinpath("pg_to_csv.json"), "r") as pg:
            pg_to_csv = json.load(pg)

        for table in table_desc:
            convert[table] = {}
            for column, (dtype, ndim) in table_desc[table].items():
                if column not in {"time", "packet_id", "gps", } and column in pg_to_csv:
                    if ndim:
                        convert[table][column] = df[pg_to_csv[column]].astype(dtype).to_numpy().tolist()
                    else:
                        convert[table][column] = df[pg_to_csv[column]].astype(dtype)
                elif (column == "time"):
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
                elif (column == "gps"):
                    convert[table][column] = np.column_stack((df["Latitude"], df["Longitude"])).tolist()
                else:
                    convert[table][column] = []

        return convert

    #Each iteration makes a new connection to the database and sends a singular row at a time
    def insert_row_from_csv(self, df, num_rows : int):
        """
        Adds data into the database. First adds packet table, then subsequent table using DBHandler insert function. Each iteration
        only inputs a singular row to the database, making this program the least efficient
        """
        data = self.dataConvert(df)
        packets = data["packet"]
        for i in tqdm(range(num_rows)):
            row_dict = {j : packets[j][i] for j in packets if (len(packets[j] != 0))}
            DBHandler.insert(table="packet", data=row_dict, user = "electric", handler = self.db_handler)
            logging.info(row_dict)

        for i in tqdm(data.keys()):
            if (i not in {"packet", "event", "classifier", "drive_day", "lut_driver", "lut_location", "lut_car", "lut_event_type"}):
                for j in range(num_rows):
                    row_dict = {k : data[k][j] for k in data[i] if (len(data[i][k]) != 0)}
                    DBHandler.insert(table = i, data = row_dict, user="electric", handler = self.db_handler)

    def insert_multi_row_from_csv(self, df, amt = 2000):
        """
        Inserts data into the database in larger batches determined by the given amt value. 
        """
        # Setup data within method--------------------------------------------------------------------------------
        data = self.dataConvert(df, table_desc=table_desc)
        packets = data["packet"]
        for j in tqdm(range(0, len(df.index), amt)):
            pack_list = []
            high = min(j + amt, len(df.index))
            pack_list = [{j : packets[j][i] for j in packets if (len(packets[j]) != 0)} for i in range(j, high, 1)]
            DBHandler.insert_multi_rows(table="packet", data = pack_list, user="electric", handler = self.db_handler)

            for i, values in data.items():
                row_list = []
                if (i not in ["packet", "event", "classifier", "drive_day", "lut_driver", "lut_location", "lut_car", "lut_event_type"]):
                    row_list = [{k : values[k][j] for k in values if (len(values[k]) != 0)} for j in range(j, high, 1)]
                    DBHandler.insert_multi_rows(table = i, data = row_list, user="electric", handler = self.db_handler)

    def stream_data(self, df):
        for chunk in (pd.read_csv(df, chunksize=2000)):
            yield chunk.reset_index(drop = True)

    def event_playback(self, file_path, table_desc, time_adjustment = True, batch_amt = 1):
        """
        Publishes to database based on time delays from the csv
        """
        def publish_row():
            """
            Takes data from a csv and sends data in rows to mqtt for event playback. Data is formatted as a dictionary and published
            to mqtt through small batches of rows
            """
            while not done or not chunk_queue.empty():

                if (not chunk_queue.empty()):
                    differences, row_dict_list = chunk_queue.get()
                    chunk_length = len(differences)

                    for i in range(chunk_length):
                        time.sleep((float(differences[i])/ 1000))
                        global_progress.update(1)
                        if (i % batch_amt == 0):
                            for table in ['packet', 'dynamics', 'controls', 'pack', 'diagnostics', 'thermal']: #Through the different tables
                                end_index = min(i + batch_amt, chunk_length)  # Ensure we don't exceed the length
                                batch_data = row_dict_list.get(table)[i:end_index]
                                self.mqtt.publish(f'data/{table}', pickle.dumps(batch_data), qos=0)
                else:
                    time.sleep(0.1)

        def setup_rows(df, table_desc, time_adjustment):
            """"
            Formats the rows in the form of dictionaries to be sent into database
            """
            data = self.dataConvert(df, table_desc=table_desc)
            differences = []
            times = np.array(data["packet"]["time"])
            differences = np.insert(np.diff(times), 0, 0)
            if (time_adjustment):
                data["packet"]["time"] = current_time[0] + np.cumsum(differences)
                current_time[0] =  current_time[0] + np.sum(differences)

            row_dict_list = {}
            for i, values in data.items():
                row_list = []
                if (i not in {"event", "classifier", "drive_day", "lut_driver", "lut_location", "lut_car", "lut_event_type"}):
                    row_list = [DBHandler.get_insert_values(table = i, data={k : values[k][j] for k in values if (len(values[k]) != 0)},
                                table_desc=table_desc[i]) for j in range(len(df.index))]
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

if __name__ == '__main__':

    logging.basicConfig(level=logging.CRITICAL)
    # Playback testing ---------------------------------------------------------------------------------------------
    with DBHandler(unsafe=True, target=DBTarget.LOCAL) as db:
        with MQTTHandler(name ='event_playback_test', target = MQTTTarget.LOCAL, db_handler=db) as mqtt:
            db.connect(target = DBTarget.LOCAL, user = 'electric')
            dataSender = CSVToDB("2024_10_13__001_AutoXCompDay", db_handler=db, mqtt=mqtt)
            table_desc = get_table_column_specs()
            ## Event playback functionarlity code TODO---------------------------------------------------------------------------------
            dataSender.event_seperator(threshold=5, speed_filter=True) #Saves list to harddrive
            mqtt.connect()
            #Where the csv is stored
            dataSender.event_playback(Path.cwd().joinpath("event_csv").joinpath("0.csv"), table_desc=table_desc, batch_amt=10)
       
