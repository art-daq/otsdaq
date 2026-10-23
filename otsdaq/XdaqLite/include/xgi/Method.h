// OTS_LITE stand-in for xdaq's xgi/Method.h: registers HTTP callbacks on an application.
// The owner may be a mix-in rather than the xdaq::Application itself (see xoap/Method.h).
#ifndef _xgi_Method_h_
#define _xgi_Method_h_

#include <string>

#include "cgicc/Cgicc.h"  // real xdaq's xgi/Method.h pulls cgicc in; otsdaq relies on that
#include "xdaq/Application.h"
#include "xgi/Input.h"
#include "xgi/Output.h"

namespace xgi
{
template<class OBJECT>
void bind(OBJECT* obj, void (OBJECT::*func)(xgi::Input*, xgi::Output*), const std::string& name)
{
	xdaq::Application* app = dynamic_cast<xdaq::Application*>(obj);
	if(!app)
		XCEPT_RAISE(xdaq::exception::Exception, "xgi::bind: handler owner for '" + name + "' is not an xdaq::Application");
	app->bindHTTP(name, [obj, func](xgi::Input* in, xgi::Output* out) { (obj->*func)(in, out); });
}
}  // namespace xgi

#endif
