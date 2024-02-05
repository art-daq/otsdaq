#include "otsdaq/PluginMakers/MakeRunInfo.h"
#include <cetlib/BasicPluginFactory.h>

#include "otsdaq/FiniteStateMachine/RunInfoVInterface.h"

ots::RunInfoVInterface* ots::makeRunInfo(const std::string& runInfoPluginName, const std::string& runInfoUID)
{
	static cet::BasicPluginFactory basicPluginInterfaceFactory("runinfo", "make");

	return basicPluginInterfaceFactory.makePlugin<ots::RunInfoVInterface*, const std::string&, const std::string&>(
	    runInfoPluginName, runInfoPluginName, runInfoUID);
}
