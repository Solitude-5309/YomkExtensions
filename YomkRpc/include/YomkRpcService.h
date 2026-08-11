#pragma once
#include <YomkServer/YomkAPI.h>

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include "FastDDSNode.h"

using namespace yomk;

// ============================================
// YomkRpc 扩展 - DDS 发布订阅通信
// ============================================
// DDS 订阅回调函数，参数为注册订阅时传入的数据包指针
using DDSCallbackFunc = std::function<void(const void *)>;

class YomkRpcService : public YomkService
{
public:
    YomkRpcService(YomkServer *server);
    virtual ~YomkRpcService() {}
    virtual int init() override;

private:
    // 版本查询
    YomkResponse getVersion(YomkPkgPtr pkg);
    // DDS 节点与主题管理
    YomkResponse createNode(YomkPkgPtr pkg);
    YomkResponse deleteNode(YomkPkgPtr pkg);
    YomkResponse registerPubTopic(YomkPkgPtr pkg);
    YomkResponse registerSubTopic(YomkPkgPtr pkg);
    YomkResponse publish(YomkPkgPtr pkg);

private:
    // 节点表：节点名 -> FastDDSNode（每个节点一个独立 DomainParticipant）
    std::map<std::string, std::unique_ptr<FastDDSNode>> nodes_;
    std::mutex mtx_;
};

// 创建节点：域 id + 节点名
struct DDSNode
{
    uint32_t domainId;
    std::string nodeName;
};

// 注册发布 topic：type 为用户传入的 PubSubType 实例（如 new MStringPubSubType()），
// 所有权移交 FastDDSNode（由 TypeSupport 接管）
struct DDSTopic
{
    std::string nodeName;
    std::string topicName;
    void *type;
};

// 注册订阅 topic：type 为 PubSubType 实例（所有权移交 FastDDSNode），
// data 为接收缓冲（生命周期由调用方保证覆盖订阅期），callback 收到数据时回调
struct DDSSubRequest
{
    std::string nodeName;
    std::string topicName;
    void *type;
    void *data;
    DDSCallbackFunc callback;
};

// 发布数据：data 为对应数据类型实例的指针
struct DDSPublish
{
    std::string nodeName;
    std::string topicName;
    void *data;
};

// clang-format off
YomkMsg(DDSNode, DDSNode, msg)
YomkMsg(DDSTopic, DDSTopic, msg)
YomkMsg(DDSSubRequest, DDSSubRequest, msg)
YomkMsg(DDSPublish, DDSPublish, msg)