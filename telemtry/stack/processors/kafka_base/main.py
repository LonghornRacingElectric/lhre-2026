from kafka import KafkaConsumer
from stack.ingest.mqtt_handler import MQTTHandler, MQTTTarget
import time
import logging

# Debugging log for Kafka connection
logging.info("Initializing Kafka Consumer...")
consumer = KafkaConsumer(
    'sensor_data',
    bootstrap_servers='kafka:9092',
    group_id='test-group',
    max_poll_records=5,
    enable_auto_commit=False,
    auto_offset_reset='earliest',
    consumer_timeout_ms=5000
)
logging.info("Kafka Consumer initialized. Connected to broker 'kafka:9092' and subscribed to topic 'sensor_data'.")

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
                    decoded_message = MQTTHandler._proto_decode(payload=record.value, car="Angelique")
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
