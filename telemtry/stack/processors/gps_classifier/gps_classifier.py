import json
import os
import logging
from time import sleep
import pandas as pd
import io
import base64
import requests
import numpy as np
from paho.mqtt import client as mqtt_client
import threading
from enum import Enum
from numpy.typing import NDArray
import warnings
import matplotlib.pyplot as plt
from matplotlib.lines import Line2D
import pandas as pd
from sqlalchemy import func
import sys
from pathlib import Path

warnings.simplefilter('ignore', np.linalg.LinAlgError)
warnings.filterwarnings("ignore")

from analysis.sql_utils.db_session import get_db, DBTarget
from analysis.sql_utils.query_builder import QueryBuilder
from stack.ingest.mqtt_handler import MQTTHandler, MQTTTarget
from analysis.sql_utils.models import Classifier, Dynamics, Packet, Controls

if os.getenv('IN_DOCKER'):
    visualizer_image_path = "/app/analysis/database/viewer_tool/static/images"
else:
    visualizer_image_path = "analysis/database/viewer_tool/static/images"


class ProcessType(Enum):
    LINEAR_ACCELERATION = 0
    TURN = 1
    
class Visualizer:
    def __init__(self):        
        self.events = pd.DataFrame(columns=['Time', 'Event'])
        self.gps = pd.DataFrame(columns=['Time', 'Time_Since_Start', 'Latitude', 'Longitude'])
        
        self.starting_time = None
    
    def start_event(self, time: int, event: ProcessType):
        if self.starting_time == None: self.starting_time = time
        self.events.loc[len(self.events)] = {'Time': time - self.starting_time, 'Event': 0}
        self.events.loc[len(self.events)] = {'Time': time - self.starting_time, 'Event': 1 if event == ProcessType.LINEAR_ACCELERATION else -1}
        
    def stop_event(self, time: int, event: ProcessType):
        if self.starting_time == None: self.starting_time = time
        self.events.loc[len(self.events)] = {'Time': time - self.starting_time, 'Event': 1 if event == ProcessType.LINEAR_ACCELERATION else -1}
        self.events.loc[len(self.events)] = {'Time': time - self.starting_time, 'Event': 0}
        
    def save_image(self):
        fig1 = plt.figure()
        ax = fig1.add_subplot(111, projection='3d')
        
        # Labels
        ax.set_title('Events')
        ax.set_xlabel('Longitude')
        ax.set_ylabel('Latitude')
        ax.set_zlabel('Time')
        
        # Remove ticks
        ax.set_xticklabels([])
        ax.set_yticklabels([])
        ax.set_zticklabels([])
        
        
        fig2 = plt.figure()

        color_mapping = {1: 'green', -1: 'red', 0: 'black'}
        colors = [color_mapping[event] for event in self.events['Event']]
        for i in range(len(self.events['Time']) - 1):
            start_index = self.gps['Time_Since_Start'].ge(self.events['Time'][i]).idxmax()
            end_index = self.gps['Time_Since_Start'].ge(self.events['Time'][i + 1]).idxmax()
            
            if start_index >= end_index:
                continue
            
            ax.plot(self.gps['Latitude'].iloc[start_index:end_index].values, self.gps['Longitude'].iloc[start_index:end_index].values, zs=self.gps['Time_Since_Start'].iloc[start_index:end_index].values, color=colors[i])

        if len(self.gps) > 0:
            start_index = 0
            if len(self.events) > 0:
                start_index = self.gps['Time_Since_Start'].ge(self.events['Time'].iloc[-1]).idxmax()
            ax.plot(self.gps['Latitude'].iloc[start_index:len(self.gps)].values, self.gps['Longitude'].iloc[start_index:len(self.gps)].values, zs=self.gps['Time_Since_Start'].iloc[start_index:len(self.gps)].values, color='black')
        np.Inf = np.inf
        # Save the plot to files

        save_path = os.path.join(visualizer_image_path, "events.png")
        os.makedirs(os.path.dirname(save_path), exist_ok=True)
        
        legend_elements = [
            Line2D([0], [0], color='green', lw=2, label='Linear Acceleration'),
            Line2D([0], [0], color='red', lw=2, label='Sharp Turn'),
            Line2D([0], [0], color='black', lw=2, label='Normal Driving')
        ]
        ax.legend(handles=legend_elements, loc='upper right')
        
        fig1.savefig(save_path)
        # logging.info(f"Image saved at {visualizer_image_path}/events.png")
        
    def run_thread(self, sleep_time: int):
        while True:
            self.save_image()
            sleep(sleep_time)

visualizer = Visualizer()
    

class Event:    
    def __init__(self, type: ProcessType, starting_time: int, starting_heading: float, event_id: int = -9999, session=None, table_specs=None):
        self.type: ProcessType = type
        self.starting_time: int = starting_time
        
        self.starting_heading: float | None = starting_heading
        self.max_speed_time: int | None = None
        
        self.event_id = event_id
        
        self.session = session
        self.table_specs = table_specs
    
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
                "start_time": self.starting_time,
                "end_time": time,
                "notes": f"{self.type.name}"
        }
        #! Will crash if no event
        table_desc = self.table_specs[Classifier.__tablename__]
        QueryBuilder.insert(self.session, 'classifier', Classifier, db_obj, table_desc, commit=True)
        
        visualizer.start_event(self.starting_time, self.type)
        visualizer.stop_event(time, self.type)
        
class Process:
    def __init__(self, type: ProcessType, starting_packet: int, starting_time: int, target_time: int):
        self.type: ProcessType = type
        self.starting_packet: int = starting_packet
        self.starting_time: int = starting_time
        self.target_time: int = target_time
        
    
class GPSClassifierProcessor:
    def __init__(self, session=None):
        self.session = session
        self.event_id = -9999
        self.start_packet: int = 0
        self.status = 0
        
        
        self.VELOCITY_THRESHOLD: float = 10
        
        self.current_process: Process | None = None
        self.processes: list[Process] = []
        self.started_event: Event = None
        
        self.min_time = 0
        self.table_specs = QueryBuilder("Nightwatch").get_table_column_specs()

    def _start_event(self, time: int, heading:int, type: ProcessType):
        # Stop any previous process
        if self.started_event and self.started_event.type != type:
            self.started_event._stop_event(time)
            self.started_event = None
        # If previous process is already the same, do nothing
        elif self.started_event and self.started_event.type == type:
            return
        
        
        self.started_event = Event(type=type, starting_time=time, starting_heading=heading, event_id=self.event_id, session=self.session, table_specs=self.table_specs)
        
        
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
                points = np.array(self.session.query(Dynamics.f_gps_heading, Dynamics.br_unsprung_accel, Packet.time, Dynamics.f_gps_velocity)
                                  .join(Packet, Packet.packet_id == Dynamics.packet_id)
                                  .filter(Packet.time > current_process.target_time)
                                  .filter(Packet.time <= self.session.query(func.min(Packet.time)).filter(Packet.time > current_process.target_time))
                                  .filter(Packet.time >= current_process.starting_time)
                                  .order_by(Packet.time.asc())
                                  .limit(500)
                                  .all(), dtype=object)
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
                    if diffs.max() >= 20:
                        self.min_time = time[np.argmax(diffs)]
                        self.started_event._stop_event(self.min_time)
                        self.started_event = None
            # Same but with turns
            elif self.started_event.type == ProcessType.TURN:
                if self.started_event.starting_heading:
                    diffs = heading - self.started_event.starting_heading
                    if diffs.min() <= 15:
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

            points = []
            while len(points) < window_size:
                points = np.array(self.session.query(Dynamics.f_gps_velocity, Controls.accel_pedal_t, Dynamics.steer_col_angle, Packet.time, Packet.packet_id, Dynamics.f_gps)
                                  .join(Packet, Packet.packet_id == Dynamics.packet_id)
                                  .join(Controls, Controls.packet_id == Dynamics.packet_id)
                                  .filter(Dynamics.packet_id >= self.start_packet)
                                  .order_by(Dynamics.packet_id.asc())
                                  .limit(window_size)
                                  .all(), dtype=object)
                sleep(1/frequency)
            
            if len(points) > 0:
                # Convert to DataFrame, keeping only relevant columns
                gps_df = pd.DataFrame(points, columns=['f_gps_velocity', 'accel_pedal_t', 'steer_col_angle', 'Time', 'packet_id', 'gps'])[['Time', 'gps']]

                # Extract latitude & longitude from the `gps` column
                gps_df[['Latitude', 'Longitude']] = gps_df['gps'].apply(lambda gps_tuple: pd.Series(gps_tuple))

                # Append and remove duplicates in one step
                if visualizer.starting_time == None: visualizer.starting_time = gps_df['Time'].iloc[0]
                visualizer.gps = (
                    pd.concat([visualizer.gps, gps_df[['Time', 'Latitude', 'Longitude']]], ignore_index=True)
                    .drop_duplicates()
                    .assign(Time_Since_Start=lambda df: pd.to_numeric(df['Time'] - visualizer.starting_time, errors='coerce'))
                )

                
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
    with get_db("Nightwatch") as nightwatch_session, get_db("Angelique") as angelique_session:
        db_sessions = {'Nightwatch': nightwatch_session, 'Angelique': angelique_session}
        with MQTTHandler(name="gps_classifier_processor", target=MQTTTarget.get(), db_sessions=db_sessions) as mqtt:
            
            processor = GPSClassifierProcessor(session=db_sessions['Nightwatch'])

            frequency = 500
            window_size = 50
            la_threshold = 10
            t_threshold=0.7
            la_time_window=10
            t_time_window=5
            t1 = threading.Thread(target=processor.run_thread, args=(frequency, window_size, la_threshold, t_threshold, la_time_window, t_time_window))
            t1.start()
            
            frequency = 100
            la_threshold = 0.00000000000033
            t_na_threshold = 0.00000000000027
            t_h_threshold = 0.000000000027
            t2 = threading.Thread(target=processor.process_thread, args=(frequency, la_threshold, t_na_threshold, t_h_threshold))
            t2.start()
            
            t3 = threading.Thread(target=visualizer.run_thread, args=(1,))
            t3.start()
            
            mqtt.client.on_message = processor.on_message
            mqtt.subscribe("config/test")