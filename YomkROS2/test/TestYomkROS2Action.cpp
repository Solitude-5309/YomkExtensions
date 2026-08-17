#include <YomkServer/YomkAPI.h>
#include <YomkROS2/YomkROS2API.h>

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
using Fibonacci = example_interfaces::action::Fibonacci;
using ServerGoalHandle = rclcpp_action::ServerGoalHandle<Fibonacci>;
using ResultCode = rclcpp_action::ResultCode; // ResultCode 为 rclcpp_action 命名空间级枚举

static int g_pass = 0;
static int g_fail = 0;

// 结果记录（异步回调由后台 spin 线程调用，统一加锁）；
// 客户端回调对外屏蔽 ClientGoalHandle，全部以 callActionAsync 返回的 goalId 为键
static std::mutex g_mtx;
static std::map<uint64_t, bool> g_accepted;            // goalId → 接受/拒绝（goalResponseCallback）
static std::map<uint64_t, int> g_resultCode;           // goalId → 结果码（SUCCEEDED/ABORTED/CANCELED/UNKNOWN）
static std::map<uint64_t, std::vector<int32_t>> g_seq; // goalId → result.sequence
static std::map<uint64_t, int> g_fbCount;              // goalId → feedback 次数
static std::map<uint64_t, size_t> g_lastFbLen;         // goalId → 最后一次 feedback 的序列长度
static std::atomic<int> g_cancelCount{0};              // CANCELED 结果计次（测试 13/14/15 前清零）
static std::atomic<int> g_activeExec{0};               // 服务端执行线程活跃数（shutdown 前等待清零）
static std::mutex g_deferredMtx;
static std::shared_ptr<ServerGoalHandle> g_deferredHandle; // ACCEPT_AND_DEFER 目标句柄（测试 9）

// 服务端执行逻辑：逐步计算 Fibonacci，每步 sleep 100ms + 发布 feedback + 检查取消；
// 用户执行线程由测试代码自开自管（封装不管线程生命周期）
static void runFib(std::shared_ptr<ServerGoalHandle> goalHandle)
{
    g_activeExec++;
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
                            YOMK_INFO_TAG("TestYomkROS2Action", "[ACTION] 执行线程: order=", order, " 已取消");
                            g_activeExec--;
                            return;
                        }
                        sequence.push_back(sequence[i] + sequence[i - 1]);
                        goalHandle->publish_feedback(feedback);
                        std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    }
                    auto result = std::make_shared<Fibonacci::Result>();
                    result->sequence = sequence;
                    goalHandle->succeed(result);
                    YOMK_INFO_TAG("TestYomkROS2Action", "[ACTION] 执行线程: order=", order, " 完成，序列长度=", sequence.size());
                    g_activeExec--; })
        .detach();
}

// 轮询等待谓词成立（默认最多 8s）；返回是否成立
template <typename Pred>
static bool waitFor(Pred pred, int maxMs = 8000)
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

// 动作通信宏 API 测试：被测接口全部通过 YOMKROS2_* 宏调用全局单例完成服务端注册、
// 客户端预创建、异步发送与取消（指定/全部/按时间戳）。远程服务端（remote_node）由
// 独立 ROS2Node 实例充当（宏为全局单例，双节点场景需另一实例承载服务端，仅作测试
// 环境，非被测接口）。
// 调用顺序遵循推荐用法：创建节点 → 注册服务端 + 预创建客户端（run 前） → run →
// 异步发送目标。客户端在 spin 启动前就绪，跨节点响应才能被正确接收（spin 期间动态
// 创建的客户端可能收不到响应）。
// 非阻塞模式：单例与远程节点均 run(false) 后台 spin（feedback/result 回调依赖本地
// spin，服务端 goal/cancel/accepted 回调由各自的 spin 处理），全部用例执行完毕
// 后自行退出。服务端三回调语义：goalCallback 返回 int（1 拒绝 / 2 接受并立即执行 /
// 3 接受但延迟执行）；cancelCallback 返回 bool（order==10 拒绝取消，危险任务示例）；
// executeCallback 内自行开线程执行。
// 客户端回调对外屏蔽 ClientGoalHandle，以 goalId 为统一身份：
//   goalResponseCallback(goalId, accepted) 必触发一次告知接受/拒绝；
//   feedbackCallback(goalId, feedback)；resultCallback(goalId, code, result)。
int main(int argc, char *argv[])
{
    YOMK_INIT();

    ROS2Node remote; // 远程服务端（测试环境，非被测接口）

    // 客户端通用回调（对外屏蔽 ClientGoalHandle，以 goalId 统一身份）：
    // resp 记录接受/拒绝，feedback 记录次数与序列长度，result 记录结果码与序列；
    // 回调先定义为变量再传入宏（避免顶层逗号问题）
    auto respCb = [](uint64_t goalId, bool accepted)
    {
        std::lock_guard<std::mutex> lock(g_mtx);
        g_accepted[goalId] = accepted;
        YOMK_INFO_TAG("TestYomkROS2Action", "[RESPONSE] goalId=", goalId, " accepted=", accepted);
    };
    auto feedbackCb = [](uint64_t goalId,
                         const std::shared_ptr<const Fibonacci::Feedback> feedback)
    {
        std::lock_guard<std::mutex> lock(g_mtx);
        g_fbCount[goalId]++;
        g_lastFbLen[goalId] = feedback->sequence.size();
    };
    auto resultCb = [](uint64_t goalId, rclcpp_action::ResultCode code,
                       const std::shared_ptr<const Fibonacci::Result> result)
    {
        std::lock_guard<std::mutex> lock(g_mtx);
        g_resultCode[goalId] = static_cast<int>(code);
        if (result)
        {
            g_seq[goalId] = result->sequence;
        }
        if (code == ResultCode::CANCELED)
        {
            g_cancelCount++;
        }
        YOMK_INFO_TAG("TestYomkROS2Action", "[RESULT] goalId=", goalId, " code=", static_cast<int>(code),
                      " 序列长度=", result ? result->sequence.size() : 0);
    };

    // 测试 1：宏初始化单例节点 + 远程服务端节点初始化（环境）
    if (!YOMKROS2_NODE(argc, argv, "action_node") || !remote.init(argc, argv, "remote_node"))
    {
        YOMK_ERROR_TAG("TestYomkROS2Action", "[FAIL] YOMKROS2_NODE 初始化");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Action", "[PASS] YOMKROS2_NODE: 单例节点=action_node 远程服务端=remote_node（环境）");
        g_pass++;
    }

    // 测试 2：宏注册 fib_action（本节点）+ run 前预创建客户端；
    // 服务端三回调：goalCallback 返回 int（order<0 拒绝 / order==7 延迟执行 / 其他立即执行），
    // cancelCallback 返回 bool（order==10 拒绝取消），executeCallback 自行开线程执行
    auto goalCb = [](const std::shared_ptr<const Fibonacci::Goal> goal) -> int
    {
        if (goal->order < 0)
        {
            return 1; // REJECT
        }
        if (goal->order == 7)
        {
            return 3; // ACCEPT_AND_DEFER
        }
        return 2; // ACCEPT_AND_EXECUTE
    };
    auto cancelCb = [](std::shared_ptr<ServerGoalHandle> goalHandle) -> bool
    {
        return goalHandle->get_goal()->order != 10; // order==10 拒绝取消（危险任务示例）
    };
    auto execCb = [](std::shared_ptr<ServerGoalHandle> goalHandle)
    {
        if (goalHandle->get_goal()->order == 7)
        {
            std::lock_guard<std::mutex> lock(g_deferredMtx);
            g_deferredHandle = goalHandle; // ACCEPT_AND_DEFER：记录句柄供测试代码手动 execute()
        }
        YOMK_INFO_TAG("TestYomkROS2Action", "[ACTION] 执行回调触发（executor 线程）: order=", goalHandle->get_goal()->order);
        runFib(goalHandle); // 用户自行开线程执行长耗时任务
    };
    if (!YOMKROS2_ACTION(Fibonacci, "fib_action", goalCb, cancelCb, execCb) ||
        !YOMKROS2_ACTION_CLIENT(Fibonacci, "fib_action"))
    {
        YOMK_ERROR_TAG("TestYomkROS2Action", "[FAIL] YOMKROS2_ACTION/ACTION_CLIENT fib_action");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Action", "[PASS] YOMKROS2_ACTION: 注册 fib_action（三回调）+ 预创建客户端（run 前）");
        g_pass++;
    }

    // 测试 3：远程环境注册 remote_fib（跨节点验证，全部接受/允许取消）+ 宏 run 前预创建客户端
    auto remoteGoalCb = [](const std::shared_ptr<const Fibonacci::Goal>) -> int
    { return 2; };
    auto remoteCancelCb = [](std::shared_ptr<ServerGoalHandle>) -> bool
    { return true; };
    auto remoteExecCb = [](std::shared_ptr<ServerGoalHandle> goalHandle)
    { runFib(goalHandle); };
    if (!remote.createAction<Fibonacci>("remote_fib", remoteGoalCb, remoteCancelCb, remoteExecCb) ||
        !YOMKROS2_ACTION_CLIENT(Fibonacci, "remote_fib"))
    {
        YOMK_ERROR_TAG("TestYomkROS2Action", "[FAIL] 远程服务端注册 remote_fib/预创建客户端");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Action", "[PASS] 远程服务端: 注册 remote_fib（环境）+ 预创建客户端（run 前）");
        g_pass++;
    }

    // 测试 4：动作重名注册返回 false（预期输出一条 RCLCPP_ERROR 日志）
    if (YOMKROS2_ACTION(Fibonacci, "fib_action", goalCb, cancelCb, execCb))
    {
        YOMK_ERROR_TAG("TestYomkROS2Action", "[FAIL] 动作重名应返回 false");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Action", "[PASS] 动作重名: fib_action 重复注册返回 false");
        g_pass++;
    }

    // 测试 5：宏运行单例节点 + 远程服务端运行（均后台 spin；
    // 客户端回调依赖本地 spin，服务端回调由各自 spin 处理）
    if (!YOMKROS2_RUN(false) || !remote.run(false))
    {
        YOMK_ERROR_TAG("TestYomkROS2Action", "[FAIL] YOMKROS2_RUN 运行节点");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Action", "[PASS] YOMKROS2_RUN(false): 单例与远程节点后台 spin 启动");
        g_pass++;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300)); // 等待图发现完成

    // 测试 6：宏异步调用本节点 fib_action order=5 → goalResponseCallback 收到接受 +
    // feedback 各步序列 + SUCCEEDED（断言直接用返回的 goalId，无需句柄）
    Fibonacci::Goal goal5;
    goal5.order = 5;
    const uint64_t id5 = YOMKROS2_CALL_ACTION_ASYNC(Fibonacci, "fib_action", goal5, respCb, feedbackCb, resultCb);
    const bool test6 = id5 != 0 &&
                       waitFor([&]
                               {
                                   std::lock_guard<std::mutex> lock(g_mtx);
                                   return g_accepted.count(id5) && g_accepted[id5] &&
                                          g_resultCode.count(id5) &&
                                          g_resultCode[id5] == static_cast<int>(ResultCode::SUCCEEDED) &&
                                          g_seq.count(id5) && g_seq[id5].size() == 6 && g_fbCount[id5] >= 4; });
    if (!test6)
    {
        YOMK_ERROR_TAG("TestYomkROS2Action", "[FAIL] YOMKROS2_CALL_ACTION_ASYNC fib_action order=5");
        g_fail++;
    }
    else
    {
        std::lock_guard<std::mutex> lock(g_mtx);
        YOMK_INFO_TAG("TestYomkROS2Action", "[PASS] YOMKROS2_CALL_ACTION_ASYNC: order=5 接受确认 SUCCEEDED 序列长度=",
                      g_seq[id5].size(), " feedback 次数=", g_fbCount[id5], " 末次反馈长度=", g_lastFbLen[id5]);
        g_pass++;
    }

    // 测试 7：不存在动作 callActionAsync → 0（预期输出一条 RCLCPP_ERROR 日志）
    const uint64_t idNone = YOMKROS2_CALL_ACTION_ASYNC(Fibonacci, "not_exist_action", goal5, respCb, feedbackCb, resultCb);
    if (idNone != 0)
    {
        YOMK_ERROR_TAG("TestYomkROS2Action", "[FAIL] 不存在的动作应返回 0");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Action", "[PASS] 不存在的动作: not_exist_action 返回 0");
        g_pass++;
    }

    // 测试 8：goalCallback 拒绝（order=-1）→ goalResponseCallback 收到 accepted=false +
    // 结果回调收到 UNKNOWN（封装合成的终结通知，验证回调不变量）
    Fibonacci::Goal goalNeg;
    goalNeg.order = -1;
    const uint64_t idNeg = YOMKROS2_CALL_ACTION_ASYNC(Fibonacci, "fib_action", goalNeg, respCb, feedbackCb, resultCb);
    const bool test8 = idNeg != 0 &&
                       waitFor([&]
                               {
                                   std::lock_guard<std::mutex> lock(g_mtx);
                                   return g_accepted.count(idNeg) && !g_accepted[idNeg] &&
                                          g_resultCode.count(idNeg) &&
                                          g_resultCode[idNeg] == static_cast<int>(ResultCode::UNKNOWN) &&
                                          !g_seq.count(idNeg); });
    if (!test8)
    {
        YOMK_ERROR_TAG("TestYomkROS2Action", "[FAIL] goal 拒绝应收到 accepted=false 与 UNKNOWN 终结通知");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Action", "[PASS] goal 拒绝: order=-1 响应回调 accepted=false + 结果回调 UNKNOWN（回调链必终结）");
        g_pass++;
    }

    // 测试 9：ACCEPT_AND_DEFER（order=7）：executeCallback 不自动触发，
    // 测试代码延时后手动 goalHandle->execute() 再开线程执行 → SUCCEEDED
    Fibonacci::Goal goal7;
    goal7.order = 7;
    const uint64_t id7 = YOMKROS2_CALL_ACTION_ASYNC(Fibonacci, "fib_action", goal7, respCb, feedbackCb, resultCb);
    const bool gotHandle = id7 != 0 && waitFor([]
                                               {
                                                   std::lock_guard<std::mutex> lock(g_deferredMtx);
                                                   return g_deferredHandle != nullptr; });
    if (!gotHandle)
    {
        YOMK_ERROR_TAG("TestYomkROS2Action", "[FAIL] ACCEPT_AND_DEFER 未记录到延迟目标句柄");
        g_fail++;
    }
    else
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(200)); // 模拟稍后手动触发
        std::shared_ptr<ServerGoalHandle> deferred;
        {
            std::lock_guard<std::mutex> lock(g_deferredMtx);
            deferred = g_deferredHandle;
        }
        deferred->execute(); // 原生延迟执行触发：此后原生调用 accepted 回调（即 execCb）
    }
    const bool test9 = gotHandle &&
                       waitFor([&]
                               {
                                   std::lock_guard<std::mutex> lock(g_mtx);
                                   return g_resultCode.count(id7) &&
                                          g_resultCode[id7] == static_cast<int>(ResultCode::SUCCEEDED) &&
                                          g_seq.count(id7) && g_seq[id7].size() == 8; });
    if (!test9)
    {
        YOMK_ERROR_TAG("TestYomkROS2Action", "[FAIL] ACCEPT_AND_DEFER 手动 execute 后应 SUCCEEDED");
        g_fail++;
    }
    else
    {
        std::lock_guard<std::mutex> lock(g_mtx);
        YOMK_INFO_TAG("TestYomkROS2Action", "[PASS] ACCEPT_AND_DEFER: order=7 手动 execute() 后 SUCCEEDED 序列长度=", g_seq[id7].size());
        g_pass++;
    }

    // 测试 10：拒绝取消能力（order=10）：执行中 CANCEL_GOAL → cancelCallback 拒绝 → 最终 SUCCEEDED
    Fibonacci::Goal goal10;
    goal10.order = 10;
    const uint64_t id10 = YOMKROS2_CALL_ACTION_ASYNC(Fibonacci, "fib_action", goal10, respCb, feedbackCb, resultCb);
    const bool running10 = id10 != 0 &&
                           waitFor([&]
                                   {
                                       std::lock_guard<std::mutex> lock(g_mtx);
                                       return g_fbCount[id10] >= 2 && !g_resultCode.count(id10); });
    const bool test10 = running10 && YOMKROS2_CANCEL_GOAL(Fibonacci, "fib_action", id10) &&
                        waitFor([&]
                                {
                                    std::lock_guard<std::mutex> lock(g_mtx);
                                    return g_resultCode.count(id10) &&
                                           g_resultCode[id10] == static_cast<int>(ResultCode::SUCCEEDED); });
    if (!test10)
    {
        YOMK_ERROR_TAG("TestYomkROS2Action", "[FAIL] 拒绝取消: order=10 取消被拒后应 SUCCEEDED");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Action", "[PASS] 拒绝取消: order=10 CANCEL_GOAL 被服务端拒绝，goal 继续执行至 SUCCEEDED");
        g_pass++;
    }

    // 测试 11：跨节点异步调用 remote_fib order=3 → SUCCEEDED 且序列长度=4
    Fibonacci::Goal goal3;
    goal3.order = 3;
    const uint64_t id3 = YOMKROS2_CALL_ACTION_ASYNC(Fibonacci, "remote_fib", goal3, respCb, feedbackCb, resultCb);
    const bool test11 = id3 != 0 &&
                        waitFor([&]
                                {
                                    std::lock_guard<std::mutex> lock(g_mtx);
                                    return g_accepted.count(id3) && g_accepted[id3] &&
                                           g_resultCode.count(id3) &&
                                           g_resultCode[id3] == static_cast<int>(ResultCode::SUCCEEDED) &&
                                           g_seq.count(id3) && g_seq[id3].size() == 4; });
    if (!test11)
    {
        YOMK_ERROR_TAG("TestYomkROS2Action", "[FAIL] 跨节点 remote_fib order=3");
        g_fail++;
    }
    else
    {
        std::lock_guard<std::mutex> lock(g_mtx);
        YOMK_INFO_TAG("TestYomkROS2Action", "[PASS] 跨节点: remote_fib order=3 接受确认 SUCCEEDED 序列长度=", g_seq[id3].size());
        g_pass++;
    }

    // 测试 12：多 goal 并发：order=4 与 order=6 同时发送 → 各自 SUCCEEDED 且序列长度正确
    Fibonacci::Goal goal4, goal6;
    goal4.order = 4;
    goal6.order = 6;
    const uint64_t id4 = YOMKROS2_CALL_ACTION_ASYNC(Fibonacci, "fib_action", goal4, respCb, feedbackCb, resultCb);
    const uint64_t id6 = YOMKROS2_CALL_ACTION_ASYNC(Fibonacci, "fib_action", goal6, respCb, feedbackCb, resultCb);
    const bool test12 = id4 != 0 && id6 != 0 &&
                        waitFor([&]
                                {
                                    std::lock_guard<std::mutex> lock(g_mtx);
                                    return g_resultCode.count(id4) && g_resultCode.count(id6) &&
                                           g_resultCode[id4] == static_cast<int>(ResultCode::SUCCEEDED) &&
                                           g_resultCode[id6] == static_cast<int>(ResultCode::SUCCEEDED) &&
                                           g_seq.count(id4) && g_seq[id4].size() == 5 &&
                                           g_seq.count(id6) && g_seq[id6].size() == 7; });
    if (!test12)
    {
        YOMK_ERROR_TAG("TestYomkROS2Action", "[FAIL] 多 goal 并发 order=4/order=6");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Action", "[PASS] 多 goal 并发: order=4 与 order=6 各自 SUCCEEDED（goalId/回调链隔离）");
        g_pass++;
    }

    // 测试 13：取消指定 goal：order=20（长耗时）执行中 CANCEL_GOAL → CANCELED
    g_cancelCount = 0;
    Fibonacci::Goal goal20;
    goal20.order = 20;
    const uint64_t id20 = YOMKROS2_CALL_ACTION_ASYNC(Fibonacci, "fib_action", goal20, respCb, feedbackCb, resultCb);
    const bool running20 = id20 != 0 &&
                           waitFor([&]
                                   {
                                       std::lock_guard<std::mutex> lock(g_mtx);
                                       return g_fbCount[id20] >= 3 && !g_resultCode.count(id20); });
    const bool test13 = running20 && YOMKROS2_CANCEL_GOAL(Fibonacci, "fib_action", id20) &&
                        waitFor([&]
                                {
                                    std::lock_guard<std::mutex> lock(g_mtx);
                                    return g_resultCode.count(id20) &&
                                           g_resultCode[id20] == static_cast<int>(ResultCode::CANCELED); });
    if (!test13)
    {
        YOMK_ERROR_TAG("TestYomkROS2Action", "[FAIL] CANCEL_GOAL order=20 应收到 CANCELED");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Action", "[PASS] CANCEL_GOAL: order=20 执行中取消，结果回调收到 CANCELED");
        g_pass++;
    }

    // 测试 14：取消全部 goal：并发 order=21/order=22 → CANCEL_ALL_GOALS → 两者均 CANCELED
    g_cancelCount = 0;
    Fibonacci::Goal goal21, goal22;
    goal21.order = 21;
    goal22.order = 22;
    const uint64_t id21 = YOMKROS2_CALL_ACTION_ASYNC(Fibonacci, "fib_action", goal21, respCb, feedbackCb, resultCb);
    const uint64_t id22 = YOMKROS2_CALL_ACTION_ASYNC(Fibonacci, "fib_action", goal22, respCb, feedbackCb, resultCb);
    const bool started14 = id21 != 0 && id22 != 0 &&
                           waitFor([&]
                                   {
                                       std::lock_guard<std::mutex> lock(g_mtx);
                                       return g_fbCount[id21] >= 1 && g_fbCount[id22] >= 1 &&
                                              !g_resultCode.count(id21) && !g_resultCode.count(id22); });
    const bool test14 = started14 && YOMKROS2_CANCEL_ALL_GOALS(Fibonacci, "fib_action") &&
                        waitFor([&]
                                {
                                    std::lock_guard<std::mutex> lock(g_mtx);
                                    return g_resultCode.count(id21) && g_resultCode.count(id22) &&
                                           g_resultCode[id21] == static_cast<int>(ResultCode::CANCELED) &&
                                           g_resultCode[id22] == static_cast<int>(ResultCode::CANCELED); });
    if (!test14)
    {
        YOMK_ERROR_TAG("TestYomkROS2Action", "[FAIL] CANCEL_ALL_GOALS order=21/22 应均收到 CANCELED");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Action", "[PASS] CANCEL_ALL_GOALS: order=21/22 全部收到 CANCELED");
        g_pass++;
    }

    // 测试 15：按时间戳取消：并发 order=23/order=24 → CANCEL_GOALS_BEFORE(now) → 两者均 CANCELED
    g_cancelCount = 0;
    Fibonacci::Goal goal23, goal24;
    goal23.order = 23;
    goal24.order = 24;
    const uint64_t id23 = YOMKROS2_CALL_ACTION_ASYNC(Fibonacci, "fib_action", goal23, respCb, feedbackCb, resultCb);
    const uint64_t id24 = YOMKROS2_CALL_ACTION_ASYNC(Fibonacci, "fib_action", goal24, respCb, feedbackCb, resultCb);
    const bool started15 = id23 != 0 && id24 != 0 &&
                           waitFor([&]
                                   {
                                       std::lock_guard<std::mutex> lock(g_mtx);
                                       return g_fbCount[id23] >= 1 && g_fbCount[id24] >= 1 &&
                                              !g_resultCode.count(id23) && !g_resultCode.count(id24); });
    const rclcpp::Time now = rclcpp::Clock().now(); // 系统时钟当前时间（两 goal 时间戳均早于 now）
    const bool test15 = started15 && YOMKROS2_CANCEL_GOALS_BEFORE(Fibonacci, "fib_action", now) &&
                        waitFor([&]
                                {
                                    std::lock_guard<std::mutex> lock(g_mtx);
                                    return g_resultCode.count(id23) && g_resultCode.count(id24) &&
                                           g_resultCode[id23] == static_cast<int>(ResultCode::CANCELED) &&
                                           g_resultCode[id24] == static_cast<int>(ResultCode::CANCELED); });
    if (!test15)
    {
        YOMK_ERROR_TAG("TestYomkROS2Action", "[FAIL] CANCEL_GOALS_BEFORE order=23/24 应均收到 CANCELED");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Action", "[PASS] CANCEL_GOALS_BEFORE: order=23/24 全部收到 CANCELED");
        g_pass++;
    }

    // 测试 16：等待全部用户执行线程退出后 shutdown 干净退出（先 remote 后单例）
    waitFor([]
            { return g_activeExec.load() == 0; });
    if (!remote.shutdown() || !YOMKROS2_SHUTDOWN())
    {
        YOMK_ERROR_TAG("TestYomkROS2Action", "[FAIL] YOMKROS2_SHUTDOWN 干净退出");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Action", "[PASS] YOMKROS2_SHUTDOWN: 执行线程已退出，远程服务端与单例节点均已销毁");
        g_pass++;
    }

    std::cout << "\n========== Test Summary (Action) ==========" << std::endl;
    std::cout << "PASS: " << g_pass << std::endl;
    std::cout << "FAIL: " << g_fail << std::endl;

    return g_fail > 0 ? 1 : 0;
}
