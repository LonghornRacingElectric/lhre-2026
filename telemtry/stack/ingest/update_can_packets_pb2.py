#!/usr/bin/env python3

import os
import shutil
import sys


def main() -> int:
    workspace_dir = os.environ.get("BUILD_WORKSPACE_DIRECTORY")
    if not workspace_dir:
        print(
            "Error: BUILD_WORKSPACE_DIRECTORY is not set. Run via `bazel run //telemtry/stack/ingest:update_can_packets_pb2`.",
            file=sys.stderr,
        )
        return 2

    # This file is produced by `bazel build //telemtry/stack/ingest:can_packets_pb2`.
    built_pb2 = os.path.join(
        workspace_dir,
        "bazel-bin/telemtry/stack/ingest/protobuf/can_packets_pb2.py",
    )

    # Write into the source tree for direct Python usage (non-Bazel runs).
    out_pb2 = os.path.join(
        workspace_dir,
        "telemtry/stack/ingest/protobuf/can_packets_pb2.py",
    )

    if not os.path.exists(built_pb2):
        print(
            f"Error: expected Bazel output not found at {built_pb2}. Try: bazel build //telemtry/stack/ingest:can_packets_pb2",
            file=sys.stderr,
        )
        return 3

    os.makedirs(os.path.dirname(out_pb2), exist_ok=True)
    shutil.copyfile(built_pb2, out_pb2)

    print(f"Wrote {out_pb2}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
