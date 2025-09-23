import pandas as pd
import numpy as np
from sqlalchemy import text
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
    AngeliqueDynamics,
    AngeliqueControls,
    AngeliqueDiagnostics,
    AngeliqueThermal,
    Partitions,
)
from sqlalchemy.dialects.postgresql import ARRAY
from geoalchemy2 import Geometry
from sqlalchemy.dialects.postgresql import JSONB # Added for JSONB type handling

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
            self._models["Pack"] = Pack # Assuming Pack is same for Angelique
            self._models["Diagnostics"] = AngeliqueDiagnostics # This is the key
            self._models["Thermal"] = AngeliqueThermal
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
                    if isinstance(column.type, Geometry):
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
                elif isinstance(column.type, Geometry):
                    dtype = 'point' # Special handling for POINT type
                    ndims = 0 # A point is a single entity, not an array of values
                elif str(column.type) == 'JSONB': # Handle JSONB type
                    dtype = dict
                    ndims = 0

                table_specs[table_name][column.name] = (dtype, ndims)

        return table_specs

    def __enter__(self):
        self.session = self._db_context_manager.__enter__() # Get the session from the context manager
        self._query = self.session.query() # Initialize query here
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

if __name__ == '__main__':
    with QueryBuilder() as qb:
        data = (qb
                .select("Event")
                .limit(10)
            ).send_query(pd.DataFrame)
        print(data)