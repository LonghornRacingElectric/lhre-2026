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
    # Packet 208: 35 frames, each frame carries 4 cell voltages (uint16, precision 0.0001).
    # Distribute bursts across both mock interfaces so dual-bus cand has traffic on each.
    for frame_idx in range(35):
        base = 30000 + ((tick * 3 + frame_idx * 7) % 2000)
        payload = struct.pack("<HHHH", base, base + 1, base + 2, base + 3)
        target_sock = sock0 if (frame_idx % 2) == 0 else sock1
        target_port = UDP_PORT_0 if (frame_idx % 2) == 0 else UDP_PORT_1
        send_packet(target_sock, 208, payload, target_port)


def send_cells_t_burst(tick):
    # Packet 256: 23 frames, each with 4 cell temps (uint16, precision 0.1).
    # Raw 300-360 → displayed 30-36 °C. Distributed across both buses.
    for frame_idx in range(23):
        base = 300 + ((tick + frame_idx * 2) % 60)
        payload = struct.pack("<HHHH", base, base + 1, base + 2, base + 3)
        target_sock = sock0 if (frame_idx % 2) == 0 else sock1
        target_port = UDP_PORT_0 if (frame_idx % 2) == 0 else UDP_PORT_1
        send_packet(target_sock, 256, payload, target_port)


print(f"LHR Mock CAN active. Sending to {UDP_IP}:{UDP_PORT_0} and {UDP_IP}:{UDP_PORT_1}...")

tick = 0
while True:
    # --- packet 448: APPS Voltages + travels (can0) ---
    apps1 = 32000 + (tick % 2000)
    apps2 = 14000 + ((tick * 3) % 2000)
    apps1_travel = 30000 + ((tick * 5) % 3000)
    apps2_travel = 12000 + ((tick * 7) % 3000)
    send_packet(sock0, 448, struct.pack("<HHHH", apps1, apps2, apps1_travel, apps2_travel), UDP_PORT_0)

    # --- packet 449: Accelerator Pedal travel + faults (can0) ---
    pedal_travel = 4500 + ((tick * 11) % 2000)
    faults = 0x01 if (tick // 20) % 2 == 0 else 0x00
    send_packet(sock0, 449, struct.pack("<HB", pedal_travel, faults), UDP_PORT_0)

    # --- packet 450: BPPS Voltages + travels (can0) ---
    bpps1_v = 25000 + ((tick * 4) % 5000)
    bpps2_v = 25500 + ((tick * 5) % 5000)
    bpps1_travel = 20000 + ((tick * 11) % 5000)
    bpps2_travel = 20500 + ((tick * 13) % 5000)
    send_packet(sock0, 450, struct.pack("<HHHH", bpps1_v, bpps2_v, bpps1_travel, bpps2_travel), UDP_PORT_0)

    # --- packet 453: Brakes (pressures + brake bias) (can0) ---
    # brake_pressure_f / rbll / rall: uint16, precision 0.05 → raw 800 ≈ 40 psi.
    bp_f = 800 + ((tick * 5) % 400)
    bp_rbll = 750 + ((tick * 7) % 400)
    bp_rall = 760 + ((tick * 6) % 400)
    brake_bias = 55                            # uint8, precision 0.01 → 0.55 (55% front)
    bse_faults = 0
    send_packet(sock0, 453, struct.pack("<HHHBB", bp_f, bp_rbll, bp_rall, brake_bias, bse_faults), UDP_PORT_0)

    # --- packet 308: Indicators + Shutdown legs (can0, vehicle-critical) ---
    # bmb_comm_error (false=OK), imd_gnd_isolation_error (false=OK),
    # shutdown_leg1..4 (true=OK).
    send_packet(sock0, 308, struct.pack("<BBBBBB", 0, 0, 1, 1, 1, 1), UDP_PORT_0)

    # --- packet 320: VCU Shutdown Status (BSPD + E-meter as OK) (can0) ---
    # Bitfield: bit 0 = bspd, bit 1 = emeter. Both true → 0x03.
    send_packet(sock0, 320, struct.pack("<B", 0x03), UDP_PORT_0)

    # --- packet 288: DUI R2D Status + temp_shutdown bits (can0) ---
    # r2d_status=1 (in R2D), temp_shutdown_1/2 cleared (no thermal fault).
    # extract_can_data inverts temp_shutdown_*, so bit=0 → dash shows OK.
    send_packet(sock0, 288, struct.pack("<BB", 1, 0), UDP_PORT_0)

    # --- packet 386: Inverter / Motor / Ambient / Discharge temps (can1) ---
    # int16, precision 0.01 → raw 4500 = 45.00 °C.
    inverter_t = 4500 + (tick % 400)
    motor_t = 6000 + (tick % 200)
    ambient_t = 2500 + (tick % 100)
    discharge_t = 3500 + (tick % 100)
    send_packet(sock1, 386, struct.pack("<hhhh", inverter_t, motor_t, ambient_t, discharge_t), UDP_PORT_1)

    # --- packet 162: Inverter Temps 2 (coolant temp at byte 0) (can1) ---
    # int16, precision 0.1 → raw 400 = 40.0 °C.
    coolant_t = 400 + ((tick // 3) % 100)
    send_packet(sock1, 162, struct.pack("<hhhh", coolant_t, 600, 500, 500), UDP_PORT_1)

    # --- packet 166: Inverter Current (dc_bus_current at byte 6) (can1) ---
    # int16, precision 0.1 → raw 600 = 60.0 A.
    dc_bus_c = 600 + ((tick * 7) % 400)
    send_packet(sock1, 166, struct.pack("<hhhh", 500 + (tick % 200), 450, 480, dc_bus_c), UDP_PORT_1)

    # --- packet 167: Inverter Voltage (dc_bus_v at byte 0) (can1) ---
    # int16, precision 0.1 → raw 3800 = 380.0 V.
    dc_bus_v_raw = 3800 + ((tick * 3) % 200)
    send_packet(sock1, 167, struct.pack("<hhhh", dc_bus_v_raw, 0, 0, 0), UDP_PORT_1)

    # --- packet 306: Battery Pack Status (HV V/A/SoC + cell top/bottom temps) (can1) ---
    hv_pack_mv = 39500 + ((tick * 13) % 1000)
    hv_c_cs = 4500 + ((tick * 9) % 1200)
    hv_soc_cs = 7800 + ((tick * 4) % 600)
    cell_top = 28 + ((tick // 4) % 6)
    cell_bottom = 26 + ((tick // 5) % 6)
    send_packet(sock1, 306, struct.pack("<HHHBB", hv_pack_mv, hv_c_cs, hv_soc_cs, cell_top, cell_bottom), UDP_PORT_1)

    # --- packet 387: LV Battery V/A/T (can1) ---
    # lv_batt_v: uint16, 0.01 → raw 2350 = 23.5 V.
    # lv_batt_c: int16, 0.01.
    # lv_batt_t: int16, 0.01.
    lv_v_cs = 2350 + (tick % 100)
    lv_c_cs = 350 + ((tick * 2) % 200)
    lv_t_cs = 2500
    send_packet(sock1, 387, struct.pack("<Hhh", lv_v_cs, lv_c_cs, lv_t_cs), UDP_PORT_1)

    # --- packet 1024: Wheel Speeds (FL/FR/BL/BR) (can1, data-acq bus) ---
    # int16, precision 0.01.
    fl = 1500 + (tick % 500)
    fr = 1480 + ((tick * 2) % 500)
    bl = 1490 + ((tick * 3) % 500)
    br = 1510 + ((tick * 5) % 500)
    send_packet(sock1, 1024, struct.pack("<hhhh", fl, fr, bl, br), UDP_PORT_1)

    # --- Cell voltage + temp bursts (split across both buses) ---
    send_cells_v_burst(tick)
    send_cells_t_burst(tick)

    tick += 1
    time.sleep(0.1)  # 10 Hz loop
