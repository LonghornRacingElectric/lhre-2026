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
from concurrent.futures import ThreadPoolExecutor, as_completed
from multiprocessing import cpu_count
from pathlib import Path
from psycopg.types.json import Jsonb
from typing import Union, Tuple

sys.path.append(str(Path(__file__).parents[2]))
from stack.ingest.mqtt_handler import MQTTHandler, MQTTTarget
from analysis.sql_utils.db_handler import get_table_column_specs, DBHandler, DBTarget



class DataTester:
    """
    Class for testing database with random values in correct data types.
    """
    def __init__(self, mqtt, seed=None):
        """
        Initializes DataTester class by initiating a numpy.random.Generator.

        :param seed:        seed to pass to numpy.random.Generator constructor
        """
        self.rng = default_rng(seed)
        self.mqtt = mqtt

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

        desc = get_table_column_specs(get_specs.get('force'), get_specs.get('verbose'))

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
            elif dtype is Jsonb:
                row[col] = Jsonb({'fake_jsonb_data': self.get_random_data(int, 3)})
            elif dtype == 'point':
                row[col] = random.choice([(30.289464, -97.735303), (30.389670, -97.728152)])
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

    def single_table_test(self, table: str, num_rows: int, delay: float, rm_cols=None, **kwargs):
        """
        This function runs an ingestion test on an individual table, sequentially publishing data to the table at
        "delay" intervals.

        :param table: str representing target table name
        :param num_rows: int representing number of rows to send to the ingest server in total
        :param delay: float representing time to sleep for between each row write
        :param rm_cols: str | list | dict to be passed to get_desc for removal from description
        :param kwargs: accepts an existing client or table_desc for parallel runs and kwargs to pass to get_desc

        :return: returns 0 for successful runs
        """
        table_desc = kwargs.pop('table_desc', self.get_desc(tables=[table], rm_cols=rm_cols, **kwargs)[table])

        # for i in range(num_rows) if kwargs.get('verbose') else tqdm(range(num_rows)):
        for i in range(num_rows) if kwargs.get('verbose') else tqdm(range(num_rows)):
            row = self.create_row(table_desc, i)
            if kwargs.get('verbose') and (num_rows < 1000 or not i % (num_rows // 100)):
                logging.info(f'Publishing payload #{i:>3} to {table}: {row}')
            self.mqtt.publish(f'data/{table}', pickle.dumps(row), qos=1)
            time.sleep(delay)
        return 0

    def concurrent_tables_test(self, tables: list, num_rows: int, delay: float, rm_cols=None, **kwargs):
        """
        This function runs an ingestion test on an multiple tables simultaneously, sequentially publishing data to the
        table at "delay" intervals.

        :param tables: list representing target table names
        :param num_rows: int representing number of rows to send to the ingest server in total
        :param delay: float representing time to sleep for between each row write
        :param rm_cols: str | list | dict to be passed to get_desc for removal from description
        :param kwargs: accepts an existing client or table_desc for parallel runs and kwargs to pass to get_desc

        :return: returns 0 for successful runs
        """
        db_desc = self.get_desc(tables=tables, rm_cols=rm_cols, **kwargs)

        with ThreadPoolExecutor(max_workers=cpu_count()) as executor:
            futures = [executor.submit(self.single_table_test, table, num_rows, delay, rm_cols,
                                       table_desc=db_desc[table], **kwargs) for table in tables]

            try:
                for future in as_completed(futures):
                    future.result() if kwargs.get('verbose') else print(f'Exit Code: {future.result()}')
            except KeyboardInterrupt:
                executor.shutdown(False)

        return 0

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
    with DBHandler(unsafe=True, target=DBTarget.LOCAL) as handler:
        with MQTTHandler('paho_test', db_handler=handler) as mqtt:
            dbtest = DataTester(mqtt)
            dbtest.single_table_test('packet', 5000, .0001)
            dbtest.concurrent_tables_test(['thermal', 'dynamics', 'pack'], 5000, .0001)