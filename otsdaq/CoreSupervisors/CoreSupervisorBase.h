#ifndef _ots_CoreSupervisorBase_h_
#define _ots_CoreSupervisorBase_h_

#include "otsdaq/FiniteStateMachine/RunControlStateMachine.h"
#include "otsdaq/SOAPUtilities/SOAPMessenger.h"
#include "otsdaq/WorkLoopManager/WorkLoopManager.h"

#include "otsdaq/CoreSupervisors/CorePropertySupervisorBase.h"

#include "otsdaq/CgiDataUtilities/CgiDataUtilities.h"
#include "otsdaq/SOAPUtilities/SOAPUtilities.h"
#include "otsdaq/XmlUtilities/HttpXmlDocument.h"

#include "otsdaq/FiniteStateMachine/VStateMachine.h"
#include "otsdaq/WebUsersUtilities/RemoteWebUsers.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <xdaq/Application.h>
#pragma GCC diagnostic pop
#include "otsdaq/Macros/XDAQApplicationMacros.h"
#include "xgi/Method.h"

#include <toolbox/fsm/FailedEvent.h>

#include <xdaq/NamespaceURI.h>
#include <xoap/Method.h>

#include <map>
#include <memory>
#include <string> /*string and to_string*/
#include <vector>
#include <time.h>

namespace ots
{
// clang-format off

// CoreSupervisorBase
//	This class should be the base class for all client otsdaq, XDAQ-based, supervisors.
//	That is, all supervisors that need web requests through the ots desktop
//		with access verified by the Gateway Supervisor,
//		or that need a state machines driven by the Gateway Supervisor.
class CoreSupervisorBase : public xdaq::Application,
                           public SOAPMessenger,
                           public CorePropertySupervisorBase,
                           public RunControlStateMachine
{
	friend class MacroMakerSupervisor;  // to allow MacroMakerSupervisor to call
	                                    // requestWrapper in Macro Maker mode

  public:
	CoreSupervisorBase(xdaq::ApplicationStub* stub);
	virtual ~CoreSupervisorBase(void);

	void destroy(void);

	unsigned int 					getSupervisorLID				(void) const { return getApplicationDescriptor()->getLocalId(); }
	time_t		 					getSupervisorUptime				(void) const { return time(0) - constructedTime_;}				

	// Here are the common web request handlers:
	//	defaultPage returns the public html page
	//	request checks the login before proceeding to virtual request
	//		- All Supervisors should implement request for their actions (and they will
	// get  the security wrapper for free)
	//		- The security setting defaults can be setup or forced by overriding
	// setSupervisorPropertyDefaults and forceSupervisorProperties
	virtual void 					defaultPage						(xgi::Input* in, xgi::Output* out);
	virtual void 					request							(const std::string&               requestType,
																	cgicc::Cgicc&                    cgiIn,
																	HttpXmlDocument&                 xmlOut,
																	const WebUsers::RequestUserInfo& userInfo);
	virtual void 					nonXmlRequest					(const std::string&               requestType,
															   	   	cgicc::Cgicc&                    cgiIn,
																	std::ostream&                    out,
																	const WebUsers::RequestUserInfo& userInfo);
	virtual std::string 			getStatusProgressDetail			(void);
	virtual std::vector<SupervisorInfo::SubappInfo>             getSubappInfo					(void) { return std::vector<SupervisorInfo::SubappInfo>(); }

  private:
	xoap::MessageReference 			workLoopStatusRequestWrapper	(xoap::MessageReference message);
	void                   			defaultPageWrapper				(xgi::Input* in, xgi::Output* out);
	void                   			requestWrapper					(xgi::Input* in, xgi::Output* out);
	xoap::MessageReference 			TRACESupervisorRequest			(xoap::MessageReference message);

  public:
	// State Machine request handlers
	void                   			stateMachineXgiHandler			(xgi::Input* in, xgi::Output* out);
	xoap::MessageReference 			stateMachineXoapHandler			(xoap::MessageReference message);

	xoap::MessageReference 			stateMachineStateRequest		(xoap::MessageReference message);
	xoap::MessageReference 			stateMachineErrorMessageRequest	(xoap::MessageReference message);

	void 							sendAsyncExceptionToGateway		(const std::string& errMsg, bool isPauseException, bool isStopException);

	virtual xoap::MessageReference 	workLoopStatusRequest			(xoap::MessageReference message);
	virtual xoap::MessageReference 	applicationStatusRequest		(xoap::MessageReference message);

	bool 							stateMachineThread				(toolbox::task::WorkLoop* workLoop);

	virtual void stateInitial			(toolbox::fsm::FiniteStateMachine& fsm);
	virtual void statePaused			(toolbox::fsm::FiniteStateMachine& fsm);
	virtual void stateRunning			(toolbox::fsm::FiniteStateMachine& fsm);
	virtual void stateHalted			(toolbox::fsm::FiniteStateMachine& fsm);
	virtual void stateConfigured		(toolbox::fsm::FiniteStateMachine& fsm);
	virtual void inError				(toolbox::fsm::FiniteStateMachine& fsm);

	virtual void transitionConfiguring	(toolbox::Event::Reference event);
  protected:
	void 		 transitionConfiguringFSMs(void);
  public:
	virtual void transitionHalting		(toolbox::Event::Reference event);
	virtual void transitionInitializing	(toolbox::Event::Reference event);
	virtual void transitionPausing		(toolbox::Event::Reference event);
	virtual void transitionResuming		(toolbox::Event::Reference event);
	virtual void transitionStarting		(toolbox::Event::Reference event);
	virtual void transitionStopping		(toolbox::Event::Reference event);
	virtual void enteringError			(toolbox::Event::Reference event);

	static const std::string WORK_LOOP_DONE, WORK_LOOP_WORKING;

  protected:
	WorkLoopManager             stateMachineWorkLoopManager_;
	toolbox::BSem               stateMachineSemaphore_;
	std::vector<VStateMachine*> theStateMachineImplementation_;

	// for managing transition iterations
	std::vector<bool> stateMachinesIterationDone_;
	unsigned int      stateMachinesIterationWorkCount_;
	unsigned int      subIterationWorkStateMachineIndex_;
	void              preStateMachineExecution(unsigned int i);
	void              postStateMachineExecution(unsigned int i);
	void              preStateMachineExecutionLoop(void);
	void              postStateMachineExecutionLoop(void);

	RemoteWebUsers 				theRemoteWebUsers_;

private:	
	const time_t				constructedTime_ = time(0);
};
// clang-format on
}  // namespace ots

#endif
