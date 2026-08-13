#include <YomkServer/YomkAPI.h>
#include <YomkROS2/YomkRos2Service.h>
#include <iostream>

using namespace yomk;

static int g_pass = 0;
static int g_fail = 0;

int main(int argc, char *argv[])
{
    YOMK_INIT();
    YOMK_NEW_SERVICE(YomkRos2Service);

    // 测试版本查询
    YomkResponse resp = YOMK_REQUEST("/YomkRos2Service/version", nullptr);
    if (resp.m_status == YomkResponse::eOk)
    {
        YomkUnPackPkg(resp.m_data, String, version);
        if (version)
        {
            YOMK_INFO_TAG("TestYomkRos2", "[PASS] version: ", version->d);
            g_pass++;
        }
        else
        {
            YOMK_ERROR_TAG("TestYomkRos2", "[FAIL] version data is null");
            g_fail++;
        }
    }
    else
    {
        YOMK_ERROR_TAG("TestYomkRos2", "[FAIL] version request failed: ", resp.m_msg);
        g_fail++;
    }

    std::cout << "\n========== Test Summary ==========" << std::endl;
    std::cout << "PASS: " << g_pass << std::endl;
    std::cout << "FAIL: " << g_fail << std::endl;

    return g_fail > 0 ? 1 : 0;
}
