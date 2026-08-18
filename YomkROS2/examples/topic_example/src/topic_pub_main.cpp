#include <YomkROS2/YomkROS2API.h>
#include <std_msgs/msg/string.hpp>
#include <thread>

void pubMessage()
{
    std::this_thread::sleep_for(std::chrono::seconds(3));
    std_msgs::msg::String msg;
    msg.data = "Hello, YomkROS2!";
    RCLCPP_INFO(rclcpp::get_logger("topic_pub_node"), "[SEND] 发送消息: %s", msg.data.c_str());
    YOMKROS2_PUB_MSG("hello_topic", msg);
}

int main(int argc, char **argv)
{
    YOMKROS2_NODE(argc, argv, "topic_pub_node");

    std::string config_path;
    YOMKROS2_DECLARE_PARAM("config_path", config_path);
    YOMKROS2_GET_PARAM("config_path", config_path);
    RCLCPP_INFO(rclcpp::get_logger("topic_pub_node"), "config_path: %s", config_path.c_str());

    YOMKROS2_PUB_TOPIC(std_msgs::msg::String, "hello_topic", 10);
    std::thread pubThread(pubMessage);
    YOMKROS2_RUN(true);
    pubThread.join();
    YOMKROS2_SHUTDOWN();
    return 0;
}
