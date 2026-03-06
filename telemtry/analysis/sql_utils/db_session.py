import os
import json
from pathlib import Path
from sqlalchemy import create_engine
from sqlalchemy.orm import sessionmaker
from contextlib import contextmanager

if os.getenv('IN_DOCKER'):
    with open("/app/net_configs.json", "r") as file:
        global_target = json.load(file)
else:
    with open(os.path.join(Path(__file__).parents[2], "net_configs.json"), "r") as file:
        global_target = json.load(file)
    from dotenv import load_dotenv
    load_dotenv()

class DBTarget:
    @staticmethod
    def get(client=False, car = "Nightwatch"):
        host = global_target["TARGETS"][global_target["CLIENT_TARGET"]] if client else 'db' if os.getenv('IN_DOCKER') else global_target["TARGETS"][global_target["SERVER_TARGET"]]

        if car == "Nightwatch":
            return {
                'dbname': 'telemetry',
                'users': {
                    'electric': os.getenv('ELECTRIC_PWD'),
                    'grafana': os.getenv('GRAFANA_PWD'),
                    'analysis': os.getenv('ANALYSIS_PWD')
                },
                'host': host,
                'port': 5432
            }
        elif car == "Angelique":
            return {
                'dbname': 'angelique',
                'users': {
                    'electric': os.getenv('ELECTRIC_PWD'),
                    'grafana': os.getenv('GRAFANA_PWD'),
                    'analysis': os.getenv('ANALYSIS_PWD')
                },
                'host': host,
                'port': 5432
            }
        elif car == "Orion":
            return {
                'dbname': 'orion',
                'users': {
                    'electric': os.getenv('ELECTRIC_PWD'),
                    'grafana': os.getenv('GRAFANA_PWD'),
                    'analysis': os.getenv('ANALYSIS_PWD')
                },
                'host': host,
                'port': 5432
            }
        else:
            raise ValueError(f'Car {car} is not supported. Please add it to the DBTarget class.')

def get_db_url(car="Nightwatch", user="electric"):
    target = DBTarget.get(car=car)
    return f"postgresql+psycopg2://{user}:{target['users'][user]}@{target['host']}:{target['port']}/{target['dbname']}"

engine_nightwatch = create_engine(get_db_url(car="Nightwatch"))
SessionLocal_nightwatch = sessionmaker(autocommit=False, autoflush=False, bind=engine_nightwatch)

engine_angelique = create_engine(get_db_url(car="Angelique"))
SessionLocal_angelique = sessionmaker(autocommit=False, autoflush=False, bind=engine_angelique)

engine_orion = create_engine(get_db_url(car="Orion"))
SessionLocal_orion = sessionmaker(autocommit=False, autoflush=False, bind=engine_orion)

@contextmanager
def get_db(car="Nightwatch"):
    if car == "Nightwatch":
        db = SessionLocal_nightwatch()
        try:
            yield db
        finally:
            db.close()
    elif car == "Angelique":
        db = SessionLocal_angelique()
        try:
            yield db
        finally:
            db.close()
    elif car == "Orion":
        db = SessionLocal_orion()
        try:
            yield db
        finally:
            db.close()
    else:
        raise ValueError(f'Car {car} is not supported.')
