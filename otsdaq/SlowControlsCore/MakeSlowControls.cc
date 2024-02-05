#include "otsdaq/SlowControlsCore/MakeSlowControls.h"
#include <cetlib/BasicPluginFactory.h>

#include "otsdaq/SlowControlsCore/SlowControlsVInterface.h"

ots::SlowControlsVInterface* ots::makeSlowControls(const std::string& slowControlsPluginName,
                                                   const std::string& slowControlsUID,               // Key value for (eventual) SlowControlsDashboard
                                                                                                     // Table in Configuration
                                                   const ots::ConfigurationTree& configurationTree,  // Pass the big tree
                                                   const std::string&            pathToControlsConfiguration)   // Path to SlowControlsDashboard Table
{
	static cet::BasicPluginFactory basicPluginInterfaceFactory("slowcontrols", "make");

	return basicPluginInterfaceFactory
	    .makePlugin<ots::SlowControlsVInterface*, const std::string&, const std::string&, const ots::ConfigurationTree&, const std::string&>(
	        slowControlsPluginName, slowControlsPluginName, slowControlsUID, configurationTree, pathToControlsConfiguration);
}
