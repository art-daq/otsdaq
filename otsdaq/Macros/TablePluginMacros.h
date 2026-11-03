#ifndef OTSDAQ_OTSDAQ_MACROS_TABLEPLUGINMACROS_H_
#define OTSDAQ_OTSDAQ_MACROS_TABLEPLUGINMACROS_H_

#include "otsdaq/TableCore/TableBase.h"

namespace ots
{
typedef TableBase*(cbmakeFunc_t)();
}

#define DEFINE_OTS_TABLE(klass) \
	extern "C" TableBase* make() { return new klass(); }
#endif  // OTSDAQ_OTSDAQ_MACROS_TABLEPLUGINMACROS_H_
