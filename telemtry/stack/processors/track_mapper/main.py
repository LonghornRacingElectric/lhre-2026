from kafka import KafkaConsumer, KafkaProducer
from stack.ingest.mqtt_handler import MQTTHandler, MQTTTarget
import time
import json
import math
import logging



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

    distance = R * c
    return distance

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

# Initialize Kafka Producer
print("Initializing Kafka Producer...")
producer = KafkaProducer(
    bootstrap_servers='kafka:9092',
    value_serializer=lambda v: json.dumps(v).encode('utf-8')
)
print("Kafka Producer initialized. Ready to send messages to 'track-mapper' topic.")

handler = MQTTHandler()
allPoints = []
gate = ((37.4300, -122.1730), (37.4305, -122.1725))  # Example gate coordinates
lapCompleted = False


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
                if record.topic == 'reload_processors':
                    lapCompleted = False
                try:
                    decoded_message = handler._proto_decode(payload=record.value, car="Angelique")
                    print(f"  Decoded Message: {decoded_message}")
                    print("LAP COMPLETED ", lapCompleted)
                    if not lapCompleted:
                        gps_raw = decoded_message.get('dynamics', {}).get('gps', [0, 0])
                        # Incoming GPS is [longitude, latitude], swap to [latitude, longitude]
                        gps = tuple([gps_raw[1], gps_raw[0]])
                        print(" Extracted GPS data (lon, lat):", gps_raw, "-> (lat, lon):", gps)
                        if allPoints == []:
                            allPoints.append(gps)
                            msg = list(gps)  # [lat, lon] for Kafka
                            producer.send('track-mapper', value=msg)
                            producer.flush()  # Ensure message is sent immediately
                            print(f"✓ Sent first point to Kafka: [lat={msg[0]}, lon={msg[1]}]")
                            
                        else:
                            lastPoint = allPoints[-1]
                            distance = haversine_distance(lastPoint[0], lastPoint[1], gps[0], gps[1])
                            if distance > 2.0:  # Minimum distance threshold in meters
                                allPoints.append(gps)
                                msg = list(gps)  # [lat, lon] for Kafka
                                producer.send('track-mapper', value=msg)
                                producer.flush()  # Ensure message is sent immediately
                                print(f"✓ Sent point [lat={msg[0]}, lon={msg[1]}] to Kafka (distance: {distance:.2f}m)")
                            if is_intersection((lastPoint, gps), gate):
                                print("Lap completed!")
                                lapCompleted = True
                except Exception as decode_error:
                    print(f"  Error decoding message: {decode_error}")
                

                

        # After successfully processing the entire batch, manually commit the offsets.
        consumer.commit()
        print("Offsets committed for the batch.")

except Exception as e:
    print(f"An error occurred: {e}")

finally:
    # Ensure the consumer is closed properly
    consumer.close()
    print("Consumer closed.")
