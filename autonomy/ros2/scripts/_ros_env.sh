#!/usr/bin/env bash
# Shared ROS 2 environment setup — auto-detects installed distro.
# Source this from other scripts: source "$(dirname "$0")/_ros_env.sh"

_DISTRO="jazzy"

if [ -f "/opt/ros/${_DISTRO}/setup.bash" ]; then
    source "/opt/ros/${_DISTRO}/setup.bash"
    return 0 2>/dev/null || exit 0
fi

echo "ERROR: ROS 2 Jazzy not found." >&2
echo "Install ROS 2 Jazzy (Ubuntu 24.04): sudo apt install ros-jazzy-desktop" >&2
exit 1
