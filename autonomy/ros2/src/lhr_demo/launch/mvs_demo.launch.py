"""Launch the full MVS autonomy demo stack."""

from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node


def generate_launch_description():
    # ----- Launch arguments -----
    seed_arg = DeclareLaunchArgument('seed', default_value='1')
    speed_arg = DeclareLaunchArgument('target_speed', default_value='5.0')
    lookahead_arg = DeclareLaunchArgument(
        'lookahead_dist', default_value='4.0')
    metrics_arg = DeclareLaunchArgument(
        'enable_metrics', default_value='false',
        description='Launch metrics node alongside the stack')

    # ----- Nodes -----
    cones = Node(
        package='lhr_trackgen',
        executable='publish_cones',
        name='publish_cones',
        parameters=[{'seed': LaunchConfiguration('seed')}],
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
        output='screen',
    )

    control = Node(
        package='lhr_control',
        executable='pursuit_node',
        name='pure_pursuit',
        parameters=[{
            'target_speed': LaunchConfiguration('target_speed'),
            'lookahead_dist': LaunchConfiguration('lookahead_dist'),
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

    return LaunchDescription([
        seed_arg,
        speed_arg,
        lookahead_arg,
        metrics_arg,
        cones,
        centerline,
        sim,
        control,
        metrics,
    ])
