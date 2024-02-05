#ifndef _ots_RunControlStateMachine_h_
#define _ots_RunControlStateMachine_h_

#include "otsdaq/FiniteStateMachine/FiniteStateMachine.h"
#include "otsdaq/ProgressBar/ProgressBar.h"

#include <string>
#include "toolbox/lang/Class.h"

namespace ots
{
// CoreSupervisorBase
// This class provides finite state machine functionality for otsdaq supervisors.
class RunControlStateMachine : public virtual toolbox::lang::Class
{
  public:
	RunControlStateMachine(const std::string& name = "Undefined Name");
	virtual ~RunControlStateMachine(void);

	void               reset(void);
	void               setStateMachineName(const std::string& name) { theStateMachine_.setStateMachineName(name); }
	const std::string& getErrorMessage(void) const { return theStateMachine_.getErrorMessage(); }
	void               setAsyncPauseExceptionMessage(const std::string& error)
	{
		asyncPauseExceptionReceived_ = true;
		theStateMachine_.setErrorMessage(error);
	}
	void setAsyncStopExceptionMessage(const std::string& error)
	{
		asyncStopExceptionReceived_ = true;
		theStateMachine_.setErrorMessage(error);
	}

	template<class OBJECT>
	void addStateTransition(toolbox::fsm::State from,
	                        toolbox::fsm::State to,
	                        const std::string&  input,
	                        const std::string&  transitionName,
	                        OBJECT*             obj,
	                        void (OBJECT::*func)(toolbox::Event::Reference))

	{
		stateTransitionFunctionTable_[from][input] = func;
		theStateMachine_.addStateTransition(from, to, input, transitionName, obj, func);
	}

	template<class OBJECT>
	void addStateTransition(toolbox::fsm::State from,
	                        toolbox::fsm::State to,
	                        const std::string&  input,
	                        const std::string&  transitionName,
	                        const std::string&  transitionParameter,
	                        OBJECT*             obj,
	                        void (OBJECT::*func)(toolbox::Event::Reference))

	{
		stateTransitionFunctionTable_[from][input] = func;
		theStateMachine_.addStateTransition(from, to, input, transitionName, transitionParameter, obj, func);
	}

	// using	stateMachineFunction_t = void (ots::RunControlStateMachine::*
	// )(toolbox::Event::Reference);  stateMachineFunction_t getTransitionFunction	(const
	// toolbox::fsm::State from, const std::string &transition);

	// Finite State Machine States
	// 1. Control Configuration and Function Manager are loaded.
	virtual void stateInitial(toolbox::fsm::FiniteStateMachine& /*fsm*/) { ; }

	// 1. XDAQ applications are running.
	// 2. Hardware is running.
	// 3. Triggers are accepted.
	// 4. Triggers are not sent.
	// 5. Data is sent / read out.
	virtual void statePaused(toolbox::fsm::FiniteStateMachine& /*fsm*/) { ; }

	// 1. XDAQ applications are running.
	// 2. Hardware is running.
	// 3. Triggers are accepted.
	// 4. Triggers are sent.
	// 5. Data is sent / read out.
	virtual void stateRunning(toolbox::fsm::FiniteStateMachine& /*fsm*/) { ; }

	// 1. Control hierarchy is instantiated.
	// 2. XDAQ executives are running and configured.
	// 3. XDAQ applications are loaded and instantiated.
	// 4. DCS nodes are allocated.
	virtual void stateHalted(toolbox::fsm::FiniteStateMachine& /*fsm*/) { ; }

	// 1. Power supplies are turned off.
	virtual void stateShutdown(toolbox::fsm::FiniteStateMachine& /*fsm*/) { ; }

	// 1. XDAQ applications are configured.
	// 2. Run parameters have been distributed.
	// 3. Hardware is configured.
	// 4. I2O connections are established, no data is sent or read out.
	// 5. Triggers are not sent.
	virtual void stateConfigured(toolbox::fsm::FiniteStateMachine& /*fsm*/) { ; }

	virtual void inError(toolbox::fsm::FiniteStateMachine& /*fsm*/) { ; }

	virtual void transitionConfiguring(toolbox::Event::Reference /*e*/) { ; }
	virtual void transitionHalting(toolbox::Event::Reference /*e*/) { ; }
	virtual void transitionShuttingDown(toolbox::Event::Reference /*e*/) { ; }
	virtual void transitionStartingUp(toolbox::Event::Reference /*e*/) { ; }
	virtual void transitionInitializing(toolbox::Event::Reference /*e*/) { ; }
	virtual void transitionPausing(toolbox::Event::Reference /*e*/) { ; }
	virtual void transitionResuming(toolbox::Event::Reference /*e*/) { ; }
	virtual void transitionStarting(toolbox::Event::Reference /*e*/) { ; }
	virtual void transitionStopping(toolbox::Event::Reference /*e*/) { ; }
	virtual void enteringError(toolbox::Event::Reference /*e*/) { ; }

	// Run Control Messages
	xoap::MessageReference runControlMessageHandler(xoap::MessageReference message);

	static const std::string FAILED_STATE_NAME;
	static const std::string INITIAL_STATE_NAME;
	static const std::string HALTED_STATE_NAME;
	static const std::string PAUSED_STATE_NAME;
	static const std::string RUNNING_STATE_NAME;

	static const std::string SHUTDOWN_TRANSITION_NAME;
	static const std::string STARTUP_TRANSITION_NAME;
	static const std::string ERROR_TRANSITION_NAME;
	static const std::string CONFIGURE_TRANSITION_NAME;

	unsigned int       getIterationIndex(void) { return iterationIndex_; }
	unsigned int       getSubIterationIndex(void) { return subIterationIndex_; }
	void               indicateIterationWork(void) { iterationWorkFlag_ = true; }
	void               clearIterationWork(void) { iterationWorkFlag_ = false; }
	bool               getIterationWork(void) { return iterationWorkFlag_; }
	void               indicateSubIterationWork(void) { subIterationWorkFlag_ = true; }
	void               clearSubIterationWork(void) { subIterationWorkFlag_ = false; }
	bool               getSubIterationWork(void) { return subIterationWorkFlag_; }
	const std::string& getLastCommand(void) { return lastIterationCommand_; }
	const std::string& getLastAttemptedConfigureGroup(void) { return lastAttemptedConfigureGroup_; }

  protected:
	FiniteStateMachine theStateMachine_;
	ProgressBar        theProgressBar_;

	volatile bool asyncFailureReceived_, asyncPauseExceptionReceived_, asyncStopExceptionReceived_;

	unsigned int iterationIndex_ = 0, subIterationIndex_ = 0;
	bool         iterationWorkFlag_, subIterationWorkFlag_;

	toolbox::fsm::State lastIterationState_;
	std::string         lastIterationCommand_;
	std::string         lastAttemptedConfigureGroup_;
	std::string         lastIterationResult_;
	unsigned int        lastIterationIndex_, lastSubIterationIndex_;

	std::map<toolbox::fsm::State, std::map<std::string, void (RunControlStateMachine::*)(toolbox::Event::Reference), std::less<std::string> > >
	    stateTransitionFunctionTable_;
};

}  // namespace ots

#endif
