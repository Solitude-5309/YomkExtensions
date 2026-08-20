from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
from launch.substitutions import LaunchConfiguration
from launch.actions import DeclareLaunchArgument
import os

def generate_launch_description():

    action_example_pkg_share = get_package_share_directory("action_example")
    config_path_arg = DeclareLaunchArgument(
        'action_ctrl_config_path',
        default_value=os.path.join(action_example_pkg_share, "config"),
        description='配置文件路径'
    )

    return LaunchDescription([
        config_path_arg,
        Node(
            package='action_example',
            executable='action_ctrl_node',
            name='action_ctrl_node',
            output='screen',
            parameters=[{
                'config_path': LaunchConfiguration('action_ctrl_config_path')
            }]
        )
    ])