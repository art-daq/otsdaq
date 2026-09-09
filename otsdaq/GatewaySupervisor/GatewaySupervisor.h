#ifndef _ots_GatewaySupervisor_h
#define _ots_GatewaySupervisor_h
#include <atomic>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <thread>

#include "otsdaq/CoreSupervisors/ConfigurationSupervisorBase.h"
#include "otsdaq/CoreSupervisors/CorePropertySupervisorBase.h"
#include "otsdaq/FiniteStateMachine/RunControlStateMachine.h"
#include "otsdaq/FiniteStateMachine/RunInfoVInterface.h"
#include "otsdaq/GatewaySupervisor/Iterator.h"
#include "otsdaq/SOAPUtilities/SOAPMessenger.h"
#include "otsdaq/SupervisorInfo/AllSupervisorInfo.h"
#include "otsdaq/WebUsersUtilities/WebUsers.h"
#include "otsdaq/WorkLoopManager/WorkLoopManager.h"

#include "otsdaq/CodeEditor/CodeEditor.h"
#include "otsdaq/TablePlugins/DesktopIconTable.h"

#include "otsdaq/NetworkUtilities/TransceiverSocket.h"  // for UDP state changer

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <xdaq/Application.h>
#pragma GCC diagnostic pop
#include "otsdaq/Macros/XDAQApplicationMacros.h"

#include <toolbox/task/WorkLoop.h>
#include <xdata/String.h>
#include <xgi/Method.h>
#include "otsdaq/GatewaySupervisor/PixelHistoPicGen.h"

#include <pthread.h>  // for pthread_setcancelstate in broadcastMessageThread
#include <set>
#include <sstream>
#include <string>

// clang-format off

/// defines used also by OtsConfigurationWizardSupervisor
// #define FSM_LAST_CONFIGURED_GROUP_ALIAS_FILE 			std::string("FSMLastConfiguredGroupAlias.hist")
// #define FSM_LAST_STARTED_GROUP_ALIAS_FILE 				std::string("FSMLastStartedGroupAlias.hist")

// #define FSM_CONFIGURED_GROUP_ALIASES_FILE 				std::string("FSMConfiguredGroupAliases.hist")
// #define FSM_STARTED_GROUP_ALIASES_FILE 					std::string("FSMStartedGroupAlias.hist")

// #define FSM_CONFIGURED_OR_STARTED_GROUP_ALIASES_FILE 	std::string("FSMConfiguredOrStartedGroupAlias.hist")

// #define FSM_CONFIGURED_CONTEXTS_FILE 					std::string("FSMConfiguredContexts.hist")
// #define FSM_STARTED_CONTEXTS_FILE 						std::string("FSMStartedContexts.hist")
// #define FSM_CONFIGURED_OR_STARTED_CONTEXTS_FILE 		std::string("FSMConfiguredOrStartedContexts.hist")

// #define FSM_CONFIGURED_BACKBONES_FILE 					std::string("FSMConfiguredBackbones.hist")
// #define FSM_STARTED_BACKBONES_FILE 						std::string("FSMStartedBackbones.hist")
// #define FSM_CONFIGURED_OR_STARTED_BACKBONES_FILE 		std::string("FSMConfiguredOrStartedBackbones.hist")

// #define FSM_CONFIGURED_ITERATORS_FILE 					std::string("FSMConfiguredIterators.hist")
// #define FSM_STARTED_ITERATORS_FILE 						std::string("FSMStartedIterators.hist")
// #define FSM_CONFIGURED_OR_STARTED_ITERATORS_FILE 		std::string("FSMConfiguredOrStartedIterators.hist")


namespace ots
{
class ConfigurationManager;
class TableGroupKey;
class WorkLoopManager;


	/// GatewaySupervisor
	///	This class is the gateway server for all otsdaq requests in "Normal Mode." It
	/// validates user access 	for every request. It synchronizes 	the state machines of all
	/// other supervisors.
	class GatewaySupervisor : public xdaq::Application,
		public SOAPMessenger,
		public RunControlStateMachine,
		public CorePropertySupervisorBase,
		public ConfigurationSupervisorBase
	{
		friend class WizardSupervisor;
		friend class Iterator;

		static const std::string COMMAND_PARAM_LOG_ENTRY_PREAMBLE;
		static const std::string COMMAND_PARAM_SUBSYSTEM_COMMON_PREAMBLE;
		static const std::string COMMAND_PARAM_SUBSYSTEM_COMMON_OVERRIDE_PREAMBLE;
		static const std::string COMMAND_PARAM_SUBSYSTEM_COMMON_CONTEXT_PREAMBLE;
		static const std::string COMMAND_PARAM_SUBSYSTEM_COMMON_CONTEXT_OVERRIDE_PREAMBLE;
		static const std::string COMMAND_PARAM_ITERATION_INDEX_PREAMBLE;
		static const std::string COMMAND_PARAM_MIN_EVENT_GEN_START_ITERATION_PREAMBLE;

	public:
		XDAQ_INSTANTIATOR();

									GatewaySupervisor				(xdaq::ApplicationStub* s);
		virtual 					~GatewaySupervisor				(void);

		void 						init							(void);

		void 						Default							(xgi::Input* in, xgi::Output* out);

		void 						loginRequest					(xgi::Input* in, xgi::Output* out);
		void 						request							(xgi::Input* in, xgi::Output* out);
		void 						tooltipRequest					(xgi::Input* in, xgi::Output* out);
		void 						XGI_Turtle						(xgi::Input* in, xgi::Output* out);

		void						addStateMachineStatusToXML		(HttpXmlDocument& xmlOut, const std::string& fsmName, bool getRunNumber = true);
		void						addFilteredConfigAliasesToXML	(HttpXmlDocument& xmlOut, const std::string& fsmName);
		void						addRequiredFsmLogInputToXML		(HttpXmlDocument& xmlOut, const std::string& fsmName);
		static std::string			getGlobalFieldsString			(ConfigurationManager* cfgMgr, const std::map<std::string, TableVersion>& memberMap = {});

		// State Machine requests handlers
		void 						stateMachineXgiHandler(xgi::Input* in, xgi::Output* out);
		void 						stateMachineIterationBreakpoint(xgi::Input* in, xgi::Output* out);

		static std::string			getIconHeaderString(void);
		static bool					handleAddDesktopIconRequest(const std::string& author, cgicc::Cgicc& cgiIn, HttpXmlDocument& xmlOut, std::vector<DesktopIconTable::DesktopIcon>* newIcons = nullptr);
		static void 				handleGetApplicationIdRequest(AllSupervisorInfo* applicationInfo, cgicc::Cgicc& cgiIn, HttpXmlDocument& xmlOut, std::map<std::string /* requestOrigin */, std::map<std::string /* requestUrlHostPort */, std::string /* translatedHostPort */>>* portTranslationMap = nullptr);

		xoap::MessageReference 		stateMachineXoapHandler(xoap::MessageReference msg);

		bool 						stateMachineThread(toolbox::task::WorkLoop* workLoop);

		// Status requests handlers
		void 						statusRequest(xgi::Input* in, xgi::Output* out);
		void 						infoRequestResultHandler(xgi::Input* in, xgi::Output* out);
		bool 						infoRequestThread(toolbox::task::WorkLoop* workLoop);

		// External GatewaySupervisor XOAP handlers
		xoap::MessageReference 		supervisorCookieCheck(xoap::MessageReference msg);
		xoap::MessageReference 		supervisorGetActiveUsers(xoap::MessageReference msg);
		xoap::MessageReference 		supervisorSystemMessage(xoap::MessageReference msg);
		xoap::MessageReference 		supervisorGetUserInfo(xoap::MessageReference msg);
		xoap::MessageReference 		supervisorSystemLogbookEntry(xoap::MessageReference msg);
		xoap::MessageReference 		supervisorLastTableGroupRequest(xoap::MessageReference msg);

		// Finite State Machine States
		void 						stateInitial(toolbox::fsm::FiniteStateMachine& fsm) override;
		void 						statePaused(toolbox::fsm::FiniteStateMachine& fsm) override;
		void 						stateRunning(toolbox::fsm::FiniteStateMachine& fsm) override;
		void 						stateHalted(toolbox::fsm::FiniteStateMachine& fsm) override;
		void 						stateConfigured(toolbox::fsm::FiniteStateMachine& fsm) override;
		void 						inError(toolbox::fsm::FiniteStateMachine& fsm) override;

		void 						transitionConfiguring(toolbox::Event::Reference e) override;
		void 						transitionHalting(toolbox::Event::Reference e) override;
		void 						transitionInitializing(toolbox::Event::Reference e) override;
		void 						transitionPausing(toolbox::Event::Reference e) override;
		void 						transitionResuming(toolbox::Event::Reference e) override;
		void 						transitionStarting(toolbox::Event::Reference e) override;
		void 						transitionStopping(toolbox::Event::Reference e) override;
		void 						transitionShuttingDown(toolbox::Event::Reference e) override;
		void 						transitionStartingUp(toolbox::Event::Reference e) override;
		void 						enteringError(toolbox::Event::Reference e) override;

		void 						makeSystemLogEntry(const std::string& entryText, const std::string& subjectText = "", bool skipFooter = false);
		static void 				addSystemMessage(std::string toUserCSV, std::string message);

		void 						checkForAsyncError(void);

		// CorePropertySupervisorBase override functions
		virtual void 					setSupervisorPropertyDefaults					(void) override;  ///< override to control supervisor specific defaults
		virtual void 					forceSupervisorPropertyValues					(void) override;  ///< override to force supervisor property values (and ignore user settings)


	private:
		unsigned int 					getNextRunNumber								(const std::string& fsmName = "");
		void 							setNextRunNumber								(unsigned int runNumber, const std::string& fsmName = "");
		std::string 					getLastLogEntry									(const std::string& logType, const std::string& fsmName = "");
		void 							setLastLogEntry									(const std::string& logType, const std::string& logEntry, const std::string& fsmName = "");
		void 							writeRunInfoTransition							(RunInfoVInterface::RunTransitionType transitionType, const std::string& comment);


		static xoap::MessageReference 	lastTableGroupRequestHandler					(const SOAPParameters& parameters);
		static void 					launchStartOTSCommand							(const std::string& command, ConfigurationManager* cfgMgr);
		static void 					launchStartOneServerCommand						(const std::string& command, ConfigurationManager* cfgMgr, const std::string& contextName);

		static void 					indicateOtsAlive								(const CorePropertySupervisorBase* properties = 0);
		xoap::MessageReference 			TRACESupervisorRequest							(xoap::MessageReference message);

		static void 					StateChangerWorkLoop							(GatewaySupervisor* supervisorPtr);
		static void 					AppStatusWorkLoop								(GatewaySupervisor* supervisorPtr, const bool doDisconnected = false);

		std::string 					attemptStateMachineTransition					(HttpXmlDocument* xmldoc,
																						std::ostringstream* out,
																						const std::string& command,
																						const std::string& fsmName,
																						const std::string& fsmWindowName,
																						const std::string& username,
																						const std::vector<std::string>& parameters,
																						std::string logEntry = "");
		void        					broadcastMessage								(xoap::MessageReference msg);
		void        					broadcastMessageToRemoteGateways				(const xoap::MessageReference msg, unsigned int iteration = 0);
		void        					broadcastMessageToRemoteGatewaysComplete		(const xoap::MessageReference msg, unsigned int iterationIndex = 0);
		void        					signalAndWaitForBroadcastThreads				(unsigned int numberOfThreads);

		struct BroadcastMessageIterationsDoneStruct
		{
			// Creating std::vector<std::vector<bool>>
			//	because of problems with the standard library
			//	not allowing passing by reference of bool types.
			// Broadcast thread implementation requires passing by reference.
			~BroadcastMessageIterationsDoneStruct()
			{
				for (auto& arr : iterationsDone_)
					delete[] arr;
				iterationsDone_.clear();
				arraySizes_.clear();
			}  // end destructor

			void push(const unsigned int& size)
			{
				iterationsDone_.push_back(new bool[size]);
				arraySizes_.push_back(size);

				// initialize to false
				for (unsigned int i = 0; i < size; ++i)
					iterationsDone_[iterationsDone_.size() - 1][i] = false;
			}  // end push()

			bool* operator[](unsigned int i) { return iterationsDone_[i]; }
			const bool* operator[](unsigned int i) const { return iterationsDone_[i]; }
			unsigned int size(unsigned int i = -1)
			{
				if (i == (unsigned int)-1)
					return iterationsDone_.size();
				return arraySizes_[i];
			}

		private:
			std::vector<bool*>        iterationsDone_;
			std::vector<unsigned int> arraySizes_;
		};  // end BroadcastMessageIterationsDoneStruct definition

		struct BroadcastThreadStruct
		{
			//===================
			BroadcastThreadStruct()
				: threadIndex_(-1)
				, exitThread_(false)
				, working_(true)
				, workToDo_(false)
				, error_(false)
			{
			}  // end BroadcastThreadStruct constructor()

			//===================
			BroadcastThreadStruct(BroadcastThreadStruct &&b)
				: threadIndex_(b.threadIndex_)
				, exitThread_(b.exitThread_.load())
				, working_(b.working_.load())
				, workToDo_(b.workToDo_.load())
				, error_(b.error_.load())
			{
			}  // end BroadcastThreadStruct move constructor()


			struct BroadcastMessageStruct
			{
				//===================
				BroadcastMessageStruct(const SupervisorInfo& appInfo,
					xoap::MessageReference message,
					const std::string& command,
					const unsigned int& iteration,
					bool& iterationsDone,
					std::shared_ptr<BroadcastMessageIterationsDoneStruct> iterationsDoneOwner)
					: appInfo_(appInfo)
					, message_(message)
					, command_(command)
					, iteration_(iteration)
					, iterationsDone_(iterationsDone)
					, iterationsDoneOwner_(iterationsDoneOwner)
				{
				}

				const SupervisorInfo& appInfo_;
				xoap::MessageReference message_;
				const std::string command_;
				const unsigned int iteration_;
				bool& iterationsDone_;
				// Keep the BroadcastMessageIterationsDoneStruct alive while this message
				// is in use by a thread, preventing UAF even if broadcastMessage() returns
				// early (e.g. on timeout or exception) before the thread finishes.
				std::shared_ptr<BroadcastMessageIterationsDoneStruct> iterationsDoneOwner_;

				std::string reply_;
			};  // end BroadcastMessageStruct definition

			//===================
			void setMessage(const SupervisorInfo& appInfo,
				xoap::MessageReference message,
				const std::string& command,
				const unsigned int& iteration,
				bool& iterationsDone,
				std::shared_ptr<BroadcastMessageIterationsDoneStruct> iterationsDoneOwner)
			{
				messages_.clear();
				messages_.push_back(BroadcastThreadStruct::BroadcastMessageStruct(
					appInfo, message, command, iteration, iterationsDone, iterationsDoneOwner));
				workToDo_ = true;
			}  // end setMessage()

			const SupervisorInfo& getAppInfo() { return messages_[0].appInfo_; }
			xoap::MessageReference getMessage() { return messages_[0].message_; }
			const std::string& getCommand() { return messages_[0].command_; }
			const unsigned int& getIteration() { return messages_[0].iteration_; }
			std::string& getReply() { return messages_[0].reply_; }
			bool& getIterationsDone() { return messages_[0].iterationsDone_; }

			// each thread accesses these members
			std::mutex           threadMutex_;
			unsigned int         threadIndex_;
			std::atomic<bool>    exitThread_, working_, workToDo_, error_;
			// always just 1 message (for now)
			std::vector<BroadcastThreadStruct::BroadcastMessageStruct> messages_;

		};  // end BroadcastThreadStruct declaration
	static void broadcastMessageThread(
		GatewaySupervisor* supervisorPtr,
		std::shared_ptr<GatewaySupervisor::BroadcastThreadStruct> threadStruct);
	bool handleBroadcastMessageTarget(const SupervisorInfo& appInfo,
		xoap::MessageReference message,
		const std::string& command,
		const unsigned int& iteration,
		std::string& reply,
		unsigned int           threadIndex = 0,
		const std::atomic<bool>* exitFlag  = nullptr);


		// Member Variables -----------------------

		bool 				supervisorGuiHasBeenLoaded_;  ///< use to indicate first access by user of ots since execution
		static WebUsers   	theWebUsers_;
		std::map<std::string /* requestOrigin */, std::map<std::string /* requestUrlHostPort */,
			std::string /* translatedHostPort */>>
							portTranslationMap_;  ///< map of translation ports, if only certain host+port ranges are allowed at gateway for example

		WorkLoopManager 	stateMachineWorkLoopManager_;
		toolbox::BSem   	stateMachineSemaphore_;

		std::string 		activeStateMachineName_;  ///< when multiple state machines, this is the name of the state machine which executed the configure transition
		std::string 		activeStateMachineWindowName_;
		std::string 		activeStateMachineDumpFormatOnRun_, activeStateMachineDumpFormatOnConfigure_; ///<cached at Configure transition
		std::string 		activeStateMachineSystemDumpOnRun_, activeStateMachineSystemDumpOnConfigure_; ///<cached at Configure transition
		std::unique_ptr<std::thread>	configDumpCachingThread_;  ///<runs dump caching in parallel with supervisor broadcast
		std::string						configDumpCachingError_;   ///<error from dump caching thread, checked after join
		bool				activeStateMachineSystemDumpOnRunEnable_, activeStateMachineSystemDumpOnConfigureEnable_; ///<cached at Configure transition
		std::string 		activeStateMachineSystemDumpOnRunFilename_, activeStateMachineSystemDumpOnConfigureFilename_; ///<cached at Configure transition
		bool				activeStateMachineRequireUserLogOnRun_, activeStateMachineRequireUserLogOnConfigure_; ///<cached at Configure transition
		std::string 		activeStateMachineRunInfoPluginType_; ///<cached at Configure transition
		std::map<std::string /* fsmName */, std::string /* logEntry */>
							stateMachineConfigureLogEntry_, stateMachineStartLogEntry_, stateMachineStopLogEntry_;
		std::string			activeStateMachineRawStartComment_, activeStateMachineRawStopComment_;
		std::string 		activeStateMachineRunNumber_, activeStateMachineRunAlias_, activeStateMachineConfigurationAlias_;
		bool				activeStateMachineRollOverLogOnConfigure_, activeStateMachineRollOverLogOnStart_;
		std::chrono::steady_clock::time_point
							activeStateMachineRunStartTime;
		time_t				activeStateMachineRunWallClockStartTime_ = 0;
		int					activeStateMachineRunDuration_ms; ///< For paused runs, don't count time spent in pause state
		bool				activeStateMachineWriteToEcl_ = true;
		bool				activeStateMachineDiscardRun_ = false;
		unsigned int		activeStateMachineConfigureConditionID_, activeStateMachineRunConditionID_;
		unsigned int		minReadyForEventGenerationStartIteration_ = 0;
		std::string			activeStateMachineSubsystemCommonList_, activeStateMachineSubsystemCommonOverrideList_; ///<cached at Configure transition CSV list of Table/Versions specified as table alias "SubsystemCommon" and "SubsystemCommonOverride" by user at top-level Primary Gateway, to be merged into the configuration for all subsystems (e.g. for DCS/DQM) when configuring
		std::string			activeSubsystemCommonContextList_, activeSubsystemCommonContextOverrideList_; ///<refreshed in AppStatusWorkLoop CSV list of Table/Versions specified as table alias "SubsystemCommonContext" and "SubsystemCommonContextOverride" by user at top-level Primary Gateway, to be pushed to remote subsystems via periodic status requests for Context group tables (e.g. StateMachineTable)
		std::string			appliedContextCommonList_, appliedContextCommonOverrideList_; ///<remote-side: last applied Context Common Table lists received from top-level
		std::mutex			contextCommonMutex_; ///<protects appliedContextCommonList_ and appliedContextCommonOverrideList_

		std::string			cachedSubsystemCommonBackboneKey_;
		std::string			cachedSubsystemCommonList_, cachedSubsystemCommonOverrideList_;
		std::string			cachedSubsystemCommonContextList_, cachedSubsystemCommonContextOverrideList_;

		std::mutex			systemStatusMutex_;
		std::string 		lastLogbookEntry_;
		time_t				lastLogbookEntryTime_ = 0;

		std::string 		lastConsoleErr_, lastConsoleWarn_, lastConsoleInfo_, lastConsoleErrTime_, lastConsoleWarnTime_, lastConsoleInfoTime_;
		std::string 		firstConsoleErr_, firstConsoleWarn_, firstConsoleInfo_, firstConsoleErrTime_, firstConsoleWarnTime_, firstConsoleInfoTime_;
		size_t				systemConsoleErrCount_ = 0, systemConsoleWarnCount_ = 0, systemConsoleInfoCount_ = 0;

		std::pair<std::string /*group name*/, TableGroupKey>
							theConfigurationTableGroup_;  ///< used to track the active configuration group at states after the configure state
		std::string			stateMachineTransitionUsername_;  ///< used to track the user who made the last state machine transition (for logging purposes)

		Iterator   			theIterator_;
		std::mutex 			stateMachineAccessMutex_;  ///< for sharing state machine access with
											  ///< iterator thread
		std::string 		stateMachineLastCommandInput_;
		enum
		{
			VERBOSE_MUTEX = 0
		};

		CodeEditor 			codeEditor_;

		std::mutex   		broadcastCommandMessageIndexMutex_;
		unsigned int 		broadcastCommandMessageIndex_;
		std::atomic<bool>	broadcastIterationsDone_{true};
		std::mutex   		broadcastIterationBreakpointMutex_;
		unsigned int 		broadcastIterationBreakpoint_;  ///< pause transition when iteration index
													 ///< matches breakpoint index
		std::mutex			broadcastCommandStatusUpdateMutex_;
		std::string			broadcastCommandStatus_;

		std::mutex              remoteIterationMutex_;
		std::condition_variable remoteIterationCV_;
		unsigned int            remoteIterationIndex_ = 0;
		std::atomic<bool>       isRemoteSubsystemIteration_{false}; ///< true when broadcastMessage() iteration loop is driven by top-level re-sends
		std::atomic<bool>       remoteSubsystemErrorReceived_{false}; ///< set when top-level sends Error/Fail while this subsystem is mid-transition; checked only in the needNextIteration wait

		static std::vector<std::shared_ptr<GatewaySupervisor::BroadcastThreadStruct>> broadcastThreadStructs_; ///<moving to static, instead of a local instance inside broadcastMessage() seems to avoid crashing when multiple error stack up and threads get stuck waiting for app replies

		std::string        	securityType_;
		PixelHistoPicGen	picGen_;

		//Variable used by the RunInfo plugin
		unsigned int 		conditionID_;

public:	//used by remote subsystem control and status

		struct RemoteGatewayInfo {
			SupervisorInfo::SubappInfo 			appInfo;

			enum class ConfigDumpTypes ///<FSM Modes: 'Follow FSM,' 'Do not Halt' (artdaq),  or 'Only Configure' (DCS/DQM)
			{
				Text,
				JSON_all,
				Unknown
			};

			std::string 						command, fsmName; ///<when not "", need to send
		  private: //make error private to connect to set timestamp
			std::string						error;
			time_t							errorTimestamp = 0;
		  public:
			void								setError(const std::string& err) { error = err; errorTimestamp = time(0); }
			void								clearError() { error = ""; errorTimestamp = 0; }
			void								copyError(const RemoteGatewayInfo& r) { error = r.error; errorTimestamp = r.errorTimestamp; }
			const std::string&					getError() const { return error; }
			const std::string					getErrorTimestamp() const { return StringMacros::getTimestampString(errorTimestamp); }

			std::string							config_dump;
			ConfigDumpTypes						config_dump_type = ConfigDumpTypes::Unknown;

			size_t								ignoreStatusCount = 0; ///<if non-zero, do not ask for status
			time_t								relaunchTime = 0; ///<timestamp of last relaunch via gatewayLaunchOTSInstance
			time_t								commandSentTime = 0; ///<timestamp of last command send; suppresses stale status write-backs briefly

			size_t								consoleErrCount = 0, consoleWarnCount = 0;

			std::string							fullName;
			std::string 						user_data_path_record; ///<used for remote gateway subapp control
			std::string							setupType, instancePath, instanceHost, instanceUser; ///<used for remote ots instance ssh launch

			std::string 						selected_config_alias; ///<used for remote gateway subapp control
			std::set<std::string> 				config_aliases; ///<used for remote gateway subapp control
			std::string 						iconString, parentIconFolderPath, landingPage, permissionThresholdString; ///<used for desktop icons

			std::string							usernameWithLock;

			enum class FSM_ModeTypes ///<FSM Modes: 'Follow FSM,' 'Do not Halt' (artdaq),  or 'Only Configure' (DCS/DQM)
			{
				Follow_FSM,
				DoNotHalt, ///<(e.g. for artdaq)
				OnlyConfigure, ///<(e.g. for DCS/DQM)
			};
			FSM_ModeTypes 						fsm_mode = FSM_ModeTypes::Follow_FSM; ///< used for remote gateway subapp control
			bool								fsm_included = true;

			std::string							getFsmMode() const {
				switch(fsm_mode)
				{
					case FSM_ModeTypes::Follow_FSM: return "Follow FSM";
					case FSM_ModeTypes::DoNotHalt: return "Do Not Halt";
					case FSM_ModeTypes::OnlyConfigure: return "Only Configure";
					default: return "Impossible";
				}
			} //end getFsmMode()

			std::string							getConfigDumpType() const {
				switch(config_dump_type)
				{
					case ConfigDumpTypes::Text: return "Text";
					case ConfigDumpTypes::JSON_all: return "JSON all";
					default: return "Unknown";
				}
			} //end getFsmMode()

			std::map<std::string, SupervisorInfo::SubappInfo>   subapps; ///< remote gateways can have subapps
			bool iterationsDone = false; ///< tracks per-gateway iteration completion during FSM transitions

			///< active context/config table group actually in use on the remote subsystem itself (as opposed to selected_config_alias, which is just the operator's chosen alias to configure with)
			std::string							activeContextGroupName, activeConfigGroupName;
			TableGroupKey						activeContextGroupKey, activeConfigGroupKey;

			///< selected_config_alias resolved to a group name+key by the remote subsystem itself (against its own active Backbone); empty until the subsystem reports back a resolution
			std::string							selectedConfigGroupName;
			TableGroupKey						selectedConfigGroupKey;
			bool doNotHaltWasCommandedHalt = false;
		}; //end GatewaySupervisor::RemoteGatewayInfo struct

		std::vector<GatewaySupervisor::RemoteGatewayInfo> 	remoteGatewayApps_;
		std::mutex											remoteGatewayAppsMutex_;
		std::map<std::string /* appName */,
			bool /* lastStatusGood */> 						appLastStatusGood_;
		std::mutex											dualStatusThreadMutex_;

		std::string											ipAddressForStateChangesOverUDP_; ///< IP used for UDP reverse-login propagation to remote gateways
		int													portForReverseLoginOverUDP_ = 0;  ///< UDP port for reverse-login; 0 = disabled

		std::map<unsigned int /* lid */, SupervisorInfo>	localAllSupervisorInfo_; ///< only use in main thread, stable copy of app status


		std::mutex											latestGatewayIconsMutex_;
		std::vector<DesktopIconTable::DesktopIcon>			latestGatewayIcons_; ///< used to track the latest desktop icons (which are defined by the active context but allowed to change dynamically)
		std::pair<std::string /* latestIconContext group */, TableGroupKey>
															latestGatewayIconsContextGroup_; ///< used to track the table group key for the latest desktop icons

		std::string											latestGatewayRemoteIconsString_; ///< cached string of remote gateway icons for quick access
		std::pair<std::string /* latestIconContext group */, TableGroupKey>
															latestGatewayRemoteIconsContextGroup_; ///< used to track the table group key for the latest remote desktop icons

		std::string											cachedGlobalFieldsString_;
		std::pair<std::string, TableGroupKey>				cachedGlobalFieldsGroup_;

		static void 				CheckRemoteGatewayStatus					(GatewaySupervisor::RemoteGatewayInfo& remoteGatewayApp, const std::unique_ptr<TransceiverSocket>& remoteGatewaySocket, const std::string& ipForReverseLoginOverUDP, int portForReverseLoginOverUDP, const std::string& contextCommonList = "", const std::string& contextCommonOverrideList = "");
		static void 				SendRemoteGatewayCommand					(GatewaySupervisor::RemoteGatewayInfo& remoteGatewayApp, const std::unique_ptr<TransceiverSocket>& remoteGatewaySocket);
		static void					applyContextCommonTables					(GatewaySupervisor* supervisor, const std::string& contextCommonList, const std::string& contextCommonOverrideList);
		static void 				GetRemoteGatewayIcons						(GatewaySupervisor::RemoteGatewayInfo& remoteGatewayApp, const std::unique_ptr<TransceiverSocket>& remoteGatewaySocket);
		void						loadRemoteGatewaySettings					(std::vector<GatewaySupervisor::RemoteGatewayInfo>& remoteGateways, bool onlyNotFound = false) const;
		void						saveRemoteGatewaySettings					(void) const;
		static std::string			translateURLForRequestOrigin				(const std::string& url, const std::string& requestOrigin, std::map<std::string /* requestOrigin */, std::map<std::string /* requestUrlHostPort */, std::string /* translatedHostPort */>>& portTranslationMap);
		static std::string			translateRemoteIconStringForRequestOrigin	(const std::string& iconString, const std::string& requestOrigin, std::map<std::string /* requestOrigin */, std::map<std::string /* requestUrlHostPort */, std::string /* translatedHostPort */>>& portTranslationMap);

	};
// clang-format on

}  // namespace ots

#endif
