#include <cetlib/BasicPluginFactory.h>

#include "otsdaq/ConfigurationInterface/ConfigurationInterface.h"
#include "otsdaq/ConfigurationInterface/MakeConfigurationInterface.h"

namespace ots
{
ConfigurationInterface* makeConfigurationInterface(std::string const& interfacePluginName)
{
	static cet::BasicPluginFactory basicPluginInterfaceFactory("configInterface", "make");

	return basicPluginInterfaceFactory.makePlugin<ConfigurationInterface*>(interfacePluginName);
}
}  // namespace ots
