"""
telemtry/stack/tests/test_protobuf.py

Unit tests for protobuf serialization/deserialization.
These tests do not require Docker containers to be running.
"""

import unittest
import os
import sys
import time

# Add the workspace root to the path
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.dirname(__file__)))))


class TestProtobufSerialization(unittest.TestCase):
    """Unit tests for protobuf message handling."""
    
    def test_template_protobuf_import(self):
        """Test that template protobuf can be imported."""
        try:
            from stack.ingest.protobuf.template_pb2 import SensorData
            self.assertIsNotNone(SensorData, "SensorData should be importable")
        except ImportError as e:
            self.skipTest(f"Protobuf import failed: {e}")
    
    def test_angelique_protobuf_import(self):
        """Test that Angelique protobuf can be imported."""
        try:
            from stack.ingest.protobuf.angelique_pb2 import AngeliqueSensorData
            self.assertIsNotNone(AngeliqueSensorData, "AngeliqueSensorData should be importable")
        except ImportError as e:
            self.skipTest(f"Protobuf import failed: {e}")
    
    def test_sensor_data_serialization(self):
        """Test SensorData protobuf serialization."""
        try:
            from stack.ingest.protobuf.template_pb2 import SensorData
            
            # Create a message
            data = SensorData()
            data.packet_id = 12345
            data.time = int(time.time() * 1000)
            
            # Serialize
            serialized = data.SerializeToString()
            self.assertIsInstance(serialized, bytes, "Serialized data should be bytes")
            self.assertGreater(len(serialized), 0, "Serialized data should not be empty")
            
            # Deserialize
            data2 = SensorData()
            data2.ParseFromString(serialized)
            
            self.assertEqual(data.packet_id, data2.packet_id, "packet_id should match after deserialization")
            self.assertEqual(data.time, data2.time, "time should match after deserialization")
            
        except ImportError as e:
            self.skipTest(f"Protobuf import failed: {e}")
    
    def test_sensor_data_to_dict(self):
        """Test SensorData conversion to dictionary (as used in MQTTHandler)."""
        try:
            from stack.ingest.protobuf.template_pb2 import SensorData
            from google.protobuf.json_format import MessageToDict
            
            # Create a message
            data = SensorData()
            data.packet_id = 67890
            data.time = int(time.time() * 1000)
            
            # Convert to dict
            data_dict = MessageToDict(
                data,
                preserving_proto_field_name=True,
                always_print_fields_with_no_presence=True
            )
            
            self.assertIsInstance(data_dict, dict, "Should convert to dict")
            self.assertIn("packet_id", data_dict, "Dict should have packet_id")
            self.assertIn("time", data_dict, "Dict should have time")
            
        except ImportError as e:
            self.skipTest(f"Protobuf or google.protobuf import failed: {e}")
    
    def test_angelique_sensor_data_serialization(self):
        """Test AngeliqueSensorData protobuf serialization."""
        try:
            from stack.ingest.protobuf.angelique_pb2 import AngeliqueSensorData
            
            # Create a message
            data = AngeliqueSensorData()
            data.packet_id = 11111
            data.time = int(time.time() * 1000)
            
            # Serialize and deserialize
            serialized = data.SerializeToString()
            data2 = AngeliqueSensorData()
            data2.ParseFromString(serialized)
            
            self.assertEqual(data.packet_id, data2.packet_id, "packet_id should match")
            
        except ImportError as e:
            self.skipTest(f"Protobuf import failed: {e}")
    
    def test_nested_message_fields(self):
        """Test protobuf nested message fields (dynamics, controls, etc.)."""
        try:
            from stack.ingest.protobuf.template_pb2 import SensorData
            
            data = SensorData()
            data.packet_id = 22222
            data.time = int(time.time() * 1000)
            
            # Check for nested message types
            if hasattr(data, 'dynamics'):
                # Set some dynamics fields if available
                if hasattr(data.dynamics, 'f_gps_velocity'):
                    data.dynamics.f_gps_velocity = 50.5
                if hasattr(data.dynamics, 'f_gps_heading'):
                    data.dynamics.f_gps_heading = 180.0
            
            if hasattr(data, 'controls'):
                if hasattr(data.controls, 'accel_pedal_t'):
                    data.controls.accel_pedal_t = 0.75
            
            # Serialize and deserialize
            serialized = data.SerializeToString()
            data2 = SensorData()
            data2.ParseFromString(serialized)
            
            self.assertEqual(data.packet_id, data2.packet_id)
            
        except ImportError as e:
            self.skipTest(f"Protobuf import failed: {e}")


class TestDataTesterPatterns(unittest.TestCase):
    """
    Tests for data generation patterns used in paho_testing.py.
    These validate that the test data generator creates valid data.
    """
    
    def test_random_data_generation(self):
        """Test random data generation for various types."""
        from telemtry.stack.tests.test_utils import TestDataGenerator
        
        generator = TestDataGenerator(seed=42)
        
        # Generate sensor data
        data = generator.generate_sensor_data(packet_id=1)
        
        self.assertIsInstance(data, dict, "Should generate a dictionary")
        self.assertIn("packet_id", data, "Should have packet_id")
        self.assertIn("time", data, "Should have time")
        self.assertEqual(data["packet_id"], 1, "packet_id should match")
    
    def test_protobuf_message_generation(self):
        """Test protobuf message generation."""
        from telemtry.stack.tests.test_utils import TestDataGenerator
        
        generator = TestDataGenerator(seed=42)
        
        # Generate protobuf message
        message = generator.generate_protobuf_message(packet_id=100)
        
        if message is not None:
            self.assertIsInstance(message, bytes, "Should generate bytes")
            self.assertGreater(len(message), 0, "Should not be empty")
        else:
            self.skipTest("Protobuf generation not available")
    
    def test_deterministic_with_seed(self):
        """Test that data generation is deterministic with same seed."""
        from telemtry.stack.tests.test_utils import TestDataGenerator
        
        gen1 = TestDataGenerator(seed=123)
        gen2 = TestDataGenerator(seed=123)
        
        data1 = gen1.generate_sensor_data(packet_id=1)
        data2 = gen2.generate_sensor_data(packet_id=1)
        
        # GPS coordinates should be the same with same seed
        self.assertEqual(
            data1["dynamics"]["f_gps_velocity"],
            data2["dynamics"]["f_gps_velocity"],
            "Same seed should produce same velocity"
        )


if __name__ == "__main__":
    unittest.main()
