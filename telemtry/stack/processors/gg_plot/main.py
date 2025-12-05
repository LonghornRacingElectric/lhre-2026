from kafka import KafkaConsumer, KafkaProducer
from stack.ingest.mqtt_handler import MQTTHandler, MQTTTarget
import json
import time

# Declare filtered_x and filtered_y as global variables at the top
filtered_x, filtered_y = 0.0, 0.0

# Initialize Kafka Consumer
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

# Initialize Kafka Producer
print("Initializing Kafka Producer...")
producer = KafkaProducer(
    bootstrap_servers='kafka:9092',
    value_serializer=lambda v: json.dumps(v).encode('utf-8')
)
print("Kafka Producer initialized. Ready to send messages to 'gg-plot' topic.")

# Low-pass filter parameters
alpha = 0.1  # Smoothing factor (0 < alpha <= 1)

print("Polling...")
# Enhanced logging for debugging
try:
    while True:
        batch = consumer.poll(timeout_ms=1000)

        if not batch:
            print("No new messages, waiting...")
            continue

        print(f"Received batch with {sum(len(records) for records in batch.values())} messages.")

        for partition, records in batch.items():
            print(f"Processing {len(records)} messages from partition {partition}.")
            for record in records:
                print(f"Processing message from topic '{record.topic}', partition {record.partition}: offset {record.offset}")
                print(f"  Key: {record.key}, Value: {record.value}")
                try:
                    decoded_message = MQTTHandler._proto_decode(payload=record.value, car="Angelique")
                    print(f"  Decoded Message: {decoded_message}")

                    # Extract dynamics.vcuAccel data
                    accel_data = decoded_message.get('dynamics', {}).get('body3_accel', [0, 0, 0])
                    if not accel_data or len(accel_data) < 2:
                        print("  Warning: Missing or incomplete vcuAccel data.")
                        continue

                    x, y = accel_data[0], accel_data[1]
                    print(f"  Extracted accel_data: x={x}, y={y}")

                    # Apply low-pass filter
                    filtered_x = alpha * x + (1 - alpha) * filtered_x
                    filtered_y = alpha * y + (1 - alpha) * filtered_y
                    print(f"  Filtered values: filtered_x={filtered_x}, filtered_y={filtered_y}")

                    # Send filtered data to Kafka
                    filtered_message = {
                        "data": {
                            'x': filtered_x,
                            'y': filtered_y
                        }
                    }
                    producer.send('gg-plot', value=filtered_message)
                    print(f"Sent filtered data to 'gg-plot': {filtered_message}")

                except Exception as decode_error:
                    print(f"  Error decoding message: {decode_error}")

        consumer.commit()
        print("Offsets committed for the batch.")

except Exception as e:
    print(f"An error occurred: {e}")

finally:
    consumer.close()
    producer.close()
    print("Consumer and Producer closed.")
