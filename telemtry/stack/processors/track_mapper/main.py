from kafka import KafkaConsumer, KafkaProducer
from stack.ingest.mqtt_handler import MQTTHandler, MQTTTarget
import time
import json
import math
import logging

MAX_SPEED_MPS = 85.0

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

# Initialize separate filters for Lat and Lon
kalman_lat = SimpleKalmanFilter()
kalman_lon = SimpleKalmanFilter()
first_run = True

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
last_timestamp = time.time()
gate = None  # Will be set when first point is added
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
                # if record.topic == 'reload_processors':
                #     lapCompleted = False
                try:
                    decoded_message = handler._proto_decode(payload=record.value, car="Angelique")
                    print(f"  Decoded Message: {decoded_message}")
                    print("LAP COMPLETED ", lapCompleted)
                    if not lapCompleted:
                        gps_raw = decoded_message.get('dynamics', {}).get('gps', [0, 0])
                        current_time = time.time()
                        gps = tuple([gps_raw[0], gps_raw[1]])

                        if first_run:
                            kalman_lat.x = gps[0]
                            kalman_lon.x = gps[1]
                            first_run = False

                        smooth_lat = kalman_lat.update(gps[0])
                        smooth_lon = kalman_lon.update(gps[1])
                        current_point = (smooth_lat, smooth_lon)
                        timestamp_ms = current_time * 1000

                        if not allPoints:
                            allPoints.append(current_point)
                            producer.send('track-mapper', value=list(current_point) + [timestamp_ms])
                            producer.flush()
                            print(f"✓ Sent point to Kafka: lat={current_point[0]:.6f}, lon={current_point[1]:.6f}, timestamp={timestamp_ms}")
                            
                            # Create gate perpendicular to first point
                            # Use a simple perpendicular offset (small lat/lon delta)
                            lat_offset = 0.0001  # ~11 meters
                            lon_offset = 0.0001  # ~11 meters
                            # gate = (
                            #     (current_point[0] + lat_offset, current_point[1] - lon_offset),
                            #     (current_point[0] - lat_offset, current_point[1] + lon_offset)
                            # )
                            print(f"✓ Lap gate created at first point: {gate}")
                        else:
                            last_point = allPoints[-1]

                            dist = haversine_distance(last_point[0], last_point[1], current_point[0], current_point[1])
                            time_delta = current_time - last_timestamp
                            
                            if time_delta > 0:
                                speed = dist / time_delta
                            else:
                                speed = 0
                            
                            if speed > MAX_SPEED_MPS:
                                print(f"Outlier detected! Speed: {speed:.2f} m/s. Ignoring point.")
                                continue
                            
                            if dist > 2.0:
                                allPoints.append(current_point)
                                producer.send('track-mapper', value=list(current_point) + [timestamp_ms])
                                # producer.flush()
                                print(f"✓ Sent point to Kafka: lat={current_point[0]:.6f}, lon={current_point[1]:.6f}, timestamp={timestamp_ms}")
                                last_timestamp = current_time

                                if gate and is_intersection((last_point, current_point), gate):
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
