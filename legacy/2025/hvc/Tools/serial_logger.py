import serial
import csv
import re

# --- SETTINGS ---
COM_PORT = 'COM3'  # Change to your serial port (e.g., COM3, /dev/ttyUSB0)
BAUD_RATE = 115200  # Match your MCU serial baudrate
OUTPUT_CSV = 'adc_data_log.csv'

# --- REGEX for parsing the incoming line ---
pattern = re.compile(r"HV Current ADC: ([\d\.\-eE]+), HV Current \(A\): ([\d\.\-eE]+), Time \(ms\): (\d+)")

# --- Open Serial Port ---
ser = serial.Serial(COM_PORT, BAUD_RATE, timeout=1)
print(f"Listening on {COM_PORT} at {BAUD_RATE} baud...")

# --- Open CSV File ---
with open(OUTPUT_CSV, mode='w', newline='') as csvfile:
    csv_writer = csv.writer(csvfile)
    csv_writer.writerow(['ADC Value', 'Current (A)', 'Time (ms)'])  # Write header

    try:
        while True:
            line = ser.readline().decode('utf-8').strip()
            if line:
                match = pattern.search(line)
                if match:
                    adc_val = float(match.group(1))
                    current_a = float(match.group(2))
                    time_ms = int(match.group(3))

                    # Write to CSV
                    csv_writer.writerow([adc_val, current_a, time_ms])
                    csvfile.flush()  # Ensure it's written immediately

                    print(f"Logged: ADC={adc_val:.6f}, Current={current_a:.3f}A, Time={time_ms}ms")
                else:
                    print(f"Unmatched line: {line}")
    except KeyboardInterrupt:
        print("\nStopped by user.")
    finally:
        ser.close()
