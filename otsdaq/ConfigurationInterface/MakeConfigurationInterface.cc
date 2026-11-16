#include "otsdaq/Macros/PluginFactory.h"  // cet::BasicPluginFactory, or dlopen in maxlite

#include "otsdaq/ConfigurationInterface/ConfigurationInterface.h"
#include "otsdaq/ConfigurationInterface/MakeConfigurationInterface.h"

namespace ots
{
ConfigurationInterface* makeConfigurationInterface(std::string const& interfacePluginName)
{
	static ots::PluginFactory basicPluginInterfaceFactory("configInterface", "make");

	return basicPluginInterfaceFactory.makePlugin<ConfigurationInterface*>(
	    interfacePluginName);
}
}  // namespace ots
