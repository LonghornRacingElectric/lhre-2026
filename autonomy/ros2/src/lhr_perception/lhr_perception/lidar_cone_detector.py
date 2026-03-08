#!/usr/bin/env python3
"""LiDAR-based cone detector for FSAE driverless.

Subscribes to a PointCloud2 topic from Gazebo's gpu_lidar sensor,
clusters the pointcloud to find cone-sized objects, and publishes
an unclassified MarkerArray of all detected cones.

Output contract (consumed by lhr_track_builder):
  - Topic: /lhr/sensor/cones_detected (MarkerArray)
  - QoS: RELIABLE + TRANSIENT_LOCAL, depth 1
  - Namespace: "cones" (ids 0..N-1)
  - Markers: SPHERE type, scale 0.35, frame_id "map"

Left/right classification is NOT performed here — the LiDAR has no
colour information.  The track_builder's 'boundary' pairing strategy
handles centerline construction from unclassified cones.
"""

import math

import numpy as np
from scipy.spatial import cKDTree

import rclpy
from rclpy.node import Node
from rclpy.qos import QoSProfile, ReliabilityPolicy, DurabilityPolicy

from nav_msgs.msg import Odometry
from sensor_msgs.msg import PointCloud2
from std_msgs.msg import ColorRGBA
from visualization_msgs.msg import Marker, MarkerArray

from sensor_msgs_py import point_cloud2


def _quat_to_yaw(q) -> float:
    """Extract yaw from a quaternion (assumes near-zero roll/pitch)."""
    siny = 2.0 * (q.w * q.z + q.x * q.y)
    cosy = 1.0 - 2.0 * (q.y * q.y + q.z * q.z)
    return math.atan2(siny, cosy)


class LidarConeDetector(Node):

    # LiDAR mount offset relative to rear axle (vehicle base_link frame).
    # Chassis link is at (0.8, 0, 0.35), sensor pose within chassis is
    # (1.0, 0, 0.2), so total offset from rear axle: (1.8, 0, 0.55).
    SENSOR_X_OFFSET = 1.8
    SENSOR_Y_OFFSET = 0.0

    def __init__(self):
        super().__init__('lidar_cone_detector')

        # --- Parameters ---
        self.declare_parameter('max_range', 20.0)
        self.declare_parameter('min_range', 0.9)
        self.declare_parameter('ground_z_min', -0.40)
        self.declare_parameter('ground_z_max', 0.5)
        self.declare_parameter('cluster_radius', 0.35)
        self.declare_parameter('min_cluster_points', 1)
        self.declare_parameter('max_cluster_extent', 0.5)
        self.declare_parameter('max_cluster_points', 50)
        self.declare_parameter('dedup_radius', 1.5)
        self.declare_parameter('publish_hz', 10.0)

        self._max_range = self.get_parameter('max_range').value
        self._min_range = self.get_parameter('min_range').value
        self._ground_z_min = self.get_parameter('ground_z_min').value
        self._ground_z_max = self.get_parameter('ground_z_max').value
        self._cluster_radius = self.get_parameter('cluster_radius').value
        self._min_cluster_pts = self.get_parameter('min_cluster_points').value
        self._max_cluster_extent = self.get_parameter('max_cluster_extent').value
        self._max_cluster_pts = self.get_parameter('max_cluster_points').value
        self._dedup_radius = self.get_parameter('dedup_radius').value
        self._dedup_radius_sq = self._dedup_radius ** 2
        publish_hz = self.get_parameter('publish_hz').value

        # --- State ---
        self._veh_x = 0.0
        self._veh_y = 0.0
        self._veh_yaw = 0.0
        self._have_odom = False
        self._latest_cloud: PointCloud2 | None = None

        # Accumulated cone positions in map frame: [x, y, observation_count].
        self._cone_map: list[list[float]] = []

        # --- QoS ---
        latch_qos = QoSProfile(
            depth=1,
            reliability=ReliabilityPolicy.RELIABLE,
            durability=DurabilityPolicy.TRANSIENT_LOCAL,
        )

        # --- Subscribers ---
        self.create_subscription(
            PointCloud2, '/lhr/lidar/points', self._cloud_cb, 10)
        self.create_subscription(
            Odometry, '/lhr/vehicle/odom', self._odom_cb, 10)

        # --- Publishers ---
        self._det_pub = self.create_publisher(
            MarkerArray, '/lhr/sensor/cones_detected', latch_qos)
        self._debug_pub = self.create_publisher(
            MarkerArray, '/lhr/perception/debug', 10)

        # --- Timer ---
        self.create_timer(1.0 / publish_hz, self._process)
        self.get_logger().info(
            f'LidarConeDetector ready  (range=[{self._min_range}, '
            f'{self._max_range}]m, cluster_r={self._cluster_radius}m)')

    # ------------------------------------------------------------------
    # Callbacks
    # ------------------------------------------------------------------
    def _cloud_cb(self, msg: PointCloud2):
        self._latest_cloud = msg

    def _odom_cb(self, msg: Odometry):
        self._veh_x = msg.pose.pose.position.x
        self._veh_y = msg.pose.pose.position.y
        self._veh_yaw = _quat_to_yaw(msg.pose.pose.orientation)
        self._have_odom = True

    # ------------------------------------------------------------------
    # Main processing loop
    # ------------------------------------------------------------------
    def _process(self):
        if not self._have_odom or self._latest_cloud is None:
            return

        cloud = self._latest_cloud
        self._latest_cloud = None  # consume

        # Step 1: Deserialize to numpy
        points = point_cloud2.read_points_numpy(
            cloud, field_names=('x', 'y', 'z'))
        if len(points) == 0:
            return

        # Step 2: Ground removal (height filter in sensor frame)
        z = points[:, 2]
        height_mask = (z > self._ground_z_min) & (z < self._ground_z_max)
        points = points[height_mask]
        if len(points) == 0:
            return

        # Step 3: Vehicle exclusion zone (filter out the car's own body)
        # In sensor frame, the car body extends roughly:
        #   x: -2.1 to +0.1  (sensor is near front of 2.2m chassis)
        #   y: -0.7 to +0.7  (wheels at ±0.6, plus margin)
        car_mask = ~(
            (points[:, 0] > -2.3) & (points[:, 0] < 0.3) &
            (points[:, 1] > -0.8) & (points[:, 1] < 0.8)
        )
        points = points[car_mask]
        if len(points) == 0:
            return

        # Step 4: Range filter (2D distance from sensor)
        xy = points[:, :2]
        ranges = np.linalg.norm(xy, axis=1)
        range_mask = (ranges > self._min_range) & (ranges < self._max_range)
        points = points[range_mask]
        xy = points[:, :2]
        if len(xy) == 0:
            return

        # Step 5: Euclidean clustering
        clusters = self._cluster(xy)

        # Step 6–8: Validate, transform, dedup
        new_cones = 0
        for cluster_xy in clusters:
            if not self._is_cone(cluster_xy):
                continue

            centroid = cluster_xy.mean(axis=0)
            sx, sy = float(centroid[0]), float(centroid[1])

            # Transform to map frame
            mx, my = self._sensor_to_map(sx, sy)

            # Dedup / merge against accumulated map
            if self._try_merge(mx, my, self._cone_map):
                continue

            # New cone
            self._cone_map.append([mx, my, 1.0])
            new_cones += 1

        if new_cones > 0:
            self.get_logger().info(
                f'+{new_cones} cones  (total: {len(self._cone_map)})')

        self._publish_accumulated()
        self._publish_debug()

    # ------------------------------------------------------------------
    # Clustering
    # ------------------------------------------------------------------
    def _cluster(self, xy: np.ndarray) -> list[np.ndarray]:
        """Cluster 2D points using cKDTree radius queries."""
        if len(xy) < self._min_cluster_pts:
            return []

        tree = cKDTree(xy)
        visited = np.zeros(len(xy), dtype=bool)
        clusters: list[np.ndarray] = []

        for i in range(len(xy)):
            if visited[i]:
                continue
            indices = tree.query_ball_point(xy[i], self._cluster_radius)
            if len(indices) < self._min_cluster_pts:
                visited[i] = True
                continue
            visited[indices] = True
            clusters.append(xy[indices])

        return clusters

    def _is_cone(self, cluster_xy: np.ndarray) -> bool:
        """Check if a cluster matches expected cone dimensions."""
        if len(cluster_xy) > self._max_cluster_pts:
            return False
        extent = cluster_xy.max(axis=0) - cluster_xy.min(axis=0)
        if extent[0] > self._max_cluster_extent:
            return False
        if extent[1] > self._max_cluster_extent:
            return False
        return True

    # ------------------------------------------------------------------
    # Coordinate transforms
    # ------------------------------------------------------------------
    def _sensor_to_map(self, sx: float, sy: float) -> tuple[float, float]:
        """Transform a point from sensor frame to map frame."""
        # Sensor → vehicle base_link
        vx = sx + self.SENSOR_X_OFFSET
        vy = sy + self.SENSOR_Y_OFFSET

        # Vehicle → map
        cos_y = math.cos(self._veh_yaw)
        sin_y = math.sin(self._veh_yaw)
        mx = self._veh_x + vx * cos_y - vy * sin_y
        my = self._veh_y + vx * sin_y + vy * cos_y
        return mx, my

    # ------------------------------------------------------------------
    # Deduplication with running average
    # ------------------------------------------------------------------
    def _try_merge(self, mx: float, my: float,
                   cone_map: list[list[float]]) -> bool:
        """Try to merge into the nearest existing cone in *cone_map*.

        Returns True if merged (position updated via running average).
        Returns False if no existing cone is within dedup radius.
        """
        for entry in cone_map:
            ex, ey = entry[0], entry[1]
            if (mx - ex) ** 2 + (my - ey) ** 2 < self._dedup_radius_sq:
                n = entry[2]
                entry[0] = (ex * n + mx) / (n + 1)
                entry[1] = (ey * n + my) / (n + 1)
                entry[2] = n + 1
                return True
        return False

    # ------------------------------------------------------------------
    # Publishing
    # ------------------------------------------------------------------
    def _publish_accumulated(self):
        """Publish accumulated cones as MarkerArray."""
        if not self._cone_map:
            return

        now = self.get_clock().now().to_msg()
        msg = MarkerArray()

        for i, entry in enumerate(self._cone_map):
            msg.markers.append(self._make_marker(
                i, 'cones', entry[0], entry[1], now,
                ColorRGBA(r=1.0, g=0.5, b=0.0, a=1.0)))

        self._det_pub.publish(msg)

    def _publish_debug(self):
        """Publish debug visualization of accumulated cones."""
        if not self._cone_map:
            return

        now = self.get_clock().now().to_msg()
        msg = MarkerArray()

        for i, entry in enumerate(self._cone_map):
            m = self._make_marker(
                i, 'debug_cones', entry[0], entry[1], now,
                ColorRGBA(r=1.0, g=0.5, b=0.0, a=0.5))
            m.scale.x = 0.25
            m.scale.y = 0.25
            m.scale.z = 0.25
            msg.markers.append(m)

        self._debug_pub.publish(msg)

    @staticmethod
    def _make_marker(mid: int, ns: str, x: float, y: float,
                     stamp, color: ColorRGBA) -> Marker:
        m = Marker()
        m.header.frame_id = 'map'
        m.header.stamp = stamp
        m.ns = ns
        m.id = mid
        m.type = Marker.SPHERE
        m.action = Marker.ADD
        m.pose.position.x = x
        m.pose.position.y = y
        m.pose.position.z = 0.0
        m.pose.orientation.w = 1.0
        m.scale.x = 0.35
        m.scale.y = 0.35
        m.scale.z = 0.35
        m.color = color
        return m


def main(args=None):
    rclpy.init(args=args)
    node = LidarConeDetector()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    finally:
        node.destroy_node()
        rclpy.shutdown()


if __name__ == '__main__':
    main()
