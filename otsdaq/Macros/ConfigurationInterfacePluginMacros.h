#ifndef _ots_ConfigurationInterfacePluginMacro_h_
#define _ots_ConfigurationInterfacePluginMacro_h_

#include "otsdaq/ConfigurationInterface/ConfigurationInterface.h"

namespace ots
{
typedef ConfigurationInterface*(cbmakeFunc_t)();
}

#define DEFINE_OTS_CONFIGURATION_INTERFACE(klass) \
	extern "C" ConfigurationInterface* make()     \
	{                                             \
		return new klass();                       \
	}
#endif /* _ots_TablePluginMacro_h_ */
