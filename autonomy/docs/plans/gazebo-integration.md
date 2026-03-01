# Gazebo Integration Plan

## Goal

Replace the kinematic vehicle simulator (`lhr_sim_kinematic`) with Gazebo physics simulation, while keeping the entire upper stack unchanged (control, track builder, mission manager, metrics).

## Current Status: Phase 2 In Progress

Phase 1 (drop-in replacement) is complete. Phase 2 (LiDAR perception) is implemented and ready for testing on native Linux with Gazebo Harmonic.

## Architecture

```
PHASE 1 (perception:=sim)            PHASE 2 (perception:=lidar)
─────────────────────────            ──────────────────────────
lhr_trackgen (ground-truth cones)    Gazebo gpu_lidar sensor
lhr_sensor_sim (FOV filter)          lhr_perception (pointcloud → cones)
Gazebo physics (joint control)       Gazebo physics (joint control)

lhr_track_builder                →   STAYS (index or nearest-neighbor pairing)
lhr_control                      →   STAYS (pure pursuit + curvature speed planning)
lhr_mission_manager              →   STAYS (FSAE state machine)
lhr_metrics                      →   STAYS (CTE, lap detection, CSV output)
```

## Implemented Components

### 1. Vehicle Model (SDF)

File: `lhr_gazebo/models/fsae_vehicle/model.sdf`

FSAE car with simplified box/cylinder geometry:
- Ackermann steering geometry (front two wheels steer, rear two driven)
- Wheelbase 1.6 m, track width 1.2 m, wheel radius 0.2 m
- 200 kg chassis + 4x 8 kg wheels = 232 kg total
- Steering limits ±0.7 rad with 10 rad/s velocity limit
- Reference point: rear axle center at ground level

**Joint control (not AckermannSteering):** The built-in `AckermannSteering` plugin was too sluggish — it converts Twist to wheel torques through the physics solver, causing significant steering latency. After studying the EUFS sim (which teleports the car entirely), we replaced it with:
- 2x `JointPositionController` (steering, with `use_velocity_commands=true` for near-instant response)
- 4x `JointController` (wheel velocity, direct mode)

Sensors: IMU (100 Hz), GPU LiDAR (10 Hz, 360x16 channels, 25 m range).

### 2. Cone Models + World Generation

Files: `lhr_gazebo/models/cone_{blue,yellow,orange_small,orange_large}/`
Script: `lhr_gazebo/scripts/generate_world.py`

- Cones placed procedurally from `lhr_trackgen` Catmull-Rom output
- Vehicle spawn auto-computed on straightest track section (avoids loop closure artifacts)
- ODE physics engine at 1 kHz, real-time factor 1.0
- `gz-sim-sensors-system` with Ogre2 render engine (required for gpu_lidar)
- Cone collisions temporarily disabled for control tuning

### 3. Cone Detection

Two perception modes selectable via `perception:=sim|lidar` launch argument:

**Sim mode (default):** Existing ROS nodes handle cone detection:
- `lhr_trackgen` publishes ground-truth cones
- `lhr_sensor_sim` filters by FOV and range relative to the Gazebo vehicle pose
- Track builder uses index-based pairing (left ID `i` ↔ right ID `10000 + i`)

**LiDAR mode:** Real sensor-based detection:
- `gpu_lidar` sensor on vehicle publishes PointCloud2 via `ros_gz_bridge`
- `lhr_perception/lidar_cone_detector` processes the pointcloud:
  - Ground removal (height threshold) → range filter → Euclidean clustering (cKDTree)
  - Cone validation by size → sensor-to-map transform → spatial dedup
  - Left/right classification by lateral position in sensor frame
  - Accumulated persistent map, sorted by angle for consistent pairing
- Track builder uses nearest-neighbor pairing (robust to detection ordering)

### 4. ROS2 ↔ Gazebo Bridge

File: `lhr_gazebo/config/ros_gz_bridge.yaml`

11 bridged topics:
- Odometry, TF, clock, IMU, LiDAR pointcloud: Gazebo → ROS2
- 2x steering position + 4x wheel velocity: ROS2 → Gazebo (Float64 ↔ gz.msgs.Double)

### 5. Joint Command Adapter

File: `lhr_gazebo/lhr_gazebo/joint_cmd_adapter.py`

Converts `AckermannDriveStamped` → 6 individual joint commands:
- Proper Ackermann differential geometry (inner/outer wheel angles)
- Per-wheel velocity accounting for turn radius differentials

### 6. RViz Integration

RViz launches automatically alongside Gazebo (configurable via `rviz:=true/false`). The pre-configured display shows centerline, cones, odometry, lookahead point, and FOV.

## Phased Rollout

### Phase 1: Drop-in Replacement — COMPLETE

- Vehicle SDF model with direct joint control
- Cone models in Gazebo world generated from trackgen script
- Existing trackgen + sensor_sim reused for cone detection
- `ros_gz_bridge` config for odom + joint commands
- Joint command adapter (Ackermann geometry)
- Ground-truth odometry from OdometryPublisher
- Curvature-adaptive lookahead in pursuit controller
- RViz integration in Gazebo launch
- Gazebo-specific launch file (kinematic launch still works independently)

### Phase 2: LiDAR Perception — IMPLEMENTED, TESTING

- GPU LiDAR sensor mounted on vehicle model (360x16 channels, 25 m range, 10 Hz)
- `gz-sim-sensors-system` with Ogre2 re-added to world generator
- PointCloud2 bridge from Gazebo to ROS2
- New `lhr_perception` package with `lidar_cone_detector` node
- Nearest-neighbor pairing strategy added to track builder
- Perception mode switch in launch file (`perception:=sim|lidar`)

### Phase 3: Fidelity Tuning (Future)

- Re-enable cone collisions and tune control to avoid them
- Tune tire friction, mass distribution to match real car
- Add sensor noise models
- Test at competition speeds
- Validate against real car telemetry data
- Add camera sensor and color-based cone classification

## Gazebo Version Matrix

| Ubuntu | ROS2 Distro | Gazebo Version | Install Package |
|--------|-------------|----------------|-----------------|
| 24.04 | Jazzy | Harmonic (gz-sim 8) | `ros-jazzy-ros-gz` |

## File Structure

```
ros2/src/lhr_gazebo/
├── config/
│   ├── ros_gz_bridge.yaml          (11 topic bridges)
│   └── default.rviz                (RViz display config)
├── launch/
│   └── gazebo_demo.launch.py       (Gazebo + perception + full autonomy stack)
├── lhr_gazebo/
│   ├── __init__.py
│   └── joint_cmd_adapter.py        (AckermannDriveStamped → 6x joint commands)
├── models/
│   ├── fsae_vehicle/
│   │   ├── model.config
│   │   └── model.sdf               (vehicle with joint controllers + odom + IMU + LiDAR)
│   ├── cone_blue/
│   ├── cone_yellow/
│   ├── cone_orange_small/
│   └── cone_orange_large/
├── scripts/
│   └── generate_world.py           (trackgen → Gazebo world SDF)
├── worlds/
│   └── autocross_seed1.sdf         (generated world file)
├── package.xml
├── setup.py
└── setup.cfg

ros2/src/lhr_perception/
├── lhr_perception/
│   ├── __init__.py
│   └── lidar_cone_detector.py      (PointCloud2 → MarkerArray cone detection)
├── package.xml
├── setup.py
└── setup.cfg
```

## Key Debugging Lessons

1. **Wheel-based odometry is unreliable** when the car gets stuck — wheels spin but position doesn't change. Always use world-frame odometry from `OdometryPublisher`.
2. **AckermannSteering plugin is too sluggish** for reactive control — it converts Twist to joint torques through the physics solver. Direct joint control (JointPositionController + JointController) gives near-instant response.
3. **Cone pairing by list index breaks** when sensor sim detects left/right cones in different orders. Pair by marker ID instead (left ID `i` ↔ right ID `i + 10000`), or use nearest-neighbor pairing for LiDAR perception.
4. **Vehicle spawn position matters** — spawning near the loop closure (where first/last cones don't meet cleanly) causes immediate collisions. Auto-compute spawn on the straightest track section.
5. **symlink-install doesn't always update** Python files. When in doubt: `rm -rf build/<pkg> install/<pkg>` then rebuild.
