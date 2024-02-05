#include "otsdaq/DataManager/MakeDataProcessor.h"
#include "otsdaq/DataManager/DataProcessor.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <cetlib/BasicPluginFactory.h>

ots::DataProcessor* ots::makeDataProcessor(std::string const&            processorPluginName,
                                           std::string const&            supervisorApplicationUID,
                                           std::string const&            bufferUID,
                                           std::string const&            processorUID,
                                           ots::ConfigurationTree const& configurationTree,
                                           std::string const&            pathToInterfaceConfiguration)
{
	static cet::BasicPluginFactory basicPluginInterfaceFactory("processor", "make");

	return basicPluginInterfaceFactory
	    .makePlugin<ots::DataProcessor*, std::string const&, std::string const&, std::string const&, ots::ConfigurationTree const&, std::string const&>(
	        processorPluginName, supervisorApplicationUID, bufferUID, processorUID, configurationTree, pathToInterfaceConfiguration);
}
