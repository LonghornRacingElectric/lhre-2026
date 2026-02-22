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
```

## Individual node scripts

If you prefer running nodes separately:

```bash
./scripts/run_cones.sh        # Terminal 1
./scripts/run_centerline.sh   # Terminal 2
./scripts/run_sim.sh          # Terminal 3
./scripts/run_control.sh      # Terminal 4
rviz2                         # Terminal 5
```

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
