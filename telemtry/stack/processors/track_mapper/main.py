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
MIN_DISTANCE_METERS = 2.0

CAR_NAME_MAP = {
    "angelique": "Angelique",
    "orion": "Orion",
    "nightwatch": "Nightwatch",
}

KAFKA_BOOTSTRAP_SERVERS = os.getenv("KAFKA_BOOTSTRAP_SERVERS", "kafka:9092")
INPUT_TOPIC = os.getenv("KAFKA_INPUT_TOPIC", "sensor_data")
OUTPUT_TOPIC = os.getenv("KAFKA_OUTPUT_TOPIC", "track-mapper")
CONSUMER_GROUP_ID = os.getenv("KAFKA_GROUP_ID", "track-mapper-group")

default_car_env = os.getenv("KAFKA_DEFAULT_CAR", "Angelique")
DEFAULT_CAR = CAR_NAME_MAP.get(default_car_env.strip().lower(), "Angelique")


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
        "gate": None,
        "lap_completed": False,
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
                        all_points.append(current_point)
                        _send_track_point(car_type, current_point, timestamp_ms)
                        car_state["last_timestamp"] = current_time
                        logging.debug(
                            "Sent initial point for %s: lat=%.6f, lon=%.6f, t=%.0f",
                            car_type,
                            current_point[0],
                            current_point[1],
                            timestamp_ms,
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
                        all_points.append(current_point)
                        _send_track_point(car_type, current_point, timestamp_ms)
                        car_state["last_timestamp"] = current_time

                        gate = car_state["gate"]
                        if gate and is_intersection((last_point, current_point), gate):
                            logging.info("Lap completed for %s", car_type)
                            car_state["lap_completed"] = True
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
