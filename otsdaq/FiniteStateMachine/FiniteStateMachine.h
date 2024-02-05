#ifndef _ots_FiniteStateMachine_h
#define _ots_FiniteStateMachine_h

#include <toolbox/fsm/FiniteStateMachine.h>
#include <xoap/MessageReference.h>

// clang-format off
namespace ots
{
class FiniteStateMachine : public toolbox::fsm::FiniteStateMachine
{
  public:
	FiniteStateMachine					(const std::string& stateMachineName);
	~FiniteStateMachine					(void);

	using toolbox::fsm::FiniteStateMachine::addStateTransition;

	template<class OBJECT>
	void addStateTransition(toolbox::fsm::State from,
	                        toolbox::fsm::State to,
	                        const std::string&  input,
	                        const std::string&  transitionName,
	                        OBJECT*             obj,
	                        void (OBJECT::*func)(toolbox::Event::Reference))

	{
		stateTransitionNameTable_[from][input] = transitionName;
		toolbox::fsm::FiniteStateMachine::addStateTransition(from, to, input, obj, func);
	}

	template<class OBJECT>
	void 					addStateTransition			(toolbox::fsm::State from,
														toolbox::fsm::State to,
														const std::string&  input,
														const std::string&  transitionName,
														const std::string&  transitionParameter,
														OBJECT*             obj,
														void (OBJECT::*func)(toolbox::Event::Reference))

	{
		stateTransitionParameterTable_[from][input] = transitionParameter;
		addStateTransition(from, to, input, transitionName, obj, func);
	}

	toolbox::fsm::State 			getProvenanceState			(void);
	toolbox::fsm::State 			getTransitionFinalState		(const std::string& transition);

	std::string        				getProvenanceStateName		(void);
	std::string        				getCurrentStateName			(void);
	time_t             				getTimeInState				(void);
	std::string        				getCurrentTransitionName	(const std::string& transition = "");
	std::string        				getTransitionName			(const toolbox::fsm::State from, const std::string& transition);
	std::string        				getTransitionParameter		(const toolbox::fsm::State from, const std::string& transition);
	std::string        				getTransitionFinalStateName	(const std::string& transition);
	const std::string& 				getErrorMessage				(void) const;
	const std::string& 				getStateMachineName			(void) const { return stateMachineName_; }
	void               				setStateMachineName			(const std::string& name) { stateMachineName_ = name; }

	const xoap::MessageReference& 	getCurrentMessage			(void) { return theMessage_; }

	bool 							execTransition				(const std::string& transition);
	bool 							execTransition				(const std::string& transition, const xoap::MessageReference& message);
	bool 							isInTransition				(void);
	void 							setInitialState				(toolbox::fsm::State state);
	void 							setErrorMessage				(const std::string& errMessage, bool append = true);

	static const std::string 				FAILED_STATE_NAME;
  protected:
	time_t stateEntranceTime_;

	// The volatile keyword indicates that a field might be modified by multiple
	// concurrently executing threads.  Fields that are declared volatile are not subject
	// to compiler optimizations that assume access by a single thread.  This ensures that
	// the most up-to-date value is present in the field at all times.  If you don't mark
	// it volatile, the generated code might optimize the value into a registry and your
	// thread will never see the change  The atomicity has nothing to do with the
	// visibility between threads...  just because an operation is executed in one CPU
	// cycle (atomic) it doesn't mean that the result will be visible to the other threads
	// unless the value is marked volatile
	volatile bool                              	inTransition_;
	toolbox::fsm::State                         provenanceState_;
	std::string									currentTransition_;
	std::map<toolbox::fsm::State, 
		std::map<std::string, std::string, 
		std::less<std::string> > > 				stateTransitionNameTable_;
	std::map<toolbox::fsm::State, 
		std::map<std::string, std::string, 
		std::less<std::string> > > 				stateTransitionParameterTable_;

	xoap::MessageReference 						theMessage_;
	std::string            						theErrorMessage_;
	std::string            						stateMachineName_;
	

  private:
};
// clang-format on
}  // namespace ots

#endif
