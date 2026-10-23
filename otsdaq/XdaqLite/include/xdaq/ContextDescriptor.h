// OTS_LITE stand-in for xdaq's xdaq/ContextDescriptor.h: one process (host:port) hosting applications.
#ifndef _xdaq_ContextDescriptor_h_
#define _xdaq_ContextDescriptor_h_

#include <string>

namespace xdaq
{
class ContextDescriptor
{
  public:
	explicit ContextDescriptor(const std::string& url) : url_(url) {}
	std::string getURL() const { return url_; }

  private:
	std::string url_;
};
}  // namespace xdaq

#endif
