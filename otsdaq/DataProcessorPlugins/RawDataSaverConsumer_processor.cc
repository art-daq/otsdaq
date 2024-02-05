#include "otsdaq/DataProcessorPlugins/RawDataSaverConsumer.h"
#include "otsdaq/Macros/ProcessorPluginMacros.h"

using namespace ots;

//==============================================================================
RawDataSaverConsumer::RawDataSaverConsumer(std::string              supervisorApplicationUID,
                                           std::string              bufferUID,
                                           std::string              processorUID,
                                           const ConfigurationTree& theXDAQContextConfigTree,
                                           const std::string&       configurationPath)
    : WorkLoop(processorUID), RawDataSaverConsumerBase(supervisorApplicationUID, bufferUID, processorUID, theXDAQContextConfigTree, configurationPath)
{
}

//==============================================================================
RawDataSaverConsumer::~RawDataSaverConsumer(void) {}

DEFINE_OTS_PROCESSOR(RawDataSaverConsumer)
