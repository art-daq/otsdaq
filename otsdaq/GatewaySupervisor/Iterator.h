#ifndef _ots_Iterator_h
#define _ots_Iterator_h

#include <atomic>  //for std::atomic
#include <memory>  //for std::shared_ptr
#include <mutex>   //for std::mutex
#include <string>
#include "otsdaq/TablePlugins/IterateTable.h"
#include "otsdaq/XmlUtilities/HttpXmlDocument.h"

#include "otsdaq/ConfigurationInterface/ConfigurationManagerRW.h"

// clang-format off

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

	static const std::string RESERVED_GEN_PLAN_NAME;

	/// an output file written by a macro command during a plan (local FE or remote MacroMaker)
	struct OutputFile
	{
		std::string subsystem;  ///< "" for Self, else remote subsystem name
		std::string path;       ///< as reported by the writer (e.g. "$OTSDAQ_DATA//macroOutput_..txt" or absolute)
		std::string label;      ///< e.g. "DTC Read on Calo14_DTC0"
		std::string iconName;   ///< desktop icon name of the subsystem's Code Editor (remote only), for open-by-name
	};

	void 								playIterationPlan			(HttpXmlDocument& xmldoc, const std::string& planName);
	void 								playGeneratedIterationPlan	(HttpXmlDocument& xmldoc, const std::string& parametersCSV);
	void 								playGeneratedIterationPlan	(HttpXmlDocument& xmldoc, const std::string& fsmName, const std::string& configAlias, uint64_t durationSeconds = -1, unsigned int numberOfRuns = 1, bool keepConfiguration = false, const std::string& logEntry = "");
	void 								pauseIterationPlan			(HttpXmlDocument& xmldoc);
	void 								haltIterationPlan			(HttpXmlDocument& xmldoc);
	void 								getIterationPlanStatus		(HttpXmlDocument& xmldoc);


	bool 								handleCommandRequest		(HttpXmlDocument& xmldoc, const std::string& command, const std::string& parameter);

	bool								isIteratorBusy				(void) const { return iteratorBusy_; }
  private:

	void 								playIterationPlanPrivate	(HttpXmlDocument& xmldoc, const std::string& planName);
	static std::vector<
				IterateTable::Command> 	generateIterationPlan		(const std::string& fsmName, const std::string& configAlias, uint64_t durationSeconds = -1, unsigned int numberOfRuns = 1);

	/// begin declaration of iterator workloop members
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
		bool onlyConfigIfNotConfigured_ = false;

		std::string                        activePlan_;
		std::vector<IterateTable::Command> commands_;
		std::vector<unsigned int>          commandIterations_;
		unsigned int                       commandIndex_;
		std::vector<unsigned int>          stepIndexStack_;  ///< pass index per open BEGIN_LABEL (innermost last)
		std::vector<std::string>           stepLabelStack_;  ///< label names parallel to stepIndexStack_
		time_t                             originalDurationInSeconds_;

		// associated with FSM
		std::string  fsmName_, fsmRunAlias_;
		unsigned int fsmNextRunNumber_;
		bool         runIsDone_;
		bool 		 waitIsDone_;

		std::vector<std::string> fsmCommandParameters_;
		std::vector<bool>        targetsDone_;

		bool remoteConfigureQueued_ = false;  ///< remote Configure needs Halt first when not Halted/Initial

		std::vector<OutputFile> outputFiles_;  ///< files written by macro commands so far in this plan
		std::string             macroArgsSummary_;  ///< "arg = val, ..." of the local macro command in progress (for output file labels)

		/// state shared with the background thread driving a remote FE macro over MacroMaker UDP
		struct RemoteMacroRun
		{
			std::atomic<bool> done{false}, abort{false};
			std::mutex        mutex;  ///< guards error, iterationsDone, progress, outputFiles
			std::string       error;
			uint64_t          iterationsDone  = 0;
			uint64_t          iterationsTotal = 0;
			int               progress        = 0;  ///< percent of current iteration, from remote
			std::vector<std::pair<std::string /*path*/, std::string /*"arg = val, ..."*/>>
			    outputFiles;  ///< "Filename" reported by the remote MacroMaker per iteration, with that iteration's inputs
		};
		std::shared_ptr<RemoteMacroRun> remoteMacroRun_;

	};  // end declaration of iterator workloop members

	static void IteratorWorkLoop(Iterator* iterator);
	static void startCommand(IteratorWorkLoopStruct* iteratorStruct);
	static bool checkCommand(IteratorWorkLoopStruct* iteratorStruct);

	static void startCommandChooseFSM(IteratorWorkLoopStruct* iteratorStruct, const std::string& fsmName);

	static bool commandSkipsIfConfigured(IteratorWorkLoopStruct* iteratorStruct);
	static void startCommandConfigureActive(IteratorWorkLoopStruct* iteratorStruct);
	static void startCommandConfigureAlias(IteratorWorkLoopStruct* iteratorStruct, const std::string& systemAlias);
	static void startCommandConfigureGroup(IteratorWorkLoopStruct* iteratorStruct);
	static bool checkCommandConfigure(IteratorWorkLoopStruct* iteratorStruct);

	static void startCommandModifyActive(IteratorWorkLoopStruct* iteratorStruct);

	static void startCommandMacro(IteratorWorkLoopStruct* iteratorStruct, bool isFEMacro);
	static bool checkCommandMacro(IteratorWorkLoopStruct* iteratorStruct, bool isFEMacro);

	static void startCommandBeginLabel(IteratorWorkLoopStruct* iteratorStruct);
	static void startCommandRepeatLabel(IteratorWorkLoopStruct* iteratorStruct);

	static void        startCommandRun(IteratorWorkLoopStruct* iteratorStruct);
	static bool        checkCommandRun(IteratorWorkLoopStruct* iteratorStruct);
	static std::string buildIteratorLogEntry(IteratorWorkLoopStruct* iteratorStruct);

	static void startCommandWait(IteratorWorkLoopStruct* iteratorStruct);
	static bool checkCommandWait(IteratorWorkLoopStruct* iteratorStruct);

	static void startCommandFSMTransition(IteratorWorkLoopStruct* iteratorStruct, const std::string& transitionCommand);
	static bool checkCommandFSMTransition(IteratorWorkLoopStruct* iteratorStruct, const std::string& finalState);

	static void startRemoteCommandFSMTransition(IteratorWorkLoopStruct* iteratorStruct, const std::string& transitionCommand, const std::string& targetSubsystem);
	static bool checkRemoteCommandFSMTransition(IteratorWorkLoopStruct* iteratorStruct, const std::string& finalState, const std::string& targetSubsystem);

	static void startRemoteCommandConfigure(IteratorWorkLoopStruct* iteratorStruct, const std::string& systemAlias, const std::string& targetSubsystem);
	static bool checkRemoteCommandConfigure(IteratorWorkLoopStruct* iteratorStruct, const std::string& targetSubsystem);

	static void startRemoteCommandMacro(IteratorWorkLoopStruct* iteratorStruct, bool isFEMacro);
	/// FE macro input name with any "(Default/Note)" suffix removed, for order-independent matching
	static std::string feMacroArgBaseName(const std::string& argName);
	static bool checkRemoteCommandMacro(IteratorWorkLoopStruct* iteratorStruct, bool isFEMacro);
	/// Parsed MacroArgumentString ("nIter,arg:init:step,...;nIter2,..."), mirroring
	///	FEVInterfacesManager::startFEMacroMultiDimensional. Iterations are NOT materialized:
	///	call macroLoopIteration(spec, i) for i in [0, totalIterations) to compute each one
	///	(dimension 0 outermost, lower dimension wins on a name clash).
	struct MacroLoopSpec
	{
		struct Arg
		{
			std::string name;
			enum { LONG, DOUBLE, STRING } type = LONG;
			long        lInit = 0, lStep = 0;
			double      dInit = 0, dStep = 0;
			std::string sVal;
		};
		std::vector<unsigned long>      dimIterations;
		std::vector<std::vector<Arg>>   dimArgs;
		std::vector<std::string>        argNames;        ///< emit order, de-duplicated (lower dimension wins)
		uint64_t                        totalIterations = 1;
	};
	/// true for step DEFAULT/Default or a numeric 0: the argument is a constant whose value is passed through untouched
	static bool isConstantMacroStep(const std::string& step);
	static MacroLoopSpec parseMacroLoopSpec(const std::string& inputArgs);
	/// name/value list for the index-th iteration, in argNames order
	static std::vector<std::pair<std::string, std::string>> macroLoopIteration(const MacroLoopSpec& spec, uint64_t index);
	/// "arg = val, arg2 = val2" for the first iteration of a MacroArgumentString (plus " (xN)" if it loops internally); "" on parse failure
	static std::string macroArgsSummary(const std::string& inputArgs);
	/// integer values as "37376 (0x9200)"; anything else (double, text) unchanged
	static std::string macroArgValueForLabel(const std::string& value);

	/// pass index of the named open BEGIN_LABEL; empty label = innermost; 0 if none open / not found
	static unsigned int getStepIndexForLabel(IteratorWorkLoopStruct* iteratorStruct, const std::string& label);
	/// returns MacroArgumentString with each numeric init replaced by init + step*passIndex(dimension's StepLabel)
	static std::string  applyStepIndexToMacroArgs(IteratorWorkLoopStruct* iteratorStruct, const std::string& inputArgs, const std::string& labelsStr);

	/// caller must hold theSupervisor_->remoteGatewayAppsMutex_ for both helpers;
	///	index refers into theSupervisor_->remoteGatewayApps_
	static size_t      findRemoteGatewayApp(IteratorWorkLoopStruct* iteratorStruct, const std::string& targetSubsystem);
	static void        queueRemoteGatewayCommand(IteratorWorkLoopStruct* iteratorStruct, size_t remoteAppIndex, const std::string& command, const std::string& statusLabel);
	static std::string buildRemoteConfigureCommand(IteratorWorkLoopStruct* iteratorStruct, const std::string& systemAlias);

	/// desktop icon name ("<folder>/<caption>") of the remote subsystem's Code Editor, or "" if none is known;
	///	locks theSupervisor_->remoteGatewayAppsMutex_ (caller must NOT hold it)
	static std::string findRemoteCodeEditorIconName(GatewaySupervisor* supervisor, const std::string& targetSubsystem);

	static bool haltIterator(Iterator*               iterator,
	                         IteratorWorkLoopStruct* iteratorStruct = 0,
							 bool 					  doNotHaltFSM = false);

	std::mutex    accessMutex_;
	volatile bool workloopRunning_;
	volatile bool activePlanIsRunning_;
	std::atomic<bool> iteratorBusy_;
	volatile bool commandPlay_, commandPause_,
	    commandHalt_;  ///< commands are set by
	                   ///< supervisor thread, and
	                   ///< cleared by iterator thread
	std::string               activePlanName_, lastStartedPlanName_, lastFinishedPlanName_;
	std::vector<OutputFile>   lastPlanOutputFiles_;  ///< output files of the last finished/halted plan (guarded by accessMutex_)
	volatile unsigned int     activeCommandIndex_, activeCommandIteration_, activeNumberOfCommands_;
	std::string				  activeCommandType_;

	volatile uint64_t 		  genPlanDurationSeconds_ = -1;
	volatile unsigned int 	  genPlanNumberOfRuns_ = 1;
	std::string 	  		  genFsmName_, genConfigAlias_, genLogEntry_;
	bool					  genKeepConfiguration_ = false;

	std::vector<unsigned int> depthIterationStack_;
	volatile time_t           activeCommandStartTime_;
	std::string               lastFsmName_;
	std::string               errorMessage_;

	GatewaySupervisor* theSupervisor_;

	template<class T>  ///< defined in included .icc source
	static void helpCommandModifyActive(IteratorWorkLoopStruct* iteratorStruct, const T& setValue, bool doTrackGroupChanges);
};

#include "otsdaq/GatewaySupervisor/Iterator.icc"  //for template definitions

}  // namespace ots

// clang-format on

#endif
