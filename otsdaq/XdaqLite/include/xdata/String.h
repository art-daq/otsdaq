// OTS_LITE stand-in for xdaq's xdata types; otsdaq only uses UnsignedIntegerT as a map key.
#ifndef _xdata_String_h_
#define _xdata_String_h_

#include <string>

namespace xdata
{
typedef unsigned int  UnsignedIntegerT;
typedef unsigned long UnsignedLongT;
typedef std::string   StringT;
}  // namespace xdata

#endif
