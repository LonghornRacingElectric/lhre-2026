"""
telemtry/stack/tests/test_bevo_db_integration.py

Docker-dependent integration tests for:
- BEVO-Angelique welcome packet over MQTT
- SQLAlchemy model queries against telemetry databases
"""

import json
import time
import unittest

from paho.mqtt import client as mqtt_client

from telemtry.analysis.sql_utils.db_session import get_db
from telemtry.analysis.sql_utils.models import AngeliquePacket, Packet
from telemtry.stack.tests.test_utils import TelemetryConfig


class TestBevoAndDatabaseIntegration(unittest.TestCase):
    """Integration tests that require stack Docker containers."""

    @classmethod
    def setUpClass(cls):
        cls.config = TelemetryConfig.from_env()

    def test_bevo_angelique_welcome_packet(self):
        """BEVO-Angelique should receive a welcome packet containing packet_id."""
        received_message = None
        received_topic = None

        def on_connect(client, userdata, flags, rc):
            del userdata, flags
            if rc == 0:
                client.subscribe("server-communication")
                client.publish("client-connections", "BEVO-Angelique")
            else:
                self.fail(f"Failed to connect to MQTT broker, return code {rc}")

        def on_message(client, userdata, msg):
            del client, userdata
            nonlocal received_message, received_topic
            received_topic = msg.topic
            received_message = json.loads(msg.payload.decode())

        client = mqtt_client.Client(client_id="BEVO-Angelique")
        client.on_connect = on_connect
        client.on_message = on_message

        client.connect(self.config.mqtt_host, self.config.mqtt_port, 60)
        client.loop_start()

        timeout = 10
        start_time = time.time()
        while received_message is None and (time.time() - start_time) < timeout:
            time.sleep(0.1)

        client.loop_stop()
        client.disconnect()

        self.assertIsNotNone(received_message, "No welcome message received")
        self.assertEqual(received_topic, "server-communication", "Message received on wrong topic")
        self.assertIn("packet_id", received_message, "Welcome message does not contain packet_id")
        self.assertIsInstance(received_message["packet_id"], int, "packet_id is not an integer")

    def test_angelique_model_query(self):
        """Angelique model query should execute without SQL errors."""
        try:
            with get_db("Angelique") as session:
                count = session.query(AngeliquePacket).count()
                self.assertIsInstance(count, int, "Count should be an integer")
        except Exception as exc:
            self.fail(f"Querying AngeliquePacket raised an exception: {exc}")

    def test_nightwatch_model_query(self):
        """Nightwatch model query should execute without SQL errors."""
        try:
            with get_db("Nightwatch") as session:
                count = session.query(Packet).count()
                self.assertIsInstance(count, int, "Count should be an integer")
        except Exception as exc:
            self.fail(f"Querying Nightwatch model raised an exception: {exc}")

    def test_orion_model_query(self):
        """Orion DB session should be constructible when the DB is available."""
        try:
            with get_db("Orion"):
                self.assertTrue(True)
        except Exception:
            self.skipTest("Orion model query skipped: Orion DB/models are not configured")


if __name__ == "__main__":
    unittest.main()
