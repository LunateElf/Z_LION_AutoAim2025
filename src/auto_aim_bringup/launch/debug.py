
# 启动整套自瞄链路，并支持 hik/mindvision 相机切换

from launch import LaunchDescription
from launch_ros.actions import Node
from launch.actions import DeclareLaunchArgument, SetEnvironmentVariable
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration, PathJoinSubstitution, EnvironmentVariable, PythonExpression
from launch_ros.substitutions import FindPackageShare

def generate_launch_description():
    # 定义参数文件路径
    params_file = DeclareLaunchArgument(
        'params_file',
        default_value=PathJoinSubstitution([FindPackageShare('auto_aim_bringup'), 'config', 'default.yaml']),
        description='Full path to the parameter file for send_trans_processor_node'
    )

    camera_type = LaunchConfiguration('camera_type')

    return LaunchDescription([
        SetEnvironmentVariable(
            name='LD_LIBRARY_PATH',
            value=[
                '/opt/intel/openvino_2024.6.0/runtime/lib/aarch64:',
                EnvironmentVariable('LD_LIBRARY_PATH', default_value='')
            ]
        ),
        params_file,
        DeclareLaunchArgument(
            name='camera_type',
            default_value='mindvision',
            description='Camera driver type: mindvision or hik'
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

        # Node(
        #     package='auto_aim',
        #     executable='serial_read_data_node',
        #     name='serial_read_data_node',
        #     parameters=[LaunchConfiguration('params_file')],
        #     output='screen',
        # ),
        
        Node(
            package='auto_aim',
            executable='serial_send_data_node',
            name='serial_send_data_node',
            parameters=[LaunchConfiguration('params_file')],
            output='screen',
        ),
        
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

    ])