#ifndef _ots_Iterator_h
#define _ots_Iterator_h

#include <mutex>  //for std::mutex
#include <string>
#include "otsdaq/TablePlugins/IterateTable.h"
#include "otsdaq/XmlUtilities/HttpXmlDocument.h"

#include "otsdaq/ConfigurationInterface/ConfigurationManagerRW.h"

namespace ots
{
class GatewaySupervisor;
class ConfigurationManagerRW;

class Iterator
{
	friend class GatewaySupervisor;

  public:
	Iterator(GatewaySupervisor* supervisor);
	~Iterator(void);

	void playIterationPlan(HttpXmlDocument& xmldoc, const std::string& planName);
	void pauseIterationPlan(HttpXmlDocument& xmldoc);
	void haltIterationPlan(HttpXmlDocument& xmldoc);
	void getIterationPlanStatus(HttpXmlDocument& xmldoc);

	bool handleCommandRequest(HttpXmlDocument& xmldoc, const std::string& command, const std::string& parameter);

  private:
	// begin declaration of iterator workloop members
	struct IteratorWorkLoopStruct
	{
		IteratorWorkLoopStruct(Iterator* iterator, ConfigurationManagerRW* cfgMgr)
		    : theIterator_(iterator)
		    , cfgMgr_(cfgMgr)
		    , originalTrackChanges_(false)
		    , running_(false)
		    , commandBusy_(false)
		    , doPauseAction_(false)
		    , doHaltAction_(false)
		    , doResumeAction_(false)
		    , commandIndex_((unsigned int)-1)
		{
		}

		Iterator*               theIterator_;
		ConfigurationManagerRW* cfgMgr_;
		bool                    originalTrackChanges_;
		std::string             originalConfigGroup_;
		TableGroupKey           originalConfigKey_;

		bool running_, commandBusy_;
		bool doPauseAction_, doHaltAction_, doResumeAction_;

		std::string                        activePlan_;
		std::vector<IterateTable::Command> commands_;
		std::vector<unsigned int>          commandIterations_;
		unsigned int                       commandIndex_;
		std::vector<unsigned int>          stepIndexStack_;
		time_t                             originalDurationInSeconds_;

		// associated with FSM
		std::string  fsmName_, fsmRunAlias_;
		unsigned int fsmNextRunNumber_;
		bool         runIsDone_;

		std::vector<std::string> fsmCommandParameters_;
		std::vector<bool>        targetsDone_;

	};  // end declaration of iterator workloop members

	static void IteratorWorkLoop(Iterator* iterator);
	static void startCommand(IteratorWorkLoopStruct* iteratorStruct);
	static bool checkCommand(IteratorWorkLoopStruct* iteratorStruct);

	static void startCommandChooseFSM(IteratorWorkLoopStruct* iteratorStruct, const std::string& fsmName);

	static void startCommandConfigureActive(IteratorWorkLoopStruct* iteratorStruct);
	static void startCommandConfigureAlias(IteratorWorkLoopStruct* iteratorStruct, const std::string& systemAlias);
	static void startCommandConfigureGroup(IteratorWorkLoopStruct* iteratorStruct);
	static bool checkCommandConfigure(IteratorWorkLoopStruct* iteratorStruct);

	static void startCommandModifyActive(IteratorWorkLoopStruct* iteratorStruct);

	static void startCommandMacro(IteratorWorkLoopStruct* iteratorStruct, bool isFEMacro);
	static bool checkCommandMacro(IteratorWorkLoopStruct* iteratorStruct, bool isFEMacro);

	static void startCommandBeginLabel(IteratorWorkLoopStruct* iteratorStruct);
	static void startCommandRepeatLabel(IteratorWorkLoopStruct* iteratorStruct);

	static void startCommandRun(IteratorWorkLoopStruct* iteratorStruct);
	static bool checkCommandRun(IteratorWorkLoopStruct* iteratorStruct);

	static void startCommandFSMTransition(IteratorWorkLoopStruct* iteratorStruct, const std::string& transitionCommand);
	static bool checkCommandFSMTransition(IteratorWorkLoopStruct* iteratorStruct, const std::string& finalState);

	static bool haltIterator(Iterator*               iterator,
	                         IteratorWorkLoopStruct* iteratorStruct = 0,
							 bool 					  doNotHaltFSM = false);

	std::mutex    accessMutex_;
	volatile bool workloopRunning_;
	volatile bool activePlanIsRunning_;
	volatile bool iteratorBusy_;
	volatile bool commandPlay_, commandPause_,
	    commandHalt_;  // commands are set by
	                   // supervisor thread, and
	                   // cleared by iterator thread
	std::string               activePlanName_, lastStartedPlanName_, lastFinishedPlanName_;
	volatile unsigned int     activeCommandIndex_, activeCommandIteration_;
	std::vector<unsigned int> depthIterationStack_;
	volatile time_t           activeCommandStartTime_;
	std::string               lastFsmName_;
	std::string               errorMessage_;

	GatewaySupervisor* theSupervisor_;

	template<class T>  // defined in included .icc source
	static void helpCommandModifyActive(IteratorWorkLoopStruct* iteratorStruct, const T& setValue, bool doTrackGroupChanges);
};

#include "otsdaq/GatewaySupervisor/Iterator.icc"  //for template definitions

}  // namespace ots

#endif
