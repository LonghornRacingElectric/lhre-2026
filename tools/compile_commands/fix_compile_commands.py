#!/usr/bin/env python3
"""
Fix compile_commands.json for clangd compatibility.

This script handles both ARM cross-compiled and host-compiled targets:
1. Splits combined "-isystem path" entries into ["-isystem", "path"]
2. Injects --target=arm-none-eabi for ARM GCC entries so clangd understands them
3. Strips unsupported -mcpu values (e.g. apple_m1) from host/zig entries
4. Removes GCC-only flags that clang doesn't support (-mthumb-interwork, etc.)

Run this after regenerating compile_commands.json with Bazel/hedron.
"""

import json
import sys
import re

# Flags that clang/clangd does not understand (GCC-specific)
REMOVE_FLAGS = {
    "-mthumb-interwork",
    "-fdiagnostics-color",
}

# Zig toolchain -mcpu values that clangd's clang doesn't recognise on host targets
UNSUPPORTED_MCPU_RE = re.compile(r"^-mcpu=")


def _is_arm_entry(args):
    """Return True if the compiler path looks like an ARM GCC cross-compiler."""
    if args:
        return "arm-none-eabi" in args[0]
    return False


def fix_arguments(args):
    """Apply all fixups to a single entry's argument list."""
    is_arm = _is_arm_entry(args)

    fixed = []
    skip_next = False
    for i, arg in enumerate(args):
        if skip_next:
            skip_next = False
            continue

        # ── Split combined -isystem/-I/-iquote ────────────────────────
        combined = re.match(r"^(-isystem|-iquote|-I)\s+(.+)$", arg)
        if combined:
            fixed.append(combined.group(1))
            fixed.append(combined.group(2))
            continue

        # ── Remove GCC-only flags ─────────────────────────────────────
        if arg in REMOVE_FLAGS:
            continue

        # ── Remove -frandom-seed (GCC-only, value is next arg or =val) ─
        if arg.startswith("-frandom-seed"):
            continue

        # ── Strip unsupported -mcpu on host entries ───────────────────
        if not is_arm and UNSUPPORTED_MCPU_RE.match(arg):
            continue

        fixed.append(arg)

    # ── Inject --target for ARM entries so clangd uses the right triple ─
    if is_arm and "--target=arm-none-eabi" not in fixed:
        # Insert right after the compiler path (index 1)
        fixed.insert(1, "--target=arm-none-eabi")

    return fixed


def main():
    input_file = "compile_commands.json"
    if len(sys.argv) > 1:
        input_file = sys.argv[1]

    print(f"Reading {input_file}...")
    with open(input_file, "r") as f:
        compile_commands = json.load(f)

    entries_fixed = 0
    for entry in compile_commands:
        if "arguments" in entry:
            original = entry["arguments"]
            entry["arguments"] = fix_arguments(original)
            if entry["arguments"] != original:
                entries_fixed += 1

    print(f"Fixed {entries_fixed} entries.")

    with open(input_file, "w") as f:
        json.dump(compile_commands, f, indent=2)

    print(f"Wrote fixed {input_file}")


if __name__ == "__main__":
    main()
