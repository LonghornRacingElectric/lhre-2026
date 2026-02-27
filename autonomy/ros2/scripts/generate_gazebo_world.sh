#!/usr/bin/env bash
set -e
cd "$(dirname "$0")/.."

source "$(dirname "$0")/_ros_env.sh"
source install/setup.bash || true

python3 src/lhr_gazebo/scripts/generate_world.py "$@"
