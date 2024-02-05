#ifndef _SimpleSoap_SimpleSoap_h
#define _SimpleSoap_SimpleSoap_h

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <xdaq/Application.h>
#pragma GCC diagnostic pop
#include "otsdaq/Macros/XDAQApplicationMacros.h"
#include "xgi/Method.h"

#include <toolbox/TimeInterval.h>
#include <toolbox/fsm/FailedEvent.h>
#include <toolbox/fsm/FiniteStateMachine.h>
#include <toolbox/task/Timer.h>
#include <toolbox/task/TimerFactory.h>
#include <toolbox/task/TimerListener.h>
#include <xcept/Exception.h>

#include <cgicc/HTMLClasses.h>
#include <cgicc/HTMLDoctype.h>
#include <cgicc/HTTPCookie.h>
#include <cgicc/HTTPHeader.h>

#include <xdata/String.h>
#include "Utilities/SOAPUtilities/include/SOAPMessenger.h"

#include <string>

namespace ots
{
class SimpleSoap : public xdaq::Application, public SOAPMessenger
{
  public:
	XDAQ_INSTANTIATOR();

	SimpleSoap(xdaq::ApplicationStub* s);
	void                   Default(xgi::Input* in, xgi::Output* out);
	void                   StateMachineXgiHandler(xgi::Input* in, xgi::Output* out);
	xoap::MessageReference Start(xoap::MessageReference msg);
	void                   stateInitial(toolbox::fsm::FiniteStateMachine& fsm);
	void                   stateHalted(toolbox::fsm::FiniteStateMachine& fsm);

  private:
	toolbox::fsm::FiniteStateMachine fsm_;
	xdata::String                    state_;  // used to reflect the current state to the outside world
	                                          // toolbox::task::WorkLoop * jobcontrolWorkloop_;
	                                          // toolbox::task::ActionSignature * jobcontrolTask_;
};

}  // namespace ots

#endif
