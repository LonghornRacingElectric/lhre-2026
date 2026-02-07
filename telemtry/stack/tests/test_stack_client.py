#!/usr/bin/env python3
"""
telemtry/stack/tests/test_stack_client.py

Main entry point for the stack integration test client.
This script runs comprehensive tests against the telemetry Docker stack.

Usage:
    python test_stack_client.py
    # or via Bazel:
    bazel run //telemtry/stack/tests:stack_test_client
"""

import sys
import os
import time
import logging

# Add the workspace root to the path
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__)))))

# Configure logging
logging.basicConfig(
    level=os.getenv('LOGLEVEL', 'INFO'),
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


def run_connectivity_tests():
    """Run basic connectivity tests for all services."""
    from test_utils import (
        TelemetryConfig,
        check_mqtt_connection,
        check_kafka_connection,
        check_db_connection,
        wait_for_service,
    )
    
    config = TelemetryConfig.from_env()
    results = {}
    
    # Test MQTT
    logger.info("Testing MQTT connectivity...")
    results['mqtt'] = wait_for_service(
        lambda: check_mqtt_connection(config.mqtt_host, config.mqtt_port),
        "MQTT Broker",
        timeout=30,
    )
    
    # Test Kafka
    logger.info("Testing Kafka connectivity...")
    results['kafka'] = wait_for_service(
        lambda: check_kafka_connection(config.kafka_host, config.kafka_port),
        "Kafka Broker",
        timeout=60,
    )
    
    # Test Database
    logger.info("Testing PostgreSQL connectivity...")
    results['db'] = wait_for_service(
        lambda: check_db_connection(config.db_host, config.db_port),
        "PostgreSQL",
        timeout=30,
    )
    
    return results


def run_data_flow_tests():
    """Run data flow tests to verify end-to-end communication."""
    from test_utils import (
        TelemetryConfig,
        MQTTTestClient,
        KafkaTestClient,
        TestDataGenerator,
    )
    
    config = TelemetryConfig.from_env()
    generator = TestDataGenerator(seed=42)
    results = {}
    
    # Test MQTT publish/subscribe
    logger.info("Testing MQTT publish/subscribe...")
    try:
        with MQTTTestClient(config) as mqtt:
            test_topic = "test/integration"
            mqtt.subscribe(test_topic)
            time.sleep(1)
            mqtt.publish(test_topic, b"test_message")
            time.sleep(2)
            results['mqtt_pubsub'] = len(mqtt.received_messages) > 0
    except Exception as e:
        logger.error(f"MQTT test failed: {e}")
        results['mqtt_pubsub'] = False
    
    # Test Kafka produce/consume
    logger.info("Testing Kafka produce/consume...")
    try:
        kafka = KafkaTestClient(config)
        producer = kafka.create_producer()
        producer.send("test-integration", b"test_message")
        producer.flush()
        results['kafka_produce'] = True
        kafka.close()
    except Exception as e:
        logger.error(f"Kafka test failed: {e}")
        results['kafka_produce'] = False
    
    # Test Protobuf data generation
    logger.info("Testing Protobuf data generation...")
    try:
        data = generator.generate_protobuf_message(packet_id=1)
        results['protobuf_gen'] = data is not None and len(data) > 0
    except Exception as e:
        logger.error(f"Protobuf test failed: {e}")
        results['protobuf_gen'] = False
    
    return results


def run_telemetry_flow_test():
    """
    Run a complete telemetry flow test simulating real data ingestion.
    This tests the pattern used by paho_testing.py.
    """
    from test_utils import (
        TelemetryConfig,
        MQTTTestClient,
        TestDataGenerator,
    )
    import json
    
    config = TelemetryConfig.from_env()
    generator = TestDataGenerator(seed=42)
    
    logger.info("Running telemetry flow test...")
    
    try:
        with MQTTTestClient(config) as mqtt:
            # 1. Start an event
            logger.info("  Sending start event...")
            start_event = {"event_id": 99999}
            mqtt.publish("config/flask", json.dumps(start_event).encode(), qos=1)
            time.sleep(1)
            
            # 2. Send sensor data
            logger.info("  Sending sensor data...")
            for i in range(10):
                data = generator.generate_protobuf_message(packet_id=i + 1)
                if data:
                    mqtt.publish("data", data, qos=0)
                time.sleep(0.1)
            
            time.sleep(2)
            
            # 3. End the event
            logger.info("  Sending end event...")
            mqtt.publish("config/flask", b"end_event", qos=1)
            time.sleep(1)
            
            return True
            
    except Exception as e:
        logger.error(f"Telemetry flow test failed: {e}")
        return False


def print_results(results: dict, section: str):
    """Print test results in a formatted manner."""
    print(f"\n{'='*50}")
    print(f" {section}")
    print('='*50)
    
    for test, passed in results.items():
        status = "✓ PASS" if passed else "✗ FAIL"
        print(f"  {test}: {status}")
    
    all_passed = all(results.values())
    print('-'*50)
    print(f"  Overall: {'✓ ALL PASSED' if all_passed else '✗ SOME FAILED'}")
    
    return all_passed


def main():
    """Main entry point for the stack integration test client."""
    print("\n" + "="*60)
    print(" TELEMETRY STACK INTEGRATION TESTS")
    print("="*60)
    
    all_passed = True
    
    try:
        # Run connectivity tests
        connectivity_results = run_connectivity_tests()
        if not print_results(connectivity_results, "Connectivity Tests"):
            all_passed = False
            # If connectivity fails, don't continue
            if not all(connectivity_results.values()):
                logger.error("Connectivity tests failed, skipping data flow tests")
                sys.exit(1)
        
        # Run data flow tests
        data_flow_results = run_data_flow_tests()
        if not print_results(data_flow_results, "Data Flow Tests"):
            all_passed = False
        
        # Run telemetry flow test
        telemetry_result = run_telemetry_flow_test()
        print_results({"telemetry_flow": telemetry_result}, "Telemetry Flow Test")
        if not telemetry_result:
            all_passed = False
        
    except Exception as e:
        logger.exception(f"Test failed with exception: {e}")
        all_passed = False
    
    # Final summary
    print("\n" + "="*60)
    if all_passed:
        print(" ✓ ALL INTEGRATION TESTS PASSED")
        print("="*60 + "\n")
        sys.exit(0)
    else:
        print(" ✗ SOME TESTS FAILED")
        print("="*60 + "\n")
        sys.exit(1)


if __name__ == "__main__":
    main()
