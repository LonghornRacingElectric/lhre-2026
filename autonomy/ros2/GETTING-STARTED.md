# Getting Started

Goal: fresh machine to a simulated car lapping in RViz, in about 30 minutes.

## Supported platform

**Ubuntu 24.04, native install only.** ROS 2 Jazzy + Gazebo Harmonic.

No WSL. We tried it, hit graphics and stability issues, and dropped it. No macOS, no other distros. If you only have a Windows machine, dual-boot or use a team machine.

## 1. Get the code

```bash
git clone https://github.com/LonghornRacingElectric/lhre-2026.git
cd lhre-2026/autonomy/ros2
```

## 2. Install ROS 2 Jazzy

```bash
# Prerequisites + ROS apt source
sudo apt update && sudo apt install -y software-properties-common curl
sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key \
  -o /usr/share/keyrings/ros-archive-keyring.gpg
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] \
  http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" \
  | sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null
sudo apt update

# ROS 2 + build tools
sudo apt install -y ros-jazzy-desktop ros-jazzy-ackermann-msgs ros-jazzy-tf2-ros \
  python3-colcon-common-extensions

# Gazebo Harmonic
sudo apt install -y ros-jazzy-ros-gz

# Optional: signal plotting
sudo apt install -y ros-jazzy-plotjuggler-ros
```

## 3. Build

```bash
./scripts/build.sh
```

## 4. Run the demo

```bash
# Terminal 1: full autonomy stack (kinematic sim)
./scripts/run_demo.sh

# Terminal 2: visualization
./scripts/rviz_demo.sh
```

## You are done when

RViz shows a cone track and the car drives itself around it and completes a lap. That is the whole pipeline working: track generation, simulated perception, path building, control, mission management.

If you see that, your setup is correct.

## Next steps

1. **Gazebo physics demo**, same upper stack but real physics and LiDAR perception:
   ```bash
   ./scripts/run_gazebo_demo.sh track_style:=oval perception:=lidar
   ```
2. **Read the [reference README](README.md)**: packages, data flow, topics, parameters.
3. **Pick a lane**: see the [lane map](../README.md) and talk to the autonomy lead.
