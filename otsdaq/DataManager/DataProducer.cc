#include "otsdaq/DataManager/DataProducer.h"
#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/DataManager/DataManager.h"
#include "otsdaq/DataManager/DataManagerSingleton.h"

#include <iostream>
#include <memory>
using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "Producer"
#define mfSubject_ (std::string("Producer-") + bufferUID_ + "-" + processorUID_)

//==============================================================================
DataProducer::DataProducer(std::string supervisorApplicationUID, std::string bufferUID, std::string processorUID, unsigned int bufferSize)
    : WorkLoop(processorUID), DataProducerBase(supervisorApplicationUID, bufferUID, processorUID, bufferSize)
{
	__GEN_COUT__ << "Constructed." << __E__;
}

//==============================================================================
DataProducer::~DataProducer(void) { __GEN_COUT__ << "Destructed." << __E__; }

//==============================================================================
void DataProducer::startProcessingData(std::string /*runNumber*/)
{
	__GEN_COUT__ << "startWorkLoop..." << std::endl;
	WorkLoop::startWorkLoop();
}

//==============================================================================
void DataProducer::stopProcessingData(void)
{
	__GEN_COUT__ << "stopWorkLoop..." << std::endl;
	WorkLoop::stopWorkLoop();
}
