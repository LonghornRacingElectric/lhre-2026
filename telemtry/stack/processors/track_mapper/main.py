import json
import logging
import math
import os
import time
from collections import defaultdict
from typing import Any

from kafka import KafkaConsumer, KafkaProducer
from stack.ingest.mqtt_handler import MQTTHandler

MAX_SPEED_MPS = 85.0
MIN_DISTANCE_METERS = 1
TRACK_END_PROXIMITY_METERS = 50.0  # Distance threshold to detect return to start gate

CAR_NAME_MAP = {
    "angelique": "Angelique",
    "orion": "Orion",
}

KAFKA_BOOTSTRAP_SERVERS = os.getenv("KAFKA_BOOTSTRAP_SERVERS", "kafka:9092")
INPUT_TOPIC = os.getenv("KAFKA_INPUT_TOPIC", "sensor_data")
OUTPUT_TOPIC = os.getenv("KAFKA_OUTPUT_TOPIC", "track-mapper")
CONSUMER_GROUP_ID = os.getenv("KAFKA_GROUP_ID", "track-mapper-group")

# Gate configuration: format "lat1,lon1,lat2,lon2"
# REQUIRED: User must set this environment variable to define the start/finish gate
# Example: "30.289464,-97.735303,30.289500,-97.735350"
START_GATE_ENV = os.getenv("START_GATE", None)
START_GATE = None
if START_GATE_ENV:
    try:
        parts = [float(x.strip()) for x in START_GATE_ENV.split(",")]
        if len(parts) == 4:
            START_GATE = ((parts[0], parts[1]), (parts[2], parts[3]))
            logging.info("Start gate configured: %.6f,%.6f to %.6f,%.6f", parts[0], parts[1], parts[2], parts[3])
        else:
            logging.error("Invalid START_GATE format. Expected 'lat1,lon1,lat2,lon2'")
            raise ValueError("Invalid START_GATE format")
    except ValueError as e:
        logging.error("Could not parse START_GATE: %s. This is required to run track mapper.", e)
        raise
else:
    logging.error("START_GATE environment variable is required but not set.")
    logging.error("Set it with: export START_GATE='lat1,lon1,lat2,lon2'")
    raise ValueError("START_GATE environment variable is required")

default_car_env = os.getenv("KAFKA_DEFAULT_CAR", "Orion")
DEFAULT_CAR = CAR_NAME_MAP.get(default_car_env.strip().lower(), "Orion")


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

class SimpleKalmanFilter:
    """
    Minimizes GPS jitter by keeping an estimate of position and uncertainty.
    """
    def __init__(self, process_noise=1e-4, measurement_noise=1e-2, estimation_error=1.0):
        self.q = process_noise       # Process noise covariance
        self.r = measurement_noise   # Measurement noise covariance
        self.p = estimation_error    # Estimation error covariance
        self.x = 0.0                 # Value (Lat or Lon)

    def update(self, measurement):
        # Prediction update
        self.p = self.p + self.q

        # Measurement update
        k = self.p / (self.p + self.r) # Kalman gain
        self.x = self.x + k * (measurement - self.x)
        self.p = (1 - k) * self.p
        return self.x

def haversine_distance(lat1, lon1, lat2, lon2):
    # Earth's mean radius in meters
    R = 6371000  

    # Convert decimal degrees to radians
    lat1, lon1, lat2, lon2 = map(math.radians, [lat1, lon1, lat2, lon2])

    # Haversine formula
    dlon = lon2 - lon1
    dlat = lat2 - lat1

    a = math.sin(dlat / 2)**2 + math.cos(lat1) * math.cos(lat2) * math.sin(dlon / 2)**2
    c = 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a))

    return R * c

def is_close_to_gate(point: tuple[float, float], gate: tuple[tuple[float, float], tuple[float, float]], threshold_meters: float) -> bool:
    """
    Checks if a point is within threshold distance of the gate's midpoint.
    Used for detecting return to start in CSV mode.
    """
    # Calculate midpoint of gate
    gate_mid_lat = (gate[0][0] + gate[1][0]) / 2
    gate_mid_lon = (gate[0][1] + gate[1][1]) / 2
    
    # Calculate distance from point to gate midpoint
    distance = haversine_distance(point[0], point[1], gate_mid_lat, gate_mid_lon)
    
    return distance <= threshold_meters

def is_intersection(line1: tuple[tuple[float, float], tuple[float, float]], line2: tuple[tuple[float, float], tuple[float, float]]) -> bool:
    """
    Checks if two line segments intersect. Used for lap detection with the gate.
    """
    denominator = (line1[0][1] - line1[1][1]) * (line2[0][0] - line2[1][0]) - (line1[0][0] - line1[1][0]) * (line2[0][1] - line2[1][1])
    if denominator == 0:
        return False
    t = ((line1[0][1] - line2[0][1]) * (line2[0][0] - line2[1][0]) - (line1[0][0] - line2[0][0]) * (line2[0][1] - line2[1][1])) / denominator
    u = ((line1[0][1] - line2[0][1]) * (line1[0][0] - line1[1][0]) - (line1[0][0] - line2[0][0]) * (line1[0][1] - line1[1][1])) / denominator
    return 0 <= t <= 1 and 0 <= u <= 1

def _new_car_state() -> dict[str, Any]:
    return {
        "kalman_lat": SimpleKalmanFilter(),
        "kalman_lon": SimpleKalmanFilter(),
        "initialized": False,
        "all_points": [],
        "last_timestamp": time.time(),
        "track_started": False,  # Tracks if we've crossed the start gate
        "track_completed": False,  # Tracks if we've returned to start gate area
    }


logging.info("Initializing Kafka Consumer...")
consumer = KafkaConsumer(
    INPUT_TOPIC,
    bootstrap_servers=KAFKA_BOOTSTRAP_SERVERS,
    group_id=CONSUMER_GROUP_ID,
    max_poll_records=5,
    enable_auto_commit=False,
    auto_offset_reset="earliest",
    consumer_timeout_ms=5000,
)
logging.info(
    "Kafka Consumer initialized. Connected to broker '%s' and subscribed to topic '%s' with group '%s'.",
    KAFKA_BOOTSTRAP_SERVERS,
    INPUT_TOPIC,
    CONSUMER_GROUP_ID,
)

# Log start gate configuration
logging.info(
    "Track mapper will record points between gate crossings: (%.6f, %.6f) to (%.6f, %.6f)",
    START_GATE[0][0],
    START_GATE[0][1],
    START_GATE[1][0],
    START_GATE[1][1],
)
logging.info(
    "Track detection: First gate crossing starts recording. Returns to start gate area within %.0f meters ends recording.",
    TRACK_END_PROXIMITY_METERS,
)

# Initialize Kafka Producer
logging.info("Initializing Kafka Producer...")
producer = KafkaProducer(
    bootstrap_servers=KAFKA_BOOTSTRAP_SERVERS,
    value_serializer=lambda v: json.dumps(v).encode("utf-8"),
)
logging.info("Kafka Producer initialized. Ready to send messages to '%s' topic.", OUTPUT_TOPIC)
logging.info("Track mapper default car: %s", DEFAULT_CAR)

handler = MQTTHandler()
car_states = defaultdict(_new_car_state)


def _send_track_point(car_type: str, point: tuple[float, float], timestamp_ms: float):
    point_message = {
        "car_type": car_type,
        "data": [point[0], point[1]],
        "timestamp_ms": timestamp_ms,
    }
    producer.send(
        OUTPUT_TOPIC,
        value=point_message,
        headers=[("car_type", car_type.encode("utf-8"))],
    )


logging.info("Polling...")
try:
    while True:
        batch = consumer.poll(timeout_ms=1000)

        if not batch:
            logging.debug("No new messages, waiting...")
            continue

        logging.debug("Received batch with %s messages.", sum(len(records) for records in batch.values()))

        for partition, records in batch.items():
            logging.debug("Processing %s messages from partition %s.", len(records), partition)
            for record in records:
                logging.debug(
                    "Processing message from topic '%s', partition %s: offset %s",
                    record.topic,
                    record.partition,
                    record.offset,
                )
                try:
                    car_type = _get_car_type_from_headers(record.headers) or DEFAULT_CAR
                    decoded_message = handler._proto_decode(payload=record.value, car=car_type)

                    car_state = car_states[car_type]
                    if car_state["lap_completed"]:
                        continue

                    gps_raw = decoded_message.get("dynamics", {}).get("gps", [0, 0])
                    if not isinstance(gps_raw, list) or len(gps_raw) < 2:
                        logging.debug("Missing gps array for car '%s': %s", car_type, gps_raw)
                        continue

                    try:
                        gps = (float(gps_raw[0]), float(gps_raw[1]))
                    except (TypeError, ValueError):
                        logging.debug("Invalid gps values for car '%s': %s", car_type, gps_raw)
                        continue

                    if not math.isfinite(gps[0]) or not math.isfinite(gps[1]):
                        continue

                    current_time = time.time()

                    if not car_state["initialized"]:
                        car_state["kalman_lat"].x = gps[0]
                        car_state["kalman_lon"].x = gps[1]
                        car_state["initialized"] = True

                    smooth_lat = car_state["kalman_lat"].update(gps[0])
                    smooth_lon = car_state["kalman_lon"].update(gps[1])
                    current_point = (smooth_lat, smooth_lon)
                    timestamp_ms = current_time * 1000

                    all_points = car_state["all_points"]
                    if not all_points:
                        # Store first point for comparison, wait for gate crossing to start recording
                        all_points.append(current_point)
                        car_state["last_timestamp"] = current_time
                        logging.debug(
                            "Waiting for start gate crossing. Current point: lat=%.6f, lon=%.6f",
                            current_point[0],
                            current_point[1],
                        )
                        continue

                    last_point = all_points[-1]
                    dist = haversine_distance(last_point[0], last_point[1], current_point[0], current_point[1])
                    time_delta = current_time - car_state["last_timestamp"]
                    speed = dist / time_delta if time_delta > 0 else 0

                    if speed > MAX_SPEED_MPS:
                        logging.debug(
                            "Outlier detected for %s. Speed %.2f m/s, skipping point.",
                            car_type,
                            speed,
                        )
                        continue

                    if dist > MIN_DISTANCE_METERS:
                        if car_state["track_completed"]:
                            continue  # Stop processing once track is complete
                        
                        # Check if this movement segment crosses the start gate
                        segment_crosses_gate = is_intersection((last_point, current_point), START_GATE)
                        
                        if not car_state["track_started"]:
                            # Waiting for first gate crossing
                            if segment_crosses_gate:
                                car_state["track_started"] = True
                                all_points.append(current_point)
                                _send_track_point(car_type, current_point, timestamp_ms)
                                car_state["last_timestamp"] = current_time
                                logging.info(
                                    "Track started for %s - gate crossed at (%.6f, %.6f)",
                                    car_type,
                                    current_point[0],
                                    current_point[1],
                                )
                        else:
                            # Track has started, collecting points
                            all_points.append(current_point)
                            _send_track_point(car_type, current_point, timestamp_ms)
                            car_state["last_timestamp"] = current_time
                            
                            # Check for return to start gate by proximity
                            if is_close_to_gate(current_point, START_GATE, TRACK_END_PROXIMITY_METERS):
                                logging.info(
                                    "Track completed for %s - returned to start gate area at (%.6f, %.6f)",
                                    car_type,
                                    current_point[0],
                                    current_point[1],
                                )
                                car_state["track_completed"] = True
                except Exception as decode_error:
                    logging.error("Error decoding/process message: %s", decode_error)

        consumer.commit()
        logging.debug("Offsets committed for batch.")

except Exception as e:
    logging.error("An error occurred: %s", e)

finally:
    consumer.close()
    producer.close()
    logging.info("Consumer and producer closed.")
