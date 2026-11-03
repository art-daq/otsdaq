#ifndef OTSDAQ_OTSDAQ_MACROS_CONFIGURATIONINTERFACEPLUGINMACROS_H_
#define OTSDAQ_OTSDAQ_MACROS_CONFIGURATIONINTERFACEPLUGINMACROS_H_

#include "otsdaq/ConfigurationInterface/ConfigurationInterface.h"

namespace ots
{
typedef ConfigurationInterface*(cbmakeFunc_t)();
}

#define DEFINE_OTS_CONFIGURATION_INTERFACE(klass) \
	extern "C" ConfigurationInterface* make() { return new klass(); }
#endif  // OTSDAQ_OTSDAQ_MACROS_CONFIGURATIONINTERFACEPLUGINMACROS_H_
