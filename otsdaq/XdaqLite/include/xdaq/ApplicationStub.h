// OTS_LITE stand-in for xdaq's xdaq/ApplicationStub.h: what the framework hands a new application.
#ifndef _xdaq_ApplicationStub_h_
#define _xdaq_ApplicationStub_h_

#include "xdaq/ApplicationContext.h"
#include "xdaq/ApplicationDescriptor.h"

namespace xdaq
{
class ApplicationStub
{
  public:
	ApplicationStub(ApplicationContext* context, const ApplicationDescriptor* descriptor) : context_(context), descriptor_(descriptor) {}
	ApplicationContext*          getContext() const { return context_; }
	const ApplicationDescriptor* getDescriptor() const { return descriptor_; }

  private:
	ApplicationContext*          context_;
	const ApplicationDescriptor* descriptor_;
};
}  // namespace xdaq

#endif
