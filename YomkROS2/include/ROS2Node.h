#pragma once

#include <condition_variable>
#include <functional>
#include <future>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <typeindex>
#include <utility>
#include <vector>

#include <rclcpp/rclcpp.hpp>
#include <rclcpp_action/rclcpp_action.hpp>

namespace yomk
{

    // ROS2 通信封装类：非模板类 + 模板方法
    // - 单节点设计：一个实例持有唯一 rclcpp::Node，发布订阅共用该节点
    // - 模板方法内联于本头文件，实例化发生在调用者编译单元，
    //   支持任意 rosidl 类型（含用户自定义类型）
    // - 内部用 rclcpp 基类指针存储实体，type_index 校验注册类型与调用类型一致
    // - init 仅初始化，节点运行由 run() 启动：blocking=false 后台线程运行，blocking=true 当前线程阻塞运行
    // - 订阅回调由 MultiThreadedExecutor 线程池驱动，每个订阅持有独立 MutuallyExclusive 回调组：
    //   跨主题回调可并发（慢回调不会阻塞其他主题），同主题回调串行保序
    //   【线程安全提示】用户回调须自行保证共享数据的线程安全（如加锁）；
    //   同主题回调可能被调度到不同线程，请勿依赖 thread_local 状态
    class ROS2Node
    {
    public:
        ROS2Node();
        ~ROS2Node();
        ROS2Node(const ROS2Node &) = delete;
        ROS2Node &operator=(const ROS2Node &) = delete;

    public:
        // 初始化 ROS2：命令行参数、节点名称（仅初始化，不启动运行，需调用 run()）
        // 进程级 rclcpp::init 仅执行一次，重复调用本接口返回 false
        bool init(int argc, char **argv, const std::string &nodeName);
        // 运行节点，开始执行订阅回调（MultiThreadedExecutor 线程池驱动 spin）
        // blocking=false：启动后台线程运行 spin，立即返回；
        // blocking=true：在当前线程运行 spin，阻塞直到 shutdown() 被调用。
        // 未初始化或已在运行时返回 false；请勿在回调内调用 shutdown/run（会死锁）
        bool run(bool blocking = false);
        // 显式销毁节点与全部发布/订阅实体，停止 spin（退出前建议调用）
        bool shutdown();
        // 是否已初始化
        bool isInitialized() const;

        // 注册发布主题：主题名、缓存队列长度；主题重名返回 false
        template <typename T>
        bool registerPubTopic(const std::string &topicName, size_t queueSize);
        // 注册订阅主题：主题名、队列长度、回调；主题重名返回 false
        // 每个订阅分配独立 MutuallyExclusive 回调组：同主题回调串行保序，跨主题可并发，
        // 【注意】回调须自行保证共享数据的线程安全
        template <typename T, typename CallbackT>
        bool registerSubTopic(const std::string &topicName, size_t queueSize, CallbackT &&callback);
        // 发布一个数据：主题名、数据；主题不存在或类型不匹配返回 false
        template <typename T>
        bool publish(const std::string &topicName, const T &data);

        // ===== 参数接口（一次调用完成；失败时输出 RCLCPP_ERROR 日志，成功无输出）=====

        // 声明参数并返回当前值：首次声明返回 defaultValue，已存在返回现有值（幂等）；
        // 未初始化或类型不匹配返回 defaultValue（并输出错误日志）
        template <typename T>
        T declareParam(const std::string &name, const T &defaultValue);
        // 查询参数：不存在或类型不匹配返回 false
        template <typename T>
        bool getParam(const std::string &name, T &out) const;
        // 设置参数：未声明、类型不匹配或被 on-set 回调拒绝返回 false；
        // 【注意】回调在锁内同步执行，勿在回调中调用本节点接口（会死锁）
        template <typename T>
        bool setParam(const std::string &name, const T &value);
        // 参数是否存在
        bool hasParam(const std::string &name) const;
        // 删除参数（撤销声明）：带默认值声明的参数为静态类型（rclcpp 语义），
        // 撤销失败返回 false 并输出错误日志；未声明返回 false
        bool undeclareParam(const std::string &name);
        // 列出全部参数名
        std::vector<std::string> listParams() const;
        // 注册 set 参数回调：返回 false 拒绝设置；返回句柄 id 供移除（未初始化返回 0）
        uint64_t addOnSetParamCallback(std::function<bool(const std::vector<rclcpp::Parameter> &)> cb);
        void removeOnSetParamCallback(uint64_t handleId);

        // 远程参数（内部自动创建异步客户端，以 future 同步等待响应，一次调用完成；
        // 响应由本节点自身的 spin 处理，故调用前需已 run 运行）
        template <typename T>
        bool getRemoteParam(const std::string &remoteNodeName, const std::string &name, T &out);
        template <typename T>
        bool setRemoteParam(const std::string &remoteNodeName, const std::string &name, const T &value);
        bool hasRemoteParam(const std::string &remoteNodeName, const std::string &name);
        std::vector<std::string> listRemoteParams(const std::string &remoteNodeName);

        // ===== 服务通信接口（一次调用完成；失败时输出 RCLCPP_ERROR 日志，成功无输出）=====

        // 注册服务端：服务名、回调；服务重名返回 false
        // 【回调规则】与订阅回调同规则（见 registerSubTopic 注释）：每个服务端分配独立
        // MutuallyExclusive 回调组——同服务请求串行保序，跨服务/跨 topic 可并发；
        // 回调内阻塞只影响本服务的后续请求，不会阻塞其他服务或 topic 回调
        // （受 MultiThreadedExecutor 线程数限制，全部线程被占用时仍会排队）；
        // 回调须自行保证共享数据的线程安全
        template <typename ServiceT, typename CallbackT>
        bool createService(const std::string &serviceName, CallbackT &&callback);
        // 预创建服务客户端：只创建并缓存客户端实体，不检查服务可用性、不发送请求；
        // 类型不匹配或未初始化返回 false
        // 【推荐用法】在 run 之前预创建客户端（创建节点 → 创建客户端 → run →
        // callService/callServiceAsync 发送请求），确保客户端实体在 spin 启动前
        // 就绪（跨节点调用必须如此；spin 期间动态创建的客户端可能收不到响应）
        template <typename ServiceT>
        bool createServiceClient(const std::string &serviceName);
        // 同步调用服务：自动创建客户端 + 等待服务 + 发送请求 + future 同步等待响应（一次调用完成）；
        // 响应由本节点自身的 spin 处理，故调用前需已 run 运行（与远程参数同模式，
        // 不建内部节点、不二次 spin）；失败返回 nullptr
        template <typename ServiceT, typename RequestT>
        typename ServiceT::Response::SharedPtr callService(const std::string &serviceName, const RequestT &request);
        // 异步调用服务：发送请求后立即返回，响应就绪时在独立 MutuallyExclusive 回调组中
        // 调用 callback(Response::SharedPtr)；发送失败（未初始化/服务不可用）返回 false
        // （此时回调不会被调用）
        // 【回调规则】与订阅回调同规则：客户端异步回调在独立回调组执行，回调内阻塞不会
        // 影响 topic 回调与其他服务回调；回调须自行保证共享数据的线程安全
        template <typename ServiceT, typename RequestT, typename CallbackT>
        bool callServiceAsync(const std::string &serviceName, const RequestT &request, CallbackT &&callback);

        // ===== 动作（Action）通信接口（只封异步；失败时输出 RCLCPP_ERROR 日志，成功无输出）=====

        // 注册动作服务端：三回调一一对应原生 handle_goal/handle_cancel/handle_accepted；
        // 动作重名返回 false
        //   goalCallback(goal) 返回 int，原样对应原生 GoalResponse 三值：
        //     1=拒绝（REJECT）
        //     2=接受并立即执行（ACCEPT_AND_EXECUTE，随后自动调 executeCallback）
        //     3=接受但延迟执行（ACCEPT_AND_DEFER，executeCallback 不会被自动调用，
        //       用户后续自行调用 goalHandle->execute() 触发执行）
        //   cancelCallback(goalHandle) 返回 true 允许取消 / false 拒绝取消（用户决定，
        //   危险任务可拒绝；原生 CancelResponse 仅二值，bool 映射无丢失）
        // 目标接受且立即执行时 executeCallback(goalHandle) 在 executor 线程被调用，
        // 用户应在其中自行开线程执行长耗时任务（避免阻塞 executor），执行线程中
        // 使用原生句柄：
        //   goalHandle->publish_feedback(fb)   发布进度反馈
        //   goalHandle->is_canceling()         检查取消请求
        //   goalHandle->succeed(result) / abort(result) / canceled(result)  终结目标
        // 【回调规则】goal/cancel/accepted 三回调在独立 MutuallyExclusive 回调组执行，
        // 不阻塞 topic/服务回调；用户执行线程的共享数据线程安全与退出时机由用户保证
        // （shutdown 前须确保执行线程已退出）
        template <typename ActionT, typename GoalCallbackT, typename CancelCallbackT, typename ExecuteCallbackT>
        bool createAction(const std::string &actionName, GoalCallbackT &&goalCallback,
                          CancelCallbackT &&cancelCallback, ExecuteCallbackT &&executeCallback);
        // 预创建动作客户端：只创建并缓存客户端实体，不检查服务端可用性、不发送请求；
        // 类型不匹配或未初始化返回 false
        // 【推荐用法】在 run 之前预创建（创建节点 → 创建客户端 → run →
        // callActionAsync 发送目标），与服务客户端同理，避免 spin 期间动态创建实体
        template <typename ActionT>
        bool createActionClient(const std::string &actionName);
        // 异步发送动作目标：立即返回，成功返回非零 goalId（唯一标识本次目标，
        // 三个回调均携带该 goalId，供 cancelGoal 使用），失败返回 0；响应由本节点
        // 自身的 spin 处理，故调用前需已 run 运行；支持同一 action 多个 goal 并发
        // （每 goal 独立回调链）
        // 【回调不变量】每次调用：goalResponseCallback 必触发且仅触发一次且先于
        // 其余回调（accepted=true 被接受 / accepted=false 被拒绝）；resultCallback
        // 必触发且仅触发一次：动作终结（SUCCEEDED/ABORTED/CANCELED）或被拒绝
        // （code=UNKNOWN 且 result 为空，原生拒绝时 result_callback 不触发，由封装
        // 合成）；goal 被接受后 feedbackCallback 收到各步反馈；发送失败（未初始化/
        // 服务端不可用/类型不匹配）输出 RCLCPP_ERROR（此时回调不会被调用）
        // 客户端回调对外屏蔽 ClientGoalHandle，以 goalId 为统一身份：
        //   goalResponseCallback(uint64_t goalId, bool accepted)
        //   feedbackCallback(uint64_t goalId,
        //                    const std::shared_ptr<const typename ActionT::Feedback> feedback)
        //   resultCallback(uint64_t goalId, rclcpp_action::ResultCode code,
        //                  const std::shared_ptr<const typename ActionT::Result> result)
        //   code: SUCCEEDED/ABORTED/CANCELED/UNKNOWN（被拒绝时为 UNKNOWN）
        // 【回调规则】与订阅回调同规则：客户端回调在独立回调组执行，回调须自行保证
        // 共享数据的线程安全
        template <typename ActionT, typename GoalResponseCallbackT, typename FeedbackCallbackT, typename ResultCallbackT>
        uint64_t callActionAsync(const std::string &actionName, const typename ActionT::Goal &goal,
                                 GoalResponseCallbackT &&goalResponseCallback,
                                 FeedbackCallbackT &&feedbackCallback, ResultCallbackT &&resultCallback);
        // 取消指定活跃 goal（goalId 由 callActionAsync 返回）：服务端 cancelCallback
        // 返回 false 时取消被拒绝，goal 继续执行至正常终结；goal 不存在或已终结返回
        // false；取消成功后 resultCallback 收到 CANCELED
        template <typename ActionT>
        bool cancelGoal(const std::string &actionName, uint64_t goalId);
        // 取消该 action 的全部活跃 goal（对应原生 async_cancel_all_goals）；
        // 无活跃 goal 返回 false；各 goal 的 resultCallback 收到 CANCELED（被服务端拒绝
        // 取消的 goal 除外）
        template <typename ActionT>
        bool cancelAllGoals(const std::string &actionName);
        // 取消该 action 中目标时间戳早于 stamp 的全部 goal（对应原生
        // async_cancel_goals_before）；无匹配活跃 goal 返回 false
        template <typename ActionT>
        bool cancelGoalsBefore(const std::string &actionName, const rclcpp::Time &stamp);

    private:
        std::shared_ptr<rclcpp::Node> node_;
        std::shared_ptr<rclcpp::Executor> executor_; // MultiThreadedExecutor
        std::thread spinThread_;
        std::map<std::string, std::shared_ptr<rclcpp::PublisherBase>> pubTopics_;
        std::map<std::string, std::shared_ptr<rclcpp::SubscriptionBase>> subTopics_;
        std::map<std::string, std::shared_ptr<rclcpp::CallbackGroup>> subGroups_; // 持有各订阅的独立回调组（节点仅存弱引用）
        std::map<std::string, std::type_index> pubTypes_;
        std::map<std::string, std::type_index> subTypes_;
        std::map<std::string, std::shared_ptr<rclcpp::AsyncParametersClient>> paramClients_; // 远程参数客户端（懒创建）
        std::map<uint64_t, std::shared_ptr<rclcpp::node_interfaces::OnSetParametersCallbackHandle>> paramCallbacks_;
        std::map<std::string, std::shared_ptr<rclcpp::ServiceBase>> serviceServers_;         // 已注册服务端
        std::map<std::string, std::shared_ptr<rclcpp::CallbackGroup>> serviceGroups_;        // 持有各服务端的独立回调组（节点仅存弱引用）
        std::map<std::string, std::shared_ptr<rclcpp::ClientBase>> serviceClients_;          // 服务客户端（懒创建）
        std::map<std::string, std::type_index> serviceClientTypes_;                          // 客户端服务类型（校验）
        std::map<std::string, std::shared_ptr<rclcpp::CallbackGroup>> serviceClientGroups_;  // 持有各客户端的独立回调组
        std::map<std::string, std::shared_ptr<rclcpp_action::ServerBase>> actionServers_;    // 已注册动作服务端
        std::map<std::string, std::shared_ptr<rclcpp::CallbackGroup>> actionGroups_;         // 持有各动作服务端的独立回调组
        std::map<std::string, std::shared_ptr<rclcpp_action::ClientBase>> actionClients_;    // 动作客户端（预创建/懒创建）
        std::map<std::string, std::type_index> actionClientTypes_;                           // 客户端动作类型（校验）
        std::map<std::string, std::shared_ptr<rclcpp::CallbackGroup>> actionClientGroups_;   // 持有各动作客户端的独立回调组
        std::map<std::string, std::map<uint64_t, std::shared_ptr<void>>> actionActiveGoals_; // 活跃 goal（goalId → ClientGoalHandle，类型擦除）
        uint64_t paramCallbackNextId_ = 1;
        uint64_t actionGoalNextId_ = 1;
        mutable std::mutex mtx_;
        std::condition_variable spinExitedCv_; // spin 退出（后台线程或阻塞线程）后通知 shutdown
        bool initialized_ = false;
        bool running_ = false;       // run 已启动 spin（后台线程或当前线程阻塞中）
        bool ownRclcppInit_ = false; // 本实例执行过 rclcpp::init，shutdown 时对称调用 rclcpp::shutdown

        // 懒创建远程参数客户端（不加锁，调用方持锁）；失败返回 nullptr（已输出日志）
        std::shared_ptr<rclcpp::AsyncParametersClient> getOrCreateParamClient(const std::string &remoteNodeName);
        // 创建并缓存服务客户端（不加锁，调用方持锁）；不检查服务可用性，类型不匹配或创建失败返回 nullptr（已输出日志）
        template <typename ServiceT>
        std::shared_ptr<rclcpp::Client<ServiceT>> createServiceClientImpl(const std::string &serviceName);
        // 懒创建服务客户端（不加锁，调用方持锁）：先复用已缓存客户端，否则现场创建并缓存；
        // 类型不匹配或创建失败返回 nullptr（已输出日志）
        template <typename ServiceT>
        std::shared_ptr<rclcpp::Client<ServiceT>> getOrCreateServiceClient(const std::string &serviceName);
        // 创建并缓存动作客户端（不加锁，调用方持锁）；不检查服务端可用性，创建失败返回 nullptr（已输出日志）
        template <typename ActionT>
        std::shared_ptr<rclcpp_action::Client<ActionT>> createActionClientImpl(const std::string &actionName);
        // 懒创建动作客户端（不加锁，调用方持锁）：先复用已缓存客户端，否则现场创建并缓存；
        // 类型不匹配或创建失败返回 nullptr（已输出日志）
        template <typename ActionT>
        std::shared_ptr<rclcpp_action::Client<ActionT>> getOrCreateActionClient(const std::string &actionName);
    };

    // ---- 模板方法实现（内联于头文件，实例化发生在调用者编译单元） ----

    template <typename T>
    bool ROS2Node::registerPubTopic(const std::string &topicName, size_t queueSize)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_)
        {
            return false;
        }
        if (pubTopics_.find(topicName) != pubTopics_.end())
        {
            return false;
        }
        auto publisher = node_->create_publisher<T>(topicName, rclcpp::QoS(queueSize));
        if (!publisher)
        {
            return false;
        }
        pubTopics_[topicName] = publisher;
        pubTypes_.insert_or_assign(topicName, std::type_index(typeid(T)));
        return true;
    }

    template <typename T, typename CallbackT>
    bool ROS2Node::registerSubTopic(const std::string &topicName, size_t queueSize, CallbackT &&callback)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_)
        {
            return false;
        }
        if (subTopics_.find(topicName) != subTopics_.end())
        {
            return false;
        }
        // 每订阅独立 MutuallyExclusive group：跨主题并发 + 同主题保序
        auto group = node_->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
        rclcpp::SubscriptionOptions options;
        options.callback_group = group;
        auto subscription = node_->create_subscription<T>(topicName, rclcpp::QoS(queueSize),
                                                          std::forward<CallbackT>(callback), options);
        if (!subscription)
        {
            return false;
        }
        subTopics_[topicName] = subscription;
        subTypes_.insert_or_assign(topicName, std::type_index(typeid(T)));
        subGroups_[topicName] = group; // 节点内部仅持 group 弱引用，须在此持有 shared_ptr
        return true;
    }

    template <typename T>
    bool ROS2Node::publish(const std::string &topicName, const T &data)
    {
        // 锁内仅查表取出 shared_ptr，rcl 调用在锁外执行，降低高频发布时的锁竞争
        std::shared_ptr<rclcpp::Publisher<T>> publisher;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            auto it = pubTopics_.find(topicName);
            if (it == pubTopics_.end())
            {
                return false;
            }
            auto typeIt = pubTypes_.find(topicName);
            if (typeIt == pubTypes_.end() || typeIt->second != std::type_index(typeid(T)))
            {
                return false; // 注册类型与调用类型不一致
            }
            publisher = std::static_pointer_cast<rclcpp::Publisher<T>>(it->second);
        }
        publisher->publish(data);
        return true;
    }

    // ---- 参数接口模板方法实现 ----

    template <typename T>
    T ROS2Node::declareParam(const std::string &name, const T &defaultValue)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "declareParam [%s] failed: node not initialized", name.c_str());
            return defaultValue;
        }
        if (node_->has_parameter(name))
        {
            try
            {
                return node_->get_parameter(name).get_value<T>();
            }
            catch (const std::exception &e)
            {
                RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "declareParam [%s] failed: existing type mismatch (%s)", name.c_str(), e.what());
                return defaultValue;
            }
        }
        node_->declare_parameter(name, rclcpp::ParameterValue(defaultValue));
        return defaultValue;
    }

    template <typename T>
    bool ROS2Node::getParam(const std::string &name, T &out) const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "getParam [%s] failed: node not initialized", name.c_str());
            return false;
        }
        if (!node_->has_parameter(name))
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "getParam [%s] failed: parameter not declared", name.c_str());
            return false;
        }
        try
        {
            out = node_->get_parameter(name).get_value<T>();
            return true;
        }
        catch (const std::exception &e)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "getParam [%s] failed: type mismatch (%s)", name.c_str(), e.what());
            return false;
        }
    }

    template <typename T>
    bool ROS2Node::setParam(const std::string &name, const T &value)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "setParam [%s] failed: node not initialized", name.c_str());
            return false;
        }
        if (!node_->has_parameter(name))
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "setParam [%s] failed: parameter not declared", name.c_str());
            return false;
        }
        rcl_interfaces::msg::SetParametersResult result = node_->set_parameter(rclcpp::Parameter(name, value));
        if (!result.successful)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "setParam [%s] failed: %s", name.c_str(), result.reason.c_str());
            return false;
        }
        return true;
    }

    template <typename T>
    bool ROS2Node::getRemoteParam(const std::string &remoteNodeName, const std::string &name, T &out)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "getRemoteParam [%s/%s] failed: node not initialized", remoteNodeName.c_str(), name.c_str());
            return false;
        }
        auto client = getOrCreateParamClient(remoteNodeName);
        if (!client)
        {
            return false;
        }
        // 异步请求 + future 同步等待：响应由本节点自身的 spin 处理（调用前需已 run）
        auto future = client->get_parameters({name});
        if (future.wait_for(std::chrono::seconds(1)) != std::future_status::ready)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "getRemoteParam [%s/%s] failed: timeout waiting for response", remoteNodeName.c_str(), name.c_str());
            return false;
        }
        const auto params = future.get();
        if (params.size() != 1 || params[0].get_type() == rclcpp::ParameterType::PARAMETER_NOT_SET)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "getRemoteParam [%s/%s] failed: parameter not found", remoteNodeName.c_str(), name.c_str());
            return false;
        }
        try
        {
            out = params[0].get_value<T>();
            return true;
        }
        catch (const std::exception &e)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "getRemoteParam [%s/%s] failed: %s", remoteNodeName.c_str(), name.c_str(), e.what());
            return false;
        }
    }

    template <typename T>
    bool ROS2Node::setRemoteParam(const std::string &remoteNodeName, const std::string &name, const T &value)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "setRemoteParam [%s/%s] failed: node not initialized", remoteNodeName.c_str(), name.c_str());
            return false;
        }
        auto client = getOrCreateParamClient(remoteNodeName);
        if (!client)
        {
            return false;
        }
        // 异步请求 + future 同步等待：响应由本节点自身的 spin 处理（调用前需已 run）
        auto future = client->set_parameters({rclcpp::Parameter(name, value)});
        if (future.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "setRemoteParam [%s/%s] failed: timeout waiting for response", remoteNodeName.c_str(), name.c_str());
            return false;
        }
        const auto results = future.get();
        if (results.empty() || !results[0].successful)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "setRemoteParam [%s/%s] failed: %s", remoteNodeName.c_str(), name.c_str(),
                         results.empty() ? "empty response" : results[0].reason.c_str());
            return false;
        }
        return true;
    }

    // ---- 服务通信接口模板方法实现 ----

    template <typename ServiceT, typename CallbackT>
    bool ROS2Node::createService(const std::string &serviceName, CallbackT &&callback)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "createService [%s] failed: node not initialized", serviceName.c_str());
            return false;
        }
        if (serviceServers_.find(serviceName) != serviceServers_.end())
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "createService [%s] failed: service already registered", serviceName.c_str());
            return false;
        }
        // 每服务端独立 MutuallyExclusive group：同服务请求串行保序，跨服务/跨 topic 可并发
        auto group = node_->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
        auto server = node_->create_service<ServiceT>(serviceName, std::forward<CallbackT>(callback),
                                                      rmw_qos_profile_services_default, group);
        if (!server)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "createService [%s] failed: create_service returned null", serviceName.c_str());
            return false;
        }
        serviceServers_[serviceName] = server;
        serviceGroups_[serviceName] = group; // 节点内部仅持 group 弱引用，须在此持有 shared_ptr
        return true;
    }

    template <typename ServiceT>
    std::shared_ptr<rclcpp::Client<ServiceT>> ROS2Node::createServiceClientImpl(const std::string &serviceName)
    {
        // 客户端响应回调在独立 MutuallyExclusive 组执行，不与 topic 回调、其他服务回调互相阻塞
        auto group = node_->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
        auto client = node_->create_client<ServiceT>(serviceName, rmw_qos_profile_services_default, group);
        if (!client)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "service client [%s] failed: create_client returned null", serviceName.c_str());
            return nullptr;
        }
        // 创建后立即缓存（不检查服务可用性）：供 run 前预创建，spin 启动时实体已就绪
        serviceClients_[serviceName] = client;
        serviceClientTypes_.insert_or_assign(serviceName, std::type_index(typeid(ServiceT)));
        serviceClientGroups_[serviceName] = group; // 节点内部仅持 group 弱引用，须在此持有 shared_ptr
        return client;
    }

    template <typename ServiceT>
    std::shared_ptr<rclcpp::Client<ServiceT>> ROS2Node::getOrCreateServiceClient(const std::string &serviceName)
    {
        auto it = serviceClients_.find(serviceName);
        if (it != serviceClients_.end())
        {
            auto typeIt = serviceClientTypes_.find(serviceName);
            if (typeIt == serviceClientTypes_.end() || typeIt->second != std::type_index(typeid(ServiceT)))
            {
                RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "service client [%s] failed: service type mismatch", serviceName.c_str());
                return nullptr;
            }
            return std::static_pointer_cast<rclcpp::Client<ServiceT>>(it->second);
        }
        return createServiceClientImpl<ServiceT>(serviceName);
    }

    template <typename ServiceT>
    bool ROS2Node::createServiceClient(const std::string &serviceName)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "createServiceClient [%s] failed: node not initialized", serviceName.c_str());
            return false;
        }
        return createServiceClientImpl<ServiceT>(serviceName) != nullptr;
    }

    template <typename ServiceT, typename RequestT>
    typename ServiceT::Response::SharedPtr ROS2Node::callService(const std::string &serviceName, const RequestT &request)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "callService [%s] failed: node not initialized", serviceName.c_str());
            return nullptr;
        }
        auto client = getOrCreateServiceClient<ServiceT>(serviceName);
        if (!client)
        {
            return nullptr;
        }
        // 发送前检查服务可用性（客户端已缓存时同样检查，不存在的服务返回 nullptr）
        if (!client->wait_for_service(std::chrono::milliseconds(1000)))
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "callService [%s] failed: service not available", serviceName.c_str());
            return nullptr;
        }
        // 异步请求 + future 同步等待：响应由本节点自身的 spin 处理（调用前需已 run）
        auto future = client->async_send_request(std::make_shared<typename ServiceT::Request>(request));
        if (future.wait_for(std::chrono::seconds(5)) != std::future_status::ready)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "callService [%s] failed: timeout waiting for response", serviceName.c_str());
            return nullptr;
        }
        return future.get();
    }

    template <typename ServiceT, typename RequestT, typename CallbackT>
    bool ROS2Node::callServiceAsync(const std::string &serviceName, const RequestT &request, CallbackT &&callback)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "callServiceAsync [%s] failed: node not initialized", serviceName.c_str());
            return false;
        }
        auto client = getOrCreateServiceClient<ServiceT>(serviceName);
        if (!client)
        {
            return false;
        }
        // 发送前检查服务可用性（客户端已缓存时同样检查，不存在的服务返回 false）
        if (!client->wait_for_service(std::chrono::milliseconds(1000)))
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "callServiceAsync [%s] failed: service not available", serviceName.c_str());
            return false;
        }
        // 回调式请求：发送后立即返回，响应就绪时在客户端独立回调组中调用用户回调；
        // 内部包装 future.get()，用户回调直接收 Response::SharedPtr，无需接触 future
        client->async_send_request(std::make_shared<typename ServiceT::Request>(request),
                                   [cb = std::forward<CallbackT>(callback)](typename rclcpp::Client<ServiceT>::SharedFuture future)
                                   {
                                       try
                                       {
                                           cb(future.get());
                                       }
                                       catch (const std::exception &e)
                                       {
                                           RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "callServiceAsync callback failed: %s", e.what());
                                       }
                                   });
        return true;
    }

    // ---- 动作通信接口模板方法实现 ----

    template <typename ActionT, typename GoalCallbackT, typename CancelCallbackT, typename ExecuteCallbackT>
    bool ROS2Node::createAction(const std::string &actionName, GoalCallbackT &&goalCallback,
                                CancelCallbackT &&cancelCallback, ExecuteCallbackT &&executeCallback)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "createAction [%s] failed: node not initialized", actionName.c_str());
            return false;
        }
        if (actionServers_.find(actionName) != actionServers_.end())
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "createAction [%s] failed: action already registered", actionName.c_str());
            return false;
        }
        // goalCallback 返回 int 原样映射原生 GoalResponse 三值（1/2/3），非法值按拒绝处理
        auto goalCb = [actionName, userGoalCb = std::forward<GoalCallbackT>(goalCallback)](
                          const rclcpp_action::GoalUUID &, std::shared_ptr<const typename ActionT::Goal> goal)
        {
            switch (userGoalCb(goal))
            {
            case 2:
                return rclcpp_action::GoalResponse::ACCEPT_AND_EXECUTE;
            case 3:
                return rclcpp_action::GoalResponse::ACCEPT_AND_DEFER;
            case 1:
                return rclcpp_action::GoalResponse::REJECT;
            default:
                RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "createAction [%s] failed: goalCallback returned invalid value, treated as REJECT", actionName.c_str());
                return rclcpp_action::GoalResponse::REJECT;
            }
        };
        // cancelCallback 返回 bool 一一映射原生 CancelResponse 二值（true 允许 / false 拒绝）
        auto cancelCb = [userCancelCb = std::forward<CancelCallbackT>(cancelCallback)](
                            std::shared_ptr<rclcpp_action::ServerGoalHandle<ActionT>> goalHandle)
        {
            return userCancelCb(goalHandle) ? rclcpp_action::CancelResponse::ACCEPT
                                            : rclcpp_action::CancelResponse::REJECT;
        };
        // 透传给用户：仅 ACCEPT_AND_EXECUTE 时原生会触发；长耗时执行由用户自行开线程
        auto acceptedCb = [userExecCb = std::forward<ExecuteCallbackT>(executeCallback)](
                              std::shared_ptr<rclcpp_action::ServerGoalHandle<ActionT>> goalHandle)
        {
            userExecCb(goalHandle);
        };
        // 每动作服务端独立 MutuallyExclusive group：不阻塞 topic/服务回调
        auto group = node_->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
        auto server = rclcpp_action::create_server<ActionT>(node_, actionName, goalCb, cancelCb, acceptedCb,
                                                            rcl_action_server_get_default_options(), group);
        if (!server)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "createAction [%s] failed: create_server returned null", actionName.c_str());
            return false;
        }
        actionServers_[actionName] = server;
        actionGroups_[actionName] = group; // 节点内部仅持 group 弱引用，须在此持有 shared_ptr
        return true;
    }

    template <typename ActionT>
    std::shared_ptr<rclcpp_action::Client<ActionT>> ROS2Node::createActionClientImpl(const std::string &actionName)
    {
        // 客户端 feedback/result 回调在独立 MutuallyExclusive 组执行，不与 topic/服务回调互相阻塞
        auto group = node_->create_callback_group(rclcpp::CallbackGroupType::MutuallyExclusive);
        auto client = rclcpp_action::create_client<ActionT>(node_, actionName, group);
        if (!client)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "action client [%s] failed: create_client returned null", actionName.c_str());
            return nullptr;
        }
        // 创建后立即缓存（不检查服务端可用性）：供 run 前预创建，spin 启动时实体已就绪
        actionClients_[actionName] = client;
        actionClientTypes_.insert_or_assign(actionName, std::type_index(typeid(ActionT)));
        actionClientGroups_[actionName] = group; // 节点内部仅持 group 弱引用，须在此持有 shared_ptr
        return client;
    }

    template <typename ActionT>
    std::shared_ptr<rclcpp_action::Client<ActionT>> ROS2Node::getOrCreateActionClient(const std::string &actionName)
    {
        auto it = actionClients_.find(actionName);
        if (it != actionClients_.end())
        {
            auto typeIt = actionClientTypes_.find(actionName);
            if (typeIt == actionClientTypes_.end() || typeIt->second != std::type_index(typeid(ActionT)))
            {
                RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "action client [%s] failed: action type mismatch", actionName.c_str());
                return nullptr;
            }
            return std::static_pointer_cast<rclcpp_action::Client<ActionT>>(it->second);
        }
        return createActionClientImpl<ActionT>(actionName);
    }

    template <typename ActionT>
    bool ROS2Node::createActionClient(const std::string &actionName)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "createActionClient [%s] failed: node not initialized", actionName.c_str());
            return false;
        }
        return createActionClientImpl<ActionT>(actionName) != nullptr;
    }

    template <typename ActionT, typename GoalResponseCallbackT, typename FeedbackCallbackT, typename ResultCallbackT>
    uint64_t ROS2Node::callActionAsync(const std::string &actionName, const typename ActionT::Goal &goal,
                                       GoalResponseCallbackT &&goalResponseCallback,
                                       FeedbackCallbackT &&feedbackCallback, ResultCallbackT &&resultCallback)
    {
        using GoalHandle = rclcpp_action::ClientGoalHandle<ActionT>;
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "callActionAsync [%s] failed: node not initialized", actionName.c_str());
            return 0;
        }
        auto client = getOrCreateActionClient<ActionT>(actionName);
        if (!client)
        {
            return 0;
        }
        // 发送前检查服务端可用性（客户端已缓存时同样检查，不存在的动作返回 0）
        if (!client->wait_for_action_server(std::chrono::milliseconds(1000)))
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "callActionAsync [%s] failed: action server not available", actionName.c_str());
            return 0;
        }
        const uint64_t goalId = actionGoalNextId_++;
        auto userRespCb = std::make_shared<std::decay_t<GoalResponseCallbackT>>(std::forward<GoalResponseCallbackT>(goalResponseCallback));
        auto userFbCb = std::make_shared<std::decay_t<FeedbackCallbackT>>(std::forward<FeedbackCallbackT>(feedbackCallback));
        auto userResultCb = std::make_shared<std::decay_t<ResultCallbackT>>(std::forward<ResultCallbackT>(resultCallback));
        typename rclcpp_action::Client<ActionT>::SendGoalOptions options;
        // 对外屏蔽 ClientGoalHandle：回调统一以 goalId 标识目标（与返回值/取消接口同身份）
        options.feedback_callback = [goalId, userFbCb](
                                        typename GoalHandle::SharedPtr,
                                        const std::shared_ptr<const typename ActionT::Feedback> feedback)
        {
            (*userFbCb)(goalId, feedback);
        };
        // 终结时移除活跃记录（保证 cancelGoal 只针对未终结 goal）；以 goalId 通知用户
        options.result_callback = [this, actionName, goalId, userResultCb](
                                      const typename GoalHandle::WrappedResult &result)
        {
            {
                std::lock_guard<std::mutex> resultLock(mtx_);
                auto it = actionActiveGoals_.find(actionName);
                if (it != actionActiveGoals_.end())
                {
                    it->second.erase(goalId);
                    if (it->second.empty())
                    {
                        actionActiveGoals_.erase(it);
                    }
                }
            }
            (*userResultCb)(goalId, result.code, result.result);
        };
        // 接受：记录活跃句柄（供取消）+ 通知 accepted=true；拒绝：通知 accepted=false
        // + 合成 UNKNOWN 终结通知（原生拒绝时 result_callback 不触发，封装保证回调链必终结）
        options.goal_response_callback = [this, actionName, goalId, userRespCb, userResultCb](
                                             typename GoalHandle::SharedPtr goalHandle)
        {
            if (goalHandle)
            {
                {
                    std::lock_guard<std::mutex> acceptLock(mtx_);
                    actionActiveGoals_[actionName][goalId] = goalHandle;
                }
                (*userRespCb)(goalId, true);
                return;
            }
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "callActionAsync [%s] failed: goal rejected by server", actionName.c_str());
            (*userRespCb)(goalId, false);
            (*userResultCb)(goalId, rclcpp_action::ResultCode::UNKNOWN, nullptr);
        };
        client->async_send_goal(goal, options);
        return goalId;
    }

    template <typename ActionT>
    bool ROS2Node::cancelGoal(const std::string &actionName, uint64_t goalId)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "cancelGoal [%s] failed: node not initialized", actionName.c_str());
            return false;
        }
        auto activeIt = actionActiveGoals_.find(actionName);
        if (activeIt == actionActiveGoals_.end())
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "cancelGoal [%s] failed: no active goal", actionName.c_str());
            return false;
        }
        auto goalIt = activeIt->second.find(goalId);
        if (goalIt == activeIt->second.end())
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "cancelGoal [%s] failed: goal not active", actionName.c_str());
            return false;
        }
        auto clientIt = actionClients_.find(actionName);
        auto typeIt = actionClientTypes_.find(actionName);
        if (clientIt == actionClients_.end() || typeIt == actionClientTypes_.end() ||
            typeIt->second != std::type_index(typeid(ActionT)))
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "cancelGoal [%s] failed: action type mismatch", actionName.c_str());
            return false;
        }
        auto client = std::static_pointer_cast<rclcpp_action::Client<ActionT>>(clientIt->second);
        auto goalHandle = std::static_pointer_cast<rclcpp_action::ClientGoalHandle<ActionT>>(goalIt->second);
        client->async_cancel_goal(goalHandle);
        return true;
    }

    template <typename ActionT>
    bool ROS2Node::cancelAllGoals(const std::string &actionName)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "cancelAllGoals [%s] failed: node not initialized", actionName.c_str());
            return false;
        }
        auto activeIt = actionActiveGoals_.find(actionName);
        if (activeIt == actionActiveGoals_.end() || activeIt->second.empty())
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "cancelAllGoals [%s] failed: no active goal", actionName.c_str());
            return false;
        }
        auto clientIt = actionClients_.find(actionName);
        auto typeIt = actionClientTypes_.find(actionName);
        if (clientIt == actionClients_.end() || typeIt == actionClientTypes_.end() ||
            typeIt->second != std::type_index(typeid(ActionT)))
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "cancelAllGoals [%s] failed: action type mismatch", actionName.c_str());
            return false;
        }
        auto client = std::static_pointer_cast<rclcpp_action::Client<ActionT>>(clientIt->second);
        client->async_cancel_all_goals();
        return true;
    }

    template <typename ActionT>
    bool ROS2Node::cancelGoalsBefore(const std::string &actionName, const rclcpp::Time &stamp)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "cancelGoalsBefore [%s] failed: node not initialized", actionName.c_str());
            return false;
        }
        auto activeIt = actionActiveGoals_.find(actionName);
        if (activeIt == actionActiveGoals_.end() || activeIt->second.empty())
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "cancelGoalsBefore [%s] failed: no active goal", actionName.c_str());
            return false;
        }
        auto clientIt = actionClients_.find(actionName);
        auto typeIt = actionClientTypes_.find(actionName);
        if (clientIt == actionClients_.end() || typeIt == actionClientTypes_.end() ||
            typeIt->second != std::type_index(typeid(ActionT)))
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "cancelGoalsBefore [%s] failed: action type mismatch", actionName.c_str());
            return false;
        }
        auto client = std::static_pointer_cast<rclcpp_action::Client<ActionT>>(clientIt->second);
        client->async_cancel_goals_before(stamp);
        return true;
    }

} // namespace yomk
