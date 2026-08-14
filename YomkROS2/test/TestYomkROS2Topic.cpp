#include <YomkServer/YomkAPI.h>
#include <YomkROS2/YomkROS2API.h>

#include <std_msgs/msg/string.hpp>

#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

using namespace yomk;

static int g_pass = 0;
static int g_fail = 0;

// 接收统计（订阅回调由后台 spin 线程调用，需加锁保护）
static std::mutex g_recvMutex;
static std::vector<std::string> g_received;

// 本测试展示宏 API 的主题发布订阅完整闭环：
// init → 注册订阅/发布主题 → run(true) 阻塞主线程（原生 ROS2 语义）→
// 辅助线程以 PUB_MSG 发布 3 条消息 → 订阅回调打印接收内容 →
// 辅助线程 shutdown 唤醒阻塞的 run(true) → 主线程汇总发布/接收结果并退出。
int main(int argc, char *argv[])
{
    YOMK_INIT();

    // 测试 1：宏初始化节点（透传 argc/argv，与原生 rclcpp::init 语义一致）
    if (!YOMKROS2_NODE(argc, argv, "topic_node"))
    {
        YOMK_ERROR_TAG("TestYomkROS2Topic", "[FAIL] YOMKROS2_NODE");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Topic", "[PASS] YOMKROS2_NODE: 节点名=topic_node");
        g_pass++;
    }

    // 测试 2：宏注册订阅主题（回调定义为变量后传入，规避回调体内顶层逗号的宏展开问题）
    auto onMessage = [](std::shared_ptr<const std_msgs::msg::String> msg)
    {
        YOMK_INFO_TAG("TestYomkROS2Topic", "[RECV] 收到消息: ", msg->data);
        std::lock_guard<std::mutex> lock(g_recvMutex);
        g_received.push_back(msg->data);
    };
    if (!YOMKROS2_SUB_TOPIC(std_msgs::msg::String, "hello_topic", 10, onMessage))
    {
        YOMK_ERROR_TAG("TestYomkROS2Topic", "[FAIL] YOMKROS2_SUB_TOPIC");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Topic", "[PASS] YOMKROS2_SUB_TOPIC: 主题=hello_topic 队列深度=10");
        g_pass++;
    }

    // 测试 3：宏注册发布主题
    if (!YOMKROS2_PUB_TOPIC(std_msgs::msg::String, "hello_topic", 10))
    {
        YOMK_ERROR_TAG("TestYomkROS2Topic", "[FAIL] YOMKROS2_PUB_TOPIC");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Topic", "[PASS] YOMKROS2_PUB_TOPIC: 主题=hello_topic 队列深度=10");
        g_pass++;
    }

    // 测试 4：辅助线程以 YOMKROS2_PUB_MSG 发布 3 条消息，随后 shutdown 唤醒阻塞的 run(true)
    std::thread publisher([]
                          {
                              std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 等待 run(true) 完成 spin 启动
                              for (int i = 1; i <= 3; ++i)
                              {
                                  std_msgs::msg::String msg;
                                  msg.data = "hello_" + std::to_string(i);
                                  if (YOMKROS2_PUB_MSG("hello_topic", msg))
                                  {
                                      YOMK_INFO_TAG("TestYomkROS2Topic", "[SEND] 发布消息: ", msg.data);
                                  }
                                  else
                                  {
                                      YOMK_ERROR_TAG("TestYomkROS2Topic", "[FAIL] YOMKROS2_PUB_MSG: ", msg.data);
                                  }
                                  std::this_thread::sleep_for(std::chrono::milliseconds(300));
                              }
                              std::this_thread::sleep_for(std::chrono::milliseconds(500)); // 等待订阅回调处理完毕
                              YOMKROS2_SHUTDOWN();                                         // 唤醒主线程阻塞的 run(true)
                          });

    // 测试 5：宏运行节点（阻塞模式）：永久阻塞主线程，直至 YOMKROS2_SHUTDOWN 唤醒
    const bool runReturned = YOMKROS2_RUN(true);
    publisher.join();
    if (runReturned)
    {
        YOMK_INFO_TAG("TestYomkROS2Topic", "[PASS] YOMKROS2_RUN(true): 阻塞运行，shutdown 后正常返回");
        g_pass++;
    }
    else
    {
        YOMK_ERROR_TAG("TestYomkROS2Topic", "[FAIL] YOMKROS2_RUN(true)");
        g_fail++;
    }

    // 测试 6：汇总发布/接收结果（关键数据）：3 条消息全部收到且内容一致
    bool recvOk = false;
    std::size_t recvCount = 0;
    std::string recvSummary;
    {
        std::lock_guard<std::mutex> lock(g_recvMutex);
        recvCount = g_received.size();
        recvOk = (recvCount == 3) &&
                 (g_received[0] == "hello_1") && (g_received[1] == "hello_2") && (g_received[2] == "hello_3");
        for (std::size_t i = 0; i < recvCount; ++i)
        {
            if (i > 0)
            {
                recvSummary += ", ";
            }
            recvSummary += g_received[i];
        }
    }
    if (!recvOk)
    {
        YOMK_ERROR_TAG("TestYomkROS2Topic", "[FAIL] 接收结果: 发布=3 接收=", recvCount, " 内容=[", recvSummary, "]");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Topic", "[PASS] 接收结果: 发布=3 接收=3 内容=[", recvSummary, "]");
        g_pass++;
    }

    // 测试 7：shutdown 幂等（已由辅助线程调用过，再次调用返回 true）
    if (!YOMKROS2_SHUTDOWN())
    {
        YOMK_ERROR_TAG("TestYomkROS2Topic", "[FAIL] YOMKROS2_SHUTDOWN");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Topic", "[PASS] YOMKROS2_SHUTDOWN: 节点已销毁");
        g_pass++;
    }

    std::cout << "\n========== Test Summary (Topic) ==========" << std::endl;
    std::cout << "PASS: " << g_pass << std::endl;
    std::cout << "FAIL: " << g_fail << std::endl;

    return g_fail > 0 ? 1 : 0;
}
