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
    apps_data = struct.pack("<HHHH", 33000, 15000, 33000, 15000)
    send_packet(240, apps_data)
    
    inv_temps = struct.pack("<hhhh", 450, 46, 44, 35)
    send_packet(160, inv_temps)

    pedal_travel = 5000 # uint16
    faults = 0x01       # bit0 set
    apps_fault_data = struct.pack("<HB", pedal_travel, faults)
    send_packet(241, apps_fault_data)

    batt_data = struct.pack("<HHHBB", 40000, 5000, 8500, 30, 28)
    send_packet(512, batt_data)

    time.sleep(0.1) # 10Hz loop