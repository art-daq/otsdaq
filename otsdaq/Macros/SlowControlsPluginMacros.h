#ifndef OTSDAQ_OTSDAQ_MACROS_SLOWCONTROLSPLUGINMACROS_H_
#define OTSDAQ_OTSDAQ_MACROS_SLOWCONTROLSPLUGINMACROS_H_

#include <memory>
#include <string>

#include "otsdaq/SlowControlsCore/SlowControlsVInterface.h"

#define DEFINE_OTS_SLOW_CONTROLS(klass)                       \
	extern "C" ots::SlowControlsVInterface* make(             \
	    const std::string&       pluginType,                  \
	    const std::string&       slowControlsUID,             \
	    const ConfigurationTree& configurationTree,           \
	    const std::string&       pathToControlsConfiguration) \
	{                                                         \
		return new klass(pluginType,                          \
		                 slowControlsUID,                     \
		                 configurationTree,                   \
		                 pathToControlsConfiguration);        \
	}

#endif  // OTSDAQ_OTSDAQ_MACROS_SLOWCONTROLSPLUGINMACROS_H_
