import struct
import time
import os

# use python-can with socketcan backend
import can

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


def wait_for_response(bus, timeout_ms=1000) -> int:
    """Waits for the 0x013 Response message and returns the status enum."""
    deadline = time.time() + (timeout_ms / 1000.0)
    # recv timeout in seconds (try roughly 1/10th of the provided ms)
    poll_interval = (timeout_ms / 10) / 1000.0
    while time.time() < deadline:
        msg = bus.recv(timeout=poll_interval)
        if msg is None:
            continue
        if msg.arbitration_id == CAN_ID_RESP and msg.data and len(msg.data) >= 1:
            return msg.data[0]
    return None


def set_bus_state(bus, enable: bool, fw_update: bool, device_id: int):
    """Send 0x011 Bus Enable/Disable message over socketcan.

    Payload: Enable (byte 0), FW Update (byte 1), Device (byte 2), Unused (bytes 3-7)
    """
    payload = bytearray([int(enable), int(fw_update), device_id, 0, 0, 0, 0, 0])
    msg = can.Message(arbitration_id=CAN_ID_BUS_STATE,
                      data=payload,
                      is_extended_id=False)
    bus.send(msg)

    state_str = "ENABLED" if enable else "DISABLED"
    mode_str = "FW UPDATE" if fw_update else "NORMAL"
    print(f"Bus state set to: {state_str}, Mode: {mode_str}, Target Device: {device_id}")
    time.sleep(0.2)  # Give nodes time to fall silent


def send_firmware_block(bus, address: int, num_blocks: int, block_data: bytes) -> bool:
    """Sends a single 256-byte block: Command -> Data Packets -> Wait for final ACK."""

    # 1. Calculate CRC for this block
    expected_crc = calculate_crc8(block_data)

    # 2. Pack and send command message (0x010)
    cmd_payload = struct.pack('<B I H B', UPDATE_COMMAND_WRITE, address, num_blocks, expected_crc)
    cmd_msg = can.Message(arbitration_id=CAN_ID_CMD,
                          data=cmd_payload,
                          is_extended_id=False)
    bus.send(cmd_msg)

    resp = wait_for_response(bus)
    if resp != UPDATE_RESPONSE_ACK:
        print(f"Failed to get ACK for Command Packet. Response: {resp}")
        return False

    # 3. Send Data Packets (0x012)
    for index in range(37):
        offset = index * BYTES_PER_PACKET
        chunk = block_data[offset:offset + BYTES_PER_PACKET]

        data_payload = bytearray([index]) + chunk
        data_msg = can.Message(arbitration_id=CAN_ID_DATA,
                               data=data_payload,
                               is_extended_id=False)
        bus.send(data_msg)

        resp = wait_for_response(bus, timeout_ms=500)

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


def flash_firmware(bus, file_path: str, start_address: int, target_device_id: int):
    """Reads a binary file, silences the bus, and sends the firmware over CAN via socketcan."""
    if not os.path.exists(file_path):
        print("Firmware file not found.")
        return False

    print("Silencing the bus...")
    set_bus_state(bus, enable=False, fw_update=True, device_id=target_device_id)

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

        success = send_firmware_block(bus, current_address, num_blocks_0indexed, block_data)
        if not success:
            print(f"Update failed at block {block_num + 1}/{total_blocks}")
            abort_msg = can.Message(arbitration_id=CAN_ID_CMD,
                                    data=struct.pack('<B I H B', UPDATE_COMMAND_ABORT, 0, 0, 0),
                                    is_extended_id=False)
            bus.send(abort_msg)
            set_bus_state(bus, enable=False, fw_update=False, device_id=target_device_id)
            return False

        current_address += FW_BLOCK_SIZE

    print("Firmware update completed successfully!")
    print("Re-enabling the bus...")
    set_bus_state(bus, enable=True, fw_update=False, device_id=target_device_id)
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
        bus = can.interface.Bus(bustype='socketcan', channel='can0', bitrate=500000)
        success = flash_firmware(
            bus,
            firmware_path,
            start_address=start_address,
            target_device_id=target_device_id,
        )
        if not success:
            raise SystemExit(1)

    except Exception as e:
        print(f"Failed to initialize socketcan bus: {e}")
        raise SystemExit(1)
    finally:
        if 'bus' in locals():
            try:
                bus.shutdown()
            except Exception:
                pass
