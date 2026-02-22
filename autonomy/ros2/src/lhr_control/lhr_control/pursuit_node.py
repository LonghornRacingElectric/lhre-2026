#!/usr/bin/env python3
"""Pure pursuit controller: follow a Path with Ackermann commands."""

import math
from typing import List, Optional, Tuple

import rclpy
from rclpy.node import Node

from ackermann_msgs.msg import AckermannDrive, AckermannDriveStamped
from geometry_msgs.msg import Point
from nav_msgs.msg import Odometry, Path
from std_msgs.msg import ColorRGBA, Header
from visualization_msgs.msg import Marker


def quat_to_yaw(q) -> float:
    """Extract yaw from a geometry_msgs Quaternion."""
    siny = 2.0 * (q.w * q.z + q.x * q.y)
    cosy = 1.0 - 2.0 * (q.y * q.y + q.z * q.z)
    return math.atan2(siny, cosy)


class PurePursuit(Node):
    """Pure pursuit controller node."""

    def __init__(self):
        super().__init__('pure_pursuit')

        # --- Parameters ---
        self.declare_parameter('lookahead_dist', 4.0)
        self.declare_parameter('target_speed', 5.0)
        self.declare_parameter('max_steer', 0.45)
        self.declare_parameter('wheelbase', 1.6)
        self.declare_parameter('control_hz', 20.0)

        self._ld = self.get_parameter(
            'lookahead_dist').get_parameter_value().double_value
        self._target_speed = self.get_parameter(
            'target_speed').get_parameter_value().double_value
        self._max_steer = self.get_parameter(
            'max_steer').get_parameter_value().double_value
        self._L = self.get_parameter(
            'wheelbase').get_parameter_value().double_value
        control_hz = self.get_parameter(
            'control_hz').get_parameter_value().double_value

        # --- State ---
        self._path: List[Tuple[float, float]] = []
        self._x = 0.0
        self._y = 0.0
        self._yaw = 0.0
        self._have_odom = False

        # --- Subscribers ---
        self.create_subscription(
            Path, '/lhr/track/centerline', self._path_cb, 10)
        self.create_subscription(
            Odometry, '/lhr/vehicle/odom', self._odom_cb, 10)

        # --- Publishers ---
        self._cmd_pub = self.create_publisher(
            AckermannDriveStamped, '/lhr/vehicle/cmd', 10)
        self._la_pub = self.create_publisher(
            Marker, '/lhr/control/lookahead', 10)

        # --- Timer ---
        self.create_timer(1.0 / control_hz, self._control_loop)
        self.get_logger().info(
            f'PurePursuit ready  (ld={self._ld}, speed={self._target_speed})')

    # ------------------------------------------------------------------
    # Callbacks
    # ------------------------------------------------------------------
    def _path_cb(self, msg: Path):
        self._path = [
            (ps.pose.position.x, ps.pose.position.y)
            for ps in msg.poses
        ]

    def _odom_cb(self, msg: Odometry):
        self._x = msg.pose.pose.position.x
        self._y = msg.pose.pose.position.y
        self._yaw = quat_to_yaw(msg.pose.pose.orientation)
        self._have_odom = True

    # ------------------------------------------------------------------
    # Control
    # ------------------------------------------------------------------
    def _control_loop(self):
        if not self._path or not self._have_odom:
            return

        goal = self._find_lookahead()
        if goal is None:
            return

        gx, gy = goal

        # Transform goal into vehicle frame
        dx = gx - self._x
        dy = gy - self._y
        local_x = math.cos(-self._yaw) * dx - math.sin(-self._yaw) * dy
        local_y = math.sin(-self._yaw) * dx + math.cos(-self._yaw) * dy

        # Pure pursuit curvature: kappa = 2 * local_y / ld^2
        ld_sq = local_x * local_x + local_y * local_y
        if ld_sq < 1e-6:
            return
        curvature = 2.0 * local_y / ld_sq

        # Steering angle: delta = atan(kappa * L)
        steer = math.atan(curvature * self._L)
        steer = max(-self._max_steer, min(self._max_steer, steer))

        # Publish command
        cmd = AckermannDriveStamped()
        cmd.header.stamp = self.get_clock().now().to_msg()
        cmd.drive = AckermannDrive()
        cmd.drive.speed = self._target_speed
        cmd.drive.steering_angle = steer
        self._cmd_pub.publish(cmd)

        # Publish lookahead marker
        self._publish_lookahead_marker(gx, gy)

    def _find_lookahead(self) -> Optional[Tuple[float, float]]:
        """Find the first path point at least lookahead_dist away.

        Treats the path as a closed loop.
        """
        n = len(self._path)
        if n == 0:
            return None

        # Find closest point on path
        best_idx = 0
        best_dist_sq = float('inf')
        for i, (px, py) in enumerate(self._path):
            d2 = (px - self._x) ** 2 + (py - self._y) ** 2
            if d2 < best_dist_sq:
                best_dist_sq = d2
                best_idx = i

        # Walk forward from closest point to find lookahead
        ld_sq = self._ld * self._ld
        for j in range(n):
            idx = (best_idx + j) % n
            px, py = self._path[idx]
            d2 = (px - self._x) ** 2 + (py - self._y) ** 2
            if d2 >= ld_sq:
                return (px, py)

        # Fallback: use the point farthest along from closest
        return self._path[(best_idx + n // 2) % n]

    # ------------------------------------------------------------------
    # Debug visualisation
    # ------------------------------------------------------------------
    def _publish_lookahead_marker(self, gx: float, gy: float):
        m = Marker()
        m.header = Header()
        m.header.stamp = self.get_clock().now().to_msg()
        m.header.frame_id = 'map'
        m.ns = 'lookahead'
        m.id = 0
        m.type = Marker.SPHERE
        m.action = Marker.ADD
        m.pose.position = Point(x=gx, y=gy, z=0.3)
        m.pose.orientation.w = 1.0
        m.scale.x = 0.4
        m.scale.y = 0.4
        m.scale.z = 0.4
        m.color = ColorRGBA(r=1.0, g=0.0, b=1.0, a=1.0)
        self._la_pub.publish(m)


def main():
    """Entry point."""
    rclpy.init()
    node = PurePursuit()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()
