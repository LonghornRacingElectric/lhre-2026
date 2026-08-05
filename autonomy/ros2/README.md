# Autonomy – ROS 2 Workspace

Colcon workspace for LHR driverless / autonomy nodes. This file is the **reference**: packages, data flow, topics, parameters.

**New here?** Follow [GETTING-STARTED.md](GETTING-STARTED.md) first.

**Supported platform:** Ubuntu 24.04 (native only, no WSL) + **ROS 2 Jazzy**

## Packages

| Package | Description |
|---------|-------------|
| `lhr_trackgen` | Publishes a synthetic cone track (`/lhr/track/cones`) and cone IDs (left: 0..N-1, right: 10000..10000+N-1) |
| `lhr_sensor_sim` | FOV-limited sensor simulation — filters cones by vehicle pose, accumulates detections |
| `lhr_track_builder` | Subscribes to cones, pairs left/right by ID, publishes centerline path (`/lhr/track/centerline`) |
| `lhr_sim_kinematic` | Kinematic bicycle-model vehicle simulator (lightweight, no Gazebo needed) |
| `lhr_control` | Pure pursuit path-following controller with curvature-adaptive lookahead and speed planning |
| `lhr_mission_manager` | FSAE driverless state machine (Off → Ready → Driving → Finished → Emergency) |
| `lhr_metrics` | Cross-track error, off-track count, and lap detection (CSV output) |
| `lhr_gazebo` | Gazebo Harmonic physics simulation — vehicle with direct joint control, ground-truth odometry, LiDAR sensor, RViz integration |
| `lhr_perception` | LiDAR-based cone detection — pointcloud clustering, persistent mapping (unclassified cones, no left/right split). Functional on oval track; path quality needs tuning on complex tracks. |
| `lhr_demo` | Launch file that starts the full kinematic stack in one command |

## Data flow

### Kinematic sim (run_demo.sh)

```
trackgen ──→ /lhr/track/cones ──→ sensor_sim ──→ /lhr/sensor/cones_detected ──→ track_builder ──→ centerline
                (ground truth,        │                (FOV-filtered,                                    │
                 visible in RViz)      │                 accumulated)                                     ▼
                                       ├──→ /lhr/sensor/cones_viz    (bright/dim visualization)     pure_pursuit ◄── mission_manager
                                       └──→ /lhr/sensor/fov_viz      (FOV frustum)                      │          (/lhr/mission/status
                                                                                                         │           gates control)
                                       sim_kinematic ◄── /lhr/vehicle/cmd ◄──────────────────────────────┘
                                            │
                                            ├──→ /lhr/vehicle/odom ──→ mission_manager
                                            │                               │
                                            │                               ├──→ /lhr/mission/status
                                            │                               └──→ /lhr/debug/mission_state
                                            │
                                            └──→ metrics_node ──→ /lhr/metrics/lap_complete ──→ mission_manager
```

### Gazebo sim (run_gazebo_demo.sh)

**Sim perception (default, `perception:=sim`):**
```
                   ┌──────────────────── GAZEBO ────────────────────┐
                   │  Vehicle (fsae_vehicle) with:                  │
                   │   • JointPositionController (steering joints)  │
                   │   • JointController (wheel velocities)         │
                   │   • OdometryPublisher (ground-truth pose)      │
                   │   • IMU sensor, GPU LiDAR sensor               │
                   └─────────────┬───────────────────┬──────────────┘
                                 │                   │
                    ros_gz_bridge│(6x Float64 + odom)│
                                 │                   │
  trackgen ──→ sensor_sim ──→ track_builder ──→ pure_pursuit ──→ joint_cmd_adapter
                                                     │               │
                                                     │          6x joint commands
                                                     │          (2 steering pos +
                                                     │           4 wheel vel)
                                                     │
                                          /lhr/vehicle/odom ◄── Gazebo OdometryPublisher
```

**LiDAR perception (`perception:=lidar`):**
```
                   ┌──────────────────── GAZEBO ────────────────────┐
                   │  Vehicle with GPU LiDAR sensor                 │
                   └──────┬──────────────────┬──────────────────────┘
                          │                  │
             ros_gz_bridge│(PointCloud2+odom)│
                          │                  │
  lidar_cone_detector ──→ track_builder ──→ pure_pursuit ──→ joint_cmd_adapter
  (pointcloud clustering,  (boundary pairing via
   unclassified cones,      Delaunay triangulation)
   persistent mapping)
```

The upper stack (track_builder, control, mission_manager, metrics) is identical in both modes. The `perception` launch argument selects between the sim pipeline and LiDAR-based detection.

To bypass the sensor sim and use all cones directly (god-mode), override the cone topic:
```bash
ros2 run lhr_track_builder track_builder --ros-args -p cone_topic:=/lhr/track/cones
```

## Prerequisites

Machine setup lives in [GETTING-STARTED.md](GETTING-STARTED.md): Ubuntu 24.04 native + ROS 2 Jazzy + Gazebo Harmonic. No WSL.

## Quick start

```bash
# Build once
./scripts/build.sh

# Terminal 1 – start the full stack
./scripts/run_demo.sh

# Terminal 2 – open RViz (pre-configured displays + fixed frame = map)
./scripts/rviz_demo.sh

# Terminal 3 (optional) – open PlotJuggler for debug signals
./scripts/run_plotjuggler.sh
```

In PlotJuggler: click **Streaming** → **ROS2 Topic Subscriber** → **Start**, select topics, then drag them onto the plot area. Useful topics: `/lhr/debug/curvature`, `/lhr/debug/v_cmd`, `/lhr/debug/mission_state`, `/lhr/vehicle/cmd`.

Launch arguments can be passed through `run_demo.sh`:

```bash
./scripts/run_demo.sh lookahead_dist:=6.0 seed:=42

# Disable metrics collection:
./scripts/run_demo.sh enable_metrics:=false

# Manual go signal (don't auto-start driving):
./scripts/run_demo.sh auto_go:=false
# Then in another terminal:
ros2 topic pub --once /lhr/mission/go std_msgs/msg/Bool "{data: true}"
```

## Scripts reference

All scripts live in `scripts/` and should be run from the `autonomy/ros2` directory.

| Script | Description |
|--------|-------------|
| `build.sh` | Builds all packages with `colcon build --symlink-install`. Run after any code change. |
| `run_demo.sh` | Launches the full kinematic stack (cones + centerline + sim + control + mission manager + metrics) via `ros2 launch`. Accepts launch args, e.g. `./scripts/run_demo.sh seed:=42`. |
| `rviz_demo.sh` | Opens RViz with the pre-configured `rviz/default.rviz` config (all displays + fixed frame already set). |
| `run_cones.sh` | Runs only the cone publisher (`lhr_trackgen`). |
| `run_centerline.sh` | Runs only the centerline builder (`lhr_track_builder`). |
| `run_sim.sh` | Runs only the kinematic vehicle simulator (`lhr_sim_kinematic`). |
| `run_control.sh` | Runs only the pure pursuit controller (`lhr_control`). |
| `run_sensor.sh` | Runs only the sensor simulation (`lhr_sensor_sim`). |
| `run_metrics.sh` | Runs only the metrics node (`lhr_metrics`). Prints summary on Ctrl+C and appends to `data/metrics.csv`. |
| `run_plotjuggler.sh` | Opens PlotJuggler for plotting debug signals (curvature, speed, steering). |
| `generate_gazebo_world.sh` | Generates a Gazebo world SDF from the track generator. Accepts `--seed`, `--style`, `--num-waypoints`, etc. |
| `run_gazebo_demo.sh` | Launches the Gazebo-based stack (physics sim + adapters + upper stack). Accepts same args as `run_demo.sh`. |

The individual `run_*.sh` scripts are useful for debugging a single node. For normal use, prefer the two-terminal workflow (`run_demo.sh` + `rviz_demo.sh`).

## Gazebo simulation

The `lhr_gazebo` package provides an alternative simulation backend using Gazebo physics. It replaces the kinematic bicycle model (`lhr_sim_kinematic`) with a full physics vehicle in Gazebo while the cone detection pipeline and upper stack remain unchanged.

### Setup

1. Install Gazebo (see prerequisites above)
2. Build: `./scripts/build.sh`
3. Generate a world file from the track generator:
   ```bash
   ./scripts/generate_gazebo_world.sh --seed 1
   ```

### Running

```bash
# Single command — launches Gazebo + RViz + full autonomy stack
./scripts/run_gazebo_demo.sh
```

RViz launches automatically alongside Gazebo (disable with `rviz:=false`). The RViz config shows centerline, cones, odometry trail, lookahead point, and FOV visualization.

Launch arguments work the same way:
```bash
./scripts/run_gazebo_demo.sh seed:=42 lookahead_dist:=6.0
./scripts/run_gazebo_demo.sh gui:=false          # headless Gazebo (no Gazebo GUI)
./scripts/run_gazebo_demo.sh rviz:=false         # disable RViz
./scripts/run_gazebo_demo.sh perception:=lidar   # LiDAR-based cone detection
./scripts/run_gazebo_demo.sh track_style:=oval   # oval track (also: autocross, simple)
./scripts/run_gazebo_demo.sh track_style:=oval perception:=lidar  # LiDAR on oval (best LiDAR experience)
```

The `track_style` argument selects the track generator (`oval`, `autocross`, or `simple`). The world SDF is auto-resolved from `track_style` + `seed` (e.g. `oval_seed1.sdf`). Pre-generated worlds are installed by colcon from `worlds/*.sdf`.

**Launch file architecture:** `gazebo_demo.launch.py` uses `OpaqueFunction` instead of `IfCondition`/`UnlessCondition` for perception mode and world selection. All launch args are declared in `generate_launch_description()`, and nodes are built in the `_launch_setup()` callback which runs at launch time with access to resolved argument values.

### Vehicle model

The FSAE vehicle (`models/fsae_vehicle/model.sdf`) uses STL meshes (`meshes/carBody.stl`, `meshes/carTire.stl`) for visuals with simplified collision geometry:

| Parameter | Value |
|-----------|-------|
| Wheelbase | 1.6 m |
| Track width | 1.2 m |
| Wheel radius | 0.2 m |
| Chassis mass | 200 kg |
| Wheel mass | 8 kg each |
| Steering limits | ±0.7 rad (~40 deg) |
| Reference point | Rear axle center at ground level |

### Joint control architecture

The vehicle does **not** use Gazebo's built-in `AckermannSteering` plugin (which is too sluggish due to physics solver latency). Instead, it uses direct joint controllers for near-instant response:

| Joint | Plugin | Control Mode | Topic |
|-------|--------|-------------|-------|
| `front_left_steering_joint` | `JointPositionController` | Position (velocity commands, no PID) | `.../cmd_pos` |
| `front_right_steering_joint` | `JointPositionController` | Position (velocity commands, no PID) | `.../cmd_pos` |
| `front_left_wheel_joint` | `JointController` | Velocity (direct) | `.../cmd_vel` |
| `front_right_wheel_joint` | `JointController` | Velocity (direct) | `.../cmd_vel` |
| `rear_left_wheel_joint` | `JointController` | Velocity (direct) | `.../cmd_vel` |
| `rear_right_wheel_joint` | `JointController` | Velocity (direct) | `.../cmd_vel` |

The `joint_cmd_adapter` ROS2 node converts `AckermannDriveStamped` commands into 6 individual joint commands:
- **Steering angles** use proper Ackermann geometry (inner wheel turns more than outer)
- **Wheel velocities** account for differential turn radii at each wheel
- All commands are bridged to Gazebo via `ros_gz_bridge` as `Float64` ↔ `gz.msgs.Double`

Odometry comes from Gazebo's `OdometryPublisher` system plugin, which reports the vehicle's world-frame pose directly (not wheel encoder integration).

### Architecture comparison

```
LIGHTWEIGHT SIM              GAZEBO (perception:=sim)         GAZEBO (perception:=lidar)
───────────────              ────────────────────────         ─────────────────────────
lhr_trackgen (cones)     →   lhr_trackgen (reused)        →   Gazebo GPU LiDAR sensor
lhr_sensor_sim (FOV)     →   lhr_sensor_sim (reused)      →   lhr_perception (pointcloud)
lhr_sim_kinematic        →   Gazebo physics               →   Gazebo physics

lhr_track_builder        →   SAME (index pairing)         →   SAME (boundary pairing)
lhr_control              →   SAME                         →   SAME
lhr_mission_manager      →   SAME                         →   SAME
lhr_metrics              →   SAME                         →   SAME
```

All paths produce identical ROS 2 topic interfaces — the upper stack doesn't know the difference.

### World generation

The world generator script (`lhr_gazebo/scripts/generate_world.py`) creates a Gazebo world SDF from `lhr_trackgen` output:
- Accepts `--style` flag to select the track generator: `autocross` (default), `simple`, or `oval`
- Procedural cone placement from Catmull-Rom splines (same geometry as kinematic sim)
- Vehicle spawn position auto-computed on the straightest section of track (avoids loop closure area)
- ODE physics engine at 1 kHz (0.001 s step), real-time factor 1.0
- Ground plane: 200 x 200 m
- Pre-generated worlds (e.g. `worlds/oval_seed1.sdf`) are installed by colcon via `setup.py` `data_files`

### Gazebo-ROS bridge

The bridge config (`config/ros_gz_bridge.yaml`) maps 11 topics:

| Direction | ROS 2 Topic | Gazebo Topic | Type |
|-----------|-------------|--------------|------|
| GZ → ROS | `/lhr/vehicle/odom` | `/model/fsae_vehicle/odometry` | Odometry |
| GZ → ROS | `/tf` | `/model/fsae_vehicle/tf` | TFMessage |
| GZ → ROS | `/clock` | `/clock` | Clock |
| GZ → ROS | `/lhr/imu/data` | `/imu/data` | Imu |
| GZ → ROS | `/lhr/lidar/points` | `/lidar/points` | PointCloud2 |
| ROS → GZ | 2x steering `cmd_pos` | (same) | Float64/Double |
| ROS → GZ | 4x wheel `cmd_vel` | (same) | Float64/Double |

## Topics

| Topic | Type | Description |
|-------|------|-------------|
| `/lhr/track/cones` | `visualization_msgs/MarkerArray` | Blue (left) and yellow (right) cone markers (ground truth) |
| `/lhr/sensor/cones_detected` | `visualization_msgs/MarkerArray` | Cones detected by sensor sim (accumulated, FOV-filtered) |
| `/lhr/sensor/cones_viz` | `visualization_msgs/MarkerArray` | All cones: detected = bright, unseen = dim/transparent |
| `/lhr/sensor/fov_viz` | `visualization_msgs/MarkerArray` | Sensor FOV frustum visualization |
| `/lhr/track/centerline` | `nav_msgs/Path` | Ordered centerline path through midpoints |
| `/lhr/track/centerline_markers` | `visualization_msgs/MarkerArray` | Debug: green spheres + line strip |
| `/lhr/vehicle/cmd` | `ackermann_msgs/AckermannDriveStamped` | Steering + speed command |
| `/lhr/vehicle/odom` | `nav_msgs/Odometry` | Vehicle pose and twist |
| `/lhr/control/lookahead` | `visualization_msgs/Marker` | Debug: lookahead target point |
| `/lhr/mission/status` | `std_msgs/String` | Driverless system status (`OFF`, `READY`, `DRIVING`, `FINISHED`, `EMERGENCY`) |
| `/lhr/mission/go` | `std_msgs/Bool` | Go signal — triggers Ready → Driving transition |
| `/lhr/mission/emergency` | `std_msgs/Bool` | Emergency stop — triggers Driving → Emergency transition |
| `/lhr/mission/reset` | `std_msgs/Bool` | Reset — triggers Emergency → Off transition |
| `/lhr/metrics/lap_complete` | `std_msgs/Bool` | Published by metrics node when a lap is completed |
| `/lhr/debug/curvature` | `std_msgs/Float32` | Debug: estimated path curvature at lookahead |
| `/lhr/debug/v_cmd` | `std_msgs/Float32` | Debug: commanded speed after accel limiting |
| `/lhr/debug/mission_state` | `std_msgs/Float32` | Debug: numeric state for PlotJuggler (0=Off, 1=Ready, 2=Driving, 3=Finished, 4=Emergency) |
| `/lhr/imu/data` | `sensor_msgs/Imu` | IMU data (Gazebo sim only) |
| `/lhr/lidar/points` | `sensor_msgs/PointCloud2` | LiDAR pointcloud (Gazebo sim only) |
| `/lhr/perception/debug` | `visualization_msgs/MarkerArray` | LiDAR perception debug visualization |

## TF tree

```
map → base_link   (broadcast by lhr_sim_kinematic or Gazebo OdometryPublisher)
```

## Parameters

### lhr_trackgen (publish_cones)

| Param | Default | Description |
|-------|---------|-------------|
| `seed` | `1` | Random seed for track generation |
| `frame_id` | `"map"` | TF frame |
| `publish_hz` | `5.0` | Publishing rate (Hz) |
| `track_style` | `"autocross"` | Generator: `autocross` (Catmull-Rom spline), `simple` (original oval), or `oval` (dedicated oval generator) |
| `num_waypoints` | `10` | Number of waypoints around the loop (autocross only) |
| `radius_m` | `25.0` | Base radius of the track (autocross only) |
| `jitter_m` | `10.0` | Radial jitter per waypoint (autocross only) |
| `width_m` | `3.5` | Track width in meters |
| `cone_spacing_m` | `2.0` | Distance between cones along the track (autocross only) |

Cone IDs: left cones use IDs `0..N-1`, right cones use IDs `10000..10000+N-1`. The track builder relies on this convention for pairing.

### lhr_sensor_sim (sensor_sim)

| Param | Default | Description |
|-------|---------|-------------|
| `fov_deg` | `200.0` | Total field of view (degrees) |
| `max_range_m` | `20.0` | Max detection range (m) |
| `min_range_m` | `0.5` | Min detection range (m) |
| `detection_hz` | `10.0` | Publish rate (Hz) |
| `noise_std_m` | `0.0` | Gaussian position noise std-dev (0 = off) |
| `false_negative_rate` | `0.0` | Probability of missing a visible cone (0 = off) |

### lhr_track_builder (track_builder)

| Param | Default | Description |
|-------|---------|-------------|
| `frame_id` | `"map"` | TF frame |
| `publish_hz` | `5.0` | Publishing rate (Hz) |
| `max_points` | `200` | Cap on centerline points |
| `pairing_strategy` | `"index"` | Pairing strategy: `index` (ID-based, for sim), `nearest` (nearest-neighbor), or `boundary` (Delaunay triangulation, for LiDAR) |
| `track_width` | `3.5` | Expected track width for boundary pairing (m) |
| `track_width_tolerance` | `1.0` | Tolerance around track width for boundary pairing (m) |
| `cone_topic` | `"/lhr/sensor/cones_detected"` | Topic to subscribe for cone data |

Cone pairing strategies:
- **index** (default): Pairs left cone ID `i` with right cone ID `i + 10000`. Works with sim perception where cone IDs follow the trackgen convention.
- **nearest**: Pairs each left cone with its nearest unpaired right cone.
- **boundary**: Uses Delaunay triangulation to pair cones that are approximately `track_width` (3.5 m +/- `track_width_tolerance`) apart. Used for LiDAR perception where cones are unclassified (no left/right split).

### lhr_sim_kinematic (sim_node)

| Param | Default | Description |
|-------|---------|-------------|
| `wheelbase` | `1.6` | Wheelbase in meters |
| `update_hz` | `50.0` | Simulation step rate (Hz) |
| `max_steer` | `0.45` | Max steering angle (rad) |
| `max_speed` | `15.0` | Max speed (m/s) |
| `frame_id` | `"map"` | Parent TF frame |
| `child_frame_id` | `"base_link"` | Child TF frame |
| `init_x` | `0.0` | Initial X position (m) |
| `init_y` | `0.0` | Initial Y position (m) |
| `init_yaw` | `0.0` | Initial heading (rad) |

### lhr_mission_manager (mission_manager)

Implements the FSAE driverless state machine (DO.1.1). Controls when the vehicle is allowed to drive.

| Param | Default | Description |
|-------|---------|-------------|
| `mission` | `"autocross"` | Selected mission: `inspection`, `manual`, `ebs_test`, `acceleration`, `skidpad`, `autocross` |
| `auto_go` | `true` | Auto-transition Ready → Driving after `ready_hold_sec` (convenient for sim) |
| `ready_hold_sec` | `5.0` | Seconds to wait in Ready before auto-go |
| `status_hz` | `10.0` | Status publish rate (Hz) |

#### State machine

```
OFF ──→ READY ──→ DRIVING ──→ FINISHED
                     │
                     └──→ EMERGENCY ──→ OFF (on reset)
```

| Transition | Trigger |
|------------|---------|
| Off → Ready | Centerline path becomes available |
| Ready → Driving | Go signal received, or auto-go timer expires |
| Driving → Finished | Mission complete + vehicle speed < 0.5 m/s |
| Driving → Emergency | Emergency signal received |
| Emergency → Off | Reset signal received |

Mission completion triggers:
- **autocross**: lap detected by `lhr_metrics` (via `/lhr/metrics/lap_complete`)
- **inspection**: 28 seconds elapsed
- **acceleration, skidpad, ebs_test, manual**: not yet implemented

The control node (`pursuit_node`) subscribes to `/lhr/mission/status` and only sends drive commands when status is `DRIVING`. When the mission manager is not running, the control node operates freely for backward compatibility.

#### Sending commands (manual go/emergency/reset)

```bash
# Go signal (when auto_go is false):
ros2 topic pub --once /lhr/mission/go std_msgs/msg/Bool "{data: true}"

# Emergency stop:
ros2 topic pub --once /lhr/mission/emergency std_msgs/msg/Bool "{data: true}"

# Reset after emergency:
ros2 topic pub --once /lhr/mission/reset std_msgs/msg/Bool "{data: true}"

# Monitor status:
ros2 topic echo /lhr/mission/status
```

### lhr_control (pursuit_node)

Steering uses pure pursuit with **curvature-adaptive lookahead**. On straights the lookahead stays long for stability; on tight curves it shortens to reduce corner-cutting. The formula is:

```
ld = clamp(ld_max - gain * |curvature|, ld_min, ld_max)
```

Speed is planned from path curvature:
`v = clamp(sqrt(a_lat_max / |kappa|), v_min, v_max)` with acceleration limiting.

| Param | Default | Description |
|-------|---------|-------------|
| `lookahead_dist` | `4.0` | Max lookahead distance on straights (m) |
| `lookahead_min` | `2.0` | Min lookahead distance on tight curves (m) |
| `lookahead_curvature_gain` | `3.0` | How aggressively lookahead shortens with curvature |
| `max_steer` | `0.55` | Max steering angle (rad) |
| `wheelbase` | `1.6` | Wheelbase for steering calc (m) |
| `control_hz` | `20.0` | Control loop rate (Hz) |
| `a_lat_max` | `6.0` | Max lateral acceleration for speed law (m/s^2) |
| `v_min` | `2.0` | Minimum commanded speed (m/s) |
| `v_max` | `12.0` | Maximum commanded speed (m/s) |
| `kappa_eps` | `1e-3` | Epsilon to avoid division by zero in curvature |
| `curvature_window` | `5` | Index offset for 3-point curvature estimation |
| `max_accel` | `2.0` | Max longitudinal acceleration (m/s^2) |
| `max_decel` | `3.0` | Max longitudinal deceleration (m/s^2) |

The Gazebo demo launch overrides some defaults for tuned physics behavior:
- `lookahead_dist=5.0`, `lookahead_min=2.0`, `lookahead_curvature_gain=3.0`
- `a_lat_max=3.0`, `v_min=1.0`, `v_max=5.0`
- `max_accel=1.0`, `max_decel=2.0`

Debug topics: `/lhr/debug/curvature` and `/lhr/debug/v_cmd` (both `std_msgs/Float32`).

### lhr_gazebo

#### joint_cmd_adapter

Converts `AckermannDriveStamped` into 6 individual Gazebo joint commands with proper Ackermann differential steering geometry.

| Constant | Value | Description |
|----------|-------|-------------|
| `WHEELBASE` | `1.6` | Wheelbase (m) |
| `TRACK_WIDTH` | `1.2` | Kingpin-to-kingpin distance (m) |
| `WHEEL_RADIUS` | `0.2` | Wheel radius (m) |
| `MAX_STEER` | `0.69` | Steering clamp, slightly inside ±0.7 joint limit (rad) |

**Ackermann geometry:** When turning left, the left (inner) wheel steers at a sharper angle than the right (outer) wheel. The adapter computes both angles from the bicycle-model center angle using:
```
R = wheelbase / tan(|steer_center|)
inner = atan(wheelbase / (R - track_width/2))
outer = atan(wheelbase / (R + track_width/2))
```

**Wheel velocity differential:** Each wheel's angular velocity accounts for its distance from the instantaneous center of rotation. Outer wheels travel farther and spin faster than inner wheels in a turn.

### lhr_perception (lidar_cone_detector)

Processes LiDAR pointcloud to detect cones. Pipeline: ground removal → range filter → Euclidean clustering → cone validation → sensor-to-map transform → spatial dedup.

| Param | Default | Description |
|-------|---------|-------------|
| `max_range` | `20.0` | Max detection range (m) |
| `min_range` | `0.8` | Min detection range — avoids vehicle self-hits (m) |
| `ground_z_min` | `-0.40` | Ground removal lower threshold in sensor frame (m) |
| `ground_z_max` | `0.5` | Ground removal upper threshold in sensor frame (m) |
| `cluster_radius` | `0.5` | Euclidean clustering radius (m) |
| `min_cluster_points` | `1` | Minimum points for a valid cluster |
| `max_cluster_extent` | `0.5` | Maximum cluster bounding box extent (m) |
| `max_cluster_points` | `50` | Maximum points in a valid cone cluster |
| `dedup_radius` | `1.5` | Spatial dedup radius — new detections within this distance of existing ones are ignored (m) |
| `publish_hz` | `10.0` | Output publish rate (Hz) |

All detected cones are published under a single "cones" namespace with IDs 0..N-1 (orange color). There is no left/right classification — the track builder's boundary pairing strategy (Delaunay triangulation) handles cone pairing by finding pairs that are approximately track-width apart.

### lhr_metrics (metrics_node)

| Param | Default | Description |
|-------|---------|-------------|
| `off_track_threshold` | `2.0` | CTE above this (m) counts as off-track |
| `start_radius` | `2.0` | Distance (m) to centerline[0] to trigger lap zone |
| `start_hysteresis` | `1.0` | Extra distance (m) vehicle must exceed before lap can complete |
| `min_lap_time` | `5.0` | Minimum seconds before a lap return is accepted |
| `output_csv` | `"data/metrics.csv"` | Path for CSV output (relative to cwd) |
| `run_id` | `""` | Run identifier; auto-generates timestamp if empty |

### Metrics output

The metrics node publishes `/lhr/metrics/lap_complete` (`std_msgs/Bool`) when a lap is detected, which the mission manager uses to trigger the Driving → Finished transition.

It also prints a summary and appends a CSV row on lap completion or Ctrl+C:

```
run_id, duration_s, samples, mean_cte, max_cte, off_track_count, mean_speed, max_speed, lap_completed
```

CSV data accumulates in `data/metrics.csv` across runs.
