import struct
import time
import os
import can

# --- Protocol Constants ---
CAN_ID_CMD = 0x010
CAN_ID_BUS_STATE = 0x011
CAN_ID_DATA = 0x012
CAN_ID_RESP = 0x013

# Updated to 256 byte blocks
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


def wait_for_response(bus: can.Bus, timeout_ms=1000) -> int:
    """Waits for the 0x013 Response packet and returns the status enum."""
    timeout_sec = timeout_ms / 1000.0
    end_time = time.time() + timeout_sec

    while time.time() < end_time:
        try:
            # python-can recv blocks until a message arrives or it times out
            msg = bus.recv(timeout=end_time - time.time())
            if (
                msg is not None
                and msg.arbitration_id == CAN_ID_RESP
                and len(msg.data) >= 1
            ):
                return msg.data[0]
        except Exception as e:
            print(f"CAN Read Error: {e}")
            break
    return None


def set_bus_state(bus: can.Bus, enable: bool, fw_update: bool, device_id: int):
    """
    Sends the 0x011 Bus Enable/Disable packet.
    Payload: Enable (byte 0), FW Update (byte 1), Device (byte 2), Unused (bytes 3-7)
    """
    payload = bytearray([int(enable), int(fw_update), device_id, 0, 0, 0, 0, 0])
    msg = can.Message(
        arbitration_id=CAN_ID_BUS_STATE, data=payload, is_extended_id=False
    )
    bus.send(msg)

    state_str = "ENABLED" if enable else "DISABLED"
    mode_str = "FW UPDATE" if fw_update else "NORMAL"
    print(
        f"Bus state set to: {state_str}, Mode: {mode_str}, Target Device: {device_id}"
    )
    time.sleep(0.2)  # Give nodes time to fall silent


def send_firmware_block(
    bus: can.Bus, address: int, num_blocks: int, block_data: bytes
) -> bool:
    """Sends a single 256-byte block: Command -> Data Packets -> Wait for final ACK."""

    # 1. Calculate CRC for this block
    expected_crc = calculate_crc8(block_data)

    # 2. Pack and Send Command Packet (0x010)
    cmd_payload = struct.pack(
        "<B I H B", UPDATE_COMMAND_WRITE, address, num_blocks, expected_crc
    )
    cmd_msg = can.Message(
        arbitration_id=CAN_ID_CMD, data=cmd_payload, is_extended_id=False
    )
    bus.send(cmd_msg)

    resp = wait_for_response(bus)
    if resp != UPDATE_RESPONSE_ACK:
        print(f"Failed to get ACK for Command Packet. Response: {resp}")
        return False

    # 3. Send Data Packets (0x012)
    # Math note: 37 packets total. 36 * 7 = 252 bytes. The 37th packet holds the final 4 bytes.
    for index in range(37):
        offset = index * BYTES_PER_PACKET
        chunk = block_data[offset : offset + BYTES_PER_PACKET]

        data_payload = bytearray([index]) + chunk
        data_msg = can.Message(
            arbitration_id=CAN_ID_DATA, data=data_payload, is_extended_id=False
        )
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


def flash_firmware(
    bus: can.Bus, file_path: str, start_address: int, target_device_id: int
):
    """Reads a binary file, silences the bus, and sends the firmware over CAN."""
    if not os.path.exists(file_path):
        print("Firmware file not found.")
        return

    print("Silencing the bus...")
    set_bus_state(bus, enable=False, fw_update=True, device_id=target_device_id)

    with open(file_path, "rb") as f:
        firmware = f.read()

    total_blocks = (len(firmware) + FW_BLOCK_SIZE - 1) // FW_BLOCK_SIZE
    print(
        f"Starting firmware update. Size: {len(firmware)} bytes ({total_blocks} blocks)."
    )

    current_address = start_address

    for block_num in range(total_blocks):
        offset = block_num * FW_BLOCK_SIZE
        block_data = firmware[offset : offset + FW_BLOCK_SIZE]

        # Pad the last block with 0xFF if it's smaller than the block size
        if len(block_data) < FW_BLOCK_SIZE:
            block_data = block_data.ljust(FW_BLOCK_SIZE, b"\xff")

        success = send_firmware_block(bus, current_address, total_blocks, block_data)
        if not success:
            print(f"Update failed at block {block_num + 1}/{total_blocks}")
            abort_payload = struct.pack("<B I H B", UPDATE_COMMAND_ABORT, 0, 0, 0)
            abort_msg = can.Message(
                arbitration_id=CAN_ID_CMD, data=abort_payload, is_extended_id=False
            )
            bus.send(abort_msg)
            set_bus_state(
                bus, enable=False, fw_update=False, device_id=target_device_id
            )
            return

        current_address += FW_BLOCK_SIZE

    print("Firmware update completed successfully!")
    print("Re-enabling the bus...")
    set_bus_state(bus, enable=True, fw_update=False, device_id=target_device_id)


if __name__ == "__main__":
    try:
        # Initialize SocketCAN. Usually, the bitrate is set via the OS (e.g., `ip link set can0 type can bitrate 500000`).
        # The channel 'can0' is the standard default for SocketCAN interfaces on Linux.
        bus = can.interface.Bus(bustype="socketcan", channel="can0")

        # Example: DEVICE_ID_VCU is usually 2 based on your enum
        flash_firmware(
            bus=bus,
            file_path="/home/home/Documents/Code/lhre-2026/bazel-bin/DUI/firmware/dui_firmware_2026.bin",
            start_address=0x00000000,
            target_device_id=6,
        )

    except Exception as e:
        print(f"Failed to initialize SocketCAN: {e}")
    finally:
        if "bus" in locals():
            bus.shutdown()
