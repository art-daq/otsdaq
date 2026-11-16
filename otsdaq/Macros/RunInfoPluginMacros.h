#ifndef _ots_RunInfoPluginMacro_h_
#define _ots_RunInfoPluginMacro_h_

#include <string>
#ifndef OTS_MAXLITE  // maxlite has no cetlib; the plain extern "C" fallback below is used
#include "cetlib/compiler_macros.h"
#endif
#include "otsdaq/FiniteStateMachine/RunInfoVInterface.h"  // for Run Info plugins

namespace ots
{
typedef RunInfoVInterface*(dpvimakeFunc_t)();
}

#define DEFINE_OTS_PROCESSOR(klass)                                                    \
	extern "C" ots::RunInfoVInterface* make(const std::string& runInfoPluginClassName, \
	                                        const std::string& activeStateMachineName) \
	{                                                                                  \
		return new klass(runInfoPluginClassName, activeStateMachineName);              \
	}

#endif /* _ots_RunInfoPluginMacro_h_ */
