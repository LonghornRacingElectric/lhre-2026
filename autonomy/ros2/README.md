# Autonomy – ROS 2 Workspace

Colcon workspace for LHR driverless / autonomy nodes.
Target: **ROS 2 Humble** on Ubuntu 22.04 (WSL2).

## Packages

| Package | Description |
|---------|-------------|
| `lhr_trackgen` | Publishes a synthetic cone track (`/lhr/track/cones`) |
| `lhr_track_builder` | Subscribes to cones, publishes centerline path (`/lhr/track/centerline`) |
| `lhr_sim_kinematic` | Kinematic bicycle-model vehicle simulator |
| `lhr_control` | Pure pursuit path-following controller |
| `lhr_metrics` | Cross-track error, off-track count, and lap detection (CSV output) |
| `lhr_demo` | Launch file that starts the full stack in one command |

## Prerequisites

```bash
sudo apt install ros-humble-ackermann-msgs ros-humble-tf2-ros
```

## Quick start (two terminals)

```bash
# Build once
./scripts/build.sh

# Terminal 1 – start the full stack
./scripts/run_demo.sh

# Terminal 2 – open RViz (pre-configured displays + fixed frame = map)
./scripts/rviz_demo.sh
```

Launch arguments can be passed through `run_demo.sh`:

```bash
./scripts/run_demo.sh target_speed:=8.0 lookahead_dist:=6.0 seed:=42

# Enable metrics collection (writes to data/metrics.csv):
./scripts/run_demo.sh enable_metrics:=true
```

## Scripts reference

All scripts live in `scripts/` and should be run from the `autonomy/ros2` directory.

| Script | Description |
|--------|-------------|
| `build.sh` | Builds all packages with `colcon build --symlink-install`. Run after any code change. |
| `run_demo.sh` | Launches the full stack (cones + centerline + sim + control) via `ros2 launch`. Accepts launch args, e.g. `./scripts/run_demo.sh target_speed:=8.0`. |
| `rviz_demo.sh` | Opens RViz with the pre-configured `rviz/default.rviz` config (all displays + fixed frame already set). |
| `run_cones.sh` | Runs only the cone publisher (`lhr_trackgen`). |
| `run_centerline.sh` | Runs only the centerline builder (`lhr_track_builder`). |
| `run_sim.sh` | Runs only the kinematic vehicle simulator (`lhr_sim_kinematic`). |
| `run_control.sh` | Runs only the pure pursuit controller (`lhr_control`). |
| `run_metrics.sh` | Runs only the metrics node (`lhr_metrics`). Prints summary on Ctrl+C and appends to `data/metrics.csv`. |

The individual `run_*.sh` scripts are useful for debugging a single node. For normal use, prefer the two-terminal workflow (`run_demo.sh` + `rviz_demo.sh`).

## Topics

| Topic | Type | Description |
|-------|------|-------------|
| `/lhr/track/cones` | `visualization_msgs/MarkerArray` | Blue (left) and yellow (right) cone markers |
| `/lhr/track/centerline` | `nav_msgs/Path` | Ordered centerline path through midpoints |
| `/lhr/track/centerline_markers` | `visualization_msgs/MarkerArray` | Debug: green spheres + line strip |
| `/lhr/vehicle/cmd` | `ackermann_msgs/AckermannDriveStamped` | Steering + speed command |
| `/lhr/vehicle/odom` | `nav_msgs/Odometry` | Vehicle pose and twist |
| `/lhr/control/lookahead` | `visualization_msgs/Marker` | Debug: lookahead target point |

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

### lhr_track_builder (track_builder)

| Param | Default | Description |
|-------|---------|-------------|
| `frame_id` | `"map"` | TF frame |
| `publish_hz` | `5.0` | Publishing rate (Hz) |
| `max_points` | `200` | Cap on centerline points |
| `pairing_strategy` | `"index"` | Cone pairing method (only `index` for now) |

### lhr_sim_kinematic (sim_node)

| Param | Default | Description |
|-------|---------|-------------|
| `wheelbase` | `1.6` | Wheelbase in meters |
| `update_hz` | `50.0` | Simulation step rate (Hz) |
| `max_steer` | `0.45` | Max steering angle (rad) |
| `max_speed` | `15.0` | Max speed (m/s) |
| `frame_id` | `"map"` | Parent TF frame |
| `child_frame_id` | `"base_link"` | Child TF frame |

### lhr_control (pursuit_node)

| Param | Default | Description |
|-------|---------|-------------|
| `lookahead_dist` | `4.0` | Lookahead distance (m) |
| `target_speed` | `5.0` | Constant speed command (m/s) |
| `max_steer` | `0.45` | Max steering angle (rad) |
| `wheelbase` | `1.6` | Wheelbase for steering calc (m) |
| `control_hz` | `20.0` | Control loop rate (Hz) |

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

The metrics node prints a summary and appends a CSV row on lap completion or Ctrl+C:

```
run_id, duration_s, samples, mean_cte, max_cte, off_track_count, lap_completed
```

CSV data accumulates in `data/metrics.csv` across runs.
