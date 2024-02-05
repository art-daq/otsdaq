#include "otsdaq/FiniteStateMachine/FiniteStateMachine.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include "otsdaq/Macros/CoutMacros.h"

#include <map>
#include <sstream>

using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "FSM"
#define mfSubject_ std::string("FSM-") + getStateMachineName()

const std::string FiniteStateMachine::FAILED_STATE_NAME = "Failed";

//==============================================================================
FiniteStateMachine::FiniteStateMachine(const std::string& stateMachineName)
    : stateEntranceTime_(0), inTransition_(false), provenanceState_('X'), theErrorMessage_(""), stateMachineName_(stateMachineName)
{
	__GEN_COUT__ << "Constructing FiniteStateMachine" << __E__;
}  // end constructor()

//==============================================================================
FiniteStateMachine::~FiniteStateMachine(void) {}

//==============================================================================
toolbox::fsm::State FiniteStateMachine::getProvenanceState(void) { return provenanceState_; }

//==============================================================================
toolbox::fsm::State FiniteStateMachine::getTransitionFinalState(const std::string& transition)
{
	if(stateTransitionTable_[currentState_].find(transition) != stateTransitionTable_[currentState_].end())
		return stateTransitionTable_[currentState_][transition];
	else
	{
		__GEN_SS__ << "Cannot find transition name with transition: " << transition << ", unknown!";
		__GEN_COUT__ << ss.str();
		XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
	}
}  // end getTransitionFinalState()

//==============================================================================
std::string FiniteStateMachine::getProvenanceStateName(void) { return getStateName(getProvenanceState()); }

//==============================================================================
std::string FiniteStateMachine::getCurrentStateName(void) { return getStateName(getCurrentState()); }

//==============================================================================
// getTimeInState
//	returns number of seconds elapsed while in current state
//	returns 0 if invalid (i.e. stateEntranceTime_ is not set - stateEntranceTime_ is
// initialized to 0)
time_t FiniteStateMachine::getTimeInState(void) { return stateEntranceTime_ ? (time(0) - stateEntranceTime_) : 0; }

//==============================================================================
std::string FiniteStateMachine::getCurrentTransitionName(const std::string& transition)
{
	// __GEN_COUTV__(stateTransitionNameTable_.size());
	// __GEN_SS__ << currentState_ << " " << transition <<  " " << currentTransition_ << __E__;
	// __GEN_COUT__ << ss.str();
	// __GEN_COUT__ << (stateTransitionNameTable_.find(currentState_) == stateTransitionNameTable_.end()?1:0);
	// __GEN_COUT__ << (stateTransitionNameTable_.at(currentState_).find(transition) ==
	// 	 stateTransitionNameTable_.at(currentState_).end()?1:0);

	// for(auto startState : stateTransitionNameTable_)
	// {
	// 	__GEN_COUTV__(getStateName(startState.first));
	// 	std::cout << startState.first << __E__;
	// 	for(auto trans : stateTransitionNameTable_.at(startState.first))
	// 		__GEN_COUT__ << "\t" << trans.first <<  " " << trans.second << __E__;
	// }

	// if looking for current active transition, calculate from provenance state
	if(transition == "")
	{
		if(stateTransitionNameTable_.at(provenanceState_).find(currentTransition_) != stateTransitionNameTable_.at(provenanceState_).end())
			return stateTransitionNameTable_.at(provenanceState_).at(currentTransition_);
		else
		{
			__GEN_SS__ << "Cannot find transition name from '" << getProvenanceStateName() << "' with transition: " << currentTransition_ << "...";
			__GEN_COUT_ERR__ << ss.str();
			XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
		}
	}

	if(stateTransitionNameTable_.at(currentState_).find(transition) != stateTransitionNameTable_.at(currentState_).end())
	{
		return stateTransitionNameTable_.at(currentState_).at(transition);
	}
	else
	{
		__GEN_SS__ << "Cannot find transition name from '" << getCurrentStateName() << "' with transition: " << transition << "...";
		__GEN_COUT_ERR__ << ss.str();
		XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
	}
}  // end getCurrentTransitionName()

//==============================================================================
std::string FiniteStateMachine::getTransitionName(const toolbox::fsm::State from, const std::string& transition)
{
	if(stateTransitionNameTable_[from].find(transition) != stateTransitionNameTable_[from].end())
	{
		return stateTransitionNameTable_[from][transition];
	}
	else
	{
		std::ostringstream error;
		error << "Cannot find transition name from " << from << " with transition: " << transition << ", unknown!";
		XCEPT_RAISE(toolbox::fsm::exception::Exception, error.str());
	}
}  // end getTransitionName()

//==============================================================================
std::string FiniteStateMachine::getTransitionParameter(const toolbox::fsm::State from, const std::string& transition)
{
	if(stateTransitionParameterTable_[from].find(transition) != stateTransitionParameterTable_[from].end())
	{
		return stateTransitionParameterTable_[from][transition];
	}
	return "";
}  // end getTransitionParameter()

//==============================================================================
std::string FiniteStateMachine::getTransitionFinalStateName(const std::string& transition) { return getStateName(getTransitionFinalState(transition)); }

//==============================================================================
bool FiniteStateMachine::execTransition(const std::string& transition)
{
	const xoap::MessageReference message;
	return execTransition(transition, message);
}  // end execTransition()

//==============================================================================
// execTransition
//
//	Returns true if transition is successfully executed
//		else false if this exec did not complete a transition.
//
//	Note: For iteration handling, there is iterationIndex_ and iterationWorkFlag_.
//		These are different (higher level) than the members of VStateMachine.
bool FiniteStateMachine::execTransition(const std::string& transition, const xoap::MessageReference& message)
{
	__GEN_COUTV__(transition);

	if(transition == "fail")
	{
		while(inTransition_)
		{
			__GEN_COUT__ << "Currently in transition '" << currentTransition_ << "' executed from current state " << getProvenanceStateName()
			             << ". Attempting to wait for the transition to complete." << __E__;
			sleep(1);
		}
		sleep(1);

		if(getStateName(getCurrentState()) == FiniteStateMachine::FAILED_STATE_NAME)
		{
			__GEN_COUT_INFO__ << "Already failed. Current state: " << getStateName(getCurrentState()) << " last state: " << getProvenanceStateName() << __E__;
			return true;
		}
		__GEN_COUT_INFO__ << "Failing now!! Current state: " << getStateName(getCurrentState()) << " last state: " << getProvenanceStateName() << __E__;

		// find any valid transition and take it..
		//	all transition functions must check for a failure
		//	flag, and throw an exception to go to Fail state

		std::map<std::string, toolbox::fsm::State> transitions = getTransitions(getCurrentState());
		for(const auto& transitionPair : transitions)
		{
			__GEN_COUT__ << "Taking any transition to indirect failure.. found '" << transitionPair.first << "'" << __E__;
			__GEN_COUT__ << "Taking fail transition from Current state: " << getStateName(getCurrentState()) << " last state: " << getProvenanceStateName()
			             << __E__;
			toolbox::Event::Reference event(new toolbox::Event(transitionPair.first, this));

			try
			{
				this->fireEvent(event);
			}
			catch(toolbox::fsm::exception::Exception& e)
			{
				std::ostringstream error;
				error << "Transition " << transition << " was not executed from current state " << getStateName(getCurrentState())
				      << ". There was an error: " << e.what();
				__GEN_COUT_ERR__ << error.str() << __E__;
			}
			inTransition_      = false;
			stateEntranceTime_ = time(0);
			return true;
		}
		//		//XCEPT_RAISE (toolbox::fsm::exception::Exception, transition);
		//		theMessage_ = message;
		//		toolbox::Event::Reference event(new toolbox::Event(, this));
		//
	}

	if(inTransition_)
	{
		__GEN_COUT_WARN__ << "In transition, and received another transition: " << transition << ". Ignoring..." << __E__;
		return false;
	}
	inTransition_             = true;
	bool transitionSuccessful = true;
	provenanceState_          = getCurrentState();

	std::map<std::string, toolbox::fsm::State> transitions = getTransitions(getCurrentState());
	if(transitions.find(transition) == transitions.end())
	{
		inTransition_ = false;
		std::ostringstream error;
		error << transition << " is not in the list of the transitions from current state " << getStateName(getCurrentState());
		__GEN_COUT_ERR__ << error.str() << __E__;
		__GEN_COUTV__(getErrorMessage());
		XCEPT_RAISE(toolbox::fsm::exception::Exception, error.str());
		//__GEN_COUT__ << error << __E__;
		//		__GEN_COUT__ << "Transition?" << inTransition_ << __E__;
		return false;
	}

	// fire FSM event by calling mapped function
	//	(e.g. mapped by RunControlStateMachine and function implemented by
	//		CoreSupervisorBase class inheriting from RunControlStateMachine)
	try
	{
		toolbox::Event::Reference event(new toolbox::Event(transition, this));
		theMessage_ = message;  // Even if it is bad, there can only be 1 transition at a time
		                        // so this parameter should not change during all transition
		currentTransition_ = transition;

		this->fireEvent(event);
	}
	catch(toolbox::fsm::exception::Exception& e)
	{
		inTransition_        = false;
		transitionSuccessful = false;
		std::ostringstream error;
		__GEN_SS__ << "Transition " << transition << " was not executed from current state " << getStateName(getCurrentState())
		           << ". There was an error: " << e.what();
		__GEN_COUT_ERR__ << ss.str() << __E__;
		// diagService_->reportError(err.str(),DIAGERROR);

		// send state machine to error
		XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
	}
	catch(...)
	{
		inTransition_        = false;
		transitionSuccessful = false;
		__GEN_SS__ << "Transition " << transition << " was not executed from current state " << getStateName(getCurrentState())
		           << ". There was an unknown error.";
		try	{ throw; } //one more try to printout extra info
		catch(const std::exception &e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...){}
		__GEN_COUT_ERR__ << ss.str() << __E__;
		// diagService_->reportError(err.str(),DIAGERROR);

		// send state machine to error
		XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
	}

	inTransition_      = false;
	stateEntranceTime_ = time(0);
	return transitionSuccessful;
}  // end execTransition()

//==============================================================================
bool FiniteStateMachine::isInTransition(void) { return inTransition_; }

//==============================================================================
void FiniteStateMachine::setErrorMessage(const std::string& errMessage, bool append)
{
	if(append)
		theErrorMessage_ += errMessage;
	else
		theErrorMessage_ = errMessage;
}  // end setErrorMessage()

//==============================================================================
const std::string& FiniteStateMachine::getErrorMessage() const { return theErrorMessage_; }

//==============================================================================
void FiniteStateMachine::setInitialState(toolbox::fsm::State state)
{
	toolbox::fsm::FiniteStateMachine::setInitialState(state);
	provenanceState_   = state;
	stateEntranceTime_ = time(0);
}  // end setInitialState()
