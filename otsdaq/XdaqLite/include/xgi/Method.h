// OTS_LITE stand-in for xdaq's xgi/Method.h: registers HTTP callbacks.
// Only records the binding; the owning xdaq::Application claims it at first dispatch
// (see xdaq/HandlerRegistry.h for why resolution cannot happen here).
#ifndef _xgi_Method_h_
#define _xgi_Method_h_

#include <string>

#include "cgicc/Cgicc.h"  // real xdaq's xgi/Method.h pulls cgicc in; otsdaq relies on that
#include "xdaq/Application.h"
#include "xdaq/HandlerRegistry.h"
#include "xgi/Input.h"
#include "xgi/Output.h"

namespace xgi
{
template<class OBJECT>
void bind(OBJECT* obj, void (OBJECT::*func)(xgi::Input*, xgi::Output*), const std::string& name)
{
	std::lock_guard<std::mutex> lk(xdaq::handlerRegistryMutex());
	xdaq::pendingHTTPHandlers().push_back({static_cast<void*>(obj), name, [obj, func](xgi::Input* in, xgi::Output* out) { (obj->*func)(in, out); },
	                                       [obj]() -> xdaq::Application* { return dynamic_cast<xdaq::Application*>(obj); }});
}
}  // namespace xgi

#endif
