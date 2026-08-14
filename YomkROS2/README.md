# YomkROS2 扩展

基于 [YomkServer](https://github.com/Solitude-5309/YomkServer) 框架的 ROS2 通信扩展。

## 功能

基于 YomkServer 框架的 ROS2 扩展，提供 ROS2 通信封装类 ROS2Node。

### ROS2Node 类

纯 C++ 封装类（非模板类 + 模板方法），单节点设计，发布与订阅共用同一节点。支持任意 rosidl 消息类型（含用户自定义类型）。

订阅回调由 `MultiThreadedExecutor` 线程池驱动，每个订阅持有独立的回调组：**跨主题回调可并发**（慢回调不会阻塞其他主题），**同主题回调串行保序**。

> ⚠️ **线程安全提示**：不同主题的回调可能在不同线程并发执行，用户回调须自行保证共享数据的线程安全（如加锁）；同主题回调也可能被调度到不同线程，请勿在回调内依赖 `thread_local` 状态。

| 接口 | 功能 | 说明 |
|------|------|------|
| `init(argc, argv, nodeName)` | 初始化 | 命令行参数、节点名称；仅初始化，不启动运行 |
| `run(blocking = false)` | 运行节点 | `false`：后台线程运行 spin，立即返回；`true`：当前线程阻塞运行，直至 `shutdown()` |
| `registerPubTopic<T>(topic, queueSize)` | 注册发布主题 | 主题名、缓存队列长度 |
| `registerSubTopic<T>(topic, queueSize, cb)` | 注册订阅主题 | 主题名、队列长度、回调 |
| `publish<T>(topic, data)` | 发布数据 | 主题名、数据包 |
| `declareParam<T>(name, defaultValue)` | 声明参数并取到值 | 首次声明返回默认值，已存在返回现有值（幂等，一次调用即取到值） |
| `getParam<T>(name, out)` | 查询参数 | 不存在或类型不匹配返回 false |
| `setParam<T>(name, value)` | 设置参数 | 未声明、类型不匹配或被回调拒绝返回 false |
| `hasParam(name)` | 参数是否存在 | — |
| `undeclareParam(name)` | 删除参数 | 带默认值声明的参数为静态类型（rclcpp 语义）不可撤销，返回 false |
| `listParams()` | 列出全部参数名 | — |
| `addOnSetParamCallback(cb)` | 注册设置回调 | 回调返回 false 拒绝设置；返回句柄 id 供移除 |
| `removeOnSetParamCallback(id)` | 移除设置回调 | — |
| `getRemoteParam<T>(remoteNode, name, out)` | 查询远程参数 | 自动创建异步客户端，future 同步等待响应；需节点已 run 运行 |
| `setRemoteParam<T>(remoteNode, name, value)` | 设置远程参数 | 自动创建异步客户端，future 同步等待响应；需节点已 run 运行 |
| `hasRemoteParam(remoteNode, name)` | 远程参数是否存在 | 自动创建异步客户端，future 同步等待响应；需节点已 run 运行 |
| `listRemoteParams(remoteNode)` | 列出远程参数名 | 自动创建异步客户端，future 同步等待响应；需节点已 run 运行 |
| `shutdown()` | 显式销毁 | 销毁节点与全部实体，退出前建议调用 |

> 参数接口失败时内部输出 `RCLCPP_ERROR` 日志（含接口名、参数名与失败原因），成功无输出，便于排查错误。

### 宏 API（YomkROSAPI.h）

基于全局单例 ROS2Node 的宏封装，与原生 ROS2 节点语义一致，仅是调用更简单（`#include <YomkROS2/YomkROSAPI.h>`）：

| 宏 | 功能 | 说明 |
|------|------|------|
| `YOMKROS2_NODE(argc, argv, nodeName)` | 初始化节点 | 透传 argc/argv，支持命令行重映射；仅初始化，不启动运行 |
| `YOMKROS2_RUN(blocking)` | 运行节点 | `true`：阻塞当前线程直至 shutdown（典型 main 用法）；`false`：后台线程运行 |
| `YOMKROS2_PUB_TOPIC(type, topic, queueSize)` | 注册发布主题 | 显式传 rosidl 消息类型 |
| `YOMKROS2_SUB_TOPIC(type, topic, queueSize, cb)` | 注册订阅主题 | 回调体内若含顶层逗号，请先将回调定义为变量再传入 |
| `YOMKROS2_PUB_MSG(topic, data)` | 发送消息 | 消息类型由 data 自动推导 |
| `YOMKROS2_DECLARE_PARAM(name, defaultValue)` | 声明参数并取到值 | 首次声明返回默认值，已存在返回现有值（幂等） |
| `YOMKROS2_GET_PARAM(name, out)` | 查询参数 | 不存在或类型不匹配返回 false |
| `YOMKROS2_SET_PARAM(name, value)` | 设置参数 | 未声明、类型不匹配或被回调拒绝返回 false |
| `YOMKROS2_HAS_PARAM(name)` | 参数是否存在 | — |
| `YOMKROS2_UNDECLARE_PARAM(name)` | 删除参数 | 带默认值声明的参数为静态类型不可撤销，返回 false |
| `YOMKROS2_LIST_PARAMS()` | 列出全部参数名 | — |
| `YOMKROS2_GET_REMOTE_PARAM(remoteNode, name, out)` | 查询远程参数 | 自动创建异步客户端，future 同步等待；需节点已 run |
| `YOMKROS2_SET_REMOTE_PARAM(remoteNode, name, value)` | 设置远程参数 | 自动创建异步客户端，future 同步等待；需节点已 run |
| `YOMKROS2_HAS_REMOTE_PARAM(remoteNode, name)` | 远程参数是否存在 | 自动创建异步客户端，future 同步等待；需节点已 run |
| `YOMKROS2_LIST_REMOTE_PARAMS(remoteNode)` | 列出远程参数名 | 自动创建异步客户端，future 同步等待；需节点已 run |
| `YOMKROS2_SHUTDOWN()` | 显式销毁 | 退出前建议调用；不调用时析构兜底 |

## 前置条件

- C++17 编译器
- CMake >= 3.14
- YomkServer 已安装
- ROS2 Humble 已安装（默认 `/opt/ros/humble`）

## 编译

```bash
# 默认安装到 YomkROS2/install/
source build.sh -DCMAKE_PREFIX_PATH=~/YomkServer/install

# 自定义安装目录
source build.sh -DCMAKE_PREFIX_PATH=~/YomkServer/install -DCMAKE_INSTALL_PREFIX=~/YomkServer/install
```

脚本编译安装主库后自动编译并依次运行 `TestYomkROS2NonBlocking`（非阻塞）、`TestYomkROS2Blocking`（阻塞）与 `TestYomkROS2API`（宏 API 阻塞模式，脚本以 SIGINT 模拟 Ctrl+C 唤醒）、`TestYomkROS2Param`（参数接口非阻塞用例），并设置好 LD_LIBRARY_PATH/PATH 环境变量。

## 工程结构

```
YomkROS2/
├── include/
│   ├── ROS2Node.h           # ROS2 通信封装类（模板方法）
│   └── YomkROSAPI.h         # 宏封装 API（全局单例简化调用）
├── src/
│   └── ROS2Node.cpp         # ROS2Node 非模板方法实现
├── test/
│   ├── CMakeLists.txt                # 测试程序构建
│   ├── TestYomkROS2NonBlocking.cpp   # 非阻塞 run(false) 端到端测试
│   ├── TestYomkROS2Blocking.cpp      # 阻塞 run(true) 端到端测试
│   ├── TestYomkROS2API.cpp           # 宏 API 阻塞模式测试（永久阻塞，Ctrl+C 退出）
│   └── TestYomkROS2Param.cpp         # 参数接口宏 API 测试（本地 + 远程参数）
├── cmake/
│   └── ProjectConfig.cmake.in  # CMake 导出配置模板
├── CMakeLists.txt              # CMake 构建配置
├── build.sh                    # 一键编译脚本
└── README.md
```

## 使用示例

### 基本用法（支持任意 rosidl 类型）

```cpp
#include <YomkROS2/ROS2Node.h>
#include <std_msgs/msg/string.hpp>

int main(int argc, char *argv[])
{
    yomk::ROS2Node node;

    // 1. 初始化（进程级 rclcpp::init 由 ROS2Node 自动处理）
    node.init(argc, argv, "my_node");

    // 2. 注册订阅主题
    node.registerSubTopic<std_msgs::msg::String>("chatter", 10,
        [](std::shared_ptr<const std_msgs::msg::String> msg)
        {
            // 收到消息 msg->data
        });

    // 3. 注册发布主题
    node.registerPubTopic<std_msgs::msg::String>("chatter", 10);

    // 4. 运行节点：run(false) 后台线程运行立即返回；
    //    run(true) 阻塞当前线程直至 shutdown()（适合主线程直接运行）
    node.run(false);

    // 5. 发布数据
    std_msgs::msg::String msg;
    msg.data = "hello";
    node.publish<std_msgs::msg::String>("chatter", msg);

    // 6. 退出前显式销毁
    node.shutdown();
    return 0;
}
```

### 自定义数据类型

模板方法内联在 `ROS2Node.h`，用户工程 include 后可用任意 rosidl 自定义类型实例化：

```cpp
#include <YomkROS2/ROS2Node.h>
#include <my_pkg/msg/my_custom_msg.hpp>  // 用户自定义 rosidl 类型

yomk::ROS2Node node;
node.init(argc, argv, "custom_node");
node.registerPubTopic<MyCustomMsg>("custom_topic", 10);
MyCustomMsg msg;                          // 直接传入自定义类型
node.publish<MyCustomMsg>("custom_topic", msg);
```

### 宏 API 用法

```cpp
#include <YomkROS2/YomkROSAPI.h>
#include <std_msgs/msg/string.hpp>

int main(int argc, char *argv[])
{
    // 1. 初始化节点（仅 init，不启动运行）
    YOMKROS2_NODE(argc, argv, "my_node");

    // 2. 注册订阅主题（回调体内若含顶层逗号，先定义为变量再传入）
    YOMKROS2_SUB_TOPIC(std_msgs::msg::String, "chatter", 10,
        [](std::shared_ptr<const std_msgs::msg::String> msg)
        {
            // 收到消息 msg->data
        });

    // 3. 注册发布主题并发送消息（可在其他线程/回调中发布）
    YOMKROS2_PUB_TOPIC(std_msgs::msg::String, "chatter", 10);
    std_msgs::msg::String msg;
    msg.data = "hello";
    YOMKROS2_PUB_MSG("chatter", msg);

    // 4. 阻塞运行直至 shutdown（典型 main 用法，防止程序退出）
    YOMKROS2_RUN(true);

    // 5. 退出前显式销毁（run 返回后执行）
    YOMKROS2_SHUTDOWN();
    return 0;
}
```

> 阻塞模式下 publish 需在回调或其他线程中进行；shutdown 可由信号处理或辅助线程触发。精细控制（多节点、自定义生命周期）请使用 ROS2Node 类。

### 参数用法（一次调用完成）

```cpp
// 1. 声明参数并立即取到值（首次返回默认值，已存在返回现有值，幂等）
int64_t speed = node.declareParam<int64_t>("speed", 30);

// 2. 查询/设置参数
int64_t v = 0;
node.getParam<int64_t>("speed", v);
node.setParam<int64_t>("speed", 60);

// 3. 设置拦截：回调返回 false 拒绝本次设置
node.addOnSetParamCallback([](const std::vector<rclcpp::Parameter> &)
{
    return false; // 拒绝一切设置
});

// 4. 远程参数：自动创建异步客户端，future 同步等待响应（一次调用完成）
//    （remote_node 为另一 ROS2Node 实例的节点名；响应由本节点 spin 处理，需已 run）
node.run(false);
int64_t rv = 0;
node.getRemoteParam<int64_t>("remote_node", "speed", rv);
node.setRemoteParam<int64_t>("remote_node", "speed", 80);
node.hasRemoteParam("remote_node", "speed");
node.listRemoteParams("remote_node");
```

> 宏用法对应 `YOMKROS2_DECLARE_PARAM` / `YOMKROS2_GET_PARAM` / `YOMKROS2_SET_REMOTE_PARAM` 等（见宏表）。参数接口失败时输出 `RCLCPP_ERROR` 日志，成功无输出。

## 开发状态

- [x] 基础框架搭建完成
- [x] CMake 构建系统集成
- [x] ROS2 集成（ROS2Node 类：发布/订阅/节点管理）
- [x] 参数接口（ROS2Node 类：本地参数管理 + 远程参数客户端）

## License

MIT License - 详见 [LICENSE.txt](../../LICENSE.txt)

## 链接

- [YomkServer 官方仓库](https://github.com/Solitude-5309/YomkServer)
- [YomkExtensions 扩展集合](https://github.com/Solitude-5309/YomkExtensions)
