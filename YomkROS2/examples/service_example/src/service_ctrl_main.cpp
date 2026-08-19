#include <YomkROS2/YomkROS2API.h>
#include <std_msgs/msg/string.hpp>
#include <rclcpp/rclcpp.hpp>
#include <example_interfaces/srv/add_two_ints.hpp>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <csignal>

using AddTwoInts = example_interfaces::srv::AddTwoInts;

static std::atomic<bool> g_stop{false}; // SIGINT（Ctrl+C）退出标志

static void onSignal(int)
{
    g_stop.store(true);
}

void asyncCb(AddTwoInts::Response::SharedPtr resp)
{
    RCLCPP_INFO(rclcpp::get_logger("service_ctrl_node"), "[ASYNC] 异步回调收到响应: sum=%ld", resp->sum);
};

int main(int argc, char **argv)
{
    YOMKROS2_NODE(argc, argv, "service_ctrl_node");

    std::string config_path;
    YOMKROS2_DECLARE_PARAM("config_path", config_path);
    YOMKROS2_GET_PARAM("config_path", config_path);
    RCLCPP_INFO(rclcpp::get_logger("service_ctrl_node"), "config_path: %s", config_path.c_str());

    YOMKROS2_SERVICE_CLIENT(AddTwoInts, "add_service");
    YOMKROS2_RUN(false);

    AddTwoInts::Request reqAdd;
    reqAdd.a = 1;
    reqAdd.b = 2;
    const auto respAdd = YOMKROS2_CALL_SERVICE(AddTwoInts, "add_service", reqAdd);
    RCLCPP_INFO(rclcpp::get_logger("service_ctrl_node"), "call add_service: %ld + %ld = %ld", reqAdd.a, reqAdd.b, respAdd->sum);

    AddTwoInts::Request reqAsync;
    reqAsync.a = 5;
    reqAsync.b = 6;
    YOMKROS2_CALL_SERVICE_ASYNC(AddTwoInts, "add_service", reqAsync, asyncCb);

    std::signal(SIGINT, onSignal);
    RCLCPP_INFO(rclcpp::get_logger("service_ctrl_node"), "subscribing hello_world, press Ctrl+C to exit");
    while (!g_stop.load())
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    RCLCPP_INFO(rclcpp::get_logger("service_ctrl_node"), "Ctrl+C pressed, shutting down");
    YOMKROS2_SHUTDOWN();
    return 0;
}
