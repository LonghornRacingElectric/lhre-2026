# Autonomy – ROS 2 Workspace

Colcon workspace for LHR driverless / autonomy nodes.

**Supported platforms:**
- Ubuntu 22.04 (WSL2 or native) + **ROS 2 Humble**
- Ubuntu 24.04 (WSL2 or native) + **ROS 2 Jazzy**

All scripts auto-detect which ROS 2 distro is installed (via `scripts/_ros_env.sh`).

## Packages

| Package | Description |
|---------|-------------|
| `lhr_trackgen` | Publishes a synthetic cone track (`/lhr/track/cones`) |
| `lhr_sensor_sim` | FOV-limited sensor simulation — filters cones by vehicle pose, accumulates detections |
| `lhr_track_builder` | Subscribes to cones, publishes centerline path (`/lhr/track/centerline`) |
| `lhr_sim_kinematic` | Kinematic bicycle-model vehicle simulator |
| `lhr_control` | Pure pursuit path-following controller |
| `lhr_mission_manager` | FSAE driverless state machine (Off → Ready → Driving → Finished → Emergency) |
| `lhr_metrics` | Cross-track error, off-track count, and lap detection (CSV output) |
| `lhr_demo` | Launch file that starts the full stack in one command |

## Data flow

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

To bypass the sensor sim and use all cones directly (god-mode), override the cone topic:
```bash
ros2 run lhr_track_builder track_builder --ros-args -p cone_topic:=/lhr/track/cones
```

## Prerequisites

### 1. WSL2 (Windows only)

If you're on Windows, install WSL2 with Ubuntu first:

```powershell
# In PowerShell (as admin)
wsl --install -d Ubuntu-24.04    # or Ubuntu-22.04
```

Restart, then open the Ubuntu terminal and continue below.

### 2. Install ROS 2

ROS 2 isn't in Ubuntu's default repos — you need to add the ROS apt source first.

```bash
# Install prerequisites
sudo apt update && sudo apt install -y software-properties-common curl

# Add the ROS 2 GPG key
sudo curl -sSL https://raw.githubusercontent.com/ros/rosdistro/master/ros.key \
  -o /usr/share/keyrings/ros-archive-keyring.gpg

# Add the ROS 2 apt repository
echo "deb [arch=$(dpkg --print-architecture) signed-by=/usr/share/keyrings/ros-archive-keyring.gpg] \
  http://packages.ros.org/ros2/ubuntu $(. /etc/os-release && echo $UBUNTU_CODENAME) main" \
  | sudo tee /etc/apt/sources.list.d/ros2.list > /dev/null

# Update package index
sudo apt update
```

### 3. Install ROS 2 packages

```bash
# Ubuntu 22.04 (Humble)
sudo apt install -y ros-humble-desktop ros-humble-ackermann-msgs ros-humble-tf2-ros

# Ubuntu 24.04 (Jazzy)
sudo apt install -y ros-jazzy-desktop ros-jazzy-ackermann-msgs ros-jazzy-tf2-ros
```

Install whichever matches your Ubuntu version. The build scripts auto-detect the distro.

### 4. Install colcon (build tool)

```bash
sudo apt install -y python3-colcon-common-extensions
```

### 5. Install PlotJuggler (optional — for plotting debug signals)

```bash
# Ubuntu 22.04 (Humble)
sudo apt install -y ros-humble-plotjuggler-ros

# Ubuntu 24.04 (Jazzy)
sudo apt install -y ros-jazzy-plotjuggler-ros
```

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

> **WSL2 note:** `run_plotjuggler.sh` uses `setsid` to launch in a separate process session, which avoids WSLg focus/input conflicts between Qt apps. If RViz becomes unresponsive (no mouse/keyboard input), close it, run `wsl --shutdown` from PowerShell, reopen WSL, and relaunch.

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
| `run_demo.sh` | Launches the full stack (cones + centerline + sim + control + mission manager + metrics) via `ros2 launch`. Accepts launch args, e.g. `./scripts/run_demo.sh seed:=42`. |
| `rviz_demo.sh` | Opens RViz with the pre-configured `rviz/default.rviz` config (all displays + fixed frame already set). |
| `run_cones.sh` | Runs only the cone publisher (`lhr_trackgen`). |
| `run_centerline.sh` | Runs only the centerline builder (`lhr_track_builder`). |
| `run_sim.sh` | Runs only the kinematic vehicle simulator (`lhr_sim_kinematic`). |
| `run_control.sh` | Runs only the pure pursuit controller (`lhr_control`). |
| `run_sensor.sh` | Runs only the sensor simulation (`lhr_sensor_sim`). |
| `run_metrics.sh` | Runs only the metrics node (`lhr_metrics`). Prints summary on Ctrl+C and appends to `data/metrics.csv`. |
| `run_plotjuggler.sh` | Opens PlotJuggler for plotting debug signals (curvature, speed, steering). |

The individual `run_*.sh` scripts are useful for debugging a single node. For normal use, prefer the two-terminal workflow (`run_demo.sh` + `rviz_demo.sh`).

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

## TF tree

```
map → base_link   (broadcast by lhr_sim_kinematic)
```

## Parameters

### lhr_trackgen (publish_cones)

| Param | Default | Description |
|-------|---------|-------------|
| `seed` | `1` | Random seed for track generation |
| `frame_id` | `"map"` | TF frame |
| `publish_hz` | `5.0` | Publishing rate (Hz) |
| `track_style` | `"autocross"` | Generator: `autocross` (Catmull-Rom spline) or `simple` (original oval) |
| `num_waypoints` | `10` | Number of waypoints around the loop (autocross only) |
| `radius_m` | `25.0` | Base radius of the track (autocross only) |
| `jitter_m` | `10.0` | Radial jitter per waypoint (autocross only) |
| `width_m` | `3.5` | Track width in meters |
| `cone_spacing_m` | `2.0` | Distance between cones along the track (autocross only) |

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
| `pairing_strategy` | `"index"` | Cone pairing method (only `index` for now) |
| `cone_topic` | `"/lhr/sensor/cones_detected"` | Topic to subscribe for cone data |

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

Steering uses pure pursuit. Speed is planned from path curvature:
`v = clamp(sqrt(a_lat_max / |kappa|), v_min, v_max)` with acceleration limiting.

| Param | Default | Description |
|-------|---------|-------------|
| `lookahead_dist` | `4.0` | Lookahead distance (m) |
| `max_steer` | `0.45` | Max steering angle (rad) |
| `wheelbase` | `1.6` | Wheelbase for steering calc (m) |
| `control_hz` | `20.0` | Control loop rate (Hz) |
| `a_lat_max` | `6.0` | Max lateral acceleration for speed law (m/s^2) |
| `v_min` | `2.0` | Minimum commanded speed (m/s) |
| `v_max` | `12.0` | Maximum commanded speed (m/s) |
| `kappa_eps` | `1e-3` | Epsilon to avoid division by zero in curvature |
| `curvature_window` | `5` | Index offset for 3-point curvature estimation |
| `max_accel` | `2.0` | Max longitudinal acceleration (m/s^2) |
| `max_decel` | `3.0` | Max longitudinal deceleration (m/s^2) |

Debug topics: `/lhr/debug/curvature` and `/lhr/debug/v_cmd` (both `std_msgs/Float32`).

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
