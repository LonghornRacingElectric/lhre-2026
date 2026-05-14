import socket
import struct
import time

# Two mock UDP endpoints to simulate can0 and can1
UDP_IP = "127.0.0.1"
UDP_PORT_0 = 5005
UDP_PORT_1 = 5006
# Rust format: Little-endian, 4-byte ID, 8-byte Data
CAN_FORMAT = "<I8s"

sock0 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock1 = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)

def send_packet(sock, packet_id, data_bytes, port):
    # Ensure data is exactly 8 bytes (padded with zeros)
    padded_data = data_bytes.ljust(8, b'\x00')
    message = struct.pack(CAN_FORMAT, packet_id, padded_data)
    sock.sendto(message, (UDP_IP, port))


def send_cells_v_burst(tick):
    # Packet 208 in can.json: 35 frames, each frame carries 4 cell voltages (uint16 with precision 0.0001)
    for frame_idx in range(35):
        base = 30000 + ((tick * 3 + frame_idx * 7) % 2000)
        cell0 = base
        cell1 = base + 1
        cell2 = base + 2
        cell3 = base + 3
        payload = struct.pack("<HHHH", cell0, cell1, cell2, cell3)
        # distribute bursts across both mock interfaces for testing
        target_sock = sock0 if (frame_idx % 2) == 0 else sock1
        target_port = UDP_PORT_0 if (frame_idx % 2) == 0 else UDP_PORT_1
        send_packet(target_sock, 208, payload, target_port)

print(f"LHR Mock CAN active. Sending to {UDP_IP}:{UDP_PORT_0} and {UDP_IP}:{UDP_PORT_1}...")

tick = 0
while True:
    apps1 = 32000 + (tick % 2000)
    apps2 = 14000 + ((tick * 3) % 2000)
    apps1_travel = 30000 + ((tick * 5) % 3000)
    apps2_travel = 12000 + ((tick * 7) % 3000)
    apps_data = struct.pack("<HHHH", apps1, apps2, apps1_travel, apps2_travel)
    # send apps over can0
    send_packet(sock0, 448, apps_data, UDP_PORT_0)
    
    inverter = 420 + (tick % 40)
    motor = 40 + (tick % 10)
    ambient = 25 + ((tick // 2) % 8)
    discharge = 30 + ((tick // 3) % 8)
    inv_temps = struct.pack("<hhhh", inverter, motor, ambient, discharge)
    # send inverter temps over can1
    send_packet(sock1, 386, inv_temps, UDP_PORT_1)

    pedal_travel = 4500 + ((tick * 11) % 2000)
    faults = 0x01 if (tick // 20) % 2 == 0 else 0x00
    apps_fault_data = struct.pack("<HB", pedal_travel, faults)
    # send faults on can0
    send_packet(sock0, 449, apps_fault_data, UDP_PORT_0)

    hv_pack_mv = 39500 + ((tick * 13) % 1000)
    hv_c_cs = 4500 + ((tick * 9) % 1200)
    hv_soc_cs = 7800 + ((tick * 4) % 600)
    cell_top = 28 + ((tick // 4) % 6)
    cell_bottom = 26 + ((tick // 5) % 6)
    batt_data = struct.pack("<HHHBB", hv_pack_mv, hv_c_cs, hv_soc_cs, cell_top, cell_bottom)
    # battery packet on can1
    send_packet(sock1, 161, batt_data, UDP_PORT_1)

    send_cells_v_burst(tick)

    tick += 1
    time.sleep(0.1) # 10Hz loop