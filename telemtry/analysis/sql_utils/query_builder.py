import pandas as pd
import numpy as np
from sqlalchemy import text
from sqlalchemy.orm import Query, aliased
from telemtry.analysis.sql_utils.db_session import get_db
from telemtry.analysis.sql_utils.models import (
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

class QueryBuilder:
    def __init__(self, car="Nightwatch"):
        self.session_generator = get_db(car)
        self.session = next(self.session_generator)
        self._query: Query = self.session.query()
        self._models = {
            "DriveDay": DriveDay,
            "LutDriver": LutDriver,
            "LutLocation": LutLocation,
            "LutCar": LutCar,
            "LutEventType": LutEventType,
            "Event": Event,
            "Packet": Packet,
            "Dynamics": Dynamics if car == "Nightwatch" else AngeliqueDynamics,
            "Controls": Controls if car == "Nightwatch" else AngeliqueControls,
            "Pack": Pack,
            "DiagnosticsHigh": DiagnosticsHigh,
            "DiagnosticsLow": DiagnosticsLow,
            "Thermal": Thermal if car == "Nightwatch" else AngeliqueThermal,
            "Classifier": Classifier,
            "Partitions": Partitions,
        }

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.session.close()

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
                self._query = self.session.query(aliased(model, name=abbreviation))
            else:
                self._query = self.session.query(model)
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