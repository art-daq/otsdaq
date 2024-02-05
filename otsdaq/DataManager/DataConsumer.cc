#include "otsdaq/DataManager/DataConsumer.h"

#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/DataManager/DataManager.h"
#include "otsdaq/DataManager/DataManagerSingleton.h"

using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "Consumer"
#define mfSubject_ (std::string("Consumer:") + DataProcessor::processorUID_)

//==============================================================================
DataConsumer::DataConsumer(std::string supervisorApplicationUID, std::string bufferUID, std::string processorUID, ConsumerPriority priority)
    : WorkLoop(processorUID), DataProcessor(supervisorApplicationUID, bufferUID, processorUID), priority_(priority)
{
	__GEN_COUT__ << "Constructor." << __E__;
	registerToBuffer();
	__GEN_COUT__ << "Constructed." << __E__;
}

//==============================================================================
DataConsumer::~DataConsumer(void)
{
	__GEN_COUT__ << "Destructor." << __E__;
	// unregisterFromBuffer();
	__GEN_COUT__ << "Destructed." << __E__;
}

//==============================================================================
DataConsumer::ConsumerPriority DataConsumer::getPriority(void) { return priority_; }

//==============================================================================
// mirror DataProducerBase::registerToBuffer
void DataConsumer::registerToBuffer(void)
{
	__GEN_COUT__ << "Consumer '" << DataProcessor::processorUID_ << "' is registering to DataManager Supervisor Buffer '"
	             << DataProcessor::supervisorApplicationUID_ << ":" << DataProcessor::bufferUID_ << ".'" << std::endl;

	DataManager* dataManager = (DataManagerSingleton::getInstance(supervisorApplicationUID_));

	dataManager->registerConsumer(bufferUID_, this);

	{
		__GEN_SS__;
		dataManager->dumpStatus(&ss);
		std::cout << ss.str() << __E__;
	}

	__GEN_COUT__ << "Consumer '" << DataProcessor::processorUID_ << "' Registered." << __E__;

	//
	//
	//	__GEN_COUT__ << "Registering to DataManager Supervisor '" <<
	//			DataProcessor::supervisorApplicationUID_ << "' and buffer '" <<
	//			DataProcessor::bufferUID_ << "'" << std::endl;
	//
	//	(DataManagerSingleton::getInstance(
	//			supervisorApplicationUID_))->registerConsumer(
	//					bufferUID_,this);
	//
	//	__GEN_COUT__ << "Registered." << __E__;
}  // end registerToBuffer()

////==============================================================================
////mirror DataProducerBase::unregisterFromBuffer
// void DataConsumer::unregisterFromBuffer(void)
//{
//	__GEN_COUT__ << "Consumer '" << DataProcessor::processorUID_ <<
//			"' is unregistering to DataManager Supervisor Buffer '" <<
//			DataProcessor::supervisorApplicationUID_ << ":" <<
//			DataProcessor::bufferUID_ << ".'" << std::endl;
//
//	DataManager* dataManager =
//		(DataManagerSingleton::getInstance(
//				supervisorApplicationUID_));
//
//	dataManager->unregisterConsumer(
//						bufferUID_,DataProcessor::processorUID_);
//
//	{
//		__GEN_SS__;
//		dataManager->dumpStatus(&ss);
//		std::cout << ss.str() << __E__;
//	}
//
//	__GEN_COUT__ << "Consumer '" << DataProcessor::processorUID_ <<
//			"' Unregistered." << __E__;
//} //end unregisterFromBuffer()

//==============================================================================
void DataConsumer::startProcessingData(std::string /*runNumber*/) { WorkLoop::startWorkLoop(); }

//==============================================================================
void DataConsumer::stopProcessingData(void) { WorkLoop::stopWorkLoop(); }
