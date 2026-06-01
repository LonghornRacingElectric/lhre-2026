"""
telemtry/stack/tests/test_bevo_db_integration.py

Docker-dependent integration tests for:
- BEVO-Angelique welcome packet over MQTT
- SQLAlchemy model queries against telemetry databases
"""

import json
import time
import unittest
from pathlib import Path

from paho.mqtt import client as mqtt_client
from sqlalchemy import text

from telemtry.analysis.sql_utils.db_session import get_db
from telemtry.analysis.sql_utils.models import AngeliquePacket, OrionPacket, Packet
from telemtry.stack.ingest.protobuf import can_packets_pb2
from telemtry.stack.tests.test_utils import TelemetryConfig


class TestBevoAndDatabaseIntegration(unittest.TestCase):
    """Integration tests that require stack Docker containers."""

    @classmethod
    def setUpClass(cls):
        cls.config = TelemetryConfig.from_env()
        cls._ensure_orion_schema()

    @classmethod
    def _ensure_orion_schema(cls):
        try:
            import psycopg2
        except Exception:
            return

        schema_sql_path = Path(__file__).resolve().parents[1] / "ingest" / "orion_db_init.sql"
        if not schema_sql_path.exists():
            return

        try:
            conn = psycopg2.connect(
                host=cls.config.db_host,
                port=cls.config.db_port,
                user=cls.config.db_user,
                password=cls.config.db_password,
                database="orion",
            )
            conn.autocommit = True
            with conn.cursor() as cur:
                cur.execute("SELECT to_regclass('public.packet')")
                regclass = cur.fetchone()
                if regclass and regclass[0] is not None:
                    return
            with conn.cursor() as cur:
                with open(schema_sql_path, "r", encoding="utf-8") as f:
                    cur.execute(f.read())
        except Exception:
            return
        finally:
            try:
                conn.close()
            except Exception:
                pass

    def _orion_schema_ready(self) -> bool:
        try:
            with get_db("Orion") as session:
                result = session.execute(text("SELECT to_regclass('public.packet')")).scalar()
                return result is not None
        except Exception:
            return False

    def _await_welcome_packet(self, client_id: str, announce_payload: str, timeout: float = 12.0):
        received_message = None
        received_topic = None

        def on_connect(client, userdata, flags, rc):
            del userdata, flags
            if rc == 0:
                client.subscribe("server-communication")
            else:
                self.fail(f"Failed to connect to MQTT broker, return code {rc}")

        def on_message(client, userdata, msg):
            del client, userdata
            nonlocal received_message, received_topic
            received_topic = msg.topic
            received_message = json.loads(msg.payload.decode())

        client = mqtt_client.Client(client_id=client_id)
        client.on_connect = on_connect
        client.on_message = on_message
        client.connect(self.config.mqtt_host, self.config.mqtt_port, 60)
        client.loop_start()

        try:
            start_time = time.time()
            while received_message is None and (time.time() - start_time) < timeout:
                client.publish("client-connections", announce_payload)
                time.sleep(0.5)
        finally:
            client.loop_stop()
            client.disconnect()

        return received_topic, received_message

    def test_bevo_angelique_welcome_packet(self):
        """BEVO-Angelique should receive a welcome packet containing packet_id."""
        received_topic, received_message = self._await_welcome_packet(
            client_id="BEVO-Angelique",
            announce_payload="BEVO-Angelique",
        )

        self.assertIsNotNone(received_message, "No welcome message received")
        self.assertEqual(received_topic, "server-communication", "Message received on wrong topic")
        self.assertIn("packet_id", received_message, "Welcome message does not contain packet_id")
        self.assertIsInstance(received_message["packet_id"], int, "packet_id is not an integer")

    def test_bevo_orion_welcome_packet(self):
        """BEVO-Orion should receive a welcome packet containing packet_id."""
        received_topic, received_message = self._await_welcome_packet(
            client_id="BEVO-Orion",
            announce_payload="BEVO-Orion",
        )

        self.assertIsNotNone(received_message, "No welcome message received")
        self.assertEqual(received_topic, "server-communication", "Message received on wrong topic")
        self.assertIn("packet_id", received_message, "Welcome message does not contain packet_id")
        self.assertIsInstance(received_message["packet_id"], int, "packet_id is not an integer")

    def _ingest_one_orion_packet(self, client_id, disconnect_before_poll):
        """Publish one fake Orion protobuf packet over MQTT and return
        (before_count, after_count) of OrionPacket rows.

        The publish is confirmed with wait_for_publish so the bytes have
        actually left the client -- a bare qos=0 publish with no running network
        loop can be dropped when disconnect() closes the socket first. When
        disconnect_before_poll is True the publisher disconnects immediately
        after that confirmation (before we poll the DB), mirroring the car
        dropping its link right after sending.
        """
        if not self._orion_schema_ready():
            self.skipTest("Orion schema not initialized in test database")

        with get_db("Orion") as session:
            before_count = session.query(OrionPacket).count()

        payload = can_packets_pb2.OrionSensorData()
        payload.packet_id = int(time.time() * 1000)
        payload.time = int(time.time() * 1000)
        payload.dynamics.steer_col_angle = 1.23
        payload.controls.apps1_v = 2.34

        client = mqtt_client.Client(client_id=client_id)
        client.connect(self.config.mqtt_host, self.config.mqtt_port, 60)
        client.loop_start()
        torn_down = False

        def teardown():
            nonlocal torn_down
            if not torn_down:
                client.loop_stop()
                client.disconnect()
                torn_down = True

        after_count = before_count
        try:
            info = client.publish("orion/data", payload.SerializeToString(), qos=0)
            info.wait_for_publish(timeout=5)
            if disconnect_before_poll:
                teardown()

            deadline = time.time() + 15
            while time.time() < deadline:
                with get_db("Orion") as session:
                    after_count = session.query(OrionPacket).count()
                if after_count > before_count:
                    break
                time.sleep(0.25)
        finally:
            teardown()

        return before_count, after_count

    def test_orion_protobuf_ingest_persists_row(self):
        """Ingest path: a published Orion protobuf packet is persisted as a DB
        row (publisher stays connected while we poll)."""
        before_count, after_count = self._ingest_one_orion_packet(
            client_id="orion-ingest-publisher",
            disconnect_before_poll=False,
        )
        self.assertGreater(
            after_count,
            before_count,
            "Orion packet row was not inserted after publishing protobuf payload",
        )

    def test_orion_ingest_survives_immediate_publisher_disconnect(self):
        """Resilience: ingest still persists the packet when the publisher
        disconnects immediately after the broker confirms the publish
        (fire-and-forget, mirroring the car dropping its link after sending)."""
        before_count, after_count = self._ingest_one_orion_packet(
            client_id="orion-ingest-disconnect-publisher",
            disconnect_before_poll=True,
        )
        self.assertGreater(
            after_count,
            before_count,
            "Orion packet row was not inserted after the publisher disconnected immediately",
        )

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
        """Orion model query should execute without SQL errors."""
        if not self._orion_schema_ready():
            self.skipTest("Orion schema not initialized in test database")
        try:
            with get_db("Orion") as session:
                count = session.query(OrionPacket).count()
                self.assertIsInstance(count, int, "Count should be an integer")
        except Exception as exc:
            self.fail(f"Querying OrionPacket raised an exception: {exc}")




if __name__ == "__main__":
    unittest.main()
