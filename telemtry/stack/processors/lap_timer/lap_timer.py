import json
import os
import logging
from time import sleep
import time
import pandas as pd
import numpy as np
from paho.mqtt import client as mqtt_client
import threading
import math
import requests

from analysis.sql_utils.db_session import get_db, DBTarget
from analysis.sql_utils.query_builder import QueryBuilder
from stack.ingest.mqtt_handler import MQTTHandler, MQTTTarget
from analysis.sql_utils.models import Classifier, Dynamics, Packet, Event


class LapTimerProcessor:
    def __init__(self, session=None):
        self.session = session
        self.event_id: int = None
        self.gate: tuple[tuple[float], tuple[float]]  = None
        self.status: int = None
        
        self.start_packet: int = 0
        self.table_specs = QueryBuilder("Nightwatch").get_table_column_specs()

    def _track_lap(self, gate: tuple[tuple[float, float], tuple[float, float]], points: list) -> float | None:
        """
        Checks if a circuit loop has happened after a given packet
        Args:
            last_packet: checking after this value
            gate: human-measured gate that represents the starting line
        """        
    
        for i in range(len(points) - 1):
            # print(f"TIME: {points[i][1]} | GPS: {points[i][0]}")
            if(self._is_intersection(gate, [points[i][0], points[i + 1][0]])):
                return round((points[i][1] + points[i + 1][1]) / 2)
        
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
    
    def _latlon_to_ecef(self, lat: float, lon: float) -> tuple[float, float, float]:
        # Constants
        R = 6371000  # Earth radius in meters

        # Convert latitude and longitude to radians
        lat_rad = math.radians(lat)
        lon_rad = math.radians(lon)

        # Convert to Cartesian coordinates
        x = R * math.cos(lat_rad) * math.cos(lon_rad)
        y = R * math.cos(lat_rad) * math.sin(lon_rad)
        z = R * math.sin(lat_rad)

        return x, y, z

    def _ecef_to_latlon(self, x: float, y: float, z: float) -> tuple[float, float]:
        # Constants
        R = 6371000  # Earth radius in meters

        # Convert back to latitude and longitude
        lat = math.degrees(math.asin(z / R))
        lon = math.degrees(math.atan2(y, x))

        return lat, lon

    def _smooth_points(self, points: list[tuple[tuple[float, float], int]], order: int) -> None:
        if order < 1 or len(points) <= order:
            # If the order is less than 1 or not enough points, do nothing
            return

        # Extract timestamps and convert latitude, longitude to ECEF coordinates
        timestamps = [point[1] for point in points]
        ecef_coords = [self._latlon_to_ecef(point[0][0], point[0][1]) for point in points]
        x_coords, y_coords, z_coords = zip(*ecef_coords)

        # Fit an n-th order polynomial to each of the Cartesian coordinates
        poly_x = np.polyfit(timestamps, x_coords, order)
        poly_y = np.polyfit(timestamps, y_coords, order)
        poly_z = np.polyfit(timestamps, z_coords, order)

        # Evaluate the polynomial at each timestamp to get smoothed Cartesian coordinates
        smoothed_points = []
        for t in timestamps:
            smoothed_x = np.polyval(poly_x, t)
            smoothed_y = np.polyval(poly_y, t)
            smoothed_z = np.polyval(poly_z, t)
            # Convert smoothed Cartesian coordinates back to latitude and longitude
            smoothed_lat, smoothed_lon = self._ecef_to_latlon(smoothed_x, smoothed_y, smoothed_z)
            # Append smoothed point with the original timestamp
            smoothed_points.append(((smoothed_lat, smoothed_lon), t))

        # Modify the original list in place
        points[:] = smoothed_points

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
   
    def run_thread(self, frequency: int, window_size: int):
        """Sliding Window"""
        while True:
            # Event not properly set
            if not self.event_id or not self.gate:
                sleep(1 / frequency)
                continue
            else:
                logging.debug(f"Event ID: {self.event_id} | Gate: {self.gate} | Status: {self.status}")
                
            points: list[tuple[str, int]] = self.session.query(Dynamics.f_gps, Packet.time).join(Packet, Packet.packet_id == Dynamics.packet_id).filter(Dynamics.packet_id >= self.start_packet).order_by(Dynamics.packet_id.desc()).limit(window_size).all()

            # Not enough points
            if len(points) < window_size:
                logging.warning("Not enough points for computation. Trashing the instance")
                sleep(1 / frequency)
                continue
            
            # Suspicious time deltas
            MAX_TIME_DELTA = 5 * 1000
            if abs(points[len(points) - 1][1] - points[0][1]) > MAX_TIME_DELTA:
                logging.warning(f"Interval is suspicious: {points[len(points) - 1][1] - points[0][1]}ms. Trashing the instance")
                sleep(1 / frequency)
                continue
            
            # Parse points
            df = pd.DataFrame(points, columns=['gps_str', 'timestamp'])
            df['parsed_coordinates'] = df['gps_str'].apply(lambda gps_point: tuple(pd.Series(gps_point, dtype=float)))
            points: list[tuple[tuple[float, float], int]] = list(zip(df['parsed_coordinates'], df['timestamp']))
            
            # logging.info("Data is parsed")
            
            # Smooth points
            self._smooth_points(points=points, order=1)
            
            # logging.info("Data is smoothed")
            
            lap_time = self._track_lap(gate=self.gate, points=points)
            # Get Time of intersect
            # logging.info(f"LAP TIME: {lap_time}")
            if lap_time:
                # Is timestamp valid? 
                if self._is_valid(time=lap_time, delta=5000):
                    # Add time to classifier
                    logging.info(f"Lap Time is Valid")
                    self._record_time(time=lap_time)
                else:
                    logging.warning(f"Lap Time is Not Valid")
            else:
                pass
            
            sleep(1 / frequency)
        
        
    def on_message(self, client: mqtt_client.Client, userdata, msg):
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
                if self.start_packet < data['start_packet']:
                    self.start_packet = data['start_packet']

                if gate_changed:
                    self._upload_gates_to_db(gates=data['gate'])
        except json.JSONDecodeError:
            self.event_id = None
            self.gate = None
            self.status = None
            self.start_packet = None
    

        
def run_processor():
    with get_db("Nightwatch") as nightwatch_session, get_db("Angelique") as angelique_session:
        db_sessions = {'Nightwatch': nightwatch_session, 'Angelique': angelique_session}
        with MQTTHandler(name="lap_timer_processor", target=MQTTTarget.get(), db_sessions=db_sessions) as mqtt:
        
            session = db_sessions["Nightwatch"]
            processor = LapTimerProcessor(session=session)
            
            frequency = 100
            window_size = 200
            
            # Processing thread
            t1 = threading.Thread(target=processor.run_thread, args=(frequency, window_size,))
            t1.start()
            
            mqtt.client.on_message = processor.on_message
            mqtt.subscribe("config/test")