#!/usr/bin/env python3
import importlib.util
import os
import sys


def _load_generate_can_proto_module():
    here = os.path.dirname(__file__)
    path = os.path.join(here, "generate_can_proto.py")
    spec = importlib.util.spec_from_file_location("generate_can_proto", path)
    if spec is None or spec.loader is None:
        raise ImportError(f"Could not load module spec from {path}")
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def main() -> int:
    workspace_dir = os.environ.get("BUILD_WORKSPACE_DIRECTORY")
    if not workspace_dir:
        print(
            "Error: BUILD_WORKSPACE_DIRECTORY is not set. Run via `bazel run //drivers/longhorn-lib:update_can_proto`.",
            file=sys.stderr,
        )
        return 2

    # In bazel runfiles, data files are available under the workspace-relative path.
    csv_path = os.path.join(workspace_dir, "drivers/longhorn-lib/config/can_packets.csv")
    bitfield_csv_path = os.path.join(
        workspace_dir, "drivers/longhorn-lib/config/can_bitfields.csv"
    )
    out_dir = os.path.join(workspace_dir, "drivers/longhorn-lib/protobuf")
    out_path = os.path.join(out_dir, "can_packets.proto")

    os.makedirs(out_dir, exist_ok=True)

    gen = _load_generate_can_proto_module()
    gen_json = gen._load_generate_can_json_module()
    bitfield_defs = gen_json.load_bitfield_definitions(bitfield_csv_path)
    packets = gen_json.process_csv(csv_path, bitfield_defs, bitfield_csv_path)
    partitions = gen.parse_can_model_to_partitions(packets)
    proto_text = gen.generate_proto_text(partitions, "Orion")

    with open(out_path, "w", encoding="utf-8") as f:
        f.write(proto_text)

    print(f"Wrote {out_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
