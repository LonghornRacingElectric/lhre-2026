import serial
import lgpio
import time
import argparse
import sys

# Serial Defines
PORT = "/dev/ttyUSB2"
BAUDRATE = 115200

# GPIO setup
FULL_CARD_POWER_OFF = 16
CHIP = 0  # Default GPIO chip on Raspberry Pi 5

# Open GPIO chip
h = lgpio.gpiochip_open(CHIP)

def check_module_status():
    try:
        lgpio.gpio_claim_input(h, FULL_CARD_POWER_OFF)
        current_state = lgpio.gpio_read(h, FULL_CARD_POWER_OFF)
        print(f"DEBUG: GPIO pin {FULL_CARD_POWER_OFF} current state: {current_state}")
        if current_state == 1:
            return True, "Cellular module is currently powered ON"
        else:
            return False, "Cellular module is currently powered OFF"
    except Exception as e:
        print(f"Error checking module status: {e}")
        return None, f"Error checking status: {e}"


def shutdown_module():
    try:
        ser = serial.Serial(port=PORT, baudrate=BAUDRATE, timeout=1)
        ser.write(b"AT+CFUN=0\r\n")  # Send AT command for shutdown
        response = ser.readline().strip()

        if response == b"OK":
            lgpio.gpio_claim_output(h, FULL_CARD_POWER_OFF, 0)
            lgpio.gpio_write(h, FULL_CARD_POWER_OFF, 0)  # Set pin LOW
            time.sleep(1)
            print("Cellular module powered OFF successfully")
        else:
            print("Module Shutdown Failed")
    except serial.SerialException as e:
        print(f"SERIAL SHUTDOWN ERROR: {e}")
    except Exception as e:
        print(f"UNEXPECTED SHUTDOWN ERROR: {e}")

def power_on_module():
    try:
        lgpio.gpio_claim_output(h, FULL_CARD_POWER_OFF, 1)
        lgpio.gpio_write(h, FULL_CARD_POWER_OFF, 1)  # Set pin HIGH
        time.sleep(1)
        print("Cellular module powered ON successfully")
    except Exception as e:
        print(f"UNEXPECTED POWER_ON ERROR: {e}")

def main():
    parser = argparse.ArgumentParser(description='Control cellular module power')
    parser.add_argument('action', choices=['on', 'off', 'status'], 
                        help='Action to perform: "on" to power on, "off" to power off, "status" to check current state')
    
    args = parser.parse_args()
    
    try:
        if args.action == 'status':
            is_on, status_msg = check_module_status()
            print(status_msg)
        elif args.action == 'on':
            is_on, status_msg = check_module_status()
            if is_on:
                print("Cellular module is already powered ON")
            else:
                power_on_module()
        elif args.action == 'off':
            is_on, status_msg = check_module_status()
            if not is_on:
                print("Cellular module is already powered OFF")
            else:
                shutdown_module()
    except KeyboardInterrupt:
        print("\nOperation cancelled by user")
        sys.exit(1)
    except Exception as e:
        print(f"Error: {e}")
        sys.exit(1)
    finally:
        lgpio.gpiochip_close(h)  # Cleanup GPIO

if __name__ == "__main__":
    main()