#!/usr/bin/env python3
"""Kinematic bicycle-model vehicle simulator."""

import math

import rclpy
from rclpy.node import Node

from ackermann_msgs.msg import AckermannDriveStamped
from geometry_msgs.msg import (
    Quaternion, TransformStamped, Twist, Vector3,
)
from nav_msgs.msg import Odometry
from tf2_ros import TransformBroadcaster


def yaw_to_quat(yaw: float) -> Quaternion:
    """Convert a yaw angle (rad) to a z-axis quaternion."""
    q = Quaternion()
    q.z = math.sin(yaw / 2.0)
    q.w = math.cos(yaw / 2.0)
    return q


class SimKinematic(Node):
    """Integrate a kinematic bicycle model and publish odom + TF."""

    def __init__(self):
        super().__init__('sim_kinematic')

        # --- Parameters ---
        self.declare_parameter('wheelbase', 1.6)
        self.declare_parameter('update_hz', 50.0)
        self.declare_parameter('max_steer', 0.45)
        self.declare_parameter('max_speed', 15.0)
        self.declare_parameter('frame_id', 'map')
        self.declare_parameter('child_frame_id', 'base_link')
        self.declare_parameter('init_x', 0.0)
        self.declare_parameter('init_y', 0.0)
        self.declare_parameter('init_yaw', 0.0)

        self._L = self.get_parameter(
            'wheelbase').get_parameter_value().double_value
        update_hz = self.get_parameter(
            'update_hz').get_parameter_value().double_value
        self._max_steer = self.get_parameter(
            'max_steer').get_parameter_value().double_value
        self._max_speed = self.get_parameter(
            'max_speed').get_parameter_value().double_value
        self._frame_id = self.get_parameter(
            'frame_id').get_parameter_value().string_value
        self._child_frame_id = self.get_parameter(
            'child_frame_id').get_parameter_value().string_value

        # --- State ---
        self._x = self.get_parameter(
            'init_x').get_parameter_value().double_value
        self._y = self.get_parameter(
            'init_y').get_parameter_value().double_value
        self._yaw = self.get_parameter(
            'init_yaw').get_parameter_value().double_value
        self._v = 0.0
        self._steer = 0.0

        # --- Command subscriber ---
        self.create_subscription(
            AckermannDriveStamped, '/lhr/vehicle/cmd',
            self._cmd_cb, 10)

        # --- Odom publisher ---
        self._odom_pub = self.create_publisher(
            Odometry, '/lhr/vehicle/odom', 10)

        # --- TF broadcaster ---
        self._tf_bc = TransformBroadcaster(self)

        # --- Timer ---
        self._dt = 1.0 / update_hz
        self.create_timer(self._dt, self._step)
        self.get_logger().info(
            f'SimKinematic ready  (L={self._L}, hz={update_hz})')

    # ------------------------------------------------------------------
    def _cmd_cb(self, msg: AckermannDriveStamped):
        self._v = max(-self._max_speed,
                      min(self._max_speed, msg.drive.speed))
        self._steer = max(-self._max_steer,
                          min(self._max_steer,
                              msg.drive.steering_angle))

    # ------------------------------------------------------------------
    def _step(self):
        """Integrate one timestep and publish."""
        # Kinematic bicycle model
        dt = self._dt
        self._x += self._v * math.cos(self._yaw) * dt
        self._y += self._v * math.sin(self._yaw) * dt
        self._yaw += (self._v / self._L) * math.tan(self._steer) * dt

        now = self.get_clock().now().to_msg()

        # --- Odometry ---
        odom = Odometry()
        odom.header.stamp = now
        odom.header.frame_id = self._frame_id
        odom.child_frame_id = self._child_frame_id

        odom.pose.pose.position.x = self._x
        odom.pose.pose.position.y = self._y
        odom.pose.pose.orientation = yaw_to_quat(self._yaw)

        odom.twist.twist.linear = Vector3(
            x=self._v * math.cos(self._yaw),
            y=self._v * math.sin(self._yaw),
            z=0.0)
        odom.twist.twist.angular = Vector3(
            x=0.0, y=0.0,
            z=(self._v / self._L) * math.tan(self._steer))

        self._odom_pub.publish(odom)

        # --- TF: map → base_link ---
        t = TransformStamped()
        t.header.stamp = now
        t.header.frame_id = self._frame_id
        t.child_frame_id = self._child_frame_id
        t.transform.translation.x = self._x
        t.transform.translation.y = self._y
        t.transform.rotation = yaw_to_quat(self._yaw)
        self._tf_bc.sendTransform(t)


def main():
    """Entry point."""
    rclpy.init()
    node = SimKinematic()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()
