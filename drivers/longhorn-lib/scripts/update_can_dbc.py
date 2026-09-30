#!/usr/bin/env python3
import importlib.util
import os
import sys

def _load_generate_can_dbc_module():
    here = os.path.dirname(__file__)
    path = os.path.join(here, "generate_can_dbc.py")
    spec = importlib.util.spec_from_file_location("generate_can_dbc", path)
    if spec is None or spec.loader is None:
        raise ImportError(f"Could not load module spec from {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module

def main() -> int:
    workspace_dir = os.environ.get("BUILD_WORKSPACE_DIRECTORY")
    if not workspace_dir:
        print(
            "Error: BUILD_WORKSPACE_DIRECTORY is not set. Run via `bazel run //drivers/longhorn-lib:update_can_dbc`.",
            file=sys.stderr,
        )
        return 2

    csv_path = os.path.join(workspace_dir, "drivers/longhorn-lib/config/can_packets.csv")
    bitfield_csv_path = os.path.join(
        workspace_dir, "drivers/longhorn-lib/config/can_bitfields.csv"
    )
    out_path = os.path.join(workspace_dir, "drivers/longhorn-lib/can.dbc")

    gen = _load_generate_can_dbc_module()
    gen.generate_dbc(csv_path, bitfield_csv_path, out_path)

    print(f"Wrote {out_path}")
    return 0

if __name__ == "__main__":
    raise SystemExit(main())
