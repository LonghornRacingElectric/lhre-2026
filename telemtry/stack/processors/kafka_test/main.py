from kafka import KafkaConsumer, KafkaProducer
import time
import json
import random

consumer = KafkaConsumer(
    'db_inserts',
    bootstrap_servers='kafka:9092',
    group_id='test-group',
    max_poll_records=5,
    enable_auto_commit=False,
    auto_offset_reset='earliest',
    consumer_timeout_ms=5000
)

# Producer for status updates
producer = KafkaProducer(
    bootstrap_servers='kafka:9092',
    value_serializer=lambda v: json.dumps(v).encode('utf-8')
)

print("Polling db_inserts + emitting dummy status ...")
start_time = time.time()
last_status_sent = 0.0
odometer_base = 5000.0  # dummy starting odometer (units: miles or km – keep consistent)
battery_start = 80.0    # dummy starting battery percentage

def build_dummy_status(elapsed: float):
    """Generate deterministic-but-changing dummy status values.

    elapsed: seconds since script start
    Returns a dict with battery and odometer fields.
    """
    # Odometer increases slowly (e.g., 0.02 units per second)
    odo = odometer_base + 0.02 * elapsed
    # Battery drains slowly (e.g., 0.005 % per second) and never below 5%
    batt = max(5.0, battery_start - 0.005 * elapsed)
    # Add a tiny random jitter to make it look alive (not required)
    batt += random.uniform(-0.05, 0.05)
    return {"battery": round(batt, 2), "odometer": round(odo, 2)}
try:
    while True:
        # Periodically emit dummy status regardless of incoming messages
        now = time.time()
        if now - last_status_sent >= 0.1:  # every 0.1 seconds
            elapsed = now - start_time
            dummy = build_dummy_status(elapsed)
            payload = {"ts": int(now * 1000), **dummy}
            try:
                producer.send('status', value=payload)
                print(f"Sent dummy status: {payload}")
            except Exception as e:
                print(f"(failed to send dummy status: {e})")
            last_status_sent = now
        # The poll() method returns a dictionary of partitions and their records.
        # It's non-blocking for the specified timeout.
        batch = consumer.poll(timeout_ms=1000)

        # Check if any records were returned
        if not batch:
            print("No new messages, waiting...")
            continue

        # Iterate over the messages in the batch (logging only)
        for partition, records in batch.items():
            for record in records:
                print(f"Processing message from topic '{record.topic}', partition {record.partition}: offset {record.offset}")
                print(f"  Key: {record.key}, Value: {record.value}")
                # (Intentionally no status extraction – status is dummy & independent)

        # After successfully processing the entire batch, manually commit the offsets.
        consumer.commit()
        print("Offsets committed for the batch.")

except Exception as e:
    print(f"An error occurred: {e}")

finally:
    # Ensure the consumer is closed properly
    consumer.close()
    try:
        producer.flush(timeout=5)
        producer.close()
    except Exception:
        pass
    print("Consumer closed.")
    