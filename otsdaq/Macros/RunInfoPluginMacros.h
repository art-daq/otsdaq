#ifndef _ots_RunInfoPluginMacro_h_
#define _ots_RunInfoPluginMacro_h_

#include <string>
#include "cetlib/compiler_macros.h"
#include "otsdaq/FiniteStateMachine/RunInfoVInterface.h"  // for Run Info plugins

namespace ots
{
typedef RunInfoVInterface*(dpvimakeFunc_t)();
}
/*
,                                                \
                                        const ConfigurationTree& configurationTree,                                           \
                                        const std::string&       pathToInterfaceConfiguration)                                \

                                        , configurationTree, pathToInterfaceConfiguration); \
*/

#define DEFINE_OTS_PROCESSOR(klass)                                          \
	extern "C" ots::RunInfoVInterface* make(std::string const& interfaceUID) \
	{                                                                        \
		return new klass(interfaceUID);                                      \
	}

#endif /* _ots_RunInfoPluginMacro_h_ */
