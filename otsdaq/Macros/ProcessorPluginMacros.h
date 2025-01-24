#ifndef _ots_ProcessorPluginMacro_h_
#define _ots_ProcessorPluginMacro_h_

#include <string>
#include "otsdaq/DataManager/DataProcessor.h"

namespace ots
{
typedef DataProcessor*(dpvimakeFunc_t)();
}

#define DEFINE_OTS_PROCESSOR(klass)                                                        \
	extern "C" ots::DataProcessor* make(std::string const&       supervisorApplicationUID, \
	                                    std::string const&       bufferUID,                \
	                                    std::string const&       processorUID,             \
	                                    const ConfigurationTree& configurationTree,        \
	                                    const std::string& pathToInterfaceConfiguration)   \
	{                                                                                      \
		return new klass(supervisorApplicationUID,                                         \
		                 bufferUID,                                                        \
		                 processorUID,                                                     \
		                 configurationTree,                                                \
		                 pathToInterfaceConfiguration);                                    \
	}

#endif /* _ots_ProcessorPluginMacro_h_ */
