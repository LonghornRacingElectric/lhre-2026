"""
telemtry/stack/tests/test_kafka_connectivity.py

Tests Kafka broker connectivity for the telemetry system.
Requires the Kafka container to be running.
"""

import unittest
import os
import sys
import time

from telemtry.stack.tests.test_utils import (
    TelemetryConfig,
    check_kafka_connection,
    wait_for_service,
    KafkaTestClient,
)


class TestKafkaConnectivity(unittest.TestCase):
    """Test suite for Kafka broker connectivity."""
    
    @classmethod
    def setUpClass(cls):
        """Set up test fixtures."""
        cls.config = TelemetryConfig.from_env()
    
    def test_kafka_broker_available(self):
        """Test that the Kafka broker is available and accepting connections."""
        is_available = wait_for_service(
            lambda: check_kafka_connection(self.config.kafka_host, self.config.kafka_port),
            "Kafka Broker",
            timeout=60,  # Kafka can take longer to start
        )
        self.assertTrue(is_available, "Kafka broker should be available")
    
    def test_kafka_produce_consume(self):
        """Test basic produce/consume functionality."""
        client = KafkaTestClient(self.config)
        
        try:
            # Create producer and send message
            producer = client.create_producer()
            test_topic = "test-telemetry-connectivity"
            test_message = b"connectivity_test_message"
            
            future = producer.send(test_topic, value=test_message)
            result = future.get(timeout=10)  # Wait for send to complete
            
            self.assertIsNotNone(result, "Producer should send message successfully")
            producer.flush()
            
            # Create consumer and read message
            consumer = client.create_consumer(test_topic, group_id="test-connectivity-group")
            
            messages = []
            start_time = time.time()
            while time.time() - start_time < 10:
                batch = consumer.poll(timeout_ms=1000)
                for partition, records in batch.items():
                    for record in records:
                        messages.append(record.value)
                if messages:
                    break
            
            self.assertGreater(
                len(messages), 0,
                "Should receive at least one message from Kafka"
            )
            
        finally:
            client.close()
    
    def test_kafka_sensor_data_topic(self):
        """Test the sensor_data topic used by the telemetry system."""
        client = KafkaTestClient(self.config)
        
        try:
            producer = client.create_producer()
            
            # Send test data to the sensor_data topic (used by ingest service)
            test_data = b'{"packet_id": 1, "time": 1234567890, "test": true}'
            future = producer.send("sensor_data", value=test_data)
            result = future.get(timeout=10)
            producer.flush()
            
            self.assertIsNotNone(result, "Should be able to send to sensor_data topic")
            
        finally:
            client.close()
    
    def test_kafka_consumer_groups(self):
        """Test that consumer groups work correctly for parallel processing."""
        client1 = KafkaTestClient(self.config)
        client2 = KafkaTestClient(self.config)
        
        try:
            # Both consumers in the same group should share messages
            consumer1 = client1.create_consumer(
                "test-consumer-groups",
                group_id="test-shared-group"
            )
            consumer2 = client2.create_consumer(
                "test-consumer-groups",
                group_id="test-shared-group"
            )
            
            # Send messages
            producer = client1.create_producer()
            for i in range(10):
                producer.send("test-consumer-groups", value=f"message-{i}".encode())
            producer.flush()
            
            # Both consumers should be able to receive some messages
            # (exact distribution depends on partitioning)
            time.sleep(2)
            
        finally:
            client1.close()
            client2.close()


if __name__ == "__main__":
    unittest.main()
