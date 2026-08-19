#include <YomkROS2/YomkROS2API.h>
#include <std_msgs/msg/string.hpp>
#include <rclcpp/rclcpp.hpp>
#include <example_interfaces/srv/add_two_ints.hpp>
#include <vector>
#include <string>
#include <thread>

using AddTwoInts = example_interfaces::srv::AddTwoInts;

void addCb(const std::shared_ptr<AddTwoInts::Request> request, std::shared_ptr<AddTwoInts::Response> response)
{
    response->sum = request->a + request->b;
    RCLCPP_INFO(rclcpp::get_logger("service_exec_node"), "[SERVICE] add_service: 收到请求 a=%d b=%d", request->a, request->b,
                " -> 响应 sum=%d", response->sum);
    RCLCPP_INFO(rclcpp::get_logger("service_exec_node"), "[SERVICE] add_service: 响应已发送");
};

int main(int argc, char **argv)
{
    YOMKROS2_NODE(argc, argv, "service_exec_node");

    std::string config_path;
    YOMKROS2_DECLARE_PARAM("config_path", config_path);
    YOMKROS2_GET_PARAM("config_path", config_path);
    RCLCPP_INFO(rclcpp::get_logger("service_exec_node"), "config_path: %s", config_path.c_str());

    YOMKROS2_SERVICE(AddTwoInts, "add_service", addCb);

    YOMKROS2_RUN(true);
    YOMKROS2_SHUTDOWN();
    return 0;
}
