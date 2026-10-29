#include "otsdaq/GatewaySupervisor/MakeIteratorPlugin.h"
#include "otsdaq/GatewaySupervisor/IteratorPluginVInterface.h"

#include <cetlib/BasicPluginFactory.h>

std::unique_ptr<ots::IteratorPluginVInterface> ots::makeIteratorPlugin(
    const std::string& iteratorPluginName)
{
	static cet::BasicPluginFactory basicPluginInterfaceFactory("iteratorPlugin", "make");

	return basicPluginInterfaceFactory
	    .makePlugin<std::unique_ptr<ots::IteratorPluginVInterface>, const std::string&>(
	        iteratorPluginName, iteratorPluginName);
}
