#!/bin/sh
set -eu
repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/../../.." && pwd)
temp_dir=$(mktemp -d "${TMPDIR:-/tmp}/orion-tc-test.XXXXXX")
trap 'rm -rf "$temp_dir"' EXIT HUP INT TERM
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
  -I"$repo_root/VCU/model/inc" -I"$repo_root/VCU/model/components" \
  -I"$repo_root/VCU/model/util" \
  "$repo_root/VCU/model/tests/traction_control_test.c" \
  "$repo_root/VCU/model/components/TractionControl.c" \
  -lm -o "$temp_dir/traction_control_test"
"$temp_dir/traction_control_test"
