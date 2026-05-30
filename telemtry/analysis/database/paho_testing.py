import argparse
import os
import random
import math
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
from google.protobuf.descriptor import FieldDescriptor
from google.protobuf.message import Message
import pandas as pd
from sqlalchemy import func, select

sys.path.append(str(Path(__file__).parents[2]))
from stack.ingest.mqtt_handler import MQTTHandler, MQTTTarget
from analysis.sql_utils.db_session import get_db, DBTarget
from analysis.sql_utils.query_builder import QueryBuilder
from analysis.sql_utils.models import Packet, AngeliquePacket, OrionPacket
from stack.ingest.protobuf.template_pb2 import SensorData
from stack.ingest.protobuf.angelique_pb2 import AngeliqueSensorData
from stack.ingest.protobuf.can_packets_pb2 import OrionSensorData



class DataTester:
    """
    Class for testing database with random values in correct data types or CSV data.
    """
    def __init__(self, mqtt, seed=None, csv_path=None, mapping_path=None, value_profile: str = "generic"):
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
        self.value_profile = value_profile.lower().strip()
        self._viewer_story_packet = None
        self._viewer_story_data = None
        
        if csv_path:
            self.load_csv(csv_path)
        if mapping_path:
            self.load_mapping(mapping_path)

    @staticmethod
    def _is_int_dtype(dtype: type) -> bool:
        return np.issubdtype(dtype, np.integer) and dtype is not bool

    @staticmethod
    def _is_float_dtype(dtype: type) -> bool:
        return np.issubdtype(dtype, np.floating)

    @staticmethod
    def _cast_numeric(value: float, dtype: type):
        if np.issubdtype(dtype, np.integer):
            return int(round(value))
        if np.issubdtype(dtype, np.floating):
            return float(value)
        return value

    @staticmethod
    def _clamp(value: float, min_value: float, max_value: float) -> float:
        return max(min_value, min(max_value, value))

    @staticmethod
    def _viewer_noise(packet: int, channel: str, scale: float = 1.0) -> float:
        # Deterministic, packet-indexed noise so all table rows for a packet stay coherent.
        phase = (sum(ord(ch) for ch in channel) % 360) * math.pi / 180.0
        return scale * math.sin(packet * 0.173 + phase)

    def _viewer_story(self, packet: int):
        if self._viewer_story_packet == packet and self._viewer_story_data is not None:
            return self._viewer_story_data

        # Use a slightly accelerated sim-time so UI widgets visibly evolve during short demos.
        t = float(packet) * 0.06
        lap_period_s = 18.0
        phase = (t % lap_period_s) / lap_period_s
        theta = 2.0 * math.pi * phase

        corner_intensity = abs(math.sin(theta)) ** 1.35
        straight_intensity = 1.0 - corner_intensity

        throttle_pct = self._clamp(
            22.0 + 68.0 * straight_intensity + self._viewer_noise(packet, "throttle", 3.0),
            0.0,
            100.0,
        )
        brake_pct = self._clamp(
            58.0 * corner_intensity - 12.0 * straight_intensity + self._viewer_noise(packet, "brake", 2.5),
            0.0,
            100.0,
        )
        speed_mps = self._clamp(
            11.0 + 22.0 * straight_intensity + 2.0 * math.sin(2.0 * theta) + self._viewer_noise(packet, "speed", 0.7),
            2.0,
            42.0,
        )
        steer_deg = self._clamp(
            105.0 * math.sin(theta) * corner_intensity + self._viewer_noise(packet, "steer", 2.0),
            -130.0,
            130.0,
        )
        steer_v = self._clamp(
            1.25 + (steer_deg / 135.0) * 1.1 + self._viewer_noise(packet, "steer_v", 0.02),
            0.0,
            2.5,
        )
        heading_deg = (90.0 - math.degrees(theta)) % 360.0
        heading_rad = math.radians(heading_deg)

        lat = 30.289464 + 0.00055 * math.cos(theta)
        lon = -97.735303 + 0.00085 * math.sin(theta)

        longitudinal_accel = self._clamp(
            3.3 * (throttle_pct / 100.0) - 4.1 * (brake_pct / 100.0) - 0.15 + 0.25 * math.sin(4.0 * theta),
            -5.5,
            4.0,
        )
        lateral_accel = self._clamp(3.6 * math.sin(theta) * corner_intensity, -4.2, 4.2)
        vertical_accel = 0.15 * math.sin(7.0 * theta + 0.4)
        yaw_rate = 28.0 * math.sin(theta)

        hv_pack_v = self._clamp(
            410.0 + 22.0 * straight_intensity - 5.0 * corner_intensity + self._viewer_noise(packet, "hv_v", 2.0),
            330.0,
            620.0,
        )
        power_kw = 1.25 * throttle_pct - 1.55 * brake_pct - 6.0 + self._viewer_noise(packet, "power", 2.0)
        hv_current = self._clamp((power_kw * 1000.0) / max(hv_pack_v, 1.0), -220.0, 240.0)
        soc_pct = self._clamp(84.0 + 4.0 * math.sin(t / 40.0) - 0.0035 * t, 18.0, 98.0)
        lv_v = self._clamp(13.2 + 0.18 * math.sin(t / 45.0) + self._viewer_noise(packet, "lv_v", 0.05), 11.8, 14.6)
        lv_current = self._clamp(
            7.0 + 0.16 * throttle_pct + 0.05 * abs(hv_current) + self._viewer_noise(packet, "lv_c", 1.2),
            2.0,
            60.0,
        )

        ambient_temp_c = 27.5 + 3.0 * math.sin(t / 400.0)
        thermal_load = self._clamp((max(power_kw, 0.0) / 95.0) + (speed_mps / 55.0), 0.0, 1.8)
        inverter_temp_c = ambient_temp_c + 13.0 + 24.0 * thermal_load + 1.2 * math.sin(t / 30.0)
        motor_temp_c = ambient_temp_c + 11.0 + 22.0 * thermal_load + 1.0 * math.cos(t / 35.0)
        coolant_temp_c = ambient_temp_c + 9.0 + 16.0 * thermal_load + 0.8 * math.sin(t / 40.0)
        battery_temp_c = ambient_temp_c + 7.0 + 14.0 * thermal_load + 0.7 * math.sin(t / 50.0)
        fan_rpm = self._clamp(
            900.0
            + max(inverter_temp_c - 45.0, 0.0) * 95.0
            + max(battery_temp_c - 40.0, 0.0) * 80.0,
            800.0,
            6000.0,
        )
        odometer = 1240.0 + 0.018 * t

        steer_factor = steer_deg / 130.0
        flw_speed = self._clamp(speed_mps * (1.0 - 0.08 * steer_factor) + self._viewer_noise(packet, "flw", 0.4), 0.0, 45.0)
        frw_speed = self._clamp(speed_mps * (1.0 + 0.08 * steer_factor) + self._viewer_noise(packet, "frw", 0.4), 0.0, 45.0)
        blw_speed = self._clamp(speed_mps * (1.0 - 0.06 * steer_factor) + self._viewer_noise(packet, "blw", 0.35), 0.0, 45.0)
        brw_speed = self._clamp(speed_mps * (1.0 + 0.06 * steer_factor) + self._viewer_noise(packet, "brw", 0.35), 0.0, 45.0)

        cell_v_base = self._clamp(3.45 + (soc_pct / 100.0) * 0.72, 3.2, 4.2)
        cells_v = [
            self._clamp(
                cell_v_base + 0.03 * math.sin(theta + i * 0.7) + self._viewer_noise(packet, f"cell_v_{i}", 0.01),
                3.2,
                4.22,
            )
            for i in range(12)
        ]
        cells_temp = [
            self._clamp(
                battery_temp_c + 1.8 * math.sin(theta + i * 0.6) + self._viewer_noise(packet, f"cell_t_{i}", 0.35),
                20.0,
                75.0,
            )
            for i in range(12)
        ]

        fault_cycle = int(t) % 210
        fault_active = 160 <= fault_cycle <= 163
        fault_leg = int((t / 210.0) % 4) + 1
        shutdown_legs = {
            f"shutdown_leg{i}": (not fault_active or i != fault_leg)
            for i in range(1, 13)
        }

        ride_height_mm = {
            "fl": 52.0 + 4.0 * math.sin(theta + 0.2),
            "fr": 52.0 + 4.0 * math.sin(theta - 0.2),
            "bl": 51.0 + 3.5 * math.sin(theta + 0.4),
            "br": 51.0 + 3.5 * math.sin(theta - 0.4),
        }

        story = {
            "t": t,
            "lat": lat,
            "lon": lon,
            "speed_mps": speed_mps,
            "flw_speed": flw_speed,
            "frw_speed": frw_speed,
            "blw_speed": blw_speed,
            "brw_speed": brw_speed,
            "throttle_pct": throttle_pct,
            "brake_pct": brake_pct,
            "steer_deg": steer_deg,
            "steer_v": steer_v,
            "heading_deg": heading_deg,
            "heading_rad": heading_rad,
            "longitudinal_accel": longitudinal_accel,
            "lateral_accel": lateral_accel,
            "vertical_accel": vertical_accel,
            "yaw_rate": yaw_rate,
            "hv_pack_v": hv_pack_v,
            "hv_current": hv_current,
            "soc_pct": soc_pct,
            "lv_v": lv_v,
            "lv_current": lv_current,
            "ambient_temp_c": ambient_temp_c,
            "inverter_temp_c": inverter_temp_c,
            "motor_temp_c": motor_temp_c,
            "coolant_temp_c": coolant_temp_c,
            "battery_temp_c": battery_temp_c,
            "fan_rpm": fan_rpm,
            "odometer": odometer,
            "cells_v": cells_v,
            "cells_temp": cells_temp,
            "avg_cell_v": float(np.mean(cells_v)),
            "avg_cell_temp": float(np.mean(cells_temp)),
            "contactor_state": 2 if not fault_active else 0,
            "hvc_state_machine": 5 if not fault_active else 2,
            "shutdown_current": self._clamp(0.6 + abs(hv_current) * 0.04, 0.0, 20.0),
            "contactors_closed": not fault_active,
            "fault_active": fault_active,
            "shutdown_legs": shutdown_legs,
            "ride_height_mm": ride_height_mm,
        }

        self._viewer_story_packet = packet
        self._viewer_story_data = story
        return story

    def _viewer_scalar_value(self, col: str, dtype: type, packet: int):
        """
        Generate coherent, plausible scalar telemetry values for live-viewer demos.
        """
        story = self._viewer_story(packet)
        lower_col = col.lower()

        if dtype is bool:
            if lower_col.startswith("shutdown_leg"):
                leg_digits = ''.join(ch for ch in lower_col if ch.isdigit())
                leg_idx = int(leg_digits) if leg_digits else 1
                return bool(story["shutdown_legs"].get(f"shutdown_leg{leg_idx}", True))
            if lower_col in {
                "neg_hv_contactor",
                "pos_hv_contactor",
                "precharge_contactor",
                "r2d_authorized",
                "r2d_status",
            }:
                return bool(story["contactors_closed"])
            if lower_col in {"cells_v_balanced", "enable", "direction"}:
                return True
            if any(token in lower_col for token in ("error", "fault", "disconnect", "implause", "mismatch", "fail", "damaged")):
                return bool(story["fault_active"] and self._viewer_noise(packet, lower_col, 1.0) > 0.6)
            return bool(self._viewer_noise(packet, lower_col, 1.0) > 0.15)

        if not (self._is_int_dtype(dtype) or self._is_float_dtype(dtype)):
            return None

        if lower_col in {"odometer"}:
            return self._cast_numeric(story["odometer"], dtype)
        if lower_col in {"time_since_on"}:
            return self._cast_numeric(story["t"], dtype)
        if lower_col.endswith("_last_seen_s"):
            board_name = lower_col.removesuffix("_last_seen_s")
            # Viewer profile shows a healthy car; all boards active (<1s).
            # Stream dropout (no active publish) is detected by the dashboard via elapsed time.
            healthy_s = 0.03 + abs(self._viewer_noise(packet, board_name, 0.09))
            return self._cast_numeric(healthy_s, dtype)

        if lower_col in {"hv_soc"}:
            return self._cast_numeric(story["soc_pct"], dtype)
        if lower_col in {"hv_pack_v", "hv_tractive_v", "dc_bus_v", "bus_voltage", "batt_v"}:
            return self._cast_numeric(story["hv_pack_v"], dtype)
        if lower_col in {"hv_c", "dc_bus_current", "batt_c"}:
            return self._cast_numeric(story["hv_current"], dtype)

        if lower_col in {"avg_cell_v"}:
            return self._cast_numeric(story["avg_cell_v"], dtype)
        if lower_col in {"cell_min_v"}:
            return self._cast_numeric(min(story["cells_v"]), dtype)
        if lower_col in {"cell_max_v"}:
            return self._cast_numeric(max(story["cells_v"]), dtype)
        if lower_col in {"avg_cell_temp"}:
            return self._cast_numeric(story["avg_cell_temp"], dtype)

        if lower_col in {"lv_v", "lv_batt_v"}:
            return self._cast_numeric(story["lv_v"], dtype)
        if lower_col in {"lv_c", "lv_batt_c", "lv_boards_current", "lights_current"}:
            return self._cast_numeric(story["lv_current"], dtype)

        if lower_col in {"contactor_state"}:
            return self._cast_numeric(story["contactor_state"], dtype)
        if lower_col in {"hvc_state_machine"}:
            return self._cast_numeric(story["hvc_state_machine"], dtype)
        if lower_col in {"shutdown_current"}:
            return self._cast_numeric(story["shutdown_current"], dtype)

        if lower_col in {"steer_col_angle"}:
            return self._cast_numeric(story["steer_deg"], dtype)
        if lower_col == "fl_steer_angle":
            return self._cast_numeric(story["steer_deg"] * 0.94, dtype)
        if lower_col == "fr_steer_angle":
            return self._cast_numeric(story["steer_deg"] * 1.06, dtype)
        if lower_col in {"steer_v"}:
            return self._cast_numeric(story["steer_v"], dtype)

        if lower_col in {"flw_speed"}:
            return self._cast_numeric(story["flw_speed"], dtype)
        if lower_col in {"frw_speed"}:
            return self._cast_numeric(story["frw_speed"], dtype)
        if lower_col in {"blw_speed"}:
            return self._cast_numeric(story["blw_speed"], dtype)
        if lower_col in {"brw_speed"}:
            return self._cast_numeric(story["brw_speed"], dtype)
        if lower_col in {"wheel_speed", "dash_speed", "gps_velocity", "f_gps_velocity", "b_gps_velocity"}:
            return self._cast_numeric(story["speed_mps"], dtype)
        if lower_col in {"motor_speed"}:
            return self._cast_numeric(story["speed_mps"] * 165.0, dtype)
        if lower_col in {"inverter_rpm", "rpm_request"}:
            return self._cast_numeric(story["speed_mps"] * 170.0, dtype)

        if lower_col in {"accel_pedal_travel", "apps1_travel", "apps2_travel", "accel_pedal_t"}:
            throttle = story["throttle_pct"]
            if lower_col.endswith("_t"):
                throttle /= 100.0
            return self._cast_numeric(throttle, dtype)
        if lower_col in {"bpps1_travel", "bpps2_travel", "brake_pedal_t", "bpps1_t", "bpps2_t"}:
            brake = story["brake_pct"]
            if lower_col.endswith("_t"):
                brake /= 100.0
            return self._cast_numeric(brake, dtype)
        if lower_col in {"apps1_v", "apps2_v", "bse1_v", "bse2_v", "bse3_v", "bpps1_v", "bpps2_v"}:
            if lower_col.startswith("apps"):
                return self._cast_numeric(0.4 + 4.3 * (story["throttle_pct"] / 100.0), dtype)
            return self._cast_numeric(0.4 + 4.3 * (story["brake_pct"] / 100.0), dtype)
        if lower_col in {"apps1_t", "apps2_t"}:
            return self._cast_numeric(story["throttle_pct"] / 100.0, dtype)
        if "brake_pressure" in lower_col:
            return self._cast_numeric(6.0 * (story["brake_pct"] / 100.0), dtype)
        if lower_col in {"brake_bias"}:
            return self._cast_numeric(52.0 + self._viewer_noise(packet, "brake_bias", 2.0), dtype)
        if lower_col in {"brake_light_pct"}:
            return self._cast_numeric(story["brake_pct"], dtype)

        if lower_col in {"torque_request", "torque_command", "torque_feedback", "commanded_torque", "torque_limit"}:
            torque = self._clamp((story["throttle_pct"] - story["brake_pct"]) * 2.1, -220.0, 220.0)
            if lower_col == "torque_limit":
                torque = max(torque, 0.0) + 70.0
            return self._cast_numeric(torque, dtype)
        if lower_col in {"motor_angle"}:
            return self._cast_numeric((packet * 7) % 360, dtype)
        if lower_col in {"inverter_freq"}:
            return self._cast_numeric(40.0 + story["speed_mps"] * 4.5, dtype)
        if lower_col in {"flow_rate", "motor_loop_flow_rate"}:
            return self._cast_numeric(20.0 + story["speed_mps"] * 0.35, dtype)
        if lower_col in {"rad_fan_set", "batt_fan_set"}:
            return self._cast_numeric(min(100.0, story["fan_rpm"] / 60.0), dtype)
        if lower_col in {"rad_fan_rpm", "batt_fan_rpm", "motor_loop_rad_fan_speed", "batt_loop_rad_fan_speed"}:
            return self._cast_numeric(story["fan_rpm"], dtype)
        if lower_col in {"phase_a_current", "phase_b_current", "phase_c_current"}:
            phase_offset = {"phase_a_current": 0.0, "phase_b_current": 0.2, "phase_c_current": -0.2}[lower_col]
            return self._cast_numeric(story["hv_current"] * (1.0 + phase_offset), dtype)
        if lower_col in {"hv_charge_state", "lv_charge_state"}:
            return self._cast_numeric(1.0 if story["hv_current"] < -5.0 else 0.0, dtype)
        if lower_col in {"precharge_r_temp"}:
            return self._cast_numeric(story["coolant_temp_c"] + 2.5, dtype)
        if lower_col in {"discharge_r_temp"}:
            return self._cast_numeric(story["coolant_temp_c"] + 3.5, dtype)
        if lower_col in {"cell_top_temp", "cell_bottom_temp"}:
            return self._cast_numeric(story["battery_temp_c"] + self._viewer_noise(packet, lower_col, 1.2), dtype)
        if lower_col in {"vab_vq_v", "vbc_vd_v", "neutral_output_v"}:
            return self._cast_numeric(0.12 * story["hv_pack_v"], dtype)
        if lower_col in {"delta_resolver_angle"}:
            return self._cast_numeric((packet * 11) % 360, dtype)
        if lower_col in {"ride_height"}:
            return self._cast_numeric(np.mean(list(story["ride_height_mm"].values())), dtype)
        if lower_col in {"fl_ride_height", "fr_ride_height", "bl_ride_height", "br_ride_height"}:
            corner = lower_col[:2]
            return self._cast_numeric(story["ride_height_mm"][corner], dtype)
        if lower_col in {"fl_sus_pot_v", "fr_sus_pot_v", "bl_sus_pot_v", "br_sus_pot_v", "sus1_v", "sus2_v"}:
            if lower_col.startswith("fl"):
                ride_height = story["ride_height_mm"]["fl"]
            elif lower_col.startswith("fr"):
                ride_height = story["ride_height_mm"]["fr"]
            elif lower_col.startswith("bl"):
                ride_height = story["ride_height_mm"]["bl"]
            elif lower_col.startswith("br"):
                ride_height = story["ride_height_mm"]["br"]
            else:
                ride_height = np.mean(list(story["ride_height_mm"].values()))
            return self._cast_numeric(self._clamp(ride_height / 15.0, 0.5, 4.5), dtype)
        if lower_col in {"fl_strain_gauge_v", "fr_strain_gauge_v", "bl_strain_gauge_v", "br_strain_gauge_v"}:
            return self._cast_numeric(2.5 + 0.18 * story["lateral_accel"] + self._viewer_noise(packet, lower_col, 0.1), dtype)

        if lower_col.endswith("_heading"):
            return self._cast_numeric(story["heading_deg"], dtype)
        if lower_col.endswith("_velocity") or lower_col.endswith("_v"):
            return self._cast_numeric(story["speed_mps"], dtype)

        if "ambient" in lower_col and "temp" in lower_col:
            return self._cast_numeric(story["ambient_temp_c"], dtype)
        if "inverter" in lower_col and "temp" in lower_col:
            return self._cast_numeric(story["inverter_temp_c"], dtype)
        if "motor" in lower_col and "temp" in lower_col:
            return self._cast_numeric(story["motor_temp_c"], dtype)
        if ("coolant" in lower_col or "water" in lower_col or "rad_temp" in lower_col) and "temp" in lower_col:
            return self._cast_numeric(story["coolant_temp_c"], dtype)
        if ("cell" in lower_col and "temp" in lower_col) or ("batt" in lower_col and "temp" in lower_col):
            return self._cast_numeric(story["battery_temp_c"], dtype)
        if "temp" in lower_col:
            return self._cast_numeric(story["battery_temp_c"] + self._viewer_noise(packet, lower_col, 1.0), dtype)

        if lower_col.endswith("_speed"):
            return self._cast_numeric(story["speed_mps"], dtype)

        # Fallback to visible-changing values for UI demos.
        if self._is_float_dtype(dtype):
            return float(50.0 + 20.0 * self._viewer_noise(packet, lower_col, 1.0))
        return int(round(50.0 + 20.0 * self._viewer_noise(packet, lower_col, 1.0)))

    def _viewer_vector_value(self, col: str, dtype: type, ndims: int, packet: int):
        story = self._viewer_story(packet)
        lower_col = col.lower()

        def cast_vector(values):
            if np.issubdtype(dtype, np.integer):
                return [int(round(v)) for v in values]
            if np.issubdtype(dtype, np.floating):
                return [float(v) for v in values]
            return values

        if lower_col in {"gps", "f_gps", "b_gps"} and ndims == 1:
            lat = float(story["lat"])
            lon = float(story["lon"])
            return cast_vector([lat, lon])

        if "cells_v" in lower_col and ndims == 1:
            return cast_vector(list(story["cells_v"]))

        if "cells_temp" in lower_col and ndims == 1:
            return cast_vector(list(story["cells_temp"]))

        if lower_col in {"vcu_position"} and ndims == 1:
            return cast_vector([story["lat"], story["lon"], story["odometer"]])

        if lower_col in {"vcu_velocity"} and ndims == 1:
            vx = story["speed_mps"] * math.cos(story["heading_rad"])
            vy = story["speed_mps"] * math.sin(story["heading_rad"])
            return cast_vector([vx, vy, 0.0])

        if lower_col in {"vcu_accel", "cent_mass_accel", "gps_imu"} and ndims == 1:
            return cast_vector([
                story["longitudinal_accel"],
                story["lateral_accel"],
                story["vertical_accel"],
            ])

        if lower_col in {"wheel_speed"} and ndims == 1:
            return cast_vector([
                story["flw_speed"],
                story["frw_speed"],
                story["blw_speed"],
                story["brw_speed"],
            ])

        if ("accel" in lower_col or "gyro" in lower_col) and ndims == 1:
            accel_offsets = {
                "fl": (-0.15, 0.25),
                "fr": (-0.15, -0.25),
                "bl": (0.15, 0.25),
                "br": (0.15, -0.25),
            }
            corner = lower_col[:2] if lower_col[:2] in accel_offsets else None
            long_offset, lat_offset = accel_offsets.get(corner, (0.0, 0.0))

            if "gyro" in lower_col or "ang_rate" in lower_col:
                return cast_vector([
                    0.02 * story["longitudinal_accel"],
                    0.02 * story["lateral_accel"],
                    story["yaw_rate"],
                ])

            return cast_vector([
                story["longitudinal_accel"] + long_offset,
                story["lateral_accel"] + lat_offset,
                story["vertical_accel"],
            ])

        return None

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

    @staticmethod
    def get_next_packet_id(target: str) -> int:
        """
        Query the selected car DB for max packet_id and return max + 1.

        :param target: car name (Nightwatch, Angelique, Orion)
        :return: next packet_id to use
        """
        packet_model = {
            "Nightwatch": Packet,
            "Angelique": AngeliquePacket,
            "Orion": OrionPacket,
        }.get(target, Packet)

        with get_db(target) as session:
            latest_packet_id = session.scalar(select(func.max(packet_model.packet_id)))

        next_packet_id = (int(latest_packet_id) if latest_packet_id is not None else 0) + 1
        logging.info(f"Starting packet IDs for {target} at {next_packet_id} (latest={latest_packet_id})")
        return next_packet_id
    
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
            # Special-case time: CSV "Time" is in seconds; DB/protobuf expects milliseconds.
            if db_col == 'time':
                if isinstance(csv_col, list):
                    raise ValueError('Mapping for "time" must be a single CSV column name, not a list.')
                time_s = csv_row[csv_col]
                try:
                    row[db_col] = int(round(float(time_s) * 1000.0))
                except (TypeError, ValueError) as e:
                    raise ValueError(f'Non-numeric CSV time value {time_s!r} in column {csv_col!r}') from e
                continue

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

    @staticmethod
    def _root_proto_message(target: str):
        if target == "Angelique":
            return AngeliqueSensorData
        if target == "Orion":
            return OrionSensorData
        return SensorData

    @staticmethod
    def _proto_scalar_dtype(field_type: int):
        if field_type in (FieldDescriptor.TYPE_FLOAT, FieldDescriptor.TYPE_DOUBLE):
            return float
        if field_type in (
            FieldDescriptor.TYPE_INT32,
            FieldDescriptor.TYPE_INT64,
            FieldDescriptor.TYPE_UINT32,
            FieldDescriptor.TYPE_UINT64,
            FieldDescriptor.TYPE_SINT32,
            FieldDescriptor.TYPE_SINT64,
            FieldDescriptor.TYPE_FIXED32,
            FieldDescriptor.TYPE_FIXED64,
            FieldDescriptor.TYPE_SFIXED32,
            FieldDescriptor.TYPE_SFIXED64,
            FieldDescriptor.TYPE_ENUM,
        ):
            return int
        if field_type == FieldDescriptor.TYPE_BOOL:
            return bool
        if field_type == FieldDescriptor.TYPE_STRING:
            return str
        if field_type == FieldDescriptor.TYPE_BYTES:
            return bytes
        if field_type == FieldDescriptor.TYPE_MESSAGE:
            return dict
        return float

    @classmethod
    def get_proto_desc(cls, target: str, tables=None, rm_cols=None):
        if isinstance(tables, str):
            tables = [tables]
        requested = set(tables) if tables else None

        proto_message = cls._root_proto_message(target)
        desc = {}
        for table_field in proto_message.DESCRIPTOR.fields:
            if table_field.type != FieldDescriptor.TYPE_MESSAGE:
                continue
            table_name = table_field.name
            if requested and table_name not in requested:
                continue
            table_desc = {}
            for column in table_field.message_type.fields:
                dtype = cls._proto_scalar_dtype(column.type)
                if hasattr(column, "is_repeated"):
                    is_repeated = bool(column.is_repeated)
                else:
                    is_repeated = column.label == FieldDescriptor.LABEL_REPEATED
                ndims = 1 if is_repeated else 0
                table_desc[column.name] = (dtype, ndims)
            desc[table_name] = table_desc

        if isinstance(rm_cols, str):
            rm_cols = [rm_cols]
        if rm_cols:
            for table in list(desc.keys()):
                cols_to_remove = rm_cols if isinstance(rm_cols, list) else rm_cols.get(table, [])
                for col in cols_to_remove:
                    desc[table].pop(col, None)
        return desc

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
            elif col == 'cells_temp' and self.value_profile != "viewer":
                # row[col] = np.random.randint(1, 101, size=(4, 5)).tolist()
                row[col] = np.random.randint(1, 101, size=140).tolist()
            elif dtype is datetime.datetime:
                row[col] = datetime.date.today()
            elif dtype is dict:
                # row[col] = Jsonb({'fake_jsonb_data': self.get_random_data(int, 3)})
                row[col] = {'fake_jsonb_data': self.get_random_data(int, 3)} 
            elif dtype == 'point' or dtype == "POINT" or (isinstance(dtype, str) and dtype.lower() == 'point') or (isinstance(dtype, str) and dtype.lower() == 'POINT'):
                if self.value_profile == "viewer":
                    story = self._viewer_story(packet)
                    row[col] = (story["lat"], story["lon"])
                else:
                    point = random.choice([(30.289464, -97.735303), (30.389670, -97.728152)])
                    row[col] = point
            elif dtype is bytearray or dtype is bytes:
                row[col] = secrets.token_bytes(16)
            else:
                if self.value_profile == "viewer":
                    if ndims == 0:
                        demo_scalar = self._viewer_scalar_value(col, dtype, packet)
                        if demo_scalar is not None:
                            row[col] = demo_scalar
                            continue
                    elif ndims in (1, 2):
                        demo_vector = self._viewer_vector_value(col, dtype, ndims, packet)
                        if demo_vector is not None:
                            row[col] = demo_vector
                            continue

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
        target = kwargs.get('target', 'Orion')
        start_packet_id = kwargs.pop('start_packet_id', self.get_next_packet_id(target))

        # for i in range(num_rows) if kwargs.get('verbose') else tqdm(range(num_rows)):
        for i in range(num_rows) if kwargs.get('verbose') else tqdm(range(num_rows)):
            packet_id = start_packet_id + i
            if use_csv and self.csv_data is not None and self.mapping is not None:
                row = self.create_row_from_csv(packet_id, table_desc)
            else:
                row = self.create_row(table_desc, packet_id)
            if kwargs.get('verbose') and (num_rows < 1000 or not i % (num_rows // 100)):
                logging.info(f'Publishing payload #{i:>3} to {table}: {row}')
            if target == "Angelique":
                self.mqtt.publish(f'angelique/{table}', pickle.dumps(row), qos=0)
            elif target == "Nightwatch":
                self.mqtt.publish(f'nightwatch/{table}', pickle.dumps(row), qos=0)
            elif target == "Orion":
                self.mqtt.publish(f'orion/{table}', pickle.dumps(row), qos=0)
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
    
    def send_proto_rows(self, tables:list,  num_rows:int, delay:float, rm_cols = None, use_csv=False,
                        schema_source: str = "proto", **kwargs):
        target = kwargs.get('target', 'Nightwatch')
        schema_source = schema_source.lower().strip()
        if schema_source == "proto":
            db_desc = self.get_proto_desc(target=target, tables=tables, rm_cols=rm_cols)
        elif schema_source == "orm":
            db_desc = self.get_desc(tables=tables, rm_cols=rm_cols, **kwargs)
        else:
            raise ValueError(f"Invalid schema_source: {schema_source}. Expected 'proto' or 'orm'.")
        if not db_desc:
            raise ValueError(f"No tables selected for {target} using schema_source={schema_source}.")

        start_packet_id = kwargs.get('start_packet_id', self.get_next_packet_id(target))
        if target == "Angelique":
            topic = 'angelique/data'
        elif target == "Nightwatch":
            topic = 'nightwatch/data'
        elif target == "Orion":
            topic = 'orion/data'
        else:
            topic = 'data'
        for i in tqdm(range(num_rows)):
            packet_id = start_packet_id + i
            data = self.create_proto_message(packet_id, db_desc, use_csv=use_csv, target=target)
            self.mqtt.publish(topic, data.SerializeToString(), qos=0)
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

        if target == "Angelique":
            data = AngeliqueSensorData()
        elif target == "Orion":
            data = OrionSensorData()
        else:
            data = SensorData()
        data.packet_id = int(packet)
        data.time = int(time.time() * 1000)
        
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
                            if (isinstance(value, list) or isinstance(value, tuple)):
                                getattr(table_instance, key).extend(value)
                            elif isinstance(value, dict):
                                setattr(table_instance, key, json.dumps(value))
                            else:
                                setattr(table_instance, key, value)
            else:
                for key, value in row.items(): # Edit packet and time
                    if hasattr(data, key):  
                        setattr(data, key, int(value))
        return data
    
    def send_base64_row(self, ver: int, high_freq=True):
        with open(os.getcwd().split('LHR')[0] + f'/LHR/stack/ingest/car_configs/version{ver:02}.json', 'r') as f:
            config = json.load(f)['high' if high_freq else 'low']
        str_to_np = {np_clas.__name__: np_clas for np_clas in getattr(getattr(sys.modules[__name__], 'np'), 'ScalarType')}
        scalar_or_list = lambda val, scalar: val.tolist()[0] if scalar else val.tolist()
        payload_str = ver.to_bytes(1, 'big') + b''.join([scalar_or_list((np.array(
            self.get_random_data(str_to_np[col_spec['type']], size=(shape := col_spec.get('shape', (1,)))),
            dtype=col_spec['type']) / col_spec.get('multiplier', 1)).flatten().reshape(shape), shape == (1,)).tobytes()
            for col, col_spec in config.items()])
        self.mqtt.publish('/h' if high_freq else '/l', payload_str, qos=1)


if __name__ == '__main__':
    logging.basicConfig(level=logging.INFO)
    parser = argparse.ArgumentParser(description="Publish telemetry test data through MQTT.")
    parser.add_argument(
        "--car",
        choices=["Nightwatch", "Angelique", "Orion"],
        default="Orion",
        help="Target car stream to publish.",
    )
    parser.add_argument(
        "--profile",
        choices=["viewer", "generic"],
        default="viewer",
        help="viewer: realistic ranges for live widget demos; generic: legacy [0,1)-heavy random ranges.",
    )
    parser.add_argument(
        "--schema-source",
        choices=["proto", "orm"],
        default="proto",
        help="Schema source for protobuf field generation. proto follows compiled protobuf definitions (SDD source of truth).",
    )
    parser.add_argument("--rows", type=int, default=500000, help="Number of protobuf rows to publish.")
    parser.add_argument("--delay", type=float, default=None, help="Delay between publishes in seconds.")
    args = parser.parse_args()

    car_name = args.car

    # Optional Paths for CSV and mapping files
    csv_path = Path(__file__).parent / 'csv_processing/csv_data/Log__2024_10_11__05_50_47.csv'
    mapping_path = Path(__file__).parent / 'csv_processing/angelique_pg_to_csv.json'

    delay = args.delay
    if delay is None:
        # The viewer profile is for stable UI updates without overwhelming the front-end.
        delay = 0.02 if args.profile == "viewer" else 0.001
    
    with get_db("Nightwatch") as nightwatch_session, get_db("Angelique") as angelique_session, get_db("Orion") as orion_session:
        db_sessions = {
            'Nightwatch': nightwatch_session,
            'Angelique': angelique_session,
            'Orion': orion_session,
        }
        with MQTTHandler('paho_test', db_sessions=db_sessions, target=MQTTTarget.get()) as mqtt:
            # Protobuf message testing with CSV data
            dt = DataTester(
                mqtt=mqtt,
                seed=42,
                csv_path=None,
                mapping_path=None,
                value_profile=args.profile,
            )
            start_packet_id = dt.get_next_packet_id(car_name)
            proto_tables = list(dt.get_proto_desc(target=car_name).keys())
            selected_tables = proto_tables
            if args.schema_source == "orm":
                available_tables = set(dt.get_desc(db=True, target=car_name).keys())
                selected_tables = [table for table in proto_tables if table in available_tables]
                missing_tables = [table for table in proto_tables if table not in available_tables]
                if missing_tables:
                    logging.warning(
                        "ORM schema missing %s tables: %s",
                        car_name,
                        ", ".join(missing_tables),
                    )

            logging.info(
                "Publishing %s rows for %s using %s schema over tables: %s",
                args.rows,
                car_name,
                args.schema_source,
                ", ".join(selected_tables),
            )
            dt.send_proto_rows(
                tables=selected_tables,
                num_rows=args.rows,
                delay=delay,
                use_csv=False,
                schema_source=args.schema_source,
                target=car_name,
                start_packet_id=start_packet_id,
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
