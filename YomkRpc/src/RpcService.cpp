#include "RpcService.h"

RpcService::RpcService(YomkServer *server)
    : YomkService(server)
{
    name("/RpcService");
}

int RpcService::init()
{
    // 占位功能
    YomkInstallFunc("/version", RpcService::getVersion);
    YOMK_INFO_TAG("RpcService::init", "install func [ /version ] to", name());

    // TODO: 后续添加 RPC 功能注册
    // 示例：
    // YomkInstallFunc("/call", RpcService::remoteCall);
    // YomkInstallFunc("/register", RpcService::serviceRegister);

    return 0;
}

YomkResponse RpcService::getVersion(YomkPkgPtr pkg)
{
    std::string version = "YomkRpc v1.0.0 (WIP)";
    YOMK_INFO_TAG("RpcService::getVersion", version);
    return YomkResponse(YomkResponse::eOk, "ok", YomkMkPtr(String, version));
}
