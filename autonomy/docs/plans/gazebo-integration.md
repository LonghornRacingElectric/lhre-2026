# Gazebo Integration Plan

## Goal

Replace the kinematic vehicle simulator (`lhr_sim_kinematic`) with a full Gazebo physics simulation with realistic sensor-based perception, targeting Ubuntu 24.04 / ROS 2 Jazzy / Gazebo Harmonic.

## Current Status

**Phase 1 (Gazebo physics):** Complete.
**Phase 2 (LiDAR perception):** Functional on the oval track. Unreliable on autocross tracks.
**Phase 3 (Camera fusion + tuning):** Not started. See [camera-fusion.md](camera-fusion.md) for detailed plan.

## What Works Today

- Gazebo physics sim with direct joint control (no AckermannSteering plugin)
- Sim perception mode (`perception:=sim`) — ground-truth cones, index pairing, fully reliable
- LiDAR perception mode (`perception:=lidar`) — pointcloud clustering, Delaunay boundary pairing
- Oval track (`track_style:=oval`) — LiDAR pipeline completes laps reliably
- `OpaqueFunction`-based launch file auto-resolves world SDF from `track_style` + `seed`
- RViz runs alongside Gazebo with centerline, cones, and odometry visualization

## Known Issues

1. **Cone duplication during swerves** — when the car turns sharply, odom-based sensor→map transform jitter places the same cone at slightly different positions, exceeding the 1.5m dedup radius. Duplicated cones corrupt the Delaunay triangulation and produce spurious centerline points.

2. **Centerline unreliable on tight corners** — the Delaunay boundary pairing produces valid midpoints, but the greedy nearest-neighbor chaining can misoreder them on sharp curves where midpoints cluster close together.

3. **No left/right cone classification** — the LiDAR has no color information. The Delaunay approach sidesteps this but is fundamentally less robust than knowing which side each cone is on. A camera would solve this.

4. **Backwards path wrapping** — the chain starts from the vehicle and goes forward, then wraps backwards through midpoints behind the vehicle. Cosmetic only (pure pursuit ignores the backwards portion) but messy in RViz.

5. **Cone collisions disabled** — cones are ghost objects. The car drives through them instead of being penalized.

---

## Remaining Work

### Phase 2b — LiDAR Tuning (no new sensors)

Goal: make LiDAR perception reliable enough to complete autocross laps.

#### 2b.1 — Improve cone dedup robustness

**File:** `lhr_perception/lidar_cone_detector.py`

The current dedup uses a fixed 1.5m radius with running-average position merging. During swerves, transform error can exceed this. Options:
- Gate new cone additions on vehicle angular velocity (skip detections during rapid yaw change)
- Use a larger dedup radius (up to ~1.8m, limited by same-side cone spacing)
- Weight running average by distance from sensor (closer = more accurate = higher weight)

#### 2b.2 — Improve centerline chaining

**File:** `lhr_track_builder/track_builder_node.py`

The greedy nearest-neighbor chain produces poor results when midpoints are clustered. Options:
- Add a maximum step distance to the chain (skip midpoints that are too far from the last chained point)
- Use angular continuity — prefer the next point that continues roughly in the same direction
- Only chain midpoints within a forward arc of the vehicle (ignore midpoints behind)

#### 2b.3 — Validate on autocross

Test with `track_style:=autocross perception:=lidar` and iterate on the above until the car completes laps.

### Phase 3 — Camera Fusion

Goal: add a camera sensor to see cone colors, enabling proper left/right classification and simpler pairing.

#### 3.1 — Add camera sensor to vehicle model

**File:** `lhr_gazebo/models/fsae_vehicle/model.sdf`

Add a forward-facing camera to the chassis link (near the LiDAR mount). Bridge the image topic via `ros_gz_bridge`.

#### 3.2 — Camera-based cone color classification

**File:** `lhr_perception/` (new node or extend `lidar_cone_detector`)

- Subscribe to camera image + LiDAR cones
- Project each cone's map position into the camera frame
- Sample the pixel color at the projected location
- Classify as blue (left) or yellow (right)
- Publish classified MarkerArray with `left_cones`/`right_cones` namespaces

This restores proper left/right classification, allowing the track builder to use simpler and more reliable pairing strategies (index or nearest-neighbor) instead of Delaunay.

#### 3.3 — Update track builder for fused perception

**File:** `lhr_track_builder/track_builder_node.py`

With camera-classified cones, add a `'fused'` pairing strategy (or reuse `'nearest'`) that pairs left/right cones with confidence. Fall back to `'boundary'` for unclassified cones.

### Phase 4 — Fidelity Tuning

Goal: make the simulation match the real car closely enough for control parameter transfer.

- Re-enable cone collisions and tune control to avoid them
- Tune tire friction, mass distribution, and steering dynamics to match real car
- Add sensor noise models (LiDAR range noise, camera exposure variation)
- Test at competition speeds (up to 15 m/s)
- Validate against real car telemetry data
- Add IMU-based state estimation (replace ground-truth odometry)

---

## Architecture

```
PHASE 1 (perception:=sim)            PHASE 2 (perception:=lidar)
─────────────────────────            ──────────────────────────
lhr_trackgen (ground-truth cones)    Gazebo gpu_lidar sensor
lhr_sensor_sim (FOV filter)          lhr_perception (pointcloud → unclassified cones)
Gazebo physics (joint control)       Gazebo physics (joint control)

lhr_track_builder (index pairing)    lhr_track_builder (Delaunay boundary pairing)
lhr_control                      →   STAYS (pure pursuit + curvature speed planning)
lhr_mission_manager              →   STAYS (FSAE state machine)
lhr_metrics                      →   STAYS (CTE, lap detection, CSV output)
```

Future Phase 3 adds a camera branch feeding into `lhr_perception` for color classification, enabling the track builder to switch back to left/right pairing.

## Implemented Components

### Vehicle Model (SDF)

File: `lhr_gazebo/models/fsae_vehicle/model.sdf`

- Ackermann steering geometry (front two wheels steer, rear two driven)
- Wheelbase 1.6 m, track width 1.2 m, wheel radius 0.2 m
- 232 kg total (200 kg chassis + 4x 8 kg wheels)
- Steering limits ±0.7 rad, 10 rad/s velocity limit
- Direct joint control (JointPositionController + JointController) — NOT AckermannSteering
- Sensors: IMU (100 Hz), GPU LiDAR (360x16 channels, 0.5–25 m, 10 Hz)

### Cone Models + World Generation

Script: `lhr_gazebo/scripts/generate_world.py`

- `--style` flag selects generator: `autocross` (Catmull-Rom spline), `oval` (arc-length ellipse), `simple` (wobble oval)
- `--seed`, `--num-waypoints`, `--radius`, `--jitter`, `--width`, `--cone-spacing` parameters
- Vehicle spawn auto-computed on straightest track section
- ODE physics at 1 kHz, real-time factor 1.0
- `gz-sim-sensors-system` with Ogre2 render engine
- Blue/yellow cone collisions disabled for tuning (Phase 4 will re-enable)
- Cone models use generated cone-shaped STL meshes (not box primitives)
- Worlds installed by colcon via `setup.py` `data_files`

### Launch File

File: `lhr_gazebo/launch/gazebo_demo.launch.py`

Uses `OpaqueFunction` for runtime resolution:
- `track_style` + `seed` → auto-resolves world SDF path (source tree or install dir)
- `perception` → selects sim or lidar node set
- `gui` → Gazebo GUI or headless
- All control/mission/metrics params exposed as launch args

### ROS2 ↔ Gazebo Bridge

File: `lhr_gazebo/config/ros_gz_bridge.yaml` — 11 bridged topics.

### Joint Command Adapter

File: `lhr_gazebo/lhr_gazebo/joint_cmd_adapter.py`

AckermannDriveStamped → 6 individual joint commands with proper Ackermann differential geometry.

## Gazebo Version Matrix

| Ubuntu | ROS 2 | Gazebo | Install |
|--------|-------|--------|---------|
| 24.04 | Jazzy | Harmonic (gz-sim 8) | `ros-jazzy-ros-gz` |

## File Structure

```
ros2/src/lhr_gazebo/
├── config/
│   ├── ros_gz_bridge.yaml          (11 topic bridges)
│   └── default.rviz
├── launch/
│   └── gazebo_demo.launch.py       (OpaqueFunction-based, auto-resolves world)
├── lhr_gazebo/
│   ├── __init__.py
│   └── joint_cmd_adapter.py
├── models/
│   ├── fsae_vehicle/
│   │   ├── model.sdf               (vehicle + IMU + LiDAR)
│   │   └── meshes/                  (carBody.stl, carTire.stl)
│   ├── cone_blue/
│   │   ├── model.sdf
│   │   └── meshes/cone.stl
│   ├── cone_yellow/                 (same structure)
│   ├── cone_orange_small/           (same structure)
│   └── cone_orange_large/           (same structure)
├── scripts/
│   └── generate_world.py           (--style autocross|oval|simple)
├── worlds/
│   ├── autocross_seed1.sdf
│   └── oval_seed1.sdf
├── package.xml
├── setup.py                        (installs worlds/*.sdf)
└── setup.cfg

ros2/src/lhr_perception/
├── lhr_perception/
│   ├── __init__.py
│   └── lidar_cone_detector.py      (PointCloud2 → unclassified MarkerArray)
├── package.xml
├── setup.py
└── setup.cfg
```

## Key Debugging Lessons

1. **AckermannSteering plugin is too sluggish** — direct joint control gives near-instant response.
2. **Gazebo Harmonic LiDAR topic** is `/lidar/points/points` (not `/lidar/points`) for PointCloud2.
3. **LiDAR self-detection** — 360° LiDAR hits the car body. Requires vehicle exclusion zone filter in sensor frame.
4. **Left/right classification by sensor-frame lateral position fails on curves** — both track boundaries appear on the same side of the sensor. Delaunay boundary pairing avoids this but camera fusion is the proper fix.
5. **Cone duplication during swerves** — odom-based transform jitter places the same cone at multiple positions. Larger dedup radius + running average helps but doesn't eliminate it.
6. **Greedy nearest-neighbor chaining** produces poor path ordering when midpoints cluster on tight curves.
7. **Vehicle spawn position matters** — spawn on the straightest section to avoid loop closure artifacts.
8. **`symlink-install` doesn't always update** Python files. When in doubt: `rm -rf build/<pkg> install/<pkg>`.
