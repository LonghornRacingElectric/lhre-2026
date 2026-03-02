import struct
import time
import os
from canlib import canlib, Frame

# --- Protocol Constants ---
CAN_ID_CMD = 0x010
CAN_ID_BUS_STATE = 0x011
CAN_ID_DATA = 0x012
CAN_ID_RESP = 0x013

FW_BLOCK_SIZE = 256
BYTES_PER_PACKET = 7

# --- Enums (Matching C Header) ---
UPDATE_COMMAND_WRITE = 0
UPDATE_COMMAND_READ = 1
UPDATE_COMMAND_ERASE = 2
UPDATE_COMMAND_VERIFY = 3
UPDATE_COMMAND_RESET = 4
UPDATE_COMMAND_ABORT = 5

UPDATE_RESPONSE_ACK = 0
UPDATE_RESPONSE_NACK = 1
UPDATE_RESPONSE_CRC_ERROR = 2
UPDATE_RESPONSE_BUSY = 3

def calculate_crc8(data: bytes) -> int:
    """Standard CRC-8 (Polynomial: 0x07) to match the C firmware."""
    crc = 0x00
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x80:
                crc = ((crc << 1) ^ 0x07) & 0xFF
            else:
                crc = (crc << 1) & 0xFF
    return crc

def wait_for_response(channel, timeout_ms=1000) -> int:
    """Waits for the 0x013 Response packet and returns the status enum."""
    timeout_time = time.time() + (timeout_ms / 1000.0)
    while time.time() < timeout_time:
        try:
            frame = channel.read(timeout=int(timeout_ms/10))
            if frame.id == CAN_ID_RESP and len(frame.data) >= 1:
                return frame.data[0]
        except canlib.CanNoMsg:
            continue
        except Exception as e:
            print(f"CAN Read Error: {e}")
            break
    return None

def set_bus_state(channel, enable: bool, fw_update: bool, device_id: int):
    """
    Sends the 0x011 Bus Enable/Disable packet.
    Payload: Enable (byte 0), FW Update (byte 1), Device (byte 2), Unused (bytes 3-7)
    """
    payload = bytearray([int(enable), int(fw_update), device_id, 0, 0, 0, 0, 0])
    frame = Frame(id_=CAN_ID_BUS_STATE, data=payload, flags=canlib.MessageFlag.STD)
    channel.write(frame)
    
    state_str = "ENABLED" if enable else "DISABLED"
    mode_str = "FW UPDATE" if fw_update else "NORMAL"
    print(f"Bus state set to: {state_str}, Mode: {mode_str}, Target Device: {device_id}")
    time.sleep(0.2) # Give nodes time to fall silent

def send_firmware_block(channel, address: int, num_blocks: int, block_data: bytes) -> bool:
    """Sends a single 256-byte block: Command -> Data Packets -> Wait for final ACK."""
    
    # 1. Calculate CRC for this block
    expected_crc = calculate_crc8(block_data)
    
    # 2. Pack and Send Command Packet (0x010)
    cmd_payload = struct.pack('<B I H B', UPDATE_COMMAND_WRITE, address, num_blocks, expected_crc)
    cmd_frame = Frame(id_=CAN_ID_CMD, data=cmd_payload, flags=canlib.MessageFlag.STD)
    channel.write(cmd_frame)
    
    resp = wait_for_response(channel)
    if resp != UPDATE_RESPONSE_ACK:
        print(f"Failed to get ACK for Command Packet. Response: {resp}")
        return False

    # 3. Send Data Packets (0x012)
    for index in range(37):
        offset = index * BYTES_PER_PACKET
        chunk = block_data[offset:offset + BYTES_PER_PACKET]
        
        data_payload = bytearray([index]) + chunk
        data_frame = Frame(id_=CAN_ID_DATA, data=data_payload, flags=canlib.MessageFlag.STD)
        channel.write(data_frame)
        
        resp = wait_for_response(channel, timeout_ms=500)
        
        if index < 36:
            if resp != UPDATE_RESPONSE_BUSY:
                print(f"Expected BUSY on index {index}, got: {resp}")
                return False
        else:
            if resp == UPDATE_RESPONSE_CRC_ERROR:
                print("Node reported CRC Error!")
                return False
            elif resp != UPDATE_RESPONSE_ACK:
                print(f"Expected ACK on final data packet, got: {resp}")
                return False

    print(f"Successfully wrote block to address 0x{address:08X}")
    return True

def flash_firmware(channel, file_path: str, start_address: int, target_device_id: int):
    """Reads a binary file, silences the bus, and sends the firmware over CAN."""
    if not os.path.exists(file_path):
        print("Firmware file not found.")
        return False

    print("Silencing the bus...")
    set_bus_state(channel, enable=False, fw_update=True, device_id=target_device_id)

    with open(file_path, 'rb') as f:
        firmware = f.read()

    total_blocks = (len(firmware) + FW_BLOCK_SIZE - 1) // FW_BLOCK_SIZE
    num_blocks_0indexed = total_blocks - 1  # protocol uses 0-indexed block count
    print(f"Starting firmware update. Size: {len(firmware)} bytes ({total_blocks} blocks).")

    current_address = start_address

    for block_num in range(total_blocks):
        offset = block_num * FW_BLOCK_SIZE
        block_data = firmware[offset:offset + FW_BLOCK_SIZE]

        if len(block_data) < FW_BLOCK_SIZE:
            block_data = block_data.ljust(FW_BLOCK_SIZE, b'\xFF')

        success = send_firmware_block(channel, current_address, num_blocks_0indexed, block_data)
        if not success:
            print(f"Update failed at block {block_num + 1}/{total_blocks}")
            abort_frame = Frame(id_=CAN_ID_CMD, data=struct.pack('<B I H B', UPDATE_COMMAND_ABORT, 0, 0, 0), flags=canlib.MessageFlag.STD)
            channel.write(abort_frame)
            set_bus_state(channel, enable=False, fw_update=False, device_id=target_device_id)
            return False

        current_address += FW_BLOCK_SIZE

    print("Firmware update completed successfully!")
    print("Re-enabling the bus...")
    set_bus_state(channel, enable=True, fw_update=False, device_id=target_device_id)
    return True

if __name__ == "__main__":
    import sys

    # Usage: bootload.py <firmware_path> [start_address] <target_device_id>
    # Examples:
    #   bootload.py /tmp/fw.bin 6
    #   bootload.py /tmp/fw.bin 0x00000000 6
    if len(sys.argv) not in (3, 4):
        print("Usage: bootload.py <firmware_path> [start_address] <target_device_id>")
        raise SystemExit(2)

    firmware_path = sys.argv[1]
    if len(sys.argv) == 3:
        start_address = 0x00000000
        target_device_id = int(sys.argv[2])
    else:
        start_address = int(sys.argv[2], 0)
        target_device_id = int(sys.argv[3])

    try:
        ch = canlib.openChannel(channel=0, flags=canlib.Open.ACCEPT_VIRTUAL)
        ch.setBusParams(canlib.canBITRATE_500K)
        ch.busOn()
        success = flash_firmware(
            ch,
            firmware_path,
            start_address=start_address,
            target_device_id=target_device_id,
        )
        if not success:
            raise SystemExit(1)
        
    except Exception as e:
        print(f"Failed to initialize Kvaser CAN: {e}")
        raise SystemExit(1)
    finally:
        if 'ch' in locals():
            ch.busOff()
            ch.close()