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

// ===== 参数接口（失败时接口内部输出 RCLCPP_ERROR 日志，成功无输出）=====
// defaultValue 若含顶层逗号（如 std::vector<int64_t>{1, 2}），
// 请先将值定义为变量再传入

// 声明参数并取到值：首次声明返回 defaultValue，已存在返回现有值（幂等）
#define YOMKROS2_DECLARE_PARAM(name, defaultValue) \
    yomk::yomkROS2Node().declareParam(name, defaultValue)

// 查询参数：不存在或类型不匹配返回 false
#define YOMKROS2_GET_PARAM(name, out) yomk::yomkROS2Node().getParam(name, out)

// 设置参数：未声明、类型不匹配或被 on-set 回调拒绝返回 false
#define YOMKROS2_SET_PARAM(name, value) yomk::yomkROS2Node().setParam(name, value)

// 参数是否存在
#define YOMKROS2_HAS_PARAM(name) yomk::yomkROS2Node().hasParam(name)

// 删除参数（撤销声明）
#define YOMKROS2_UNDECLARE_PARAM(name) yomk::yomkROS2Node().undeclareParam(name)

// 列出全部参数名
#define YOMKROS2_LIST_PARAMS() yomk::yomkROS2Node().listParams()

// 预创建远程参数客户端：只创建并缓存 AsyncParametersClient 实体，不检查远端
// 可用性、不发送请求；【推荐用法】在 YOMKROS2_RUN 之前预创建（创建节点 →
// 创建客户端 → run → 远程参数接口发送请求），跨节点调用必须如此
#define YOMKROS2_PARAM_CLIENT(remoteNode) \
    yomk::yomkROS2Node().createParamClient(remoteNode)

// 远程参数（调用前需已 YOMKROS2_PARAM_CLIENT 预创建对应远程节点的客户端，
// 且节点已 run 运行；future 同步等待响应，一次调用完成）
#define YOMKROS2_GET_REMOTE_PARAM(remoteNode, name, out) \
    yomk::yomkROS2Node().getRemoteParam(remoteNode, name, out)

#define YOMKROS2_SET_REMOTE_PARAM(remoteNode, name, value) \
    yomk::yomkROS2Node().setRemoteParam(remoteNode, name, value)

#define YOMKROS2_HAS_REMOTE_PARAM(remoteNode, name) \
    yomk::yomkROS2Node().hasRemoteParam(remoteNode, name)

#define YOMKROS2_LIST_REMOTE_PARAMS(remoteNode) \
    yomk::yomkROS2Node().listRemoteParams(remoteNode)

// ===== 服务通信接口（失败时接口内部输出 RCLCPP_ERROR 日志，成功无输出）=====
// callback 体内若含顶层逗号（如模板参数），请先将回调定义为变量再传入

// 注册服务端：type 为 rosidl 服务类型（如 example_interfaces::srv::AddTwoInts）
#define YOMKROS2_SERVICE(type, serviceName, callback) \
    yomk::yomkROS2Node().createService<type>(serviceName, callback)

// 预创建服务客户端：只创建并缓存客户端实体，不检查服务可用性、不发送请求；
// 【推荐用法】在 YOMKROS2_RUN 之前预创建（创建节点 → 创建客户端 → run →
// CALL_SERVICE/CALL_SERVICE_ASYNC 发送请求），跨节点调用必须如此
#define YOMKROS2_SERVICE_CLIENT(type, serviceName) \
    yomk::yomkROS2Node().createServiceClient<type>(serviceName)

// 同步调用服务：返回响应 SharedPtr，失败返回 nullptr（自动建客户端，一次调用完成）；
// request 若含顶层逗号（如聚合初始化 {1, 2}），先定义为变量再传入
#define YOMKROS2_CALL_SERVICE(type, serviceName, request) \
    yomk::yomkROS2Node().callService<type>(serviceName, request)

// 异步调用服务：立即返回，响应就绪时回调收到 Response::SharedPtr；
// callback 先定义为变量后传入
#define YOMKROS2_CALL_SERVICE_ASYNC(type, serviceName, request, callback) \
    yomk::yomkROS2Node().callServiceAsync<type>(serviceName, request, callback)

// ===== 动作（Action）通信接口（只封异步；失败时接口内部输出 RCLCPP_ERROR 日志，成功无输出）=====
// callback/goal 体内若含顶层逗号（如模板参数），请先将回调/目标定义为变量再传入

// 注册动作服务端：三回调一一对应原生 handle_goal/handle_cancel/handle_accepted；
// goalCallback 返回 int：1=拒绝 / 2=接受并立即执行 / 3=接受但延迟执行（用户后续
// 手动 goalHandle->execute()）；cancelCallback 返回 bool：true 允许取消 / false 拒绝取消；
// executeCallback(goalHandle) 在 executor 线程被调用，用户应在其中自行开线程执行：
// goalHandle->publish_feedback / is_canceling / succeed / abort / canceled；
// type 为 rosidl 动作类型（如 example_interfaces::action::Fibonacci）
#define YOMKROS2_ACTION(type, actionName, goalCallback, cancelCallback, executeCallback) \
    yomk::yomkROS2Node().createAction<type>(actionName, goalCallback, cancelCallback, executeCallback)

// 预创建动作客户端：只创建并缓存客户端实体，不检查服务端可用性、不发送请求；
// 【推荐用法】在 YOMKROS2_RUN 之前预创建（创建节点 → 创建客户端 → run →
// CALL_ACTION_ASYNC 发送目标），跨节点调用必须如此
#define YOMKROS2_ACTION_CLIENT(type, actionName) \
    yomk::yomkROS2Node().createActionClient<type>(actionName)

// 异步发送动作目标：立即返回，成功返回非零 goalId（供 CANCEL_GOAL 使用），失败返回 0；
// 三个回调均携带 goalId（对外屏蔽 ClientGoalHandle）：goalResponseCallback(goalId,
// accepted) 必触发一次告知目标被接受/拒绝；goal 被接受后 feedbackCallback 收到各步
// 反馈；动作终结或被拒绝（code=UNKNOWN）时 resultCallback 必触发一次
#define YOMKROS2_CALL_ACTION_ASYNC(type, actionName, goal, goalResponseCallback, feedbackCallback, resultCallback) \
    yomk::yomkROS2Node().callActionAsync<type>(actionName, goal, goalResponseCallback, feedbackCallback, resultCallback)

// 取消指定活跃 goal：服务端拒绝取消时 goal 继续执行至正常终结；
// goal 不存在或已终结返回 false；取消成功后 resultCallback 收到 CANCELED
#define YOMKROS2_CANCEL_GOAL(type, actionName, goalId) \
    yomk::yomkROS2Node().cancelGoal<type>(actionName, goalId)

// 取消该 action 的全部活跃 goal；无活跃 goal 返回 false
#define YOMKROS2_CANCEL_ALL_GOALS(type, actionName) \
    yomk::yomkROS2Node().cancelAllGoals<type>(actionName)

// 取消该 action 中目标时间戳早于 stamp（rclcpp::Time）的全部 goal；
// 无匹配活跃 goal 返回 false
#define YOMKROS2_CANCEL_GOALS_BEFORE(type, actionName, stamp) \
    yomk::yomkROS2Node().cancelGoalsBefore<type>(actionName, stamp)
