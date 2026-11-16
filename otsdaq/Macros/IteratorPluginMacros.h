#ifndef _ots_IteratorPluginMacros_h_
#define _ots_IteratorPluginMacros_h_

#include <memory>
#include <string>
#ifndef OTS_MAXLITE  // maxlite has no cetlib; the plain extern "C" fallback below is used
#include "cetlib/compiler_macros.h"
#endif
#include "otsdaq/GatewaySupervisor/IteratorPluginVInterface.h"

#ifndef EXTERN_C_FUNC_DECLARE_START
#define EXTERN_C_FUNC_DECLARE_START extern "C" {
#endif

#define DEFINE_OTS_ITERATOR_PLUGIN(klass)                                              \
	EXTERN_C_FUNC_DECLARE_START                                                        \
	std::unique_ptr<ots::IteratorPluginVInterface> make(const std::string& pluginName) \
	{                                                                                  \
		return std::unique_ptr<ots::IteratorPluginVInterface>(new klass(pluginName));  \
	}                                                                                  \
	}

#endif /* _ots_IteratorPluginMacros_h_ */
