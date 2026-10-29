// OTS_LITE stand-in for xdaq's xdaq/ApplicationDescriptor.h
#ifndef _xdaq_ApplicationDescriptor_h_
#define _xdaq_ApplicationDescriptor_h_

#include <string>

#include "xdaq/ContextDescriptor.h"

namespace xdaq
{
class ApplicationDescriptor
{
  public:
	ApplicationDescriptor(const std::string& className, unsigned int localId, unsigned int instance, const ContextDescriptor* context)
	    : className_(className)
	    , localId_(localId)
	    , instance_(instance)
	    , context_(context)
	{
	}
	std::string              getClassName() const { return className_; }
	unsigned int             getLocalId() const { return localId_; }
	unsigned int             getInstance() const { return instance_; }
	bool                     hasInstanceNumber() const { return true; }
	std::string              getURN() const { return "urn:xdaq-application:lid=" + std::to_string(localId_); }
	const ContextDescriptor* getContextDescriptor() const { return context_; }
	std::string              getAttribute(const std::string&) const { return ""; }

  private:
	std::string              className_;
	unsigned int             localId_, instance_;
	const ContextDescriptor* context_;
};
}  // namespace xdaq

#endif
