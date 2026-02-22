#!/usr/bin/env bash
set -e
cd "$(dirname "$0")/.."

source /opt/ros/humble/setup.bash

rviz2 -d "$(pwd)/rviz/default.rviz"
