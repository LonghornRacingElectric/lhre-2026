#!/usr/bin/env python3
"""Track builder node: subscribes to cones, publishes centerline Path."""

from typing import List, Tuple

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy

from builtin_interfaces.msg import Time
from geometry_msgs.msg import PoseStamped, Point
from nav_msgs.msg import Path
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

        # --- Stored cone positions keyed by marker ID ---
        self._left_cones: dict = {}
        self._right_cones: dict = {}

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

        # --- Subscriber ---
        self.create_subscription(
            MarkerArray, cone_topic, self._cones_cb, sub_qos)

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
        """Extract left/right cone positions from the MarkerArray, keyed by ID."""
        left: dict = {}
        right: dict = {}
        for marker in msg.markers:
            pos = marker.pose.position
            if marker.ns == 'left_cones':
                left[marker.id] = (pos.x, pos.y)
            elif marker.ns == 'right_cones':
                right[marker.id] = (pos.x, pos.y)
        self._left_cones = left
        self._right_cones = right

    def _on_timer(self):
        """Compute centerline and publish Path + debug markers."""
        if not self._left_cones or not self._right_cones:
            return  # wait for cones

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
        """Pair left/right cones and return midpoints.

        Strategy 'index': match left ID ``i`` with right ID ``10000 + i``.
        Strategy 'nearest': pair each left cone with its nearest right cone.
        """
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
        """Pair each left cone with the nearest unpaired right cone."""
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

        return midpoints

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
