#ifndef _ots_VStateMachine_h_
#define _ots_VStateMachine_h_

#include <string>

namespace ots
{
class CoreSupervisorBase;

class VStateMachine
{
  public:
	VStateMachine(const std::string& name)
	    : subsystemIterationIndex_(0)
	    , iterationIndex_(0)
	    , subIterationIndex_(0)
	    , subsystemIterationWorkFlag_(false)
	    , iterationWorkFlag_(false)
	    , subIterationWorkFlag_(false)
	    , name_(name)
	{
		;
	}
	virtual ~VStateMachine(void) { ; }

	/// Transitions
	virtual void configure(void)              = 0;
	virtual void halt(void)                   = 0;
	virtual void pause(void)                  = 0;
	virtual void resume(void)                 = 0;
	virtual void start(std::string runNumber) = 0;
	virtual void stop(void)                   = 0;

	/// States
	virtual bool running(void) { return false; }
	virtual void paused(void) { ; }
	virtual void halted(void) { ; }
	virtual void configured(void) { ; }
	virtual void initial(void) { ; }
	virtual void inError(void) { ; }

	/// Status
	//==============================================================================
	/// virtual progress detail string that can be overridden with more info
	///	e.g. step and sub-step aliases, etc
	virtual std::string getStatusProgressDetail(void)
	{
		std::string progress = "";

		if(VStateMachine::getSubIterationWork())
		{
			// sub-steps going on — show subsystemIteration:iteration:subIteration
			progress += name_ + ":";

			// check for subsystem-iteration alias
			if(subsystemIterationAliasMap_.find(transitionName_) != subsystemIterationAliasMap_.end() &&
			   subsystemIterationAliasMap_.at(transitionName_)
			           .find(VStateMachine::getSubsystemIterationIndex()) !=
			       subsystemIterationAliasMap_.at(transitionName_).end())
				progress += subsystemIterationAliasMap_.at(transitionName_)
				                .at(VStateMachine::getSubsystemIterationIndex());
			else
				progress +=
				    std::to_string(VStateMachine::getSubsystemIterationIndex());

			progress += ":";

			// check for iteration alias
			if(iterationAliasMap_.find(transitionName_) != iterationAliasMap_.end() &&
			   iterationAliasMap_.at(transitionName_)
			           .find(VStateMachine::getIterationIndex()) !=
			       iterationAliasMap_.at(transitionName_).end())
				progress += iterationAliasMap_.at(transitionName_)
				                .at(VStateMachine::getIterationIndex());
			else
				progress +=
				    std::to_string(VStateMachine::getIterationIndex());  ///< just index

			progress += ":";

			// check for sub-iteration alias
			if(subIterationAliasMap_.find(transitionName_) !=
			       subIterationAliasMap_.end() &&
			   subIterationAliasMap_.at(transitionName_)
			           .find(VStateMachine::getSubIterationIndex()) !=
			       subIterationAliasMap_.at(transitionName_).end())
				progress += subIterationAliasMap_.at(transitionName_)
				                .at(VStateMachine::getSubIterationIndex());
			else
				progress += std::to_string(
				    VStateMachine::getSubIterationIndex());  ///< just index
		}
		else if(VStateMachine::getIterationWork())
		{
			// iteration steps going on — show subsystemIteration:iteration
			progress += name_ + ":";

			// check for subsystem-iteration alias
			if(subsystemIterationAliasMap_.find(transitionName_) != subsystemIterationAliasMap_.end() &&
			   subsystemIterationAliasMap_.at(transitionName_)
			           .find(VStateMachine::getSubsystemIterationIndex()) !=
			       subsystemIterationAliasMap_.at(transitionName_).end())
				progress += subsystemIterationAliasMap_.at(transitionName_)
				                .at(VStateMachine::getSubsystemIterationIndex());
			else
				progress +=
				    std::to_string(VStateMachine::getSubsystemIterationIndex());

			progress += ":";

			// check for iteration alias
			if(iterationAliasMap_.find(transitionName_) != iterationAliasMap_.end() &&
			   iterationAliasMap_.at(transitionName_)
			           .find(VStateMachine::getIterationIndex()) !=
			       iterationAliasMap_.at(transitionName_).end())
				progress += iterationAliasMap_.at(transitionName_)
				                .at(VStateMachine::getIterationIndex());
			else
				progress +=
				    std::to_string(VStateMachine::getIterationIndex());  ///< just index
		}
		else if(VStateMachine::getSubsystemIterationWork())
		{
			// subsystem-iteration steps going on — show subsystemIteration only
			progress += name_ + ":";

			// check for subsystem-iteration alias
			if(subsystemIterationAliasMap_.find(transitionName_) != subsystemIterationAliasMap_.end() &&
			   subsystemIterationAliasMap_.at(transitionName_)
			           .find(VStateMachine::getSubsystemIterationIndex()) !=
			       subsystemIterationAliasMap_.at(transitionName_).end())
				progress += subsystemIterationAliasMap_.at(transitionName_)
				                .at(VStateMachine::getSubsystemIterationIndex());
			else
				progress +=
				    std::to_string(VStateMachine::getSubsystemIterationIndex());
		}
		else if(transitionName_ != "")
			progress += name_ + ":" + transitionName_;
		// else Done

		// if(progress.size())
		//	__COUTV__(progress);

		return progress;
	}  // end getStatusProgressDetail()

	virtual unsigned int getMinReadyForEventGenerationStartIteration(void) const { return 0; }

	void setTransitionName(const std::string& transitionName)
	{
		transitionName_ = transitionName;
	}
	const std::string& getTransitionName(void) { return transitionName_; }

	// Subsystem-iteration accessors (outermost tier — synchronized across subsystems)
	void               setSubsystemIterationIndex(unsigned int i) { subsystemIterationIndex_ = i; }
	unsigned int       getSubsystemIterationIndex(void) { return subsystemIterationIndex_; }
	void               indicateSubsystemIterationWork(void) { subsystemIterationWorkFlag_ = true; }
	void               clearSubsystemIterationWork(void) { subsystemIterationWorkFlag_ = false; }
	bool               getSubsystemIterationWork(void) { return subsystemIterationWorkFlag_; }

	// Iteration accessors (middle tier — synchronized within one subsystem)
	void               setIterationIndex(unsigned int i) { iterationIndex_ = i; }
	void               setSubIterationIndex(unsigned int i) { subIterationIndex_ = i; }
	unsigned int       getIterationIndex(void) { return iterationIndex_; }
	unsigned int       getSubIterationIndex(void) { return subIterationIndex_; }
	void               setSystemMinReadyForEventGenerationStartIteration(unsigned int i) { systemMinReadyForEventGenerationStartIteration_ = i; }
	unsigned int       getSystemMinReadyForEventGenerationStartIteration(void) const { return systemMinReadyForEventGenerationStartIteration_; }
	void               indicateIterationWork(void) { iterationWorkFlag_ = true; }
	void               clearIterationWork(void) { iterationWorkFlag_ = false; }
	bool               getIterationWork(void) { return iterationWorkFlag_; }

	// Sub-iteration accessors (innermost tier — internal to one application)
	void               indicateSubIterationWork(void) { subIterationWorkFlag_ = true; }
	void               clearSubIterationWork(void) { subIterationWorkFlag_ = false; }
	bool               getSubIterationWork(void) { return subIterationWorkFlag_; }

	CoreSupervisorBase* parentSupervisor_ = nullptr;  ///< e.g. to communicate error fault and start
	                                                  ///< transition to error for entire system
  protected:
	std::map<std::string /*transition*/,
	         std::map<unsigned int /*step index*/, std::string /*step alias*/>>
	    subsystemIterationAliasMap_;
	std::map<std::string /*transition*/,
	         std::map<unsigned int /*step index*/, std::string /*step alias*/>>
	    iterationAliasMap_;
	std::map<std::string /*transition*/,
	         std::map<unsigned int /*step index*/, std::string /*step alias*/>>
	    subIterationAliasMap_;

  private:
	unsigned int      subsystemIterationIndex_, iterationIndex_, subIterationIndex_;
	unsigned int      systemMinReadyForEventGenerationStartIteration_ = 0;
	bool              subsystemIterationWorkFlag_, iterationWorkFlag_, subIterationWorkFlag_;
	const std::string name_;
	std::string       transitionName_;
};

}  // namespace ots

#endif
