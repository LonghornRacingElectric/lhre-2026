#!/usr/bin/env python3
"""Adapter: Gazebo LogicalCameraImage → MarkerArray for the track builder.

The Gazebo logical camera detects models by name and reports their pose
relative to the camera. This node converts those detections to the same
MarkerArray format that lhr_sensor_sim produces, preserving the topic
contract that lhr_track_builder depends on:

  - ns = 'left_cones' for blue cones, 'right_cones' for yellow
  - id = index for left, 10000 + index for right
  - Sorted by (ns, id)
  - TRANSIENT_LOCAL QoS

Model name convention from the world generator:
  cone_blue_<i>    → left cone, index i
  cone_yellow_<i>  → right cone, index i

Detections are accumulated (once seen, always kept) to match the existing
sensor sim's persistent-mapping behaviour.
"""

import math
import re

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy

from ros_gz_interfaces.msg import LogicalCameraImage
from visualization_msgs.msg import Marker, MarkerArray


# Regex to parse model names: "cone_blue_3" → ("blue", 3)
_CONE_RE = re.compile(r'^cone_(blue|yellow)_(\d+)$')

# Marker colour lookup
_COLORS = {
    'blue':   (0.0, 0.2, 1.0, 1.0),
    'yellow': (1.0, 1.0, 0.0, 1.0),
}

# Namespace mapping
_NS = {
    'blue':   'left_cones',
    'yellow': 'right_cones',
}

# ID offset: yellow (right) cones start at 10000
_ID_OFFSET = {
    'blue':   0,
    'yellow': 10000,
}

LATCHED_QOS = QoSProfile(
    reliability=ReliabilityPolicy.RELIABLE,
    durability=DurabilityPolicy.TRANSIENT_LOCAL,
    depth=1,
)


class LogicalCameraAdapter(Node):

    def __init__(self):
        super().__init__('logical_camera_adapter')

        self.declare_parameter('detection_hz', 10.0)
        hz = self.get_parameter('detection_hz').value

        # Accumulated cone detections: (ns, id) → Marker
        self._accumulated: dict[tuple, Marker] = {}

        self._sub = self.create_subscription(
            LogicalCameraImage,
            '/gz/logical_camera',
            self._camera_cb,
            10)

        self._pub_detected = self.create_publisher(
            MarkerArray,
            '/lhr/sensor/cones_detected',
            LATCHED_QOS)

        self._pub_viz = self.create_publisher(
            MarkerArray,
            '/lhr/sensor/cones_viz',
            10)

        self._timer = self.create_timer(1.0 / hz, self._publish)

        self._latest_msg: LogicalCameraImage | None = None

        self.get_logger().info('Logical camera adapter ready')

    def _camera_cb(self, msg: LogicalCameraImage):
        """Store latest camera message for processing on the timer."""
        self._latest_msg = msg

    def _publish(self):
        msg = self._latest_msg
        if msg is None:
            return

        # Camera pose in world frame (from the message header)
        cam_pos = msg.pose.position
        cam_ori = msg.pose.orientation

        # Convert camera quaternion to yaw for 2D projection
        # (camera is mounted on a ground vehicle, pitch/roll ≈ 0)
        cam_yaw = _quat_to_yaw(cam_ori.x, cam_ori.y, cam_ori.z, cam_ori.w)

        for model in msg.model:
            m = _CONE_RE.match(model.name)
            if m is None:
                continue

            color_name = m.group(1)
            index = int(m.group(2))
            ns = _NS[color_name]
            marker_id = _ID_OFFSET[color_name] + index

            key = (ns, marker_id)
            if key in self._accumulated:
                continue  # already detected

            # Model pose is relative to the camera — transform to world frame
            rel = model.pose.position
            wx, wy = _transform_to_world(
                rel.x, rel.y, cam_pos.x, cam_pos.y, cam_yaw)

            marker = Marker()
            marker.header.frame_id = 'map'
            marker.header.stamp = self.get_clock().now().to_msg()
            marker.ns = ns
            marker.id = marker_id
            marker.type = Marker.SPHERE
            marker.action = Marker.ADD
            marker.pose.position.x = wx
            marker.pose.position.y = wy
            marker.pose.position.z = 0.0
            marker.pose.orientation.w = 1.0
            marker.scale.x = 0.35
            marker.scale.y = 0.35
            marker.scale.z = 0.35
            r, g, b, a = _COLORS[color_name]
            marker.color.r = r
            marker.color.g = g
            marker.color.b = b
            marker.color.a = a

            self._accumulated[key] = marker

        # Publish accumulated detections sorted by (ns, id)
        if self._accumulated:
            det_msg = MarkerArray()
            det_msg.markers = [
                self._accumulated[k]
                for k in sorted(self._accumulated.keys())
            ]
            # Update timestamps
            now = self.get_clock().now().to_msg()
            for mk in det_msg.markers:
                mk.header.stamp = now
            self._pub_detected.publish(det_msg)

            # Publish visualization (all accumulated cones at full brightness)
            self._pub_viz.publish(det_msg)


def _quat_to_yaw(x, y, z, w):
    """Extract yaw from a quaternion (assumes near-zero roll/pitch)."""
    siny = 2.0 * (w * z + x * y)
    cosy = 1.0 - 2.0 * (y * y + z * z)
    return math.atan2(siny, cosy)


def _transform_to_world(rel_x, rel_y, cam_x, cam_y, cam_yaw):
    """Transform a point from camera frame to world frame (2D)."""
    cos_y = math.cos(cam_yaw)
    sin_y = math.sin(cam_yaw)
    wx = cam_x + cos_y * rel_x - sin_y * rel_y
    wy = cam_y + sin_y * rel_x + cos_y * rel_y
    return wx, wy


def main(args=None):
    rclpy.init(args=args)
    node = LogicalCameraAdapter()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
