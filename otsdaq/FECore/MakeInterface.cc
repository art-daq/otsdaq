#include "otsdaq/FECore/MakeInterface.h"
#include "otsdaq/FECore/FEVInterface.h"

#include "otsdaq/Macros/PluginFactory.h"  // cet::BasicPluginFactory, or dlopen in maxlite

std::unique_ptr<ots::FEVInterface> ots::makeInterface(
    const std::string&            interfacePluginName,
    const std::string&            interfaceUID,
    const ots::ConfigurationTree& configuration,
    const std::string&            pathToInterfaceConfiguration)

{
	static ots::PluginFactory basicPluginInterfaceFactory("interface", "make");

	return basicPluginInterfaceFactory.makePlugin<std::unique_ptr<ots::FEVInterface>,
	                                              const std::string&,
	                                              const ots::ConfigurationTree&,
	                                              const std::string&>(
	    interfacePluginName, interfaceUID, configuration, pathToInterfaceConfiguration);
}
