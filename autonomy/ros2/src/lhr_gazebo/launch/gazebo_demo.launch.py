"""Launch the FSAE autonomy stack with Gazebo physics simulation.

Replaces lhr_sim_kinematic with Gazebo for physics and odometry.
Cone detection reuses the existing lhr_trackgen + lhr_sensor_sim pipeline.
The upper stack (track_builder, control, mission_manager, metrics) is unchanged.

Usage:
    ros2 launch lhr_gazebo gazebo_demo.launch.py
    ros2 launch lhr_gazebo gazebo_demo.launch.py gui:=false
    ros2 launch lhr_gazebo gazebo_demo.launch.py world:=/absolute/path/to/world.sdf
"""

import os
import shutil

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    ExecuteProcess,
    SetEnvironmentVariable,
)
from launch.conditions import IfCondition, UnlessCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def _find_default_world():
    """Locate the default world SDF in the source tree."""
    launch_dir = os.path.dirname(os.path.abspath(__file__))
    worlds_dir = os.path.join(launch_dir, os.pardir, 'worlds')
    default = os.path.join(worlds_dir, 'autocross_seed1.sdf')
    if os.path.isfile(default):
        return os.path.abspath(default)
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

    # Track generation (must match the seed used for world generation)
    seed_arg = DeclareLaunchArgument('seed', default_value='1')
    num_wp_arg = DeclareLaunchArgument('num_waypoints', default_value='10')

    # Sensor sim
    fov_arg = DeclareLaunchArgument('fov_deg', default_value='200.0')
    range_arg = DeclareLaunchArgument('max_range_m', default_value='20.0')

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
    # Fortress uses IGN_GAZEBO_RESOURCE_PATH; Garden+ uses GZ_SIM_RESOURCE_PATH
    gz_model_path = SetEnvironmentVariable(
        'GZ_SIM_RESOURCE_PATH', models_dir)
    ign_model_path = SetEnvironmentVariable(
        'IGN_GAZEBO_RESOURCE_PATH', models_dir)
    # WSL: prefer discrete NVIDIA GPU for both GLX and EGL paths
    mesa_gpu = SetEnvironmentVariable(
        'MESA_D3D12_DEFAULT_ADAPTER_NAME', 'NVIDIA')

    _gz_env = {
        'GZ_SIM_RESOURCE_PATH': models_dir,
        'IGN_GAZEBO_RESOURCE_PATH': models_dir,
        # WSL: prefer the discrete NVIDIA GPU over the integrated AMD iGPU
        'MESA_D3D12_DEFAULT_ADAPTER_NAME': 'NVIDIA',
    }

    # ----- Gazebo server -----
    # Fortress (ROS Humble default): `ign gazebo`
    # Garden / Harmonic:             `gz sim`
    if shutil.which('ign'):
        _gz_base = ['ign', 'gazebo']
    else:
        _gz_base = ['gz', 'sim']

    # With GUI
    gz_sim_gui = ExecuteProcess(
        cmd=_gz_base + ['-r', LaunchConfiguration('world')],
        output='screen',
        additional_env=_gz_env,
        condition=IfCondition(LaunchConfiguration('gui')),
    )

    # Headless (server only, no rendering)
    gz_sim_headless = ExecuteProcess(
        cmd=_gz_base + ['-r', '-s', LaunchConfiguration('world')],
        output='screen',
        additional_env=_gz_env,
        condition=UnlessCondition(LaunchConfiguration('gui')),
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

    # ----- Adapter: AckermannDriveStamped → Twist for Gazebo -----
    cmd_adapter = Node(
        package='lhr_gazebo',
        executable='ackermann_cmd_adapter',
        name='ackermann_cmd_adapter',
        parameters=[{'use_sim_time': True}],
        output='screen',
    )

    # ----- Cone pipeline (reuse existing trackgen + sensor sim) -----
    cones = Node(
        package='lhr_trackgen',
        executable='publish_cones',
        name='publish_cones',
        parameters=[{
            'seed': LaunchConfiguration('seed'),
            'num_waypoints': LaunchConfiguration('num_waypoints'),
            'use_sim_time': True,
        }],
        output='screen',
    )

    sensor_sim = Node(
        package='lhr_sensor_sim',
        executable='sensor_sim',
        name='sensor_sim',
        parameters=[{
            'fov_deg': LaunchConfiguration('fov_deg'),
            'max_range_m': LaunchConfiguration('max_range_m'),
            'use_sim_time': True,
        }],
        output='screen',
    )

    # ----- Upper stack (unchanged from mvs_demo) -----
    centerline = Node(
        package='lhr_track_builder',
        executable='track_builder',
        name='track_builder',
        parameters=[{'use_sim_time': True}],
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
            'use_sim_time': True,
        }],
        output='screen',
    )

    metrics = Node(
        package='lhr_metrics',
        executable='metrics_node',
        name='metrics_node',
        parameters=[{'use_sim_time': True}],
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
            'use_sim_time': True,
        }],
        output='screen',
    )

    return LaunchDescription([
        # Arguments
        world_arg,
        gui_arg,
        seed_arg,
        num_wp_arg,
        fov_arg,
        range_arg,
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
        ign_model_path,
        mesa_gpu,
        # Gazebo physics (one of these will activate based on gui arg)
        gz_sim_gui,
        gz_sim_headless,
        # Bridge + adapter
        bridge,
        cmd_adapter,
        # Cone pipeline (reuses existing ROS nodes)
        cones,
        sensor_sim,
        # Upper stack
        centerline,
        mission_mgr,
        control,
        metrics,
    ])
