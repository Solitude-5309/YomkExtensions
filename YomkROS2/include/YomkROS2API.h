#pragma once

#include <YomkROS2/ROS2Node.h>

namespace yomk
{
    // 全局唯一 ROS2Node 实例（懒加载单例），供 YOMKROS2_* 宏共享
    inline ROS2Node &yomkROS2Node()
    {
        static ROS2Node node;
        return node;
    }
} // namespace yomk

// 初始化节点（仅 init，不启动运行）；透传 argc/argv，
// 与原生 rclcpp::init 语义一致（支持命令行重映射等参数）；重复调用返回 false
#define YOMKROS2_NODE(argc, argv, nodeName) \
    yomk::yomkROS2Node().init(argc, argv, nodeName)

// 运行节点：blocking=true 阻塞当前线程直至 YOMKROS2_SHUTDOWN（典型 main 用法）；
// blocking=false 后台线程 spin 立即返回；未初始化或已运行时返回 false
#define YOMKROS2_RUN(blocking) yomk::yomkROS2Node().run(blocking)

// 注册发布主题：type 为 rosidl 消息类型
#define YOMKROS2_PUB_TOPIC(type, topicName, queueSize) \
    yomk::yomkROS2Node().registerPubTopic<type>(topicName, queueSize)

// 注册订阅主题：callback 体内若含顶层逗号（如模板参数），
// 请先将回调定义为变量再传入，或对整个回调外加一层括号
#define YOMKROS2_SUB_TOPIC(type, topicName, queueSize, callback) \
    yomk::yomkROS2Node().registerSubTopic<type>(topicName, queueSize, callback)

// 发送消息：类型由 data 自动推导
#define YOMKROS2_PUB_MSG(topicName, data) \
    yomk::yomkROS2Node().publish(topicName, data)

// 显式销毁节点与全部实体（退出前建议调用；不调用时析构兜底）
#define YOMKROS2_SHUTDOWN() yomk::yomkROS2Node().shutdown()
