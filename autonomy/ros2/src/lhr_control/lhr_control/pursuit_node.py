#!/usr/bin/env python3
"""Pure pursuit controller with curvature-based speed planning."""

import math
from typing import List, Optional, Tuple

import rclpy
from rclpy.node import Node

from ackermann_msgs.msg import AckermannDrive, AckermannDriveStamped
from geometry_msgs.msg import Point
from nav_msgs.msg import Odometry, Path
from std_msgs.msg import ColorRGBA, Float32, Header, String
from visualization_msgs.msg import Marker


def quat_to_yaw(q) -> float:
    """Extract yaw from a geometry_msgs Quaternion."""
    siny = 2.0 * (q.w * q.z + q.x * q.y)
    cosy = 1.0 - 2.0 * (q.y * q.y + q.z * q.z)
    return math.atan2(siny, cosy)


class PurePursuit(Node):
    """Pure pursuit steering + curvature-based speed planning."""

    def __init__(self):
        super().__init__('pure_pursuit')

        # --- Steering params ---
        self.declare_parameter('lookahead_dist', 4.0)
        self.declare_parameter('lookahead_min', 2.0)
        self.declare_parameter('lookahead_curvature_gain', 3.0)
        self.declare_parameter('max_steer', 0.55)
        self.declare_parameter('wheelbase', 1.6)
        self.declare_parameter('control_hz', 20.0)

        # --- Speed planning params ---
        self.declare_parameter('a_lat_max', 6.0)
        self.declare_parameter('v_min', 2.0)
        self.declare_parameter('v_max', 12.0)
        self.declare_parameter('kappa_eps', 1e-3)
        self.declare_parameter('curvature_window', 5)
        self.declare_parameter('max_accel', 2.0)
        self.declare_parameter('max_decel', 3.0)

        # Legacy param — ignored but kept so launch files don't break
        self.declare_parameter('target_speed', 5.0)

        self._ld_max = self.get_parameter(
            'lookahead_dist').get_parameter_value().double_value
        self._ld_min = self.get_parameter(
            'lookahead_min').get_parameter_value().double_value
        self._ld_curv_gain = self.get_parameter(
            'lookahead_curvature_gain').get_parameter_value().double_value
        self._max_steer = self.get_parameter(
            'max_steer').get_parameter_value().double_value
        self._L = self.get_parameter(
            'wheelbase').get_parameter_value().double_value
        control_hz = self.get_parameter(
            'control_hz').get_parameter_value().double_value

        self._a_lat_max = self.get_parameter(
            'a_lat_max').get_parameter_value().double_value
        self._v_min = self.get_parameter(
            'v_min').get_parameter_value().double_value
        self._v_max = self.get_parameter(
            'v_max').get_parameter_value().double_value
        self._kappa_eps = self.get_parameter(
            'kappa_eps').get_parameter_value().double_value
        self._curv_win = self.get_parameter(
            'curvature_window').get_parameter_value().integer_value
        self._max_accel = self.get_parameter(
            'max_accel').get_parameter_value().double_value
        self._max_decel = self.get_parameter(
            'max_decel').get_parameter_value().double_value

        self._dt = 1.0 / control_hz

        # --- State ---
        self._path: List[Tuple[float, float]] = []
        self._x = 0.0
        self._y = 0.0
        self._yaw = 0.0
        self._have_odom = False
        self._ld = self._ld_max  # current (adaptive) lookahead
        self._v_prev = 0.0  # for accel limiting
        self._mission_status = ''  # empty = no manager, run freely

        # --- Subscribers ---
        self.create_subscription(
            Path, '/lhr/track/centerline', self._path_cb, 10)
        self.create_subscription(
            Odometry, '/lhr/vehicle/odom', self._odom_cb, 10)
        self.create_subscription(
            String, '/lhr/mission/status', self._status_cb, 10)

        # --- Publishers ---
        self._cmd_pub = self.create_publisher(
            AckermannDriveStamped, '/lhr/vehicle/cmd', 10)
        self._la_pub = self.create_publisher(
            Marker, '/lhr/control/lookahead', 10)
        self._curv_pub = self.create_publisher(
            Float32, '/lhr/debug/curvature', 10)
        self._vcmd_pub = self.create_publisher(
            Float32, '/lhr/debug/v_cmd', 10)

        # --- Timer ---
        self.create_timer(self._dt, self._control_loop)
        self.get_logger().info(
            f'PurePursuit ready  (ld=[{self._ld_min}..{self._ld_max}], '
            f'v=[{self._v_min}..{self._v_max}], '
            f'a_lat_max={self._a_lat_max})')

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

    def _status_cb(self, msg: String):
        self._mission_status = msg.data

    # ------------------------------------------------------------------
    # Control
    # ------------------------------------------------------------------
    def _control_loop(self):
        # Gate on mission status — only drive when DRIVING (or no manager)
        if self._mission_status and self._mission_status != 'DRIVING':
            cmd = AckermannDriveStamped()
            cmd.header.stamp = self.get_clock().now().to_msg()
            cmd.drive = AckermannDrive()
            cmd.drive.speed = 0.0
            cmd.drive.steering_angle = 0.0
            self._cmd_pub.publish(cmd)
            self._v_prev = 0.0
            return

        if not self._path or not self._have_odom:
            return

        # --- Curvature-adaptive lookahead ---
        # First, estimate curvature near the car to shorten lookahead
        # on tight turns (reduces corner cutting).
        closest_idx = self._find_closest_idx()
        kappa_near = abs(self._estimate_curvature(closest_idx))
        # ld = ld_max - gain * |kappa|, clamped to [ld_min, ld_max]
        self._ld = max(self._ld_min, min(self._ld_max,
                       self._ld_max - self._ld_curv_gain * kappa_near))

        la = self._find_lookahead()
        if la is None:
            return

        la_idx, gx, gy = la

        # --- Steering (pure pursuit) ---
        dx = gx - self._x
        dy = gy - self._y
        local_x = math.cos(-self._yaw) * dx - math.sin(-self._yaw) * dy
        local_y = math.sin(-self._yaw) * dx + math.cos(-self._yaw) * dy

        ld_sq = local_x * local_x + local_y * local_y
        if ld_sq < 1e-6:
            return
        curvature_pp = 2.0 * local_y / ld_sq

        steer = math.atan(curvature_pp * self._L)
        steer = max(-self._max_steer, min(self._max_steer, steer))

        # --- Speed planning ---
        kappa = self._estimate_curvature(la_idx)
        v_des = math.sqrt(
            self._a_lat_max / max(abs(kappa), self._kappa_eps))
        v_des = max(self._v_min, min(self._v_max, v_des))

        # Accel limiting
        dv = v_des - self._v_prev
        dv = max(-self._max_decel * self._dt,
                 min(self._max_accel * self._dt, dv))
        v_cmd = max(self._v_min, min(self._v_max, self._v_prev + dv))
        self._v_prev = v_cmd

        # --- Publish command ---
        cmd = AckermannDriveStamped()
        cmd.header.stamp = self.get_clock().now().to_msg()
        cmd.drive = AckermannDrive()
        cmd.drive.speed = v_cmd
        cmd.drive.steering_angle = steer
        self._cmd_pub.publish(cmd)

        # --- Debug ---
        self._publish_lookahead_marker(gx, gy)
        self._curv_pub.publish(Float32(data=kappa))
        self._vcmd_pub.publish(Float32(data=v_cmd))

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------
    def _find_closest_idx(self) -> int:
        """Return index of the path point closest to the vehicle."""
        best_idx = 0
        best_d2 = float('inf')
        for i, (px, py) in enumerate(self._path):
            d2 = (px - self._x) ** 2 + (py - self._y) ** 2
            if d2 < best_d2:
                best_d2 = d2
                best_idx = i
        return best_idx

    # ------------------------------------------------------------------
    # Lookahead (returns index + point)
    # ------------------------------------------------------------------
    def _find_lookahead(self) -> Optional[Tuple[int, float, float]]:
        """Find the first path point >= lookahead_dist away.

        Returns (index, x, y) or None.  Treats path as closed loop.
        """
        n = len(self._path)
        if n == 0:
            return None

        best_idx = self._find_closest_idx()

        # Walk forward to find lookahead
        ld_sq = self._ld * self._ld
        for j in range(n):
            idx = (best_idx + j) % n
            px, py = self._path[idx]
            d2 = (px - self._x) ** 2 + (py - self._y) ** 2
            if d2 >= ld_sq:
                return (idx, px, py)

        # Fallback
        idx = (best_idx + n // 2) % n
        return (idx, self._path[idx][0], self._path[idx][1])

    # ------------------------------------------------------------------
    # Curvature estimation
    # ------------------------------------------------------------------
    def _estimate_curvature(self, idx: int) -> float:
        """Estimate curvature at path[idx] using circumcircle of 3 points.

        Uses points at idx-w, idx, idx+w (wrapped for closed loop).
        """
        n = len(self._path)
        w = self._curv_win

        ax, ay = self._path[(idx - w) % n]
        bx, by = self._path[idx]
        cx, cy = self._path[(idx + w) % n]

        # Lengths of triangle sides
        ab = math.hypot(bx - ax, by - ay)
        bc = math.hypot(cx - bx, cy - by)
        ca = math.hypot(ax - cx, ay - cy)

        # Signed area * 2 (cross product)
        cross = (bx - ax) * (cy - ay) - (by - ay) * (cx - ax)

        denom = ab * bc * ca
        if denom < 1e-12:
            return 0.0

        # kappa = 2 * signed_area / (ab * bc * ca)
        return 2.0 * cross / denom

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
