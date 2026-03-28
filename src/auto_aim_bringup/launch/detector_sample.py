
# 只启动相机与串口节点

from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution, PythonExpression
from launch_ros.substitutions import FindPackageShare

def generate_launch_description():
    config_profile = LaunchConfiguration('profile')

    # 定义参数文件路径
    params_file = DeclareLaunchArgument(
        'params_file',
        default_value=PathJoinSubstitution([
            FindPackageShare('auto_aim_bringup'),
            'config',
            PythonExpression(["'", config_profile, "' + '.yaml'"])
        ]),
        description='Full path to the parameter file for send_trans_processor_node'
    )


    return LaunchDescription([
        DeclareLaunchArgument(
            name='profile',
            default_value='default',
            description='YAML profile name: default/hero/sentry/infantry'
        ),
        params_file,
        DeclareLaunchArgument(name='use_sensor_data_qos',
                              default_value='false'),
        
        Node(
            package='auto_aim',
            executable='auto_aim_detector_node',
            name='auto_aim_detector_node',
            parameters=[LaunchConfiguration('params_file')],
            output='screen',
        ),
        
        Node(
            package='auto_aim',
            executable='auto_aim_processor_node',
            name='auto_aim_processor_node',
            parameters=[LaunchConfiguration('params_file')],
            output='screen',
        ),

        Node(
            package='auto_aim_debug',
            executable='auto_aim_debug_node',
            name='auto_aim_debug_node',
            parameters=[LaunchConfiguration('params_file')],
            output='screen',
        ),

        Node(
            package='auto_aim_publisher',
            executable='send_image_node',
            name='send_image_node',
            parameters=[LaunchConfiguration('params_file')],
            output='screen',
        ),

        Node(
            package='auto_aim_publisher',
            executable='send_serial_read_data_node',
            name='send_serial_read_data_node',
            parameters=[LaunchConfiguration('params_file')],
            output='screen',
        ),

    ])