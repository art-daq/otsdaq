// OTS_LITE stand-in for xdaq's xoap/Method.h: registers SOAP command handlers on an application.
// As in real xdaq, the handler owner may be a mix-in (e.g. a toolbox::lang::Class) rather than
// the xdaq::Application itself, so the owning application is found with dynamic_cast.
#ifndef _xoap_Method_h_
#define _xoap_Method_h_

#include <string>

#include "xdaq/Application.h"
#include "xoap/MessageReference.h"

namespace xoap
{
template<class OBJECT>
void bind(OBJECT* obj, MessageReference (OBJECT::*func)(MessageReference), const std::string& command, const std::string& /*namespaceURI*/ = "")
{
	xdaq::Application* app = dynamic_cast<xdaq::Application*>(obj);
	if(!app)
		XCEPT_RAISE(xdaq::exception::Exception, "xoap::bind: handler owner for '" + command + "' is not an xdaq::Application");
	app->bindSOAP(command, [obj, func](MessageReference m) { return (obj->*func)(m); });
}
}  // namespace xoap

#endif
