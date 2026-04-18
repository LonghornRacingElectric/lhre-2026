import argparse
import os
import re
import subprocess
import sys
import serial
import serial.tools.list_ports
import time


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


# Boards that have location variants (FR, FL, RR, RL).
# These generate per-location DFU targets like :dfu_FR instead of a plain :dfu.
VARIANT_BOARDS = {
    "CSM": ["FR", "FL", "RR", "RL"],
    "USM": ["FR", "FL", "RR", "RL"],
}

# Map from target name to the bazel DFU target.
# Variant boards use the format "BOARD_LOCATION" -> "//BOARD/firmware:dfu_LOCATION".
BOARD_TO_DFU_TARGET = {
    # Boards without variants
    "DUI": "//DUI/firmware:dfu",
    "HVC": "//HVC/firmware:dfu",
    "LVBMS": "//LVBMS/firmware:dfu",
    "PDU": "//PDU/firmware:dfu",
    "TSM": "//TSM/firmware:dfu",
    "VCU": "//VCU/firmware:dfu",
}

# Add variant board entries
for board, locations in VARIANT_BOARDS.items():
    for loc in locations:
        BOARD_TO_DFU_TARGET[f"{board}_{loc}"] = f"//{board}/firmware:dfu_{loc}"

# The USB VID:PID for STM32 CDC (application mode)
APP_HWID = "0483:5740"
# The USB VID:PID for STM32 DFU bootloader mode
DFU_DEVICE_ID = "0483:df11"


def find_target_port(retries=10, delay=2.0):
    """
    Finds the target device serial port using HWID/Description.
    Retries several times to handle USB re-enumeration after a flash.
    """
    print(bcolors.OKCYAN + "--- Finding Device Serial Port ---" + bcolors.ENDC)

    target_desc = "lhre"
    target_desc_windows = "USB Serial Device"

    for attempt in range(retries):
        ports = serial.tools.list_ports.comports()

        for port in sorted(ports):
            desc_match = (
                target_desc
                and port.description
                and target_desc.lower() in port.description.lower()
            )
            hwid_match = (
                APP_HWID and port.hwid and APP_HWID.lower() in port.hwid.lower()
            )
            windows_match = (
                target_desc_windows
                and port.description
                and target_desc_windows.lower() in port.description.lower()
            )

            if desc_match or hwid_match or windows_match:
                print(
                    bcolors.OKGREEN
                    + f"Found target device at port {port.device}"
                    + bcolors.ENDC
                )
                return port.device

        if attempt < retries - 1:
            print(
                f"  Device not found (attempt {attempt + 1}/{retries}), "
                f"retrying in {delay}s..."
            )
            time.sleep(delay)

    return None


def flash_firmware(target):
    """
    Flashes the board by running the Bazel DFU target. The board must already
    be connected. The DFU flash script handles sending the 'update.' command
    to enter bootloader mode and then flashing the binary.

    Returns True on success, False on failure.
    """
    dfu_target = BOARD_TO_DFU_TARGET.get(target.upper())
    if not dfu_target:
        print(
            f"{bcolors.FAIL}Unknown board target '{target}'. "
            f"Valid targets: {', '.join(BOARD_TO_DFU_TARGET.keys())}{bcolors.ENDC}",
            file=sys.stderr,
        )
        return False

    print(bcolors.OKBLUE + f"--- Flashing firmware via {dfu_target} ---" + bcolors.ENDC)

    try:
        result = subprocess.run(
            ["bazel", "run", "--copt=-DHIL", dfu_target],
            timeout=120,
        )
        if result.returncode != 0:
            print(
                f"{bcolors.FAIL}Flash failed with exit code {result.returncode}{bcolors.ENDC}",
                file=sys.stderr,
            )
            return False

        # Give the board time to reset and re-enumerate USB after flashing
        print("Waiting for board to boot after flash...")
        time.sleep(5)
        return True

    except subprocess.TimeoutExpired:
        print(
            f"{bcolors.FAIL}Flash timed out after 120 seconds{bcolors.ENDC}",
            file=sys.stderr,
        )
        return False
    except FileNotFoundError:
        print(
            f"{bcolors.FAIL}Could not find 'bazel' executable. "
            f"Is Bazel installed and on PATH?{bcolors.ENDC}",
            file=sys.stderr,
        )
        return False


def drain_buffer(ser, timeout=2.0):
    """
    Reads and discards all pending data in the serial buffer.
    This clears out boot messages and any other garbage before starting the
    echo test.
    """
    ser.reset_input_buffer()
    ser.reset_output_buffer()

    old_timeout = ser.timeout
    ser.timeout = timeout

    drained = 0
    while True:
        data = ser.read(1024)
        if not data:
            break
        drained += len(data)

    ser.timeout = old_timeout

    if drained > 0:
        print(f"  Drained {drained} bytes of boot/stale data from buffer.")


def run_hil_test(port, target, duration_seconds=15, baudrate=115200):
    """
    Connects to the port and continuously sends/receives messages
    to verify board stability over the given duration.

    The firmware (when compiled with -DHIL) echoes back the exact raw
    buffer it receives via CDC_Transmit_FS — no extra \r\n is appended.
    We compare using .strip() to be resilient to any trailing whitespace.
    """
    print(
        bcolors.OKBLUE
        + f"--- Starting HIL Echo Test on {port} ({target}) "
        + f"for {duration_seconds}s ---"
        + bcolors.ENDC
    )

    try:
        # Timeout set to 2 seconds so we don't give up too quickly if the
        # board is busy with other RTOS tasks. A genuine crash will still
        # be caught; it just takes ~2s per failed message instead of 1s.
        with serial.Serial(port, baudrate=baudrate, timeout=2.0) as ser:
            drain_buffer(ser)

            start_time = time.time()
            messages_sent = 0
            consecutive_timeouts = 0
            max_consecutive_timeouts = 3

            while (time.time() - start_time) < duration_seconds:
                test_payload = f"HIL_ECHO_TEST_{messages_sent}"
                # Send with newline so the firmware knows where the message ends
                ser.write((test_payload + "\n").encode("utf-8"))
                ser.flush()

                # Read the response. The firmware's usb_printf appends \r\n,
                # so readline() will return bytes ending in \n (with a \r
                # before it).
                response = ser.readline()

                # Check for timeout (crash / unresponsiveness)
                if not response:
                    consecutive_timeouts += 1
                    print(
                        f"{bcolors.WARNING}WARNING: Read timeout on message "
                        f"{messages_sent} ({consecutive_timeouts}/"
                        f"{max_consecutive_timeouts}){bcolors.ENDC}",
                    )
                    if consecutive_timeouts >= max_consecutive_timeouts:
                        print(
                            f"{bcolors.FAIL}ERROR: {max_consecutive_timeouts} "
                            f"consecutive timeouts. Board may have crashed."
                            f"{bcolors.ENDC}",
                            file=sys.stderr,
                        )
                        return False
                    continue

                # Reset the timeout counter on any successful read
                consecutive_timeouts = 0

                # Decode the response
                try:
                    response_str = response.decode("utf-8").strip()
                except UnicodeDecodeError as e:
                    print(
                        f"{bcolors.FAIL}ERROR: Received malformed bytes: "
                        f"{response!r}. Exception: {e}{bcolors.ENDC}",
                        file=sys.stderr,
                    )
                    return False

                # Compare stripped payload (ignoring \r\n differences)
                if response_str != test_payload:
                    print(
                        f"{bcolors.FAIL}ERROR: Data mismatch!{bcolors.ENDC}",
                        file=sys.stderr,
                    )
                    print(f"  Sent:     '{test_payload}'", file=sys.stderr)
                    print(f"  Received: '{response_str}'", file=sys.stderr)
                    return False

                messages_sent += 1

                # Print progress every 100 messages
                if messages_sent % 100 == 0:
                    elapsed = time.time() - start_time
                    print(
                        f"  Progress: {elapsed:.1f}/{duration_seconds}s "
                        f"- {messages_sent} messages verified..."
                    )

                # Small delay to avoid saturating the USB bus
                time.sleep(0.01)

            elapsed_time = time.time() - start_time
            print(bcolors.OKGREEN + "--- HIL Echo Test Passed! ---" + bcolors.ENDC)
            print(
                f"Successfully exchanged {messages_sent} messages "
                f"over {elapsed_time:.2f} seconds."
            )
            return True

    except serial.SerialException as e:
        print(f"{bcolors.FAIL}Serial port error: {e}{bcolors.ENDC}", file=sys.stderr)
        return False
    except Exception as e:
        print(
            f"{bcolors.FAIL}Unexpected error during test: {e}{bcolors.ENDC}",
            file=sys.stderr,
        )
        return False


def main():
    parser = argparse.ArgumentParser(
        description="Hardware-In-the-Loop (HIL) Serial Stability Test"
    )
    parser.add_argument(
        "--target",
        type=str,
        required=True,
        help=(
            "Board target to test (e.g. DUI, VCU, CSM_FR, USM_RL, ...) or "
            "'dfu' for a driver-only DFU test. Boards with location variants "
            "(CSM, USM) must specify the variant, e.g. CSM_FR."
        ),
    )
    parser.add_argument(
        "--duration",
        type=int,
        default=15,
        help="Test duration in seconds (default: 15)",
    )
    parser.add_argument(
        "--baud",
        type=int,
        default=115200,
        help="Baudrate (default: 115200)",
    )
    parser.add_argument(
        "--skip-flash",
        action="store_true",
        help="Skip the flashing step (assume the board already has the right firmware)",
    )
    args = parser.parse_args()

    # Normalize target: 'dfu' maps to DUI since that's the DFU test board
    target = args.target.upper()
    if target == "DFU":
        target = "DUI"
        print(
            bcolors.OKCYAN
            + "DFU target requested — using DUI board for driver validation."
            + bcolors.ENDC
        )

    # Check if the user passed a bare variant board name without a location
    if target in VARIANT_BOARDS:
        variants = VARIANT_BOARDS[target]
        print(
            f"{bcolors.FAIL}'{target}' has location variants. "
            f"Please specify one: {', '.join(f'{target}_{v}' for v in variants)}"
            f"{bcolors.ENDC}",
            file=sys.stderr,
        )
        sys.exit(1)

    if target not in BOARD_TO_DFU_TARGET:
        print(
            f"{bcolors.FAIL}Unknown target '{args.target}'. "
            f"Valid targets: {', '.join(sorted(BOARD_TO_DFU_TARGET.keys()))}, dfu"
            f"{bcolors.ENDC}",
            file=sys.stderr,
        )
        sys.exit(1)

    # --- Step 1: Flash the firmware ---
    if not args.skip_flash:
        if not flash_firmware(target):
            print(
                f"{bcolors.FAIL}Flashing failed. Aborting HIL test.{bcolors.ENDC}",
                file=sys.stderr,
            )
            sys.exit(1)

    # --- Step 2: Find the serial port ---
    port = find_target_port()
    if not port:
        print(
            f"{bcolors.FAIL}Failed to locate target device after flashing. "
            f"Aborting test.{bcolors.ENDC}",
            file=sys.stderr,
        )
        sys.exit(1)

    # --- Step 3: Run the echo stability test ---
    success = run_hil_test(
        port, target, duration_seconds=args.duration, baudrate=args.baud
    )

    if success:
        print(f"\n{bcolors.OKGREEN}{bcolors.BOLD}=== HIL TEST PASSED ==={bcolors.ENDC}")
        sys.exit(0)
    else:
        print(f"\n{bcolors.FAIL}{bcolors.BOLD}=== HIL TEST FAILED ==={bcolors.ENDC}")
        sys.exit(1)


if __name__ == "__main__":
    main()
