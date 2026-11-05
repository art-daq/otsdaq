// OTS_LITE stand-in for xdaq's xgi/Utils.h
#ifndef _xgi_Utils_h_
#define _xgi_Utils_h_

#include <string>

#include "xgi/Input.h"
#include "xgi/Output.h"

namespace xgi
{
namespace Utils
{
inline void getPageHeader(xgi::Output* out, const std::string& title) { *out << "<html><head><title>" << title << "</title></head><body>"; }
inline void getPageFooter(xgi::Output* out) { *out << "</body></html>"; }
}  // namespace Utils
}  // namespace xgi

#endif
