"""
telemtry/stack/tests/test_mqtt_connectivity.py

Tests MQTT broker connectivity for the telemetry system.
Requires the MQTT broker (mosquitto) container to be running.
"""

import unittest
import os
import sys
import time

from telemtry.stack.tests.test_utils import (
    TelemetryConfig,
    check_mqtt_connection,
    wait_for_service,
    MQTTTestClient,
)


class TestMQTTConnectivity(unittest.TestCase):
    """Test suite for MQTT broker connectivity."""
    
    @classmethod
    def setUpClass(cls):
        """Set up test fixtures."""
        cls.config = TelemetryConfig.from_env()
    
    def test_mqtt_broker_available(self):
        """Test that the MQTT broker is available and accepting connections."""
        is_available = wait_for_service(
            lambda: check_mqtt_connection(self.config.mqtt_host, self.config.mqtt_port),
            "MQTT Broker",
            timeout=30,
        )
        self.assertTrue(is_available, "MQTT broker should be available")
    
    def test_mqtt_publish_subscribe(self):
        """Test basic publish/subscribe functionality."""
        with MQTTTestClient(self.config) as client:
            # Subscribe to a test topic
            test_topic = "test/telemetry/connectivity"
            client.subscribe(test_topic)
            time.sleep(1)  # Wait for subscription to be active
            
            # Publish a test message
            test_message = b"connectivity_test_message"
            client.publish(test_topic, test_message)
            
            # Wait for message to be received
            time.sleep(2)
            
            # Verify message was received
            self.assertGreater(
                len(client.received_messages), 0,
                "Should receive at least one message"
            )
            self.assertEqual(
                client.received_messages[-1]["payload"],
                test_message,
                "Received message should match sent message"
            )
    
    def test_mqtt_qos_levels(self):
        """Test that different QoS levels work correctly."""
        with MQTTTestClient(self.config) as client:
            test_topic = "test/telemetry/qos"
            
            for qos in [0, 1, 2]:
                client.publish(test_topic, f"QoS {qos} test".encode(), qos=qos)
                time.sleep(0.5)
            
            # If we get here without exception, QoS levels are working
            self.assertTrue(True)
    
    def test_mqtt_multiple_topics(self):
        """Test subscription to multiple topics (simulating telemetry data topics)."""
        with MQTTTestClient(self.config) as client:
            # Subscribe to telemetry-like topics
            topics = [
                "data/packet",
                "data/dynamics",
                "data/controls",
                "data/thermal",
                "config/flask",
            ]
            
            for topic in topics:
                client.subscribe(topic)
            
            time.sleep(1)
            
            # Publish to each topic
            for topic in topics:
                client.publish(topic, f"test message for {topic}".encode())
            
            time.sleep(2)
            
            # Verify messages were received
            self.assertGreater(
                len(client.received_messages), 0,
                "Should receive messages from subscribed topics"
            )


if __name__ == "__main__":
    unittest.main()
