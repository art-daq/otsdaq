#ifndef _ots_SlowControlsPluginMacro_h_
#define _ots_SlowControlsPluginMacro_h_

#include <memory>

#include "otsdaq-core/SlowControlsCore/SlowControlsVInterface.h"

#define DEFINE_OTS_SLOW_CONTROLS(klass)                                       \
	extern "C" ots::SlowControlsVInterface* make(                             \
	    const std::string&       slowControlsUID,                             \
	    const ConfigurationTree& configurationTree,                           \
	    const std::string&       pathToControlsConfiguration)                 \
	{                                                                         \
		return new klass(                                                     \
		    slowControlsUID, configurationTree, pathToControlsConfiguration); \
	}

#endif /* _ots_SlowControlsPluginMacro_h_ */