#pragma once

#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <typeindex>
#include <utility>

#include <rclcpp/rclcpp.hpp>

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

    private:
        std::shared_ptr<rclcpp::Node> node_;
        std::shared_ptr<rclcpp::Executor> executor_; // MultiThreadedExecutor
        std::thread spinThread_;
        std::map<std::string, std::shared_ptr<rclcpp::PublisherBase>> pubTopics_;
        std::map<std::string, std::shared_ptr<rclcpp::SubscriptionBase>> subTopics_;
        std::map<std::string, std::shared_ptr<rclcpp::CallbackGroup>> subGroups_; // 持有各订阅的独立回调组（节点仅存弱引用）
        std::map<std::string, std::type_index> pubTypes_;
        std::map<std::string, std::type_index> subTypes_;
        mutable std::mutex mtx_;
        std::condition_variable spinExitedCv_; // spin 退出（后台线程或阻塞线程）后通知 shutdown
        bool initialized_ = false;
        bool running_ = false;       // run 已启动 spin（后台线程或当前线程阻塞中）
        bool ownRclcppInit_ = false; // 本实例执行过 rclcpp::init，shutdown 时对称调用 rclcpp::shutdown
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

} // namespace yomk
