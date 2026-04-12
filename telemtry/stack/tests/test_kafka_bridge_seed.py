"""
Integration test that verifies the Kafka bridge seeds car-specific grafana_data topics.
The bridge service seeds a minimal JSON payload via seedTopic for each car when it starts
(implemented in cmd/bridge/main.go). This test consumes from each car's topic and
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

    def test_grafana_topics_seeded(self):
        client = KafkaTestClient(self.config)
        cars = ["angelique", "orion", "nightwatch"]

        for car in cars:
            topic = f"grafana_data_{car}"
            consumer = client.create_consumer(topic, group_id=f"bridge-seed-test-{car}")

            seed_found = False
            start = time.time()
            while time.time() - start < 15:
                batch = consumer.poll(timeout_ms=1000)
                for _, records in batch.items():
                    for record in records:
                        try:
                            payload = json.loads(record.value.decode("utf-8"))
                        except Exception:
                            continue

                        if payload.get("packet_id") == 0 and payload.get("car_type") == car:
                            seed_found = True
                            break
                    if seed_found:
                        break
                if seed_found:
                    break

            self.assertTrue(seed_found, f"Kafka bridge should seed {topic} at startup")

        client.close()


if __name__ == "__main__":
    unittest.main()
