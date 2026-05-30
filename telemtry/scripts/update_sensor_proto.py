#!/usr/bin/env python3
"""
Update sensor.proto with Orion schema from can_packets.proto.
Preserves non-Orion messages and updates OrionSensorData structure with correct
field numbering and enumeration.
"""

import re
import sys
from pathlib import Path


def extract_messages(proto_content):
    """
    Extract all message definitions from proto content.
    Returns dict: {message_name: (start_pos, end_pos, full_text)}
    """
    messages = {}
    pattern = r'^message\s+(\w+)\s*\{(.*?)\n\}'
    
    for match in re.finditer(pattern, proto_content, re.MULTILINE | re.DOTALL):
        msg_name = match.group(1)
        start_pos = match.start()
        end_pos = match.end()
        full_text = match.group(0)
        messages[msg_name] = (start_pos, end_pos, full_text)
    
    return messages


def extract_orion_messages(can_packets_proto):
    """Extract all Orion-related message definitions from can_packets.proto.
    
    Maps unprefixed names in can_packets.proto to Orion-prefixed names in sensor.proto:
    - Dynamics -> OrionDynamics
    - Controls -> OrionControls
    - Pack -> OrionPack
    - DiagnosticsHigh -> OrionDiagnosticsHigh
    - DiagnosticsLow -> OrionDiagnosticsLow
    - Thermal -> OrionThermal
    - BoardStatus -> OrionBoardStatus
    """
    messages = extract_messages(can_packets_proto)
    
    # Mapping from can_packets.proto names to sensor.proto names
    name_mapping = {
        'Dynamics': 'OrionDynamics',
        'Controls': 'OrionControls',
        'Pack': 'OrionPack',
        'DiagnosticsHigh': 'OrionDiagnosticsHigh',
        'DiagnosticsLow': 'OrionDiagnosticsLow',
        'Thermal': 'OrionThermal',
        'BoardStatus': 'OrionBoardStatus',
    }
    
    orion_messages = {}
    for source_name, target_name in name_mapping.items():
        if source_name in messages:
            source_def = messages[source_name][2]
            # Replace message name in definition
            updated_def = source_def.replace(f'message {source_name}', f'message {target_name}')
            orion_messages[target_name] = (source_def, updated_def)
    
    return orion_messages


def update_sensor_proto(sensor_proto_path, can_packets_proto_path):
    """
    Update sensor.proto with Orion schema from can_packets.proto.
    
    Maps unprefixed message names from can_packets.proto to Orion-prefixed names
    in sensor.proto, while preserving field numbering and enumeration.
    
    Args:
        sensor_proto_path: Path to sensor.proto (target)
        can_packets_proto_path: Path to can_packets.proto (source)
    
    Returns:
        Updated sensor.proto content
    """
    # Read source files
    with open(can_packets_proto_path, 'r') as f:
        can_packets_content = f.read()
    
    with open(sensor_proto_path, 'r') as f:
        sensor_content = f.read()
    
    # Extract Orion messages from can_packets.proto
    orion_messages = extract_orion_messages(can_packets_content)
    
    if not orion_messages:
        print("Warning: No Orion messages found in can_packets.proto", file=sys.stderr)
        return sensor_content
    
    # Replace each Orion message in sensor.proto
    updated_content = sensor_content
    
    # Pattern to find each Orion message definition
    for target_msg_name, (source_def, updated_def) in orion_messages.items():
        # Find the existing message definition in sensor.proto by target name
        pattern = rf'^message\s+{re.escape(target_msg_name)}\s*\{{.*?\n\}}'
        
        # Replace with new definition (already has correct target name)
        updated_content = re.sub(
            pattern,
            updated_def,
            updated_content,
            count=1,
            flags=re.MULTILINE | re.DOTALL
        )
    
    return updated_content


def main():
    if len(sys.argv) != 3:
        print("Usage: update_sensor_proto.py <sensor.proto> <can_packets.proto>", file=sys.stderr)
        sys.exit(1)
    
    sensor_proto_path = Path(sys.argv[1])
    can_packets_proto_path = Path(sys.argv[2])
    
    # Validate paths
    if not sensor_proto_path.exists():
        print(f"Error: {sensor_proto_path} not found", file=sys.stderr)
        sys.exit(1)
    
    if not can_packets_proto_path.exists():
        print(f"Error: {can_packets_proto_path} not found", file=sys.stderr)
        sys.exit(1)
    
    # Update sensor.proto
    updated_content = update_sensor_proto(sensor_proto_path, can_packets_proto_path)
    
    # Write back to sensor.proto
    with open(sensor_proto_path, 'w') as f:
        f.write(updated_content)
    
    print(f"Updated {sensor_proto_path} with Orion schema from {can_packets_proto_path}")


if __name__ == '__main__':
    main()
