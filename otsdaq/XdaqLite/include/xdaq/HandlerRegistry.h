// OTS_LITE: deferred handler registration shared by xgi::bind and xoap::bind.
//
// otsdaq binds handlers from constructors of mix-in base classes (e.g. RunControlStateMachine
// binds SOAP handlers while CoreSupervisorBase is still being constructed). At that moment the
// object is not yet an xdaq::Application, so it cannot be resolved with dynamic_cast. As real
// xdaq does, bind() only records (owner, name, callable); the owning application claims its
// handlers lazily at first dispatch, when the object is complete.
#ifndef _xdaq_HandlerRegistry_h_
#define _xdaq_HandlerRegistry_h_

#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace xgi
{
class Input;
class Output;
}  // namespace xgi
namespace xoap
{
class SOAPMessage;
}

namespace xdaq
{
class Application;

struct PendingHTTPHandler
{
	void*                                          owner;
	std::string                                    name;
	std::function<void(xgi::Input*, xgi::Output*)> fn;
	std::function<Application*()>                  resolve;  // dynamic_cast owner -> Application, valid once complete
};
struct PendingSOAPHandler
{
	void*                                                                                 owner;
	std::string                                                                           command;
	std::function<std::shared_ptr<xoap::SOAPMessage>(std::shared_ptr<xoap::SOAPMessage>)> fn;
	std::function<Application*()>                                                         resolve;
};

std::vector<PendingHTTPHandler>& pendingHTTPHandlers();
std::vector<PendingSOAPHandler>& pendingSOAPHandlers();
std::mutex&                      handlerRegistryMutex();
}  // namespace xdaq

#endif
