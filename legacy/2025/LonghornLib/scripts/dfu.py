import platform
import serial
import serial.tools.list_ports
import os
import time
import sys
import threading
# Added prompt_toolkit imports
from prompt_toolkit import PromptSession
from prompt_toolkit.patch_stdout import patch_stdout

# --- ANSI Color Codes ---
class bcolors:
    HEADER = '\033[95m'
    OKBLUE = '\033[94m'
    OKCYAN = '\033[96m'
    OKGREEN = '\033[92m'
    WARNING = '\033[93m'
    FAIL = '\033[91m'
    ENDC = '\033[0m'
    BOLD = '\033[1m'
    UNDERLINE = '\033[4m'

# --- Helper Functions ---
def find_first_file(directory, extension=".bin"):
    """Finds the first file with the given extension in a directory."""
    for root, dirs, files in os.walk(directory):
        for file in files:
            if file.endswith(extension):
                return os.path.join(root, file)
    return None

def run_command(command, ignore_error = False):
    """Runs a shell command and prints its output."""
    print(f"{bcolors.BOLD}Running: {command}{bcolors.ENDC}")
    # Use sys.stdout.write and flush for potentially better real-time output
    process = os.popen(command)
    while True:
        # Read in chunks to avoid blocking on large output
        output_chunk = process.read(4096)
        if not output_chunk:
            break
        # Use sys.stdout directly; prompt_toolkit will handle patching if necessary
        sys.stdout.write(output_chunk)
        sys.stdout.flush() # Ensure output is shown immediately
    status = process.close()
    if status:
        exit_code = status >> 8 # Common way to get exit code from popen status
        if exit_code != 0 and not ignore_error:
            # Use stderr for error messages to keep stdout cleaner if redirected
            sys.stderr.write(f"{bcolors.FAIL}Command failed with exit code {exit_code}{bcolors.ENDC}\n")
            sys.stderr.flush()
            exit(exit_code)

def serial_reader(ser_instance, stop_event):
    """Continuously reads from the serial port and prints lines until stop_event is set."""
    # This thread will print directly to stdout.
    # prompt_toolkit's patch_stdout will ensure it prints above the prompt line.
    print(f"\n{bcolors.OKCYAN}Serial reader thread started.{bcolors.ENDC}", flush=True)
    try:
        buffer = bytearray()
        while not stop_event.is_set():
            try:
                # Read chunk of data, non-blocking
                if ser_instance.in_waiting > 0:
                    data = ser_instance.read(ser_instance.in_waiting)
                    buffer.extend(data)

                    # Process complete lines from the buffer
                    while True:
                        try:
                            # Find newline character
                            newline_pos = buffer.find(b'\n')
                            if newline_pos == -1:
                                break # No complete line yet

                            # Extract line (including newline)
                            line_bytes = buffer[:newline_pos+1]
                            del buffer[:newline_pos+1] # Remove line from buffer

                            # Decode and print
                            line_str = line_bytes.decode('utf-8', errors='replace').rstrip()
                            # Print directly - patch_stdout will handle placement
                            print(line_str, flush=True)

                        except UnicodeDecodeError as e:
                            sys.stderr.write(f"{bcolors.WARNING}Decode error: {e}{bcolors.ENDC}\n")
                            sys.stderr.flush()
                            # If decode fails, potentially discard the problematic part or print raw bytes
                            # For simplicity here, we might lose some data if a newline was part of bad bytes
                            if newline_pos != -1: # If we found a newline but failed decode
                                del buffer[:newline_pos+1] # Still remove it to avoid infinite loop
                            else: # If no newline, maybe clear buffer on error? Risky.
                                buffer.clear() # Or handle more gracefully

                else:
                    # No data waiting, sleep briefly to avoid busy-waiting
                    time.sleep(0.05) # Adjust sleep time as needed

            except serial.SerialException as e:
                # Check if the error is due to the port closing or a real issue
                if not stop_event.is_set():
                    sys.stderr.write(f"{bcolors.FAIL}Serial error: {e}{bcolors.ENDC}\n")
                    sys.stderr.flush()
                break # Exit thread on serial error
            except OSError as e:
                # Catch potential OS errors (e.g., device disconnected)
                if not stop_event.is_set():
                    sys.stderr.write(f"{bcolors.FAIL}Serial OS error: {e}{bcolors.ENDC}\n")
                    sys.stderr.flush()
                break
            except Exception as e: # Catch other potential errors during read/decode loop
                if not stop_event.is_set():
                    sys.stderr.write(f"{bcolors.FAIL}Error in reader loop: {e}{bcolors.ENDC}\n")
                    sys.stderr.flush()
                # Decide whether to break or continue based on the error
                break # Safer to break on unknown errors

    except Exception as e:
        # Catch errors during thread setup/outer loop logic itself
        if not stop_event.is_set():
            sys.stderr.write(f"{bcolors.FAIL}Critical error in reader thread: {e}{bcolors.ENDC}\n")
            sys.stderr.flush()
    finally:
        # Ensure final message is printed clearly
        print(f"\n{bcolors.OKCYAN}Serial reader thread finished.{bcolors.ENDC}", flush=True)


# --- Main Script Logic ---

# [ ... Keep Steps 1 through 6 exactly the same as before ... ]
# 1. Build Project
print(bcolors.OKBLUE + "--- Building Project ---" + bcolors.ENDC)
if len(sys.argv) < 2:
    print(f"{bcolors.FAIL}Error: Build target not specified.{bcolors.ENDC}")
    print("Usage: python dfu.py <target_name> [serial_desc_keyword] [serial_hwid]", file=sys.stderr)
    exit(1)

target = sys.argv[1]
build_dir = "cmake-build-debug" # Or your actual build directory

run_command(f"cmake -B {build_dir}/ -DCMAKE_BUILD_TYPE=PRODUCTION")
run_command(f"cmake --build {build_dir}/ --config PRODUCTION --target {target} -j 8")

# 2. Find Binary File
print(bcolors.OKCYAN + f"--- Searching for .bin file in '{build_dir}' ---" + bcolors.ENDC)
binpath = find_first_file(build_dir)

if not binpath:
    print(f"{bcolors.FAIL}No binary file (.bin) found in '{build_dir}'. Make sure the build was successful.{bcolors.ENDC}", file=sys.stderr)
    exit(-1)

print(bcolors.OKGREEN + f"Found binary file: {binpath}\n" + bcolors.ENDC)

# 3. Find Device Serial Port
print(bcolors.OKCYAN + "--- Finding Device Serial Port ---" + bcolors.ENDC)
ports = serial.tools.list_ports.comports()
serial_port_name = None
# Default values
target_desc = "lhre"  # Default description keyword
target_hwid = "0483:5740" # Default VID:PID
target_desc_windows = "USB Serial Device"

# Allow overriding description keyword from command line
if len(sys.argv) > 2:
    target_desc = sys.argv[2]
    print(f"Using custom serial description keyword: '{target_desc}'")

# Allow overriding HWID from command line (optional)
# Example: python dfu.py mytarget mykeyword VID:PID
if len(sys.argv) > 3:
    target_hwid = sys.argv[3]
    print(f"Using custom HWID: '{target_hwid}'")


print(f"Searching for port with description containing '{target_desc}' OR HWID containing '{target_hwid}'")
for port in sorted(ports):
    print(f"Checking port: {port.device} - {port.description} [{port.hwid}]")
    # Ensure checks handle None values gracefully
    desc_match = target_desc and port.description and target_desc.lower() in port.description.lower()
    hwid_match = target_hwid and port.hwid and target_hwid.lower() in port.hwid.lower()
    bullshit_windows_match = target_desc_windows and port.description and target_desc_windows.lower() in port.description.lower()

    if desc_match or hwid_match or bullshit_windows_match:
        print(bcolors.OKGREEN + f"Found target device at port {port.device}" + bcolors.ENDC)
        serial_port_name = port.device
        break # Stop after finding the first match

if not serial_port_name:
    print(f"{bcolors.WARNING}Target device not found automatically based on description/HWID. Proceeding directly to DFU.{bcolors.ENDC}")
else:
    # 4. Send Update Command (Optional - only if device found)
    print(bcolors.OKBLUE + "--- Sending 'update' command ---" + bcolors.ENDC)
    try:
        # Short timeout for the initial command send
        with serial.Serial(serial_port_name, baudrate=115200, timeout=1) as ser:
            ser.write("update\n".encode('utf-8')) # Send update command (\n is often enough)
            #ser.write("update\n\r".encode('utf-8')) # Or use \n\r if needed
            ser.flush() # Ensure data is sent
            print("Command sent. Waiting for device to potentially reset...")
        time.sleep(2) # Give time for the device to potentially reset to DFU mode
    except serial.SerialException as e:
        print(f"{bcolors.FAIL}Could not open or write to {serial_port_name}: {e}{bcolors.ENDC}", file=sys.stderr)
        print("Proceeding to DFU update anyway.")
    except Exception as e:
        print(f"{bcolors.FAIL}An unexpected error occurred: {e}{bcolors.ENDC}", file=sys.stderr)
        print("Proceeding to DFU update anyway.")

import re # Need to import the regex module

def find_first_path_for_device_regex(data_string, device_id="0483:df11"):
    # Regex to find path="..." and capture the content inside quotes
    # We'll apply this *only* to the line that matches the device_id
    path_regex = re.compile(r'path="([^"]*)"')

    for line in data_string.splitlines():
        if device_id in line:
            # Found the first line with the device ID
            match = path_regex.search(line) # Search for the path pattern *on this line*
            if match:
                # Pattern found, return the captured group (the content inside quotes)
                return match.group(1)
            else:
                # Device ID found, but path="xxx" pattern not found on this line.
                # Since we need the path from the *first* matching line, stop and return None.
                return None
    # If the loop finishes without finding the device_id
    return None


# 5. Update Device via DFU
print(bcolors.OKCYAN + "\n--- Updating Device via DFU ---" + bcolors.ENDC)

process = os.popen("dfu-util --list")

dfu_path = find_first_path_for_device_regex(process.read())

# Note: Ensure dfu-util is installed and in your system's PATH
# Use the correct DFU VID:PID for STM32 bootloader
dfu_vid_pid = "0483:df11"
dfu_command = f"dfu-util -a 0 -p {dfu_path} --dfuse-address 0x08000000 -D \"{binpath}\""
print(f"DFU VID:PID used: -p {dfu_path}")



run_command(dfu_command, ignore_error=True)
run_command(f"dfu-util -a 0 -p {dfu_path} -s :leave", ignore_error=True)
# The ':leave' suffix should handle the reset. If not, uncomment the lines below.
# run_command("dfu-util -a 0 -d 0483:df11 -s 0x08000000:leave", ignore_error=True) # Alternative leave command

# If ':leave' doesn't work reliably:
# run_command(f"dfu-util -a 0 -d {dfu_vid_pid} --dfuse-address 0x08000000 -D \"{binpath}\"")
# print(bcolors.OKCYAN + "--- Resetting device ---" + bcolors.ENDC)
# run_command(f"dfu-util -a 0 -d {dfu_vid_pid} -e", ignore_error=True) # -e performs the reset/leave action

print(f"{bcolors.OKGREEN}DFU Update process finished.{bcolors.ENDC}")
print("Waiting for device to re-enumerate...")
time.sleep(5) # Increase delay to allow device to reset and reappear as serial

# 6. Re-Find Serial Port (Device might have a new port name after reset)
print(bcolors.OKCYAN + "\n--- Finding Serial Port Post-Update ---" + bcolors.ENDC)
serial_port_name_after_dfu = None
retries = 5
found = False
print(f"Searching again for port with description containing '{target_desc}' OR HWID containing '{target_hwid}'")
for i in range(retries):
    ports = serial.tools.list_ports.comports()
    for port in sorted(ports):
        # Use the same matching logic as before
        desc_match = target_desc and port.description and target_desc.lower() in port.description.lower()
        hwid_match = target_hwid and port.hwid and target_hwid.lower() in port.hwid.lower()
        bullshit_windows_match = target_desc_windows and port.description and target_desc_windows.lower() in port.description.lower()

        if desc_match or hwid_match or bullshit_windows_match:
            print(bcolors.OKGREEN + f"Found device post-update at port {port.device}" + bcolors.ENDC)
            serial_port_name_after_dfu = port.device
            found = True
            break # Stop inner loop after finding the first match
    if found:
        break # Stop outer loop if found
    print(f"Device not found, retrying ({i+1}/{retries})...")
    time.sleep(2) # Wait before retrying


# 7. Connect to Serial Monitor (Using prompt_toolkit)
if serial_port_name_after_dfu:
    print(bcolors.OKBLUE + f"--- Opening Serial Monitor on {serial_port_name_after_dfu} ---" + bcolors.ENDC)
    print(f"{bcolors.WARNING}Type your commands and press Enter. Press Ctrl+C or Ctrl+D to exit.{bcolors.ENDC}")

    stop_reader_event = threading.Event()
    reader_thread = None
    ser = None # Initialize ser outside try block
    session = PromptSession() # Create a prompt session object

    try:
        ser = serial.Serial(serial_port_name_after_dfu, baudrate=115200, timeout=0.1) # Use short timeout

        # Start the reader thread (will print directly to stdout)
        reader_thread = threading.Thread(target=serial_reader, args=(ser, stop_reader_event), daemon=True)
        reader_thread.start()

        # Main thread handles user input using prompt_toolkit
        while True:
            try:
                # Use patch_stdout to ensure prints from the reader thread
                # appear above the prompt line.
                with patch_stdout():
                    # Prompt the user for input. This handles rendering the prompt
                    # and user input correctly, even with background prints.
                    command = session.prompt('> ')

                # Send command to serial port
                ser.write((command + '\n').encode('utf-8'))
                ser.flush() # Ensure command is sent immediately

            except EOFError: # Handle Ctrl+D
                print(f"\n{bcolors.WARNING}EOF detected, exiting.{bcolors.ENDC}")
                break
            except KeyboardInterrupt: # Handle Ctrl+C
                print(f"\n{bcolors.WARNING}Ctrl+C detected. Exiting serial monitor.{bcolors.ENDC}")
                break
            except serial.SerialException as e:
                # Use stderr for errors
                sys.stderr.write(f"{bcolors.FAIL}Serial write error: {e}{bcolors.ENDC}\n")
                sys.stderr.flush()
                break # Exit if writing fails
            except Exception as e:
                sys.stderr.write(f"{bcolors.FAIL}Error during prompt/write: {e}{bcolors.ENDC}\n")
                sys.stderr.flush()
                break


    except serial.SerialException as e:
        sys.stderr.write(f"{bcolors.FAIL}Failed to open serial port {serial_port_name_after_dfu}: {e}{bcolors.ENDC}\n")
        sys.stderr.flush()
    except KeyboardInterrupt: # Handle Ctrl+C during setup
        print(f"\n{bcolors.WARNING}Ctrl+C detected during setup. Exiting.{bcolors.ENDC}")
    except Exception as e:
        sys.stderr.write(f"{bcolors.FAIL}An unexpected error occurred setting up serial monitor: {e}{bcolors.ENDC}\n")
        sys.stderr.flush()
    finally:
        # --- Cleanup ---
        print(f"\n{bcolors.OKBLUE}--- Closing Serial Monitor ---{bcolors.ENDC}", flush=True)

        # 1. Signal the reader thread to stop
        stop_reader_event.set()

        # 2. Close the serial port (important!)
        if ser and ser.is_open:
            try:
                ser.close()
                print(f"Serial port {serial_port_name_after_dfu} closed.", flush=True)
            except Exception as e:
                sys.stderr.write(f"{bcolors.WARNING}Error closing serial port: {e}{bcolors.ENDC}\n")
                sys.stderr.flush()


        # 3. Wait for the reader thread to finish
        if reader_thread and reader_thread.is_alive():
            print(f"Waiting for reader thread to finish...", flush=True)
            reader_thread.join(timeout=2) # Wait for the reader thread to exit gracefully
            if reader_thread.is_alive():
                # Use stderr for warnings
                sys.stderr.write(f"{bcolors.WARNING}Reader thread did not stop cleanly.{bcolors.ENDC}\n")
                sys.stderr.flush()
            else:
                print("Reader thread finished.", flush=True)

else:
    # Use stderr for error messages
    sys.stderr.write(f"{bcolors.FAIL}Could not find the serial port after DFU update. Cannot open serial monitor.{bcolors.ENDC}\n")
    sys.stderr.flush()


print(f"{bcolors.OKGREEN}--- Script Finished ---{bcolors.ENDC}")
