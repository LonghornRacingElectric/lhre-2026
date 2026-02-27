"""Launch the full MVS autonomy demo stack."""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    # ----- Launch arguments -----
    seed_arg = DeclareLaunchArgument('seed', default_value='1')
    lookahead_arg = DeclareLaunchArgument(
        'lookahead_dist', default_value='4.0')
    metrics_arg = DeclareLaunchArgument(
        'enable_metrics', default_value='true',
        description='Launch metrics node alongside the stack')
    track_style_arg = DeclareLaunchArgument(
        'track_style', default_value='autocross',
        description='Track generator: autocross | simple')
    num_wp_arg = DeclareLaunchArgument(
        'num_waypoints', default_value='10')

    # Speed planning
    a_lat_arg = DeclareLaunchArgument('a_lat_max', default_value='6.0')
    v_min_arg = DeclareLaunchArgument('v_min', default_value='2.0')
    v_max_arg = DeclareLaunchArgument('v_max', default_value='12.0')
    max_accel_arg = DeclareLaunchArgument('max_accel', default_value='2.0')
    max_decel_arg = DeclareLaunchArgument('max_decel', default_value='3.0')

    # Sensor sim
    fov_arg = DeclareLaunchArgument('fov_deg', default_value='200.0')
    range_arg = DeclareLaunchArgument('max_range_m', default_value='20.0')

    # Initial pose
    init_x_arg = DeclareLaunchArgument('init_x', default_value='25.0')
    init_y_arg = DeclareLaunchArgument('init_y', default_value='0.0')
    init_yaw_arg = DeclareLaunchArgument('init_yaw', default_value='1.5708')

    # Mission manager
    mission_arg = DeclareLaunchArgument(
        'mission', default_value='autocross',
        description='Mission: inspection | manual | ebs_test '
                    '| acceleration | skidpad | autocross')
    auto_go_arg = DeclareLaunchArgument(
        'auto_go', default_value='true',
        description='Auto-transition READY -> DRIVING after hold time')
    ready_hold_arg = DeclareLaunchArgument(
        'ready_hold_sec', default_value='5.0',
        description='Seconds to hold in READY before auto-go')

    # ----- Nodes -----
    cones = Node(
        package='lhr_trackgen',
        executable='publish_cones',
        name='publish_cones',
        parameters=[{
            'seed': LaunchConfiguration('seed'),
            'track_style': LaunchConfiguration('track_style'),
            'num_waypoints': LaunchConfiguration('num_waypoints'),
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
        }],
        output='screen',
    )

    centerline = Node(
        package='lhr_track_builder',
        executable='track_builder',
        name='track_builder',
        output='screen',
    )

    sim = Node(
        package='lhr_sim_kinematic',
        executable='sim_node',
        name='sim_kinematic',
        parameters=[{
            'init_x': LaunchConfiguration('init_x'),
            'init_y': LaunchConfiguration('init_y'),
            'init_yaw': LaunchConfiguration('init_yaw'),
        }],
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
        seed_arg,
        lookahead_arg,
        metrics_arg,
        track_style_arg,
        num_wp_arg,
        a_lat_arg,
        v_min_arg,
        v_max_arg,
        max_accel_arg,
        max_decel_arg,
        fov_arg,
        range_arg,
        init_x_arg,
        init_y_arg,
        init_yaw_arg,
        mission_arg,
        auto_go_arg,
        ready_hold_arg,
        cones,
        sensor_sim,
        centerline,
        sim,
        mission_mgr,
        control,
        metrics,
    ])
