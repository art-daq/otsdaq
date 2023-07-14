#include "otsdaq/FiniteStateMachine/MakeRunInfo.h"
#include <cetlib/BasicPluginFactory.h>

#include "otsdaq/FiniteStateMachine/RunInfoVInterface.h"

ots::RunInfoVInterface* ots::makeRunInfo(const std::string& runInfoPluginName, const std::string& runInfoUID)
//    ,               // Key value for (eventual) RunInfoDashboard
//                                                      // Table in Configuration
//    const ots::ConfigurationTree& configurationTree,  // Pass the big tree
//    const std::string&            pathToControlsConfiguration)   // Path to RunInfoDashboard Table
{
	static cet::BasicPluginFactory basicPluginInterfaceFactory("runInfo", "make");

	return basicPluginInterfaceFactory
	    .makePlugin<ots::RunInfoVInterface*, const std::string&, const std::string&>(  //, const ots::ConfigurationTree&, const std::string&>(
	        runInfoPluginName,
	        runInfoPluginName,
	        runInfoUID);  //, configurationTree, pathToControlsConfiguration);
}
