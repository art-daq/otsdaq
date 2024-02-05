#include "otsdaq/DataProcessorPlugins/RawDataVisualizerConsumer.h"

#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

using namespace ots;

//==============================================================================
RawDataVisualizerConsumer::RawDataVisualizerConsumer(std::string              supervisorApplicationUID,
                                                     std::string              bufferUID,
                                                     std::string              processorUID,
                                                     const ConfigurationTree& theXDAQContextConfigTree,
                                                     const std::string&       configurationPath)
    : WorkLoop(processorUID)
    , DataConsumer(supervisorApplicationUID, bufferUID, processorUID, LowConsumerPriority)
    , Configurable(theXDAQContextConfigTree, configurationPath)
{
}

//==============================================================================
RawDataVisualizerConsumer::~RawDataVisualizerConsumer(void) {}

//==============================================================================
void RawDataVisualizerConsumer::startProcessingData(std::string runNumber) { DataConsumer::startProcessingData(runNumber); }

//==============================================================================
void RawDataVisualizerConsumer::stopProcessingData(void) { DataConsumer::stopProcessingData(); }

//==============================================================================
bool RawDataVisualizerConsumer::workLoopThread(toolbox::task::WorkLoop* /*workLoop*/)
{
	__COUT__ << DataProcessor::processorUID_ << " running, because workloop: " << WorkLoop::continueWorkLoop_ << std::endl;
	slowRead();  // fastRead();
	return WorkLoop::continueWorkLoop_;
}

//==============================================================================
void RawDataVisualizerConsumer::fastRead(void)
{
	//__COUT__ << processorUID_ << " running!" << std::endl;
	// This is making a copy!!!
	if(DataConsumer::read(dataP_, headerP_) < 0)
	{
		usleep(100);
		return;
	}
	__COUT__ << DataProcessor::processorUID_ << " UID: " << supervisorApplicationUID_ << std::endl;

	//	//HW emulator
	//	//	 Burst Type | Sequence | 8B data
	//	__COUT__ << "Size fill: " << dataP_->length() << std::endl;
	//	dqmHistos_->fill(*dataP_,*headerP_);

	DataConsumer::setReadSubBuffer<std::string, std::map<std::string, std::string>>();
}

//==============================================================================
void RawDataVisualizerConsumer::slowRead(void)
{
	// This is making a copy!!!
	if(DataConsumer::read(data_, header_) < 0)
	{
		usleep(1000);
		return;
	}
	__MOUT__ << DataProcessor::processorUID_ << " UID: " << supervisorApplicationUID_ << std::endl;
}
