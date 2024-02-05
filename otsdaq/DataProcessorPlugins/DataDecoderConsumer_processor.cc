#include "otsdaq/DataProcessorPlugins/DataDecoderConsumer.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/ProcessorPluginMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"
#include "otsdaq/TablePlugins/DataDecoderConsumerTable.h"

#include <unistd.h>
#include <cassert>
#include <iostream>

using namespace ots;

//==============================================================================
DataDecoderConsumer::DataDecoderConsumer(std::string              supervisorApplicationUID,
                                         std::string              bufferUID,
                                         std::string              processorUID,
                                         const ConfigurationTree& theXDAQContextConfigTree,
                                         const std::string&       configurationPath)
    : WorkLoop(processorUID)
    , DataDecoder(supervisorApplicationUID, bufferUID, processorUID)
    , DataConsumer(supervisorApplicationUID, bufferUID, processorUID, HighConsumerPriority)
    , Configurable(theXDAQContextConfigTree, configurationPath)
{
}

//==============================================================================
DataDecoderConsumer::~DataDecoderConsumer(void) {}

//==============================================================================
bool DataDecoderConsumer::workLoopThread(toolbox::task::WorkLoop* workLoop)
{
	// std::cout << __COUT_HDR_FL__ << __PRETTY_FUNCTION__ << processorUID_ << " running!"
	// << std::endl;
	std::string                        buffer;
	std::map<std::string, std::string> header;
	// unsigned long block;
	if(DataConsumer::read(buffer, header) < 0)
		usleep(100000);
	else
	{
		std::cout << __COUT_HDR_FL__ << __PRETTY_FUNCTION__ << processorUID_ << " Buffer: " << buffer << std::endl;
	}
	return true;
}

DEFINE_OTS_PROCESSOR(DataDecoderConsumer)
