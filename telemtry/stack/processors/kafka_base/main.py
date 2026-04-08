import logging
import os

from kafka import KafkaConsumer
from stack.ingest.mqtt_handler import MQTTHandler

CAR_NAME_MAP = {
    "angelique": "Angelique",
    "orion": "Orion",
    "nightwatch": "Nightwatch",
}

KAFKA_BOOTSTRAP_SERVERS = os.getenv("KAFKA_BOOTSTRAP_SERVERS", "kafka:9092")
INPUT_TOPIC = os.getenv("KAFKA_INPUT_TOPIC", "sensor_data")
CONSUMER_GROUP_ID = os.getenv("KAFKA_GROUP_ID", "kafka-base-group")

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

# Debugging log for Kafka connection
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
logging.info("Kafka base default car: %s", DEFAULT_CAR)

logging.debug("Polling...")
try:
    while True:
        # The poll() method returns a dictionary of partitions and their records.
        # It's non-blocking for the specified timeout.
        batch = consumer.poll(timeout_ms=1000)

        # Check if any records were returned
        if not batch:
            logging.debug("No new messages, waiting...")
            continue

        # Debugging log for received batch
        logging.debug(f"Received batch with {sum(len(records) for records in batch.values())} messages.")

        # Iterate over the messages in the batch
        for partition, records in batch.items():
            logging.debug(f"Processing {len(records)} messages from partition {partition}.")
            for record in records:
                logging.debug(f"Processing message from topic '{record.topic}', partition {record.partition}: offset {record.offset}")
                logging.debug(f"  Key: {record.key}, Value: {record.value}")
                try:
                    car_type = _get_car_type_from_headers(record.headers) or DEFAULT_CAR
                    decoded_message = MQTTHandler._proto_decode(payload=record.value, car=car_type)
                    logging.debug(f"  Decoded Message: {decoded_message}")
                except Exception as decode_error:
                    logging.error(f"  Error decoding message: {decode_error}")
                # Place your message processing logic here

        # After successfully processing the entire batch, manually commit the offsets.
        consumer.commit()
        logging.debug("Offsets committed for the batch.")

except Exception as e:
    logging.error(f"An error occurred: {e}")

finally:
    # Ensure the consumer is closed properly
    consumer.close()
    logging.info("Consumer closed.")
