import socket
import struct
import time

UDP_IP = "127.0.0.1"
UDP_PORT = 5005
# Rust format: Little-endian, 4-byte ID, 8-byte Data
CAN_FORMAT = "<I8s"

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def send_packet(packet_id, data_bytes):
    # Ensure data is exactly 8 bytes (padded with zeros)
    padded_data = data_bytes.ljust(8, b'\x00')
    message = struct.pack(CAN_FORMAT, packet_id, padded_data)
    sock.sendto(message, (UDP_IP, UDP_PORT))

print(f"LHR Mock CAN active. Sending to {UDP_IP}:{UDP_PORT}...")

while True:
    # 1. APPS Voltages (Packet 240)
    # Mapping: APPS1_V, APPS2_V, APPS1_T, APPS2_T (all uint16, prec: 0.0001)
    # Send ~3.3V (33000) and ~1.5V (15000)
    apps_data = struct.pack("<HHHH", 33000, 15000, 33000, 15000)
    send_packet(240, apps_data)

    # 2. Inverter Temps (Packet 160)
    # Mapping: Module A, B, C, Gate Driver (all int16, prec: 0.1 or 1.0)
    # Send 45.0C, 46.0C, 44.0C, 35.0C
    inv_temps = struct.pack("<hhhh", 450, 46, 44, 35)
    send_packet(160, inv_temps)

    # 3. APPS Faults Bitfield (Packet 241)
    # byte 2 bitfield: bit0=apps1_disc, bit1=apps2_disc
    # Let's simulate apps1_disconnect (0x01)
    pedal_travel = 5000 # uint16
    faults = 0x01       # bit0 set
    apps_fault_data = struct.pack("<HB", pedal_travel, faults)
    send_packet(241, apps_fault_data)

    # 4. Battery Pack Status (Packet 512)
    # hv_pack_v (uint16, prec: 0.01) -> 400.00V = 40000
    # hv_c (uint16, prec: 0.01)      -> 50.00A  = 5000
    # hv_soc (uint16, prec: 0.01)    -> 85.00%  = 8500
    batt_data = struct.pack("<HHHBB", 40000, 5000, 8500, 30, 28)
    send_packet(512, batt_data)

    time.sleep(0.1) # 10Hz loop