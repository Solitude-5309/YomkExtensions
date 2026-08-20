#include <YomkROS2/YomkROS2API.h>
#include <std_msgs/msg/string.hpp>
#include <rclcpp/rclcpp.hpp>
#include <example_interfaces/action/fibonacci.hpp>
#include <vector>
#include <string>
#include <thread>
using Fibonacci = example_interfaces::action::Fibonacci;
using ServerGoalHandle = rclcpp_action::ServerGoalHandle<Fibonacci>;
using ResultCode = rclcpp_action::ResultCode;

static void runFib(std::shared_ptr<ServerGoalHandle> goalHandle)
{
    std::thread([goalHandle]()
                {
                    RCLCPP_INFO(rclcpp::get_logger("action_exec_node"), "[ACTION] 执行线程: 1秒后执行");
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    const int order = goalHandle->get_goal()->order;
                    auto feedback = std::make_shared<Fibonacci::Feedback>();
                    auto &sequence = feedback->sequence;
                    sequence.push_back(0);
                    sequence.push_back(1);
                    for (int i = 1; i < order && rclcpp::ok(); ++i)
                    {
                        if (goalHandle->is_canceling())
                        {
                            auto result = std::make_shared<Fibonacci::Result>();
                            result->sequence = sequence;
                            goalHandle->canceled(result);
                            RCLCPP_INFO(rclcpp::get_logger("action_exec_node"), "[ACTION] 执行线程: order=%d 已取消", order);
                            return;
                        }
                        sequence.push_back(sequence[i] + sequence[i - 1]);
                        goalHandle->publish_feedback(feedback);
                        RCLCPP_INFO(rclcpp::get_logger("action_exec_node"), "[ACTION] 执行线程: order=%d feedback... i=%d", order, i);
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                    }
                    auto result = std::make_shared<Fibonacci::Result>();
                    result->sequence = sequence;
                    goalHandle->succeed(result);
                    RCLCPP_INFO(rclcpp::get_logger("action_exec_node"), "[ACTION] 执行线程: order=%d 完成，序列长度=%ld", order, sequence.size()); })
        .detach();
}

int goalCb(const std::shared_ptr<const Fibonacci::Goal> goal)
{
    if (goal->order < 0)
    {
        RCLCPP_INFO(rclcpp::get_logger("action_exec_node"), "[ACTION] 拒绝无效的 goal: order=%d", goal->order);
        return 1; // REJECT
    }
    if (goal->order == 7)
    {
        return 3; // ACCEPT_AND_DEFER
    }
    RCLCPP_INFO(rclcpp::get_logger("action_exec_node"), "[ACTION] 接受 goal: order=%d", goal->order);
    return 2; // ACCEPT_AND_EXECUTE
};
bool cancelCb(std::shared_ptr<ServerGoalHandle> goalHandle)
{
    bool canCancel = goalHandle->get_goal()->order != 10;
    if (canCancel)
    {
        RCLCPP_INFO(rclcpp::get_logger("action_exec_node"), "[ACTION] 接受取消请求: order=%d", goalHandle->get_goal()->order);
    }
    else
    {
        RCLCPP_INFO(rclcpp::get_logger("action_exec_node"), "[ACTION] 拒绝取消请求: order=%d", goalHandle->get_goal()->order);
    }
    return canCancel; // order==10 拒绝取消（危险任务示例）
};
void execCb(std::shared_ptr<ServerGoalHandle> goalHandle)
{
    RCLCPP_INFO(rclcpp::get_logger("action_exec_node"), "[ACTION] 执行回调触发（executor 线程）: order=%d", goalHandle->get_goal()->order);
    runFib(goalHandle); // 用户自行开线程执行长耗时任务
};

int main(int argc, char **argv)
{
    YOMKROS2_NODE(argc, argv, "action_exec_node");

    std::string config_path;
    YOMKROS2_DECLARE_PARAM("config_path", config_path);
    YOMKROS2_GET_PARAM("config_path", config_path);
    RCLCPP_INFO(rclcpp::get_logger("action_exec_node"), "config_path: %s", config_path.c_str());

    YOMKROS2_ACTION(Fibonacci, "fib_action", goalCb, cancelCb, execCb);
    RCLCPP_INFO(rclcpp::get_logger("action_exec_node"), "[ACTION] fib_action已启动");

    YOMKROS2_RUN(true);
    YOMKROS2_SHUTDOWN();
    return 0;
}
