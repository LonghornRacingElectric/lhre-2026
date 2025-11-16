import argparse
import subprocess
import sys
import os
import re
import serial
import serial.tools.list_ports
import time

from python.runfiles import runfiles


# --- ANSI Color Codes ---
class bcolors:
    HEADER = "\033[95m"
    OKBLUE = "\033[94m"
    OKCYAN = "\033[96m"
    OKGREEN = "\033[92m"
    WARNING = "\033[93m"
    FAIL = "\033[91m"
    ENDC = "\033[0m"
    BOLD = "\033[1m"
    UNDERLINE = "\033[4m"


def find_first_path_for_device_regex(data_string, device_id="0483:df11"):
    # Regex to find path="..." and capture the content inside quotes
    # We'll apply this *only* to the line that matches the device_id
    path_regex = re.compile(r'path="([^"]*)"')

    for line in data_string.splitlines():
        if device_id in line:
            # Found the first line with the device ID
            match = path_regex.search(
                line
            )  # Search for the path pattern *on this line*
            if match:
                # Pattern found, return the captured group (the content inside quotes)
                return match.group(1)
            else:
                # Device ID found, but path="xxx" pattern not found on this line.
                # Since we need the path from the *first* matching line, stop and return None.
                return None
    # If the loop finishes without finding the device_id
    return None


def run_command(command, ignore_error=False):
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
        sys.stdout.flush()  # Ensure output is shown immediately
    status = process.close()
    if status:
        exit_code = status >> 8  # Common way to get exit code from popen status
        if exit_code != 0 and not ignore_error:
            # Use stderr for error messages to keep stdout cleaner if redirected
            sys.stderr.write(
                f"{bcolors.FAIL}Command failed with exit code {exit_code}{bcolors.ENDC}\n"
            )
            sys.stderr.flush()
            exit(exit_code)


def main():
    parser = argparse.ArgumentParser(
        description="Flash a firmware file to a target using DFU Util."
    )
    # receive the canonical paths from the BUILD file arguments
    parser.add_argument("dfu_util_canonical_path")
    parser.add_argument("firmware_canonical_path")
    args = parser.parse_args()

    r = runfiles.Create()

    dfu_util_exe_actual_path = r.Rlocation(args.dfu_util_canonical_path)
    firmware_elf_actual_path = r.Rlocation(args.firmware_canonical_path)

    print("--- Flashing Firmware (Paths resolved via Runfiles Library) ---")
    print(f"Working Directory:      {os.getcwd()}")
    print(f"Resolved DFU Util Path:  {dfu_util_exe_actual_path}")
    print(f"Resolved Firmware Path: {firmware_elf_actual_path}")
    print("-----------------------------------------------------------------")

    firmware_elf_arg = firmware_elf_actual_path.replace("\\", "/")

    try:
        print(bcolors.OKCYAN + "--- Finding Device Serial Port ---" + bcolors.ENDC)
        ports = serial.tools.list_ports.comports()
        serial_port_name = None

        target_desc = "lhre"
        target_hwid = "0483:5740"
        target_desc_windows = "USB Serial Device"

        print(
            f"Searching for port with description containing '{target_desc}' OR HWID containing '{target_hwid}'"
        )
        for port in sorted(ports):
            print(f"Checking port: {port.device} - {port.description} [{port.hwid}]")
            # Ensure checks handle None values gracefully
            desc_match = (
                target_desc
                and port.description
                and target_desc.lower() in port.description.lower()
            )
            hwid_match = (
                target_hwid and port.hwid and target_hwid.lower() in port.hwid.lower()
            )
            bullshit_windows_match = (
                target_desc_windows
                and port.description
                and target_desc_windows.lower() in port.description.lower()
            )

            if desc_match or hwid_match or bullshit_windows_match:
                print(
                    bcolors.OKGREEN
                    + f"Found target device at port {port.device}"
                    + bcolors.ENDC
                )
                serial_port_name = port.device
                break

        if not serial_port_name:
            print(
                f"{bcolors.WARNING}Target device not found automatically based on description/HWID. Proceeding directly to DFU.{bcolors.ENDC}"
            )
        else:
            print(bcolors.OKBLUE + "--- Sending 'update.' command ---" + bcolors.ENDC)
            try:
                with serial.Serial(serial_port_name, baudrate=115200, timeout=1) as ser:
                    ser.write("update.\n".encode("utf-8"))
                    ser.flush()
                    print("Command sent. Waiting for device to potentially reset...")
                time.sleep(2)
            except serial.SerialException as e:
                print(
                    f"{bcolors.FAIL}Could not open or write to {serial_port_name}: {e}{bcolors.ENDC}",
                    file=sys.stderr,
                )
                print("Proceeding to DFU update anyway.")
            except Exception as e:
                print(
                    f"{bcolors.FAIL}An unexpected error occurred: {e}{bcolors.ENDC}",
                    file=sys.stderr,
                )
                print("Proceeding to DFU update anyway.")

        process = os.popen(f"{dfu_util_exe_actual_path} --list")
        dfu_path = find_first_path_for_device_regex(process.read())
        dfu_command = f'{dfu_util_exe_actual_path} -a 0 -p "{dfu_path}" --dfuse-address 0x08000000 -D "{firmware_elf_actual_path}"'
        print(f'DFU VID:PID used: -p "{dfu_path}"')

        run_command(dfu_command, ignore_error=False)
        run_command(
            f'{dfu_util_exe_actual_path} -a 0 -p "{dfu_path}" -s :leave',
            ignore_error=True,
        )

        print(f"{bcolors.OKGREEN}DFU Update process finished.{bcolors.ENDC}")
        print("--- Flash Complete ---")
    except Exception as e:
        print(f"An unexpected error occurred: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
