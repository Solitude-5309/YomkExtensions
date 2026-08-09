// Publisher for FastDDS Hello World Test
// Uses MString type from YomkRpcMsg

#include <iostream>
#include <string>
#include <thread>
#include <chrono>

#include <fastdds/dds/domain/DomainParticipantFactory.hpp>
#include <fastdds/dds/domain/DomainParticipant.hpp>
#include <fastdds/dds/publisher/DataWriter.hpp>
#include <fastdds/dds/publisher/Publisher.hpp>
#include <fastdds/dds/publisher/qos/DataWriterQos.hpp>
#include <fastdds/dds/publisher/qos/PublisherQos.hpp>
#include <fastdds/dds/topic/TypeSupport.hpp>

#include <YomkRpcMsg/YomkRpcMsgPubSubTypes.hpp>

using namespace eprosima::fastdds::dds;

int main(int argc, char *argv[])
{
    std::cout << "=== FastDDS Publisher (MString) ===" << std::endl;

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

    // Create publisher
    PublisherQos pub_qos = PUBLISHER_QOS_DEFAULT;
    participant->get_default_publisher_qos(pub_qos);
    Publisher *publisher = participant->create_publisher(pub_qos, nullptr, StatusMask::none());
    if (publisher == nullptr)
    {
        std::cerr << "Error creating publisher" << std::endl;
        participant->delete_contained_entities();
        DomainParticipantFactory::get_instance()->delete_participant(participant);
        return -1;
    }

    // Create datawriter
    DataWriterQos writer_qos = DATAWRITER_QOS_DEFAULT;
    publisher->get_default_datawriter_qos(writer_qos);
    DataWriter *writer = publisher->create_datawriter(topic, writer_qos, nullptr, StatusMask::none());
    if (writer == nullptr)
    {
        std::cerr << "Error creating datawriter" << std::endl;
        participant->delete_contained_entities();
        DomainParticipantFactory::get_instance()->delete_participant(participant);
        return -1;
    }

    std::cout << "Publisher ready. Sending 10 messages..." << std::endl;

    // Send messages
    YomkRpc::MString data;
    for (int i = 0; i < 10; ++i)
    {
        data.data("Hello World " + std::to_string(i));

        if (writer->write(&data) == RETCODE_OK)
        {
            std::cout << "Sent: " << data.data() << std::endl;
        }
        else
        {
            std::cerr << "Error writing message" << std::endl;
        }

        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "\nPublishing complete." << std::endl;

    // Cleanup
    participant->delete_contained_entities();
    DomainParticipantFactory::get_instance()->delete_participant(participant);

    return 0;
}
