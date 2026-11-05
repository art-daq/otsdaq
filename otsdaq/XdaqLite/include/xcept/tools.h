// OTS_LITE stand-in for xdaq's xcept/tools.h
#ifndef _xcept_tools_h_
#define _xcept_tools_h_

#include "xcept/Exception.h"

namespace xcept
{
inline std::string stdformat_exception_history(const xcept::Exception& e) { return e.name() + ": " + e.message(); }
inline std::string htmlformat_exception_history(const xcept::Exception& e) { return stdformat_exception_history(e); }
}  // namespace xcept

#endif
