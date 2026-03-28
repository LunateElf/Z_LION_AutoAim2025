
# 启动相机与串口节点，并支持 hik/mindvision 相机切换

from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
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
    camera_type = LaunchConfiguration('camera_type')

    return LaunchDescription([
        DeclareLaunchArgument(
            name='profile',
            default_value='default',
            description='YAML profile name: default/hero/sentry/infantry'
        ),
        params_file,
        DeclareLaunchArgument(
            name='camera_type',
            default_value='hik',
            description='Camera driver type: hik or mindvision'
        ),
        DeclareLaunchArgument(
            name='mv_camera_info_url',
            default_value='package://mindvision_camera/config/camera_info.yaml',
            description='MindVision camera info url'
        ),
        DeclareLaunchArgument(
            name='hik_camera_info_url',
            default_value='package://hik_camera/config/camera_info.yaml',
            description='Hik camera info url'
        ),
        DeclareLaunchArgument(name='use_sensor_data_qos',
                              default_value='false'),

        Node(
            package='hik_camera',
            executable='hik_camera_node',
            output='screen',
            emulate_tty=True,
            condition=IfCondition(PythonExpression(["'", camera_type, "' == 'hik'"])),
            parameters=[LaunchConfiguration('params_file'), {
                'camera_info_url': LaunchConfiguration('hik_camera_info_url'),
                'use_sensor_data_qos': LaunchConfiguration('use_sensor_data_qos'),
            }],
        ),

        Node(
            package='mindvision_camera',
            executable='mindvision_camera_node',
            output='screen',
            emulate_tty=True,
            condition=IfCondition(PythonExpression(["'", camera_type, "' == 'mindvision'"])),
            parameters=[LaunchConfiguration('params_file'), {
                'camera_info_url': LaunchConfiguration('mv_camera_info_url'),
                'use_sensor_data_qos': LaunchConfiguration('use_sensor_data_qos'),
            }],
        ),

        # Node(
        #     package='auto_aim',
        #     executable='serial_read_data_node',
        #     name='serial_read_data_node',
        #     parameters=[LaunchConfiguration('params_file')],
        #     output='screen',
        # ),

    ])