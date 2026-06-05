import argparse
import subprocess
import sys
import os
import re
import shutil
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


def command_works(command):
    try:
        result = subprocess.run(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=5,
        )
    except (OSError, subprocess.SubprocessError):
        return False

    return result.returncode == 0


def command_output(command):
    result = subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        timeout=5,
    )
    return result.stdout


def resolve_dfu_util(bundled_dfu_util_path):
    candidates = [
        bundled_dfu_util_path,
        shutil.which("dfu-util"),
        "/opt/homebrew/bin/dfu-util",
        "/usr/local/bin/dfu-util",
    ]

    seen = set()
    for candidate in candidates:
        if not candidate or candidate in seen:
            continue
        seen.add(candidate)
        if command_works([candidate, "--version"]):
            if candidate != bundled_dfu_util_path:
                print(
                    f"{bcolors.WARNING}Bundled dfu-util is unavailable; using {candidate}{bcolors.ENDC}"
                )
            return candidate

    raise RuntimeError(
        "No working dfu-util found. Install one with `brew install dfu-util` "
        "or fix the bundled dfu-util/libusb runtime."
    )


def wait_for_dfu_path(dfu_util_path, timeout_s=10.0):
    deadline = time.time() + timeout_s

    while time.time() < deadline:
        output = command_output([dfu_util_path, "--list"])
        dfu_path = find_first_path_for_device_regex(output)
        if dfu_path:
            return dfu_path
        time.sleep(0.5)

    return None


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
    resolved_dfu_util_path = resolve_dfu_util(dfu_util_exe_actual_path)

    print("--- Flashing Firmware (Paths resolved via Runfiles Library) ---")
    print(f"Working Directory:      {os.getcwd()}")
    print(f"Resolved DFU Util Path:  {dfu_util_exe_actual_path}")
    print(f"Using DFU Util Path:     {resolved_dfu_util_path}")
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

        print(bcolors.OKCYAN + "--- Waiting for STM32 DFU Device ---" + bcolors.ENDC)
        dfu_path = wait_for_dfu_path(resolved_dfu_util_path)
        if not dfu_path:
            raise RuntimeError("No STM32 DFU device found.")

        # Flash to both banks so the board boots correctly regardless of
        # which bank BFB2 is currently pointing at (OTA toggles BFB2).
        # Bank 1: 0x08000000, Bank 2: 0x08040000 (STM32G4, 512K flash)
        for bank_addr in ["0x08000000"]:
            dfu_command = (
                f'{resolved_dfu_util_path} -a 0 -p "{dfu_path}" '
                f'--dfuse-address {bank_addr} -D "{firmware_elf_actual_path}"'
            )
            print(f"{bcolors.OKBLUE}--- Flashing to {bank_addr} ---{bcolors.ENDC}")
            run_command(dfu_command, ignore_error=False)

        run_command(
            f'{resolved_dfu_util_path} -a 0 -p "{dfu_path}" -s :leave',
            ignore_error=True,
        )

        print(f"{bcolors.OKGREEN}DFU Update process finished.{bcolors.ENDC}")
        print("--- Flash Complete ---")
    except Exception as e:
        print(f"An unexpected error occurred: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
