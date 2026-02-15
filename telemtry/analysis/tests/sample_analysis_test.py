# telemtry/analysis/tests/sample_analysis_test.py

import unittest
import json
import time
import sys
import os
from pathlib import Path

# Add the project root to sys.path to import modules
sys.path.append(str(Path(__file__).parents[2]))

from paho.mqtt import client as mqtt_client
from stack.ingest.mqtt_handler import MQTTTarget

class SampleAnalysisTest(unittest.TestCase):
    """
    A sample test suite for the analysis library.
    """

    def test_bevo_angelique_welcome_packet(self):
        """
        Test that BEVO-Angelique client receives a welcome packet with packet_id upon connecting.
        """
        received_message = None
        received_topic = None

        def on_connect(client, userdata, flags, rc):
            if rc == 0:
                client.subscribe('server-communication')
                # Announce connection
                client.publish('client-connections', 'BEVO-Angelique')
            else:
                self.fail(f"Failed to connect to MQTT broker, return code {rc}")

        def on_message(client, userdata, msg):
            nonlocal received_message, received_topic
            received_topic = msg.topic
            received_message = json.loads(msg.payload.decode())

        # Create MQTT client as BEVO-Angelique
        client = mqtt_client.Client(client_id='BEVO-Angelique')
        client.on_connect = on_connect
        client.on_message = on_message

        # Connect to the broker
        broker_ip = MQTTTarget.get()
        print(f"Connecting to broker at: {broker_ip}")  # Debug print
        client.connect(broker_ip, 1883, 60)

        # Start the loop
        client.loop_start()

        # Wait for the message (up to 10 seconds)
        timeout = 10
        start_time = time.time()
        while received_message is None and (time.time() - start_time) < timeout:
            time.sleep(0.1)

        # Stop the loop
        client.loop_stop()
        client.disconnect()

        # Assert that we received the message
        self.assertIsNotNone(received_message, "No welcome message received")
        self.assertEqual(received_topic, 'server-communication', "Message received on wrong topic")
        self.assertIn('packet_id', received_message, "Welcome message does not contain packet_id")
        self.assertIsInstance(received_message['packet_id'], int, "packet_id is not an integer")

    def test_placeholder(self):
        """
        A placeholder test.
        Replace this with actual tests for your analysis code.
        """
        self.assertTrue(True, "This is a placeholder test that should be replaced.")

    def test_import(self):
        """
        Example of how you might import and test a module.
        """
        # from telemtry.analysis import my_module
        # self.assertIsNotNone(my_module)
        pass

if __name__ == '__main__':
    unittest.main()
