#include "otsdaq/GatewaySupervisor/MakeIteratorPlugin.h"
#include "otsdaq/GatewaySupervisor/IteratorPluginVInterface.h"

#include "otsdaq/Macros/PluginFactory.h"  // cet::BasicPluginFactory, or dlopen in maxlite

std::unique_ptr<ots::IteratorPluginVInterface> ots::makeIteratorPlugin(
    const std::string& iteratorPluginName)
{
	static ots::PluginFactory basicPluginInterfaceFactory("iteratorPlugin", "make");

	return basicPluginInterfaceFactory
	    .makePlugin<std::unique_ptr<ots::IteratorPluginVInterface>, const std::string&>(
	        iteratorPluginName, iteratorPluginName);
}
