// FastDDSManager 临时验证程序：同一进程内注册发布/订阅主题并收发 MString 数据
#include <atomic>
#include <chrono>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include <YomkRpc/FastDDSManager.h>
#include <YomkRpcMsg/YomkRpcMsg.hpp>
#include <YomkRpcMsg/YomkRpcMsgPubSubTypes.hpp>

int main()
{
    FastDDSManager dds;

    // 1. 设置域 ID，创建参与者
    if (!dds.setDomainId(0))
    {
        std::cerr << "setDomainId failed" << std::endl;
        return 1;
    }

    // 3. 注册发布主题
    if (!dds.registerPubTopic("manager_test_topic", new YomkRpc::MStringPubSubType()))
    {
        std::cerr << "registerPubTopic failed" << std::endl;
        return 1;
    }

    // 2. 注册订阅主题
    YomkRpc::MString recvBuf;
    std::atomic<int> received{0};
    std::string lastMsg;
    std::mutex msgMtx;
    if (!dds.registerSubTopic("manager_test_topic", new YomkRpc::MStringPubSubType(), &recvBuf,
                              [&](const void *data)
                              {
                                  auto *msg = static_cast<const YomkRpc::MString *>(data);
                                  std::lock_guard<std::mutex> lock(msgMtx);
                                  lastMsg = msg->data();
                                  received++;
                                  std::cout << "Received: " << msg->data() << std::endl;
                              }))
    {
        std::cerr << "registerSubTopic failed" << std::endl;
        return 1;
    }

    // 等待 DDS discovery 完成
    std::this_thread::sleep_for(std::chrono::seconds(1));

    // 4. 发布数据
    for (int i = 0; i < 5; ++i)
    {
        YomkRpc::MString msg;
        msg.data("Hello FastDDSManager " + std::to_string(i));
        if (!dds.publish("manager_test_topic", &msg))
        {
            std::cerr << "publish failed" << std::endl;
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    // 等待接收完成
    std::this_thread::sleep_for(std::chrono::seconds(1));

    std::cout << "Total received: " << received.load() << "/5" << std::endl;
    // 首条消息可能因 discovery 时序丢失，收到 4 条以上视为通过
    return received.load() >= 4 ? 0 : 1;
}
