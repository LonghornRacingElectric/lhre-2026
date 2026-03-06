#!/usr/bin/env python3
"""Generate a Gazebo world SDF from the same track generator used by lhr_trackgen.

Usage:
    python3 generate_world.py --seed 1 --output worlds/autocross_seed1.sdf
    python3 generate_world.py --seed 42 --num-waypoints 12 --output worlds/test.sdf

The script imports generate_autocross_track from lhr_trackgen so the track
layout is identical for a given seed — same cones, same positions.
"""

import argparse
import math
import os
import sys
import textwrap

# Allow importing lhr_trackgen even when not installed as a ROS package.
# Walk up to find the src directory and add trackgen to the path.
_this_dir = os.path.dirname(os.path.abspath(__file__))
_trackgen_pkg = os.path.join(
    _this_dir, os.pardir, os.pardir, 'lhr_trackgen')
if os.path.isdir(_trackgen_pkg):
    sys.path.insert(0, _trackgen_pkg)

from lhr_trackgen.publish_cones import GENERATORS  # noqa: E402


def _cone_include(model_name: str, instance_name: str,
                  x: float, y: float, z: float) -> str:
    """Return an SDF <include> block for a cone."""
    return textwrap.dedent(f"""\
    <include>
      <uri>model://{model_name}</uri>
      <name>{instance_name}</name>
      <pose>{x:.4f} {y:.4f} {z:.4f} 0 0 0</pose>
    </include>""")


def _compute_spawn(left_cones, right_cones):
    """Compute a spawn pose on the straightest section of track, clear of cones."""
    n = len(left_cones)

    def _mid(i):
        return ((left_cones[i][0] + right_cones[i][0]) / 2.0,
                (left_cones[i][1] + right_cones[i][1]) / 2.0)

    # Score each cone pair by how straight the surrounding track is.
    # Measure angular deviation over a window, skip the first/last few
    # pairs to avoid the messy loop closure.
    margin = max(8, n // 10)
    best_idx = margin
    best_score = float('inf')
    window = 5

    for i in range(margin, n - margin):
        m_prev = _mid(i - window)
        m_curr = _mid(i)
        m_next = _mid(i + window)
        a1 = math.atan2(m_curr[1] - m_prev[1], m_curr[0] - m_prev[0])
        a2 = math.atan2(m_next[1] - m_curr[1], m_next[0] - m_curr[0])
        # Angular deviation (smaller = straighter)
        diff = abs(math.atan2(math.sin(a2 - a1), math.cos(a2 - a1)))
        if diff < best_score:
            best_score = diff
            best_idx = i

    # Spawn at this pair, heading toward the next few pairs
    mx, my = _mid(best_idx)
    look = min(best_idx + 5, n - margin)
    fx, fy = _mid(look)
    yaw = math.atan2(fy - my, fx - mx)
    return mx, my, yaw


def generate_world_sdf(
    left_cones, right_cones,
    vehicle_x: float = None,
    vehicle_y: float = None,
    vehicle_yaw: float = None,
    model_dir: str = '',
) -> str:
    """Build a complete Gazebo world SDF string."""

    # Auto-compute spawn from track geometry if not explicitly set
    if vehicle_x is None or vehicle_y is None or vehicle_yaw is None:
        sx, sy, syaw = _compute_spawn(left_cones, right_cones)
        vehicle_x = vehicle_x if vehicle_x is not None else sx
        vehicle_y = vehicle_y if vehicle_y is not None else sy
        vehicle_yaw = vehicle_yaw if vehicle_yaw is not None else syaw

    cone_blocks = []

    # Blue cones on the left boundary
    blue_half_h = 0.325 / 2.0
    for i, (cx, cy) in enumerate(left_cones):
        cone_blocks.append(
            _cone_include('cone_blue', f'cone_blue_{i}',
                          cx, cy, blue_half_h))

    # Yellow cones on the right boundary
    yellow_half_h = 0.325 / 2.0
    for i, (cx, cy) in enumerate(right_cones):
        cone_blocks.append(
            _cone_include('cone_yellow', f'cone_yellow_{i}',
                          cx, cy, yellow_half_h))

    cones_xml = '\n'.join(cone_blocks)

    # Vehicle spawn height: wheel radius so it sits on the ground
    veh_z = 0.0

    world_sdf = textwrap.dedent(f"""\
    <?xml version="1.0" ?>
    <sdf version="1.9">
      <world name="fsae_world">

        <!-- ===== Required world plugins ===== -->
        <plugin filename="gz-sim-physics-system"
                name="gz::sim::systems::Physics"/>
        <plugin filename="gz-sim-user-commands-system"
                name="gz::sim::systems::UserCommands"/>
        <plugin filename="gz-sim-scene-broadcaster-system"
                name="gz::sim::systems::SceneBroadcaster"/>
        <plugin filename="gz-sim-imu-system"
                name="gz::sim::systems::Imu"/>
        <plugin filename="gz-sim-sensors-system"
                name="gz::sim::systems::Sensors">
          <render_engine>ogre2</render_engine>
        </plugin>

        <!-- ===== Scene ===== -->
        <scene>
          <ambient>1.0 1.0 1.0</ambient>
          <background>0.7 0.8 0.9</background>
        </scene>

        <gravity>0 0 -9.81</gravity>

        <physics name="default_physics" type="ode">
          <max_step_size>0.001</max_step_size>
          <real_time_factor>1.0</real_time_factor>
        </physics>

        <!-- ===== Lighting ===== -->
        <light type="directional" name="sun">
          <cast_shadows>true</cast_shadows>
          <pose>0 0 50 0 0 0</pose>
          <diffuse>0.9 0.9 0.9 1</diffuse>
          <specular>0.3 0.3 0.3 1</specular>
          <direction>-0.5 0.3 -1.0</direction>
        </light>

        <!-- ===== Ground plane ===== -->
        <model name="ground_plane">
          <static>true</static>
          <link name="link">
            <collision name="collision">
              <geometry>
                <plane>
                  <normal>0 0 1</normal>
                  <size>200 200</size>
                </plane>
              </geometry>
              <surface>
                <friction>
                  <ode><mu>1.0</mu><mu2>1.0</mu2></ode>
                </friction>
              </surface>
            </collision>
            <visual name="visual">
              <geometry>
                <plane>
                  <normal>0 0 1</normal>
                  <size>200 200</size>
                </plane>
              </geometry>
              <material>
                <ambient>0.4 0.4 0.4 1</ambient>
                <diffuse>0.5 0.5 0.5 1</diffuse>
              </material>
            </visual>
          </link>
        </model>

        <!-- ===== Vehicle ===== -->
        <include>
          <uri>model://fsae_vehicle</uri>
          <name>fsae_vehicle</name>
          <pose>{vehicle_x:.4f} {vehicle_y:.4f} {veh_z:.4f} 0 0 {vehicle_yaw:.4f}</pose>
        </include>

        <!-- ===== Cones ===== -->
    {textwrap.indent(cones_xml, '    ')}

      </world>
    </sdf>
    """)

    return world_sdf


def main():
    parser = argparse.ArgumentParser(
        description='Generate a Gazebo world SDF from LHR track generator.')
    parser.add_argument('--seed', type=int, default=1,
                        help='Random seed (default: 1)')
    parser.add_argument('--num-waypoints', type=int, default=10,
                        help='Number of track waypoints (default: 10)')
    parser.add_argument('--radius', type=float, default=25.0,
                        help='Base track radius in meters (default: 25.0)')
    parser.add_argument('--jitter', type=float, default=10.0,
                        help='Radial jitter in meters (default: 10.0)')
    parser.add_argument('--width', type=float, default=3.5,
                        help='Track width in meters (default: 3.5)')
    parser.add_argument('--cone-spacing', type=float, default=2.0,
                        help='Cone spacing in meters (default: 2.0)')
    parser.add_argument('--vehicle-x', type=float, default=None,
                        help='Vehicle initial X (default: auto from track)')
    parser.add_argument('--vehicle-y', type=float, default=None,
                        help='Vehicle initial Y (default: auto from track)')
    parser.add_argument('--vehicle-yaw', type=float, default=None,
                        help='Vehicle initial yaw in radians (default: auto from track)')
    parser.add_argument('--style', type=str, default='autocross',
                        choices=list(GENERATORS.keys()),
                        help='Track style (default: autocross)')
    parser.add_argument('--output', type=str, default=None,
                        help='Output SDF file path (default: worlds/<style>_seed<N>.sdf)')
    args = parser.parse_args()

    gen = GENERATORS[args.style]
    left, right = gen(
        seed=args.seed,
        num_waypoints=args.num_waypoints,
        radius_m=args.radius,
        jitter_m=args.jitter,
        width_m=args.width,
        cone_spacing_m=args.cone_spacing,
    )

    print(f'Generated track: {len(left)} left cones, {len(right)} right cones')

    sdf = generate_world_sdf(
        left, right,
        vehicle_x=args.vehicle_x,
        vehicle_y=args.vehicle_y,
        vehicle_yaw=args.vehicle_yaw,
    )

    if args.output is None:
        # Default output path relative to this script's parent (lhr_gazebo/)
        worlds_dir = os.path.join(_this_dir, os.pardir, 'worlds')
        os.makedirs(worlds_dir, exist_ok=True)
        out_path = os.path.join(worlds_dir, f'{args.style}_seed{args.seed}.sdf')
    else:
        out_path = args.output
        os.makedirs(os.path.dirname(out_path) or '.', exist_ok=True)

    with open(out_path, 'w') as f:
        f.write(sdf)

    print(f'World written to: {os.path.abspath(out_path)}')


if __name__ == '__main__':
    main()
