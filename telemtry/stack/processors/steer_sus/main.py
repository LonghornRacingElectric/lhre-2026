import json
import logging
import math
import os
import signal
import time
from pathlib import Path
from typing import Any

from google.protobuf.json_format import MessageToDict
from kafka import KafkaConsumer, KafkaProducer
from stack.ingest.protobuf import angelique_pb2, can_packets_pb2, template_pb2

from kin_backend.core.fmu import FMU
from kin_backend.core.lookup import ShockToWheel

logging.basicConfig(level=os.getenv("LOGLEVEL", "INFO"))
logging.getLogger("kafka").setLevel(logging.WARNING)

CAR_NAME_MAP = {
    "angelique": "Angelique",
    "orion": "Orion",
    "nightwatch": "Nightwatch",
}

BASE = Path(__file__).resolve().parent
LOOKUP_MAP_PATH = BASE / "kin_backend" / "calibration" / "maps.npz"
FRONT_FMU_PATH = BASE / "kin_backend" / "fmus" / "FrKnCFMI.fmu"
REAR_FMU_PATH = BASE / "kin_backend" / "fmus" / "RrKnCFMI.fmu"

TRACK_F = 48.0 * 0.0254
TRACK_R = 48.0 * 0.0254

KAFKA_BOOTSTRAP_SERVERS = os.getenv("KAFKA_BOOTSTRAP_SERVERS", "kafka:9092")
INPUT_TOPIC = os.getenv("KAFKA_INPUT_TOPIC", "sensor_data")
OUTPUT_TOPIC = os.getenv("KAFKA_OUTPUT_TOPIC", "steer_sus")
CONSUMER_GROUP_ID = os.getenv("KAFKA_GROUP_ID", "steer-sus-group")
MAX_POLL_RECORDS = int(os.getenv("KAFKA_MAX_POLL_RECORDS", "200"))
POLL_TIMEOUT_MS = int(os.getenv("KAFKA_POLL_TIMEOUT_MS", "1000"))
SKIP_CATCHUP = os.getenv("STEER_SUS_SKIP_CATCHUP", "1").strip().lower() in {"1", "true", "yes", "on"}
PUBLISH_LOG_INTERVAL_S = float(os.getenv("STEER_SUS_LOG_INTERVAL_S", "1.5"))
OUTPUT_FRAME = os.getenv("STEER_SUS_OUTPUT_FRAME", "viewer").strip().lower()
ROTATE_X_DEG = float(os.getenv("STEER_SUS_ROTATE_X_DEG", "0"))
ROTATE_Y_DEG = float(os.getenv("STEER_SUS_ROTATE_Y_DEG", "0"))
ROTATE_Z_DEG = float(os.getenv("STEER_SUS_ROTATE_Z_DEG", "0"))
ROTATE_X_RAD = math.radians(ROTATE_X_DEG)
ROTATE_Y_RAD = math.radians(ROTATE_Y_DEG)
ROTATE_Z_RAD = math.radians(ROTATE_Z_DEG)

default_car_env = os.getenv("KAFKA_DEFAULT_CAR", "Orion")
DEFAULT_CAR = CAR_NAME_MAP.get(default_car_env.strip().lower(), "Orion")

shutdown_requested = False


def _normalize_car_name(raw_car: str | None) -> str | None:
    if raw_car is None:
        return None
    return CAR_NAME_MAP.get(raw_car.strip().lower())


def _get_car_type_from_headers(headers: list[tuple[str, bytes]] | None) -> str | None:
    if not headers:
        return None

    for key, value in headers:
        key_str = key.decode("utf-8", errors="ignore") if isinstance(key, bytes) else str(key)
        if key_str.lower() != "car_type":
            continue

        value_str = value.decode("utf-8", errors="ignore") if isinstance(value, bytes) else str(value)
        normalized = _normalize_car_name(value_str)
        if normalized:
            return normalized

        logging.warning("Unknown car_type header '%s'; using default car '%s'", value_str, DEFAULT_CAR)
        return None

    return None


def _decode_sensor_payload(payload: bytes, car_type: str) -> dict[str, Any]:
    if car_type == "Angelique":
        row = angelique_pb2.AngeliqueSensorData()
    elif car_type == "Orion":
        row = can_packets_pb2.OrionSensorData()
    else:
        row = template_pb2.SensorData()

    row.ParseFromString(payload)
    return MessageToDict(
        row,
        preserving_proto_field_name=True,
        always_print_fields_with_no_presence=True,
    )


def _to_float(value: Any) -> float | None:
    try:
        parsed = float(value)
    except (TypeError, ValueError):
        return None
    if not math.isfinite(parsed):
        return None
    return parsed


def _first_numeric(mapping: Any, keys: list[str]) -> float | None:
    if not isinstance(mapping, dict):
        return None
    for key in keys:
        if key not in mapping:
            continue
        parsed = _to_float(mapping.get(key))
        if parsed is not None:
            return parsed
    return None


def _find_numeric_deep(root: Any, keys: list[str]) -> float | None:
    if not isinstance(root, dict):
        return None

    stack: list[dict[str, Any]] = [root]
    visited: set[int] = set()

    while stack:
        current = stack.pop()
        current_id = id(current)
        if current_id in visited:
            continue
        visited.add(current_id)

        value = _first_numeric(current, keys)
        if value is not None:
            return value

        for entry in current.values():
            if isinstance(entry, dict):
                stack.append(entry)
            elif isinstance(entry, list):
                for sub_entry in entry:
                    if isinstance(sub_entry, dict):
                        stack.append(sub_entry)

    return None


def _coalesce(*values: float | None) -> float | None:
    for value in values:
        if value is not None:
            return value
    return None


def _clamp(value: float, low: float, high: float) -> float:
    return max(low, min(high, value))


def _linear_map(value: float, in_low: float, in_high: float, out_low: float, out_high: float) -> float:
    if in_high <= in_low:
        return out_low
    t = (_clamp(value, in_low, in_high) - in_low) / (in_high - in_low)
    return out_low + t * (out_high - out_low)


def _rotate_vec3(vec: list[float]) -> list[float]:
    x, y, z = vec

    if ROTATE_X_RAD != 0.0:
        cos_x = math.cos(ROTATE_X_RAD)
        sin_x = math.sin(ROTATE_X_RAD)
        y, z = (y * cos_x - z * sin_x), (y * sin_x + z * cos_x)

    if ROTATE_Y_RAD != 0.0:
        cos_y = math.cos(ROTATE_Y_RAD)
        sin_y = math.sin(ROTATE_Y_RAD)
        x, z = (x * cos_y + z * sin_y), (-x * sin_y + z * cos_y)

    if ROTATE_Z_RAD != 0.0:
        cos_z = math.cos(ROTATE_Z_RAD)
        sin_z = math.sin(ROTATE_Z_RAD)
        x, y = (x * cos_z - y * sin_z), (x * sin_z + y * cos_z)

    return [x, y, z]


def _apply_output_frame(vec: list[float]) -> list[float]:
    x, y, z = vec

    # FMU frame: x=longitudinal, y=lateral(left+), z=up
    # Viewer frame: x=lateral(left-), y=up, z=longitudinal(front+)
    if OUTPUT_FRAME == "viewer":
        transformed = [-y, z, x]
    else:
        transformed = [x, y, z]

    return _rotate_vec3(transformed)


def _extract_vec3(value: Any) -> list[float] | None:
    if not isinstance(value, (list, tuple)) or len(value) < 3:
        return None

    x = _to_float(value[0])
    y = _to_float(value[1])
    z = _to_float(value[2])
    if x is None or y is None or z is None:
        return None

    return [x, y, z]


def _transform_output_vectors(value: Any) -> Any:
    vec3 = _extract_vec3(value)
    if vec3 is not None:
        return _apply_output_frame(vec3)

    if isinstance(value, dict):
        return {k: _transform_output_vectors(v) for k, v in value.items()}

    if isinstance(value, list):
        return [_transform_output_vectors(v) for v in value]

    return value


def _extract_kinematics_inputs(decoded_message: dict[str, Any]) -> tuple[dict[str, float | None], float]:
    dynamics = decoded_message.get("dynamics", {})
    controls = decoded_message.get("controls", {})

    fl_keys = ["fl_sus_pot_v", "flSusPotV", "fl_ride_height", "flRideHeight"]
    fr_keys = ["fr_sus_pot_v", "frSusPotV", "fr_ride_height", "frRideHeight"]
    rl_keys = ["rl_sus_pot_v", "rlSusPotV", "bl_sus_pot_v", "blSusPotV", "bl_ride_height", "blRideHeight"]
    rr_keys = ["rr_sus_pot_v", "rrSusPotV", "br_sus_pot_v", "brSusPotV", "br_ride_height", "brRideHeight"]

    lengths = {
        "FL": _coalesce(
            _first_numeric(dynamics, fl_keys),
            _first_numeric(controls, ["sus1_v", "sus1V"]),
            _find_numeric_deep(decoded_message, fl_keys),
        ),
        "FR": _coalesce(
            _first_numeric(dynamics, fr_keys),
            _first_numeric(controls, ["sus2_v", "sus2V"]),
            _find_numeric_deep(decoded_message, fr_keys),
        ),
        "RL": _coalesce(
            _first_numeric(dynamics, rl_keys),
            _find_numeric_deep(decoded_message, rl_keys),
        ),
        "RR": _coalesce(
            _first_numeric(dynamics, rr_keys),
            _find_numeric_deep(decoded_message, rr_keys),
        ),
    }

    steer_deg = _coalesce(
        _first_numeric(dynamics, ["steer_col_angle", "steerColAngle", "fl_steer_angle", "flSteerAngle"]),
        _first_numeric(controls, ["steer_col_angle", "steerColAngle", "steer_v", "steerV"]),
        _find_numeric_deep(decoded_message, ["steer_col_angle", "steerColAngle", "steer_v", "steerV"]),
    )
    steer_input = math.radians(steer_deg) if steer_deg is not None else 0.0

    return lengths, steer_input


def _side_geometry(prefix: str, fmu: FMU) -> dict[str, list[float]]:
    return {
        "upperFore_i": fmu.get_vec3(f"{prefix}UpperFore_i"),
        "upperAft_i": fmu.get_vec3(f"{prefix}UpperAft_i"),
        "lowerFore_i": fmu.get_vec3(f"{prefix}LowerFore_i"),
        "lowerAft_i": fmu.get_vec3(f"{prefix}LowerAft_i"),
        "upper_o": fmu.get_vec3(f"{prefix}Upper_o"),
        "lower_o": fmu.get_vec3(f"{prefix}Lower_o"),
        "tie_i": fmu.get_vec3(f"{prefix}Tie_i"),
        "tie_o": fmu.get_vec3(f"{prefix}Tie_o"),
        "wheelCenter": fmu.get_vec3(f"{prefix}WheelCenter"),
        "tire_ex": fmu.get_vec3(f"{prefix}Tire_ex"),
        "tire_ey": fmu.get_vec3(f"{prefix}Tire_ey"),
        "CP": fmu.get_vec3(f"{prefix}CP"),
        "CPForce": fmu.get_vec3(f"{prefix}CPForce"),
        "bellcrankPivot": fmu.get_vec3(f"{prefix}BellcrankPivot"),
        "bellcrankPickup1": fmu.get_vec3(f"{prefix}BellcrankPickup1"),
        "bellcrankPickup2": fmu.get_vec3(f"{prefix}BellcrankPickup2"),
        "bellcrankPickup3": fmu.get_vec3(f"{prefix}BellcrankPickup3"),
        "rodMount": fmu.get_vec3(f"{prefix}RodMount"),
        "shockMount": fmu.get_vec3(f"{prefix}ShockMount"),
        "barEnd": fmu.get_vec3(f"{prefix}BarEnd"),
        "armEnd": fmu.get_vec3(f"{prefix}ArmEnd"),
    }


def _build_geometry(front_fmu: FMU, rear_fmu: FMU) -> dict[str, Any]:
    return {
        "front": {
            "left": _side_geometry("left", front_fmu),
            "right": _side_geometry("right", front_fmu),
        },
        "rear": {
            "left": _side_geometry("left", rear_fmu),
            "right": _side_geometry("right", rear_fmu),
        },
    }


def _flatten_front_signals(geometry: dict[str, Any]) -> dict[str, list[float]]:
    front = geometry.get("front", {})
    left = front.get("left", {})
    right = front.get("right", {})
    return {
        "leftUpperFore_i": left.get("upperFore_i"),
        "leftUpperAft_i": left.get("upperAft_i"),
        "leftLowerFore_i": left.get("lowerFore_i"),
        "leftLowerAft_i": left.get("lowerAft_i"),
        "leftUpper_o": left.get("upper_o"),
        "leftLower_o": left.get("lower_o"),
        "leftTie_i": left.get("tie_i"),
        "leftTie_o": left.get("tie_o"),
        "leftWheelCenter": left.get("wheelCenter"),
        "leftTire_ex": left.get("tire_ex"),
        "leftTire_ey": left.get("tire_ey"),
        "leftCP": left.get("CP"),
        "leftCPForce": left.get("CPForce"),
        "leftBellcrankPivot": left.get("bellcrankPivot"),
        "leftBellcrankPickup1": left.get("bellcrankPickup1"),
        "leftBellcrankPickup2": left.get("bellcrankPickup2"),
        "leftBellcrankPickup3": left.get("bellcrankPickup3"),
        "leftRodMount": left.get("rodMount"),
        "leftShockMount": left.get("shockMount"),
        "leftBarEnd": left.get("barEnd"),
        "leftArmEnd": left.get("armEnd"),
        "rightUpperFore_i": right.get("upperFore_i"),
        "rightUpperAft_i": right.get("upperAft_i"),
        "rightLowerFore_i": right.get("lowerFore_i"),
        "rightLowerAft_i": right.get("lowerAft_i"),
        "rightUpper_o": right.get("upper_o"),
        "rightLower_o": right.get("lower_o"),
        "rightTie_i": right.get("tie_i"),
        "rightTie_o": right.get("tie_o"),
        "rightWheelCenter": right.get("wheelCenter"),
        "rightTire_ex": right.get("tire_ex"),
        "rightTire_ey": right.get("tire_ey"),
        "rightCP": right.get("CP"),
        "rightCPForce": right.get("CPForce"),
        "rightBellcrankPivot": right.get("bellcrankPivot"),
        "rightBellcrankPickup1": right.get("bellcrankPickup1"),
        "rightBellcrankPickup2": right.get("bellcrankPickup2"),
        "rightBellcrankPickup3": right.get("bellcrankPickup3"),
        "rightRodMount": right.get("rodMount"),
        "rightShockMount": right.get("shockMount"),
        "rightBarEnd": right.get("barEnd"),
        "rightArmEnd": right.get("armEnd"),
    }


def _flatten_rear_signals(geometry: dict[str, Any]) -> dict[str, list[float]]:
    rear = geometry.get("rear", {})
    left = rear.get("left", {})
    right = rear.get("right", {})
    return {
        "rearLeftUpperFore_i": left.get("upperFore_i"),
        "rearLeftUpperAft_i": left.get("upperAft_i"),
        "rearLeftLowerFore_i": left.get("lowerFore_i"),
        "rearLeftLowerAft_i": left.get("lowerAft_i"),
        "rearLeftUpper_o": left.get("upper_o"),
        "rearLeftLower_o": left.get("lower_o"),
        "rearLeftTie_i": left.get("tie_i"),
        "rearLeftTie_o": left.get("tie_o"),
        "rearLeftWheelCenter": left.get("wheelCenter"),
        "rearLeftTire_ex": left.get("tire_ex"),
        "rearLeftTire_ey": left.get("tire_ey"),
        "rearLeftCP": left.get("CP"),
        "rearLeftCPForce": left.get("CPForce"),
        "rearLeftBellcrankPivot": left.get("bellcrankPivot"),
        "rearLeftBellcrankPickup1": left.get("bellcrankPickup1"),
        "rearLeftBellcrankPickup2": left.get("bellcrankPickup2"),
        "rearLeftBellcrankPickup3": left.get("bellcrankPickup3"),
        "rearLeftRodMount": left.get("rodMount"),
        "rearLeftShockMount": left.get("shockMount"),
        "rearLeftBarEnd": left.get("barEnd"),
        "rearLeftArmEnd": left.get("armEnd"),
        "rearRightUpperFore_i": right.get("upperFore_i"),
        "rearRightUpperAft_i": right.get("upperAft_i"),
        "rearRightLowerFore_i": right.get("lowerFore_i"),
        "rearRightLowerAft_i": right.get("lowerAft_i"),
        "rearRightUpper_o": right.get("upper_o"),
        "rearRightLower_o": right.get("lower_o"),
        "rearRightTie_i": right.get("tie_i"),
        "rearRightTie_o": right.get("tie_o"),
        "rearRightWheelCenter": right.get("wheelCenter"),
        "rearRightTire_ex": right.get("tire_ex"),
        "rearRightTire_ey": right.get("tire_ey"),
        "rearRightCP": right.get("CP"),
        "rearRightCPForce": right.get("CPForce"),
        "rearRightBellcrankPivot": right.get("bellcrankPivot"),
        "rearRightBellcrankPickup1": right.get("bellcrankPickup1"),
        "rearRightBellcrankPickup2": right.get("bellcrankPickup2"),
        "rearRightBellcrankPickup3": right.get("bellcrankPickup3"),
        "rearRightRodMount": right.get("rodMount"),
        "rearRightShockMount": right.get("shockMount"),
        "rearRightBarEnd": right.get("barEnd"),
        "rearRightArmEnd": right.get("armEnd"),
    }


class KinematicsRunner:
    def __init__(self):
        self.front = FMU(FRONT_FMU_PATH, "front")
        self.rear = FMU(REAR_FMU_PATH, "rear")
        self.lookup = ShockToWheel(LOOKUP_MAP_PATH)
        self.last_lengths = {corner: self._nominal_length(corner) for corner in ("FL", "FR", "RL", "RR")}
        self.map_bounds = {
            corner: (
                float(min(self.lookup.maps[corner][0])),
                float(max(self.lookup.maps[corner][0])),
            )
            for corner in ("FL", "FR", "RL", "RR")
        }

    def _nominal_length(self, corner: str) -> float:
        lengths = self.lookup.maps[corner][0]
        mid_index = len(lengths) // 2
        return float(lengths[mid_index])

    def _normalize_corner_length(self, corner: str, raw_value: float | None) -> float | None:
        if raw_value is None:
            return None

        l_min, l_max = self.map_bounds[corner]
        if l_min <= raw_value <= l_max:
            return raw_value

        # Orion suspot values are typically volts (roughly 0.5-4.5 V), not direct shock lengths.
        if 0.3 <= raw_value <= 5.5:
            return _linear_map(raw_value, 0.5, 4.5, l_min, l_max)

        # Fallback for ride-height style millimeter values from test streams.
        if 5.0 < raw_value <= 300.0:
            return _linear_map(raw_value, 20.0, 120.0, l_min, l_max)

        return _clamp(raw_value, l_min, l_max)

    def update(self, lengths: dict[str, float | None], steer_input: float) -> dict[str, Any]:
        normalized_lengths = dict(self.last_lengths)
        raw_lengths: dict[str, float | None] = {}

        for corner, value in lengths.items():
            parsed = _to_float(value)
            raw_lengths[corner] = parsed
            normalized = self._normalize_corner_length(corner, parsed)
            if normalized is not None:
                normalized_lengths[corner] = normalized

        self.last_lengths.update(normalized_lengths)

        z_fl = float(self.lookup.z("FL", self.last_lengths["FL"]))
        z_fr = float(self.lookup.z("FR", self.last_lengths["FR"]))
        z_rl = float(self.lookup.z("RL", self.last_lengths["RL"]))
        z_rr = float(self.lookup.z("RR", self.last_lengths["RR"]))

        h_f = 0.5 * (z_fl + z_fr)
        r_f = (z_fr - z_fl) / TRACK_F
        h_r = 0.5 * (z_rl + z_rr)
        r_r = (z_rr - z_rl) / TRACK_R

        self.front.set("heaveInput", h_f)
        self.front.set("rollInput", r_f)
        self.front.set("steerInput", steer_input)
        self.rear.set("heaveInput", h_r)
        self.rear.set("rollInput", r_r)

        self.front.step(0.0)
        self.rear.step(0.0)

        geometry_raw = _build_geometry(self.front, self.rear)
        geometry = _transform_output_vectors(geometry_raw)
        state = {
            "rawInputs": raw_lengths,
            "lengths": {
                "FL": self.last_lengths["FL"],
                "FR": self.last_lengths["FR"],
                "RL": self.last_lengths["RL"],
                "RR": self.last_lengths["RR"],
            },
            "zFL": z_fl,
            "zFR": z_fr,
            "zRL": z_rl,
            "zRR": z_rr,
            "hf": h_f,
            "rf": r_f,
            "hr": h_r,
            "rr": r_r,
            "steerInput": steer_input,
        }
        return {"state": state, "geometry": geometry}

    def terminate(self):
        self.front.terminate()
        self.rear.terminate()


def _select_latest_record(batch: dict[Any, list[Any]]) -> Any | None:
    latest = None
    best_key = (-1, -1)
    for records in batch.values():
        if not records:
            continue
        candidate = records[-1]
        ts = int(candidate.timestamp) if candidate.timestamp is not None else -1
        offset = int(candidate.offset) if candidate.offset is not None else -1
        key = (ts, offset)
        if latest is None or key > best_key:
            latest = candidate
            best_key = key
    return latest


def _request_shutdown(signum, _frame):
    global shutdown_requested
    if shutdown_requested:
        return
    shutdown_requested = True
    logging.info("Shutdown signal received (%s); stopping steer_sus processor.", signum)


signal.signal(signal.SIGTERM, _request_shutdown)
signal.signal(signal.SIGINT, _request_shutdown)

logging.info("Initializing Kafka Consumer...")
consumer = KafkaConsumer(
    INPUT_TOPIC,
    bootstrap_servers=KAFKA_BOOTSTRAP_SERVERS,
    group_id=CONSUMER_GROUP_ID,
    max_poll_records=MAX_POLL_RECORDS,
    enable_auto_commit=False,
    auto_offset_reset="latest",
    consumer_timeout_ms=5000,
)
logging.info(
    "Kafka Consumer initialized. broker='%s' input='%s' group='%s'",
    KAFKA_BOOTSTRAP_SERVERS,
    INPUT_TOPIC,
    CONSUMER_GROUP_ID,
)

logging.info("Initializing Kafka Producer...")
producer = KafkaProducer(
    bootstrap_servers=KAFKA_BOOTSTRAP_SERVERS,
    value_serializer=lambda v: json.dumps(v).encode("utf-8"),
)
logging.info(
    "Kafka Producer initialized. output='%s' default_car='%s' output_frame='%s' rotate=(x=%s,y=%s,z=%s)deg",
    OUTPUT_TOPIC,
    DEFAULT_CAR,
    OUTPUT_FRAME,
    ROTATE_X_DEG,
    ROTATE_Y_DEG,
    ROTATE_Z_DEG,
)

runners: dict[str, KinematicsRunner] = {}


def _runner_for(car_type: str) -> KinematicsRunner:
    runner = runners.get(car_type)
    if runner is not None:
        return runner
    runner = KinematicsRunner()
    runners[car_type] = runner
    return runner


logging.info("Polling for steer_sus input...")
last_idle_log = time.monotonic()
idle_log_interval_s = 10.0
publish_count = 0
last_publish_log_at = 0.0

try:
    while not shutdown_requested:
        batch = consumer.poll(timeout_ms=POLL_TIMEOUT_MS)
        if not batch:
            now = time.monotonic()
            if now - last_idle_log >= idle_log_interval_s:
                logging.info("Waiting for Kafka messages on '%s'...", INPUT_TOPIC)
                last_idle_log = now
            continue

        latest_record = _select_latest_record(batch)
        if latest_record is not None:
            process_start = time.perf_counter()
            try:
                car_type = _get_car_type_from_headers(latest_record.headers) or DEFAULT_CAR
                decoded = _decode_sensor_payload(payload=latest_record.value, car_type=car_type)
                lengths, steer_input = _extract_kinematics_inputs(decoded)
                result = _runner_for(car_type).update(lengths=lengths, steer_input=steer_input)
                payload = {
                    "car_type": car_type,
                    "timestamp_ms": int(latest_record.timestamp) if latest_record.timestamp is not None else None,
                    "source_offset": latest_record.offset,
                    "state": result["state"],
                    "geometry": result["geometry"],
                    **_flatten_front_signals(result["geometry"]),
                    **_flatten_rear_signals(result["geometry"]),
                }
                producer.send(
                    OUTPUT_TOPIC,
                    value=payload,
                    headers=[("car_type", car_type.encode("utf-8"))],
                )
                publish_count += 1
                process_ms = (time.perf_counter() - process_start) * 1000.0
                logging.debug(
                    "Published steer_sus update for %s (offset=%s, %.2f ms).",
                    car_type,
                    latest_record.offset,
                    process_ms,
                )
                now = time.monotonic()
                if now - last_publish_log_at >= PUBLISH_LOG_INTERVAL_S:
                    state = result["state"]
                    logging.info(
                        "steer_sus publish #%s car=%s offset=%s hf=%.6f rf=%.6f hr=%.6f rr=%.6f steer=%.4f lengths=%s raw=%s",
                        publish_count,
                        car_type,
                        latest_record.offset,
                        state["hf"],
                        state["rf"],
                        state["hr"],
                        state["rr"],
                        state["steerInput"],
                        state["lengths"],
                        state["rawInputs"],
                    )
                    last_publish_log_at = now
            except Exception as process_error:
                logging.exception("Failed to process steer_sus record: %s", process_error)

        if SKIP_CATCHUP:
            partitions = list(batch.keys())
            if partitions:
                consumer.seek_to_end(*partitions)

        consumer.commit()

    logging.info("steer_sus shutdown requested; leaving consume loop.")
except Exception as err:
    logging.exception("steer_sus processor fatal error: %s", err)
finally:
    try:
        producer.flush(timeout=5)
    except Exception:
        pass
    consumer.close()
    producer.close()
    for runner in runners.values():
        runner.terminate()
    logging.info("steer_sus consumer, producer, and FMUs closed.")
