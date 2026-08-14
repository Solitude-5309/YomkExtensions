#include <YomkServer/YomkAPI.h>
#include <YomkROS2/ROS2Node.h>

#include <std_msgs/msg/string.hpp>

#include <chrono>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

using namespace yomk;

static int g_pass = 0;
static int g_fail = 0;

int main(int argc, char *argv[])
{
    YOMK_INIT();

    // 测试 1：ROS2Node 初始化
    ROS2Node node;
    if (!node.init(argc, argv, "test_node"))
    {
        YOMK_ERROR_TAG("TestYomkROS2NonBlocking", "[FAIL] ROS2Node init");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2NonBlocking", "[PASS] ROS2Node init");
        g_pass++;
    }

    // 测试 2：注册订阅主题（回调逐条输出接收消息日志）
    int recvCount = 0;
    std::mutex recvMtx;
    if (!node.registerSubTopic<std_msgs::msg::String>("test_topic", 10,
                                                      [&](std::shared_ptr<const std_msgs::msg::String> msg)
                                                      {
                                                          std::lock_guard<std::mutex> lock(recvMtx);
                                                          ++recvCount;
                                                          YOMK_INFO_TAG("TestYomkROS2NonBlocking", "[RECV] #", recvCount, " ", msg->data);
                                                      }))
    {
        YOMK_ERROR_TAG("TestYomkROS2NonBlocking", "[FAIL] registerSubTopic");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2NonBlocking", "[PASS] registerSubTopic");
        g_pass++;
    }

    // 测试 3：注册发布主题
    if (!node.registerPubTopic<std_msgs::msg::String>("test_topic", 10))
    {
        YOMK_ERROR_TAG("TestYomkROS2NonBlocking", "[FAIL] registerPubTopic");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2NonBlocking", "[PASS] registerPubTopic");
        g_pass++;
    }

    // 测试 4：非阻塞运行 run(false)（后台线程 spin），重复 run 返回 false
    if (!node.run(false))
    {
        YOMK_ERROR_TAG("TestYomkROS2NonBlocking", "[FAIL] run(false)");
        g_fail++;
    }
    else if (node.run(false))
    {
        YOMK_ERROR_TAG("TestYomkROS2NonBlocking", "[FAIL] duplicate run should return false");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2NonBlocking", "[PASS] run(false), duplicate run rejected");
        g_pass++;
    }

    // 测试 5：发布 5 条数据（带序号，便于日志区分每条消息）
    bool publishOk = true;
    for (int i = 0; i < 5; ++i)
    {
        std_msgs::msg::String msg;
        msg.data = "hello yomk ros2 #" + std::to_string(i);
        if (!node.publish<std_msgs::msg::String>("test_topic", msg))
        {
            publishOk = false;
            break;
        }
    }
    if (!publishOk)
    {
        YOMK_ERROR_TAG("TestYomkROS2NonBlocking", "[FAIL] publish");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2NonBlocking", "[PASS] publish x5");
        g_pass++;
    }

    // 测试 6：等待收集所有消息并输出接收统计（发布订阅建立需 discovery 时间）
    {
        // 最多等待 5s，收满 5 条提前结束
        for (int i = 0; i < 50; ++i)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            std::lock_guard<std::mutex> lock(recvMtx);
            if (recvCount >= 5)
            {
                break;
            }
        }
        std::lock_guard<std::mutex> lock(recvMtx);
        if (recvCount == 0)
        {
            YOMK_ERROR_TAG("TestYomkROS2NonBlocking", "[FAIL] no message received");
            g_fail++;
        }
        else
        {
            YOMK_INFO_TAG("TestYomkROS2NonBlocking", "[PASS] total received: ", recvCount, " messages");
            g_pass++;
        }
    }

    // 测试 7：显式销毁节点（避免退出崩溃）
    if (!node.shutdown())
    {
        YOMK_ERROR_TAG("TestYomkROS2NonBlocking", "[FAIL] ROS2Node shutdown");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2NonBlocking", "[PASS] ROS2Node shutdown");
        g_pass++;
    }

    std::cout << "\n========== Test Summary (NonBlocking) ==========" << std::endl;
    std::cout << "PASS: " << g_pass << std::endl;
    std::cout << "FAIL: " << g_fail << std::endl;

    return g_fail > 0 ? 1 : 0;
}
