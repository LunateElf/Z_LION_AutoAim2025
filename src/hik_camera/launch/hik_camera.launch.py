from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution, PythonExpression
from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare


def generate_launch_description():
    config_profile = LaunchConfiguration('profile')

    camera_info_url = 'package://hik_camera/config/camera_info.yaml'

    return LaunchDescription([
        DeclareLaunchArgument(
            name='profile',
            default_value='default',
            description='YAML profile name from auto_aim_bringup config: default/hero/sentry/infantry'
        ),
        DeclareLaunchArgument(
            name='params_file',
            default_value=PathJoinSubstitution([
                FindPackageShare('auto_aim_bringup'),
                'config',
                PythonExpression(["'", config_profile, "' + '.yaml'"])
            ]),
            description='Full path to parameter file; override to use custom yaml'
        ),
        DeclareLaunchArgument(name='camera_info_url',
                              default_value=camera_info_url),
        DeclareLaunchArgument(name='use_sensor_data_qos',
                              default_value='false'),

        Node(
            package='hik_camera',
            executable='hik_camera_node',
            output='screen',
            emulate_tty=True,
            parameters=[LaunchConfiguration('params_file'), {
                'camera_info_url': LaunchConfiguration('camera_info_url'),
                'use_sensor_data_qos': LaunchConfiguration('use_sensor_data_qos'),
            }],
        )
    ])
