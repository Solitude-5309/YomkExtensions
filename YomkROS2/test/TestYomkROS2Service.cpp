#include <YomkServer/YomkAPI.h>
#include <YomkROS2/YomkROS2API.h>

#include <example_interfaces/srv/add_two_ints.hpp>
#include <std_srvs/srv/set_bool.hpp>

#include <atomic>
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using namespace yomk;
using AddTwoInts = example_interfaces::srv::AddTwoInts;
using SetBool = std_srvs::srv::SetBool;

static int g_pass = 0;
static int g_fail = 0;

// 异步调用统计（异步回调由后台 spin 线程调用）
static std::atomic<int> g_asyncCalls{0};
static std::atomic<int64_t> g_asyncSum{0};

// 服务通信宏 API 测试：被测接口全部通过 YOMKROS2_* 宏调用全局单例完成服务端注册、
// 客户端预创建与同步/异步调用。远程服务端（remote_node）由独立 ROS2Node 实例充当
// （宏为全局单例，双节点场景需另一实例承载服务端，仅作测试环境，非被测接口）。
// 调用顺序遵循推荐用法：创建节点 → 注册服务端 + 预创建客户端（run 前） → run →
// 发送请求。客户端在 spin 启动前就绪，跨节点响应才能被正确接收（spin 期间动态
// 创建的客户端可能收不到响应）。
// 非阻塞模式：单例与远程节点均 run(false) 后台 spin（客户端 future 的响应依赖本地
// spin，服务端请求由各自的 spin 处理），全部用例执行完毕后自行退出。
int main(int argc, char *argv[])
{
    YOMK_INIT();

    ROS2Node remote; // 远程服务端（测试环境，非被测接口）

    // 测试 1：宏初始化单例节点 + 远程服务端节点初始化（环境）
    if (!YOMKROS2_NODE(argc, argv, "service_node") || !remote.init(argc, argv, "remote_node"))
    {
        YOMK_ERROR_TAG("TestYomkROS2Service", "[FAIL] YOMKROS2_NODE 初始化");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Service", "[PASS] YOMKROS2_NODE: 单例节点=service_node 远程服务端=remote_node（环境）");
        g_pass++;
    }

    // 测试 2：宏注册 add_service（AddTwoInts：a+b=sum）+ run 前预创建客户端；
    // 服务端回调输出 [SERVICE] 关键数据（每次收到请求打印请求内容与响应结果）
    auto addCb = [](const std::shared_ptr<AddTwoInts::Request> request, std::shared_ptr<AddTwoInts::Response> response)
    {
        response->sum = request->a + request->b;
        YOMK_INFO_TAG("TestYomkROS2Service", "[SERVICE] add_service: 收到请求 a=", request->a, " b=", request->b,
                      " -> 响应 sum=", response->sum);
    };
    if (!YOMKROS2_SERVICE(AddTwoInts, "add_service", addCb) || !YOMKROS2_SERVICE_CLIENT(AddTwoInts, "add_service"))
    {
        YOMK_ERROR_TAG("TestYomkROS2Service", "[FAIL] YOMKROS2_SERVICE/CLIENT add_service");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Service", "[PASS] YOMKROS2_SERVICE: 注册 add_service（AddTwoInts a+b）+ 预创建客户端（run 前）");
        g_pass++;
    }

    // 测试 3：宏注册 set_bool_service（SetBool：success=true + message）+ run 前预创建客户端
    auto boolCb = [](const std::shared_ptr<SetBool::Request> request, std::shared_ptr<SetBool::Response> response)
    {
        response->success = true;
        response->message = "set_bool_service: 收到 data=" + std::string(request->data ? "true" : "false");
        YOMK_INFO_TAG("TestYomkROS2Service", "[SERVICE] set_bool_service: 收到请求 data=", (request->data ? "true" : "false"),
                      " -> 响应 success=true");
    };
    if (!YOMKROS2_SERVICE(SetBool, "set_bool_service", boolCb) || !YOMKROS2_SERVICE_CLIENT(SetBool, "set_bool_service"))
    {
        YOMK_ERROR_TAG("TestYomkROS2Service", "[FAIL] YOMKROS2_SERVICE/CLIENT set_bool_service");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Service", "[PASS] YOMKROS2_SERVICE: 注册 set_bool_service（SetBool）+ 预创建客户端（run 前）");
        g_pass++;
    }

    // 测试 4：服务重名注册返回 false（预期输出一条 RCLCPP_ERROR 日志）
    if (YOMKROS2_SERVICE(AddTwoInts, "add_service", addCb))
    {
        YOMK_ERROR_TAG("TestYomkROS2Service", "[FAIL] 服务重名应返回 false");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Service", "[PASS] 服务重名: add_service 重复注册返回 false");
        g_pass++;
    }

    // 测试 5：远程环境注册 remote_service（AddTwoInts：a*b，跨节点验证）+ 宏 run 前预创建客户端
    auto mulCb = [](const std::shared_ptr<AddTwoInts::Request> request, std::shared_ptr<AddTwoInts::Response> response)
    {
        response->sum = request->a * request->b;
        YOMK_INFO_TAG("TestYomkROS2Service", "[SERVICE] remote_service: 收到请求 a=", request->a, " b=", request->b,
                      " -> 响应 sum=", response->sum);
    };
    if (!remote.createService<AddTwoInts>("remote_service", mulCb) || !YOMKROS2_SERVICE_CLIENT(AddTwoInts, "remote_service"))
    {
        YOMK_ERROR_TAG("TestYomkROS2Service", "[FAIL] 远程服务端注册 remote_service/预创建客户端");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Service", "[PASS] 远程服务端: 注册 remote_service（AddTwoInts a*b，环境）+ 预创建客户端（run 前）");
        g_pass++;
    }

    // 测试 6：宏运行单例节点 + 远程服务端运行（均后台 spin；
    // 客户端 future 的响应依赖本地 spin，服务端请求由各自 spin 处理）
    if (!YOMKROS2_RUN(false) || !remote.run(false))
    {
        YOMK_ERROR_TAG("TestYomkROS2Service", "[FAIL] YOMKROS2_RUN 运行节点");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Service", "[PASS] YOMKROS2_RUN(false): 单例与远程节点后台 spin 启动");
        g_pass++;
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(300)); // 等待图发现完成

    // 测试 7：宏同步调用本节点 add_service：请求 a=1,b=2 -> 响应 sum=3
    AddTwoInts::Request reqAdd;
    reqAdd.a = 1;
    reqAdd.b = 2;
    const auto respAdd = YOMKROS2_CALL_SERVICE(AddTwoInts, "add_service", reqAdd);
    if (!respAdd || respAdd->sum != 3)
    {
        YOMK_ERROR_TAG("TestYomkROS2Service", "[FAIL] YOMKROS2_CALL_SERVICE add_service");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Service", "[PASS] YOMKROS2_CALL_SERVICE: add_service 请求 a=1 b=2 -> 响应 sum=", respAdd->sum);
        g_pass++;
    }

    // 测试 8：宏同步调用本节点 set_bool_service：请求 data=true -> success=true + message
    SetBool::Request reqBool;
    reqBool.data = true;
    const auto respBool = YOMKROS2_CALL_SERVICE(SetBool, "set_bool_service", reqBool);
    if (!respBool || !respBool->success)
    {
        YOMK_ERROR_TAG("TestYomkROS2Service", "[FAIL] YOMKROS2_CALL_SERVICE set_bool_service");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Service", "[PASS] YOMKROS2_CALL_SERVICE: set_bool_service 请求 data=true -> success=true message=\"",
                      respBool->message, "\"");
        g_pass++;
    }

    // 测试 9：宏同步调用远程 remote_service：请求 a=3,b=4 -> 响应 sum=12（跨节点，复用 run 前预创建的客户端）
    AddTwoInts::Request reqRemote;
    reqRemote.a = 3;
    reqRemote.b = 4;
    const auto respRemote = YOMKROS2_CALL_SERVICE(AddTwoInts, "remote_service", reqRemote);
    if (!respRemote || respRemote->sum != 12)
    {
        YOMK_ERROR_TAG("TestYomkROS2Service", "[FAIL] YOMKROS2_CALL_SERVICE remote_service 跨节点");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Service", "[PASS] YOMKROS2_CALL_SERVICE: remote_service 请求 a=3 b=4 -> 响应 sum=", respRemote->sum);
        g_pass++;
    }

    // 测试 10：宏同步调用不存在的服务返回 nullptr（预期输出一条 RCLCPP_ERROR 日志）
    AddTwoInts::Request reqNone;
    reqNone.a = 0;
    reqNone.b = 0;
    const auto respNone = YOMKROS2_CALL_SERVICE(AddTwoInts, "not_exist_service", reqNone);
    if (respNone)
    {
        YOMK_ERROR_TAG("TestYomkROS2Service", "[FAIL] 不存在的服务应返回 nullptr");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Service", "[PASS] 不存在的服务: not_exist_service 返回 nullptr");
        g_pass++;
    }

    // 测试 11：宏异步调用本节点 add_service：请求 a=5,b=6，立即返回，响应就绪时回调收到响应
    auto asyncCb = [](AddTwoInts::Response::SharedPtr resp)
    {
        YOMK_INFO_TAG("TestYomkROS2Service", "[ASYNC] 异步回调收到响应: sum=", resp->sum);
        g_asyncCalls++;
        g_asyncSum = resp->sum;
    };
    AddTwoInts::Request reqAsync;
    reqAsync.a = 5;
    reqAsync.b = 6;
    if (!YOMKROS2_CALL_SERVICE_ASYNC(AddTwoInts, "add_service", reqAsync, asyncCb))
    {
        YOMK_ERROR_TAG("TestYomkROS2Service", "[FAIL] YOMKROS2_CALL_SERVICE_ASYNC add_service");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Service", "[PASS] YOMKROS2_CALL_SERVICE_ASYNC: add_service 请求 a=5 b=6（异步发送，立即返回）");
        g_pass++;
    }

    // 测试 12：主线程等待异步回调完成，校验回调计次与响应数据（sum=11）
    for (int i = 0; i < 30 && g_asyncCalls.load() == 0; ++i)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // 最多等待 3s
    }
    if (g_asyncCalls.load() != 1 || g_asyncSum.load() != 11)
    {
        YOMK_ERROR_TAG("TestYomkROS2Service", "[FAIL] 异步响应校验: 回调次数=", g_asyncCalls.load(), " sum=", g_asyncSum.load());
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Service", "[PASS] 异步响应校验: 回调次数=1 sum=", g_asyncSum.load());
        g_pass++;
    }

    // 测试 13：shutdown 干净退出（先关远程服务端，再关单例——单例持有进程级 rclcpp::init）
    if (!remote.shutdown() || !YOMKROS2_SHUTDOWN())
    {
        YOMK_ERROR_TAG("TestYomkROS2Service", "[FAIL] YOMKROS2_SHUTDOWN 干净退出");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Service", "[PASS] YOMKROS2_SHUTDOWN: 远程服务端与单例节点均已销毁");
        g_pass++;
    }

    std::cout << "\n========== Test Summary (Service) ==========" << std::endl;
    std::cout << "PASS: " << g_pass << std::endl;
    std::cout << "FAIL: " << g_fail << std::endl;

    return g_fail > 0 ? 1 : 0;
}
