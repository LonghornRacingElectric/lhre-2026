#!/usr/bin/env python3
"""
Fix compile_commands.json for clangd compatibility with GCC ARM cross-compilation.

This script:
1. Splits combined "-isystem path" entries into ["-isystem", "path"]
2. Also handles any other combined -I/-isystem/-iquote patterns

Run this after regenerating compile_commands.json with Bazel/hedron.
"""

import json
import sys
import re


def fix_arguments(args):
    """Split combined -isystem/-I/-iquote entries into separate args."""
    fixed = []
    for arg in args:
        # Match patterns like "-isystem path/to/dir" (with space in single string)
        match = re.match(r"^(-isystem|-iquote|-I)\s+(.+)$", arg)
        if match:
            fixed.append(match.group(1))
            fixed.append(match.group(2))
        else:
            fixed.append(arg)
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
            original_len = len(entry["arguments"])
            entry["arguments"] = fix_arguments(entry["arguments"])
            if len(entry["arguments"]) != original_len:
                fixes_made += 1

    print(f"Fixed {fixes_made} entries with combined -isystem/-I/-iquote flags.")

    with open(input_file, "w") as f:
        json.dump(compile_commands, f, indent=2)

    print(f"Wrote fixed {input_file}")


if __name__ == "__main__":
    main()
