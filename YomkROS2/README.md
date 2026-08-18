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
| `createParamClient(remoteNode)` | 预创建远程参数客户端 | 只创建并缓存 AsyncParametersClient 实体，不检查远端可用性、不发送请求；推荐在 run 前调用（跨节点必须） |
| `getRemoteParam<T>(remoteNode, name, out)` | 查询远程参数 | 需已 createParamClient 预创建客户端；future 同步等待响应；需节点已 run 运行 |
| `setRemoteParam<T>(remoteNode, name, value)` | 设置远程参数 | 需已 createParamClient 预创建客户端；future 同步等待响应；需节点已 run 运行 |
| `hasRemoteParam(remoteNode, name)` | 远程参数是否存在 | 需已 createParamClient 预创建客户端；future 同步等待响应；需节点已 run 运行 |
| `listRemoteParams(remoteNode)` | 列出远程参数名 | 需已 createParamClient 预创建客户端；future 同步等待响应；需节点已 run 运行 |
| `createService<T>(service, cb)` | 注册服务端 | 服务名、回调；服务重名返回 false |
| `createServiceClient<T>(service)` | 预创建服务客户端 | 只创建并缓存客户端实体，不检查服务可用性、不发送请求；推荐在 run 前调用 |
| `callService<T>(service, request)` | 同步调用服务 | 自动创建客户端 + 等待服务 + 发请求 + future 同步等待响应（一次调用完成）；需节点已 run 运行；失败返回 nullptr |
| `callServiceAsync<T>(service, request, cb)` | 异步调用服务 | 立即返回，响应就绪时回调收到 `Response::SharedPtr`；发送失败返回 false |
| `createAction<T>(action, goalCb, cancelCb, execCb)` | 注册动作服务端 | 三回调一一对应原生 handle_goal/handle_cancel/handle_accepted；动作重名返回 false |
| `createActionClient<T>(action)` | 预创建动作客户端 | 只创建并缓存客户端实体，不检查服务端可用性、不发送请求；推荐在 run 前调用 |
| `callActionAsync<T>(action, goal, respCb, fbCb, resultCb)` | 异步发送动作目标 | 立即返回，成功返回非零 goalId（三个回调均携带，供取消），失败返回 0；支持同 action 多 goal 并发；respCb 必触发一次告知接受/拒绝，resultCb 必触发且仅一次（被拒绝时合成 code=UNKNOWN） |
| `cancelGoal<T>(action, goalId)` | 取消指定 goal | goalId 由 callActionAsync 返回；服务端可拒绝取消；取消成功后 resultCb 收到 CANCELED |
| `cancelAllGoals<T>(action)` | 取消全部活跃 goal | 对应原生 async_cancel_all_goals；无活跃 goal 返回 false |
| `cancelGoalsBefore<T>(action, stamp)` | 按时间戳取消 | 取消目标时间戳早于 stamp 的全部 goal（rclcpp::Time） |
| `shutdown()` | 显式销毁 | 销毁节点与全部实体，退出前建议调用 |

> 参数与服务接口失败时内部输出 `RCLCPP_ERROR` 日志（含接口名、服务名/参数名与失败原因），成功无输出，便于排查错误。
>
> 服务端回调与订阅回调同规则：每个服务端分配独立 MutuallyExclusive 回调组——同服务请求串行保序，跨服务/跨 topic 可并发；回调内阻塞只影响本服务的后续请求，不会阻塞其他服务或 topic 回调（受 executor 线程数限制）。客户端异步回调同样在独立回调组执行。回调须自行保证共享数据的线程安全。
>
> ⚠️ **服务客户端推荐在 run 之前预创建**（创建节点 → 创建客户端 → run → 发送请求）：跨节点调用时，spin 期间动态创建的客户端可能收不到响应；run 前预创建的客户端在 spin 启动时实体已就绪，响应接收可靠。动作客户端同理（推荐顺序：创建节点 → 注册服务端 + 预创建客户端 → run → callActionAsync）。
>
> **动作接口回调语义**（完整保留原生 rclcpp_action 能力）：
> - `goalCallback(goal)` 返回 int 对应原生 GoalResponse 三值：1=拒绝（REJECT）/ 2=接受并立即执行（ACCEPT_AND_EXECUTE，随后自动调 executeCallback）/ 3=接受但延迟执行（ACCEPT_AND_DEFER，用户后续自行调用 `goalHandle->execute()` 触发）
> - `cancelCallback(goalHandle)` 返回 true 允许取消 / false 拒绝取消（危险任务可拒绝，用户决定）
> - `executeCallback(goalHandle)` 在 executor 线程被调用，内部不开线程，用户应自行开线程执行长耗时任务；执行线程中使用原生句柄：`publish_feedback(fb)` / `is_canceling()` / `succeed(result)` / `abort(result)` / `canceled(result)`；用户执行线程的退出时机由用户保证（shutdown 前须退出）
> - 客户端回调对外屏蔽 ClientGoalHandle，以 goalId 为统一身份：`goalResponseCallback(goalId, accepted)` 必触发一次且先于其余回调（告知目标被接受/拒绝）；`feedbackCallback(goalId, feedback)` 收到各步反馈；`resultCallback(goalId, code, result)` 中 code 为 SUCCEEDED / ABORTED / CANCELED / UNKNOWN（goal 被拒绝时为 UNKNOWN 且 result 为空）

### 宏 API（YomkROS2API.h）

基于全局单例 ROS2Node 的宏封装，与原生 ROS2 节点语义一致，仅是调用更简单（`#include <YomkROS2/YomkROS2API.h>`）：

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
| `YOMKROS2_PARAM_CLIENT(remoteNode)` | 预创建远程参数客户端 | 只创建并缓存客户端实体，不发送请求；推荐在 YOMKROS2_RUN 之前调用（跨节点必须） |
| `YOMKROS2_GET_REMOTE_PARAM(remoteNode, name, out)` | 查询远程参数 | 需已 PARAM_CLIENT 预创建客户端；future 同步等待；需节点已 run |
| `YOMKROS2_SET_REMOTE_PARAM(remoteNode, name, value)` | 设置远程参数 | 需已 PARAM_CLIENT 预创建客户端；future 同步等待；需节点已 run |
| `YOMKROS2_HAS_REMOTE_PARAM(remoteNode, name)` | 远程参数是否存在 | 需已 PARAM_CLIENT 预创建客户端；future 同步等待；需节点已 run |
| `YOMKROS2_LIST_REMOTE_PARAMS(remoteNode)` | 列出远程参数名 | 需已 PARAM_CLIENT 预创建客户端；future 同步等待；需节点已 run |
| `YOMKROS2_SERVICE(type, service, cb)` | 注册服务端 | 回调体内若含顶层逗号，请先将回调定义为变量再传入 |
| `YOMKROS2_SERVICE_CLIENT(type, service)` | 预创建服务客户端 | 只创建并缓存客户端实体，不发送请求；推荐在 YOMKROS2_RUN 之前调用（跨节点必须） |
| `YOMKROS2_CALL_SERVICE(type, service, request)` | 同步调用服务 | 返回响应 SharedPtr，失败返回 nullptr（自动建客户端，一次调用完成）；需节点已 run |
| `YOMKROS2_CALL_SERVICE_ASYNC(type, service, request, cb)` | 异步调用服务 | 立即返回，响应就绪时回调收到 Response::SharedPtr |
| `YOMKROS2_ACTION(type, action, goalCb, cancelCb, execCb)` | 注册动作服务端 | 三回调对应原生 goal/cancel/accepted；回调体内含顶层逗号时先定义为变量再传入 |
| `YOMKROS2_ACTION_CLIENT(type, action)` | 预创建动作客户端 | 只创建并缓存客户端实体，不发送请求；推荐在 YOMKROS2_RUN 之前调用 |
| `YOMKROS2_CALL_ACTION_ASYNC(type, action, goal, respCb, fbCb, resultCb)` | 异步发送动作目标 | 返回非零 goalId（失败返回 0）；三回调均携带 goalId（对外屏蔽 ClientGoalHandle）；respCb 告知接受/拒绝 |
| `YOMKROS2_CANCEL_GOAL(type, action, goalId)` | 取消指定 goal | goalId 由 CALL_ACTION_ASYNC 返回 |
| `YOMKROS2_CANCEL_ALL_GOALS(type, action)` | 取消全部活跃 goal | 无活跃 goal 返回 false |
| `YOMKROS2_CANCEL_GOALS_BEFORE(type, action, stamp)` | 按时间戳取消 | 取消目标时间戳早于 stamp（rclcpp::Time）的全部 goal |
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

脚本编译安装主库后自动编译并依次运行 `TestYomkROS2Topic`（主题宏测试，阻塞模式，脚本以 SIGINT 模拟 Ctrl+C 唤醒）、`TestYomkROS2Param`（参数接口宏测试，非阻塞用例）、`TestYomkROS2Service`（服务通信宏测试，非阻塞用例）与 `TestYomkROS2Action`（动作通信宏测试，非阻塞用例），最后运行跨进程集成测试（`TestYomkROS2Exec` 服务端角色后台启动 + `TestYomkROS2Control` 客户端角色前台运行，两个独立进程演示真实用户用法），并设置好 LD_LIBRARY_PATH/PATH 环境变量。

## 工程结构

```
YomkROS2/
├── include/
│   ├── ROS2Node.h           # ROS2 通信封装类（模板方法）
│   └── YomkROS2API.h        # 宏封装 API（全局单例简化调用）
├── src/
│   └── ROS2Node.cpp         # ROS2Node 非模板方法实现
├── test/
│   ├── CMakeLists.txt                # 测试程序构建
│   ├── TestYomkROS2Topic.cpp         # 主题宏测试（阻塞模式，Ctrl+C 退出）
│   ├── TestYomkROS2Param.cpp         # 参数接口宏 API 测试（本地 + 远程参数）
│   ├── TestYomkROS2Service.cpp       # 服务通信宏 API 测试（服务端注册 + 同步/异步调用）
│   ├── TestYomkROS2Action.cpp        # 动作通信宏 API 测试（服务端三回调 + 异步调用 + 取消）
│   ├── TestYomkROS2Control.cpp       # 双进程参考程序：control_node 客户端角色（发主题/设参/调服务/发目标）
│   └── TestYomkROS2Exec.cpp          # 双进程参考程序：exec_node 服务端角色（收主题/被设参/执行服务/执行动作）
├── cmake/
│   └── ProjectConfig.cmake.in  # CMake 导出配置模板
├── CMakeLists.txt              # CMake 构建配置
├── build.sh                    # 一键编译脚本
└── README.md
```

## 使用示例

以下示例统一使用 `YOMKROS2_*` 宏 API（全局单例节点，定义于 `YomkROS2API.h`）；上方 ROS2Node 类接口表仅供需要多节点/自定义生命周期等精细控制时参考。

### 基本用法（主题收发，支持任意 rosidl 类型）

```cpp
#include <YomkROS2/YomkROS2API.h>
#include <std_msgs/msg/string.hpp>

int main(int argc, char *argv[])
{
    // 1. 初始化节点（仅 init，不启动运行；进程级 rclcpp::init 由封装自动处理）
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

    // 4. 阻塞运行直至 shutdown（典型 main 用法，防止程序退出）；
    //    也可 YOMKROS2_RUN(false) 后台线程运行立即返回
    YOMKROS2_RUN(true);

    // 5. 退出前显式销毁（run 返回后执行）
    YOMKROS2_SHUTDOWN();
    return 0;
}
```

> 阻塞模式下 publish 需在回调或其他线程中进行；shutdown 可由信号处理或辅助线程触发。

### 自定义数据类型

宏的 type 参数可传任意 rosidl 类型（含用户自定义类型），封装模板在调用者编译单元实例化：

```cpp
#include <YomkROS2/YomkROS2API.h>
#include <my_pkg/msg/my_custom_msg.hpp>  // 用户自定义 rosidl 类型
using MyCustomMsg = my_pkg::msg::MyCustomMsg;

int main(int argc, char *argv[])
{
    YOMKROS2_NODE(argc, argv, "custom_node");
    YOMKROS2_PUB_TOPIC(MyCustomMsg, "custom_topic", 10);
    YOMKROS2_RUN(false);

    MyCustomMsg msg;                        // 直接传入自定义类型
    YOMKROS2_PUB_MSG("custom_topic", msg);

    YOMKROS2_SHUTDOWN();
    return 0;
}
```

### 参数用法（一次调用完成）

```cpp
#include <YomkROS2/YomkROS2API.h>

int main(int argc, char *argv[])
{
    YOMKROS2_NODE(argc, argv, "my_node");

    // 1. 声明参数并立即取到值（首次返回默认值，已存在返回现有值，幂等）
    int64_t speed = YOMKROS2_DECLARE_PARAM("speed", int64_t(30));

    // 2. 查询/设置参数
    int64_t v = 0;
    YOMKROS2_GET_PARAM("speed", v);
    YOMKROS2_SET_PARAM("speed", int64_t(60));

    // 3. 远程参数：run 前预创建客户端，future 同步等待响应（一次调用完成）
    //    （remote_node 为另一节点名；响应由本节点 spin 处理，需已 run）
    YOMKROS2_PARAM_CLIENT("remote_node");
    YOMKROS2_RUN(false);
    int64_t rv = 0;
    YOMKROS2_GET_REMOTE_PARAM("remote_node", "speed", rv);
    YOMKROS2_SET_REMOTE_PARAM("remote_node", "speed", int64_t(80));
    YOMKROS2_HAS_REMOTE_PARAM("remote_node", "speed");
    YOMKROS2_LIST_REMOTE_PARAMS("remote_node");

    YOMKROS2_SHUTDOWN();
    return 0;
}
```

> 参数接口失败时输出 `RCLCPP_ERROR` 日志，成功无输出；set 拦截回调（addOnSetParamCallback）无宏封装，需要时使用 ROS2Node 类接口。

### 服务通信用法（一次调用完成）

```cpp
#include <YomkROS2/YomkROS2API.h>
#include <example_interfaces/srv/add_two_ints.hpp>

int main(int argc, char *argv[])
{
    // 1. 创建节点
    YOMKROS2_NODE(argc, argv, "my_node");

    // 2. 注册服务端（回调内填充 response；服务端回调在独立回调组执行，
    //    回调内阻塞只影响本服务的后续请求，不阻塞 topic 与其他服务回调）
    auto addCb = [](const std::shared_ptr<example_interfaces::srv::AddTwoInts::Request> request,
                    std::shared_ptr<example_interfaces::srv::AddTwoInts::Response> response)
    {
        response->sum = request->a + request->b;
    };
    YOMKROS2_SERVICE(example_interfaces::srv::AddTwoInts, "add_service", addCb);

    // 3. run 前预创建客户端（只创建实体不发送请求；跨节点调用必须如此，
    //    spin 期间动态创建的客户端可能收不到响应）
    YOMKROS2_SERVICE_CLIENT(example_interfaces::srv::AddTwoInts, "add_service");

    // 4. 运行节点（客户端 future 的响应依赖本地 spin，需已 run）
    YOMKROS2_RUN(false);

    // 5. 同步调用服务：等待服务 + 发请求 + 同步等待响应（复用预创建的客户端）
    example_interfaces::srv::AddTwoInts::Request req;
    req.a = 1;
    req.b = 2;
    if (auto resp = YOMKROS2_CALL_SERVICE(example_interfaces::srv::AddTwoInts, "add_service", req))
    {
        // resp->sum == 3
    }

    // 6. 异步调用服务：立即返回，响应就绪时回调收到 Response::SharedPtr
    auto asyncCb = [](example_interfaces::srv::AddTwoInts::Response::SharedPtr resp)
    {
        // resp->sum
    };
    req.a = 5;
    req.b = 6;
    YOMKROS2_CALL_SERVICE_ASYNC(example_interfaces::srv::AddTwoInts, "add_service", req, asyncCb);

    YOMKROS2_SHUTDOWN();
    return 0;
}
```

> 类用法对应 `createService` / `createServiceClient` / `callService` / `callServiceAsync`。服务接口失败时输出 `RCLCPP_ERROR` 日志（含接口名、服务名与失败原因），成功无输出；`callService` 失败返回 nullptr，`callServiceAsync` 发送失败返回 false（此时回调不会被调用）。

### 动作通信用法（完整保留原生能力）

```cpp
#include <YomkROS2/YomkROS2API.h>
#include <example_interfaces/action/fibonacci.hpp>
using Fibonacci = example_interfaces::action::Fibonacci;
using GoalHandle = rclcpp_action::ServerGoalHandle<Fibonacci>;

int main(int argc, char *argv[])
{
    // 1. 创建节点
    YOMKROS2_NODE(argc, argv, "my_node");

    // 2. 注册动作服务端（三回调一一对应原生 handle_goal/handle_cancel/handle_accepted）
    auto goalCb = [](std::shared_ptr<const Fibonacci::Goal> goal)
    {
        return goal->order < 0 ? 1 : 2; // 1=拒绝 2=接受并立即执行 3=接受但延迟执行
    };
    auto cancelCb = [](std::shared_ptr<GoalHandle>)
    {
        return true; // true 允许取消 / false 拒绝取消（危险任务可拒绝）
    };
    auto execCb = [](std::shared_ptr<GoalHandle> goalHandle)
    {
        // executor 线程触发，用户自行开线程执行长耗时任务
        std::thread([goalHandle]()
        {
            auto result = std::make_shared<Fibonacci::Result>();
            auto feedback = std::make_shared<Fibonacci::Feedback>();
            // ... 逐步计算并 goalHandle->publish_feedback(feedback)；
            //     循环中检查 goalHandle->is_canceling()，为 true 时
            //     goalHandle->canceled(result) 提前退出
            goalHandle->succeed(result); // 或 abort(result)
        }).detach();
    };
    YOMKROS2_ACTION(Fibonacci, "fibonacci", goalCb, cancelCb, execCb);

    // 3. run 前预创建客户端（同服务客户端，跨节点必须）
    YOMKROS2_ACTION_CLIENT(Fibonacci, "fibonacci");

    // 4. 运行节点
    YOMKROS2_RUN(false);

    // 5. 异步发送目标：返回 goalId；三个回调均携带 goalId（对外屏蔽 ClientGoalHandle）
    Fibonacci::Goal goal;
    goal.order = 10;
    auto respCb = [](uint64_t goalId, bool accepted)
    {
        // accepted：目标被服务端接受/拒绝（必触发一次，先于其余回调）
    };
    auto fbCb = [](uint64_t goalId, const std::shared_ptr<const Fibonacci::Feedback> fb)
    {
        // fb->sequence 各步进度
    };
    auto resultCb = [](uint64_t goalId, rclcpp_action::ResultCode code,
                       const std::shared_ptr<const Fibonacci::Result> result)
    {
        // code：SUCCEEDED / ABORTED / CANCELED / UNKNOWN（goal 被拒绝，此时 result 为空）
    };
    uint64_t goalId = YOMKROS2_CALL_ACTION_ASYNC(Fibonacci, "fibonacci", goal, respCb, fbCb, resultCb);

    // 6. 取消：指定 goal / 全部活跃 goal / 时间戳早于 stamp 的 goal
    YOMKROS2_CANCEL_GOAL(Fibonacci, "fibonacci", goalId);
    YOMKROS2_CANCEL_ALL_GOALS(Fibonacci, "fibonacci");
    YOMKROS2_CANCEL_GOALS_BEFORE(Fibonacci, "fibonacci", rclcpp::Clock().now());

    YOMKROS2_SHUTDOWN();
    return 0;
}
```

> 类用法对应 `createAction` / `createActionClient` / `callActionAsync` / `cancelGoal` / `cancelAllGoals` / `cancelGoalsBefore`。动作接口失败时输出 `RCLCPP_ERROR` 日志，成功无输出；每次 `callActionAsync` 的 goalResponseCallback 必触发一次（告知接受/拒绝），resultCallback 必触发且仅一次（goal 被拒绝时封装合成 code=UNKNOWN）。服务端执行线程由用户自开自管，shutdown 前须确保已退出。

## 开发状态

- ✅ 基础框架搭建完成
- ✅ CMake 构建系统集成
- ✅ ROS2 集成（发布/订阅/节点管理，ROS2Node 类 + 宏 API）
- ✅ 参数接口（本地参数管理 + 远程参数客户端预创建）
- ✅ 服务通信接口（服务端注册 + 客户端同步/异步调用）
- ✅ 动作通信接口（服务端三回调 + 客户端异步调用 + 取消，完整保留原生 rclcpp_action 能力）
- ✅ 双进程参考程序（TestYomkROS2Control/TestYomkROS2Exec，四类通信跨进程演示）

## License

MIT License - 详见 [LICENSE.txt](../../LICENSE.txt)

## 链接

- [YomkServer 官方仓库](https://github.com/Solitude-5309/YomkServer)
- [YomkExtensions 扩展集合](https://github.com/Solitude-5309/YomkExtensions)
