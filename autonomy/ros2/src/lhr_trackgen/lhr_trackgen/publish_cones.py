#!/usr/bin/env python3
"""Cone publisher: generates a track and publishes left/right cone markers."""

import math
import random
from typing import List, Tuple

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy

from visualization_msgs.msg import Marker, MarkerArray
from geometry_msgs.msg import Point

# ---------------------------------------------------------------------------
# Track generators
# ---------------------------------------------------------------------------

ConeList = List[Tuple[float, float]]


def generate_simple_track(seed: int = 0,
                          **_kw) -> Tuple[ConeList, ConeList]:
    """Original squashed-oval generator (kept for debugging)."""
    rng = random.Random(seed)

    n = 80
    a, b, wobble = 20.0, 12.0, 2.0

    center = []
    for i in range(n):
        t = 2.0 * math.pi * i / n
        x = a * math.cos(t) + wobble * math.cos(3 * t)
        y = b * math.sin(t) + wobble * math.sin(2 * t)
        center.append((x, y))

    width = 3.5
    half = width / 2.0

    left: ConeList = []
    right: ConeList = []
    for i in range(n):
        x0, y0 = center[i - 1]
        x1, y1 = center[i]
        x2, y2 = center[(i + 1) % n]

        tx = x2 - x0
        ty = y2 - y0
        norm = math.hypot(tx, ty) or 1.0
        tx /= norm
        ty /= norm

        nx, ny = -ty, tx
        jitter = rng.uniform(-0.05, 0.05)

        lx = x1 + (half + jitter) * nx
        ly = y1 + (half + jitter) * ny
        rx = x1 - (half + jitter) * nx
        ry = y1 - (half + jitter) * ny

        if i % 2 == 0:
            left.append((lx, ly))
            right.append((rx, ry))

    return left, right


# ---------------------------------------------------------------------------
# Oval generator
# ---------------------------------------------------------------------------

def generate_oval_track(
    seed: int = 0,
    radius_m: float = 25.0,
    width_m: float = 3.5,
    cone_spacing_m: float = 2.0,
    aspect: float = 0.6,
    **_kw,
) -> Tuple[ConeList, ConeList]:
    """Generate a smooth oval (elliptical) track.

    The oval has semi-major axis *radius_m* and semi-minor axis
    *radius_m * aspect*.  Cones are placed at uniform arc-length
    intervals of *cone_spacing_m* along the centerline.
    """
    a = radius_m          # semi-major axis (x)
    b = radius_m * aspect  # semi-minor axis (y)
    half_w = width_m / 2.0

    # Step 1: Dense sampling of the ellipse centerline
    n_dense = 1000
    dense_pts: List[Tuple[float, float]] = []
    for i in range(n_dense):
        t = 2.0 * math.pi * i / n_dense
        dense_pts.append((a * math.cos(t), b * math.sin(t)))

    # Step 2: Compute cumulative arc lengths
    arc = [0.0]
    for i in range(1, n_dense):
        dx = dense_pts[i][0] - dense_pts[i - 1][0]
        dy = dense_pts[i][1] - dense_pts[i - 1][1]
        arc.append(arc[-1] + math.hypot(dx, dy))
    # Close the loop
    dx = dense_pts[0][0] - dense_pts[-1][0]
    dy = dense_pts[0][1] - dense_pts[-1][1]
    perimeter = arc[-1] + math.hypot(dx, dy)

    # Step 3: Resample at uniform arc-length intervals
    n_cones = max(20, int(perimeter / cone_spacing_m))
    target_spacing = perimeter / n_cones

    left: ConeList = []
    right: ConeList = []
    j = 0  # index into dense_pts

    for i in range(n_cones):
        target_s = i * target_spacing

        # Advance j until arc[j] >= target_s
        while j < n_dense - 1 and arc[j + 1] < target_s:
            j += 1

        # Interpolate between dense_pts[j] and dense_pts[j+1]
        if j < n_dense - 1:
            seg_len = arc[j + 1] - arc[j]
            frac = (target_s - arc[j]) / seg_len if seg_len > 0 else 0.0
            cx = dense_pts[j][0] + frac * (dense_pts[j + 1][0] - dense_pts[j][0])
            cy = dense_pts[j][1] + frac * (dense_pts[j + 1][1] - dense_pts[j][1])
        else:
            cx, cy = dense_pts[j]

        # Tangent via finite difference on the ellipse parametric form
        t = 2.0 * math.pi * target_s / perimeter
        tx = -a * math.sin(t)
        ty = b * math.cos(t)
        tn = math.hypot(tx, ty) or 1.0
        tx /= tn
        ty /= tn

        # Outward normal (left of tangent direction = counterclockwise)
        nx, ny = -ty, tx

        left.append((cx + half_w * nx, cy + half_w * ny))
        right.append((cx - half_w * nx, cy - half_w * ny))

    return left, right


# ---------------------------------------------------------------------------
# Autocross generator (waypoint + Catmull-Rom)
# ---------------------------------------------------------------------------

def _catmull_rom_segment(
    p0: Tuple[float, float],
    p1: Tuple[float, float],
    p2: Tuple[float, float],
    p3: Tuple[float, float],
    num_pts: int,
) -> List[Tuple[float, float]]:
    """Evaluate Catmull-Rom spline between p1 and p2."""
    pts: List[Tuple[float, float]] = []
    for k in range(num_pts):
        t = k / num_pts
        t2 = t * t
        t3 = t2 * t

        # Catmull-Rom basis (tau = 0.5)
        b0 = -0.5 * t3 + t2 - 0.5 * t
        b1 = 1.5 * t3 - 2.5 * t2 + 1.0
        b2 = -1.5 * t3 + 2.0 * t2 + 0.5 * t
        b3 = 0.5 * t3 - 0.5 * t2

        x = b0 * p0[0] + b1 * p1[0] + b2 * p2[0] + b3 * p3[0]
        y = b0 * p0[1] + b1 * p1[1] + b2 * p2[1] + b3 * p3[1]
        pts.append((x, y))
    return pts


def _catmull_rom_closed(
    waypoints: List[Tuple[float, float]],
    pts_per_seg: int = 20,
) -> List[Tuple[float, float]]:
    """Closed Catmull-Rom spline through waypoints."""
    n = len(waypoints)
    curve: List[Tuple[float, float]] = []
    for i in range(n):
        p0 = waypoints[(i - 1) % n]
        p1 = waypoints[i]
        p2 = waypoints[(i + 1) % n]
        p3 = waypoints[(i + 2) % n]
        curve.extend(_catmull_rom_segment(p0, p1, p2, p3, pts_per_seg))
    return curve


def _arc_lengths(pts: List[Tuple[float, float]]) -> List[float]:
    """Cumulative arc-length along a point list (closed)."""
    lengths = [0.0]
    for i in range(1, len(pts)):
        dx = pts[i][0] - pts[i - 1][0]
        dy = pts[i][1] - pts[i - 1][1]
        lengths.append(lengths[-1] + math.hypot(dx, dy))
    return lengths


def _resample_uniform(
    pts: List[Tuple[float, float]],
    spacing: float,
) -> List[Tuple[float, float]]:
    """Resample a polyline to approximately uniform spacing."""
    cum = _arc_lengths(pts)
    total = cum[-1]
    n_out = max(2, int(total / spacing))

    resampled: List[Tuple[float, float]] = []
    j = 0
    for i in range(n_out):
        target = total * i / n_out
        while j < len(cum) - 1 and cum[j + 1] < target:
            j += 1
        seg_len = cum[j + 1] - cum[j] if j < len(cum) - 1 else 1.0
        frac = (target - cum[j]) / seg_len if seg_len > 1e-9 else 0.0
        j2 = min(j + 1, len(pts) - 1)
        x = pts[j][0] + frac * (pts[j2][0] - pts[j][0])
        y = pts[j][1] + frac * (pts[j2][1] - pts[j][1])
        resampled.append((x, y))
    return resampled


def _offset_cones(
    center: List[Tuple[float, float]],
    half_width: float,
    cone_spacing: float,
    rng: random.Random,
) -> Tuple[ConeList, ConeList]:
    """Offset centerline to produce left/right cone lists."""
    n = len(center)
    # Resample to cone spacing first
    cone_pts = _resample_uniform(center, cone_spacing)
    nc = len(cone_pts)

    left: ConeList = []
    right: ConeList = []
    for i in range(nc):
        x0, y0 = cone_pts[(i - 1) % nc]
        x1, y1 = cone_pts[i]
        x2, y2 = cone_pts[(i + 1) % nc]

        tx = x2 - x0
        ty = y2 - y0
        norm = math.hypot(tx, ty) or 1.0
        tx /= norm
        ty /= norm

        nx, ny = -ty, tx
        jitter = rng.uniform(-0.05, 0.05)

        left.append((x1 + (half_width + jitter) * nx,
                      y1 + (half_width + jitter) * ny))
        right.append((x1 - (half_width + jitter) * nx,
                       y1 - (half_width + jitter) * ny))

    return left, right


def generate_autocross_track(
    seed: int = 0,
    num_waypoints: int = 10,
    radius_m: float = 25.0,
    jitter_m: float = 10.0,
    width_m: float = 3.5,
    cone_spacing_m: float = 2.0,
    **_kw,
) -> Tuple[ConeList, ConeList]:
    """Generate a randomised autocross track with S-curves and chicanes.

    1. Sample waypoints around a circle with radial + angular jitter.
    2. Sort by angle to form a closed loop.
    3. Fit a Catmull-Rom spline through the waypoints.
    4. Resample to uniform centerline spacing.
    5. Offset left/right to produce cones.
    """
    rng = random.Random(seed)

    # --- 1. Generate waypoints ---
    waypoints: List[Tuple[float, float]] = []
    for i in range(num_waypoints):
        base_angle = 2.0 * math.pi * i / num_waypoints
        # Angular jitter: ± half the gap between adjacent waypoints
        max_angle_jitter = math.pi / num_waypoints * 0.6
        angle = base_angle + rng.uniform(-max_angle_jitter, max_angle_jitter)

        # Radial jitter
        r = radius_m + rng.uniform(-jitter_m, jitter_m)
        # Clamp radius so track doesn't collapse to centre
        r = max(r, radius_m * 0.3)

        waypoints.append((r * math.cos(angle), r * math.sin(angle)))

    # --- 2. Sort by angle to guarantee loop ordering ---
    waypoints.sort(key=lambda p: math.atan2(p[1], p[0]))

    # --- 3. Spline ---
    pts_per_seg = max(10, int(120 / num_waypoints))
    centerline = _catmull_rom_closed(waypoints, pts_per_seg)

    # --- 4. Resample to ~0.5 m for smooth normals ---
    centerline = _resample_uniform(centerline, 0.5)

    # --- 5. Offset to cones ---
    return _offset_cones(centerline, width_m / 2.0, cone_spacing_m, rng)


# ---------------------------------------------------------------------------
# Dispatcher
# ---------------------------------------------------------------------------

GENERATORS = {
    'simple': generate_simple_track,
    'autocross': generate_autocross_track,
    'oval': generate_oval_track,
}


# ---------------------------------------------------------------------------
# ROS node
# ---------------------------------------------------------------------------

class ConePublisher(Node):
    """Publish left/right cone markers on /lhr/track/cones."""

    def __init__(self):
        super().__init__("cone_publisher")

        # --- Parameters ---
        self.declare_parameter("seed", 1)
        self.declare_parameter("frame_id", "map")
        self.declare_parameter("publish_hz", 5.0)
        self.declare_parameter("track_style", "autocross")
        self.declare_parameter("num_waypoints", 10)
        self.declare_parameter("radius_m", 25.0)
        self.declare_parameter("jitter_m", 10.0)
        self.declare_parameter("width_m", 3.5)
        self.declare_parameter("cone_spacing_m", 2.0)

        self.seed = int(self.get_parameter("seed").value)
        self.frame_id = str(self.get_parameter("frame_id").value)
        hz = float(self.get_parameter("publish_hz").value)
        style = str(self.get_parameter("track_style").value)
        num_wp = int(self.get_parameter("num_waypoints").value)
        radius = float(self.get_parameter("radius_m").value)
        jitter = float(self.get_parameter("jitter_m").value)
        width = float(self.get_parameter("width_m").value)
        spacing = float(self.get_parameter("cone_spacing_m").value)

        # --- Generate track ---
        gen = GENERATORS.get(style)
        if gen is None:
            self.get_logger().warn(
                f"Unknown track_style '{style}', falling back to 'autocross'")
            gen = generate_autocross_track

        self.left, self.right = gen(
            seed=self.seed,
            num_waypoints=num_wp,
            radius_m=radius,
            jitter_m=jitter,
            width_m=width,
            cone_spacing_m=spacing,
        )

        # --- Publisher ---
        qos = QoSProfile(depth=1)
        qos.reliability = ReliabilityPolicy.RELIABLE
        qos.durability = DurabilityPolicy.TRANSIENT_LOCAL

        self.pub = self.create_publisher(
            MarkerArray, "/lhr/track/cones", qos)

        period = 1.0 / max(hz, 0.1)
        self.timer = self.create_timer(period, self.on_timer)

        self.get_logger().info(
            f"Publishing {len(self.left)} left + {len(self.right)} right "
            f"cones (style={style}, seed={self.seed})")

    def make_cone_marker(
        self, mid: int, ns: str,
        x: float, y: float,
        r: float, g: float, b: float,
    ) -> Marker:
        """Create a single cone sphere marker."""
        m = Marker()
        m.header.frame_id = self.frame_id
        m.header.stamp = self.get_clock().now().to_msg()
        m.ns = ns
        m.id = mid
        m.type = Marker.SPHERE
        m.action = Marker.ADD
        m.pose.position.x = float(x)
        m.pose.position.y = float(y)
        m.pose.position.z = 0.0
        m.pose.orientation.w = 1.0

        m.scale.x = 0.35
        m.scale.y = 0.35
        m.scale.z = 0.35

        m.color.a = 1.0
        m.color.r = float(r)
        m.color.g = float(g)
        m.color.b = float(b)
        return m

    def on_timer(self):
        """Publish all cone markers."""
        arr = MarkerArray()

        for i, (x, y) in enumerate(self.left):
            arr.markers.append(
                self.make_cone_marker(i, "left_cones", x, y,
                                      0.0, 0.2, 1.0))

        base = 10000
        for i, (x, y) in enumerate(self.right):
            arr.markers.append(
                self.make_cone_marker(base + i, "right_cones", x, y,
                                      1.0, 1.0, 0.0))

        self.pub.publish(arr)


def main():
    """Entry point."""
    rclpy.init()
    node = ConePublisher()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()


if __name__ == "__main__":
    main()
