"""Launch the FSAE autonomy stack with Gazebo Harmonic physics simulation.

Replaces lhr_sim_kinematic with Gazebo for physics and odometry.
Perception is selectable: 'sim' (trackgen + sensor_sim) or 'lidar' (LiDAR).
The upper stack (track_builder, control, mission_manager, metrics) is unchanged.

Usage:
    ros2 launch lhr_gazebo gazebo_demo.launch.py
    ros2 launch lhr_gazebo gazebo_demo.launch.py gui:=false
    ros2 launch lhr_gazebo gazebo_demo.launch.py perception:=lidar
    ros2 launch lhr_gazebo gazebo_demo.launch.py track_style:=oval
    ros2 launch lhr_gazebo gazebo_demo.launch.py track_style:=oval perception:=lidar
"""

import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchContext, LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    ExecuteProcess,
    OpaqueFunction,
    SetEnvironmentVariable,
)
from launch.conditions import IfCondition, UnlessCondition
from launch.substitutions import (
    LaunchConfiguration,
    PythonExpression,
)
from launch_ros.actions import Node


def _find_world(context: LaunchContext):
    """Resolve the world SDF path from track_style and seed.

    Search order:
      1. Explicit ``world`` argument (if the user overrode it)
      2. ``<worlds_dir>/<track_style>_seed<seed>.sdf`` in the source tree
      3. Same pattern in the installed share directory
      4. Fall back to any SDF in the source worlds directory
    """
    world_arg = context.launch_configurations.get('world', '')
    track_style = context.launch_configurations.get('track_style', 'autocross')
    seed = context.launch_configurations.get('seed', '1')

    # If the user explicitly passed a valid world path, use it
    if world_arg and os.path.isfile(world_arg):
        return world_arg

    expected = f'{track_style}_seed{seed}.sdf'

    # Source tree (launch/ is one level below the package root)
    launch_dir = os.path.dirname(os.path.abspath(__file__))
    src_worlds = os.path.join(launch_dir, os.pardir, 'worlds')
    src_path = os.path.join(src_worlds, expected)
    if os.path.isfile(src_path):
        return os.path.abspath(src_path)

    # Installed share directory
    try:
        pkg_share = get_package_share_directory('lhr_gazebo')
        share_path = os.path.join(pkg_share, 'worlds', expected)
        if os.path.isfile(share_path):
            return share_path
    except Exception:
        pass

    # Fallback: first SDF found in source worlds directory
    if os.path.isdir(src_worlds):
        for f in sorted(os.listdir(src_worlds)):
            if f.endswith('.sdf'):
                return os.path.abspath(os.path.join(src_worlds, f))

    return ''


def _launch_setup(context: LaunchContext):
    """Build all launch actions — called via OpaqueFunction so we can
    resolve the world path from track_style + seed at launch time."""

    world_path = _find_world(context)
    if not world_path:
        raise RuntimeError(
            'No world SDF found. Generate one with: '
            'python3 scripts/generate_world.py --style <style> --seed <seed>')

    pkg_share = get_package_share_directory('lhr_gazebo')
    models_dir = os.path.join(pkg_share, 'models')
    config_dir = os.path.join(pkg_share, 'config')

    perception = context.launch_configurations.get('perception', 'sim')

    _gz_env = {
        'GZ_SIM_RESOURCE_PATH': models_dir,
    }

    # ----- Gazebo server -----
    gui = context.launch_configurations.get('gui', 'true').lower()
    if gui == 'true':
        gz_cmd = ['gz', 'sim', '-r', world_path]
    else:
        gz_cmd = ['gz', 'sim', '-r', '-s', world_path]

    gz_sim = ExecuteProcess(
        cmd=gz_cmd,
        output='screen',
        additional_env=_gz_env,
    )

    # ----- ros_gz_bridge -----
    bridge = Node(
        package='ros_gz_bridge',
        executable='parameter_bridge',
        name='ros_gz_bridge',
        parameters=[{
            'config_file': os.path.join(config_dir, 'ros_gz_bridge.yaml'),
        }],
        output='screen',
    )

    # ----- Joint command adapter -----
    cmd_adapter = Node(
        package='lhr_gazebo',
        executable='joint_cmd_adapter',
        name='joint_cmd_adapter',
        parameters=[{'use_sim_time': True}],
        output='screen',
    )

    # ----- Perception nodes -----
    perception_nodes = []

    if perception == 'sim':
        perception_nodes.append(Node(
            package='lhr_trackgen',
            executable='publish_cones',
            name='publish_cones',
            parameters=[{
                'seed': LaunchConfiguration('seed'),
                'track_style': LaunchConfiguration('track_style'),
                'num_waypoints': LaunchConfiguration('num_waypoints'),
                'use_sim_time': True,
            }],
            output='screen',
        ))
        perception_nodes.append(Node(
            package='lhr_sensor_sim',
            executable='sensor_sim',
            name='sensor_sim',
            parameters=[{
                'fov_deg': LaunchConfiguration('fov_deg'),
                'max_range_m': LaunchConfiguration('max_range_m'),
                'use_sim_time': True,
            }],
            output='screen',
        ))
    elif perception == 'lidar':
        perception_nodes.append(Node(
            package='lhr_perception',
            executable='lidar_cone_detector',
            name='lidar_cone_detector',
            parameters=[{
                'use_sim_time': True,
                'max_range': 20.0,
                'min_range': 0.8,
                'ground_z_min': -0.40,
                'ground_z_max': 0.5,
                'cluster_radius': 0.5,
                'min_cluster_points': 1,
                'dedup_radius': 1.5,
            }],
            output='screen',
        ))

    # ----- Track builder -----
    if perception == 'lidar':
        centerline = Node(
            package='lhr_track_builder',
            executable='track_builder',
            name='track_builder',
            parameters=[{
                'use_sim_time': True,
                'pairing_strategy': 'boundary',
                'track_width': 3.5,
                'track_width_tolerance': 1.0,
            }],
            output='screen',
        )
    else:
        centerline = Node(
            package='lhr_track_builder',
            executable='track_builder',
            name='track_builder',
            parameters=[{
                'use_sim_time': True,
                'pairing_strategy': 'index',
            }],
            output='screen',
        )

    # ----- Control -----
    control = Node(
        package='lhr_control',
        executable='pursuit_node',
        name='pure_pursuit',
        parameters=[{
            'lookahead_dist': LaunchConfiguration('lookahead_dist'),
            'lookahead_min': LaunchConfiguration('lookahead_min'),
            'lookahead_curvature_gain': LaunchConfiguration(
                'lookahead_curvature_gain'),
            'a_lat_max': LaunchConfiguration('a_lat_max'),
            'v_min': LaunchConfiguration('v_min'),
            'v_max': LaunchConfiguration('v_max'),
            'max_accel': LaunchConfiguration('max_accel'),
            'max_decel': LaunchConfiguration('max_decel'),
            'use_sim_time': True,
        }],
        output='screen',
    )

    # ----- Metrics -----
    metrics_nodes = []
    if context.launch_configurations.get(
            'enable_metrics', 'true').lower() == 'true':
        metrics_nodes.append(Node(
            package='lhr_metrics',
            executable='metrics_node',
            name='metrics_node',
            parameters=[{'use_sim_time': True}],
            output='screen',
        ))

    # ----- RViz -----
    rviz_nodes = []
    if context.launch_configurations.get('rviz', 'true').lower() == 'true':
        rviz_nodes.append(Node(
            package='rviz2',
            executable='rviz2',
            name='rviz2',
            arguments=['-d', os.path.join(config_dir, 'default.rviz')],
            parameters=[{'use_sim_time': True}],
            output='screen',
        ))

    # ----- Mission manager -----
    mission_mgr = Node(
        package='lhr_mission_manager',
        executable='mission_manager',
        name='mission_manager',
        parameters=[{
            'mission': LaunchConfiguration('mission'),
            'auto_go': LaunchConfiguration('auto_go'),
            'ready_hold_sec': LaunchConfiguration('ready_hold_sec'),
            'use_sim_time': True,
        }],
        output='screen',
    )

    return [
        SetEnvironmentVariable('GZ_SIM_RESOURCE_PATH', models_dir),
        gz_sim,
        bridge,
        cmd_adapter,
        *perception_nodes,
        centerline,
        mission_mgr,
        control,
        *metrics_nodes,
        *rviz_nodes,
    ]


def generate_launch_description():
    return LaunchDescription([
        # ----- Launch arguments -----
        DeclareLaunchArgument(
            'world', default_value='',
            description='Override: absolute path to world SDF file'),
        DeclareLaunchArgument(
            'gui', default_value='true',
            description='Launch Gazebo GUI (set false for headless)'),
        DeclareLaunchArgument(
            'perception', default_value='sim',
            description='Perception mode: "sim" or "lidar"'),
        DeclareLaunchArgument(
            'track_style', default_value='autocross',
            description='Track style: "autocross", "oval", or "simple"'),
        DeclareLaunchArgument('seed', default_value='1'),
        DeclareLaunchArgument('num_waypoints', default_value='10'),
        # Sensor sim
        DeclareLaunchArgument('fov_deg', default_value='200.0'),
        DeclareLaunchArgument('max_range_m', default_value='20.0'),
        # Control
        DeclareLaunchArgument('lookahead_dist', default_value='5.0'),
        DeclareLaunchArgument('lookahead_min', default_value='2.0'),
        DeclareLaunchArgument('lookahead_curvature_gain', default_value='3.0'),
        DeclareLaunchArgument('a_lat_max', default_value='3.0'),
        DeclareLaunchArgument('v_min', default_value='1.0'),
        DeclareLaunchArgument('v_max', default_value='5.0'),
        DeclareLaunchArgument('max_accel', default_value='1.0'),
        DeclareLaunchArgument('max_decel', default_value='2.0'),
        # RViz / metrics
        DeclareLaunchArgument('rviz', default_value='true'),
        DeclareLaunchArgument('enable_metrics', default_value='true'),
        # Mission
        DeclareLaunchArgument('mission', default_value='autocross'),
        DeclareLaunchArgument('auto_go', default_value='true'),
        DeclareLaunchArgument('ready_hold_sec', default_value='5.0'),
        # ----- Build all nodes via OpaqueFunction -----
        OpaqueFunction(function=_launch_setup),
    ])
