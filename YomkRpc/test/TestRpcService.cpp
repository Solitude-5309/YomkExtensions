#include <YomkServer/YomkAPI.h>
#include <YomkRpc/YomkRpcService.h>
#include <YomkRpcMsg/YomkRpcMsg.hpp>
#include <YomkRpcMsg/YomkRpcMsgPubSubTypes.hpp>
#include <atomic>
#include <chrono>
#include <cmath>
#include <iostream>
#include <mutex>
#include <thread>

using namespace yomk;

static int g_pass = 0;
static int g_fail = 0;

void check(const char *name, bool condition)
{
    if (condition)
    {
        std::cout << "[PASS] " << name << std::endl;
        g_pass++;
    }
    else
    {
        std::cout << "[FAIL] " << name << std::endl;
        g_fail++;
    }
}

int main(int argc, char *argv[])
{
    YOMK_INIT();
    YOMK_NEW_SERVICE(YomkRpcService);

    // 测试版本查询
    std::cout << "\n=== Test YomkRpcService::getVersion ===" << std::endl;
    YomkResponse resp = YOMK_REQUEST("/YomkRpcService/version", nullptr);
    if (resp.m_status == YomkResponse::eOk)
    {
        YomkUnPackPkg(resp.m_data, String, version);
        if (version && version->d.find("YomkRpc") != std::string::npos)
        {
            std::cout << "[PASS] getVersion returns valid version: " << version->d << std::endl;
            g_pass++;
        }
        else
        {
            std::cout << "[FAIL] getVersion returns invalid version" << std::endl;
            g_fail++;
        }
    }
    else
    {
        std::cout << "[FAIL] version request failed: " << resp.m_msg << std::endl;
        g_fail++;
    }

    // TODO: 后续添加更多测试用例
    // 示例：
    // std::cout << "\n=== Test YomkRpcService::remoteCall ===" << std::endl;
    // YomkResponse resp = YOMK_REQUEST("/YomkRpcService/call", YomkMkPtr(YRpcRequest, rpcReq));

    // 测试创建节点
    std::cout << "\n=== Test YomkRpcService::createNode ===" << std::endl;
    resp = YOMK_REQUEST("/YomkRpcService/create_node", YomkMkPtr(DDSNode, DDSNode{0, "node0"}));
    check("createNode node0", resp.m_status == YomkResponse::eOk);

    // 重复创建同名节点应失败
    resp = YOMK_REQUEST("/YomkRpcService/create_node", YomkMkPtr(DDSNode, DDSNode{0, "node0"}));
    check("createNode duplicate should fail", resp.m_status == YomkResponse::eNo);

    // 向不存在的节点注册应失败
    resp = YOMK_REQUEST("/YomkRpcService/register_pub_topic",
                        YomkMkPtr(DDSTopic, DDSTopic{"no_such_node", "rpc_test_topic", nullptr}));
    check("registerPubTopic on missing node should fail", resp.m_status == YomkResponse::eNo);

    // 测试注册发布主题
    std::cout << "\n=== Test YomkRpcService::registerPubTopic ===" << std::endl;
    resp = YOMK_REQUEST("/YomkRpcService/register_pub_topic",
                        YomkMkPtr(DDSTopic, DDSTopic{"node0", "rpc_test_topic", new YomkRpc::MStringPubSubType()}));
    check("registerPubTopic rpc_test_topic", resp.m_status == YomkResponse::eOk);

    // 测试注册订阅主题
    std::cout << "\n=== Test YomkRpcService::registerSubTopic ===" << std::endl;
    YomkRpc::MString recvBuf;
    std::atomic<int> received{0};
    std::mutex msgMtx;
    std::string lastMsg;
    DDSSubRequest subReq{"node0", "rpc_test_topic", new YomkRpc::MStringPubSubType(), &recvBuf,
                         [&](const void *data)
                         {
                             auto *msg = static_cast<const YomkRpc::MString *>(data);
                             std::lock_guard<std::mutex> lock(msgMtx);
                             lastMsg = msg->data();
                             received++;
                             std::cout << "Received: " << msg->data() << std::endl;
                         }};
    resp = YOMK_REQUEST("/YomkRpcService/register_sub_topic", YomkMkPtr(DDSSubRequest, subReq));
    check("registerSubTopic rpc_test_topic", resp.m_status == YomkResponse::eOk);

    // 等待 DDS discovery 完成后发布数据
    std::cout << "\n=== Test YomkRpcService::publish ===" << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(1));
    bool allPublished = true;
    for (int i = 0; i < 5; ++i)
    {
        YomkRpc::MString msg;
        msg.data("Hello YomkRpcService " + std::to_string(i));
        resp = YOMK_REQUEST("/YomkRpcService/publish",
                            YomkMkPtr(DDSPublish, DDSPublish{"node0", "rpc_test_topic", &msg}));
        if (resp.m_status != YomkResponse::eOk)
        {
            allPublished = false;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    check("publish 5 messages", allPublished);

    // 等待接收完成，首条消息可能因 discovery 时序丢失，收到 4 条以上视为通过
    std::this_thread::sleep_for(std::chrono::seconds(1));
    std::cout << "Total received: " << received.load() << "/5" << std::endl;
    check("subscriber received >= 4 messages", received.load() >= 4);

    // 测试删除节点：删除不存在的节点应失败
    std::cout << "\n=== Test YomkRpcService::deleteNode ===" << std::endl;
    resp = YOMK_REQUEST("/YomkRpcService/delete_node", YomkMkPtr(DDSNode, DDSNode{0, "no_such_node"}));
    check("deleteNode missing node should fail", resp.m_status == YomkResponse::eNo);

    // 退出前销毁节点，确保 DDS 实体在 FastDDS 静态资源销毁前清理
    resp = YOMK_REQUEST("/YomkRpcService/delete_node", YomkMkPtr(DDSNode, DDSNode{0, "node0"}));
    check("deleteNode node0", resp.m_status == YomkResponse::eOk);

    std::cout << "\n========== Test Summary ==========" << std::endl;
    std::cout << "PASS: " << g_pass << std::endl;
    std::cout << "FAIL: " << g_fail << std::endl;

    return g_fail > 0 ? 1 : 0;
}
