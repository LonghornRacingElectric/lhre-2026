# Gazebo Integration Plan

## Goal

Replace the bottom three simulation nodes (`lhr_sim_kinematic`, `lhr_sensor_sim`, `lhr_trackgen`) with Gazebo physics simulation, while keeping the entire upper stack unchanged (control, track builder, mission manager, metrics).

## Why Gazebo

- Standard ROS2 physics sim with native integration (no bridge glue)
- Good enough physics (ODE/Bullet/DART) for FSAE speeds
- Sensor plugins for camera and LiDAR (needed for real perception testing)
- Lightweight enough for WSL2
- Isaac ROS packages (for Jetson deployment) consume standard ROS2 topics — they work with Gazebo or Isaac Sim
- Install: `sudo apt install ros-{humble,jazzy}-ros-gz`

## Architecture: What Changes, What Stays

```
CURRENT                              WITH GAZEBO
───────                              ───────────
lhr_trackgen (procedural cones)  →   Gazebo world file (cones placed in scene)
lhr_sensor_sim (FOV filter)      →   Gazebo sensor plugins (camera, LiDAR)
lhr_sim_kinematic (bicycle model)→   Gazebo physics (tire dynamics, collisions)

lhr_track_builder                →   STAYS (still pairs cones → centerline)
lhr_control                      →   STAYS (still sends Ackermann commands)
lhr_mission_manager              →   STAYS (still manages state)
lhr_metrics                      →   STAYS (still tracks CTE, laps)
```

## Pieces to Build

### 1. Vehicle Model (SDF)

An FSAE car model with:
- Ackermann steering geometry (two front wheels that turn, two rear driven)
- Mass, inertia, wheelbase matching real car (~1.6m wheelbase, ~250kg)
- Tire/friction properties (where the physics value comes from)
- Sensor mounts for cameras and/or LiDAR

Start from simple box/cylinder geometry — visual fidelity doesn't matter, physics fidelity does. Can adapt an open-source FSAE Gazebo model if one fits.

### 2. Cone Models + World File

- Small blue cone: 228mm x 228mm x 325mm (single white stripe)
- Small yellow cone: 228mm x 228mm x 325mm (single black stripe)
- Small orange cone: 228mm x 228mm x 325mm (single white stripe)
- Large orange cone: 285mm x 285mm x 505mm (dual white stripe)

Cone placement options:
- **A) Procedural (recommended):** Script that takes existing `lhr_trackgen` Catmull-Rom output and generates a Gazebo world file (`.sdf`). Reuses proven track generation, can test many layouts.
- **B) Manual:** Hand-place cones in the world file for specific test tracks.

Option A is preferred — keeps procedural track generation and parameterized testing.

### 3. Sensor Plugins

Gazebo built-in sensor plugins attached to the vehicle:

| Sensor | Gazebo Plugin | ROS2 Topic Output | Purpose |
|--------|--------------|-------------------|---------|
| Logical Camera | `gz::sim::sensors::LogicalCamera` | Bounding boxes (ground truth) | Bypass perception initially |
| Camera | `gz::sim::sensors::Camera` | `sensor_msgs/Image` | Cone detection via vision |
| LiDAR | `gz::sim::sensors::Lidar` | `sensor_msgs/PointCloud2` | Cone detection via point cloud |
| IMU | `gz::sim::sensors::Imu` | `sensor_msgs/Imu` | Orientation, acceleration |

**The logical camera is the key enabler for incremental integration.** It gives ground-truth cone positions (with color labels) in the sensor frame — effectively replacing `lhr_sensor_sim` without needing a real perception pipeline yet. Swap in real camera-based detection later.

### 4. ROS2 <-> Gazebo Bridge

The `ros_gz_bridge` package connects Gazebo topics to ROS2 topics:

```
Gazebo → ROS2:
  /model/vehicle/odometry  →  /lhr/vehicle/odom
  /logical_camera           →  (adapter node) → /lhr/sensor/cones_detected
  /camera/image             →  /lhr/camera/image_raw  (for future perception)

ROS2 → Gazebo:
  /lhr/vehicle/cmd          →  /model/vehicle/cmd_vel  (Ackermann commands)
```

### 5. Ackermann Controller

The `gz-sim-ackermann-steering-system` Gazebo plugin translates `AckermannDriveStamped` messages into wheel joint commands. Configure with wheelbase and track width.

## Phased Rollout

### Phase 1: Drop-in Replacement (minimum viable)

- Vehicle SDF model with Ackermann plugin
- Cone models in Gazebo world generated from trackgen script
- Logical camera for ground-truth cone detection (no perception needed)
- `ros_gz_bridge` config for odom + commands
- Small adapter node: logical camera output → `/lhr/sensor/cones_detected` MarkerArray
- Gazebo-specific launch file (keep old launch file working too)

**Result:** Same behavior as current stack but with Gazebo physics instead of kinematic bicycle model. Upper stack (control, track builder, mission manager, metrics) is untouched.

### Phase 2: Real Sensors

- Mount camera and/or LiDAR on the vehicle model
- Bridge image/pointcloud topics to ROS2
- Build a perception node (cone detection from camera or LiDAR)
- Replace logical camera with real perception pipeline

### Phase 3: Fidelity Tuning

- Tune tire friction, mass, suspension to match real car
- Add sensor noise models
- Test at competition speeds
- Validate against real car telemetry data

## Gazebo Version Matrix

| Ubuntu | ROS2 Distro | Gazebo Version | Install Package |
|--------|-------------|----------------|-----------------|
| 22.04 | Humble | Fortress (gz-sim 6) | `ros-humble-ros-gz` |
| 24.04 | Jazzy | Harmonic (gz-sim 8) | `ros-jazzy-ros-gz` |

## Effort Estimate (Phase 1)

| Piece | Effort | Notes |
|-------|--------|-------|
| Vehicle SDF + Ackermann plugin | Medium | Most time here — geometry, joints, inertia |
| Cone models + world generator script | Small | Simple meshes + Python script |
| `ros_gz_bridge` config | Small | YAML config file |
| Logical camera → cone topic adapter | Small-Medium | New ROS2 node |
| Launch file integration | Small | New launch file for Gazebo mode |

## File Structure (anticipated)

```
ros2/src/
├── lhr_gazebo/                      (new package)
│   ├── models/
│   │   ├── fsae_vehicle/            (vehicle SDF + meshes)
│   │   ├── cone_blue/               (cone SDF)
│   │   ├── cone_yellow/
│   │   ├── cone_orange_small/
│   │   └── cone_orange_large/
│   ├── worlds/
│   │   └── autocross.sdf            (generated or hand-made)
│   ├── launch/
│   │   └── gazebo_demo.launch.py
│   ├── config/
│   │   └── ros_gz_bridge.yaml
│   ├── scripts/
│   │   └── generate_world.py        (trackgen → Gazebo world)
│   └── lhr_gazebo/
│       ├── __init__.py
│       └── logical_camera_adapter.py (logical cam → /lhr/sensor/cones_detected)
├── lhr_control/                     (unchanged)
├── lhr_track_builder/               (unchanged)
├── lhr_mission_manager/             (unchanged)
├── lhr_metrics/                     (unchanged)
└── lhr_demo/                        (keep existing launch for non-Gazebo runs)
```

## Compatibility

- The existing `lhr_sim_kinematic` + `lhr_sensor_sim` + `lhr_trackgen` stack remains functional for quick algorithm testing without Gazebo overhead
- Gazebo mode is an alternative launch path, not a replacement
- Both paths produce the same ROS2 topic interface — upper stack nodes don't know the difference
