import pandas as pd
import numpy as np
import datetime
import logging
import time
from sqlalchemy import text, insert
from sqlalchemy.inspection import inspect
from sqlalchemy.orm import Query, aliased
from .db_session import get_db
from .models import (
    DriveDay,
    LutDriver,
    LutLocation,
    LutCar,
    LutEventType,
    Event,
    Packet,
    Dynamics,
    Controls,
    Pack,
    DiagnosticsHigh,
    DiagnosticsLow,
    Thermal,
    Classifier,
    Partitions,
    AngeliquePacket,
    AngeliqueDynamics,
    AngeliqueControls,
    AngeliquePack,
    AngeliqueDiagnostics,
    AngeliqueThermal,
    OrionPacket,
    OrionDynamics,
    OrionControls,
    OrionPack,
    OrionDiagnosticsLow,
    OrionThermal,
)
from sqlalchemy.dialects.postgresql import ARRAY
from sqlalchemy.dialects.postgresql import JSONB # Added for JSONB type handling

import json
import os

class QueryBuilder:

    def __init__(self, car="Nightwatch"):
        self._car = car # Store car name
        self._db_context_manager = get_db(car) # Store the context manager
        self.session = None # Session will be set in __enter__
        self._query: Query = None # Query will be set in __enter__
        
        # Initialize _models based on the car
        self._models = {
            "DriveDay": DriveDay,
            "LutDriver": LutDriver,
            "LutLocation": LutLocation,
            "LutCar": LutCar,
            "LutEventType": LutEventType,
            "Event": Event,
            "Packet": Packet,
            "Classifier": Classifier,
            "Partitions": Partitions,
        }

        if car == "Nightwatch":
            self._models["Dynamics"] = Dynamics
            self._models["Controls"] = Controls
            self._models["Pack"] = Pack
            self._models["DiagnosticsHigh"] = DiagnosticsHigh
            self._models["DiagnosticsLow"] = DiagnosticsLow
            self._models["Thermal"] = Thermal
        elif car == "Angelique":
            self._models["Dynamics"] = AngeliqueDynamics
            self._models["Controls"] = AngeliqueControls
            self._models["Pack"] = AngeliquePack
            self._models["Diagnostics"] = AngeliqueDiagnostics
            self._models["Thermal"] = AngeliqueThermal
            self._models["Packet"] = AngeliquePacket
        elif car == "Orion":
            self._models["Dynamics"] = OrionDynamics
            self._models["Controls"] = OrionControls
            self._models["Pack"] = OrionPack
            self._models["DiagnosticsLow"] = OrionDiagnosticsLow
            self._models["Thermal"] = OrionThermal
            self._models["Packet"] = OrionPacket
        else:
            raise ValueError(f"Car {car} is not supported.")

    def get_table_column_specs(self, force=False, verbose=False, target=None):
        table_specs = {}
        for model_name, model_class in self._models.items():
            table_name = model_class.__tablename__
            
            if table_name not in table_specs:
                table_specs[table_name] = {}

            for column in model_class.__table__.columns:
                try:
                    dtype = column.type.python_type
                except NotImplementedError:
                    if str(column.type).upper() == 'POINT':
                        dtype = 'point'
                    elif str(column.type) == 'JSONB':
                        dtype = dict
                    elif str(column.type) == 'BYTEA':
                        dtype = bytes
                    else:
                        dtype = str # Generic fallback
                ndims = 0

                if isinstance(column.type, ARRAY):
                    ndims = 1 # Assuming 1D array for simplicity, can be extended
                    if hasattr(column.type, 'item_type'):
                        dtype = column.type.item_type.python_type
                elif str(column.type) == 'JSONB': # Handle JSONB type
                    dtype = dict
                    ndims = 0

                table_specs[table_name][column.name] = (dtype, ndims)

        return table_specs

    def __enter__(self):
        self.session = self._db_context_manager.__enter__() # Get the session from the context manager
        self._query = self.session.query() # Initialize query here
        if self._car.lower() == "angelique":
            self.session.execute(text("SET search_path TO angelique"))
        elif self._car.lower() == "orion":
            self.session.execute(text("SET search_path TO orion"))
        else:
            self.session.execute(text("SET search_path TO telemetry"))
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self._db_context_manager.__exit__(exc_type, exc_val, exc_tb)

    def select(self, *args):
        entities = []
        for arg in args:
            if isinstance(arg, str):
                if " as " in arg:
                    model_name, alias_name = arg.split(" as ")
                    model = self._models.get(model_name.strip())
                    if model:
                        entities.append(aliased(model, name=alias_name.strip()))
                    else:
                        entities.append(text(arg))
                else:
                    model = self._models.get(arg.strip())
                    if model:
                        entities.append(model)
                    else:
                        entities.append(text(arg))
            else:
                entities.append(arg)
        self._query = self.session.query(*entities)
        return self

    def from_table(self, table: str, abbreviation: str = ''):
        model = self._models.get(table)
        if model:
            if abbreviation:
                self._query = self._query.query(aliased(model, name=abbreviation))
            else:
                self._query = self._query.query(model)
        return self

    def join(self, *args):
        for arg in args:
            if isinstance(arg, dict):
                target = self._models.get(arg.get("table"))
                if target:
                    on_clause = arg.get("on")
                    if isinstance(on_clause, str):
                        on_clause = text(on_clause)
                    self._query = self._query.join(target, on_clause)
            elif isinstance(arg, tuple):
                self._query = self._query.join(*arg)
        return self

    def where(self, *args):
        for arg in args:
            if isinstance(arg, str):
                self._query = self._query.filter(text(arg))
            else:
                self._query = self._query.filter(arg)
        return self

    def groupby(self, *args):
        self._query = self._query.group_by(*args)
        return self

    def order(self, *args, desc: bool = True):
        for arg in args:
            if isinstance(arg, str):
                if desc:
                    self._query = self._query.order_by(text(f"{arg} DESC"))
                else:
                    self._query = self._query.order_by(text(f"{arg} ASC"))
            else:
                if desc:
                    self._query = self._query.order_by(arg.desc())
                else:
                    self._query = self._query.order_by(arg.asc())
        return self

    def limit(self, val: int = None):
        if val:
            self._query = self._query.limit(val)
        return self

    def get_query(self):
        return self._query

    def send_query(self, return_type: type = pd.DataFrame, **kwargs):
        results = self.session.execute(self._query).fetchall()
        if return_type is pd.DataFrame:
            return pd.DataFrame(results, columns=self._query.statement.columns.keys())
        elif return_type is pd.Series:
            return pd.Series([row[0] for row in results])
        elif return_type is dict:
            return [row._asdict() for row in results]
        elif return_type is np.ndarray:
            return np.array(results)
        elif return_type is list:
            return [list(row) for row in results]
        elif return_type == 'raw':
            return results
        else:
            raise NotImplementedError(f'Send query has not been implemented for {return_type} yet.')

    @staticmethod
    def manual_query(query: str, car="Nightwatch", return_type: type = pd.DataFrame, **kwargs):
        with get_db(car) as session:
            results = session.execute(text(query)).fetchall()
            if return_type is pd.DataFrame:
                return pd.DataFrame(results)
            elif return_type is pd.Series:
                return pd.Series([row[0] for row in results])
            elif return_type is dict:
                return [row._asdict() for row in results]
            elif return_type is np.ndarray:
                return np.array(results)
            elif return_type is list:
                return [list(row) for row in results]
            elif return_type == 'raw':
                return results
            else:
                raise NotImplementedError(f'Send query has not been implemented for {return_type} yet.')
            

    @staticmethod
    def normalize_value(val, expected_type):
        """Convert to psycopg2-acceptable value for bulk inserts."""
        if val is None:
            return None

        # Handle numpy → python conversions
        if isinstance(val, (np.integer,)):
            return int(val)
        if isinstance(val, (np.floating,)):
            return float(val)
        if isinstance(val, (np.bool_,)):
            return bool(val)
        if isinstance(val, (np.datetime64,)):
            return val.astype("M8[ms]").astype(datetime.datetime)
        if isinstance(val, (np.ndarray,)):
            return val.tolist()

        # Try to cast into the expected type
        if expected_type is bytes and isinstance(val, str):
            return val.encode('utf-8')
        try:
            return expected_type(val)
        except Exception:
            return val

    @staticmethod
    def get_insert_values(table: str, data: dict, model, table_desc):
        """
        Collects data to be inserted into DB via SQLAlchemy bulk insert.
        Applies table-specific preprocessing and type normalization.
        """
        def is_missing_value(val):
            if val is None:
                return True
            if isinstance(val, (float, np.floating)):
                return np.isnan(val)
            return False

        # --- Find missing columns ---
        missing_cols = [col for col in table_desc if col not in data]
        if missing_cols:
            logging.warning(f"Missing columns in {table}: {', '.join(missing_cols)}")

        # --- Handle NaN/None values ---
        missing_flags = [not is_missing_value(val) for _, val in data.items()]
        nans = {k: v for (k, v), ok in zip(data.items(), missing_flags) if not ok}
        if nans:
            logging.warning(f"NaN-like data in {table}: {nans}")

        # --- Normalize values ---
        processed = {}
        for (key, val), ok in zip(data.items(), missing_flags):
            if not ok:
                continue
            if key not in table_desc:
                continue  # skip unknown keys
            expected_type = table_desc[key][0]
            processed[key] = QueryBuilder.normalize_value(val, expected_type)

        return processed
    
    @staticmethod
    def bulk_insert(session, table_name, model, rows, table_desc, commit=True):
        processed = [QueryBuilder.get_insert_values(table_name, row, model, table_desc) for row in rows]
        session.bulk_insert_mappings(model, processed)
        if commit:
            session.commit()

    @staticmethod
    def insert(session, table_name, model, row, table_desc, commit=True):
        processed = QueryBuilder.get_insert_values(table_name, row, model, table_desc)
        session.add(model(**processed))
        if commit:
            session.commit()

    @staticmethod
    def execute_insert(session, rows, table_desc, commit=True):
        if not rows:
            return
        
        model_lookup = {
            model.__tablename__: model for model in [
                Packet, Dynamics, Controls, Pack, DiagnosticsHigh, DiagnosticsLow, Thermal,
                AngeliquePacket, AngeliqueDynamics, AngeliqueControls, AngeliquePack, AngeliqueDiagnostics, AngeliqueThermal,
                OrionPacket, OrionDynamics, OrionControls, OrionPack, OrionDiagnosticsLow, OrionThermal,
            ]
        }
        
        rows_by_table = {}
        for table_name, row_data in rows:
            if table_name not in rows_by_table:
                rows_by_table[table_name] = []
            rows_by_table[table_name].append(row_data)
        
        packet_tables = [t for t in rows_by_table.keys() if 'packet' in t]
        other_tables = [t for t in rows_by_table.keys() if 'packet' not in t]
        
        for table_name in packet_tables + other_tables:
            table_rows = rows_by_table[table_name]
            model = model_lookup.get(table_name)
            
            if not model or not table_rows:
                continue
            
            processed_rows = [
                QueryBuilder.get_insert_values(table_name, row, model, table_desc[model.__tablename__])
                for row in table_rows
            ]
            
            stmt = insert(model.__table__).values(processed_rows)
            session.execute(stmt)
        
        if commit:
            session.commit()


if __name__ == '__main__':
    with QueryBuilder() as qb:
        data = (qb
                .select("Event")
                .limit(10)
            ).send_query(pd.DataFrame)
        logging.debug(data)