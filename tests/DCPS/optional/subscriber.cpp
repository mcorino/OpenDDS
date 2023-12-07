// -*- C++ -*-
#include "optionalC.h"
#include "optionalTypeSupportImpl.h"

#include <ace/Assert.h>
#include <tests/Utils/DistributedConditionSet.h>
#include <tests/Utils/StatusMatching.h>

#include <dds/DCPS/Service_Participant.h>
#include <dds/DCPS/Marked_Default_Qos.h>

int ACE_TMAIN(int argc, ACE_TCHAR* argv[])
{
  // Production apps should check the return values.
  // For tests, it just makes the code noisy.

  // Coordination across processes.
  DistributedConditionSet_rch distributed_condition_set =
    OpenDDS::DCPS::make_rch<FileBasedDistributedConditionSet>();

  DDS::DomainParticipantFactory_var domain_participant_factory = TheParticipantFactoryWithArgs(argc, argv);
  DDS::DomainParticipant_var participant =
    domain_participant_factory->create_participant(OptionalTest::OPTIONAL_DOMAIN,
                                                   PARTICIPANT_QOS_DEFAULT,
                                                   0,
                                                   0);
  ACE_ASSERT(participant != 0);

  OptionalTest::MessageTypeSupport_var type_support = new OptionalTest::MessageTypeSupportImpl;
  type_support->register_type(participant, "");
  CORBA::String_var type_name = type_support->get_type_name();

  DDS::Topic_var topic = participant->create_topic(OptionalTest::MESSAGE_TOPIC_NAME.c_str(),
                                                   type_name,
                                                   TOPIC_QOS_DEFAULT,
                                                   0,
                                                   0);

  DDS::Subscriber_var subscriber = participant->create_subscriber(SUBSCRIBER_QOS_DEFAULT,
                                                                  0,
                                                                  0);

  DDS::DataReaderQos data_reader_qos;
  subscriber->get_default_datareader_qos(data_reader_qos);
  data_reader_qos.reliability.kind = DDS::RELIABLE_RELIABILITY_QOS;

  DDS::DataReader_var data_reader = subscriber->create_datareader(topic,
                                                                  data_reader_qos,
                                                                  0,
                                                                  0);

  OptionalTest::MessageDataReader_var message_data_reader = OptionalTest::MessageDataReader::_narrow(data_reader);

  Utils::wait_match(data_reader, 1);
  distributed_condition_set->post(OptionalTest::SUBSCRIBER, OptionalTest::SUBSCRIBER_READY);

  DDS::ReadCondition_var read_condition =
    data_reader->create_readcondition(DDS::ANY_SAMPLE_STATE, DDS::ANY_VIEW_STATE, DDS::ANY_INSTANCE_STATE);

  DDS::WaitSet_var wait_set = new DDS::WaitSet;
  wait_set->attach_condition(read_condition);

  bool done = false;
  while (!done) {
    DDS::ConditionSeq conditions;
    const DDS::Duration_t timeout = { 1, 0 };
    wait_set->wait(conditions, timeout);

    OptionalTest::MessageSeq messages;
    DDS::SampleInfoSeq infos;
    message_data_reader->take(messages, infos, DDS::LENGTH_UNLIMITED,
                              DDS::ANY_SAMPLE_STATE, DDS::ANY_VIEW_STATE, DDS::ANY_INSTANCE_STATE);
    for (unsigned int idx = 0; idx != messages.length(); ++idx) {
      if (infos[idx].valid_data) {
        const std::optional<std::string> opt = messages[idx].value();
        ACE_ASSERT(opt.has_value());
        ACE_DEBUG((LM_DEBUG, "received %C\n", opt ? opt->c_str() : "nullopt"));
        distributed_condition_set->post(OptionalTest::SUBSCRIBER, OptionalTest::SUBSCRIBER_DONE);
        done = true;
      }
    }
  }

  wait_set->detach_condition(read_condition);

  participant->delete_contained_entities();
  domain_participant_factory->delete_participant(participant);
  TheServiceParticipant->shutdown();

  return 0;
}
