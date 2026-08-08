#include <YomkServer/YomkAPI.h>
#include <YomkRpc/RpcService.h>
#include <iostream>
#include <cmath>

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
    YOMK_NEW_SERVICE(RpcService);

    // 测试版本查询
    std::cout << "\n=== Test RpcService::getVersion ===" << std::endl;
    YomkResponse resp = YOMK_REQUEST("/RpcService/version", nullptr);
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
    // std::cout << "\n=== Test RpcService::remoteCall ===" << std::endl;
    // YomkResponse resp = YOMK_REQUEST("/RpcService/call", YomkMkPtr(YRpcRequest, rpcReq));

    std::cout << "\n========== Test Summary ==========" << std::endl;
    std::cout << "PASS: " << g_pass << std::endl;
    std::cout << "FAIL: " << g_fail << std::endl;

    return g_fail > 0 ? 1 : 0;
}
