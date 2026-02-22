#!/usr/bin/env bash
# Shared ROS 2 environment setup — auto-detects installed distro.
# Source this from other scripts: source "$(dirname "$0")/_ros_env.sh"

_DISTROS="jazzy humble"

for _d in $_DISTROS; do
    if [ -f "/opt/ros/${_d}/setup.bash" ]; then
        source "/opt/ros/${_d}/setup.bash"
        return 0 2>/dev/null || exit 0
    fi
done

echo "ERROR: No supported ROS 2 distro found (checked: $_DISTROS)." >&2
echo "Install ROS 2 Humble (Ubuntu 22.04) or Jazzy (Ubuntu 24.04)." >&2
exit 1
