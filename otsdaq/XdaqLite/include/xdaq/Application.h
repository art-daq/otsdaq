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

	// dispatch (called by the lite front end / ApplicationContext::postSOAP)
	bool                   dispatchHTTP(const std::string& name, xgi::Input* in, xgi::Output* out);
	xoap::MessageReference dispatchSOAP(xoap::MessageReference msg);
	const std::map<std::string, HTTPHandler>& getHTTPHandlers() const { return httpHandlers_; }

  private:
	ApplicationStub*                   stub_;
	std::map<std::string, HTTPHandler> httpHandlers_;
	std::map<std::string, SOAPHandler> soapHandlers_;
};

// Factory registration, mirroring xdaq's macros: each Supervisor class declares
// XDAQ_INSTANTIATOR() and defines XDAQ_INSTANTIATOR_IMPL(Class) in its .cc.
typedef Application* (*ApplicationInstantiator)(ApplicationStub*);
void                    registerApplicationClass(const std::string& className, ApplicationInstantiator fn);
ApplicationInstantiator findApplicationClass(const std::string& className);
}  // namespace xdaq

#define XDAQ_INSTANTIATOR() static xdaq::Application* instantiate(xdaq::ApplicationStub* stub)

#define XDAQ_INSTANTIATOR_IMPL(CLASS)                                                                \
	xdaq::Application* CLASS::instantiate(xdaq::ApplicationStub* stub) { return new CLASS(stub); }  \
	namespace                                                                                      \
	{                                                                                              \
	struct CLASS##_registrar                                                                       \
	{                                                                                              \
		CLASS##_registrar() { xdaq::registerApplicationClass("ots::" #CLASS, &CLASS::instantiate); } \
	} CLASS##_registrar_instance;                                                                  \
	}

#endif
