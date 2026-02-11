#ifndef _ots_RunInfoVInterface_h_
#define _ots_RunInfoVInterface_h_

#include <string>
#include "otsdaq/Configurable/Configurable.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/StringMacros.h"

namespace ots
{

// clang-format off

/// ~~ RunInfoVInterface expected flow (managed by GatewaySupervisor) ~~:
///
///		Configure transtion:
///			- insertConfigureCondition(<local configure blob>, <comment>)
///				==> Run Info plugin records configure conditions associated with this Configure transition.
///					The blob is local to the parent GatewaySupervisor (i.e. not from subsystems, as subsystems could configure asynchronously)
///
///		Pre-start transtion:
///			- claimNextRunNumber(<configure condition ID>, <comment>)
///				==> Run Info plugin retrieves/returns next run number from run number
///
///		During start transtion:
///			- Gateway Supervisor collects configure blobs from all subsystem Gateways
///				==> Run Info plugin not involved
///
///		End of start transtion:
/// 		- insertRunCondition(<run number>, <map of blob from all subsystems>, <configure condition ID>, <comment>)
///				==> Run Info plugin records run conditions as desired associated with run number)
///
///		In stop/pause/resume/halt/error transtions:
/// 		- updateRunInfo(<run number>, <transition type>, <comment>)
///				==> Run Info plugin records run transition as desired associated with run number)
///
class RunInfoVInterface  ///< : public Configurable
{
  public:
	enum class RunTransitionType
	{
		HALT,
		STOP,
		ERROR,
		PAUSE,
		RESUME,
		START
	};

	RunInfoVInterface							(const std::string& runInfoPluginClassName,
												const std::string& activeStateMachineName)
		:
		mfSubject_(runInfoPluginClassName),
		activeStateMachineName_(activeStateMachineName)
	{;}
	virtual ~RunInfoVInterface						(void) { ; }



	/// Set functions ----

	virtual unsigned int 	insertConfigureCondition	(const std::string&  /*blob*/,
														 const std::string&  /*comment*/) 					{ __SS__ << "insertConfigureCondition() Not implemented by the Run Info Plugin (" << mfSubject_ << ")!!"; __SS_THROW__; };
	virtual unsigned int 	claimNextRunNumber			(unsigned int        /* configureConditionID */,
														 const std::string&  /* comment */) 				{ __SS__ << "claimNextRunNumber() Not implemented by the Run Info Plugin (" << mfSubject_ << ")!!"; __SS_THROW__; };
	virtual unsigned int 	insertRunCondition			(unsigned int        /* runNumber */,
														 const std::map<std::string /* subsystem */,
															std::map<std::string /*type/name/field */,
																std::string  /* value */>>&
																			 /* runConditionMap */,
														 unsigned int        /* configureConditionID */,
														 const std::string&  /* comment */) 				{ __SS__ << "insertRunCondition() Not implemented by the Run Info Plugin (" << mfSubject_ << ")!!"; __SS_THROW__; };

	virtual void 			updateRunInfo				(unsigned int        /* runConditionID */,
														 RunTransitionType   /* runTransitionType */,
														 const std::string&  /* comment */) 				{ __SS__ << "updateRunInfo() Not implemented by the Run Info Plugin (" << mfSubject_ << ")!!"; __SS_THROW__; };



	/// Get functions ----

	const std::string& 		getActiveStateMachineName	(void) const { return activeStateMachineName_; }

	//start queryFilter with 'AND' to fiter more the selection
	virtual std::vector<std::vector<std::string>>
							getRunRecords				(unsigned int        /* startTime */,
														 unsigned int        /* endTime */,
														 const std::string&  queryFilter = "") 				{ __SS__ << "getRunRecords() Not implemented by the Run Info Plugin (" << mfSubject_ << ")!!" << (queryFilter == ""/* use variable */?"":""); __SS_THROW__;};

	virtual std::vector<std::vector<std::string>>
							getRunConditionByID			(uint64_t 			 /* conditionID*/) 				{ __SS__ << "getRunConditionByID() Not implemented by the Run Info Plugin (" << mfSubject_ << ")!!"; __SS_THROW__; };

	virtual std::vector<std::vector<std::string>>
							getRunConfigSubsystemInfo	(uint64_t 			 /* configID */) 				{ __SS__ << "getRunConfigSubsystemInfo() Not implemented by the Run Info Plugin (" << mfSubject_ << ")!!"; __SS_THROW__; };

  protected:
	const std::string 		mfSubject_; ///< Unique identifier for decorating trace printouts
	const std::string 		activeStateMachineName_;
};
// clang-format on

}  // namespace ots

#endif
