#include <YomkROS2/YomkROS2API.h>
#include <std_msgs/msg/string.hpp>
#include <rclcpp/rclcpp.hpp>

void onMessage(std::shared_ptr<const std_msgs::msg::String> msg)
{
    RCLCPP_INFO(rclcpp::get_logger("topic_sub_node"), "[RECV] 收到消息: %s", msg->data.c_str());
}
int main(int argc, char **argv)
{
    YOMKROS2_NODE(argc, argv, "topic_sub_node");
    YOMKROS2_SUB_TOPIC(std_msgs::msg::String, "hello_topic", 10, onMessage);
    YOMKROS2_RUN(true);
    YOMKROS2_SHUTDOWN();
    return 0;
}
