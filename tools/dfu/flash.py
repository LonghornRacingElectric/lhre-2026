import argparse
import subprocess
import sys
import os

from python.runfiles import runfiles


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

    command = [
        dfu_util_exe_actual_path,
        "-f",
    ]

    try:
        subprocess.run(command, check=True)
        print("--- Flash Complete ---")
    except Exception as e:
        print(f"An unexpected error occurred: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
