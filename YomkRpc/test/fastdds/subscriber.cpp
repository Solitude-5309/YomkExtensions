// Subscriber for FastDDS Hello World Test
// Uses MString type from YomkRpcMsg

#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <mutex>
#include <condition_variable>

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/subscriber/DataReader.hpp>
#include <fastdds/dds/subscriber/DataReaderListener.hpp>
#include <fastdds/dds/subscriber/Subscriber.hpp>
#include <fastdds/dds/subscriber/SampleInfo.hpp>
#include <fastdds/dds/subscriber/qos/DataReaderQos.hpp>
#include <fastdds/dds/subscriber/qos/SubscriberQos.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>
#include <fastdds/dds/core/status/SubscriptionMatchedStatus.hpp>

#include <YomkRpcMsg/YomkRpcMsgPubSubTypes.hpp>

using namespace eprosima::fastdds::dds;

// Listener callback class
class SubListener : public DataReaderListener
{
public:
    SubListener()
        : matched_(0), received_(0)
    {
    }

    void on_subscription_matched(
        DataReader * /*reader*/,
        const SubscriptionMatchedStatus &info) override
    {
        if (info.current_count_change == 1)
        {
            std::cout << "Subscriber matched." << std::endl;
            matched_++;
        }
        else if (info.current_count_change == -1)
        {
            std::cout << "Subscriber unmatched." << std::endl;
            matched_--;
        }
    }

    void on_data_available(
        DataReader *reader) override
    {
        SampleInfo info;
        while (RETCODE_OK == reader->take_next_sample(&data_, &info))
        {
            if (info.instance_state == ALIVE_INSTANCE_STATE && info.valid_data)
            {
                received_++;
                std::cout << "Received: " << data_.data() << std::endl;
                cv_.notify_one();
            }
        }
    }

    YomkRpc::MString data_;
    std::atomic<int> matched_;
    std::atomic<int> received_;
    std::mutex cv_mtx_;
    std::condition_variable cv_;
};

int main(int argc, char *argv[])
{
    std::cout << "=== FastDDS Subscriber (MString) ===" << std::endl;

    // Create participant
    DomainParticipant *participant = DomainParticipantFactory::get_instance()
                                         ->create_participant_with_default_profile(nullptr, StatusMask::none());
    if (participant == nullptr)
    {
        std::cerr << "Error creating participant" << std::endl;
        return -1;
    }

    // Register type using TypeSupport
    TypeSupport type(new YomkRpc::MStringPubSubType());
    type.register_type(participant);

    // Create topic
    Topic *topic = participant->create_topic("MStringTopic", type.get_type_name(), TOPIC_QOS_DEFAULT);
    if (topic == nullptr)
    {
        std::cerr << "Error creating topic" << std::endl;
        participant->delete_contained_entities();
        DomainParticipantFactory::get_instance()->delete_participant(participant);
        return -1;
    }

    // Create subscriber
    SubscriberQos sub_qos = SUBSCRIBER_QOS_DEFAULT;
    participant->get_default_subscriber_qos(sub_qos);
    Subscriber *subscriber = participant->create_subscriber(sub_qos, nullptr, StatusMask::none());
    if (subscriber == nullptr)
    {
        std::cerr << "Error creating subscriber" << std::endl;
        participant->delete_contained_entities();
        DomainParticipantFactory::get_instance()->delete_participant(participant);
        return -1;
    }

    // Create datareader with listener
    SubListener listener;
    DataReaderQos reader_qos = DATAREADER_QOS_DEFAULT;
    subscriber->get_default_datareader_qos(reader_qos);
    DataReader *reader = subscriber->create_datareader(topic, reader_qos, &listener, StatusMask::all());
    if (reader == nullptr)
    {
        std::cerr << "Error creating datareader" << std::endl;
        participant->delete_contained_entities();
        DomainParticipantFactory::get_instance()->delete_participant(participant);
        return -1;
    }

    std::cout << "Subscriber ready. Waiting for 10 messages..." << std::endl;

    // Wait until 10 messages received or timeout
    const int MAX_MESSAGES = 10;
    auto start = std::chrono::steady_clock::now();
    while (listener.received_ < MAX_MESSAGES)
    {
        auto elapsed = std::chrono::steady_clock::now() - start;
        if (std::chrono::duration_cast<std::chrono::seconds>(elapsed).count() > 30)
        {
            std::cout << "Timeout after 30 seconds." << std::endl;
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    std::cout << "\nTotal received: " << listener.received_.load() << " / " << MAX_MESSAGES << std::endl;
    std::cout << "Subscription complete." << std::endl;

    // Cleanup
    participant->delete_contained_entities();
    DomainParticipantFactory::get_instance()->delete_participant(participant);

    return 0;
}
