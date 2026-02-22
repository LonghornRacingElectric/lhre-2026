# Autonomy – ROS 2 Workspace

Colcon workspace for LHR driverless / autonomy nodes.
Target: **ROS 2 Humble** on Ubuntu 22.04 (WSL2).

## Packages

| Package | Description |
|---------|-------------|
| `lhr_trackgen` | Publishes a synthetic cone track (`/lhr/track/cones`) |
| `lhr_track_builder` | Subscribes to cones, publishes centerline path (`/lhr/track/centerline`) |

## Quick start

```bash
# 1. Build everything
./scripts/build.sh

# 2. In terminal 1 – publish cones
./scripts/run_cones.sh

# 3. In terminal 2 – publish centerline
./scripts/run_centerline.sh

# 4. In terminal 3 – open RViz
rviz2
```

### RViz setup

1. Set **Fixed Frame** to `map`.
2. Add display **MarkerArray** on topic `/lhr/track/cones` (cones).
3. Add display **Path** on topic `/lhr/track/centerline` (centerline).
4. (Optional) Add **MarkerArray** on `/lhr/track/centerline_markers` (debug midpoints + line).

## Topics

| Topic | Type | Description |
|-------|------|-------------|
| `/lhr/track/cones` | `visualization_msgs/MarkerArray` | Blue (left) and yellow (right) cone markers |
| `/lhr/track/centerline` | `nav_msgs/Path` | Ordered centerline path through midpoints |
| `/lhr/track/centerline_markers` | `visualization_msgs/MarkerArray` | Debug: green spheres + line strip |

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
