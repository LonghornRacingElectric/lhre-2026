#!/usr/bin/env python3
"""
Refresh compile_commands.json by running hedron's refresh_all and then
applying the fix_compile_commands fixups for clangd compatibility.

Usage:
    bazel run //tools/compile_commands:refresh_compile_commands
"""

import os
import subprocess
import sys


def main():
    # Determine the workspace root.
    # When run via `bazel run`, BUILD_WORKSPACE_DIRECTORY is set to the workspace root.
    workspace_dir = os.environ.get("BUILD_WORKSPACE_DIRECTORY")
    if not workspace_dir:
        print(
            "ERROR: BUILD_WORKSPACE_DIRECTORY not set. "
            "This script must be run via `bazel run`.",
            file=sys.stderr,
        )
        sys.exit(1)

    # Step 1: Run hedron compile commands refresh_all
    print("==> Running hedron compile_commands refresh_all...")
    result = subprocess.run(
        ["bazel", "run", "@hedron_compile_commands//:refresh_all"],
        cwd=workspace_dir,
    )
    if result.returncode != 0:
        print("ERROR: hedron refresh_all failed.", file=sys.stderr)
        sys.exit(result.returncode)

    # Step 2: Run fix_compile_commands.py on the generated file
    compile_commands_path = os.path.join(workspace_dir, "compile_commands.json")
    if not os.path.exists(compile_commands_path):
        print(
            f"ERROR: {compile_commands_path} not found after refresh.",
            file=sys.stderr,
        )
        sys.exit(1)

    print("==> Fixing compile_commands.json for clangd compatibility...")
    # Import and run the fix inline to avoid needing a separate process
    fix_script = os.path.join(
        os.path.dirname(os.path.abspath(__file__)), "fix_compile_commands.py"
    )

    result = subprocess.run(
        [sys.executable, fix_script, compile_commands_path],
        cwd=workspace_dir,
    )
    if result.returncode != 0:
        print("ERROR: fix_compile_commands failed.", file=sys.stderr)
        sys.exit(result.returncode)

    print("==> Done! compile_commands.json is ready.")


if __name__ == "__main__":
    main()
