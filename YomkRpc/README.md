# YomkRpc 扩展

基于 [YomkServer](https://github.com/Solitude-5309/YomkServer) 框架的 RPC 分布式通信扩展。

## 功能

基于 YomkServer 框架的 RPC 扩展，集成 FastDDS 提供分布式发布订阅能力。

| URL | 功能 | 说明 |
|-----|------|------|
| `/YomkRpcService/version` | 版本查询 | 返回 YomkRpc 扩展版本信息 |
| `/YomkRpcService/create_node` | 创建节点 | 传入 `DDSNode{domainId, nodeName}`，每节点一个独立 DDS 参与者 |
| `/YomkRpcService/delete_node` | 删除节点 | 销毁节点及其全部 DDS 实体，不存在的节点返回错误 |
| `/YomkRpcService/register_pub_topic` | 注册发布主题 | 传入 `DDSTopic{nodeName, topicName, type}`，type 为 PubSubType 实例指针 |
| `/YomkRpcService/register_sub_topic` | 注册订阅主题 | 传入 `DDSSubRequest{nodeName, topicName, type, callback}`，data 由内部自动创建 |
| `/YomkRpcService/publish` | 发布数据 | 传入 `DDSPublish{nodeName, topicName, data}`，data 为数据实例指针 |

## 前置条件

- C++17 编译器
- CMake >= 3.14
- YomkServer 已安装

## 编译

```bash
# 默认安装到 YomkRpc/install/
source build.sh -DCMAKE_PREFIX_PATH=~/YomkServer/install

# 自定义安装目录
source build.sh -DCMAKE_PREFIX_PATH=~/YomkServer/install -DCMAKE_INSTALL_PREFIX=~/YomkServer/install
```

脚本编译安装主库后自动编译并运行 `TestRpcService`，并设置好 LD_LIBRARY_PATH/PATH 环境变量。

## 工程结构

```
YomkRpc/
├── include/
│   ├── YomkRpcService.h    # 服务头文件（消息包定义 + 类声明）
│   └── YomkRpcAPI.h        # API 宏封装（简化调用）
├── src/
│   ├── YomkRpcService.cpp  # 服务实现
│   └── FastDDSNode.cpp     # DDS 节点实现
├── msg/
│   ├── YomkRpcMsg.idl      # IDL 消息定义（如 MString）
│   └── ...                 # fastddsgen 生成代码
├── test/
│   ├── CMakeLists.txt      # 测试程序构建
│   └── TestRpcService.cpp  # 服务接口端到端测试
├── cmake/
│   └── ProjectConfig.cmake.in  # CMake 导出配置模板
├── CMakeLists.txt            # CMake 构建配置
├── build.sh                  # 一键编译脚本
└── README.md
```

## 使用示例

```cpp
#include <YomkRpc/YomkRpcAPI.h>
#include <YomkRpcMsg/YomkRpcMsg.hpp>
#include <YomkRpcMsg/YomkRpcMsgPubSubTypes.hpp>

using namespace yomk;

// 启动服务
YOMK_INIT();
YOMK_NEW_SERVICE(YomkRpcService);

// 查看版本（宏内部自动解包并打印）
YOMKRPC_VERSION();

// 创建节点
YOMKRPC_NODE(0, "node0");

// 注册发布主题（type 所有权移交 FastDDSNode）
YOMKRPC_PUB_TOPIC("node0", "my_topic", new YomkRpc::MStringPubSubType());

// 注册订阅主题（data 由内部 create_data() 创建）
YOMKRPC_SUB_TOPIC("node0", "my_topic",
    new YomkRpc::MStringPubSubType(),
    [](const void *data) { /* 收到数据 */ });

// 发布数据
YomkRpc::MString msg;
msg.data("hello");
YOMKRPC_PUB_MSG("node0", "my_topic", &msg);

// 退出前删除节点
YOMKRPC_DEL_NODE("node0");
```

## 开发状态

- ✅ 基础框架搭建完成
- ✅ CMake 构建系统集成
- ✅ FastDDS 集成（FastDDSNode 发布订阅）
- ✅ YomkRpcService DDS 接口（节点管理/主题注册/发布）
- 🚧 跨进程/跨机通信验证
- 🚧 更多数据类型支持

## License

MIT License - 详见 [LICENSE.txt](../../LICENSE.txt)

## 链接

- [YomkServer 官方仓库](https://github.com/Solitude-5309/YomkServer)
- [YomkExtensions 扩展集合](https://github.com/Solitude-5309/YomkExtensions)
