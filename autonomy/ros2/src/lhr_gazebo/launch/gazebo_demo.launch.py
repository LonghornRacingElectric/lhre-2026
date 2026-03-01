"""Launch the FSAE autonomy stack with Gazebo Harmonic physics simulation.

Replaces lhr_sim_kinematic with Gazebo for physics and odometry.
Perception is selectable: 'sim' (trackgen + sensor_sim) or 'lidar' (LiDAR).
The upper stack (track_builder, control, mission_manager, metrics) is unchanged.

Usage:
    ros2 launch lhr_gazebo gazebo_demo.launch.py
    ros2 launch lhr_gazebo gazebo_demo.launch.py gui:=false
    ros2 launch lhr_gazebo gazebo_demo.launch.py perception:=lidar
"""

import os

from ament_index_python.packages import get_package_share_directory

from launch import LaunchDescription
from launch.actions import (
    DeclareLaunchArgument,
    ExecuteProcess,
    SetEnvironmentVariable,
)
from launch.conditions import IfCondition, UnlessCondition
from launch.substitutions import (
    LaunchConfiguration,
    PythonExpression,
)
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
    perception_arg = DeclareLaunchArgument(
        'perception', default_value='sim',
        description='Perception mode: "sim" (trackgen+sensor_sim) '
                    'or "lidar" (LiDAR pointcloud)')

    # Track generation (must match the seed used for world generation)
    seed_arg = DeclareLaunchArgument('seed', default_value='1')
    num_wp_arg = DeclareLaunchArgument('num_waypoints', default_value='10')

    # Sensor sim
    fov_arg = DeclareLaunchArgument('fov_deg', default_value='200.0')
    range_arg = DeclareLaunchArgument('max_range_m', default_value='20.0')

    # Control params
    lookahead_arg = DeclareLaunchArgument(
        'lookahead_dist', default_value='5.0')
    lookahead_min_arg = DeclareLaunchArgument(
        'lookahead_min', default_value='2.0')
    lookahead_curv_gain_arg = DeclareLaunchArgument(
        'lookahead_curvature_gain', default_value='3.0')
    a_lat_arg = DeclareLaunchArgument('a_lat_max', default_value='3.0')
    v_min_arg = DeclareLaunchArgument('v_min', default_value='1.0')
    v_max_arg = DeclareLaunchArgument('v_max', default_value='5.0')
    max_accel_arg = DeclareLaunchArgument('max_accel', default_value='1.0')
    max_decel_arg = DeclareLaunchArgument('max_decel', default_value='2.0')

    # RViz
    rviz_arg = DeclareLaunchArgument(
        'rviz', default_value='true',
        description='Launch RViz alongside Gazebo')

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

    # ----- Conditions for perception mode -----
    perception_cfg = LaunchConfiguration('perception')
    is_sim = IfCondition(PythonExpression([
        "'", perception_cfg, "' == 'sim'"]))
    is_lidar = IfCondition(PythonExpression([
        "'", perception_cfg, "' == 'lidar'"]))

    # ----- Environment: tell Gazebo where to find our models -----
    gz_model_path = SetEnvironmentVariable(
        'GZ_SIM_RESOURCE_PATH', models_dir)

    _gz_env = {
        'GZ_SIM_RESOURCE_PATH': models_dir,
    }

    # ----- Gazebo server (Harmonic: `gz sim`) -----
    # With GUI
    gz_sim_gui = ExecuteProcess(
        cmd=['gz', 'sim', '-r', LaunchConfiguration('world')],
        output='screen',
        additional_env=_gz_env,
        condition=IfCondition(LaunchConfiguration('gui')),
    )

    # Headless (server only, no rendering)
    gz_sim_headless = ExecuteProcess(
        cmd=['gz', 'sim', '-r', '-s', LaunchConfiguration('world')],
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

    # ----- Adapter: AckermannDriveStamped → individual joint commands -----
    cmd_adapter = Node(
        package='lhr_gazebo',
        executable='joint_cmd_adapter',
        name='joint_cmd_adapter',
        parameters=[{'use_sim_time': True}],
        output='screen',
    )

    # ----- Sim perception (trackgen + sensor_sim, perception:=sim) -----
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
        condition=is_sim,
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
        condition=is_sim,
    )

    # ----- LiDAR perception (perception:=lidar) -----
    lidar_perception = Node(
        package='lhr_perception',
        executable='lidar_cone_detector',
        name='lidar_cone_detector',
        parameters=[{
            'use_sim_time': True,
            'max_range': 20.0,
            'min_range': 0.8,
            'ground_z_min': -0.40,
            'ground_z_max': 0.5,
            'cluster_radius': 0.35,
            'dedup_radius': 1.0,
        }],
        output='screen',
        condition=is_lidar,
    )

    # ----- Upper stack -----
    # Track builder: use nearest pairing for LiDAR, index for sim
    centerline_sim = Node(
        package='lhr_track_builder',
        executable='track_builder',
        name='track_builder',
        parameters=[{
            'use_sim_time': True,
            'pairing_strategy': 'index',
        }],
        output='screen',
        condition=is_sim,
    )

    centerline_lidar = Node(
        package='lhr_track_builder',
        executable='track_builder',
        name='track_builder',
        parameters=[{
            'use_sim_time': True,
            'pairing_strategy': 'nearest',
        }],
        output='screen',
        condition=is_lidar,
    )

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

    metrics = Node(
        package='lhr_metrics',
        executable='metrics_node',
        name='metrics_node',
        parameters=[{'use_sim_time': True}],
        output='screen',
        condition=IfCondition(LaunchConfiguration('enable_metrics')),
    )

    # ----- RViz -----
    rviz_node = Node(
        package='rviz2',
        executable='rviz2',
        name='rviz2',
        arguments=['-d', os.path.join(config_dir, 'default.rviz')],
        parameters=[{'use_sim_time': True}],
        output='screen',
        condition=IfCondition(LaunchConfiguration('rviz')),
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
        perception_arg,
        seed_arg,
        num_wp_arg,
        fov_arg,
        range_arg,
        lookahead_arg,
        lookahead_min_arg,
        lookahead_curv_gain_arg,
        a_lat_arg,
        v_min_arg,
        v_max_arg,
        max_accel_arg,
        max_decel_arg,
        metrics_arg,
        mission_arg,
        auto_go_arg,
        ready_hold_arg,
        rviz_arg,
        # Environment
        gz_model_path,
        # Gazebo physics (one of these will activate based on gui arg)
        gz_sim_gui,
        gz_sim_headless,
        # Bridge + adapter
        bridge,
        cmd_adapter,
        # Perception (one mode activates based on perception arg)
        cones,
        sensor_sim,
        lidar_perception,
        # Upper stack
        centerline_sim,
        centerline_lidar,
        mission_mgr,
        control,
        metrics,
        # Visualization
        rviz_node,
    ])
