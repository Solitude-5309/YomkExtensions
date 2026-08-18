# Yomk Extensions

基于 [YomkServer](https://github.com/Solitude-5309/YomkServer) 框架的官方扩展库集合。

## 简介

YomkExtensions 是 YomkServer 的扩展模块仓库，提供了各种功能丰富的可复用服务组件。每个扩展都以共享库（`.so`）形式编译，支持通过 CMake `find_package()` 轻松集成到您的项目中。

## 核心特性

- **模块化设计**：每个扩展独立编译，只集成所需功能
- **热插拔架构**：基于 YomkServer 的"一切皆服务"理念
- **数据源无关**：扩展专注业务逻辑，不耦合具体数据源
- **CMake 友好**：提供完整的包导出配置，一键集成

## 目录结构

```
YomkExtensions/
├── YomkRpc/           # RPC 分布式通信扩展（FastDDS 发布订阅 + Loan 零拷贝）
├── YomkROS2/          # ROS2 通信扩展（主题/参数/服务/动作四类通信封装）
└── ...                # 更多扩展将持续添加
```

## 快速开始

### 1. 编译扩展

```bash
cd YomkExtensions/YomkRpc
source build.sh -DCMAKE_PREFIX_PATH=/path/to/YomkServer/install
```

### 2. 在工程中使用

```cmake
# CMakeLists.txt
find_package(YomkRpc REQUIRED)

add_executable(MyApp main.cpp)
target_link_libraries(MyApp PRIVATE YomkRpc::YomkRpc YomkServer::YomkServer YomkRpcMsg)
```

```cpp
// main.cpp（各扩展均提供 API 宏封装，示例统一使用宏 API）
#include <YomkServer/YomkAPI.h>
#include <YomkRpc/YomkRpcAPI.h>
#include <YomkRpcMsg/YomkRpcMsg.hpp>
#include <YomkRpcMsg/YomkRpcMsgPubSubTypes.hpp>

YOMK_INIT();
YOMK_NEW_SERVICE(YomkRpcService);

// 创建节点 + 注册发布主题 + 发布数据
YOMKRPC_NODE(0, "node0");
YOMKRPC_PUB_TOPIC("node0", "my_topic", new YomkRpc::MStringPubSubType());
YomkRpc::MString msg;
msg.data("hello");
YOMKRPC_PUB_MSG("node0", "my_topic", &msg);
```

各扩展的完整宏 API 与使用示例见子目录 README：[YomkRpc](./YomkRpc/README.md)、[YomkROS2](./YomkROS2/README.md)。

## 开发规范

所有扩展遵循统一的开发标准：

- **语言标准**：C++17
- **构建系统**：CMake >= 3.14
- **代码风格**：参见 [YomkServer SKILL 规范](./.qoder/skills/yomkserver-modular/SKILL.md)
- **目录结构**：`include/` + `src/` + `test/` + `cmake/`

## 贡献扩展

如需创建新的扩展，请参考技能文档中的[示例6：标准扩展模板](./.qoder/skills/yomkserver-modular/examples.md)：

```bash
# 使用模板快速创建
# 详见 examples.md 中的完整模板
```

## License

MIT License - 详见 [LICENSE.txt](LICENSE.txt)

## 链接

- [YomkServer 官方仓库](https://github.com/Solitude-5309/YomkServer)
- [YomkServer 文档](https://github.com/Solitude-5309/YomkServer#readme)
