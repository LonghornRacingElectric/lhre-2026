#!/usr/bin/env bash
set -e
cd "$(dirname "$0")/.."

source /opt/ros/humble/setup.bash
colcon build --symlink-install
echo "✅ Build complete"
