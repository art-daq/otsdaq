#ifndef _ots_InterfacePluginMacro_h_
#define _ots_InterfacePluginMacro_h_

#include <memory>
#ifndef OTS_MAXLITE  // maxlite has no cetlib; the plain extern "C" fallback below is used
#include "cetlib/compiler_macros.h"
#endif
#include "otsdaq/FECore/FEVInterface.h"

#ifndef EXTERN_C_FUNC_DECLARE_START
#define EXTERN_C_FUNC_DECLARE_START extern "C" {
#endif

#define DEFINE_OTS_INTERFACE(klass)                                                    \
	EXTERN_C_FUNC_DECLARE_START                                                        \
	std::unique_ptr<ots::FEVInterface> make(                                           \
	    const std::string&            interfaceUID,                                    \
	    const ots::ConfigurationTree& configurationTree,                               \
	    const std::string&            pathToInterfaceConfiguration)                    \
	{                                                                                  \
		return std::unique_ptr<ots::FEVInterface>(                                     \
		    new klass(interfaceUID, configurationTree, pathToInterfaceConfiguration)); \
	}                                                                                  \
	}

#endif /* _ots_InterfacePluginMacro_h_ */
