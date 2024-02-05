#include "otsdaq/DataManager/CircularBufferBase.h"
#include "otsdaq/DataManager/DataConsumer.h"
#include "otsdaq/DataManager/DataProducer.h"

using namespace ots;

//==============================================================================
CircularBufferBase::CircularBufferBase(const std::string& bufferID) : dataBufferId_(bufferID), mfSubject_("CircularBuffer:" + dataBufferId_) {}

//==============================================================================
CircularBufferBase::~CircularBufferBase(void) {}

//==============================================================================
void CircularBufferBase::registerProducer(DataProcessor* producer, unsigned int numberOfSubBuffers)
{
	registerProducer(producer->getProcessorID(), numberOfSubBuffers);
	producer->setCircularBuffer(this);
}

//==============================================================================
void CircularBufferBase::registerConsumer(DataProcessor* consumer)
{
	registerConsumer(consumer->getProcessorID(), HighConsumerPriority);
	consumer->setCircularBuffer(this);
}
//
////==============================================================================
// void CircularBufferBase::unregisterProducer(DataProcessor* producer)
//{
//	unregisterProducer(producer->getProcessorID());
//	producer->setCircularBuffer(0);
//}
//
////==============================================================================
// void CircularBufferBase::unregisterConsumer(DataProcessor* consumer)
//{
//	unregisterConsumer(consumer->getProcessorID());
//	consumer->setCircularBuffer(0);
//}
