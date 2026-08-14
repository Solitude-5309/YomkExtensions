#include <YomkServer/YomkAPI.h>
#include <YomkROS2/YomkROS2API.h>

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace yomk;

static int g_pass = 0;
static int g_fail = 0;

// 参数接口宏 API 测试：被测接口全部通过 YOMKROS2_* 宏调用全局单例完成本地与远程参数操作。
// 远程参数服务端（remote_node）由独立 ROS2Node 实例充当（宏为全局单例，双节点场景需
// 另一实例承载服务端，仅作测试环境，非被测接口）。
// 非阻塞模式：单例与远程节点均 run(false) 后台 spin（异步客户端 future 的响应依赖本地 spin），
// 全部用例执行完毕后自行退出。
int main(int argc, char *argv[])
{
    YOMK_INIT();

    ROS2Node remote; // 远程参数服务端（测试环境，非被测接口）

    // 测试 1：宏初始化单例节点 + 远程服务端节点初始化（环境）
    if (!YOMKROS2_NODE(argc, argv, "param_node") || !remote.init(argc, argv, "remote_node"))
    {
        YOMK_ERROR_TAG("TestYomkROS2Param", "[FAIL] YOMKROS2_NODE 初始化");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Param", "[PASS] YOMKROS2_NODE: 单例节点=param_node 远程服务端=remote_node（环境）");
        g_pass++;
    }

    // 测试 2：宏声明参数一次调用取到默认值
    if (YOMKROS2_DECLARE_PARAM("my_int", int64_t(42)) != 42)
    {
        YOMK_ERROR_TAG("TestYomkROS2Param", "[FAIL] YOMKROS2_DECLARE_PARAM 默认值");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Param", "[PASS] YOMKROS2_DECLARE_PARAM: my_int=42（首次声明返回默认值）");
        g_pass++;
    }

    // 测试 3：重复声明幂等（返回现有值而非新默认值）
    if (YOMKROS2_DECLARE_PARAM("my_int", int64_t(99)) != 42)
    {
        YOMK_ERROR_TAG("TestYomkROS2Param", "[FAIL] YOMKROS2_DECLARE_PARAM 幂等");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Param", "[PASS] YOMKROS2_DECLARE_PARAM 幂等: my_int 再次声明 99 返回现有值 42");
        g_pass++;
    }

    // 测试 4：多类型声明读回：double、string、bool、vector<int64_t>
    // （vector 默认值含顶层逗号，先定义为变量再传入）
    const std::vector<int64_t> vecDefault{1, 2, 3};
    const bool multiOk = (YOMKROS2_DECLARE_PARAM("my_double", 3.14) == 3.14) &&
                         (YOMKROS2_DECLARE_PARAM("my_string", std::string("hello")) == "hello") &&
                         (YOMKROS2_DECLARE_PARAM("my_bool", true) == true) &&
                         (YOMKROS2_DECLARE_PARAM("my_vec", vecDefault) == vecDefault);
    if (!multiOk)
    {
        YOMK_ERROR_TAG("TestYomkROS2Param", "[FAIL] 多类型声明读回");
        g_fail++;
    }
    else
    {
        std::ostringstream oss;
        oss << "my_double=" << 3.14 << " my_string=hello my_bool=true my_vec=[";
        for (std::size_t i = 0; i < vecDefault.size(); ++i)
        {
            if (i > 0)
            {
                oss << ",";
            }
            oss << vecDefault[i];
        }
        oss << "]";
        YOMK_INFO_TAG("TestYomkROS2Param", "[PASS] 多类型声明读回: ", oss.str());
        g_pass++;
    }

    // 测试 5：宏查询参数读出正确值
    int64_t v = 0;
    if (!YOMKROS2_GET_PARAM("my_int", v) || v != 42)
    {
        YOMK_ERROR_TAG("TestYomkROS2Param", "[FAIL] YOMKROS2_GET_PARAM 读值");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Param", "[PASS] YOMKROS2_GET_PARAM: my_int=", v);
        g_pass++;
    }

    // 测试 6：宏设置参数后查询读到新值
    if (!YOMKROS2_SET_PARAM("my_int", int64_t(99)) || !YOMKROS2_GET_PARAM("my_int", v) || v != 99)
    {
        YOMK_ERROR_TAG("TestYomkROS2Param", "[FAIL] YOMKROS2_SET_PARAM 设置");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Param", "[PASS] YOMKROS2_SET_PARAM: my_int 42 -> ", v);
        g_pass++;
    }

    // 测试 7：类型不匹配返回 false（预期输出一条 RCLCPP_ERROR 日志）
    double d = 0;
    if (YOMKROS2_GET_PARAM("my_int", d))
    {
        YOMK_ERROR_TAG("TestYomkROS2Param", "[FAIL] 类型不匹配应返回 false");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Param", "[PASS] 类型不匹配: my_int（整数）以 double 读返回 false");
        g_pass++;
    }

    // 测试 8：宏判断参数存在/不存在
    if (!YOMKROS2_HAS_PARAM("my_int") || YOMKROS2_HAS_PARAM("not_exist"))
    {
        YOMK_ERROR_TAG("TestYomkROS2Param", "[FAIL] YOMKROS2_HAS_PARAM 存在性判断");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Param", "[PASS] YOMKROS2_HAS_PARAM: my_int=true not_exist=false");
        g_pass++;
    }

    // 测试 9：宏列出全部参数名包含已声明参数
    bool listOk = false;
    std::string namesStr;
    const auto names = YOMKROS2_LIST_PARAMS();
    for (std::size_t i = 0; i < names.size(); ++i)
    {
        if (names[i] == "my_int")
        {
            listOk = true;
        }
        if (i > 0)
        {
            namesStr += ", ";
        }
        namesStr += names[i];
    }
    if (!listOk)
    {
        YOMK_ERROR_TAG("TestYomkROS2Param", "[FAIL] YOMKROS2_LIST_PARAMS 包含参数名");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Param", "[PASS] YOMKROS2_LIST_PARAMS: 共 ", names.size(), " 个参数 [", namesStr, "]");
        g_pass++;
    }

    // 测试 10：宏撤销参数——带默认值声明的参数为静态类型不可撤销（rclcpp 语义），
    // 返回 false 且参数仍存在（预期输出两条 RCLCPP_ERROR 日志）；未声明的参数撤销同样返回 false
    if (YOMKROS2_UNDECLARE_PARAM("my_double") || !YOMKROS2_HAS_PARAM("my_double") ||
        YOMKROS2_UNDECLARE_PARAM("not_exist"))
    {
        YOMK_ERROR_TAG("TestYomkROS2Param", "[FAIL] YOMKROS2_UNDECLARE_PARAM 静态类型不可撤销");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Param", "[PASS] YOMKROS2_UNDECLARE_PARAM: my_double 静态类型不可撤销返回 false，参数仍存在");
        g_pass++;
    }

    // 测试 11：宏运行单例节点（后台 spin，异步客户端 future 的响应依赖本地 spin）；
    // 远程服务端运行并声明参数（环境）
    if (!YOMKROS2_RUN(false) || !remote.run(false) || remote.declareParam<int64_t>("remote_int", 7) != 7)
    {
        YOMK_ERROR_TAG("TestYomkROS2Param", "[FAIL] YOMKROS2_RUN 运行并声明远程参数");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Param", "[PASS] YOMKROS2_RUN(false): 后台 spin 启动，远程节点声明 remote_int=7");
        g_pass++;
    }

    // 测试 12：宏查询远程参数一次调用读出正确值（自动建客户端）
    int64_t rv = 0;
    if (!YOMKROS2_GET_REMOTE_PARAM("remote_node", "remote_int", rv) || rv != 7)
    {
        YOMK_ERROR_TAG("TestYomkROS2Param", "[FAIL] YOMKROS2_GET_REMOTE_PARAM 读远程参数");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Param", "[PASS] YOMKROS2_GET_REMOTE_PARAM: remote_node/remote_int=", rv);
        g_pass++;
    }

    // 测试 13：宏设置远程参数后查询读到新值（验证设置生效）
    if (!YOMKROS2_SET_REMOTE_PARAM("remote_node", "remote_int", int64_t(8)) ||
        !YOMKROS2_GET_REMOTE_PARAM("remote_node", "remote_int", rv) || rv != 8)
    {
        YOMK_ERROR_TAG("TestYomkROS2Param", "[FAIL] YOMKROS2_SET_REMOTE_PARAM 设远程参数");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Param", "[PASS] YOMKROS2_SET_REMOTE_PARAM: remote_node/remote_int 7 -> ", rv);
        g_pass++;
    }

    // 测试 14：宏判断远程参数存在/不存在
    if (!YOMKROS2_HAS_REMOTE_PARAM("remote_node", "remote_int") ||
        YOMKROS2_HAS_REMOTE_PARAM("remote_node", "not_exist"))
    {
        YOMK_ERROR_TAG("TestYomkROS2Param", "[FAIL] YOMKROS2_HAS_REMOTE_PARAM 存在性判断");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Param", "[PASS] YOMKROS2_HAS_REMOTE_PARAM: remote_int=true not_exist=false");
        g_pass++;
    }

    // 测试 15：宏列出远程参数名包含已声明参数
    bool listRemoteOk = false;
    std::string remoteNamesStr;
    const auto remoteNames = YOMKROS2_LIST_REMOTE_PARAMS("remote_node");
    for (std::size_t i = 0; i < remoteNames.size(); ++i)
    {
        if (remoteNames[i] == "remote_int")
        {
            listRemoteOk = true;
        }
        if (i > 0)
        {
            remoteNamesStr += ", ";
        }
        remoteNamesStr += remoteNames[i];
    }
    if (!listRemoteOk)
    {
        YOMK_ERROR_TAG("TestYomkROS2Param", "[FAIL] YOMKROS2_LIST_REMOTE_PARAMS 包含参数名");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Param", "[PASS] YOMKROS2_LIST_REMOTE_PARAMS: 共 ", remoteNames.size(), " 个参数 [", remoteNamesStr, "]");
        g_pass++;
    }

    // 测试 16：shutdown 干净退出（先关远程服务端，再关单例——单例持有进程级 rclcpp::init）
    if (!remote.shutdown() || !YOMKROS2_SHUTDOWN())
    {
        YOMK_ERROR_TAG("TestYomkROS2Param", "[FAIL] YOMKROS2_SHUTDOWN 干净退出");
        g_fail++;
    }
    else
    {
        YOMK_INFO_TAG("TestYomkROS2Param", "[PASS] YOMKROS2_SHUTDOWN: 远程服务端与单例节点均已销毁");
        g_pass++;
    }

    std::cout << "\n========== Test Summary (Param) ==========" << std::endl;
    std::cout << "PASS: " << g_pass << std::endl;
    std::cout << "FAIL: " << g_fail << std::endl;

    return g_fail > 0 ? 1 : 0;
}
