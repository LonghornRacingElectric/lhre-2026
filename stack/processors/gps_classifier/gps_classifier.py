import json
import os
import logging
from time import sleep
import pandas as pd
import numpy as np
from paho.mqtt import client as mqtt_client
import threading
from psycopg import logger
from enum import Enum
from numpy.typing import NDArray
import warnings

warnings.simplefilter('ignore', np.linalg.LinAlgError)


if os.getenv('IN_DOCKER'):
    from db_handler import DBHandler, DBTarget, get_table_column_specs    # Cheesed import statement using bind mount
    from mqtt_handler import MQTTHandler
else:
    from analysis.sql_utils.db_handler import DBHandler, DBTarget, get_table_column_specs
    from stack.ingest.mqtt_handler import MQTTHandler


class ProcessType(Enum):
    LINEAR_ACCELERATION = 0
    TURN = 1
    

class Event:    
    def __init__(self, type: ProcessType, starting_time: int, starting_heading: float, event_id: int = -9999):
        self.type: ProcessType = type
        self.starting_time: int = starting_time
        
        self.starting_heading: float | None = starting_heading
        self.max_speed_time: int | None = None
        
        self.event_id = event_id
    
    def _stop_event(self, time: int):
        """
        Kills any ongoing process and adds data to classifier table
        ARGS:
            time: the end time of the event
        """
        
        if self.type == ProcessType.LINEAR_ACCELERATION:
            if self.max_speed_time: time = min(time, self.max_speed_time)        
            
        if abs(time - self.starting_time) < 1000: #1 second
            return
        
        db_obj = {
                "event_id": self.event_id,
                "type": "trajectory",
                "start_time": self.starting_time.astype(int),
                "end_time": time,
                "notes": f"{self.type.name}"
        }
        #! Will crash if no event
        DBHandler.insert(table="classifier", data=db_obj, target=DBTarget.LOCAL, user="electric", handler=self.handler)
        
        times.append(self.starting_time)
        events.append(0)
        times.append(self.starting_time)
        events.append(1 if self.type == ProcessType.LINEAR_ACCELERATION else -1)
        
class Process:
    def __init__(self, type: ProcessType, starting_packet: int, starting_time: int, target_time: int):
        self.type: ProcessType = type
        self.starting_packet: int = starting_packet
        self.starting_time: int = starting_time
        self.target_time: int = target_time
        
    
class GPSClassifierProcessor:
    def __init__(self, db_handler: DBHandler=None):
        self.handler = db_handler if db_handler else DBHandler()
        self.event_id = -9999
        self.start_packet: int = 0
        self.status = 0
        
        
        self.VELOCITY_THRESHOLD: float = 4
        
        self.current_process: Process | None = None
        self.processes: list[Process] = []
        self.started_event: Event = None
        
        self.min_time = 0
   
    def _start_event(self, time: int, heading:int, type: ProcessType):
        # Stop any previous process
        if self.started_event and self.started_event.type != type:
            self.started_event._stop_event(time)
            self.started_event = None
        # If previous process is already the same, do nothing
        elif self.started_event and self.started_event.type == type:
            return
        
        self.started_event = Event(type=type, starting_time=time, starting_heading=heading, event_id=self.event_id)
        
        
    def _detect_events(self, points: NDArray, la_threshold: float, t_threshold: float, la_time_window: float, t_time_window: float, check_delay: int):
        """
        Detects for start of linear acceleration or turn events and calls associated functions
        ARGS:
            handler: active handler to call database
            points: array containing torque request, steer voltage, and time info
            la_threshold: threshold for the slope of the linear regression of torque request to trigger an event
            t_threshold: threshold for the slope of the linear regression of steer voltage to trigger an event
            la_time_window: linear acceleration time window to confirm a linear acceleration event (s)
            t_time_window: turn time window to confirm a turn event (s)
            check_delay: delay between each event starts (s)
        """
        
        torque_request = points[:, 1]
        steer_v = points[:, 2]
        time = points[:, 3]
        packet = points[:, 4]
        
        # Not enough time between event starts
        if len(self.processes) > 0 and time[0] < self.processes[-1].starting_time + check_delay * 1000:
            return
        
        # Look for high spike in steer voltage
        avg_steer_voltage = np.average(steer_v) - 1.25 # 1.25 = center
        
        if abs(avg_steer_voltage) > t_threshold:
            starting_packet: int = packet[0]
            starting_time: int = time[0]
            target_time = starting_time + t_time_window * 1000
            new_process = Process(type=ProcessType.TURN, starting_packet=starting_packet, starting_time=starting_time, target_time=target_time)
            self.processes.append(new_process)
            return
        else:
            pass
        
        # Look for high spike in torque request             
        avg_torque_request = np.average(torque_request)
        
        if avg_torque_request > la_threshold:
            starting_packet: int = packet[0]
            starting_time: int = time[0]
            target_time = starting_time + la_time_window * 1000
            new_process = Process(type=ProcessType.LINEAR_ACCELERATION, starting_packet=starting_packet, starting_time=starting_time, target_time=target_time)
            self.processes.append(new_process)
            return
        else:
            pass
                
        
    def process_thread(self, frequency: int, la_threshold: float, t_na_threshold: float, t_h_threshold: float):
        """
        Sequentially handles processes from the class list
        ARGS:
            handler: active handler to call database
            frequency: frequency to run the functino at
            la_threshold: threshold for linear acceleration 
            t_na_threshold: threshold for normal acceleration
            t_h_threshold: threshold for heading
        """
        
        while True:            
            if len(self.processes) == 0:
                sleep(1 / frequency)
                continue
            
            current_process = self.processes.pop(0)
            
            if current_process.starting_time < self.min_time:
                continue
            
            points: NDArray = []
            while True:
                points = np.array(DBHandler.simple_select(f"""
                    SELECT d.gps_heading, d.body3_accel, p.time, d.gps_velocity
                    FROM dynamics d
                    JOIN packet p ON p.packet_id = d.packet_id
                    WHERE p.time > {current_process.target_time}
                    AND p.time <= (
                        SELECT MIN(time) 
                        FROM packet 
                        WHERE time > {current_process.target_time}
                    )
                    AND p.time >= {current_process.starting_time}
                    ORDER BY p.time ASC
                    LIMIT 500
                    """, handler=self.handler, target=DBTarget.LOCAL), dtype=object)
                if points.any(): break
                sleep(1 / frequency)
                            
            body3_accel = points[:, 1]
            body3_accel = np.array([list(row) for row in body3_accel])
            tangential_accel = body3_accel[:, 0]
            normal_accel = body3_accel[:, 1]
            heading = np.asarray(points[:, 0], dtype=np.float64)
            time = np.asarray(points[:, 2], dtype=np.float64)
            
            if current_process and current_process.type == ProcessType.LINEAR_ACCELERATION:
                smoothed_tangential_accel = np.polyfit(time, tangential_accel, 1)
                
                
                if smoothed_tangential_accel[0] > la_threshold:
                    self._start_event(time=current_process.starting_time, heading=heading[0], type=current_process.type)
                else:
                    pass
                
            
            if current_process and current_process.type == ProcessType.TURN:
                smoothed_normal_accel = np.polyfit(time, normal_accel, 1)
                smoothed_heading = np.polyfit(time, heading, 1)
                
                if abs(smoothed_normal_accel[0]) > t_na_threshold or abs(smoothed_heading[0]) > t_h_threshold:
                    self._start_event(time=current_process.starting_time, heading=heading[0], type=current_process.type)
                else:
                    pass
                
            if not self.started_event: continue
            
            # Check if linear acceleration event is still ongoing
            if self.started_event.type == ProcessType.LINEAR_ACCELERATION:
                gps_velocity = points[:, 3]
                # Set marker to highest velocity point to end there
                self.started_event.max_speed_time = time[np.argmax(gps_velocity)]
                if self.started_event.starting_heading:
                    diffs = np.abs(heading - self.started_event.starting_heading)
                    # End at the point where a difference is heading is too big
                    if diffs.max() >= 30:
                        self.min_time = time[np.argmax(diffs)]
                        self.started_event._stop_event(self.min_time)
                        self.started_event = None
            # Same but with turns
            elif self.started_event.type == ProcessType.TURN:
                if self.started_event.starting_heading:
                    diffs = heading - self.started_event.starting_heading
                    if diffs.min() <= 10:
                        self.min_time = time[np.argmax(diffs)]
                        self.started_event._stop_event(self.min_time)
                        self.started_event = None
   
    def run_thread(self, frequency: int, window_size: int, la_threshold: float, t_threshold: float, la_time_window: float, t_time_window: float, debug=True):
        """Sliding Window"""
        
        while True:
            # Event not properly set
            if not debug and (not self.event_id or self.status != 1):
                sleep(1 / frequency)
                continue
            
            # Not enough points
            if not debug and len(points) < window_size:
                logging.warning("Not enough points for computation. Trashing the instance")
                sleep(1 / frequency)
                continue
            
            # Suspicious time deltas
            MAX_TIME_DELTA = 5 * 1000
            if not debug and points[len(points) - 1][1] - points[0][1] < MAX_TIME_DELTA:
                logging.error(f"Interval is suspicious: {points[len(points) - 1][1] - points[0][1]}ms. Trashing the instance")
                sleep(1 / frequency)
                continue
            
            points = np.array(DBHandler.simple_select(f"""SELECT d.gps_velocity, d.torque_request, c.steer_v, p.time, p.packet_id 
                                                          FROM dynamics d 
                                                          JOIN packet p ON p.packet_id = d.packet_id 
                                                          JOIN controls c ON c.packet_id = d.packet_id 
                                                          WHERE d.packet_id >= {self.start_packet} 
                                                          ORDER BY d.packet_id ASC 
                                                          LIMIT {window_size}""", handler=self.handler, target=DBTarget.LOCAL))
            
            if len(points) > 0:
                # Checks if at rest
                if abs(points[:, 0].mean()) < self.VELOCITY_THRESHOLD:
                    sleep(1 / frequency)
                    if self.started_event:
                        self.started_event._stop_event(points[:, 3][-1])
                        self.started_event = None
                    
                    
                else:
                    self._detect_events(points=points, la_threshold=la_threshold, t_threshold=t_threshold, la_time_window=la_time_window, t_time_window=t_time_window, check_delay=2)
                # increment window by half size
                self.start_packet = points[int(len(points) / 2)][4]

            sleep(1 / frequency)
        
    def on_message(self, client: mqtt_client.Client, userdata, msg):
        try:
            logging.info(f"MESSAGE HAS BEEN RECEIVED AT TOPIC {msg.topic}")
            if msg.topic == 'config/test':
                data = json.loads(msg.payload.decode())
                # Modify object variables
                self.event_id = data['event_id']
                self.status = data['status']
                if self.start_packet < data['start_packet']:
                    self.start_packet = data['start_packet']
        except json.JSONDecodeError:
            self.event_id = None
            self.status = None
            self.start_packet = None
    

        
def run_processor():
    with MQTTHandler(name="gps_classifier_processor") as mqtt:
        
        handler = DBHandler()
        processor = GPSClassifierProcessor(db_handler=handler)

        
        frequency = 500
        window_size = 50
        la_threshold = 7
        t_threshold=0.6
        la_time_window=10
        t_time_window=5
        t1 = threading.Thread(target=processor.run_thread, args=(frequency, window_size, la_threshold, t_threshold, la_time_window, t_time_window))
        t1.start()
        
        frequency = 100
        la_threshold = 0.0006
        t_na_threshold = 10000.004
        t_h_threshold = 0.009
        t2 = threading.Thread(target=processor.process_thread, args=(frequency, la_threshold, t_na_threshold, t_h_threshold))
        t2.start()
        
        mqtt.client.on_message = processor.on_message
        mqtt.subscribe("config/test")
