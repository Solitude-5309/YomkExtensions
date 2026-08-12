#ifndef FASTDDSNODE_H
#define FASTDDSNODE_H

#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>

#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/topic/Topic.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>

/**
 * @brief FastDDS 节点管理类
 *
 * 封装 DDS 参与者/发布者/订阅者的生命周期管理，支持动态注册主题和发布数据。
 * 全部接口不使用模板，数据类型和数据包均通过 void* 传递；
 * 类型参数要求传入 fastddsgen 从 IDL 生成的 PubSubType 实例（如 new MStringPubSubType()），
 * 实现内部转换为 TopicDataType* 交给 TypeSupport 管理。
 */
class FastDDSNode
{
public:
    // 订阅回调，参数为注册时传入的数据包指针
    using DataCallback = std::function<void(const void *)>;

    FastDDSNode();
    // 声明于头文件、实现于 cpp，以保证 SubListener 不完整类型下
    // unique_ptr<SubListener> 成员能正确析构
    ~FastDDSNode();

    FastDDSNode(const FastDDSNode &) = delete;
    FastDDSNode &operator=(const FastDDSNode &) = delete;

    /**
     * @brief 设置域 ID，创建参与者（含发布者/订阅者）
     * @param domainId DDS 域 ID
     * @return 成功返回 true
     */
    bool setDomainId(uint32_t domainId);

    /**
     * @brief 注册订阅主题
     * @param topicName 主题名
     * @param type IDL 生成的 PubSubType 实例（如 new MStringPubSubType()），
     *             以 void* 传入，实现内部转换为 TopicDataType*，所有权交给本类
     * @param data 接收用数据包（指向对应数据类型实例的指针），生命周期需覆盖订阅期
     * @param callback 收到数据时以 data 指针回调
     * @return 成功返回 true
     */
    bool registerSubTopic(const std::string &topicName,
                          void *type,
                          void *data,
                          DataCallback callback);

    /**
     * @brief 注册发布主题
     * @param topicName 主题名
     * @param type IDL 生成的 PubSubType 实例，以 void* 传入，所有权交给本类
     * @return 成功返回 true
     */
    bool registerPubTopic(const std::string &topicName,
                          void *type);

    /**
     * @brief 发布数据
     * @param topicName 主题名
     * @param data 数据指针，必须为该主题注册时对应数据类型的指针
     * @return 成功返回 true
     */
    bool publish(const std::string &topicName, const void *data);

private:
    // 订阅监听器（非模板，使用注册时传入的数据包缓冲）
    class SubListener;

    // 获取或创建主题：已存在则直接复用（校验类型名一致），不存在则创建并记录
    // 调用前需持有 mtx_
    eprosima::fastdds::dds::Topic *getOrCreateTopic(const std::string &topicName,
                                                    const std::string &typeName);

    struct PubInfo
    {
        eprosima::fastdds::dds::TypeSupport type;
        eprosima::fastdds::dds::DataWriter *writer = nullptr;
    };
    struct SubInfo
    {
        eprosima::fastdds::dds::TypeSupport type;
        eprosima::fastdds::dds::DataReader *reader = nullptr;
        std::unique_ptr<SubListener> listener;
    };

    eprosima::fastdds::dds::DomainParticipant *participant_ = nullptr;
    eprosima::fastdds::dds::Publisher *publisher_ = nullptr;
    eprosima::fastdds::dds::Subscriber *subscriber_ = nullptr;
    // 全量主题表：发布/订阅共用同名主题时不重复创建
    std::map<std::string, eprosima::fastdds::dds::Topic *> topics_;
    std::map<std::string, PubInfo> pubTopics_;
    std::map<std::string, SubInfo> subTopics_;
    std::mutex mtx_;
};

#endif // FASTDDSNODE_H
