"""Launch the FSAE autonomy stack with Gazebo physics simulation.

Replaces lhr_trackgen, lhr_sensor_sim, and lhr_sim_kinematic with Gazebo.
The upper stack (track_builder, control, mission_manager, metrics) is unchanged.

Usage:
    ros2 launch lhr_gazebo gazebo_demo.launch.py
    ros2 launch lhr_gazebo gazebo_demo.launch.py world:=/absolute/path/to/world.sdf
"""

import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    ExecuteProcess,
    SetEnvironmentVariable,
)
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def _find_default_world():
    """Locate the default world SDF in the source tree."""
    # This launch file lives in <src>/lhr_gazebo/launch/
    # The worlds dir is at <src>/lhr_gazebo/worlds/
    launch_dir = os.path.dirname(os.path.abspath(__file__))
    worlds_dir = os.path.join(launch_dir, os.pardir, 'worlds')
    default = os.path.join(worlds_dir, 'autocross_seed1.sdf')
    if os.path.isfile(default):
        return os.path.abspath(default)
    # Fallback: check all world files and pick the first
    if os.path.isdir(worlds_dir):
        for f in sorted(os.listdir(worlds_dir)):
            if f.endswith('.sdf'):
                return os.path.abspath(os.path.join(worlds_dir, f))
    return ''


def generate_launch_description():
    pkg_share = get_package_share_directory('lhr_gazebo')
    models_dir = os.path.join(pkg_share, 'models')
    config_dir = os.path.join(pkg_share, 'config')

    default_world = _find_default_world()

    # ----- Launch arguments -----
    world_arg = DeclareLaunchArgument(
        'world', default_value=default_world,
        description='Absolute path to world SDF file')
    gui_arg = DeclareLaunchArgument(
        'gui', default_value='true',
        description='Launch Gazebo GUI (set false for headless)')

    # Control params (same as mvs_demo)
    lookahead_arg = DeclareLaunchArgument(
        'lookahead_dist', default_value='4.0')
    a_lat_arg = DeclareLaunchArgument('a_lat_max', default_value='6.0')
    v_min_arg = DeclareLaunchArgument('v_min', default_value='2.0')
    v_max_arg = DeclareLaunchArgument('v_max', default_value='12.0')
    max_accel_arg = DeclareLaunchArgument('max_accel', default_value='2.0')
    max_decel_arg = DeclareLaunchArgument('max_decel', default_value='3.0')

    # Metrics
    metrics_arg = DeclareLaunchArgument(
        'enable_metrics', default_value='true')

    # Mission manager
    mission_arg = DeclareLaunchArgument(
        'mission', default_value='autocross')
    auto_go_arg = DeclareLaunchArgument(
        'auto_go', default_value='true')
    ready_hold_arg = DeclareLaunchArgument(
        'ready_hold_sec', default_value='5.0')

    # ----- Environment: tell Gazebo where to find our models -----
    gz_model_path = SetEnvironmentVariable(
        'GZ_SIM_RESOURCE_PATH',
        models_dir)

    # ----- Gazebo server -----
    gz_sim = ExecuteProcess(
        cmd=['gz', 'sim', '-r', LaunchConfiguration('world')],
        output='screen',
        additional_env={'GZ_SIM_RESOURCE_PATH': models_dir},
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

    # ----- Adapter nodes -----
    cmd_adapter = Node(
        package='lhr_gazebo',
        executable='ackermann_cmd_adapter',
        name='ackermann_cmd_adapter',
        output='screen',
    )

    camera_adapter = Node(
        package='lhr_gazebo',
        executable='logical_camera_adapter',
        name='logical_camera_adapter',
        output='screen',
    )

    # ----- Upper stack (unchanged from mvs_demo) -----
    centerline = Node(
        package='lhr_track_builder',
        executable='track_builder',
        name='track_builder',
        output='screen',
    )

    control = Node(
        package='lhr_control',
        executable='pursuit_node',
        name='pure_pursuit',
        parameters=[{
            'lookahead_dist': LaunchConfiguration('lookahead_dist'),
            'a_lat_max': LaunchConfiguration('a_lat_max'),
            'v_min': LaunchConfiguration('v_min'),
            'v_max': LaunchConfiguration('v_max'),
            'max_accel': LaunchConfiguration('max_accel'),
            'max_decel': LaunchConfiguration('max_decel'),
        }],
        output='screen',
    )

    metrics = Node(
        package='lhr_metrics',
        executable='metrics_node',
        name='metrics_node',
        output='screen',
        condition=IfCondition(LaunchConfiguration('enable_metrics')),
    )

    mission_mgr = Node(
        package='lhr_mission_manager',
        executable='mission_manager',
        name='mission_manager',
        parameters=[{
            'mission': LaunchConfiguration('mission'),
            'auto_go': LaunchConfiguration('auto_go'),
            'ready_hold_sec': LaunchConfiguration('ready_hold_sec'),
        }],
        output='screen',
    )

    return LaunchDescription([
        # Arguments
        world_arg,
        gui_arg,
        lookahead_arg,
        a_lat_arg,
        v_min_arg,
        v_max_arg,
        max_accel_arg,
        max_decel_arg,
        metrics_arg,
        mission_arg,
        auto_go_arg,
        ready_hold_arg,
        # Environment
        gz_model_path,
        # Gazebo
        gz_sim,
        # Bridge + adapters
        bridge,
        cmd_adapter,
        camera_adapter,
        # Upper stack
        centerline,
        mission_mgr,
        control,
        metrics,
    ])
