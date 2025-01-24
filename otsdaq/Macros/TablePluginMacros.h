#ifndef _ots_TablePluginMacro_h_
#define _ots_TablePluginMacro_h_

#include "otsdaq/TableCore/TableBase.h"

namespace ots
{
typedef TableBase*(cbmakeFunc_t)();
}

#define DEFINE_OTS_TABLE(klass) \
	extern "C" TableBase* make() { return new klass(); }
#endif /* _ots_TablePluginMacro_h_ */
