#!/usr/bin/env bash
set -e
cd "$(dirname "$0")/.."

source "$(dirname "$0")/_ros_env.sh"
source install/setup.bash || true

# setsid avoids WSLg focus conflicts with other Qt apps (e.g. RViz)
setsid ros2 run plotjuggler plotjuggler "$@"
