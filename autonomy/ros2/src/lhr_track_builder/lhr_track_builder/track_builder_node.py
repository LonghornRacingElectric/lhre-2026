#!/usr/bin/env python3
"""Track builder node: subscribes to cones, publishes centerline Path."""

import math
from typing import List, Tuple

import numpy as np
from scipy.spatial import Delaunay
from scipy.spatial.qhull import QhullError

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy

from builtin_interfaces.msg import Time
from geometry_msgs.msg import PoseStamped, Point
from nav_msgs.msg import Odometry, Path
from std_msgs.msg import ColorRGBA, Header
from visualization_msgs.msg import Marker, MarkerArray


class TrackBuilder(Node):
    """Subscribe to cone markers, compute midpoints, publish centerline."""

    def __init__(self):
        super().__init__('track_builder')

        # --- Parameters ---
        self.declare_parameter('frame_id', 'map')
        self.declare_parameter('publish_hz', 5.0)
        self.declare_parameter('max_points', 200)
        self.declare_parameter('pairing_strategy', 'index')
        self.declare_parameter('cone_topic', '/lhr/sensor/cones_detected')
        self.declare_parameter('track_width', 3.5)
        self.declare_parameter('track_width_tolerance', 1.0)

        self._frame_id = self.get_parameter(
            'frame_id').get_parameter_value().string_value
        publish_hz = self.get_parameter(
            'publish_hz').get_parameter_value().double_value
        self._max_points = self.get_parameter(
            'max_points').get_parameter_value().integer_value
        self._pairing_strategy = self.get_parameter(
            'pairing_strategy').get_parameter_value().string_value
        cone_topic = self.get_parameter(
            'cone_topic').get_parameter_value().string_value
        self._track_width = self.get_parameter(
            'track_width').get_parameter_value().double_value
        self._track_width_tol = self.get_parameter(
            'track_width_tolerance').get_parameter_value().double_value

        # --- Stored cone positions keyed by marker ID ---
        self._left_cones: dict = {}
        self._right_cones: dict = {}
        self._all_cones: dict = {}

        # --- Vehicle pose (used by nearest/boundary strategy) ---
        self._veh_x = 0.0
        self._veh_y = 0.0
        self._veh_yaw = 0.0
        self._have_odom = False

        # --- QoS ---
        # Subscriber must match cone publisher (TRANSIENT_LOCAL)
        sub_qos = QoSProfile(
            depth=1,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
        )
        # Publishers use VOLATILE — we republish at publish_hz so
        # latching is unnecessary, and avoids DDS overhead on WSL2.
        pub_qos = QoSProfile(
            depth=1,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.VOLATILE,
        )

        # --- Subscribers ---
        self.create_subscription(
            MarkerArray, cone_topic, self._cones_cb, sub_qos)
        if self._pairing_strategy in ('nearest', 'boundary'):
            self.create_subscription(
                Odometry, '/lhr/vehicle/odom', self._odom_cb, 10)

        # --- Publishers ---
        self._path_pub = self.create_publisher(
            Path, '/lhr/track/centerline', pub_qos)
        self._debug_pub = self.create_publisher(
            MarkerArray, '/lhr/track/centerline_markers', pub_qos)

        # --- Timer ---
        self.create_timer(1.0 / publish_hz, self._on_timer)
        self.get_logger().info(
            f'TrackBuilder ready  (strategy={self._pairing_strategy}, '
            f'hz={publish_hz}, max_points={self._max_points})')

    # ------------------------------------------------------------------
    # Callbacks
    # ------------------------------------------------------------------
    def _cones_cb(self, msg: MarkerArray):
        """Extract cone positions from the MarkerArray, keyed by ID."""
        left: dict = {}
        right: dict = {}
        all_cones: dict = {}
        for marker in msg.markers:
            pos = marker.pose.position
            if marker.ns == 'left_cones':
                left[marker.id] = (pos.x, pos.y)
            elif marker.ns == 'right_cones':
                right[marker.id] = (pos.x, pos.y)
            elif marker.ns == 'cones':
                all_cones[marker.id] = (pos.x, pos.y)
        self._left_cones = left
        self._right_cones = right
        self._all_cones = all_cones

    def _odom_cb(self, msg: Odometry):
        self._veh_x = msg.pose.pose.position.x
        self._veh_y = msg.pose.pose.position.y
        q = msg.pose.pose.orientation
        self._veh_yaw = math.atan2(
            2.0 * (q.w * q.z + q.x * q.y),
            1.0 - 2.0 * (q.y * q.y + q.z * q.z))
        self._have_odom = True

    def _on_timer(self):
        """Compute centerline and publish Path + debug markers."""
        if self._pairing_strategy == 'boundary':
            if len(self._all_cones) < 4:
                return
        else:
            if not self._left_cones or not self._right_cones:
                return

        midpoints = self._compute_midpoints()
        if not midpoints:
            return

        now = self.get_clock().now().to_msg()
        self._publish_path(midpoints, now)
        self._publish_debug_markers(midpoints, now)

    # ------------------------------------------------------------------
    # Centerline computation
    # ------------------------------------------------------------------
    def _compute_midpoints(self) -> List[Tuple[float, float]]:
        """Pair cones and return midpoints.

        Strategy 'index': match left ID ``i`` with right ID ``10000 + i``.
        Strategy 'nearest': pair each left cone with its nearest right cone.
        Strategy 'boundary': Delaunay triangulation, filter by track width.
        """
        if self._pairing_strategy == 'boundary':
            return self._pair_boundary()
        if self._pairing_strategy == 'nearest':
            return self._pair_nearest()
        return self._pair_by_index()

    def _pair_by_index(self) -> List[Tuple[float, float]]:
        """Pair left/right cones by matching ID offset (trackgen convention)."""
        midpoints: List[Tuple[float, float]] = []
        for lid in sorted(self._left_cones.keys()):
            rid = lid + 10000
            if rid in self._right_cones:
                lx, ly = self._left_cones[lid]
                rx, ry = self._right_cones[rid]
                midpoints.append(((lx + rx) / 2.0, (ly + ry) / 2.0))
            if len(midpoints) >= self._max_points:
                break
        return midpoints

    def _pair_nearest(self) -> List[Tuple[float, float]]:
        """Pair each left cone with the nearest unpaired right cone.

        After pairing, midpoints are chained into path-sequential order
        using a greedy nearest-neighbor walk starting from the point
        nearest to the vehicle, oriented in the vehicle's heading
        direction.
        """
        if not self._left_cones or not self._right_cones:
            return []

        right_items = list(self._right_cones.items())
        used_right: set = set()
        midpoints: List[Tuple[float, float]] = []

        for lid in sorted(self._left_cones.keys()):
            lx, ly = self._left_cones[lid]
            best_dist = float('inf')
            best_idx = -1

            for j, (rid, (rx, ry)) in enumerate(right_items):
                if j in used_right:
                    continue
                d = (lx - rx) ** 2 + (ly - ry) ** 2
                if d < best_dist:
                    best_dist = d
                    best_idx = j

            if best_idx >= 0:
                used_right.add(best_idx)
                rx, ry = right_items[best_idx][1]
                midpoints.append(((lx + rx) / 2.0, (ly + ry) / 2.0))

            if len(midpoints) >= self._max_points:
                break

        # Chain midpoints into path order starting from the vehicle
        if len(midpoints) > 2 and self._have_odom:
            midpoints = self._chain_path_from_vehicle(midpoints)
        elif len(midpoints) > 2:
            midpoints = self._chain_path(midpoints, 0)

        return midpoints

    def _pair_boundary(self) -> List[Tuple[float, float]]:
        """Pair cones across track boundaries using Delaunay triangulation.

        Finds natural geometric neighbors via Delaunay, then filters
        edges to those approximately track-width apart.  Each surviving
        edge is a cross-track pair whose midpoint lies on the centerline.
        """
        if len(self._all_cones) < 4:
            return []

        pts = np.array(list(self._all_cones.values()))

        try:
            tri = Delaunay(pts)
        except QhullError:
            return []

        # Extract unique edges from triangles
        edges: set = set()
        for simplex in tri.simplices:
            for i in range(3):
                a, b = int(simplex[i]), int(simplex[(i + 1) % 3])
                edges.add((min(a, b), max(a, b)))

        # Filter edges by track-width band
        lo = self._track_width - self._track_width_tol
        hi = self._track_width + self._track_width_tol
        midpoints: List[Tuple[float, float]] = []
        for a, b in edges:
            dx = pts[a][0] - pts[b][0]
            dy = pts[a][1] - pts[b][1]
            d = math.sqrt(dx * dx + dy * dy)
            if lo <= d <= hi:
                mx = (pts[a][0] + pts[b][0]) / 2.0
                my = (pts[a][1] + pts[b][1]) / 2.0
                midpoints.append((mx, my))

        if len(midpoints) > self._max_points:
            midpoints = midpoints[:self._max_points]

        # Chain into sequential path order from vehicle
        if len(midpoints) > 2 and self._have_odom:
            midpoints = self._chain_path_from_vehicle(midpoints)
        elif len(midpoints) > 2:
            midpoints = self._chain_path(midpoints, 0)

        return midpoints

    # ------------------------------------------------------------------
    # Path chaining helpers
    # ------------------------------------------------------------------
    def _chain_path_from_vehicle(
        self, points: List[Tuple[float, float]],
    ) -> List[Tuple[float, float]]:
        """Chain midpoints starting from the nearest to the vehicle,
        oriented in the vehicle's heading direction."""

        # Find the midpoint closest to the vehicle
        vx, vy = self._veh_x, self._veh_y
        start_idx = min(
            range(len(points)),
            key=lambda i: (points[i][0] - vx) ** 2
                          + (points[i][1] - vy) ** 2)

        ordered = self._chain_path(points, start_idx)

        # Check if the chain goes in the vehicle's heading direction.
        # Compare the vector from ordered[0]→ordered[1] against the
        # vehicle's yaw.  If they disagree, reverse the chain.
        if len(ordered) >= 2:
            dx = ordered[1][0] - ordered[0][0]
            dy = ordered[1][1] - ordered[0][1]
            path_angle = math.atan2(dy, dx)
            angle_diff = math.atan2(
                math.sin(path_angle - self._veh_yaw),
                math.cos(path_angle - self._veh_yaw))
            if abs(angle_diff) > math.pi / 2:
                ordered.reverse()

        return ordered

    @staticmethod
    def _chain_path(
        points: List[Tuple[float, float]],
        start: int,
    ) -> List[Tuple[float, float]]:
        """Order points into a path using greedy nearest-neighbor chaining."""
        ordered = [points[start]]
        remaining = set(range(len(points)))
        remaining.discard(start)

        while remaining:
            lx, ly = ordered[-1]
            best_j = min(
                remaining,
                key=lambda j: (points[j][0] - lx) ** 2
                              + (points[j][1] - ly) ** 2)
            ordered.append(points[best_j])
            remaining.remove(best_j)

        return ordered

    # ------------------------------------------------------------------
    # Publishing helpers
    # ------------------------------------------------------------------
    def _make_header(self, stamp: Time) -> Header:
        header = Header()
        header.stamp = stamp
        header.frame_id = self._frame_id
        return header

    def _publish_path(self, midpoints: List[Tuple[float, float]],
                      stamp: Time):
        path = Path()
        path.header = self._make_header(stamp)
        for x, y in midpoints:
            ps = PoseStamped()
            ps.header = self._make_header(stamp)
            ps.pose.position.x = x
            ps.pose.position.y = y
            ps.pose.position.z = 0.0
            ps.pose.orientation.w = 1.0
            path.poses.append(ps)
        self._path_pub.publish(path)

    def _publish_debug_markers(self, midpoints: List[Tuple[float, float]],
                               stamp: Time):
        markers = MarkerArray()

        # Midpoint spheres
        for i, (x, y) in enumerate(midpoints):
            m = Marker()
            m.header = self._make_header(stamp)
            m.ns = 'centerline_points'
            m.id = i
            m.type = Marker.SPHERE
            m.action = Marker.ADD
            m.pose.position = Point(x=x, y=y, z=0.0)
            m.pose.orientation.w = 1.0
            m.scale.x = 0.2
            m.scale.y = 0.2
            m.scale.z = 0.2
            m.color = ColorRGBA(r=0.0, g=1.0, b=0.0, a=1.0)
            markers.markers.append(m)

        # LINE_STRIP connecting midpoints
        line = Marker()
        line.header = self._make_header(stamp)
        line.ns = 'centerline_line'
        line.id = 0
        line.type = Marker.LINE_STRIP
        line.action = Marker.ADD
        line.scale.x = 0.08
        line.color = ColorRGBA(r=0.0, g=1.0, b=0.0, a=0.8)
        line.pose.orientation.w = 1.0
        for x, y in midpoints:
            line.points.append(Point(x=x, y=y, z=0.0))
        markers.markers.append(line)

        self._debug_pub.publish(markers)


def main():
    """Entry point."""
    rclpy.init()
    node = TrackBuilder()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()
