#include <YomkServer/YomkAPI.h>
#include <YomkROS2/YomkROS2API.h>

#include <std_msgs/msg/string.hpp>
#include <std_msgs/msg/int32.hpp>
#include <std_srvs/srv/set_bool.hpp>
#include <example_interfaces/srv/add_two_ints.hpp>
#include <example_interfaces/action/fibonacci.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

using namespace yomk;
using AddTwoInts = example_interfaces::srv::AddTwoInts;
using SetBool = std_srvs::srv::SetBool;
using Fibonacci = example_interfaces::action::Fibonacci;
using ServerGoalHandle = rclcpp_action::ServerGoalHandle<Fibonacci>;

// 双进程集成测试——执行端（服务端角色，用户参考程序）：
// 与 TestYomkROS2Control（control_node，客户端角色）分属两个独立进程，
// 演示真实跨进程使用：订阅 2 个 topic、声明并被远端修改 2 个参数、
// 提供 2 个服务、执行 2 个动作。
// 自适应终结：全部完成条件满足后 shutdown 并以退出码 0 退出；30s 未满足退出码 1。

// 完成状态统计（回调在 executor 线程池并发执行，原子变量保证线程安全）
static std::atomic<int> g_statusCount{0};  // ctrl_status 收到计次
static std::atomic<int> g_counterCount{0}; // ctrl_counter 收到计次
static std::atomic<int> g_addCount{0};     // exec_add 处理计次
static std::atomic<int> g_enableCount{0};  // exec_enable 处理计次
static std::atomic<int> g_actionDone{0};   // 动作终结计次（两个动作各一次）

// 动作执行逻辑：用户自开线程逐步计算 Fibonacci（每步 50ms + 发布 feedback +
// 检查取消），终结后计次；执行线程生命周期由用户自管
static void runFib(std::shared_ptr<ServerGoalHandle> goalHandle)
{
    std::thread([goalHandle]()
                {
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
                            g_actionDone++;
                            return;
                        }
                        sequence.push_back(sequence[i] + sequence[i - 1]);
                        goalHandle->publish_feedback(feedback);
                        std::this_thread::sleep_for(std::chrono::milliseconds(50));
                    }
                    auto result = std::make_shared<Fibonacci::Result>();
                    result->sequence = sequence;
                    goalHandle->succeed(result);
                    YOMK_INFO_TAG("TestYomkROS2Exec", "[ACTION] order=", order, " 完成，序列长度=", sequence.size());
                    g_actionDone++; })
        .detach();
}

int main(int argc, char *argv[])
{
    YOMK_INIT();

    // 1. 创建节点
    if (!YOMKROS2_NODE(argc, argv, "exec_node"))
    {
        YOMK_ERROR_TAG("TestYomkROS2Exec", "[FAIL] YOMKROS2_NODE 初始化");
        return 1;
    }

    // 2. 声明两个参数（等待 control_node 远程修改：exec_speed 10→20、exec_name idle→working）
    const int64_t initSpeed = YOMKROS2_DECLARE_PARAM("exec_speed", int64_t(10));
    const std::string initName = YOMKROS2_DECLARE_PARAM("exec_name", std::string("idle"));
    YOMK_INFO_TAG("TestYomkROS2Exec", "[PARAM] 声明 exec_speed=", initSpeed, " exec_name=", initName);

    // 3. 订阅两个 topic（control_node 周期发布）
    auto statusCb = [](std::shared_ptr<const std_msgs::msg::String> msg)
    {
        if (g_statusCount.load() == 0)
        {
            YOMK_INFO_TAG("TestYomkROS2Exec", "[TOPIC] ctrl_status 首次收到: ", msg->data);
        }
        g_statusCount++;
    };
    auto counterCb = [](std::shared_ptr<const std_msgs::msg::Int32> msg)
    {
        if (g_counterCount.load() == 0)
        {
            YOMK_INFO_TAG("TestYomkROS2Exec", "[TOPIC] ctrl_counter 首次收到: ", msg->data);
        }
        g_counterCount++;
    };
    if (!YOMKROS2_SUB_TOPIC(std_msgs::msg::String, "ctrl_status", 10, statusCb) ||
        !YOMKROS2_SUB_TOPIC(std_msgs::msg::Int32, "ctrl_counter", 10, counterCb))
    {
        YOMK_ERROR_TAG("TestYomkROS2Exec", "[FAIL] YOMKROS2_SUB_TOPIC 订阅");
        return 1;
    }

    // 4. 注册两个服务（exec_add 加法、exec_enable 使能）
    auto addCb = [](const std::shared_ptr<AddTwoInts::Request> req,
                    std::shared_ptr<AddTwoInts::Response> resp)
    {
        resp->sum = req->a + req->b;
        YOMK_INFO_TAG("TestYomkROS2Exec", "[SERVICE] exec_add: a=", req->a, " b=", req->b, " -> sum=", resp->sum);
        g_addCount++;
    };
    auto enableCb = [](const std::shared_ptr<SetBool::Request> req,
                       std::shared_ptr<SetBool::Response> resp)
    {
        resp->success = true;
        resp->message = req->data ? "exec_node enabled" : "exec_node disabled";
        YOMK_INFO_TAG("TestYomkROS2Exec", "[SERVICE] exec_enable: data=", req->data, " -> ", resp->message);
        g_enableCount++;
    };
    if (!YOMKROS2_SERVICE(AddTwoInts, "exec_add", addCb) ||
        !YOMKROS2_SERVICE(SetBool, "exec_enable", enableCb))
    {
        YOMK_ERROR_TAG("TestYomkROS2Exec", "[FAIL] YOMKROS2_SERVICE 注册");
        return 1;
    }

    // 5. 注册两个动作服务端（三回调：全部接受 / 允许取消 / 自开线程执行）
    auto goalCb = [](const std::shared_ptr<const Fibonacci::Goal> goal) -> int
    {
        YOMK_INFO_TAG("TestYomkROS2Exec", "[ACTION] 收到目标 order=", goal->order, "，接受");
        return 2; // 接受并立即执行
    };
    auto cancelCb = [](std::shared_ptr<ServerGoalHandle>) -> bool
    { return true; }; // 允许取消
    auto execCb = [](std::shared_ptr<ServerGoalHandle> goalHandle)
    { runFib(goalHandle); };
    if (!YOMKROS2_ACTION(Fibonacci, "exec_fib", goalCb, cancelCb, execCb) ||
        !YOMKROS2_ACTION(Fibonacci, "exec_fib_fast", goalCb, cancelCb, execCb))
    {
        YOMK_ERROR_TAG("TestYomkROS2Exec", "[FAIL] YOMKROS2_ACTION 注册");
        return 1;
    }

    // 6. 后台 spin（回调由 executor 线程池驱动）
    if (!YOMKROS2_RUN(false))
    {
        YOMK_ERROR_TAG("TestYomkROS2Exec", "[FAIL] YOMKROS2_RUN 运行节点");
        return 1;
    }
    YOMK_INFO_TAG("TestYomkROS2Exec", "exec_node 就绪，等待 control_node 通信...");

    // 7. 轮询完成条件：2 topic 各收到 ≥1 条 + 2 服务各处理 ≥1 次 +
    //    2 动作均终结 + 参数已被远程修改为目标值；30s 超时
    bool done = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(30);
    while (std::chrono::steady_clock::now() < deadline)
    {
        int64_t speed = 0;
        std::string name;
        YOMKROS2_GET_PARAM("exec_speed", speed);
        YOMKROS2_GET_PARAM("exec_name", name);
        if (g_statusCount.load() >= 1 && g_counterCount.load() >= 1 &&
            g_addCount.load() >= 1 && g_enableCount.load() >= 1 &&
            g_actionDone.load() >= 2 && speed == 20 && name == "working")
        {
            done = true;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    YOMKROS2_SHUTDOWN();
    if (done)
    {
        YOMK_INFO_TAG("TestYomkROS2Exec", "[DONE] 全部完成条件满足: topic=", g_statusCount.load(), "/", g_counterCount.load(),
                      " service=", g_addCount.load(), "/", g_enableCount.load(),
                      " action=", g_actionDone.load(), " 参数已更新，干净退出");
        return 0;
    }
    YOMK_ERROR_TAG("TestYomkROS2Exec", "[FAIL] 30s 内完成条件未满足: topic=", g_statusCount.load(), "/", g_counterCount.load(),
                   " service=", g_addCount.load(), "/", g_enableCount.load(), " action=", g_actionDone.load());
    return 1;
}
