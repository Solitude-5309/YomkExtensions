#pragma once
#include <YomkServer/YomkAPI.h>

using namespace yomk;

// ============================================
// YomkRpc 扩展 - RPC 分布式通信框架
// ============================================
// 消息包定义区域（后续添加）
// 示例：
// struct RpcRequest { std::string service; std::string method; std::string data; };
// YomkMsg(RpcRequest, YRpcRequest, req)

class RpcService : public YomkService
{
public:
    RpcService(YomkServer *server);
    virtual ~RpcService() {}
    virtual int init() override;

private:
    // 占位功能（后续添加具体实现）
    YomkResponse getVersion(YomkPkgPtr pkg);
};
