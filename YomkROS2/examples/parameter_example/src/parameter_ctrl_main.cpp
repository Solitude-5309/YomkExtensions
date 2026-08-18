#include <YomkROS2/YomkROS2API.h>
#include <std_msgs/msg/string.hpp>
#include <rclcpp/rclcpp.hpp>
#include <vector>
#include <string>
#include <thread>

int main(int argc, char **argv)
{
    YOMKROS2_NODE(argc, argv, "parameter_ctrl_node");

    std::string config_path;
    YOMKROS2_DECLARE_PARAM("config_path", config_path);
    YOMKROS2_GET_PARAM("config_path", config_path);
    RCLCPP_INFO(rclcpp::get_logger("parameter_ctrl_node"), "config_path: %s", config_path.c_str());

    YOMKROS2_PARAM_CLIENT("parameter_exec_node");
    YOMKROS2_RUN(false);

    std::this_thread::sleep_for(std::chrono::seconds(2)); // 等待节点进入运行态

    if (YOMKROS2_HAS_REMOTE_PARAM("parameter_exec_node", "state"))
    {
        RCLCPP_INFO(rclcpp::get_logger("parameter_ctrl_node"), "parameter_exec_node param state exists");
    }
    else
    {
        RCLCPP_INFO(rclcpp::get_logger("parameter_ctrl_node"), "parameter_exec_node param state does not exist");
    }

    int state;
    YOMKROS2_GET_REMOTE_PARAM("parameter_exec_node", "state", state);
    RCLCPP_INFO(rclcpp::get_logger("parameter_ctrl_node"), "get parameter_exec_node parameter state: %d", state);

    YOMKROS2_SET_REMOTE_PARAM("parameter_exec_node", "state", 100);
    RCLCPP_INFO(rclcpp::get_logger("parameter_ctrl_node"), "set parameter_exec_node parameter state: 100");
    YOMKROS2_GET_REMOTE_PARAM("parameter_exec_node", "state", state);
    RCLCPP_INFO(rclcpp::get_logger("parameter_ctrl_node"), "get parameter_exec_node parameter state: %d", state);

    std::vector<std::string> params = YOMKROS2_LIST_REMOTE_PARAMS("parameter_exec_node");
    for (const std::string &param : params)
    {
        RCLCPP_INFO(rclcpp::get_logger("parameter_ctrl_node"), "parameter_exec_node param: %s", param.c_str());
    }
    
    YOMKROS2_SHUTDOWN();
    return 0;
}
