// OTS_LITE stand-in for xdaq's xdaq/Application.h: base class of every otsdaq Supervisor.
// Holds the bound HTTP (xgi) and SOAP (xoap) handlers; the lite front end dispatches into them.
// Real xdaq's Application.h transitively provides a lot of std/toolbox/xdata; otsdaq headers
// rely on that, so the same set is included here.
#ifndef _xdaq_Application_h_
#define _xdaq_Application_h_

#include <deque>
#include <fstream>
#include <functional>  // std::function; real xdaq provides it transitively and otsdaq relies on that
#include <iostream>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "toolbox/BSem.h"
#include "toolbox/lang/Class.h"
#include "xcept/tools.h"
#include "xdaq/ApplicationContext.h"
#include "xdaq/ApplicationDescriptor.h"
#include "xdaq/ApplicationStub.h"
#include "xdaq/exception/Exception.h"
#include "xdata/String.h"
#include "xoap/MessageReference.h"

namespace xgi
{
class Input;
class Output;
}  // namespace xgi

namespace xdaq
{
class Application : public virtual toolbox::lang::Class
{
  public:
	typedef std::function<void(xgi::Input*, xgi::Output*)>                   HTTPHandler;
	typedef std::function<xoap::MessageReference(xoap::MessageReference)>   SOAPHandler;

	explicit Application(ApplicationStub* stub) : stub_(stub) {}
	virtual ~Application() {}

	const ApplicationDescriptor* getApplicationDescriptor() const { return stub_->getDescriptor(); }
	ApplicationContext*          getApplicationContext() const { return stub_->getContext(); }

	// registration (called by xgi::bind / xoap::bind)
	void bindHTTP(const std::string& name, HTTPHandler h) { httpHandlers_[name] = h; }
	void bindSOAP(const std::string& command, SOAPHandler h) { soapHandlers_[command] = h; }

	// dispatch (called by the HTTP server in ots_lite_main / ApplicationContext::postSOAP)
	bool                   dispatchHTTP(const std::string& name, xgi::Input* in, xgi::Output* out);
	xoap::MessageReference dispatchSOAP(xoap::MessageReference msg);
	const std::map<std::string, HTTPHandler>& getHTTPHandlers()
	{
		claimPendingHandlers();
		return httpHandlers_;
	}

  private:
	// Moves handlers bound (possibly from mix-in base constructors) that resolve to this
	// application out of the pending registry into this object. Safe to call repeatedly.
	void claimPendingHandlers();

	ApplicationStub*                   stub_;
	std::map<std::string, HTTPHandler> httpHandlers_;
	std::map<std::string, SOAPHandler> soapHandlers_;
};

// Each Supervisor class declares XDAQ_INSTANTIATOR() and defines XDAQ_INSTANTIATOR_IMPL(Class)
// in its .cc, producing an exported `ots::Class::instantiate(xdaq::ApplicationStub*)`.
// As real xdaq.exe does, ots_lite_main finds it with dlsym by its mangled name; there is no
// registry. (otsdaq's own Macros/XDAQApplicationMacros.h redefines these two macros identically
// after including this header, so they must stay plain.)
typedef Application* (*ApplicationInstantiator)(ApplicationStub*);
}  // namespace xdaq

#define XDAQ_INSTANTIATOR() static xdaq::Application* instantiate(xdaq::ApplicationStub* stub)

#define XDAQ_INSTANTIATOR_IMPL(CLASS) \
	xdaq::Application* CLASS::instantiate(xdaq::ApplicationStub* stub) { return new CLASS(stub); }

#endif
