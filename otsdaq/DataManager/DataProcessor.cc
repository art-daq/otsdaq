#include "otsdaq/DataManager/DataProcessor.h"

using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "Processor"
#define mfSubject_ (std::string("Processor:") + DataProcessor::processorUID_)

//==============================================================================
DataProcessor::DataProcessor(std::string supervisorApplicationUID, std::string bufferUID, std::string processorUID)
    : supervisorApplicationUID_(supervisorApplicationUID), bufferUID_(bufferUID), processorUID_(processorUID), theCircularBuffer_(nullptr)
{
	__GEN_COUT__ << "Constructor." << __E__;
	__GEN_COUTV__(supervisorApplicationUID_);
	__GEN_COUTV__(bufferUID_);
	__GEN_COUTV__(processorUID_);

	__GEN_COUT__ << "Constructed." << __E__;
}

//==============================================================================
DataProcessor::~DataProcessor(void) { __GEN_COUT__ << "Destructed." << __E__; }

//==============================================================================
void DataProcessor::setCircularBuffer(CircularBufferBase* circularBuffer) { theCircularBuffer_ = circularBuffer; }
