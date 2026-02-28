#!/usr/bin/env bash
set -e
cd "$(dirname "$0")/.."

source "$(dirname "$0")/_ros_env.sh"
source install/setup.bash || true

# Default world file: first .sdf in the worlds directory
WORLDS_DIR="src/lhr_gazebo/worlds"
if ! echo "$@" | grep -q "world:="; then
    WORLD_FILE="$(ls "$WORLDS_DIR"/*.sdf 2>/dev/null | head -1)"
    if [ -z "$WORLD_FILE" ]; then
        echo "ERROR: No world file found in $WORLDS_DIR" >&2
        echo "Run: ./scripts/generate_gazebo_world.sh --seed 1" >&2
        exit 1
    fi
    WORLD_ABS="$(cd "$(dirname "$WORLD_FILE")" && pwd)/$(basename "$WORLD_FILE")"
    echo "Using world: $WORLD_ABS"
    ros2 launch lhr_gazebo gazebo_demo.launch.py world:="$WORLD_ABS" "$@"
else
    ros2 launch lhr_gazebo gazebo_demo.launch.py "$@"
fi
