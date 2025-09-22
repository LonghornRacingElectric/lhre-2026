from kafka import KafkaConsumer
import time

consumer = KafkaConsumer(
    'testing_kafka',
    bootstrap_servers='kafka:9092',
    group_id='test-group',
    auto_offset_reset='earliest',
    consumer_timeout_ms=5000
)

print("Polling...")
for message in consumer:
    print(f"Message: {message.value}")
    