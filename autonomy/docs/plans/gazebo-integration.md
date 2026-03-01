# Gazebo Integration Plan

## Goal

Replace the kinematic vehicle simulator (`lhr_sim_kinematic`) with Gazebo physics simulation, while keeping the entire upper stack unchanged (control, track builder, mission manager, metrics).

## Current Status: Phase 1 Complete

Phase 1 (drop-in replacement) is functional. The Gazebo sim runs with direct joint control, ground-truth odometry, curvature-adaptive pure pursuit, and RViz integration. Cone collisions are temporarily disabled for tuning.

## Architecture: What Changes, What Stays

```
CURRENT                              WITH GAZEBO
───────                              ───────────
lhr_trackgen (procedural cones)  →   REUSED (same node, provides ground truth)
lhr_sensor_sim (FOV filter)      →   REUSED (same node, filters by vehicle pose)
lhr_sim_kinematic (bicycle model)→   Gazebo physics (direct joint control, ODE solver)

lhr_track_builder                →   STAYS (pairs cones by ID → centerline)
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

Sensors: IMU (100 Hz).

### 2. Cone Models + World Generation

Files: `lhr_gazebo/models/cone_{blue,yellow,orange_small,orange_large}/`
Script: `lhr_gazebo/scripts/generate_world.py`

- Cones placed procedurally from `lhr_trackgen` Catmull-Rom output
- Vehicle spawn auto-computed on straightest track section (avoids loop closure artifacts)
- ODE physics engine at 1 kHz, real-time factor 1.0
- Cone collisions temporarily disabled for control tuning

### 3. Cone Detection (Reused Pipeline)

Instead of using Gazebo's logical camera (not available on all platforms), the existing ROS nodes handle cone detection:
- `lhr_trackgen` publishes ground-truth cones
- `lhr_sensor_sim` filters by FOV and range relative to the Gazebo vehicle pose
- Both nodes run with `use_sim_time: True` to sync with Gazebo's clock

### 4. ROS2 ↔ Gazebo Bridge

File: `lhr_gazebo/config/ros_gz_bridge.yaml`

10 bridged topics:
- Odometry, TF, clock, IMU: Gazebo → ROS2
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

### Phase 2: Real Sensors (Future)

- Mount camera and/or LiDAR on the vehicle model
- Bridge image/pointcloud topics to ROS2
- Build a perception node (cone detection from camera or LiDAR)
- Replace trackgen + sensor_sim pipeline with real perception

### Phase 3: Fidelity Tuning (Future)

- Re-enable cone collisions and tune control to avoid them
- Tune tire friction, mass distribution to match real car
- Add sensor noise models
- Test at competition speeds
- Validate against real car telemetry data

## Gazebo Version Matrix

| Ubuntu | ROS2 Distro | Gazebo Version | Install Package |
|--------|-------------|----------------|-----------------|
| 22.04 | Humble | Fortress (gz-sim 6) | `ros-humble-ros-gz` |
| 24.04 | Jazzy | Harmonic (gz-sim 8) | `ros-jazzy-ros-gz` |

Note: `JointPositionController` with `use_velocity_commands` requires gz-sim 7+ (Garden or Harmonic). On Fortress, PID tuning would be needed instead.

## File Structure (current)

```
ros2/src/lhr_gazebo/
├── config/
│   ├── ros_gz_bridge.yaml          (10 topic bridges)
│   └── default.rviz                (RViz display config)
├── launch/
│   └── gazebo_demo.launch.py       (Gazebo + RViz + full autonomy stack)
├── lhr_gazebo/
│   ├── __init__.py
│   └── joint_cmd_adapter.py        (AckermannDriveStamped → 6x joint commands)
├── models/
│   ├── fsae_vehicle/
│   │   ├── model.config
│   │   └── model.sdf               (vehicle with 6 joint controllers + odom + IMU)
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
```

## Key Debugging Lessons

1. **Wheel-based odometry is unreliable** when the car gets stuck — wheels spin but position doesn't change. Always use world-frame odometry from `OdometryPublisher`.
2. **AckermannSteering plugin is too sluggish** for reactive control — it converts Twist to joint torques through the physics solver. Direct joint control (JointPositionController + JointController) gives near-instant response.
3. **Cone pairing by list index breaks** when sensor sim detects left/right cones in different orders. Pair by marker ID instead (left ID `i` ↔ right ID `i + 10000`).
4. **Vehicle spawn position matters** — spawning near the loop closure (where first/last cones don't meet cleanly) causes immediate collisions. Auto-compute spawn on the straightest track section.
5. **symlink-install doesn't always update** Python files. When in doubt: `rm -rf build/<pkg> install/<pkg>` then rebuild.
