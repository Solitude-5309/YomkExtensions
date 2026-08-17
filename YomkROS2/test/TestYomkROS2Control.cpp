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
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

using namespace yomk;
using AddTwoInts = example_interfaces::srv::AddTwoInts;
using SetBool = std_srvs::srv::SetBool;
using Fibonacci = example_interfaces::action::Fibonacci;

// 双进程集成测试——控制端（客户端角色，用户参考程序）：
// 与 TestYomkROS2Exec（exec_node，服务端角色）分属两个独立进程，
// 演示真实跨进程使用：发布 2 个 topic、远程设置 2 个参数、请求 2 个服务、
// 发送 2 个动作目标。全部校验通过退出码 0，任一失败退出码 1。
// 客户端严格遵守推荐顺序：创建节点 → run 前预创建客户端 → run → 通信。

// 动作结果记录（回调由后台 spin 线程调用，统一加锁）；以动作名为键（每动作 1 个目标）
static std::mutex g_mtx;
static std::map<std::string, bool> g_accepted;  // 动作名 → 目标接受/拒绝
static std::map<std::string, int> g_resultCode; // 动作名 → 结果码
static std::map<std::string, size_t> g_seqLen;  // 动作名 → result.sequence 长度

// 轮询等待谓词成立（默认最多 15s）
template <typename Pred>
static bool waitFor(Pred pred, int maxMs = 15000)
{
    for (int waited = 0; waited < maxMs; waited += 100)
    {
        if (pred())
        {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    return pred();
}

int main(int argc, char *argv[])
{
    YOMK_INIT();

    // 1. 创建节点
    if (!YOMKROS2_NODE(argc, argv, "control_node"))
    {
        YOMK_ERROR_TAG("TestYomkROS2Control", "[FAIL] YOMKROS2_NODE 初始化");
        return 1;
    }

    // 2. run 前预创建参数/服务/动作客户端（跨进程调用必须如此，spin 期间动态创建可能收不到响应）
    if (!YOMKROS2_PARAM_CLIENT("exec_node") ||
        !YOMKROS2_SERVICE_CLIENT(AddTwoInts, "exec_add") || !YOMKROS2_SERVICE_CLIENT(SetBool, "exec_enable") ||
        !YOMKROS2_ACTION_CLIENT(Fibonacci, "exec_fib") || !YOMKROS2_ACTION_CLIENT(Fibonacci, "exec_fib_fast"))
    {
        YOMK_ERROR_TAG("TestYomkROS2Control", "[FAIL] 预创建客户端");
        return 1;
    }

    // 3. 注册两个发布主题
    if (!YOMKROS2_PUB_TOPIC(std_msgs::msg::String, "ctrl_status", 10) ||
        !YOMKROS2_PUB_TOPIC(std_msgs::msg::Int32, "ctrl_counter", 10))
    {
        YOMK_ERROR_TAG("TestYomkROS2Control", "[FAIL] YOMKROS2_PUB_TOPIC 注册");
        return 1;
    }

    // 4. 后台 spin（客户端响应/回调依赖本地 spin）
    if (!YOMKROS2_RUN(false))
    {
        YOMK_ERROR_TAG("TestYomkROS2Control", "[FAIL] YOMKROS2_RUN 运行节点");
        return 1;
    }

    // 5. 后台线程周期发布两个 topic（每 200ms 一次，应对跨进程图发现延迟）
    std::atomic<bool> stopPub{false};
    std::thread pubThread([&stopPub]()
                          {
                              int32_t counter = 0;
                              while (!stopPub.load())
                              {
                                  std_msgs::msg::String status;
                                  status.data = "working";
                                  YOMKROS2_PUB_MSG("ctrl_status", status);
                                  std_msgs::msg::Int32 count;
                                  count.data = counter++;
                                  YOMKROS2_PUB_MSG("ctrl_counter", count);
                                  std::this_thread::sleep_for(std::chrono::milliseconds(200));
                              } });

    bool allOk = true;

    // 6. 远程设置两个参数（exec_node 未就绪时重试，设置成功后回读校验）
    bool paramOk = false;
    for (int i = 0; i < 20 && !paramOk; ++i)
    {
        if (YOMKROS2_SET_REMOTE_PARAM("exec_node", "exec_speed", int64_t(20)) &&
            YOMKROS2_SET_REMOTE_PARAM("exec_node", "exec_name", std::string("working")))
        {
            int64_t speed = 0;
            std::string name;
            if (YOMKROS2_GET_REMOTE_PARAM("exec_node", "exec_speed", speed) && speed == 20 &&
                YOMKROS2_GET_REMOTE_PARAM("exec_node", "exec_name", name) && name == "working")
            {
                paramOk = true;
            }
        }
        if (!paramOk)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
    if (!paramOk)
    {
        YOMK_ERROR_TAG("TestYomkROS2Control", "[FAIL] 远程参数设置/回读校验");
        allOk = false;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Control", "[OK] 远程参数: exec_speed=20 exec_name=working（回读校验通过）");
    }

    // 7. 请求两个服务（带重试等待 exec_node 就绪）
    AddTwoInts::Request addReq;
    addReq.a = 1;
    addReq.b = 2;
    SetBool::Request enableReq;
    enableReq.data = true;
    bool serviceOk = false;
    for (int i = 0; i < 20 && !serviceOk; ++i)
    {
        const auto addResp = YOMKROS2_CALL_SERVICE(AddTwoInts, "exec_add", addReq);
        const auto enableResp = YOMKROS2_CALL_SERVICE(SetBool, "exec_enable", enableReq);
        if (addResp && addResp->sum == 3 && enableResp && enableResp->success)
        {
            serviceOk = true;
        }
        if (!serviceOk)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
        }
    }
    if (!serviceOk)
    {
        YOMK_ERROR_TAG("TestYomkROS2Control", "[FAIL] 服务调用校验（exec_add sum=3 / exec_enable success）");
        allOk = false;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Control", "[OK] 服务调用: exec_add 1+2=3，exec_enable success=true");
    }

    // 8. 发送两个动作目标（order=5 与 order=8）：goalResponseCallback 确认接受，
    //    resultCallback 校验 SUCCEEDED 且序列长度 6/9
    const auto makeRespCb = [](const std::string &name)
    {
        return [name](uint64_t goalId, bool accepted)
        {
            std::lock_guard<std::mutex> lock(g_mtx);
            g_accepted[name] = accepted;
            YOMK_INFO_TAG("TestYomkROS2Control", "[ACTION] ", name, " goalId=", goalId, " accepted=", accepted);
        };
    };
    const auto makeFbCb = [](const std::string &)
    {
        return [](uint64_t, const std::shared_ptr<const Fibonacci::Feedback>) {};
    };
    const auto makeResultCb = [](const std::string &name)
    {
        return [name](uint64_t goalId, rclcpp_action::ResultCode code,
                      const std::shared_ptr<const Fibonacci::Result> result)
        {
            std::lock_guard<std::mutex> lock(g_mtx);
            g_resultCode[name] = static_cast<int>(code);
            if (result)
            {
                g_seqLen[name] = result->sequence.size();
            }
            YOMK_INFO_TAG("TestYomkROS2Control", "[ACTION] ", name, " goalId=", goalId,
                          " code=", static_cast<int>(code), " 序列长度=", result ? result->sequence.size() : 0);
        };
    };
    Fibonacci::Goal goal5;
    goal5.order = 5;
    Fibonacci::Goal goal8;
    goal8.order = 8;
    const uint64_t idFib = YOMKROS2_CALL_ACTION_ASYNC(Fibonacci, "exec_fib", goal5,
                                                      makeRespCb("exec_fib"), makeFbCb("exec_fib"), makeResultCb("exec_fib"));
    const uint64_t idFast = YOMKROS2_CALL_ACTION_ASYNC(Fibonacci, "exec_fib_fast", goal8,
                                                       makeRespCb("exec_fib_fast"), makeFbCb("exec_fib_fast"), makeResultCb("exec_fib_fast"));
    const bool actionOk = idFib != 0 && idFast != 0 &&
                          waitFor([]
                                  {
                                      std::lock_guard<std::mutex> lock(g_mtx);
                                      return g_accepted.count("exec_fib") && g_accepted["exec_fib"] &&
                                             g_accepted.count("exec_fib_fast") && g_accepted["exec_fib_fast"] &&
                                             g_resultCode.count("exec_fib") && g_resultCode.count("exec_fib_fast") &&
                                             g_resultCode["exec_fib"] == static_cast<int>(rclcpp_action::ResultCode::SUCCEEDED) &&
                                             g_resultCode["exec_fib_fast"] == static_cast<int>(rclcpp_action::ResultCode::SUCCEEDED) &&
                                             g_seqLen.count("exec_fib") && g_seqLen["exec_fib"] == 6 &&
                                             g_seqLen.count("exec_fib_fast") && g_seqLen["exec_fib_fast"] == 9; });
    if (!actionOk)
    {
        YOMK_ERROR_TAG("TestYomkROS2Control", "[FAIL] 动作目标校验（接受 + SUCCEEDED + 序列长度 6/9）");
        allOk = false;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Control", "[OK] 动作目标: exec_fib order=5 与 exec_fib_fast order=8 均 SUCCEEDED");
    }

    // 9. 停止发布线程并退出
    stopPub = true;
    pubThread.join();
    YOMKROS2_SHUTDOWN();

    if (allOk)
    {
        YOMK_INFO_TAG("TestYomkROS2Control", "[DONE] 参数/服务/动作/主题全部校验通过，干净退出");
        return 0;
    }
    return 1;
}
