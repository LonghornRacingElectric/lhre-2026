#!/usr/bin/env python3
"""Adapter: AckermannDriveStamped → Twist for the Gazebo Ackermann plugin.

The pure pursuit controller publishes AckermannDriveStamped on /lhr/vehicle/cmd.
Gazebo's AckermannSteering plugin expects geometry_msgs/Twist.

Conversion:
    twist.linear.x  = drive.speed
    twist.angular.z  = speed * tan(steering_angle) / wheelbase
"""

import math

import rclpy
from rclpy.node import Node

from ackermann_msgs.msg import AckermannDriveStamped
from geometry_msgs.msg import Twist


class AckermannCmdAdapter(Node):

    def __init__(self):
        super().__init__('ackermann_cmd_adapter')

        self.declare_parameter('wheelbase', 1.6)
        self._wheelbase = self.get_parameter('wheelbase').value

        self._sub = self.create_subscription(
            AckermannDriveStamped,
            '/lhr/vehicle/cmd',
            self._cmd_cb,
            10)

        self._pub = self.create_publisher(
            Twist,
            '/model/fsae_vehicle/cmd_vel',
            10)

        self.get_logger().info(
            f'Ackermann adapter ready (wheelbase={self._wheelbase:.2f}m)')

    def _cmd_cb(self, msg: AckermannDriveStamped):
        speed = msg.drive.speed
        steer = msg.drive.steering_angle

        twist = Twist()
        twist.linear.x = speed
        if abs(speed) > 0.01:
            twist.angular.z = speed * math.tan(steer) / self._wheelbase
        else:
            twist.angular.z = 0.0

        self._pub.publish(twist)


def main(args=None):
    rclpy.init(args=args)
    node = AckermannCmdAdapter()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
