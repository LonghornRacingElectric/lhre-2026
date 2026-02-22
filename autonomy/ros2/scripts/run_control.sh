#!/usr/bin/env bash
set -e
cd "$(dirname "$0")/.."

source /opt/ros/humble/setup.bash
source install/setup.bash || true

ros2 run lhr_control pursuit_node
