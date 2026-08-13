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
| `init(argc, argv, nodeName)` | 初始化 | 命令行参数、节点名称、启动后台阻塞 spin 线程 |
| `registerPubTopic<T>(topic, queueSize)` | 注册发布主题 | 主题名、缓存队列长度 |
| `registerSubTopic<T>(topic, queueSize, cb)` | 注册订阅主题 | 主题名、队列长度、回调 |
| `publish<T>(topic, data)` | 发布数据 | 主题名、数据包 |
| `shutdown()` | 显式销毁 | 销毁节点与全部实体，退出前建议调用 |

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

脚本编译安装主库后自动编译并运行 `TestYomkRos2`，并设置好 LD_LIBRARY_PATH/PATH 环境变量。

## 工程结构

```
YomkROS2/
├── include/
│   └── ROS2Node.h           # ROS2 通信封装类（模板方法）
├── src/
│   └── ROS2Node.cpp         # ROS2Node 非模板方法实现
├── test/
│   ├── CMakeLists.txt       # 测试程序构建
│   └── TestYomkRos2.cpp     # ROS2Node 端到端测试
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

    // 4. 发布数据
    std_msgs::msg::String msg;
    msg.data = "hello";
    node.publish<std_msgs::msg::String>("chatter", msg);

    // 5. 退出前显式销毁
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

## 开发状态

- [x] 基础框架搭建完成
- [x] CMake 构建系统集成
- [x] ROS2 集成（ROS2Node 类：发布/订阅/节点管理）

## License

MIT License - 详见 [LICENSE.txt](../../LICENSE.txt)

## 链接

- [YomkServer 官方仓库](https://github.com/Solitude-5309/YomkServer)
- [YomkExtensions 扩展集合](https://github.com/Solitude-5309/YomkExtensions)
