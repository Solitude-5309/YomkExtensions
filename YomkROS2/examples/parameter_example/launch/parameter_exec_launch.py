from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
from launch.substitutions import LaunchConfiguration
from launch.actions import DeclareLaunchArgument
import os

def generate_launch_description():

    parameter_example_pkg_share = get_package_share_directory("parameter_example")
    config_path_arg = DeclareLaunchArgument(
        'parameter_exec_config_path',
        default_value=os.path.join(parameter_example_pkg_share, "config"),
        description='配置文件路径'
    )

    return LaunchDescription([
        config_path_arg,
        Node(
            package='parameter_example',
            executable='parameter_exec_node',
            name='parameter_exec_node',
            output='screen',
            parameters=[{
                'config_path': LaunchConfiguration('parameter_exec_config_path')
            }]
        )
    ])