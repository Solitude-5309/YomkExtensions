#include <YomkROS2/YomkROS2API.h>
#include <std_msgs/msg/string.hpp>
#include <rclcpp/rclcpp.hpp>
#include <vector>
#include <string>

int main(int argc, char **argv)
{
    YOMKROS2_NODE(argc, argv, "parameter_exec_node");

    std::string config_path;
    YOMKROS2_DECLARE_PARAM("config_path", config_path);
    YOMKROS2_GET_PARAM("config_path", config_path);
    RCLCPP_INFO(rclcpp::get_logger("parameter_exec_node"), "config_path: %s", config_path.c_str());

    YOMKROS2_DECLARE_PARAM("state", 0);
    int state;
    YOMKROS2_GET_PARAM("state", state);
    RCLCPP_INFO(rclcpp::get_logger("parameter_exec_node"), "get state: %d", state);

    YOMKROS2_SET_PARAM("state", 1);
    RCLCPP_INFO(rclcpp::get_logger("parameter_exec_node"), "set state: 1");
    YOMKROS2_GET_PARAM("state", state);
    RCLCPP_INFO(rclcpp::get_logger("parameter_exec_node"), "get state: %d", state);

    if (YOMKROS2_HAS_PARAM("state"))
    {
        RCLCPP_INFO(rclcpp::get_logger("parameter_exec_node"), "param state exists");
    }
    else
    {
        RCLCPP_INFO(rclcpp::get_logger("parameter_exec_node"), "param state does not exist");
    }

    std::vector<std::string> params = YOMKROS2_LIST_PARAMS();
    for (const std::string &param : params)
    {
        RCLCPP_INFO(rclcpp::get_logger("parameter_exec_node"), "param: %s", param.c_str());
    }

    YOMKROS2_RUN(true);
    YOMKROS2_SHUTDOWN();
    return 0;
}
