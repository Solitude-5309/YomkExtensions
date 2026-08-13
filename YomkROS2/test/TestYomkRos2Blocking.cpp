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
    if (!node.init(argc, argv, "blocking_node"))
    {
        YOMK_ERROR_TAG("TestYomkRos2Blocking", "[FAIL] ROS2Node init");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkRos2Blocking", "[PASS] ROS2Node init");
        g_pass++;
    }

    // 测试 2：注册订阅主题（回调输出接收消息日志）
    int recvCount = 0;
    std::mutex recvMtx;
    if (!node.registerSubTopic<std_msgs::msg::String>("blocking_topic", 10,
                                                      [&](std::shared_ptr<const std_msgs::msg::String> msg)
                                                      {
                                                          std::lock_guard<std::mutex> lock(recvMtx);
                                                          ++recvCount;
                                                          YOMK_INFO_TAG("TestYomkRos2Blocking", "[RECV] ", msg->data);
                                                      }))
    {
        YOMK_ERROR_TAG("TestYomkRos2Blocking", "[FAIL] registerSubTopic");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkRos2Blocking", "[PASS] registerSubTopic");
        g_pass++;
    }

    // 测试 3：注册发布主题
    if (!node.registerPubTopic<std_msgs::msg::String>("blocking_topic", 10))
    {
        YOMK_ERROR_TAG("TestYomkRos2Blocking", "[FAIL] registerPubTopic");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkRos2Blocking", "[PASS] registerPubTopic");
        g_pass++;
    }

    // 测试 4：阻塞运行 run(true)：主线程阻塞于 spin，
    // 辅助线程等待 discovery 建立后发布消息，再调用 shutdown 唤醒主线程
    {
        std::thread helper([&node]()
                           {
                               std::this_thread::sleep_for(std::chrono::milliseconds(500));
                               std_msgs::msg::String msg;
                               msg.data = "blocking mode message";
                               node.publish<std_msgs::msg::String>("blocking_topic", msg);
                               std::this_thread::sleep_for(std::chrono::milliseconds(500));
                               //    node.shutdown(); // 唤醒阻塞中的 run(true)
                           });
        const bool runReturned = node.run(true); // 阻塞直至 shutdown
        helper.join();
        std::lock_guard<std::mutex> lock(recvMtx);
        if (runReturned && recvCount >= 1)
        {
            YOMK_INFO_TAG("TestYomkRos2Blocking", "[PASS] blocking run returned after shutdown, received ", recvCount, " message");
            g_pass++;
        }
        else
        {
            YOMK_ERROR_TAG("TestYomkRos2Blocking", "[FAIL] blocking run: returned=", runReturned, " received=", recvCount);
            g_fail++;
        }
    }

    std::cout << "\n========== Test Summary (Blocking) ==========" << std::endl;
    std::cout << "PASS: " << g_pass << std::endl;
    std::cout << "FAIL: " << g_fail << std::endl;

    return g_fail > 0 ? 1 : 0;
}
