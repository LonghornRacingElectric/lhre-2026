import json
import logging
import os
import signal
import time
from collections import defaultdict

from google.protobuf.json_format import MessageToDict
from kafka import KafkaConsumer, KafkaProducer
from stack.ingest.protobuf import angelique_pb2, can_packets_pb2, template_pb2

logging.basicConfig(level=os.getenv("LOGLEVEL", "INFO"))
logging.getLogger("kafka").setLevel(logging.WARNING)

CAR_NAME_MAP = {
    "angelique": "Angelique",
    "orion": "Orion",
    "nightwatch": "Nightwatch",
}

KAFKA_BOOTSTRAP_SERVERS = os.getenv("KAFKA_BOOTSTRAP_SERVERS", "kafka:9092")
INPUT_TOPIC = os.getenv("KAFKA_INPUT_TOPIC", "sensor_data")
OUTPUT_TOPIC = os.getenv("KAFKA_OUTPUT_TOPIC", "gg-plot")
CONSUMER_GROUP_ID = os.getenv("KAFKA_GROUP_ID", "gg-plot-group")

default_car_env = os.getenv("KAFKA_DEFAULT_CAR", "Angelique")
DEFAULT_CAR = CAR_NAME_MAP.get(default_car_env.strip().lower(), "Angelique")

try:
    ALPHA = float(os.getenv("GG_PLOT_ALPHA", "0.1"))
except ValueError:
    logging.warning("Invalid GG_PLOT_ALPHA provided, defaulting to 0.1")
    ALPHA = 0.1


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


def _decode_sensor_payload(payload: bytes, car_type: str) -> dict:
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


def _extract_xy_accel(decoded_message: dict, car_type: str) -> tuple[float, float] | None:
    dynamics = decoded_message.get("dynamics", {})
    if not isinstance(dynamics, dict):
        return None

    if car_type == "Orion":
        corner_keys = ["fl_sprung_accel", "fr_sprung_accel", "bl_sprung_accel", "br_sprung_accel"]
        corner_samples: list[tuple[float, float]] = []

        for key in corner_keys:
            accel = dynamics.get(key)
            if not isinstance(accel, list) or len(accel) < 2:
                continue
            try:
                corner_samples.append((float(accel[0]), float(accel[1])))
            except (TypeError, ValueError):
                continue

        if corner_samples:
            avg_x = sum(sample[0] for sample in corner_samples) / len(corner_samples)
            avg_y = sum(sample[1] for sample in corner_samples) / len(corner_samples)
            return avg_x, avg_y

    for key in ("body3_accel", "cent_mass_accel", "vcu_accel", "gps_imu"):
        accel = dynamics.get(key)
        if not isinstance(accel, list) or len(accel) < 2:
            continue
        try:
            return float(accel[0]), float(accel[1])
        except (TypeError, ValueError):
            continue

    return None

# Initialize Kafka Consumer
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
logging.info("GG plot default car: %s, alpha: %s", DEFAULT_CAR, ALPHA)

logging.debug("Polling...")
filtered_values_by_car = defaultdict(lambda: {"x": 0.0, "y": 0.0})
last_idle_log = time.monotonic()
idle_log_interval_s = 10.0
shutdown_requested = False


def _request_shutdown(signum, _frame):
    global shutdown_requested
    if shutdown_requested:
        return
    shutdown_requested = True
    logging.info("Shutdown signal received (%s); stopping gg-plot processor.", signum)


signal.signal(signal.SIGTERM, _request_shutdown)
signal.signal(signal.SIGINT, _request_shutdown)

# Enhanced logging for debugging
try:
    while not shutdown_requested:
        batch = consumer.poll(timeout_ms=1000)

        if not batch:
            now = time.monotonic()
            if now - last_idle_log >= idle_log_interval_s:
                logging.info(
                    "Waiting for Kafka messages on '%s' (group '%s').",
                    INPUT_TOPIC,
                    CONSUMER_GROUP_ID,
                )
                last_idle_log = now
            continue

        logging.debug(f"Received batch with {sum(len(records) for records in batch.values())} messages.")

        for partition, records in batch.items():
            logging.debug(f"Processing {len(records)} messages from partition {partition}.")
            for record in records:
                logging.debug(f"Processing message from topic '{record.topic}', partition {record.partition}: offset {record.offset}")
                logging.debug(f"  Key: {record.key}, Value: {record.value}")
                try:
                    car_type = _get_car_type_from_headers(record.headers) or DEFAULT_CAR
                    decoded_message = _decode_sensor_payload(payload=record.value, car_type=car_type)
                    logging.debug(f"  Decoded Message: {decoded_message}")

                    accel_pair = _extract_xy_accel(decoded_message, car_type)
                    if accel_pair is None:
                        logging.debug("  Warning: Missing or incomplete acceleration data for car '%s'.", car_type)
                        continue
                    x, y = accel_pair

                    logging.debug(f"  Extracted accel_data: x={x}, y={y}")

                    filter_state = filtered_values_by_car[car_type]
                    filtered_x = ALPHA * x + (1 - ALPHA) * filter_state["x"]
                    filtered_y = ALPHA * y + (1 - ALPHA) * filter_state["y"]
                    filter_state["x"] = filtered_x
                    filter_state["y"] = filtered_y
                    logging.debug(f"  Filtered values: filtered_x={filtered_x}, filtered_y={filtered_y}")

                    # Send filtered data to Kafka
                    filtered_message = {
                        "car_type": car_type,
                        "data": {
                            "x": filtered_x,
                            "y": filtered_y,
                        },
                    }
                    producer.send(
                        OUTPUT_TOPIC,
                        value=filtered_message,
                        headers=[("car_type", car_type.encode("utf-8"))],
                    )
                    logging.debug("Sent filtered data to '%s': %s", OUTPUT_TOPIC, filtered_message)

                except Exception as decode_error:
                    logging.error(f"  Error decoding message: {decode_error}")

        consumer.commit()
        logging.debug("Offsets committed for the batch.")

    logging.info("gg-plot processor shutdown requested; leaving consume loop.")

except Exception as e:
    logging.error(f"An error occurred: {e}")

finally:
    try:
        producer.flush(timeout=5)
    except Exception:
        pass
    consumer.close()
    producer.close()
    logging.info("Consumer and Producer closed.")
