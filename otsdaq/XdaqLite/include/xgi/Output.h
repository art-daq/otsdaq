// OTS_LITE stand-in for xdaq's xgi/Output.h: handlers stream the HTTP response body into it.
#ifndef _xgi_Output_h_
#define _xgi_Output_h_

#include <sstream>
#include <string>

#include "cgicc/HTTPResponseHeader.h"

namespace xgi
{
class Output : public std::ostringstream
{
  public:
	Output()
	    : header_("HTTP/1.1", 200, "OK") {}
	cgicc::HTTPResponseHeader& getHTTPResponseHeader() { return header_; }
	void                       setHTTPResponseHeader(const cgicc::HTTPResponseHeader& h) { header_ = h; }

  private:
	cgicc::HTTPResponseHeader header_;
};
}  // namespace xgi

#endif
