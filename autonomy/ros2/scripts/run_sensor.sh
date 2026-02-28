#!/usr/bin/env bash
set -e
cd "$(dirname "$0")/.."

source "$(dirname "$0")/_ros_env.sh"
source install/setup.bash || true

ros2 run lhr_sensor_sim sensor_sim "$@"
