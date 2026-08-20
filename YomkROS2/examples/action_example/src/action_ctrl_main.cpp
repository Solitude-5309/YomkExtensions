#include <YomkROS2/YomkROS2API.h>
#include <std_msgs/msg/string.hpp>
#include <rclcpp/rclcpp.hpp>
#include <example_interfaces/action/fibonacci.hpp>
#include <vector>
#include <string>
#include <thread>
#include <atomic>
#include <csignal>

using Fibonacci = example_interfaces::action::Fibonacci;
using ServerGoalHandle = rclcpp_action::ServerGoalHandle<Fibonacci>;
using ResultCode = rclcpp_action::ResultCode;

static std::atomic<bool> g_stop{false}; // SIGINT（Ctrl+C）退出标志

static void onSignal(int)
{
    g_stop.store(true);
}

void goalCb(uint64_t goalId, bool accepted)
{
    RCLCPP_INFO(rclcpp::get_logger("action_ctrl_node"), "[GOAL] goalId=%ld  accepted=%s", goalId, accepted ? "true" : "false");
};
void feedbackCb(uint64_t goalId,
                const std::shared_ptr<const Fibonacci::Feedback> feedback)
{
    RCLCPP_INFO(rclcpp::get_logger("action_ctrl_node"), "[FEEDBACK] goalId=%ld  feedbackLen=%zu", goalId, feedback->sequence.size());
};
void resultCb(uint64_t goalId, rclcpp_action::ResultCode code,
              const std::shared_ptr<const Fibonacci::Result> result)
{
    RCLCPP_INFO(rclcpp::get_logger("action_ctrl_node"), "[RESULT] goalId=%ld  code=%d  resultLen=%zu", goalId, static_cast<int>(code), result ? result->sequence.size() : 0);
};

int main(int argc, char **argv)
{
    YOMKROS2_NODE(argc, argv, "action_ctrl_node");

    std::string config_path;
    YOMKROS2_DECLARE_PARAM("config_path", config_path);
    YOMKROS2_GET_PARAM("config_path", config_path);
    RCLCPP_INFO(rclcpp::get_logger("action_ctrl_node"), "config_path: %s", config_path.c_str());

    YOMKROS2_ACTION_CLIENT(Fibonacci, "fib_action");
    YOMKROS2_RUN(false);
    RCLCPP_INFO(rclcpp::get_logger("action_ctrl_node"), "subscribing hello_world, press Ctrl+C to exit");

    Fibonacci::Goal goal;
    goal.order = 9;
    const uint64_t id = YOMKROS2_CALL_ACTION_ASYNC(Fibonacci, "fib_action", goal, goalCb, feedbackCb, resultCb);
    RCLCPP_INFO(rclcpp::get_logger("action_ctrl_node"), "[GOAL] sent goalId=%ld, action execute goal, but cancel goal run action, after 8 seconds", id);

    std::this_thread::sleep_for(std::chrono::seconds(8));
    YOMKROS2_CANCEL_GOAL(Fibonacci, "fib_action", id);
    RCLCPP_INFO(rclcpp::get_logger("action_ctrl_node"), "[GOAL] cancel goalId=%ld", id);

    std::signal(SIGINT, onSignal);
    while (!g_stop.load())
    {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    RCLCPP_INFO(rclcpp::get_logger("action_ctrl_node"), "Ctrl+C pressed, shutting down");
    YOMKROS2_SHUTDOWN();
    return 0;
}
