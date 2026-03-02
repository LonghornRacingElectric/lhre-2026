# Phase 3 — Camera Fusion for Cone Classification

## Goal

Add a monocular camera to the Gazebo model and fuse its output with the existing LiDAR pipeline to classify cones as left (blue) or right (yellow). Use color labels to strengthen Delaunay-based boundary pairing rather than replace it.

## Background

LiDAR gives accurate 3D positions but no color. The Delaunay track builder filters edges by track-width distance, but on tight autocross corners same-side cones can be ~3.5m apart, producing false cross-track pairs and bad midpoints. Knowing cone color eliminates these false pairs.

## Approach: Color-Augmented Delaunay

Keep Delaunay triangulation as the core pairing strategy. Add cone color as an additional edge filter:

1. LiDAR detects cone positions (existing pipeline, unchanged)
2. Camera classifies each cone as blue / yellow / unknown
3. Fusion step assigns a color label to each cone in the map
4. Delaunay triangulation (existing)
5. Filter edges by track-width band (existing)
6. **New**: reject edges where both endpoints share the same color
7. Unclassified cones pass through the existing distance-only filter

Color is purely additive — if the camera misses a cone, behavior falls back to current Delaunay-only filtering.

## Simulation Shortcut

In Gazebo, cones are rendered with distinct colors. We can use simple color segmentation (or ground-truth object labels from Gazebo) instead of a trained YOLO model. A real YOLO model is only needed for the physical car.

## How Other Teams Do It

Most FSAE driverless teams use LiDAR-camera fusion:

- YOLO (v5/v8) detects cones in 2D and classifies by color in one pass
- LiDAR points are projected into the camera frame using extrinsic calibration
- Points inside a bounding box inherit that box's color label
- Teams train on the FSOCO dataset plus custom data
- IIT Bombay Racing (2024) uses a three-tier fallback: LiDAR-camera fusion primary, monocular depth secondary, stereo keypoint tertiary

For our sim pipeline, the equivalent is: render camera image, segment by color, project LiDAR cone positions into the image, assign the color of the pixel/region at that projection.

## Implementation Tasks

### 3.1 — Add camera sensor to Gazebo model
- Mount a forward-facing camera on the vehicle SDF/URDF
- Bridge the image topic to ROS 2 (`/lhr/sensor/camera/image_raw`)
- Verify image stream in RViz

### 3.2 — Cone color classifier (sim)
- Subscribe to camera image
- Use color segmentation (HSV thresholding) to identify blue/yellow regions
- Publish classified regions or a lookup service

### 3.3 — LiDAR-camera fusion node
- Subscribe to both LiDAR cone detections and camera image
- Project each LiDAR cone position into the camera frame using TF
- Look up the color at that pixel location in the classified image
- Publish cone markers with color labels (blue/yellow/unknown)

### 3.4 — Update track builder to use color
- In `_pair_boundary`, add a filter: reject Delaunay edges where both endpoints have the same known color
- Accept edges where at least one endpoint is unknown (fallback to distance-only)
- Minimal change — ~5 lines in the existing method

### 3.5 — Validate on autocross track
- Run full pipeline on an autocross layout
- Compare centerline quality vs current Delaunay-only approach
- Verify that unclassified cones degrade gracefully

## Dependencies

- Phase 2 LiDAR pipeline must be functional (it is on oval)
- Vehicle model must support camera mount point
- Gazebo bridge must support image transport

## References

- [IIT Bombay Racing Driverless Stack (2024)](https://arxiv.org/html/2408.06113v1)
- [YOLOv8 + ZED2 Cone Clustering](https://www.mdpi.com/2673-4591/79/1/96)
- [FSOCO Dataset](https://ddavid.github.io/fsoco/)
