"""
Integration test that verifies the Kafka bridge seeds the grafana_data topic.
The bridge service seeds a minimal JSON payload via seedTopic when it starts
(implemented in cmd/bridge/main.go). This test consumes from grafana_data and
asserts that the seed message is present.
"""

import json
import time
import unittest

from telemtry.stack.tests.test_utils import (
    TelemetryConfig,
    KafkaTestClient,
    check_kafka_connection,
    wait_for_service,
)


class TestKafkaBridgeSeed(unittest.TestCase):
    """Validate Kafka bridge seedTopic behavior."""

    @classmethod
    def setUpClass(cls):
        cls.config = TelemetryConfig.from_env()
        # Ensure Kafka is reachable before consuming
        wait_for_service(
            lambda: check_kafka_connection(cls.config.kafka_host, cls.config.kafka_port),
            "Kafka Broker",
            timeout=60,
        )

    def test_grafana_topic_seeded(self):
        client = KafkaTestClient(self.config)
        consumer = client.create_consumer("grafana_data", group_id="bridge-seed-test")

        seed_found = False
        start = time.time()
        while time.time() - start < 30:
            batch = consumer.poll(timeout_ms=1000)
            for _, records in batch.items():
                for record in records:
                    try:
                        payload = json.loads(record.value.decode("utf-8"))
                    except Exception:
                        continue

                    if payload.get("packet_id") == 0 and payload.get("car_type") == "init":
                        seed_found = True
                        break
                if seed_found:
                    break
            if seed_found:
                break

        client.close()
        self.assertTrue(seed_found, "Kafka bridge should seed grafana_data at startup")


if __name__ == "__main__":
    unittest.main()
