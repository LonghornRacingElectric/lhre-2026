# Gazebo Integration — Change Notes (2026-02-27)

Changes made while debugging Gazebo launch failures on WSL (ROS Humble / Ignition Fortress).

## Bridge config (`lhr_gazebo/config/ros_gz_bridge.yaml`)
- Removed `LogicalCameraImage` mapping — `ros_gz_interfaces` on ROS Humble does not include this message type. Both the bridge and the adapter node crashed on startup.

## World SDF (`lhr_gazebo/worlds/autocross_seed1.sdf`) + generator (`lhr_gazebo/scripts/generate_world.py`)
- Removed `gz-sim-sensors-system` plugin (requires ogre2 rendering engine). No longer needed since we are not using the Gazebo logical camera. This plugin caused an immediate crash on WSL due to missing GL3+ support in Mesa's D3D12 backend.

## Launch file (`lhr_gazebo/launch/gazebo_demo.launch.py`)
- **Replaced `logical_camera_adapter`** with existing `publish_cones` (lhr_trackgen) + `sensor_sim` (lhr_sensor_sim) nodes. Cone detection now reuses the working MVS pipeline instead of relying on Gazebo sensors. Gazebo's role is reduced to physics + odometry only.
- **Added `use_sim_time: True`** to all ROS nodes so they use the Gazebo `/clock` topic instead of wall time.
- **Auto-detects Gazebo CLI**: checks for `ign` (Fortress) vs `gz` (Garden/Harmonic) on PATH.
- **Headless mode**: `gui:=false` passes `-s` flag for server-only mode (no rendering).
- **Environment variables**: sets both `IGN_GAZEBO_RESOURCE_PATH` and `GZ_SIM_RESOURCE_PATH` for model discovery, plus `MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA` for WSL GPU selection.

## Still needs testing on native Linux
- End-to-end autonomy loop: does the vehicle actually move?
- Topic namespacing: verify Gazebo's actual topic names match the bridge config (e.g. `/model/fsae_vehicle/odometry` vs `/world/fsae_world/model/fsae_vehicle/odometry`)
- Confirm odom → sensor sim → track builder → control → ackermann adapter → Gazebo round-trip works
