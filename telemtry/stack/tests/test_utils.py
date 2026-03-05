"""
telemtry/stack/tests/test_utils.py

Common test utilities for telemetry integration tests.
Provides helper functions for connecting to MQTT, Kafka, and PostgreSQL services.

Environment Loading Strategy:
-----------------------------
1. First check if required environment variables are already set (e.g., from CI secrets).
2. If env vars are missing, attempt to load from telemtry/.env file (for local development).
3. This allows CI to work with secrets (no .env file needed) while local dev uses .env file.

For GitHub CI:
- Secrets are set as environment variables in the workflow
- No .env file is created or committed
- Secrets remain private

For Local Development:
- Copy .env.example to .env and fill in values
- The .env file is gitignored and never committed
"""

import os
import time
import logging
from typing import Optional, Callable, Any
from dataclasses import dataclass
from pathlib import Path

# List of required environment variables for telemetry
REQUIRED_ENV_VARS = [
    "POSTGRES_USER",
    "POSTGRES_DB", 
    "POSTGRES_PASSWORD",
    "POSTGRES_HOST",
    "POSTGRES_PORT",
]

# Optional env vars that have sensible defaults
OPTIONAL_ENV_VARS = [
    "KAFKA_BROKERS",
    "KAFKA_CLIENT_ID",
    "KAFKA_GROUP_ID",
    "KAFKA_TOPICS",
    "KAFKA_AUTO_CREATE",
    "KAFKA_ROUTES_FILE",
    "ELECTRIC_PWD",
    "GRAFANA_PWD",
    "ANALYSIS_PWD",
    "NEXTAUTH_SECRET",
    "AUTH_DATABASE_URL",
]


def _check_env_vars_set() -> bool:
    """Check if required environment variables are already set."""
    return all(os.environ.get(var) for var in REQUIRED_ENV_VARS)


def _load_env_file() -> bool:
    """
    Attempt to load .env file from various locations.
    Returns True if successfully loaded, False otherwise.
    """
    try:
        from dotenv import load_dotenv
    except ImportError:
        return False
    
    # Try multiple potential locations for the .env file
    # In Bazel runfiles, the structure is: <workspace>/<package>/<file>
    possible_env_paths = [
        # Bazel runfiles paths
        Path.cwd() / "telemtry" / ".env",
        Path.cwd() / ".env",
    ]
    
    for env_path in possible_env_paths:
        if env_path.exists():
            load_dotenv(env_path)
            return True
    
    # Try to find via runfiles
    try:
        from rules_python.python.runfiles import runfiles
        r = runfiles.Create()
        env_file = r.Rlocation("_main/telemtry/.env")
        if env_file and Path(env_file).exists():
            load_dotenv(env_file)
            return True
    except ImportError:
        pass
    
    return False


# Environment loading logic:
# 1. If env vars are already set (CI), don't load .env file
# 2. If env vars are missing (local dev), try to load .env file
if not _check_env_vars_set():
    _load_env_file()

# Configure logging
logging.basicConfig(level=os.getenv('LOGLEVEL', 'INFO'))
logger = logging.getLogger(__name__)


@dataclass
class TelemetryConfig:
    """Configuration for telemetry service connections."""
    mqtt_host: str = "localhost"
    mqtt_port: int = 1883
    kafka_host: str = "localhost"
    kafka_port: int = 9092
    db_host: str = "localhost"
    db_port: int = 5432
    db_user: str = "postgres"
    db_password: str = "postgres"
    db_name: str = "telemetry"
    grafana_host: str = "localhost"
    grafana_port: int = 3000
    
    @classmethod
    def from_env(cls) -> "TelemetryConfig":
        """Create config from environment variables (loaded from .env file)."""
        # Parse Kafka brokers to extract host:port
        kafka_brokers = os.getenv("KAFKA_BROKERS", "localhost:29092")
        kafka_parts = kafka_brokers.split(":")
        kafka_host = kafka_parts[0] if kafka_parts else "localhost"
        kafka_port = int(kafka_parts[1]) if len(kafka_parts) > 1 else 29092
        
        return cls(
            mqtt_host="localhost",
            mqtt_port=1883,
            kafka_host=kafka_host,
            kafka_port=kafka_port,
            db_host="localhost",
            db_port=5432,
            db_user=os.getenv("POSTGRES_USER", "postgres"),
            db_password=os.getenv("POSTGRES_PASSWORD", "postgres"),
            db_name=os.getenv("POSTGRES_DB", "telemetry"),
        )


def wait_for_service(
    check_fn: Callable[[], bool],
    service_name: str,
    timeout: int = 30,
    interval: float = 1.0
) -> bool:
    """
    Wait for a service to become available.
    
    Args:
        check_fn: Function that returns True if service is available
        service_name: Name of service for logging
        timeout: Maximum time to wait in seconds
        interval: Time between checks in seconds
    
    Returns:
        True if service became available, False if timeout reached
    """
    start_time = time.time()
    
    while time.time() - start_time < timeout:
        try:
            if check_fn():
                logger.info(f"Service {service_name} is available")
                return True
        except Exception as e:
            logger.debug(f"Waiting for {service_name}: {e}")
        
        time.sleep(interval)
    
    logger.error(f"Timeout waiting for {service_name}")
    return False


def check_mqtt_connection(host: str = "localhost", port: int = 1883) -> bool:
    """Check if MQTT broker is available."""
    try:
        from paho.mqtt import client as mqtt_client
        
        client = mqtt_client.Client("test_client")
        client.connect(host, port, keepalive=5)
        client.disconnect()
        return True
    except Exception as e:
        logger.debug(f"MQTT connection check failed: {e}")
        return False


def check_kafka_connection(host: str = "localhost", port: int = 9092) -> bool:
    """Check if Kafka broker is available."""
    try:
        from kafka import KafkaConsumer
        from kafka.errors import NoBrokersAvailable
        
        consumer = KafkaConsumer(
            bootstrap_servers=f"{host}:{port}",
            consumer_timeout_ms=5000,
        )
        consumer.topics()
        consumer.close()
        return True
    except Exception as e:
        logger.debug(f"Kafka connection check failed: {e}")
        return False


def check_db_connection(
    host: str = None, 
    port: int = None,
    user: str = None,
    password: str = None,
    database: str = None
) -> bool:
    """Check if PostgreSQL database is available."""
    try:
        import psycopg2
        
        # Use provided values or fall back to environment variables
        conn = psycopg2.connect(
            host=host or os.getenv("POSTGRES_HOST", "localhost"),
            port=port or int(os.getenv("POSTGRES_PORT", "5432")),
            user=user or os.getenv("POSTGRES_USER", "postgres"),
            password=password or os.getenv("POSTGRES_PASSWORD", "postgres"),
            database=database or os.getenv("POSTGRES_DB", "postgres"),
            connect_timeout=5,
        )
        conn.close()
        return True
    except Exception as e:
        logger.debug(f"PostgreSQL connection check failed: {e}")
        return False


class TestDataGenerator:
    """
    Generator for test data that mimics real telemetry sensor data.
    Based on patterns from paho_testing.py DataTester class.
    """
    
    def __init__(self, seed: Optional[int] = None):
        import numpy as np
        from numpy.random import default_rng
        
        self.rng = default_rng(seed)
    
    def generate_sensor_data(self, packet_id: int = 1) -> dict:
        """Generate mock sensor data payload."""
        import time
        import numpy as np
        
        return {
            "packet_id": packet_id,
            "time": int(time.time() * 1000),
            "dynamics": {
                "f_gps_velocity": float(self.rng.random() * 100),
                "f_gps_heading": float(self.rng.random() * 360),
                "f_gps": (
                    30.289464 + self.rng.random() * 0.001,
                    -97.735303 + self.rng.random() * 0.001,
                ),
                "steer_col_angle": float(self.rng.random() * 2.5),
            },
            "controls": {
                "accel_pedal_t": float(self.rng.random()),
                "brake_pedal_t": float(self.rng.random()),
            },
        }
    
    def generate_protobuf_message(self, packet_id: int = 1, car: str = "Nightwatch"):
        """Generate a protobuf-encoded sensor data message."""
        import time
        import json
        
        try:
            if car == "Angelique":
                from telemtry.stack.ingest.protobuf.angelique_pb2 import AngeliqueSensorData
                data = AngeliqueSensorData()
            elif car == "Orion":
                from telemtry.stack.ingest.protobuf.can_packets_pb2 import OrionSensorData
                data = OrionSensorData()
            else:
                from telemtry.stack.ingest.protobuf.template_pb2 import SensorData
                data = SensorData()
            
            data.packet_id = packet_id
            data.time = int(time.time() * 1000)
            
            # Set dynamics fields if they exist
            if hasattr(data, 'dynamics'):
                if car == "Orion":
                    data.dynamics.steer_col_angle = float(self.rng.random() * 2.5)
                    data.dynamics.flw_speed = float(self.rng.random() * 100)
                elif car != "Angelique":
                    data.dynamics.f_gps_velocity = float(self.rng.random() * 100)
                    data.dynamics.f_gps_heading = float(self.rng.random() * 360)
                    data.dynamics.steer_col_angle = float(self.rng.random() * 2.5)
                else:
                    data.dynamics.gps_velocity = float(self.rng.random() * 100)
                    data.dynamics.gps_heading = float(self.rng.random() * 360)
                    data.dynamics.torque_request = float(self.rng.random() * 2.5)
            
            # Set controls fields if they exist
            if hasattr(data, 'controls'):
                if car == "Orion":
                    data.controls.apps1_v = float(self.rng.random())
                elif car != "Angelique":
                    data.controls.accel_pedal_t = float(self.rng.random())
                else:
                    data.controls.apps1_v = float(self.rng.random())

            
            return data.SerializeToString()
        except ImportError as e:
            logger.warning(f"Could not import protobuf: {e}")
            return None


class MQTTTestClient:
    """Test client for MQTT communication."""
    
    def __init__(self, config: Optional[TelemetryConfig] = None):
        from paho.mqtt import client as mqtt_client
        import threading
        import uuid
        
        self.config = config or TelemetryConfig.from_env()
        # Use unique client ID to avoid conflicts with other test clients
        client_id = f"test_mqtt_client_{uuid.uuid4().hex[:8]}"
        self.client = mqtt_client.Client(client_id)
        self.received_messages = []
        self._loop_started = False
        self._connected = False
        self._connect_event = threading.Event()
        
        # Set up connection callback
        def on_connect(client, userdata, flags, rc):
            if rc == 0:
                logger.info("MQTT connected successfully")
                self._connected = True
                self._connect_event.set()
            else:
                logger.error(f"MQTT connection failed with code: {rc}")
                self._connect_event.set()
        
        # Set up disconnection callback
        def on_disconnect(client, userdata, rc):
            logger.info(f"MQTT disconnected with code: {rc}")
            self._connected = False
        
        # Set up message callback
        def on_message(client, userdata, msg):
            logger.debug(f"Received message on {msg.topic}: {msg.payload[:50]}...")
            self.received_messages.append({
                "topic": msg.topic,
                "payload": msg.payload,
            })
        
        self.client.on_connect = on_connect
        self.client.on_disconnect = on_disconnect
        self.client.on_message = on_message
    
    def connect(self, timeout: float = 10.0) -> bool:
        """Connect to MQTT broker and wait for connection to be established."""
        try:
            logger.info(f"Connecting to MQTT at {self.config.mqtt_host}:{self.config.mqtt_port}")
            self._connect_event.clear()
            self._connected = False
            self.client.connect(self.config.mqtt_host, self.config.mqtt_port)
            # Start the network loop - required for callbacks to work
            self.client.loop_start()
            self._loop_started = True
            
            # Wait for on_connect callback
            if not self._connect_event.wait(timeout=timeout):
                logger.error("MQTT connection timed out")
                return False
            
            # Small delay to ensure connection is stable
            time.sleep(0.1)
            
            return self._connected
        except Exception as e:
            logger.error(f"Failed to connect to MQTT: {e}")
            return False
    
    def disconnect(self):
        """Disconnect from MQTT broker."""
        self._connected = False
        if self._loop_started:
            self.client.loop_stop()
            self._loop_started = False
        try:
            self.client.disconnect()
        except Exception:
            pass
    
    def is_connected(self) -> bool:
        """Check if client is connected."""
        return self._connected and self._loop_started
    
    def publish(self, topic: str, payload: bytes, qos: int = 1):
        """Publish a message to MQTT topic."""
        if not self.is_connected():
            raise RuntimeError("MQTT client is not connected")
        result = self.client.publish(topic, payload, qos=qos)
        # Wait for publish to complete (with timeout)
        result.wait_for_publish(timeout=5.0)
        logger.debug(f"Published to {topic}, result: {result.rc}")
    
    def subscribe(self, topic: str, qos: int = 0):
        """Subscribe to an MQTT topic."""
        if not self.is_connected():
            raise RuntimeError("MQTT client is not connected")
        result, mid = self.client.subscribe(topic, qos=qos)
        logger.debug(f"Subscribed to {topic}, result: {result}, mid: {mid}")
        return result
    
    def __enter__(self):
        if not self.connect():
            raise ConnectionError("Failed to connect to MQTT broker")
        return self
    
    def __exit__(self, exc_type, exc_val, exc_tb):
        self.disconnect()


class KafkaTestClient:
    """Test client for Kafka communication."""
    
    def __init__(self, config: Optional[TelemetryConfig] = None):
        self.config = config or TelemetryConfig.from_env()
        self.bootstrap_servers = f"{self.config.kafka_host}:{self.config.kafka_port}"
        self.producer = None
        self.consumer = None
    
    def create_producer(self):
        """Create a Kafka producer."""
        from kafka import KafkaProducer
        
        self.producer = KafkaProducer(
            bootstrap_servers=self.bootstrap_servers,
        )
        return self.producer
    
    def create_consumer(self, topic: str, group_id: str = "test-group"):
        """Create a Kafka consumer."""
        from kafka import KafkaConsumer
        
        self.consumer = KafkaConsumer(
            topic,
            bootstrap_servers=self.bootstrap_servers,
            group_id=group_id,
            auto_offset_reset='earliest',
            consumer_timeout_ms=10000,
        )
        return self.consumer
    
    def close(self):
        """Close producer and consumer connections."""
        if self.producer:
            self.producer.close()
        if self.consumer:
            self.consumer.close()


def assert_eventually(
    condition: Callable[[], bool],
    timeout: float = 10.0,
    interval: float = 0.5,
    message: str = "Condition not met within timeout"
):
    """
    Assert that a condition becomes true within a timeout period.
    Useful for testing asynchronous operations.
    """
    start_time = time.time()
    
    while time.time() - start_time < timeout:
        if condition():
            return True
        time.sleep(interval)
    
    raise AssertionError(message)
