from itertools import count
import os
from typing import Tuple, Union
import numpy as np
import pandas as pd
from numpy.random import default_rng
import time
import datetime
import pickle
import json
import requests
import logging
from tqdm import tqdm
import sys
from concurrent.futures import ThreadPoolExecutor, as_completed
from multiprocessing import cpu_count
from pathlib import Path
from psycopg.types.json import Jsonb
from typing import Union, Tuple
import requests



sys.path.append(str(Path(__file__).parents[4]))

from stack.ingest.mqtt_handler import MQTTHandler, MQTTTarget
from analysis.sql_utils.db_handler import get_table_column_specs
from analysis.database.paho_testing import DataTester


class LapTimerDataTester(DataTester):
    def add_data_for_gps(self, file: str, delay: float):
        dict = pd.read_csv(file).to_dict(orient='index')
        
        print("DICT: ", dict)
        
        for index, row_dict in dict.items():
            packet_row = {}
            packet_row["packet_id"] = row_dict["packet_id"]
            packet_row["time"] = round(time.time() * 1000)
            
            dynamics_row = {};
            dynamics_row["packet_id"] = row_dict["packet_id"]
            dynamics_row["gps"] = (float(row_dict["gps_lat"]), float(row_dict["gps_long"]))
            
            print("PACKET ROW: ", packet_row)
            print("DYNAMICS ROW: ", dynamics_row)
            
            self.mqtt.publish(f'data/packet', pickle.dumps(packet_row))
            self.mqtt.publish(f'data/dynamics', pickle.dumps(dynamics_row))
            time.sleep(delay)
        return 0
    
    def start_event_with_gate(self, event_id: str, gate: tuple[float, float]):
        config = {"event_id": event_id, "gate": gate, "status": 0, "start_packet": 0}
        self.mqtt.publish(f'config/test', json.dumps(config))
        return
    
    def test_page_sync(self):
        # self.mqtt.publish(f'page_sync', json.dumps({"laps": [14312, 24321, 34321, 44321]}))
        time = 32423423
        requests.post("http://localhost:5000/new_lap", data={"time": time})

if __name__ == '__main__':

    logging.basicConfig(level=logging.INFO)
    mqtt = MQTTHandler('terence_test', MQTTTarget.LOCAL)
    mqtt.connect()
    dbtest = LapTimerDataTester(mqtt)
    # dbtest.concurrent_tables_test(['thermal', 'dynamics'], 25, .1, rm_cols=['event_id'], mqtt_handler=mqtt)
    # dbtest.single_table_test('packet', 500, .1)
    
    
    dbtest.start_event_with_gate(1, ((30.289727, -97.736346), (30.289604, -97.736272)))
    
    if not os.path.exists("gps_test_data_2.csv"):
        raise FileNotFoundError(f"File gps_test_data_2.csv not found.")
    
    dbtest.add_data_for_gps("gps_test_data_2.csv", 1)
    # mqtt.disconnect()