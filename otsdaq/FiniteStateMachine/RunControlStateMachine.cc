#include "otsdaq/FiniteStateMachine/RunControlStateMachine.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/StringMacros.h"

#include "otsdaq/SOAPUtilities/SOAPCommand.h"
#include "otsdaq/SOAPUtilities/SOAPUtilities.h"

#include <toolbox/fsm/FailedEvent.h>
#include <xdaq/NamespaceURI.h>
#include <xoap/Method.h>

#include <iostream>

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "FSM"
#define mfSubject_ std::string("FSM-") + theStateMachine_.getStateMachineName()

using namespace ots;

const std::string RunControlStateMachine::FAILED_STATE_NAME  = FiniteStateMachine::FAILED_STATE_NAME;
const std::string RunControlStateMachine::INITIAL_STATE_NAME = "Initial";
const std::string RunControlStateMachine::HALTED_STATE_NAME  = "Halted";
const std::string RunControlStateMachine::PAUSED_STATE_NAME  = "Paused";
const std::string RunControlStateMachine::RUNNING_STATE_NAME = "Running";

const std::string RunControlStateMachine::SHUTDOWN_TRANSITION_NAME = "Shutdown";
const std::string RunControlStateMachine::STARTUP_TRANSITION_NAME  = "Startup";
const std::string RunControlStateMachine::ERROR_TRANSITION_NAME  = "Error";
const std::string RunControlStateMachine::CONFIGURE_TRANSITION_NAME  = "Configure";

//==============================================================================
RunControlStateMachine::RunControlStateMachine(const std::string& name)
    : theStateMachine_(name), asyncFailureReceived_(false), asyncPauseExceptionReceived_(false), asyncStopExceptionReceived_(false)
{
	INIT_MF("." /*directory used is USER_DATA/LOG/.*/);

	theStateMachine_.addState('I', RunControlStateMachine::INITIAL_STATE_NAME, this, &RunControlStateMachine::stateInitial);
	theStateMachine_.addState('H', RunControlStateMachine::HALTED_STATE_NAME, this, &RunControlStateMachine::stateHalted);
	theStateMachine_.addState('C', "Configured", this, &RunControlStateMachine::stateConfigured);
	theStateMachine_.addState('R', RunControlStateMachine::RUNNING_STATE_NAME, this, &RunControlStateMachine::stateRunning);
	theStateMachine_.addState('P', RunControlStateMachine::PAUSED_STATE_NAME, this, &RunControlStateMachine::statePaused);
	theStateMachine_.addState('X', "Shutdown", this, &RunControlStateMachine::stateShutdown);
	// theStateMachine_.addState('v', "Recovering",  this,
	// &RunControlStateMachine::stateRecovering);  theStateMachine_.addState('T',
	// "TTSTestMode", this, &RunControlStateMachine::stateTTSTestMode);

	// RAR added back in on 11/20/2016.. why was it removed..
	// exceptions like..
	//	XCEPT_RAISE (toolbox::fsm::exception::Exception, ss.str());)
	//	take state machine to "failed" otherwise
	theStateMachine_.setStateName('F', RunControlStateMachine::FAILED_STATE_NAME);
	theStateMachine_.setFailedStateTransitionAction(this, &RunControlStateMachine::enteringError);
	theStateMachine_.setFailedStateTransitionChanged(this, &RunControlStateMachine::inError);

	//clang-format off
	// this line was added to get out of Failed state
	RunControlStateMachine::addStateTransition('F', 'H', "Halt", "Halting", this, &RunControlStateMachine::transitionHalting);
	RunControlStateMachine::addStateTransition(
	    'F', 'X', RunControlStateMachine::SHUTDOWN_TRANSITION_NAME, "Shutting Down", this, &RunControlStateMachine::transitionShuttingDown);
	RunControlStateMachine::addStateTransition(
		'F', 'F', RunControlStateMachine::ERROR_TRANSITION_NAME, "Erroring", this, &RunControlStateMachine::transitionShuttingDown);
	RunControlStateMachine::addStateTransition('F', 'F', "Fail", "Failing", this, &RunControlStateMachine::transitionShuttingDown);

	RunControlStateMachine::addStateTransition(
	    'H', 'C', RunControlStateMachine::CONFIGURE_TRANSITION_NAME, "Configuring", "ConfigurationAlias", this, &RunControlStateMachine::transitionConfiguring);
	RunControlStateMachine::addStateTransition(
	    'H', 'X', RunControlStateMachine::SHUTDOWN_TRANSITION_NAME, "Shutting Down", this, &RunControlStateMachine::transitionShuttingDown);
	RunControlStateMachine::addStateTransition(
	    'X', 'I', RunControlStateMachine::STARTUP_TRANSITION_NAME, "Starting Up", this, &RunControlStateMachine::transitionStartingUp);

	// Every state can transition to halted
	RunControlStateMachine::addStateTransition('I', 'H', "Initialize", "Initializing", this, &RunControlStateMachine::transitionInitializing);
	RunControlStateMachine::addStateTransition('H', 'H', "Halt", "Halting", this, &RunControlStateMachine::transitionHalting);
	RunControlStateMachine::addStateTransition('C', 'H', "Halt", "Halting", this, &RunControlStateMachine::transitionHalting);
	RunControlStateMachine::addStateTransition('R', 'H', "Abort", "Aborting", this, &RunControlStateMachine::transitionHalting);
	RunControlStateMachine::addStateTransition('P', 'H', "Abort", "Aborting", this, &RunControlStateMachine::transitionHalting);

	RunControlStateMachine::addStateTransition('R', 'P', "Pause", "Pausing", this, &RunControlStateMachine::transitionPausing);
	RunControlStateMachine::addStateTransition('P', 'R', "Resume", "Resuming", this, &RunControlStateMachine::transitionResuming);
	RunControlStateMachine::addStateTransition('C', 'R', "Start", "Starting", this, &RunControlStateMachine::transitionStarting);
	RunControlStateMachine::addStateTransition('R', 'C', "Stop", "Stopping", this, &RunControlStateMachine::transitionStopping);
	RunControlStateMachine::addStateTransition('P', 'C', "Stop", "Stopping", this, &RunControlStateMachine::transitionStopping);
	//clang-format on

	// NOTE!! There must be a defined message handler for each transition name created
	// above
	xoap::bind(this, &RunControlStateMachine::runControlMessageHandler, "Initialize", XDAQ_NS_URI);
	xoap::bind(this, &RunControlStateMachine::runControlMessageHandler, RunControlStateMachine::CONFIGURE_TRANSITION_NAME, XDAQ_NS_URI);
	xoap::bind(this, &RunControlStateMachine::runControlMessageHandler, "Start", XDAQ_NS_URI);
	xoap::bind(this, &RunControlStateMachine::runControlMessageHandler, "Stop", XDAQ_NS_URI);
	xoap::bind(this, &RunControlStateMachine::runControlMessageHandler, "Pause", XDAQ_NS_URI);
	xoap::bind(this, &RunControlStateMachine::runControlMessageHandler, "Resume", XDAQ_NS_URI);
	xoap::bind(this, &RunControlStateMachine::runControlMessageHandler, "Halt", XDAQ_NS_URI);
	xoap::bind(this, &RunControlStateMachine::runControlMessageHandler, "Abort", XDAQ_NS_URI);
	xoap::bind(this, &RunControlStateMachine::runControlMessageHandler, RunControlStateMachine::SHUTDOWN_TRANSITION_NAME, XDAQ_NS_URI);
	xoap::bind(this, &RunControlStateMachine::runControlMessageHandler, RunControlStateMachine::STARTUP_TRANSITION_NAME, XDAQ_NS_URI);
	xoap::bind(this, &RunControlStateMachine::runControlMessageHandler, "Fail", XDAQ_NS_URI);
	xoap::bind(this, &RunControlStateMachine::runControlMessageHandler, RunControlStateMachine::ERROR_TRANSITION_NAME, XDAQ_NS_URI);

	xoap::bind(this, &RunControlStateMachine::runControlMessageHandler, "AsyncError", XDAQ_NS_URI);
	xoap::bind(this, &RunControlStateMachine::runControlMessageHandler, "AsyncPauseException", XDAQ_NS_URI);

	reset();
}

//==============================================================================
RunControlStateMachine::~RunControlStateMachine(void) {}

//==============================================================================
void RunControlStateMachine::reset(void)
{
	__GEN_COUT__ << "Resetting RunControlStateMachine with name '" << theStateMachine_.getStateMachineName() << "'..." << __E__;
	theStateMachine_.setInitialState('I');
	theStateMachine_.reset();

	theStateMachine_.setErrorMessage("", false /*append*/);  // clear error message

	asyncFailureReceived_        = false;
	asyncPauseExceptionReceived_ = false;
	asyncStopExceptionReceived_  = false;
}  // end reset()

////==============================================================================
//(RunControlStateMachine::stateMachineFunction_t)
// RunControlStateMachine::getTransitionName( 		const toolbox::fsm::State from,
//		const std::string& transition)
//{
//	auto itFrom = stateTransitionFunctionTable_.find(from);
//	if(itFrom == stateTransitionFunctionTable_.end())
//	{
//		__GEN_SS__ <<	"Cannot find transition function from '" << from <<
//				"' with transition '" << transition << "!'" << __E__;
//		XCEPT_RAISE (toolbox::fsm::exception::Exception, ss.str());
//	}
//
//	auto itTrans = itFrom->second.find(transition);
//	if(itTrans == itFrom->second.end())
//	{
//		__GEN_SS__ <<	"Cannot find transition function from '" << from <<
//				"' with transition '" << transition << "!'" << __E__;
//		XCEPT_RAISE (toolbox::fsm::exception::Exception, ss.str());
//	}
//
//	return itTrans->second;
//}

//==============================================================================
// runControlMessageHandler
//	Handles the command broadcast message from the Gateway Supervisor
//	and maps the command to a transition function, allowing for multiple iteration
//	passes through the transition function.
xoap::MessageReference RunControlStateMachine::runControlMessageHandler(xoap::MessageReference message)
{
	__GEN_COUT__ << "Received... \t" << SOAPUtilities::translate(message) << std::endl;

	std::string command = SOAPUtilities::translate(message).getCommand();

	// get iteration index
	try
	{
		StringMacros::getNumber(SOAPUtilities::translate(message).getParameters().getValue("iterationIndex"), iterationIndex_);
	}
	catch(...)  // ignore errors and set iteration index to 0
	{
		__GEN_COUT__ << "Defaulting iteration index to 0." << __E__;
		iterationIndex_ = 0;
	}
	// get subIteration index
	try
	{
		StringMacros::getNumber(SOAPUtilities::translate(message).getParameters().getValue("subIterationIndex"), subIterationIndex_);
	}
	catch(...)  // ignore errors and set subIteration index to 0
	{
		__GEN_COUT__ << "Defaulting subIterationIndex_ index to 0." << __E__;
		subIterationIndex_ = 0;
	}

	// get retransmission indicator
	bool retransmittedCommand = false;
	try
	{
		retransmittedCommand = (SOAPUtilities::translate(message).getParameters().getValue("retransmission") == "1");
	}
	catch(...)  // ignore errors for retransmission indicator (assume it is not a retransmission)
	{
		;
	}

	if(retransmittedCommand)
	{
		// handle retransmission

		// attempt to stop an error if last command was same
		if(lastIterationCommand_ == command && lastIterationIndex_ == iterationIndex_ && lastSubIterationIndex_ == subIterationIndex_)
		{
			__GEN_COUT__ << "Assuming a timeout occurred at Gateway waiting for a response. "
			             << "Attempting to avoid error, by giving last result for command '" << command << "': " << lastIterationResult_ << __E__;
			try
			{
				return SOAPUtilities::makeSOAPMessageReference(lastIterationResult_);
			}
			catch(const std::exception& e)  // if an illegal result ever propagates here, it is bug!
			{
				__GEN_COUT__ << "There was an illegal result propagation: " << lastIterationResult_ << ". Here was the error: " << e.what() << __E__;
				throw;
			}
		}
		else
			__GEN_COUT__ << "Looks like Gateway command '" << command << "' was lost - attempting to handle retransmission." << __E__;
	}

	lastIterationIndex_    = iterationIndex_;
	lastSubIterationIndex_ = subIterationIndex_;

	std::string currentState;
	if(iterationIndex_ == 0 && subIterationIndex_ == 0)
	{
		// this is the first iteration attempt for this transition
		theProgressBar_.reset(command, theStateMachine_.getStateMachineName());
		currentState = theStateMachine_.getCurrentStateName();
		__GEN_COUT__ << "Starting state for " << theStateMachine_.getStateMachineName() << " is " << currentState << " and attempting to " << command
		             << std::endl;
	}
	else
	{
		currentState = theStateMachine_.getStateName(lastIterationState_);

		__GEN_COUT__ << "Iteration index " << iterationIndex_ << "." << subIterationIndex_ << " for " << theStateMachine_.getStateMachineName() << " from "
		             << currentState << " attempting to " << command << std::endl;
	}

	RunControlStateMachine::theProgressBar_.step();

	std::string result   = command + "Done";
	lastIterationResult_ = result;

	// if error is received, immediately go to fail state
	//	likely error was sent by central FSM or external xoap
	if(command == "Error" || command == "Fail")
	{
		__GEN_SS__ << command << " was received! Halting immediately." << std::endl;
		__GEN_COUT_ERR__ << "\n" << ss.str();

		try
		{
			if(currentState == "Configured")
				theStateMachine_.execTransition("Halt", message);
			else if(currentState == "Running" || currentState == "Paused")
				theStateMachine_.execTransition("Abort", message);
		}
		catch(...)
		{
			__GEN_COUT_ERR__ << "Halting failed in reaction to " << command << "... ignoring." << __E__;
		}
		return SOAPUtilities::makeSOAPMessageReference(result);
	}
	else if(command == "AsyncError")
	{
		std::string errorMessage = SOAPUtilities::translate(message).getParameters().getValue("ErrorMessage");

		__GEN_SS__ << command << " was received! Error'ing immediately: " << errorMessage << std::endl;
		__GEN_COUT_ERR__ << "\n" << ss.str();
		theStateMachine_.setErrorMessage(ss.str());

		asyncFailureReceived_ = true;  // mark flag, to be used to abort next transition
		// determine any valid transition from where we are
		theStateMachine_.execTransition("fail");
		// XCEPT_RAISE (toolbox::fsm::exception::Exception, ss.str());

		return SOAPUtilities::makeSOAPMessageReference(result);
	}
	else if(command == "AsyncPauseException")
	{
		std::string errorMessage = SOAPUtilities::translate(message).getParameters().getValue("ErrorMessage");

		__GEN_SS__ << command << " was received! Pause'ing immediately: " << errorMessage << std::endl;
		__GEN_COUT_ERR__ << "\n" << ss.str();
		theStateMachine_.setErrorMessage(ss.str());

		if(!asyncPauseExceptionReceived_)  // launch pause only first time
		{
			asyncPauseExceptionReceived_ = true;  // mark flag, to be used to avoid double
			                                      // pausing and identify pause was due to
			                                      // soft error
			theStateMachine_.execTransition("Pause");
		}

		return SOAPUtilities::makeSOAPMessageReference(result);
	}
	else if(command == "AsyncStopException")
	{
		std::string errorMessage = SOAPUtilities::translate(message).getParameters().getValue("ErrorMessage");

		__GEN_SS__ << command << " was received! Stop'ing immediately: " << errorMessage << std::endl;
		__GEN_COUT_ERR__ << "\n" << ss.str();
		theStateMachine_.setErrorMessage(ss.str());

		if(!asyncStopExceptionReceived_)  // launch stop only first time
		{
			asyncStopExceptionReceived_ = true;  // mark flag, to be used to avoid double
			                                     // pausing and identify pause was due to
			                                     // soft error
			theStateMachine_.execTransition("Stop");
		}

		return SOAPUtilities::makeSOAPMessageReference(result);
	}

	// if already Halted, respond to Initialize with "done"
	//	(this avoids race conditions involved with artdaq mpi reset)
	if(command == "Initialize" && currentState == RunControlStateMachine::HALTED_STATE_NAME)
	{
		__GEN_COUT__ << "Already Initialized.. ignoring Initialize command." << std::endl;

		theStateMachine_.setErrorMessage("", false /*append*/);  // clear error message
		return SOAPUtilities::makeSOAPMessageReference(result);
	}

	if(command == "Halt" && currentState == RunControlStateMachine::FAILED_STATE_NAME)
	{
		__GEN_COUT__ << "Clearing Errors after failure..." << std::endl;
		theStateMachine_.setErrorMessage("", false /*append*/);  // clear error message
		asyncFailureReceived_ = false;
	}

	__GEN_COUTV__(command);
	__GEN_COUTV__(currentState);
	__GEN_COUTV__(asyncFailureReceived_);
	__GEN_COUTV__(asyncPauseExceptionReceived_);
	__GEN_COUTV__(asyncStopExceptionReceived_);
	__GEN_COUTV__(getErrorMessage());
	__GEN_COUTV__(retransmittedCommand);

	if(command == "Halt" && currentState == RunControlStateMachine::INITIAL_STATE_NAME)
	{
		__GEN_COUT__ << "Converting Halt command to Initialize, since currently in "
		                "Initialized state."
		             << std::endl;
		command = "Initialize";
		message = SOAPUtilities::makeSOAPMessageReference(command);
	}

	// handle normal transitions here
	try
	{
		// Clear error message if starting a normal transition.
		//	Do not clear soft PAUSE or harder STOP exception
		//	Do not clear if retransmission transition from Failed (likely an error just occured that we do not want to lose!)
		if(!((asyncPauseExceptionReceived_ && command == "Pause") || (asyncStopExceptionReceived_ && command == "Stop")))
			theStateMachine_.setErrorMessage("", false /*append*/);  // clear error message

		iterationWorkFlag_    = false;
		subIterationWorkFlag_ = false;
		if(iterationIndex_ || subIterationIndex_)
		{
			__GEN_COUT__ << command << " iteration " << iterationIndex_ << "." << subIterationIndex_ << __E__;
			toolbox::Event::Reference event(new toolbox::Event(command, this));

			// call inheriting transition function based on last state and command
			{
				// e.g. transitionConfiguring(event);
				__GEN_COUT__ << "Iterating on the transition function from " << currentState << " through " << lastIterationCommand_ << __E__;

				auto itFrom = stateTransitionFunctionTable_.find(lastIterationState_);
				if(itFrom == stateTransitionFunctionTable_.end())
				{
					__GEN_SS__ << "Cannot find transition function from '" << currentState << "' with transition '" << lastIterationCommand_ << "!'" << __E__;
					__GEN_COUT_ERR__ << ss.str();
					XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
				}

				auto itTransition = itFrom->second.find(lastIterationCommand_);
				if(itTransition == itFrom->second.end())
				{
					__GEN_SS__ << "Cannot find transition function from '" << currentState << "' with transition '" << lastIterationCommand_ << "!'" << __E__;
					__GEN_COUT_ERR__ << ss.str();
					XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
				}

				(this->*(itTransition->second))(event);  // call the transition function
			}
		}
		else
		{
			// save the lookup parameters for the last function to be called for the case
			// of additional iterations
			lastIterationState_   = theStateMachine_.getCurrentState();
			lastIterationCommand_ = command;
			if(command == RunControlStateMachine::CONFIGURE_TRANSITION_NAME)
			{
				lastAttemptedConfigureGroup_ = SOAPUtilities::translate(message).getParameters().getValue("ConfigurationAlias"); 

				//all entities but Gateway will have alias group name and key translation:
				try
				{
					lastAttemptedConfigureGroup_ += " " + SOAPUtilities::translate(message).getParameters().getValue("ConfigurationTableGroupName") + 
						"(" +  SOAPUtilities::translate(message).getParameters().getValue("ConfigurationTableGroupKey") + ")";
				}
				catch(...) { /* ignore missing parameters */ }
			}

			theStateMachine_.execTransition(command, message);
		}

		if(subIterationWorkFlag_)  // sub-iteration has priority over 'Working'
		{
			__GEN_COUTV__(subIterationWorkFlag_);
			result = command + "SubIterate";  // indicate another sub-iteration back to Gateway
		}
		else if(iterationWorkFlag_)
		{
			__GEN_COUTV__(iterationWorkFlag_);
			result = command + "Iterate";  // indicate another iteration back to Gateway
		}
	}
	catch(const std::runtime_error& e)
	{
		__GEN_SS__ << "Run Control Message Handling Failed with command '" << command << "': " << e.what() << " " << theStateMachine_.getErrorMessage() << __E__;
		__GEN_COUT_ERR__ << ss.str();
		theStateMachine_.setErrorMessage(ss.str());

		result = command + RunControlStateMachine::FAILED_STATE_NAME;
	}
	catch(toolbox::fsm::exception::Exception& e)
	{
		__GEN_SS__ << "Run Control Message Handling Failed with command '" << command << "': " << e.what() << " " << theStateMachine_.getErrorMessage() << __E__;
		__GEN_COUT_ERR__ << ss.str();
		theStateMachine_.setErrorMessage(ss.str());

		result = command + RunControlStateMachine::FAILED_STATE_NAME;
	}
	catch(...)
	{
		__GEN_SS__ << "Run Control Message Handling Failed  with command '" << command << "' and encountered an unknown error." << theStateMachine_.getErrorMessage() << __E__;
		try	{ throw; } //one more try to printout extra info
		catch(const std::exception &e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...){}
		__GEN_COUT_ERR__ << ss.str();
		theStateMachine_.setErrorMessage(ss.str());

		result = command + RunControlStateMachine::FAILED_STATE_NAME;
	}

	RunControlStateMachine::theProgressBar_.step();

	currentState = theStateMachine_.getCurrentStateName();

	if(currentState == RunControlStateMachine::FAILED_STATE_NAME)
	{
		result = command + RunControlStateMachine::FAILED_STATE_NAME;
		__GEN_COUT_ERR__ << "Unexpected Failure state for " << theStateMachine_.getStateMachineName() << " is " << currentState << std::endl;
		__GEN_COUT_ERR__ << "Error message was as follows: " << theStateMachine_.getErrorMessage() << std::endl;
	}

	RunControlStateMachine::theProgressBar_.step();

	if(!iterationWorkFlag_ && !subIterationWorkFlag_)
		theProgressBar_.complete();
	else
	{
		__GEN_COUTV__(theProgressBar_.read());
		__GEN_COUTV__(theProgressBar_.isComplete());
	}

	__GEN_COUT__ << "Ending state for " << theStateMachine_.getStateMachineName() << " is " << currentState << std::endl;
	__GEN_COUT__ << "result = " << result << std::endl;
	lastIterationResult_ = result;
	return SOAPUtilities::makeSOAPMessageReference(result);
}  // end runControlMessageHandler()
