#ifndef _ots_InterfacePluginMacro_h_
#define _ots_InterfacePluginMacro_h_

#include <memory>
#include "cetlib/compiler_macros.h"
#include "otsdaq/FECore/FEVInterface.h"

#ifndef EXTERN_C_FUNC_DECLARE_START
#define EXTERN_C_FUNC_DECLARE_START extern "C" {
#endif

#define DEFINE_OTS_INTERFACE(klass)                                                                                                        \
	EXTERN_C_FUNC_DECLARE_START                                                                                                            \
	std::unique_ptr<ots::FEVInterface> make(                                                                                               \
	    const std::string& interfaceUID, const ots::ConfigurationTree& configurationTree, const std::string& pathToInterfaceConfiguration) \
	{                                                                                                                                      \
		return std::unique_ptr<ots::FEVInterface>(new klass(interfaceUID, configurationTree, pathToInterfaceConfiguration));               \
	}                                                                                                                                      \
	}

#endif /* _ots_InterfacePluginMacro_h_ */
