# Gazebo Integration — Change Notes

Chronological log of changes made while bringing up the Gazebo simulation backend.

## 2026-02-27 — Initial WSL debugging

Changes made while debugging Gazebo launch failures on WSL (ROS Humble / Ignition Fortress).

### Bridge config (`lhr_gazebo/config/ros_gz_bridge.yaml`)
- Removed `LogicalCameraImage` mapping — `ros_gz_interfaces` on ROS Humble does not include this message type. Both the bridge and the adapter node crashed on startup.

### World SDF + generator (`lhr_gazebo/scripts/generate_world.py`)
- Removed `gz-sim-sensors-system` plugin (requires Ogre2 rendering engine). No longer needed since we are not using the Gazebo logical camera. This plugin caused an immediate crash on WSL due to missing GL3+ support in Mesa's D3D12 backend.

### Launch file (`lhr_gazebo/launch/gazebo_demo.launch.py`)
- **Replaced `logical_camera_adapter`** with existing `publish_cones` (lhr_trackgen) + `sensor_sim` (lhr_sensor_sim) nodes. Cone detection now reuses the working MVS pipeline instead of relying on Gazebo sensors. Gazebo's role is reduced to physics + odometry only.
- **Added `use_sim_time: True`** to all ROS nodes so they use the Gazebo `/clock` topic instead of wall time.
- **Auto-detects Gazebo CLI**: checks for `ign` (Fortress) vs `gz` (Garden/Harmonic) on PATH.
- **Headless mode**: `gui:=false` passes `-s` flag for server-only mode (no rendering).
- **Environment variables**: sets both `IGN_GAZEBO_RESOURCE_PATH` and `GZ_SIM_RESOURCE_PATH` for model discovery, plus `MESA_D3D12_DEFAULT_ADAPTER_NAME=NVIDIA` for WSL GPU selection.

---

## 2026-02-28 — Vehicle spawn, cone pairing, odometry, steering

Major debugging session to get the car driving correctly on track.

### Vehicle spawn position (`lhr_gazebo/scripts/generate_world.py`)
- **Problem:** Default spawn position (25, 0) was off-track. Car spawned on top of cones.
- **Fix 1:** Auto-compute spawn from first cone pair midpoint — too close to cones.
- **Fix 2:** Offset 3m behind first pair — still clipping cones due to messy loop closure (first/last cone pairs don't meet cleanly).
- **Final fix:** Added `_compute_spawn()` that scores every track section by angular deviation over a 5-point window. Picks the straightest section, avoids a margin around the loop closure area (first/last `max(8, n//10)` pairs). Heading computed from centerline direction at that point.

### Cone pairing bug (`lhr_track_builder/track_builder_node.py`)
- **Problem:** Track builder paired left/right cones by list index. Sensor sim detects cones in different orders (left might start at marker ID 63, right at ID 64), causing misaligned pairs and a skewed centerline.
- **Fix:** Changed `_left_cones` and `_right_cones` from `List` to `dict` keyed by marker ID. Pairing logic now matches left cone ID `i` with right cone ID `i + 10000` (trackgen assigns left IDs as `0..N-1`, right IDs as `10000..10000+N-1`).

### Odometry source (`lhr_gazebo/models/fsae_vehicle/model.sdf`)
- **Root cause of "car stuck but thinks it's driving":** The `AckermannSteering` plugin's built-in odometry uses wheel encoder integration, starting at `(0, 0, 0)`. When wheels spin against an obstacle, odometry drifts even though the car is stationary. The autonomy stack thought it was driving and even logged "lap completed" while physically stuck on a cone.
- **Fix:** Added `gz-sim-odometry-publisher-system` plugin for ground-truth world-frame odometry. Changed AckermannSteering's odom output to a dead-end topic (`/model/fsae_vehicle/wheel_odom`). Bridge now reads from the OdometryPublisher's `/model/fsae_vehicle/odometry` topic.

### Steering joint tuning (`lhr_gazebo/models/fsae_vehicle/model.sdf`)
- Reduced steering knuckle mass from 0.5 → 0.1 kg
- Reduced steering knuckle inertia from 0.001 → 0.0001
- Added velocity limit of 10.0 rad/s to steering joints

### Cone collisions disabled (temporarily)
- Commented out `<collision>` geometry in `cone_blue/model.sdf` and `cone_yellow/model.sdf` to allow tuning without the car getting stuck on cones.

### Control tuning (`lhr_gazebo/launch/gazebo_demo.launch.py`)
- Set `lookahead_dist=5.0`, `v_max=5.0`, `v_min=1.0`, `a_lat_max=3.0`, `max_accel=1.0`, `max_decel=2.0`
- Increased `max_steer` from 0.45 → 0.55 rad in pursuit node defaults

### Build cache issue
- Stale `build/` and `install/` directories from `--symlink-install` didn't pick up Python changes in some cases. Fix: `rm -rf build/lhr_track_builder install/lhr_track_builder` then rebuild.

---

## 2026-02-28 — EUFS sim comparison and direct joint control

### EUFS sim analysis
Cloned [QUT-Motorsport/eufs_sim](https://github.com/QUT-Motorsport/eufs_sim) to compare vehicle models. Key findings:

- **EUFS does NOT use Gazebo's built-in AckermannSteering at all.** Their custom Gazebo Classic plugin (`libgazebo_ros_race_car.so`) runs its own dynamic bicycle model with Pacejka tire forces, then **teleports the car** each frame via `_model->SetWorldPose()`.
- Steering joints are set directly via `SetPosition(0, angle)` — no physics solver involved.
- Steering is rate-limited in software, not by joint physics.
- Their plugin is for Gazebo Classic (not compatible with Gazebo Sim / gz-sim).
- Other differences: chassis mass 140 kg (vs our 200 kg), steering joint velocity 1,000,000 (vs our 10), wheel friction mu=2 (vs our 1.0).

### Replace AckermannSteering with direct joint controllers (`model.sdf`)
- **Removed** the `gz-sim-ackermann-steering-system` plugin entirely.
- **Added 2x `JointPositionController`** for steering joints with `use_velocity_commands=true` and `cmd_max=10.0`. This bypasses PID and sets joint velocity directly to reach the target position — effectively instant steering response.
- **Added 4x `JointController`** for wheel joints in velocity mode (no PID, no force commands). Each wheel gets its own topic.

### New joint command adapter (`lhr_gazebo/lhr_gazebo/joint_cmd_adapter.py`)
- Replaces `ackermann_cmd_adapter.py` (which converted to Twist for AckermannSteering).
- Subscribes to `/lhr/vehicle/cmd` (AckermannDriveStamped).
- Computes **proper Ackermann differential steering angles** — inner wheel turns more than outer wheel.
- Computes **per-wheel angular velocities** accounting for different turn radii at each wheel.
- Publishes 6x `Float64` messages to individual joint topics.
- Steering clamped to ±0.69 rad (slightly inside ±0.7 joint limit to avoid edge-case instability).

### Bridge config update (`ros_gz_bridge.yaml`)
- **Removed** single Twist bridge for `/model/fsae_vehicle/cmd_vel`.
- **Added** 6 individual bridges: 2x steering position (`Float64` ↔ `gz.msgs.Double`, ROS_TO_GZ) + 4x wheel velocity (`Float64` ↔ `gz.msgs.Double`, ROS_TO_GZ).

### Launch file update (`gazebo_demo.launch.py`)
- Changed adapter node from `ackermann_cmd_adapter` to `joint_cmd_adapter`.
- Added `joint_cmd_adapter` entry point to `setup.py` (old entry point kept for rollback).

### Curvature-adaptive lookahead (`lhr_control/pursuit_node.py`)
- **Problem:** Pure pursuit with fixed lookahead cuts inside on tight turns — the pursuit point is already around the bend, so the car drives straight toward it.
- **Fix:** Lookahead distance now adapts to local curvature: `ld = ld_max - gain * |curvature|`, clamped to `[ld_min, ld_max]`. On straights (curvature ~0) it stays at 5m for stability. On tight hairpins it shortens to 2m to stay on the actual path.
- New parameters: `lookahead_min` (default 2.0), `lookahead_curvature_gain` (default 3.0).
- Refactored `_find_closest_idx()` out of `_find_lookahead()` for reuse by the curvature estimation.

### RViz integration (`gazebo_demo.launch.py`)
- **Added RViz2 node** to the Gazebo launch file — launches automatically with the pre-configured `default.rviz` config.
- Config shows: centerline path, visible cones, odometry trail, lookahead point, FOV visualization.
- Controlled by `rviz:=true/false` launch argument (default: true).
- RViz config file copied to `lhr_gazebo/config/default.rviz` so it's installed alongside the bridge YAML and accessible from the installed share directory.

---

## 2026-03-04 — Mesh models and LiDAR tuning

### Vehicle mesh model (`lhr_gazebo/models/fsae_vehicle/`)
- Replaced box chassis and cylinder wheel visuals with STL meshes (`meshes/carBody.stl`, `meshes/carTire.stl`) sourced from the telemetry web viewer.
- Collision geometry remains as primitives (box/cylinder) for physics performance.
- `setup.py` updated to install `meshes/` subdirectories.

### Cone mesh models (`lhr_gazebo/models/cone_*/`)
- Replaced box visuals with generated cone-shaped STL meshes (24-segment, proper FSAE dimensions).
- Each cone model now has a `meshes/` subdirectory with its STL file.
- Small cones: 0.228m base diameter, 0.325m tall. Large cones: 0.285m base, 0.505m tall.

### LiDAR cone detector tuning (`lhr_perception/lidar_cone_detector.py`)
- `cluster_radius`: 0.35 → 0.5m — wider radius to capture sparse returns from tapered cone geometry.
- `min_cluster_points`: 2 → 1 — allows single-point detections, since cones produce far fewer LiDAR returns than flat-faced boxes.

### Launch script fix (`scripts/run_gazebo_demo.sh`)
- Fixed `track_style:=` passthrough — the script was always auto-selecting the first world file alphabetically, ignoring the `track_style` argument. Now defers to the launch file when `track_style:=` is provided.
