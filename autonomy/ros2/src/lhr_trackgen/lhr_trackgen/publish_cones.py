#!/usr/bin/env python3

import math
import random
from typing import List, Tuple

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy

from visualization_msgs.msg import Marker, MarkerArray
from geometry_msgs.msg import Point


def generate_simple_track(seed: int = 0) -> Tuple[List[Tuple[float, float]], List[Tuple[float, float]]]:
    """
    Generate a simple closed-ish autocross-like path and produce left/right cones.
    This is NOT physics-accurate; it's just enough to stand up the pipeline.

    Returns:
      left_cones:  [(x,y), ...]
      right_cones: [(x,y), ...]
    """
    rng = random.Random(seed)

    # Centerline: parametric "squashed oval" with a wobble
    n = 80
    a = 20.0
    b = 12.0
    wobble = 2.0

    center = []
    for i in range(n):
        t = 2.0 * math.pi * i / n
        x = a * math.cos(t) + wobble * math.cos(3 * t)
        y = b * math.sin(t) + wobble * math.sin(2 * t)
        center.append((x, y))

    # Track width
    width = 3.5  # meters (>= 3m per typical constraint)
    half = width / 2.0

    # Compute left/right offsets using simple finite differences for tangent
    left, right = [], []
    for i in range(n):
        x0, y0 = center[i - 1]
        x1, y1 = center[i]
        x2, y2 = center[(i + 1) % n]

        # tangent approx
        tx = x2 - x0
        ty = y2 - y0
        norm = math.hypot(tx, ty) or 1.0
        tx /= norm
        ty /= norm

        # normal (left-hand)
        nx = -ty
        ny = tx

        # add slight randomization to mimic cone placement noise
        jitter = rng.uniform(-0.05, 0.05)

        lx = x1 + (half + jitter) * nx
        ly = y1 + (half + jitter) * ny
        rx = x1 - (half + jitter) * nx
        ry = y1 - (half + jitter) * ny

        # downsample cones (not every point gets a cone)
        if i % 2 == 0:
            left.append((lx, ly))
            right.append((rx, ry))

    return left, right


class ConePublisher(Node):
    def __init__(self):
        super().__init__("cone_publisher")

        self.declare_parameter("seed", 1)
        self.declare_parameter("frame_id", "map")
        self.declare_parameter("publish_hz", 5.0)

        self.seed = int(self.get_parameter("seed").value)
        self.frame_id = str(self.get_parameter("frame_id").value)
        hz = float(self.get_parameter("publish_hz").value)

        qos = QoSProfile(depth=1)
        qos.reliability = ReliabilityPolicy.RELIABLE
        qos.durability = DurabilityPolicy.TRANSIENT_LOCAL  # RViz friendly (latched)

        self.pub = self.create_publisher(MarkerArray, "/lhr/track/cones", qos)

        self.left, self.right = generate_simple_track(self.seed)

        period = 1.0 / max(hz, 0.1)
        self.timer = self.create_timer(period, self.on_timer)

        self.get_logger().info(
            f"Publishing {len(self.left)} left cones and {len(self.right)} right cones on /lhr/track/cones "
            f"(frame_id={self.frame_id}, seed={self.seed})"
        )

    def make_cone_marker(self, mid: int, ns: str, x: float, y: float, r: float, g: float, b: float) -> Marker:
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
        arr = MarkerArray()

        # Left cones: blue
        for i, (x, y) in enumerate(self.left):
            arr.markers.append(self.make_cone_marker(i, "left_cones", x, y, 0.0, 0.2, 1.0))

        # Right cones: yellow
        base = 10000
        for i, (x, y) in enumerate(self.right):
            arr.markers.append(self.make_cone_marker(base + i, "right_cones", x, y, 1.0, 1.0, 0.0))

        self.pub.publish(arr)


def main():
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
