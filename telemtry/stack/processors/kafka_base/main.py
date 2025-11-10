from kafka import KafkaConsumer
from stack.ingest.mqtt_handler import MQTTHandler, MQTTTarget
import time

# Debugging log for Kafka connection
print("Initializing Kafka Consumer...")
consumer = KafkaConsumer(
    'sensor_data',
    bootstrap_servers='kafka:9092',
    group_id='test-group',
    max_poll_records=5,
    enable_auto_commit=False,
    auto_offset_reset='earliest',
    consumer_timeout_ms=5000
)
print("Kafka Consumer initialized. Connected to broker 'kafka:9092' and subscribed to topic 'sensor_data'.")

print("Polling...")
try:
    while True:
        # The poll() method returns a dictionary of partitions and their records.
        # It's non-blocking for the specified timeout.
        batch = consumer.poll(timeout_ms=1000)

        # Check if any records were returned
        if not batch:
            print("No new messages, waiting...")
            continue

        # Debugging log for received batch
        print(f"Received batch with {sum(len(records) for records in batch.values())} messages.")

        # Iterate over the messages in the batch
        for partition, records in batch.items():
            print(f"Processing {len(records)} messages from partition {partition}.")
            for record in records:
                print(f"Processing message from topic '{record.topic}', partition {record.partition}: offset {record.offset}")
                print(f"  Key: {record.key}, Value: {record.value}")
                try:
                    decoded_message = MQTTHandler._proto_decode(payload=record.value, car="Angelique")
                    print(f"  Decoded Message: {decoded_message}")
                except Exception as decode_error:
                    print(f"  Error decoding message: {decode_error}")
                # Place your message processing logic here

        # After successfully processing the entire batch, manually commit the offsets.
        consumer.commit()
        print("Offsets committed for the batch.")

except Exception as e:
    print(f"An error occurred: {e}")

finally:
    # Ensure the consumer is closed properly
    consumer.close()
    print("Consumer closed.")
