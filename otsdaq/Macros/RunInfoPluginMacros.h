#ifndef OTSDAQ_OTSDAQ_MACROS_RUNINFOPLUGINMACROS_H_
#define OTSDAQ_OTSDAQ_MACROS_RUNINFOPLUGINMACROS_H_

#include <string>
#include "cetlib/compiler_macros.h"
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

#endif  // OTSDAQ_OTSDAQ_MACROS_RUNINFOPLUGINMACROS_H_
