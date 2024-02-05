#ifndef _ots_GatewaySupervisor_h
#define _ots_GatewaySupervisor_h

#include "otsdaq/CoreSupervisors/ConfigurationSupervisorBase.h"
#include "otsdaq/CoreSupervisors/CorePropertySupervisorBase.h"
#include "otsdaq/FiniteStateMachine/RunControlStateMachine.h"
#include "otsdaq/GatewaySupervisor/Iterator.h"
#include "otsdaq/SOAPUtilities/SOAPMessenger.h"
#include "otsdaq/SupervisorInfo/AllSupervisorInfo.h"
// #include "otsdaq/SystemMessenger/SystemMessenger.h"
// #include "otsdaq/TableCore/TableGroupKey.h"
#include "otsdaq/WebUsersUtilities/WebUsers.h"
#include "otsdaq/WorkLoopManager/WorkLoopManager.h"

#include "otsdaq/CodeEditor/CodeEditor.h"
#include "otsdaq/TablePlugins/DesktopIconTable.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <xdaq/Application.h>
#pragma GCC diagnostic pop
#include "otsdaq/Macros/XDAQApplicationMacros.h"
// #include <toolbox/fsm/FiniteStateMachine.h>
#include <toolbox/task/WorkLoop.h>
#include <xdata/String.h>
#include <xgi/Method.h>

#include <set>
#include <sstream>
#include <string>

// defines used also by OtsConfigurationWizardSupervisor
#define FSM_LAST_CONFIGURED_GROUP_ALIAS_FILE std::string("FSMLastConfiguredGroupAlias.hist")
#define FSM_LAST_STARTED_GROUP_ALIAS_FILE std::string("FSMLastStartedGroupAlias.hist")

namespace ots
{
class ConfigurationManager;
class TableGroupKey;
class WorkLoopManager;

// clang-format off

	// GatewaySupervisor
	//	This class is the gateway server for all otsdaq requests in "Normal Mode." It
	// validates user access 	for every request. It synchronizes 	the state machines of all
	// other supervisors.
	class GatewaySupervisor : public xdaq::Application,
		public SOAPMessenger,
		public RunControlStateMachine,
		public CorePropertySupervisorBase,
		public ConfigurationSupervisorBase
	{
		friend class WizardSupervisor;
		friend class Iterator;

	public:
		XDAQ_INSTANTIATOR();

		GatewaySupervisor(xdaq::ApplicationStub* s);
		virtual 					~GatewaySupervisor(void);

		void 						init(void);

		void 						Default(xgi::Input* in, xgi::Output* out);

		void 						loginRequest(xgi::Input* in, xgi::Output* out);
		void 						request(xgi::Input* in, xgi::Output* out);
		void 						tooltipRequest(xgi::Input* in, xgi::Output* out);

		// State Machine requests handlers
		void 						stateMachineXgiHandler(xgi::Input* in, xgi::Output* out);
		void 						stateMachineIterationBreakpoint(xgi::Input* in, xgi::Output* out);

		static std::string			getIconHeaderString(void);
		static bool					handleAddDesktopIconRequest(const std::string& author, cgicc::Cgicc& cgiIn, HttpXmlDocument& xmlOut, std::vector<DesktopIconTable::DesktopIcon>* newIcons = nullptr);
		static void 				handleGetApplicationIdRequest(AllSupervisorInfo* applicationInfo, cgicc::Cgicc& cgiIn, HttpXmlDocument& xmlOut);

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

		void 						makeSystemLogEntry(std::string entryText);

		void 						checkForAsyncError(void);

		// CorePropertySupervisorBase override functions
		virtual void 					setSupervisorPropertyDefaults					(void) override;  // override to control supervisor specific defaults
		virtual void 					forceSupervisorPropertyValues					(void) override;  // override to force supervisor property values (and ignore user settings)

	private:
		unsigned int 					getNextRunNumber								(const std::string& fsmName = "");
		bool 							setNextRunNumber								(unsigned int runNumber, const std::string& fsmName = "");

		static xoap::MessageReference 	lastTableGroupRequestHandler					(const SOAPParameters& parameters);
		static void 					launchStartOTSCommand							(const std::string& command, ConfigurationManager* cfgMgr);
		static void 					indicateOtsAlive								(const CorePropertySupervisorBase* properties = 0);
		xoap::MessageReference 			TRACESupervisorRequest							(xoap::MessageReference message);

		static void 					StateChangerWorkLoop							(GatewaySupervisor* supervisorPtr);
		static void 					AppStatusWorkLoop								(GatewaySupervisor* supervisorPtr);

		std::string 					attemptStateMachineTransition					(HttpXmlDocument* xmldoc,
																						std::ostringstream* out,
																						const std::string& command,
																						const std::string& fsmName,
																						const std::string& fsmWindowName,
																						const std::string& username,
																						const std::vector<std::string>& parameters,
																						const std::string& logEntry = "");
		void        					broadcastMessage								(xoap::MessageReference msg);

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
				, exitThread_(b.exitThread_)
				, working_(b.working_)
				, workToDo_(b.workToDo_)
				, error_(b.error_)
			{
			}  // end BroadcastThreadStruct move constructor()


			struct BroadcastMessageStruct
			{
				//===================
				BroadcastMessageStruct(const SupervisorInfo& appInfo,
					xoap::MessageReference message,
					const std::string& command,
					const unsigned int& iteration,
					bool& iterationsDone)
					: appInfo_(appInfo)
					, message_(message)
					, command_(command)
					, iteration_(iteration)
					, iterationsDone_(iterationsDone)
				{
				}

				const SupervisorInfo& appInfo_;
				xoap::MessageReference message_;
				const std::string command_;
				const unsigned int iteration_;
				bool& iterationsDone_;

				std::string reply_;
			};  // end BroadcastMessageStruct definition

			//===================
			void setMessage(const SupervisorInfo& appInfo,
				xoap::MessageReference message,
				const std::string& command,
				const unsigned int& iteration,
				bool& iterationsDone)
			{
				messages_.clear();
				messages_.push_back(BroadcastThreadStruct::BroadcastMessageStruct(
					appInfo, message, command, iteration, iterationsDone));
				workToDo_ = true;
			}  // end setMessage()

			const SupervisorInfo& getAppInfo() { return messages_[0].appInfo_; }
			xoap::MessageReference getMessage() { return messages_[0].message_; }
			const std::string& getCommand() { return messages_[0].command_; }
			const unsigned int& getIteration() { return messages_[0].iteration_; }
			std::string& getReply() { return messages_[0].reply_; }
			bool& getIterationsDone() { return messages_[0].iterationsDone_; }

			// each thread accesses these members
			std::mutex    threadMutex_;
			unsigned int  threadIndex_;
			volatile bool exitThread_, working_, workToDo_, error_;
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
			unsigned int           threadIndex = 0);


		// Member Variables -----------------------

		bool 				supervisorGuiHasBeenLoaded_;  // use to indicate first access by user of ots since execution
		static WebUsers   	theWebUsers_;

		WorkLoopManager 	stateMachineWorkLoopManager_;
		toolbox::BSem   	stateMachineSemaphore_;

		std::string 		activeStateMachineName_;  // when multiple state machines, this is the name of the state machine which executed the configure transition
		std::string 		activeStateMachineWindowName_;
		std::string			activeStateMachineLogEntry_;
		std::string 		activeStateMachineRunNumber_;
		std::chrono::steady_clock::time_point 
							activeStateMachineRunStartTime;
		int					activeStateMachineRunDuration_ms; // For paused runs, don't count time spent in pause state


		std::pair<std::string /*group name*/, TableGroupKey>
							theConfigurationTableGroup_;  // used to track the active configuration group atstates after the configure state

		Iterator   			theIterator_;
		std::mutex 			stateMachineAccessMutex_;  // for sharing state machine access with
											  // iterator thread
		std::string 		stateMachineLastCommandInput_;
		std::string			lastConfigurationAlias_;
		enum
		{
			VERBOSE_MUTEX = 0
		};

		CodeEditor 			codeEditor_;

		std::mutex   		broadcastCommandMessageIndexMutex_, broadcastIterationsDoneMutex_;
		unsigned int 		broadcastCommandMessageIndex_;
		bool         		broadcastIterationsDone_;
		std::mutex   		broadcastIterationBreakpointMutex_;
		unsigned int 		broadcastIterationBreakpoint_;  // pause transition when iteration index
													 // matches breakpoint index
		std::mutex			broadcastCommandStatusUpdateMutex_;
		std::string			broadcastCommandStatus_;
		static std::vector<std::shared_ptr<GatewaySupervisor::BroadcastThreadStruct>> broadcastThreadStructs_; //moving to static, instead of a local instance inside broadcastMessage() seems to avoid crashing when multiple error stack up and threads get stuck waiting for app replies

		// temporary member variable to avoid redeclaration in repetitive functions
		char 				tmpStringForConversions_[100];

		std::string        	securityType_;
	};
// clang-format on

}  // namespace ots

#endif
