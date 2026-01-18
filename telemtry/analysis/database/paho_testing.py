import os
import random
import numpy as np
import secrets
from numpy.random import default_rng
import time
import datetime
import pickle
import json
import requests
import logging
from itertools import count
from tqdm import tqdm
import sys
import secrets
from concurrent.futures import ThreadPoolExecutor, as_completed
from multiprocessing import cpu_count
from pathlib import Path
from typing import Union, Tuple
from google.protobuf.message import Message
import pandas as pd

sys.path.append(str(Path(__file__).parents[2]))
from stack.ingest.mqtt_handler import MQTTHandler, MQTTTarget
from analysis.sql_utils.db_session import get_db, DBTarget
from analysis.sql_utils.query_builder import QueryBuilder
from stack.ingest.protobuf.template_pb2 import SensorData
from stack.ingest.protobuf.angelique_pb2 import AngeliqueSensorData



class DataTester:
    """
    Class for testing database with random values in correct data types or CSV data.
    """
    def __init__(self, mqtt, seed=None, csv_path=None, mapping_path=None):
        """
        Initializes DataTester class by initiating a numpy.random.Generator.

        :param seed:        seed to pass to numpy.random.Generator constructor
        :param csv_path:    path to CSV file to read data from (optional)
        :param mapping_path: path to JSON mapping file (optional)
        """
        self.rng = default_rng(seed)
        self.mqtt = mqtt
        self.csv_path = csv_path
        self.mapping_path = mapping_path
        self.csv_data = None
        self.mapping = None
        self.csv_index = 0
        
        if csv_path:
            self.load_csv(csv_path)
        if mapping_path:
            self.load_mapping(mapping_path)

    def load_csv(self, csv_path):
        """
        Load CSV file into memory.
        
        :param csv_path: path to CSV file
        """
        self.csv_data = pd.read_csv(csv_path)
        self.csv_index = 0
        logging.info(f"Loaded CSV with {len(self.csv_data)} rows and columns: {list(self.csv_data.columns)}")
    
    def load_mapping(self, mapping_path):
        """
        Load JSON mapping file.
        
        :param mapping_path: path to JSON mapping file
        """
        with open(mapping_path, 'r') as f:
            self.mapping = json.load(f)
        logging.info(f"Loaded mapping with {len(self.mapping)} fields")
    
    def create_row_from_csv(self, packet: int, table_desc: dict = None):
        """
        Create a row of data from CSV using the mapping.
        
        :param packet: packet ID
        :param table_desc: optional table description with type information
        :return: dict with database column names as keys
        """
        if self.csv_data is None or self.mapping is None:
            raise ValueError("CSV data or mapping is not loaded.")

        if self.csv_index >= len(self.csv_data):
            self.csv_index = 0  # Reset index if we reach the end of the CSV

        csv_row = self.csv_data.iloc[self.csv_index]
        self.csv_index += 1

        row = {}
        row['packet_id'] = packet

        for db_col, csv_col in self.mapping.items():
            if isinstance(csv_col, list):
                row[db_col] = [csv_row[col] for col in csv_col]
            else:
                row[db_col] = csv_row[csv_col]

        return row

    @staticmethod
    def get_desc(db=False, tables=None, rm_cols=None, **get_specs):
        """
        Most advanced infra to pull a table description from the DB. Can return either a full table description or
        number of tables. Refer to get_table_column_specs for return format

        :param db:      bool indicating whether entire DB description is desired
        :param tables:  str | list indicating which table(s) is desired
        :param rm_cols: str | list | dict indicating which columns to remove or if dict, which columns to
                                          remove from which table (format: {'electronics': ['imd_on'], 'dynamics': ...})
        :param get_specs: kwargs to pass to get_table_columns_specs

        :return: db_description (see get_table_column_specs for return format details)
        """
        if tables and db:
            raise ValueError('Can not produce full DB and table(s) descriptions simultaneously.')
        elif not (tables or db):
            raise ValueError('Either db arg must be true or table names must be given.')
        elif db and not isinstance(db, bool):
            raise ValueError('Input to db must be True or False.')

        if isinstance(rm_cols, str):
            rm_cols = [rm_cols]
        if isinstance(tables, str):
            tables = [tables]

        with QueryBuilder(car=get_specs.get('target')) as qb:
            desc = qb.get_table_column_specs(get_specs.get('force'), get_specs.get('verbose'), target=get_specs.get('target'))

        if rm_cols:
            for table in (desc if db else tables):
                for col in rm_cols if isinstance(rm_cols, list) else rm_cols[table]:
                    desc[table].pop(col)

        if db:
            return desc
        return {table: desc[table] for table in tables}

    def get_random_data(self, dtype: type, size: Union[int, Tuple[int, int]], as_scalar=False, **kwargs):
        """
        Creates and returns array of (pseudo-)randomly generated number based on given data type.

        :param dtype:       class indicating desired output data type
        :param size:        int indicating number of values desired
        :param as_scalar:   bool if true and size is 1, return value as scalar instead of list
        :param kwargs:      min/low: int indicating minimum value for rng
                            max/high: int indicating maximum value for rng
                            endpoint: bool indicating whether to include high/max value (note: only works for floats)
                            length: int indicating number of sentences (note: only works for strings)
        :return:            array of randomly generated numbers
        """

        # Checks if low/min was passed by name, but not high/max which would cause low/min to be treated as high/max
        if ('low' in kwargs or 'min' in kwargs) and 'high' not in kwargs and 'max' not in kwargs:
            raise ValueError('min/low number specified, but max/high number was not.')

        # Checks if as_scalar matches size. If size > 1, a scalar cannot contain the values
        if as_scalar and size != 1:
            raise ValueError('as_scalar was set to true, but more than 1 value was expected to be returned.')

        # If bool or int, use default_rng.integers method with some argument manipulation
        if dtype is bool or np.issubdtype(dtype, np.integer):
            res = self.rng.integers(0, 2 if dtype is bool else 32767, size=size, dtype=int).tolist()
        # If float, use default_rng.random method with [0, 1) --> [low/min, high/max) transformation algorithm
        elif np.issubdtype(dtype, np.floating):
            # Algorithm used is (big - small) * random(0-1) + small
            res = ((kwargs.get('high', kwargs.get('max', 1)) - kwargs.get('low', kwargs.get('min', 0))) * \
                   self.rng.random(size, dtype) + kwargs.get('low', kwargs.get('min', 0))).tolist()
        # If str, bacon
        elif dtype is str:
            res = [requests.get('https://baconipsum.com/api/',
                                params={'type': 'meat-and-filler', 'sentences': kwargs.get('length', 10),
                                        'start-with-lorem': 1}).text[2:-2] for _ in range(size)]
        elif dtype is bytearray:
            res = secrets.token_bytes(16)
            
        else:
            logging.warning(f'Data type {dtype} not implemented yet.')

        return res[0] if as_scalar else list(res)

    def create_row(self, table_desc: dict, packet: int):
        """
        This function is used to create a row of test data.

        :param table_desc: table_desc to base row of data off of

        :return: a row of data in dict {col_name: random_data, col2_name: ...} format
        """
        row = {}
        for col, (dtype, ndims) in table_desc.items():
            if col == 'time':
                row[col] = time.time() * 1000
            elif col == 'packet_id':
                row[col] = packet
            elif col == 'cells_temp':
                # row[col] = np.random.randint(1, 101, size=(4, 5)).tolist()
                row[col] = np.random.randint(1, 101, size=140).tolist()
            elif dtype is datetime.datetime:
                row[col] = datetime.date.today()
            elif dtype is dict:
                # row[col] = Jsonb({'fake_jsonb_data': self.get_random_data(int, 3)})
                row[col] = {'fake_jsonb_data': self.get_random_data(int, 3)} 
            elif dtype == 'point' or dtype == "POINT" or (isinstance(dtype, str) and dtype.lower() == 'point') or (isinstance(dtype, str) and dtype.lower() == 'POINT'):
                point = random.choice([(30.289464, -97.735303), (30.389670, -97.728152)])
                row[col] = point
            elif dtype is bytearray or dtype is bytes:
                row[col] = secrets.token_bytes(16)
            else:
                if ndims == 0:
                    row[col] = self.get_random_data(dtype, size=1, as_scalar=True)
                elif ndims == 1:
                    row[col] = self.get_random_data(dtype, size=3)
                elif ndims == 2:
                    row[col] = self.get_random_data(dtype, size=(3, 3))
                else:
                    raise ValueError(f'Invalid number of dimensions: {ndims}')
        return row

    def single_table_test(self, table: str, num_rows: int, delay: float, rm_cols=None, use_csv=False, **kwargs):
        """
        This function runs an ingestion test on an individual table, sequentially publishing data to the table at
        "delay" intervals.

        :param table: str representing target table name
        :param num_rows: int representing number of rows to send to the ingest server in total
        :param delay: float representing time to sleep for between each row write
        :param rm_cols: str | list | dict to be passed to get_desc for removal from description
        :param use_csv: bool indicating whether to use CSV data instead of random data
        :param kwargs: accepts an existing client or table_desc for parallel runs and kwargs to pass to get_desc

        :return: returns 0 for successful runs
        """
        table_desc = kwargs.pop('table_desc', self.get_desc(tables=[table], rm_cols=rm_cols, **kwargs)[table])

        # for i in range(num_rows) if kwargs.get('verbose') else tqdm(range(num_rows)):
        for i in range(num_rows) if kwargs.get('verbose') else tqdm(range(num_rows)):
            if use_csv and self.csv_data is not None and self.mapping is not None:
                row = self.create_row_from_csv(i + 1, table_desc)
            else:
                row = self.create_row(table_desc, i + 1)
            if kwargs.get('verbose') and (num_rows < 1000 or not i % (num_rows // 100)):
                logging.info(f'Publishing payload #{i:>3} to {table}: {row}')
            if kwargs.get('target') == "Angelique":
                self.mqtt.publish(f'angelique/{table}', pickle.dumps(row), qos=0)
            else:
                self.mqtt.publish(f'data/{table}', pickle.dumps(row), qos=0)
            time.sleep(delay)
        return 0

    def concurrent_tables_test(self, tables: list, num_rows: int, delay: float, rm_cols=None, use_csv=False, **kwargs):
        """
        This function runs an ingestion test on an multiple tables simultaneously, sequentially publishing data to the
        table at "delay" intervals.

        :param tables: list representing target table names
        :param num_rows: int representing number of rows to send to the ingest server in total
        :param delay: float representing time to sleep for between each row write
        :param rm_cols: str | list | dict to be passed to get_desc for removal from description
        :param use_csv: bool indicating whether to use CSV data instead of random data
        :param kwargs: accepts an existing client or table_desc for parallel runs and kwargs to pass to get_desc

        :return: returns 0 for successful runs
        """
        db_desc = self.get_desc(tables=tables, rm_cols=rm_cols, **kwargs)

        with ThreadPoolExecutor(max_workers=cpu_count()) as executor:
            futures = [executor.submit(self.single_table_test, table, num_rows, delay, rm_cols, use_csv,
                                       table_desc=db_desc[table], **kwargs) for table in tables]

            try:
                for future in as_completed(futures):
                    future.result() if kwargs.get('verbose') else logging.error(f'Exit Code: {future.result()}')
            except KeyboardInterrupt:
                executor.shutdown(False)

        return 0
    
    def send_proto_rows(self, tables:list,  num_rows:int, delay:float, rm_cols = None, use_csv=False, **kwargs):
        db_desc = self.get_desc(tables=tables, rm_cols=rm_cols, **kwargs)
        topic = 'angelique/data' if kwargs.get('target') == "Angelique" else 'data'
        for i in tqdm(range(num_rows)):
            data = self.create_proto_message(i + 1, db_desc, use_csv=use_csv, target=kwargs.get('target', 'Nightwatch'))
            mqtt.publish(topic, data.SerializeToString(), qos=0)
            time.sleep(delay)

    def create_proto_message(self, packet: int, db_desc, use_csv=False, target="Nightwatch"):
        """
        Create a protobuf message using either random data or CSV data.
        
        :param packet: packet ID
        :param db_desc: database description
        :param use_csv: whether to use CSV data
        :param target: target database
        :return: protobuf message
        """
        if use_csv:
            row = self.create_row_from_csv(packet, db_desc)
        else:
            row = self.create_row(db_desc, packet)

        if (target == "Angelique"):
            data = AngeliqueSensorData()
        else:
            data = SensorData()
        
        # Get data from CSV once if needed
        csv_row = None
        if use_csv and self.csv_data is not None and self.mapping is not None:
            # We need to pass table_desc but we'll use a merged version for all tables
            # or just handle type conversion more carefully
            csv_row = self.create_row_from_csv(packet, table_desc=None)
        
        for table in db_desc:
            if use_csv and csv_row is not None:
                row = csv_row  # Use the CSV data for all tables
            else:
                row = self.create_row(db_desc[table], packet)  # Create random data
            
            if hasattr(data, table):  
                table_instance = getattr(data, table)
                if isinstance(table_instance, Message): # Iterate through a specific table (not packet table)
                    for key, value in row.items():
                        if hasattr(table_instance, key): # Set values in protobuf message
                            try:
                                if (isinstance(value, list) or isinstance(value, tuple)):
                                    getattr(table_instance, key).extend(value)
                                elif isinstance(value, dict):
                                    setattr(table_instance, key, json.dumps(value))
                                else:
                                    setattr(table_instance, key, value)
                            except TypeError as e:
                                # If type error, try converting to int
                                if isinstance(value, float):
                                    setattr(table_instance, key, int(value))
                                else:
                                    logging.warning(f"Type error setting {table}.{key} = {value}: {e}")
            else:
                for key, value in row.items(): # Edit packet and time
                    if hasattr(data, key):
                        try:
                            setattr(data, key, int(value))
                        except (TypeError, ValueError):
                            logging.warning(f"Could not set {key} = {value}")
        return data
    
    def send_base64_row(self, ver: int, high_freq=True):
        with open(os.getcwd().split('LHR')[0] + f'/LHR/stack/ingest/car_configs/version{ver:02}.json', 'r') as f:
            config = json.load(f)['high' if high_freq else 'low']
        str_to_np = {np_clas.__name__: np_clas for np_clas in getattr(getattr(sys.modules[__name__], 'np'), 'ScalarType')}
        scalar_or_list = lambda val, scalar: val.tolist()[0] if scalar else val.tolist()
        payload_str = ver.to_bytes(1, 'big') + b''.join([scalar_or_list((np.array(
            dbtest.get_random_data(str_to_np[col_spec['type']], size=(shape := col_spec.get('shape', (1,)))),
            dtype=col_spec['type']) / col_spec.get('multiplier', 1)).flatten().reshape(shape), shape == (1,)).tobytes()
            for col, col_spec in config.items()])
        self.mqtt.publish('/h' if high_freq else '/l', payload_str, qos=1)


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    car_name = "Angelique"
    with get_db("Nightwatch") as nightwatch_session, get_db("Angelique") as angelique_session:
        db_sessions = {'Nightwatch': nightwatch_session, 'Angelique': angelique_session}
        with MQTTHandler('paho_test', db_sessions=db_sessions, target=MQTTTarget.get()) as mqtt:
            # Protobuf message testing with CSV data
            dt = DataTester(mqtt=mqtt, seed=42, csv_path=csv_path, mapping_path=mapping_path)
            dt.send_proto_rows(
                tables=['packet', 'dynamics', 'controls', 'pack', 'diagnostics', 'thermal'],
                num_rows= 20000,
                delay= 0.01,
                target=car_name
            )

            # data_ingest with fully processed data
            # dt = DataTester(mqtt=mqtt, seed=42)
            # dt.single_table_test('packet', 2000, 0.01, target=car_name)  # sequential
            # time.sleep(1)
            # if car_name == "Nightwatch":
            #     dt.concurrent_tables_test(['dynamics', 'controls', 'pack', 'diagnostics_high', 'diagnostics_low', 'thermal'],
            #                               2000, 0.01, target=car_name)  # batch
            # elif car_name == "Angelique":
            #     dt.concurrent_tables_test(['dynamics', 'controls', 'pack', 'diagnostics', 'thermal'],
            #                               2000, 0.01, target=car_name)  # batch
            #print (dt.get_desc(db=True, target=car_name))