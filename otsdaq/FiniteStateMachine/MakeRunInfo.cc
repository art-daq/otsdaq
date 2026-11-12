#include "otsdaq/FiniteStateMachine/MakeRunInfo.h"
#include "otsdaq/Macros/PluginFactory.h"  // cet::BasicPluginFactory, or dlopen in maxlite

#include "otsdaq/FiniteStateMachine/RunInfoVInterface.h"

// clang-format off
ots::RunInfoVInterface* ots::makeRunInfo(const std::string& runInfoPluginClassName,
										 const std::string& activeStateMachineName)
{
	static ots::PluginFactory basicPluginInterfaceFactory("runInfo", "make");

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
