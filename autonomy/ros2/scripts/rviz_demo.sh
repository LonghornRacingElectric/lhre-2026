#!/usr/bin/env bash
set -e
cd "$(dirname "$0")/.."

source "$(dirname "$0")/_ros_env.sh"

rviz2 -d "$(pwd)/rviz/default.rviz"
