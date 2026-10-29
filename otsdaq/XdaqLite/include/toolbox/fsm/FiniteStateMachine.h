// OTS_LITE stand-in for xdaq's toolbox/fsm/FiniteStateMachine.h.
// Provides exactly the surface ots::FiniteStateMachine / RunControlStateMachine use, with the
// same return types as real xdaq (getStates -> vector, getInputs -> set, getTransitions -> map).
#ifndef _toolbox_fsm_FiniteStateMachine_h_
#define _toolbox_fsm_FiniteStateMachine_h_

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

#include "toolbox/Event.h"
#include "toolbox/fsm/FailedEvent.h"
#include "toolbox/fsm/exception/Exception.h"

namespace toolbox
{
namespace fsm
{
class FiniteStateMachine
{
  public:
	FiniteStateMachine() : currentState_(0), initialState_(0) {}
	virtual ~FiniteStateMachine() {}

	// --- state definition -------------------------------------------------
	void addState(State s, const std::string& name)
	{
		if(std::find(states_.begin(), states_.end(), s) == states_.end())
			states_.push_back(s);
		stateNames_[s] = name;
	}
	template<class OBJECT>
	void addState(State s, const std::string& name, OBJECT* obj, void (OBJECT::*func)(toolbox::fsm::FiniteStateMachine&))
	{
		addState(s, name);
		stateActions_[s] = [obj, func](FiniteStateMachine& fsm) { (obj->*func)(fsm); };
	}
	void setStateName(State s, const std::string& name) { stateNames_[s] = name; }

	template<class OBJECT>
	void addStateTransition(State from, State to, const std::string& input, OBJECT* obj, void (OBJECT::*func)(toolbox::Event::Reference))
	{
		stateTransitionTable_[from][input] = to;
		transitionActions_[from][input]    = [obj, func](toolbox::Event::Reference e) { (obj->*func)(e); };
	}
	void addStateTransition(State from, State to, const std::string& input) { stateTransitionTable_[from][input] = to; }

	template<class OBJECT>
	void setFailedStateTransitionAction(OBJECT* obj, void (OBJECT::*func)(toolbox::Event::Reference))
	{
		failedAction_ = [obj, func](toolbox::Event::Reference e) { (obj->*func)(e); };
	}
	template<class OBJECT>
	void setFailedStateTransitionChanged(OBJECT* obj, void (OBJECT::*func)(toolbox::fsm::FiniteStateMachine&))
	{
		failedChanged_ = [obj, func](FiniteStateMachine& fsm) { (obj->*func)(fsm); };
	}

	void setInitialState(State s) { initialState_ = s; }
	void reset()
	{
		currentState_ = initialState_;
		if(stateActions_.count(currentState_))
			stateActions_[currentState_](*this);
	}

	// --- queries ------------------------------------------------------------
	State       getCurrentState() const { return currentState_; }
	std::string getStateName(State s) const
	{
		auto it = stateNames_.find(s);
		return it == stateNames_.end() ? std::string(1, s) : it->second;
	}
	std::vector<State>    getStates() const { return states_; }
	std::set<std::string> getInputs(State s) const
	{
		std::set<std::string> r;
		auto                  it = stateTransitionTable_.find(s);
		if(it != stateTransitionTable_.end())
			for(const auto& p : it->second)
				r.insert(p.first);
		return r;
	}
	std::set<std::string>        getInputs() const { return getInputs(currentState_); }
	std::map<std::string, State> getTransitions(State s) const
	{
		auto it = stateTransitionTable_.find(s);
		return it == stateTransitionTable_.end() ? std::map<std::string, State>() : it->second;
	}

	// --- driving ------------------------------------------------------------
	// Fires a transition on the current state. Transition handler exceptions move the
	// machine to the failed state ('F') and invoke the failed-state hooks, as xdaq does.
	void fireEvent(toolbox::Event::Reference e)
	{
		const std::string input = e->type();
		auto              row   = stateTransitionTable_.find(currentState_);
		if(row == stateTransitionTable_.end() || row->second.find(input) == row->second.end())
			XCEPT_RAISE(toolbox::fsm::exception::Exception, "Invalid input '" + input + "' for state '" + getStateName(currentState_) + "'");

		State from = currentState_;
		State to   = row->second[input];
		try
		{
			auto act = transitionActions_.find(from);
			if(act != transitionActions_.end() && act->second.count(input))
				act->second[input](e);
		}
		catch(xcept::Exception& ex)
		{
			currentState_ = 'F';
			toolbox::Event::Reference failed(new toolbox::fsm::FailedEvent(ex, from, to, this));
			if(failedAction_)
				failedAction_(failed);
			if(failedChanged_)
				failedChanged_(*this);
			throw;
		}
		currentState_ = to;
		if(stateActions_.count(currentState_))
			stateActions_[currentState_](*this);
	}

  protected:
	State                                                              currentState_;
	State                                                              initialState_;
	std::vector<State>                                                 states_;
	std::map<State, std::string>                                       stateNames_;
	std::map<State, std::map<std::string, State>>                      stateTransitionTable_;
	std::map<State, std::function<void(FiniteStateMachine&)>>          stateActions_;
	std::map<State, std::map<std::string, std::function<void(toolbox::Event::Reference)>>> transitionActions_;
	std::function<void(toolbox::Event::Reference)>                     failedAction_;
	std::function<void(FiniteStateMachine&)>                           failedChanged_;
};
}  // namespace fsm
}  // namespace toolbox

#endif
