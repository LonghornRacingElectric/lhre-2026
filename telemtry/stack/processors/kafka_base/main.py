from kafka import KafkaConsumer
import time

consumer = KafkaConsumer(
    'db_inserts',
    bootstrap_servers='kafka:9092',
    group_id='test-group',
    max_poll_records=5,
    enable_auto_commit=False,
    auto_offset_reset='earliest',
    consumer_timeout_ms=5000
)

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

        # Iterate over the messages in the batch
        for partition, records in batch.items():
            for record in records:
                print(f"Processing message from topic '{record.topic}', partition {record.partition}: offset {record.offset}")
                print(f"  Key: {record.key}, Value: {record.value}")
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
    