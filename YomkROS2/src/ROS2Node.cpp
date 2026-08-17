#include "ROS2Node.h"

namespace yomk
{

    ROS2Node::ROS2Node() = default;

    ROS2Node::~ROS2Node()
    {
        shutdown();
    }

    bool ROS2Node::init(int argc, char **argv, const std::string &nodeName)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (initialized_)
        {
            return false; // 重复初始化
        }

        // 进程级 rclcpp::init 仅一次；用户进程已初始化 ROS2 时跳过
        const bool needRclcppInit = !rclcpp::ok();
        if (needRclcppInit)
        {
            rclcpp::init(argc, argv);
        }

        node_ = std::make_shared<rclcpp::Node>(nodeName);
        // MultiThreadedExecutor：线程数默认 hardware_concurrency；
        // 配合 registerSubTopic 中每订阅独立的回调组，实现跨主题回调并发，
        // 慢/阻塞回调最多占用一个线程，不会饿死其他主题的回调。
        // 本接口仅初始化，spin 由 run() 启动
        executor_ = std::make_shared<rclcpp::executors::MultiThreadedExecutor>();
        executor_->add_node(node_);

        ownRclcppInit_ = needRclcppInit;
        initialized_ = true;
        return true;
    }

    bool ROS2Node::run(bool blocking)
    {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (!initialized_ || running_)
            {
                return false; // 未初始化或已在运行
            }
            running_ = true;
            if (!blocking)
            {
                spinThread_ = std::thread([this]()
                                          {
                                              executor_->spin();
                                              {
                                                  std::lock_guard<std::mutex> lock(mtx_);
                                                  running_ = false;
                                              }
                                              spinExitedCv_.notify_all(); });
                return true;
            }
        }
        // 阻塞模式：锁外在当前线程运行 spin（register/publish 均需要 mtx_），
        // shutdown() -> cancel() 后返回
        executor_->spin();
        {
            std::lock_guard<std::mutex> lock(mtx_);
            running_ = false;
        }
        spinExitedCv_.notify_all();
        return true;
    }

    bool ROS2Node::shutdown()
    {
        std::thread threadToJoin;
        std::shared_ptr<rclcpp::Executor> executor;
        std::shared_ptr<rclcpp::Node> node;
        bool ownRclcppInit = false;
        {
            std::lock_guard<std::mutex> lock(mtx_);
            if (!initialized_)
            {
                return true; // 未初始化或已销毁
            }
            initialized_ = false;
            ownRclcppInit = ownRclcppInit_;
            ownRclcppInit_ = false;
            executor = executor_;
            node = node_;
            executor_.reset();
            node_.reset();
            threadToJoin = std::move(spinThread_);
        }

        // 停止 spin（后台线程或阻塞中的当前线程）：cancel 后等待 running_ 清零，
        // 确保无线程仍在使用 executor 后再销毁它（阻塞模式下 spin 运行在调用方线程，
        // 没有可 join 的线程，必须显式等待 spin 返回）
        if (executor)
        {
            executor->cancel();
        }
        {
            std::unique_lock<std::mutex> lock(mtx_);
            spinExitedCv_.wait(lock, [this]()
                               { return !running_; });
        }
        if (threadToJoin.joinable())
        {
            threadToJoin.join();
        }

        // MultiThreadedExecutor 内置工作线程池，spin 主循环退出后工作线程可能仍在运行；
        // 销毁 executor 触发其析构 join 全部工作线程，之后清理实体才是安全的。
        // 注意：此处不调用 executor->remove_node——独立回调组是 spin 期间被 executor
        // 内存策略动态发现的，并未正式关联到 executor，remove_node 遍历节点全部
        // 回调组逐一移除时会抛出 "Callback group needs to be associated with executor"。
        // executor 随后即将销毁，无需从其中移除节点。
        executor.reset();

        {
            std::lock_guard<std::mutex> lock(mtx_);
            subTopics_.clear(); // 先销毁订阅，再销毁发布
            pubTopics_.clear();
            subGroups_.clear();
            subTypes_.clear();
            pubTypes_.clear();
            paramClients_.clear();
            paramCallbacks_.clear();
            serviceServers_.clear(); // 先销毁服务端，再销毁客户端（与订阅/发布同序）
            serviceGroups_.clear();
            serviceClients_.clear();
            serviceClientTypes_.clear();
            serviceClientGroups_.clear();
        }

        // 仅销毁本实例创建的进程级 ROS2 上下文，不干扰用户自行初始化的环境
        if (ownRclcppInit && rclcpp::ok())
        {
            rclcpp::shutdown();
        }
        return true;
    }

    bool ROS2Node::isInitialized() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        return initialized_;
    }

    // ---- 参数接口非模板方法实现 ----

    bool ROS2Node::hasParam(const std::string &name) const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "hasParam [%s] failed: node not initialized", name.c_str());
            return false;
        }
        return node_->has_parameter(name);
    }

    bool ROS2Node::undeclareParam(const std::string &name)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "undeclareParam [%s] failed: node not initialized", name.c_str());
            return false;
        }
        if (!node_->has_parameter(name))
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "undeclareParam [%s] failed: parameter not declared", name.c_str());
            return false;
        }
        try
        {
            node_->undeclare_parameter(name);
        }
        catch (const std::exception &e)
        {
            // 数组类型参数声明后为静态类型，rclcpp 不允许撤销声明
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "undeclareParam [%s] failed: %s", name.c_str(), e.what());
            return false;
        }
        return true;
    }

    std::vector<std::string> ROS2Node::listParams() const
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "listParams failed: node not initialized");
            return {};
        }
        return node_->list_parameters({}, 0).names;
    }

    uint64_t ROS2Node::addOnSetParamCallback(std::function<bool(const std::vector<rclcpp::Parameter> &)> cb)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "addOnSetParamCallback failed: node not initialized");
            return 0;
        }
        const uint64_t id = paramCallbackNextId_++;
        auto handle = node_->add_on_set_parameters_callback(
            [cb](const std::vector<rclcpp::Parameter> &params)
            {
                rcl_interfaces::msg::SetParametersResult result;
                result.successful = cb(params);
                if (!result.successful)
                {
                    result.reason = "rejected by callback";
                }
                return result;
            });
        paramCallbacks_[id] = handle;
        return id;
    }

    void ROS2Node::removeOnSetParamCallback(uint64_t handleId)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = paramCallbacks_.find(handleId);
        if (it == paramCallbacks_.end())
        {
            return;
        }
        if (initialized_)
        {
            node_->remove_on_set_parameters_callback(it->second.get());
        }
        paramCallbacks_.erase(it);
    }

    bool ROS2Node::hasRemoteParam(const std::string &remoteNodeName, const std::string &name)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "hasRemoteParam [%s/%s] failed: node not initialized", remoteNodeName.c_str(), name.c_str());
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
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "hasRemoteParam [%s/%s] failed: timeout waiting for response", remoteNodeName.c_str(), name.c_str());
            return false;
        }
        const auto params = future.get();
        return params.size() == 1 && params[0].get_type() != rclcpp::ParameterType::PARAMETER_NOT_SET;
    }

    std::vector<std::string> ROS2Node::listRemoteParams(const std::string &remoteNodeName)
    {
        std::lock_guard<std::mutex> lock(mtx_);
        if (!initialized_)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "listRemoteParams [%s] failed: node not initialized", remoteNodeName.c_str());
            return {};
        }
        auto client = getOrCreateParamClient(remoteNodeName);
        if (!client)
        {
            return {};
        }
        // 异步请求 + future 同步等待：响应由本节点自身的 spin 处理（调用前需已 run）
        auto future = client->list_parameters({}, 0);
        if (future.wait_for(std::chrono::seconds(1)) != std::future_status::ready)
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "listRemoteParams [%s] failed: timeout waiting for response", remoteNodeName.c_str());
            return {};
        }
        return future.get().names;
    }

    std::shared_ptr<rclcpp::AsyncParametersClient> ROS2Node::getOrCreateParamClient(const std::string &remoteNodeName)
    {
        auto it = paramClients_.find(remoteNodeName);
        if (it != paramClients_.end())
        {
            return it->second;
        }
        // 异步客户端不 spin 也不占用节点 executor（与 SyncParametersClient 不同），
        // 响应由本节点自身的 spin 处理，故调用方需已 run 运行
        auto client = std::make_shared<rclcpp::AsyncParametersClient>(node_, remoteNodeName);
        if (!client->wait_for_service(std::chrono::milliseconds(1000)))
        {
            RCLCPP_ERROR(rclcpp::get_logger("YomkROS2"), "param client [%s] failed: service not available", remoteNodeName.c_str());
            return nullptr;
        }
        paramClients_[remoteNodeName] = client;
        return client;
    }

} // namespace yomk
