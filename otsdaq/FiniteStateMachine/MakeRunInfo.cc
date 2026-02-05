#include "otsdaq/FiniteStateMachine/MakeRunInfo.h"
#include <cetlib/BasicPluginFactory.h>

#include "otsdaq/FiniteStateMachine/RunInfoVInterface.h"

// clang-format off
ots::RunInfoVInterface* ots::makeRunInfo(const std::string& runInfoPluginClassName,
                                         const std::string& activeStateMachineName)
{
	static cet::BasicPluginFactory basicPluginInterfaceFactory("runInfo", "make");

	return basicPluginInterfaceFactory.makePlugin<
	    ots::RunInfoVInterface*,
	    const std::string&,
	    const std::string&>(  
			runInfoPluginClassName,			//run info plugin class name (used by plugin factory)
			//parameters to plugin constructor
			runInfoPluginClassName,			//run info plugin class name
			activeStateMachineName);  	   //activeStateMachineName
}
// clang-format on
