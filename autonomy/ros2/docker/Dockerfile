# Dev container for the autonomy stack on non-Ubuntu hosts (macOS, Windows).
# Base: Ubuntu 24.04 + ROS 2 Jazzy desktop + a lightweight desktop served over
# noVNC (browser at http://localhost:6080). Works on arm64 (Apple Silicon) and amd64.
FROM tiryoh/ros2-desktop-vnc:jazzy

RUN apt-get update && apt-get install -y --no-install-recommends \
    ros-jazzy-ros-gz \
    ros-jazzy-ackermann-msgs \
    ros-jazzy-tf2-ros \
    ros-jazzy-plotjuggler-ros \
    python3-colcon-common-extensions \
    python3-numpy \
    python3-scipy \
    && rm -rf /var/lib/apt/lists/*

# The base image ships a pip numpy 2.x in /usr/local that shadows apt's numpy,
# breaking apt's scipy (built against numpy 1.x). Shadow scipy the same way so
# the pair is ABI-consistent.
RUN pip install --no-cache-dir --break-system-packages --ignore-installed "scipy>=1.14"
