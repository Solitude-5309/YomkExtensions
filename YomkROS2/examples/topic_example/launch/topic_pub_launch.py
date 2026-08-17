from launch import LaunchDescription
from launch_ros.actions import Node
from ament_index_python.packages import get_package_share_directory
from launch.substitutions import LaunchConfiguration
from launch.actions import DeclareLaunchArgument
import os

def generate_launch_description():

    topic_example_pkg_share = get_package_share_directory("topic_example")
    config_path_arg = DeclareLaunchArgument(
        'topic_pub_config_path',
        default_value=os.path.join(topic_example_pkg_share, "config"),
        description='配置文件路径'
    )

    return LaunchDescription([
        config_path_arg,
        Node(
            package='topic_example',
            executable='topic_pub_node',
            name='topic_pub_node',
            output='screen',
            parameters=[{
                'config_path': LaunchConfiguration('topic_pub_config_path')
            }]
        )
    ])