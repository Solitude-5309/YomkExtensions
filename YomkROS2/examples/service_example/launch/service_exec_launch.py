from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
from launch.substitutions import LaunchConfiguration
from launch.actions import DeclareLaunchArgument
import os

def generate_launch_description():

    service_example_pkg_share = get_package_share_directory("service_example")
    config_path_arg = DeclareLaunchArgument(
        'service_exec_config_path',
        default_value=os.path.join(service_example_pkg_share, "config"),
        description='配置文件路径'
    )

    return LaunchDescription([
        config_path_arg,
        Node(
            package='service_example',
            executable='service_exec_node',
            name='service_exec_node',
            output='screen',
            parameters=[{
                'config_path': LaunchConfiguration('service_exec_config_path')
            }]
        )
    ])