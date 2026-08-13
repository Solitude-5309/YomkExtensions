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
        // 慢/阻塞回调最多占用一个线程，不会饿死其他主题的回调
        executor_ = std::make_shared<rclcpp::executors::MultiThreadedExecutor>();
        executor_->add_node(node_);
        spinThread_ = std::thread([this]()
                                  { executor_->spin(); });

        ownRclcppInit_ = needRclcppInit;
        initialized_ = true;
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

        // 停止 spin 主循环线程并等待退出
        if (executor)
        {
            executor->cancel();
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

} // namespace yomk
