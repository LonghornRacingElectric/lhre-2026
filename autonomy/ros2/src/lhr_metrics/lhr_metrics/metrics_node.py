#!/usr/bin/env python3
"""Metrics v0: CTE, off-track count, lap detection, speed stats, CSV output."""

import csv
import math
import os
import time
from typing import List, Tuple

import rclpy
from rclpy.node import Node

from nav_msgs.msg import Odometry, Path
from std_msgs.msg import Bool


CSV_HEADER = [
    'run_id', 'duration_s', 'samples', 'mean_cte', 'max_cte',
    'off_track_count', 'mean_speed', 'max_speed', 'lap_completed',
]


class MetricsNode(Node):
    """Compute CTE stats, speed stats, and detect lap completion."""

    def __init__(self):
        super().__init__('metrics_node')

        # --- Parameters ---
        self.declare_parameter('off_track_threshold', 2.0)
        self.declare_parameter('start_radius', 2.0)
        self.declare_parameter('start_hysteresis', 1.0)
        self.declare_parameter('min_lap_time', 5.0)
        self.declare_parameter('output_csv', 'data/metrics.csv')
        self.declare_parameter('run_id', '')

        self._off_track_thresh = self.get_parameter(
            'off_track_threshold').get_parameter_value().double_value
        self._start_radius = self.get_parameter(
            'start_radius').get_parameter_value().double_value
        self._start_hyst = self.get_parameter(
            'start_hysteresis').get_parameter_value().double_value
        self._min_lap_time = self.get_parameter(
            'min_lap_time').get_parameter_value().double_value
        self._csv_path = self.get_parameter(
            'output_csv').get_parameter_value().string_value
        run_id = self.get_parameter(
            'run_id').get_parameter_value().string_value
        self._run_id = run_id if run_id else time.strftime('%Y%m%d_%H%M%S')

        # --- Centerline cache ---
        self._path: List[Tuple[float, float]] = []

        # --- CTE stats ---
        self._samples = 0
        self._cte_sum = 0.0
        self._cte_max = 0.0
        self._off_track_count = 0
        self._start_time: float = 0.0

        # --- Speed stats ---
        self._speed_sum = 0.0
        self._speed_max = 0.0

        # --- Lap detection state ---
        self._lap_completed = False
        self._near_start = False
        self._left_start = False
        self._lap_start_time: float = 0.0

        # --- Publisher ---
        self._lap_pub = self.create_publisher(
            Bool, '/lhr/metrics/lap_complete', 10)

        # --- Subscribers ---
        self.create_subscription(
            Path, '/lhr/track/centerline', self._path_cb, 10)
        self.create_subscription(
            Odometry, '/lhr/vehicle/odom', self._odom_cb, 10)

        self.get_logger().info(
            f'Metrics v0 ready  (run_id={self._run_id}, '
            f'off_track>{self._off_track_thresh}m)')

    # ------------------------------------------------------------------
    # Callbacks
    # ------------------------------------------------------------------
    def _path_cb(self, msg: Path):
        self._path = [
            (ps.pose.position.x, ps.pose.position.y)
            for ps in msg.poses
        ]

    def _odom_cb(self, msg: Odometry):
        if not self._path:
            return

        px = msg.pose.pose.position.x
        py = msg.pose.pose.position.y

        now = time.monotonic()
        if self._samples == 0:
            self._start_time = now
            self._lap_start_time = now

        # --- CTE ---
        cte = self._nearest_distance(px, py)
        self._samples += 1
        self._cte_sum += cte
        if cte > self._cte_max:
            self._cte_max = cte
        if cte > self._off_track_thresh:
            self._off_track_count += 1

        # --- Speed ---
        vx = msg.twist.twist.linear.x
        vy = msg.twist.twist.linear.y
        speed = math.hypot(vx, vy)
        self._speed_sum += speed
        if speed > self._speed_max:
            self._speed_max = speed

        # --- Lap detection ---
        if not self._lap_completed and len(self._path) > 1:
            self._update_lap_detection(px, py, now)

    # ------------------------------------------------------------------
    # Geometry
    # ------------------------------------------------------------------
    def _nearest_distance(self, px: float, py: float) -> float:
        """Distance from (px, py) to nearest point on centerline."""
        best = float('inf')
        for cx, cy in self._path:
            d2 = (px - cx) ** 2 + (py - cy) ** 2
            if d2 < best:
                best = d2
        return math.sqrt(best)

    # ------------------------------------------------------------------
    # Lap detection
    # ------------------------------------------------------------------
    def _update_lap_detection(self, px: float, py: float, now: float):
        sx, sy = self._path[0]
        dist = math.hypot(px - sx, py - sy)

        in_zone = dist < self._start_radius
        beyond = dist > (self._start_radius + self._start_hyst)

        if not self._left_start:
            if beyond:
                self._left_start = True
                self._lap_start_time = now
        else:
            elapsed = now - self._lap_start_time
            if in_zone and elapsed > self._min_lap_time:
                self._lap_completed = True
                self._lap_pub.publish(Bool(data=True))
                self.get_logger().info('Lap completed!')
                self._print_summary()
                self._write_csv()

    # ------------------------------------------------------------------
    # Reporting
    # ------------------------------------------------------------------
    def _build_row(self) -> dict:
        duration = time.monotonic() - self._start_time if self._samples else 0.0
        mean_cte = (self._cte_sum / self._samples) if self._samples else 0.0
        mean_speed = (self._speed_sum / self._samples) if self._samples else 0.0
        return {
            'run_id': self._run_id,
            'duration_s': f'{duration:.2f}',
            'samples': str(self._samples),
            'mean_cte': f'{mean_cte:.4f}',
            'max_cte': f'{self._cte_max:.4f}',
            'off_track_count': str(self._off_track_count),
            'mean_speed': f'{mean_speed:.2f}',
            'max_speed': f'{self._speed_max:.2f}',
            'lap_completed': str(self._lap_completed).lower(),
        }

    def _print_summary(self):
        row = self._build_row()
        self.get_logger().info('--- Metrics Summary ---')
        for k, v in row.items():
            self.get_logger().info(f'  {k}: {v}')

    def _write_csv(self):
        csv_dir = os.path.dirname(self._csv_path)
        if csv_dir:
            os.makedirs(csv_dir, exist_ok=True)

        write_header = not os.path.exists(self._csv_path)
        row = self._build_row()

        with open(self._csv_path, 'a', newline='') as f:
            writer = csv.DictWriter(f, fieldnames=CSV_HEADER)
            if write_header:
                writer.writeheader()
            writer.writerow(row)

        self.get_logger().info(f'CSV row appended to {self._csv_path}')

    def on_shutdown(self):
        """Called on Ctrl+C — always dump a summary."""
        if self._samples > 0:
            self._print_summary()
            self._write_csv()


def main():
    """Entry point."""
    rclpy.init()
    node = MetricsNode()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.on_shutdown()
    node.destroy_node()
    rclpy.shutdown()
