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


def send_cells_v_burst(tick):
    # Packet 208 in can.json: 35 frames, each frame carries 4 cell voltages (uint16 with precision 0.0001)
    for frame_idx in range(35):
        base = 30000 + ((tick * 3 + frame_idx * 7) % 2000)
        cell0 = base
        cell1 = base + 1
        cell2 = base + 2
        cell3 = base + 3
        payload = struct.pack("<HHHH", cell0, cell1, cell2, cell3)
        send_packet(208, payload)

print(f"LHR Mock CAN active. Sending to {UDP_IP}:{UDP_PORT}...")

tick = 0
while True:
    apps1 = 32000 + (tick % 2000)
    apps2 = 14000 + ((tick * 3) % 2000)
    apps1_travel = 30000 + ((tick * 5) % 3000)
    apps2_travel = 12000 + ((tick * 7) % 3000)
    apps_data = struct.pack("<HHHH", apps1, apps2, apps1_travel, apps2_travel)
    send_packet(448, apps_data)
    
    inverter = 420 + (tick % 40)
    motor = 40 + (tick % 10)
    ambient = 25 + ((tick // 2) % 8)
    discharge = 30 + ((tick // 3) % 8)
    inv_temps = struct.pack("<hhhh", inverter, motor, ambient, discharge)
    send_packet(386, inv_temps)

    pedal_travel = 4500 + ((tick * 11) % 2000)
    faults = 0x01 if (tick // 20) % 2 == 0 else 0x00
    apps_fault_data = struct.pack("<HB", pedal_travel, faults)
    send_packet(449, apps_fault_data)

    hv_pack_mv = 39500 + ((tick * 13) % 1000)
    hv_c_cs = 4500 + ((tick * 9) % 1200)
    hv_soc_cs = 7800 + ((tick * 4) % 600)
    cell_top = 28 + ((tick // 4) % 6)
    cell_bottom = 26 + ((tick // 5) % 6)
    batt_data = struct.pack("<HHHBB", hv_pack_mv, hv_c_cs, hv_soc_cs, cell_top, cell_bottom)
    send_packet(161, batt_data)

    send_cells_v_burst(tick)

    tick += 1
    time.sleep(0.1) # 10Hz loop