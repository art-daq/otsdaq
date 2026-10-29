// OTS_LITE stand-in for xdaq's xoap/Method.h: registers SOAP command handlers.
// Only records the binding; the owning xdaq::Application claims it at first dispatch
// (see xdaq/HandlerRegistry.h for why resolution cannot happen here).
#ifndef _xoap_Method_h_
#define _xoap_Method_h_

#include <string>

#include "xdaq/Application.h"
#include "xdaq/HandlerRegistry.h"
#include "xoap/MessageReference.h"

namespace xoap
{
template<class OBJECT>
void bind(OBJECT* obj, MessageReference (OBJECT::*func)(MessageReference), const std::string& command, const std::string& /*namespaceURI*/ = "")
{
	std::lock_guard<std::mutex> lk(xdaq::handlerRegistryMutex());
	xdaq::pendingSOAPHandlers().push_back({static_cast<void*>(obj), command, [obj, func](MessageReference m) { return (obj->*func)(m); }, [obj]() -> xdaq::Application* { return dynamic_cast<xdaq::Application*>(obj); }});
}
}  // namespace xoap

#endif
