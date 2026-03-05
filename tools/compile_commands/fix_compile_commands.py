#!/usr/bin/env python3
"""
Fix compile_commands.json for clangd compatibility with GCC ARM cross-compilation.

This script:
1. Splits combined "-isystem path" entries into ["-isystem", "path"]
2. Also handles any other combined -I/-isystem/-iquote patterns
3. Adds `--target=arm-none-eabi` explicitly to embedded commands
4. Removes host CPU flags (like -mcpu=apple_m1) from host commands

Run this after regenerating compile_commands.json with Bazel/hedron.
"""

import json
import sys
import re


def is_arm_compiler(compiler_cmd):
    """Check if the compiler being called targets ARM."""
    return "arm-none-eabi" in compiler_cmd


def should_remove_host_flag(arg, is_arm):
    """Return True if the argument is a host-only flag that clangd rejects."""
    # If not compiling for ARM, strip ALL -mcpu=* flags since Bazel injects
    # apple_m1 or similar host targets which clangd doesn't recognize natively.
    if not is_arm and arg.startswith("-mcpu="):
        return True
    return False


def fix_arguments(args):
    """Split combined entries and add necessary clangd targeting flags."""
    if not args:
        return args

    is_arm = is_arm_compiler(args[0])
    fixed = []

    for arg in args:
        if should_remove_host_flag(arg, is_arm):
            continue

        # Match patterns like "-isystem path/to/dir" (with space in single string)
        match = re.match(r"^(-isystem|-iquote|-I)\s+(.+)$", arg)
        if match:
            fixed.append(match.group(1))
            fixed.append(match.group(2))
        else:
            fixed.append(arg)

    # For embedded targets, ensure clangd correctly identifies the target triple
    if is_arm and "--target=arm-none-eabi" not in fixed:
        # Insert target right after the compiler
        fixed.insert(1, "--target=arm-none-eabi")
        # Also suppress unknown warnings (like mthumb-interwork) if desired
        fixed.insert(2, "-Wno-unknown-warning-option")

    return fixed


def main():
    input_file = "compile_commands.json"
    if len(sys.argv) > 1:
        input_file = sys.argv[1]

    print(f"Reading {input_file}...")
    with open(input_file, "r") as f:
        compile_commands = json.load(f)

    fixes_made = 0
    for entry in compile_commands:
        if "arguments" in entry:
            original = entry["arguments"]
            entry["arguments"] = fix_arguments(original)
            if entry["arguments"] != original:
                fixes_made += 1

    print(f"Fixed {fixes_made} commands by adjusting targets and flags.")

    with open(input_file, "w") as f:
        json.dump(compile_commands, f, indent=2)

    print(f"Wrote fixed {input_file}")


if __name__ == "__main__":
    main()
