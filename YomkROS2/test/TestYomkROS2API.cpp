#include <YomkServer/YomkAPI.h>
#include <YomkROS2/YomkROSAPI.h>

#include <std_msgs/msg/string.hpp>

#include <iostream>
#include <memory>

using namespace yomk;

static int g_pass = 0;
static int g_fail = 0;

// 本测试遵循原生 ROS2 节点用法：初始化与主题注册后 run(true) 永久阻塞主线程，
// 由用户 Ctrl+C（SIGINT/SIGTERM）或异常退出；run 返回后的清理（条件 shutdown）由用户控制。
// 阻塞期间消息发布由用户在回调或其他线程中进行，此处不做发布校验。
int main(int argc, char *argv[])
{
    YOMK_INIT();

    // 测试 1：宏初始化节点（透传 argc/argv，与原生 rclcpp::init 语义一致）
    if (!YOMKROS2_NODE(argc, argv, "api_node"))
    {
        YOMK_ERROR_TAG("TestYomkROS2API", "[FAIL] YOMKROS2_NODE");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2API", "[PASS] YOMKROS2_NODE");
        g_pass++;
    }

    // 测试 2：宏注册订阅主题（回调定义为变量后传入，规避回调体内顶层逗号的宏展开问题）
    auto onMessage = [](std::shared_ptr<const std_msgs::msg::String> msg)
    {
        YOMK_INFO_TAG("TestYomkROS2API", "[RECV] ", msg->data);
    };
    if (!YOMKROS2_SUB_TOPIC(std_msgs::msg::String, "api_topic", 10, onMessage))
    {
        YOMK_ERROR_TAG("TestYomkROS2API", "[FAIL] YOMKROS2_SUB_TOPIC");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2API", "[PASS] YOMKROS2_SUB_TOPIC");
        g_pass++;
    }

    // 测试 3：宏注册发布主题（阻塞期间由用户在回调/其他线程中发布）
    if (!YOMKROS2_PUB_TOPIC(std_msgs::msg::String, "api_topic", 10))
    {
        YOMK_ERROR_TAG("TestYomkROS2API", "[FAIL] YOMKROS2_PUB_TOPIC");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2API", "[PASS] YOMKROS2_PUB_TOPIC");
        g_pass++;
    }

    // 测试 4：宏运行节点（阻塞模式）：永久阻塞主线程，直至用户 Ctrl+C/信号退出
    const bool runReturned = YOMKROS2_RUN(true);
    if (runReturned)
    {
        YOMK_INFO_TAG("TestYomkROS2API", "[PASS] YOMKROS2_RUN(true) returned after signal");
        g_pass++;
    }
    else
    {
        YOMK_ERROR_TAG("TestYomkROS2API", "[FAIL] YOMKROS2_RUN(true)");
        g_fail++;
    }

    // 测试 5：run 返回后由用户控制的清理（此处执行 shutdown）
    if (!YOMKROS2_SHUTDOWN())
    {
        YOMK_ERROR_TAG("TestYomkROS2API", "[FAIL] YOMKROS2_SHUTDOWN");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2API", "[PASS] YOMKROS2_SHUTDOWN");
        g_pass++;
    }

    std::cout << "\n========== Test Summary (API) ==========" << std::endl;
    std::cout << "PASS: " << g_pass << std::endl;
    std::cout << "FAIL: " << g_fail << std::endl;

    return g_fail > 0 ? 1 : 0;
}
