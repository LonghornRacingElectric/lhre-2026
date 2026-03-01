#!/usr/bin/env python3
"""FSAE driverless mission state machine manager.

States (per FSAE Driverless Supplement 2026, DO.1.1):
    OFF       – system idle
    READY     – checks passed, waiting for go signal
    DRIVING   – executing selected mission
    FINISHED  – mission complete, vehicle stopped
    EMERGENCY – shutdown circuit opened
"""

import math

import rclpy
from rclpy.node import Node

from nav_msgs.msg import Odometry, Path
from std_msgs.msg import Bool, Float32, String

# ---------------------------------------------------------------------------
# State constants
# ---------------------------------------------------------------------------
STATE_OFF = 'OFF'
STATE_READY = 'READY'
STATE_DRIVING = 'DRIVING'
STATE_FINISHED = 'FINISHED'
STATE_EMERGENCY = 'EMERGENCY'

MISSIONS = [
    'inspection', 'manual', 'ebs_test',
    'acceleration', 'skidpad', 'autocross',
]

# Numeric mapping for PlotJuggler (plot /lhr/debug/mission_state)
STATE_NUM = {
    STATE_OFF: 0.0,
    STATE_READY: 1.0,
    STATE_DRIVING: 2.0,
    STATE_FINISHED: 3.0,
    STATE_EMERGENCY: 4.0,
}


class MissionManager(Node):
    """Driverless mission state machine."""

    def __init__(self):
        super().__init__('mission_manager')

        # --- Parameters ---
        self.declare_parameter('mission', 'autocross')
        self.declare_parameter('auto_go', True)
        self.declare_parameter('ready_hold_sec', 5.0)
        self.declare_parameter('status_hz', 10.0)

        self._mission = self.get_parameter(
            'mission').get_parameter_value().string_value
        self._auto_go = self.get_parameter(
            'auto_go').get_parameter_value().bool_value
        self._ready_hold_sec = self.get_parameter(
            'ready_hold_sec').get_parameter_value().double_value
        status_hz = self.get_parameter(
            'status_hz').get_parameter_value().double_value

        if self._mission not in MISSIONS:
            self.get_logger().warn(
                f'Unknown mission "{self._mission}", '
                f'expected one of {MISSIONS}')

        # --- State ---
        self._state = STATE_OFF
        self._ready_enter_time = None
        self._driving_enter_time = None

        # Cached inputs
        self._path_available = False
        self._go_received = False
        self._emergency_received = False
        self._reset_received = False
        self._lap_complete = False
        self._current_speed = 0.0

        # --- Subscribers ---
        self.create_subscription(
            Bool, '/lhr/mission/go', self._go_cb, 10)
        self.create_subscription(
            Bool, '/lhr/mission/emergency', self._emergency_cb, 10)
        self.create_subscription(
            Bool, '/lhr/mission/reset', self._reset_cb, 10)
        self.create_subscription(
            Path, '/lhr/track/centerline', self._centerline_cb, 10)
        self.create_subscription(
            Odometry, '/lhr/vehicle/odom', self._odom_cb, 10)
        self.create_subscription(
            Bool, '/lhr/metrics/lap_complete', self._lap_complete_cb, 10)

        # --- Publishers ---
        self._status_pub = self.create_publisher(
            String, '/lhr/mission/status', 10)
        self._state_num_pub = self.create_publisher(
            Float32, '/lhr/debug/mission_state', 10)

        # --- Timer ---
        self.create_timer(1.0 / status_hz, self._tick)

        self.get_logger().info(
            f'MissionManager ready  (mission={self._mission}, '
            f'auto_go={self._auto_go}, '
            f'ready_hold={self._ready_hold_sec}s)')

    # ------------------------------------------------------------------
    # Callbacks – update cached state only
    # ------------------------------------------------------------------
    def _go_cb(self, msg: Bool):
        if msg.data:
            self._go_received = True

    def _emergency_cb(self, msg: Bool):
        if msg.data:
            self._emergency_received = True

    def _reset_cb(self, msg: Bool):
        if msg.data:
            self._reset_received = True

    def _centerline_cb(self, msg: Path):
        self._path_available = len(msg.poses) > 0

    def _odom_cb(self, msg: Odometry):
        vx = msg.twist.twist.linear.x
        vy = msg.twist.twist.linear.y
        self._current_speed = math.hypot(vx, vy)

    def _lap_complete_cb(self, msg: Bool):
        if msg.data:
            self._lap_complete = True

    # ------------------------------------------------------------------
    # State machine tick
    # ------------------------------------------------------------------
    def _tick(self):
        """Evaluate transitions, then publish current status."""
        if self._state == STATE_OFF:
            self._tick_off()
        elif self._state == STATE_READY:
            self._tick_ready()
        elif self._state == STATE_DRIVING:
            self._tick_driving()
        elif self._state == STATE_EMERGENCY:
            self._tick_emergency()
        # FINISHED is terminal (no automatic transitions out)

        self._status_pub.publish(String(data=self._state))
        self._state_num_pub.publish(
            Float32(data=STATE_NUM.get(self._state, -1.0)))

    def _tick_off(self):
        """OFF -> READY when centerline path is available."""
        if self._path_available:
            self._transition(STATE_READY)
            self._ready_enter_time = self.get_clock().now()

    def _tick_ready(self):
        """READY -> DRIVING on go signal or auto-go timeout."""
        if self._go_received:
            self._go_received = False
            self._transition(STATE_DRIVING)
            self._driving_enter_time = self.get_clock().now()
            return

        if self._auto_go and self._ready_enter_time is not None:
            elapsed = (self.get_clock().now()
                       - self._ready_enter_time).nanoseconds / 1e9
            if elapsed >= self._ready_hold_sec:
                self._transition(STATE_DRIVING)
                self._driving_enter_time = self.get_clock().now()

    def _tick_driving(self):
        """DRIVING -> EMERGENCY on e-stop, or FINISHED on mission complete."""
        if self._emergency_received:
            self._emergency_received = False
            self._transition(STATE_EMERGENCY)
            return

        if self._check_mission_complete():
            if self._current_speed < 0.5:
                self._transition(STATE_FINISHED)

    def _tick_emergency(self):
        """EMERGENCY -> OFF on reset signal."""
        if self._reset_received:
            self._reset_received = False
            self._lap_complete = False
            self._path_available = False
            self._transition(STATE_OFF)

    # ------------------------------------------------------------------
    # Mission completion
    # ------------------------------------------------------------------
    def _check_mission_complete(self) -> bool:
        """Return True when the active mission's completion criteria are met."""
        if self._mission == 'autocross':
            return self._lap_complete

        if self._mission == 'inspection':
            if self._driving_enter_time is None:
                return False
            elapsed = (self.get_clock().now()
                       - self._driving_enter_time).nanoseconds / 1e9
            return elapsed >= 28.0  # 25-30 sec per FSAE rules

        # acceleration, skidpad, ebs_test, manual: not yet implemented
        return False

    # ------------------------------------------------------------------
    # Helpers
    # ------------------------------------------------------------------
    def _transition(self, new_state: str):
        self.get_logger().info(f'State: {self._state} -> {new_state}')
        self._state = new_state


def main():
    """Entry point."""
    rclpy.init()
    node = MissionManager()
    try:
        rclpy.spin(node)
    except KeyboardInterrupt:
        pass
    node.destroy_node()
    rclpy.shutdown()
