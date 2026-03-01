#!/usr/bin/env python3
"""Sensor simulation: FOV-limited cone detection with accumulation."""

import math
import random

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy

from geometry_msgs.msg import Point
from nav_msgs.msg import Odometry
from std_msgs.msg import ColorRGBA
from visualization_msgs.msg import Marker, MarkerArray


def quat_to_yaw(q) -> float:
    """Extract yaw from a quaternion."""
    siny_cosp = 2.0 * (q.w * q.z + q.x * q.y)
    cosy_cosp = 1.0 - 2.0 * (q.y * q.y + q.z * q.z)
    return math.atan2(siny_cosp, cosy_cosp)


class SensorSim(Node):
    """Filter ground-truth cones by vehicle FOV, accumulate over time."""

    def __init__(self):
        super().__init__('sensor_sim')

        # --- Parameters ---
        self.declare_parameter('fov_deg', 200.0)
        self.declare_parameter('max_range_m', 20.0)
        self.declare_parameter('min_range_m', 0.5)
        self.declare_parameter('detection_hz', 10.0)
        self.declare_parameter('noise_std_m', 0.0)
        self.declare_parameter('false_negative_rate', 0.0)

        self._fov_rad = math.radians(
            self.get_parameter('fov_deg').get_parameter_value().double_value)
        self._max_range = self.get_parameter(
            'max_range_m').get_parameter_value().double_value
        self._min_range = self.get_parameter(
            'min_range_m').get_parameter_value().double_value
        detection_hz = self.get_parameter(
            'detection_hz').get_parameter_value().double_value
        self._noise_std = self.get_parameter(
            'noise_std_m').get_parameter_value().double_value
        self._fn_rate = self.get_parameter(
            'false_negative_rate').get_parameter_value().double_value

        # --- State ---
        self._all_cones: list = []          # latest ground-truth MarkerArray
        self._veh_x = 0.0
        self._veh_y = 0.0
        self._veh_yaw = 0.0
        self._have_odom = False
        self._accumulated: dict = {}        # (ns, id) -> Marker

        # --- QoS ---
        latch_qos = QoSProfile(
            depth=1,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
        )

        # --- Subscribers ---
        self.create_subscription(
            MarkerArray, '/lhr/track/cones', self._cones_cb, latch_qos)
        self.create_subscription(
            Odometry, '/lhr/vehicle/odom', self._odom_cb, 10)

        # --- Publishers ---
        self._det_pub = self.create_publisher(
            MarkerArray, '/lhr/sensor/cones_detected', latch_qos)
        self._fov_pub = self.create_publisher(
            MarkerArray, '/lhr/sensor/fov_viz', 10)
        self._viz_pub = self.create_publisher(
            MarkerArray, '/lhr/sensor/cones_viz', 10)

        # --- Timer ---
        self.create_timer(1.0 / detection_hz, self._detect)
        self.get_logger().info(
            f'SensorSim ready  (fov={math.degrees(self._fov_rad):.0f}deg, '
            f'range=[{self._min_range}, {self._max_range}]m, '
            f'hz={detection_hz})')

    # ------------------------------------------------------------------
    def _cones_cb(self, msg: MarkerArray):
        self._all_cones = msg.markers

    def _odom_cb(self, msg: Odometry):
        self._veh_x = msg.pose.pose.position.x
        self._veh_y = msg.pose.pose.position.y
        self._veh_yaw = quat_to_yaw(msg.pose.pose.orientation)
        self._have_odom = True

    # ------------------------------------------------------------------
    def _detect(self):
        if not self._have_odom or not self._all_cones:
            return

        half_fov = self._fov_rad / 2.0

        for marker in self._all_cones:
            key = (marker.ns, marker.id)
            if key in self._accumulated:
                continue  # already seen

            cx = marker.pose.position.x
            cy = marker.pose.position.y
            dx = cx - self._veh_x
            dy = cy - self._veh_y
            dist = math.hypot(dx, dy)

            if dist < self._min_range or dist > self._max_range:
                continue

            angle = math.atan2(dy, dx) - self._veh_yaw
            # normalise to [-pi, pi]
            angle = math.atan2(math.sin(angle), math.cos(angle))

            if abs(angle) > half_fov:
                continue

            # false negative
            if self._fn_rate > 0.0 and random.random() < self._fn_rate:
                continue

            # store (with optional noise)
            m = Marker()
            m.header = marker.header
            m.ns = marker.ns
            m.id = marker.id
            m.type = marker.type
            m.action = marker.action
            m.scale = marker.scale
            m.color = marker.color
            m.pose = marker.pose
            m.pose.orientation.w = 1.0

            if self._noise_std > 0.0:
                m.pose.position.x += random.gauss(0, self._noise_std)
                m.pose.position.y += random.gauss(0, self._noise_std)

            self._accumulated[key] = m

        self._publish_accumulated()
        self._publish_fov_viz()
        self._publish_cones_viz()

    # ------------------------------------------------------------------
    def _publish_accumulated(self):
        msg = MarkerArray()
        # sort by namespace then id for stable index-based pairing
        sorted_markers = sorted(
            self._accumulated.values(), key=lambda m: (m.ns, m.id))
        msg.markers = sorted_markers
        self._det_pub.publish(msg)

    def _publish_cones_viz(self):
        """Publish all cones: detected at full color, unseen dim/transparent."""
        msg = MarkerArray()
        now = self.get_clock().now().to_msg()
        for marker in self._all_cones:
            key = (marker.ns, marker.id)
            detected = key in self._accumulated

            m = Marker()
            m.header.frame_id = 'map'
            m.header.stamp = now
            m.ns = marker.ns + '_viz'
            m.id = marker.id
            m.type = marker.type
            m.action = Marker.ADD
            m.pose = marker.pose
            m.pose.orientation.w = 1.0
            m.scale = marker.scale

            if detected:
                m.color = marker.color
            else:
                # dim: same hue but low alpha and desaturated
                c = marker.color
                m.color = ColorRGBA(
                    r=c.r * 0.4 + 0.2,
                    g=c.g * 0.4 + 0.2,
                    b=c.b * 0.4 + 0.2,
                    a=0.25)

            msg.markers.append(m)
        self._viz_pub.publish(msg)

    def _publish_fov_viz(self):
        """Publish FOV frustum as a LINE_STRIP arc + two range lines."""
        half_fov = self._fov_rad / 2.0
        arc_steps = 30

        arc = Marker()
        arc.header.frame_id = 'map'
        arc.header.stamp = self.get_clock().now().to_msg()
        arc.ns = 'fov'
        arc.id = 0
        arc.type = Marker.LINE_STRIP
        arc.action = Marker.ADD
        arc.scale.x = 0.08
        arc.color = ColorRGBA(r=1.0, g=1.0, b=0.0, a=0.5)
        arc.pose.orientation.w = 1.0

        # start ray
        arc.points.append(Point(x=self._veh_x, y=self._veh_y, z=0.0))

        # outer arc
        for i in range(arc_steps + 1):
            a = self._veh_yaw - half_fov + self._fov_rad * i / arc_steps
            arc.points.append(Point(
                x=self._veh_x + self._max_range * math.cos(a),
                y=self._veh_y + self._max_range * math.sin(a),
                z=0.0))

        # close back to vehicle
        arc.points.append(Point(x=self._veh_x, y=self._veh_y, z=0.0))

        msg = MarkerArray()
        msg.markers.append(arc)
        self._fov_pub.publish(msg)


def main():
    """Entry point."""
    rclpy.init()
    node = SensorSim()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()
