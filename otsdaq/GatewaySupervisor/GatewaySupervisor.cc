#include "otsdaq/GatewaySupervisor/GatewaySupervisor.h"
#include "otsdaq/CgiDataUtilities/CgiDataUtilities.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/ITRACEController.h"
#include "otsdaq/MessageFacility/MessageFacility.h"
#include "otsdaq/SOAPUtilities/SOAPCommand.h"
#include "otsdaq/SOAPUtilities/SOAPUtilities.h"
#include "otsdaq/XmlUtilities/HttpXmlDocument.h"

#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/ConfigurationInterface/ConfigurationManagerRW.h"
#include "otsdaq/TablePlugins/XDAQContextTable/XDAQContextTable.h"
#include "otsdaq/WorkLoopManager/WorkLoopManager.h"

#include "otsdaq/FiniteStateMachine/MakeRunInfo.h"        // for Run Info plugin macro
#include "otsdaq/FiniteStateMachine/RunInfoVInterface.h"  // for Run Info plugins

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#include <cgicc/HTMLClasses.h>
#include <cgicc/HTMLDoctype.h>
#include <cgicc/HTTPCookie.h>
#include <cgicc/HTTPHeader.h>
#include <xgi/Utils.h>
#pragma GCC diagnostic pop

#include <toolbox/fsm/FailedEvent.h>
#include <toolbox/task/WorkLoopFactory.h>
#include <xdaq/NamespaceURI.h>
#include <xoap/Method.h>
#include <sstream>

#include <sys/stat.h>  // for mkdir
#include <cctype>      // for std::isspace
#include <chrono>      // std::chrono::seconds
#include <cstdio>      // for snprintf
#include <fstream>
#include <thread>  // std::this_thread::sleep_for

using namespace ots;

// clang-format off
#define RUN_NUMBER_PATH std::string(__ENV__("SERVICE_DATA_PATH")) + "/RunNumber/"
#define RUN_NUMBER_FILE_NAME "NextRunNumber.txt"
#define LOG_ENTRY_PATH std::string(__ENV__("SERVICE_DATA_PATH")) + "/FSM_LastLogEntry/"
#define LOG_ENTRY_FILE_NAME "LastLogEntry.txt"
#define FSM_LAST_GROUP_ALIAS_FILE_START std::string("FSMLastGroupAlias-")
#define FSM_USERS_PREFERENCES_FILETYPE "pref"

#define REMOTE_SUBSYSTEM_SETTINGS_FILE_NAME "RemoteSubsystems.txt"

#define TLVL_StateChanger		 	9	// = TLVL_DEBUG + 9
#define TLVL_RemoteIcons		 	10	// = TLVL_DEBUG + 10
#define TLVL_StateChangerDetail	 	11	// = TLVL_DEBUG + 11
#define TLVL_StateChangerStatus	 	12	// = TLVL_DEBUG + 12
#define TLVL_SystemDump	 	15	// = TLVL_DEBUG + 15
#define TLVL_Permissions		 	20	// = TLVL_DEBUG + 20
#define TLVL_GetDesktopIcons	 	21	// = TLVL_DEBUG + 21
#define TLVL_RemoteFSMRequests	 	22	// = TLVL_DEBUG + 22
#define TLVL_StatusParams		 	23	// = TLVL_DEBUG + 23
#define TLVL_RemoteStatusVerbose 	24	// = TLVL_DEBUG + 24
#define TLVL_RemoteStatusParams	 	25	// = TLVL_DEBUG + 25
#define TLVL_RemoteDesktopIcons	 	35	// = TLVL_DEBUG + 35
#define TLVL_DebugStatusDetail 		36	// = TLVL_DEBUG + 36
#define TLVL_StatusRemoteWorkloop 	38	// = TLVL_DEBUG + 38
#define TLVL_StatusWorkloop 		39	// = TLVL_DEBUG + 39
#define TLVL_DebugStatusWorkloop 	40	// = TLVL_DEBUG + 40
#define TLVL_DebugArtdaqStatus 		41	// = TLVL_DEBUG + 41
#define TLVL_DebugRequests 			42	// = TLVL_DEBUG + 42
#define TLVL_StatusFullDetail	 	50	// = TLVL_DEBUG + 50

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "GatewaySupervisor"

#define REMOTE_BACKBONE_ERR			"A valid active Backbone configuration group must be specified at the subsystem User Data Path, and it must be retrievable (i.e. same configuration database URI) from the primary Gateway."

const std::string GatewaySupervisor::COMMAND_PARAM_LOG_ENTRY_PREAMBLE = "LogEntry:";
const std::string GatewaySupervisor::COMMAND_PARAM_SUBSYSTEM_COMMON_PREAMBLE = "SubsystemCommonTableList:";
const std::string GatewaySupervisor::COMMAND_PARAM_SUBSYSTEM_COMMON_OVERRIDE_PREAMBLE = "SubsystemCommonOverrideTableList:";
const std::string GatewaySupervisor::COMMAND_PARAM_SUBSYSTEM_COMMON_CONTEXT_PREAMBLE = "SubsystemCommonContextTableList:";
const std::string GatewaySupervisor::COMMAND_PARAM_SUBSYSTEM_COMMON_CONTEXT_OVERRIDE_PREAMBLE = "SubsystemCommonContextOverrideTableList:";
const std::string GatewaySupervisor::COMMAND_PARAM_ITERATION_INDEX_PREAMBLE = "IterationIndex:";

// clang-format on

XDAQ_INSTANTIATOR_IMPL(GatewaySupervisor)

WebUsers GatewaySupervisor::theWebUsers_ = WebUsers();
std::vector<std::shared_ptr<GatewaySupervisor::BroadcastThreadStruct>>
    GatewaySupervisor::broadcastThreadStructs_;

//==============================================================================
GatewaySupervisor::GatewaySupervisor(xdaq::ApplicationStub* s)
    : xdaq::Application(s)
    , SOAPMessenger(this)
    , RunControlStateMachine("GatewaySupervisor")
    , CorePropertySupervisorBase(this)
    , stateMachineWorkLoopManager_(toolbox::task::bind(
          this, &GatewaySupervisor::stateMachineThread, "StateMachine"))
    , stateMachineSemaphore_(toolbox::BSem::FULL)
    , activeStateMachineName_("")
    , theIterator_(this)
    , broadcastCommandMessageIndex_(0)
    , broadcastIterationBreakpoint_(
          -1)  // for standard transitions, ignore the breakpoint
{
	INIT_MF("." /*directory used is USER_DATA/LOG/.*/);

	__COUT__ << "Constructing" << __E__;

	if(0)  // to test xdaq exception what
	{
		std::stringstream ss;
		ss << "This is a test" << std::endl;
		try
		{
			XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
		}
		catch(const toolbox::fsm::exception::Exception& e)
		{
			// std::cout << "1Message: " << e.rbegin()->at("message") << std::endl;
			// std::cout << "2Message: " << e.message() << std::endl;
			// std::cout << "3Message: " << e.what() << std::endl;
			// std::string what =  e.what();
			// std::cout << "4Message: " << what << std::endl;
			// if(what != e.message())
			{
				std::cout << "Mismatch!" << std::endl;
				throw;
			}
		}
	}

	// attempt to make directory structure (just in case)
	mkdir((std::string(__ENV__("SERVICE_DATA_PATH"))).c_str(), 0755);

	// make table group history directory here and at ConfigurationManagerRW (just in case)
	mkdir((ConfigurationManager::LAST_TABLE_GROUP_SAVE_PATH).c_str(), 0755);
	mkdir((RUN_NUMBER_PATH).c_str(), 0755);
	mkdir((LOG_ENTRY_PATH).c_str(), 0755);

	securityType_ = GatewaySupervisor::theWebUsers_.getSecurity();

	__COUT__ << "Security: " << securityType_ << __E__;

	xgi::bind(this, &GatewaySupervisor::Default, "Default");
	xgi::bind(this, &GatewaySupervisor::loginRequest, "LoginRequest");
	xgi::bind(this, &GatewaySupervisor::request, "Request");
	xgi::bind(this, &GatewaySupervisor::stateMachineXgiHandler, "StateMachineXgiHandler");
	xgi::bind(this,
	          &GatewaySupervisor::stateMachineIterationBreakpoint,
	          "StateMachineIterationBreakpoint");
	xgi::bind(this, &GatewaySupervisor::tooltipRequest, "TooltipRequest");
	xgi::bind(this, &GatewaySupervisor::XGI_Turtle, "XGI_Turtle");

	xoap::bind(this,
	           &GatewaySupervisor::supervisorCookieCheck,
	           "SupervisorCookieCheck",
	           XDAQ_NS_URI);
	xoap::bind(this,
	           &GatewaySupervisor::supervisorGetActiveUsers,
	           "SupervisorGetActiveUsers",
	           XDAQ_NS_URI);
	xoap::bind(this,
	           &GatewaySupervisor::supervisorSystemMessage,
	           "SupervisorSystemMessage",
	           XDAQ_NS_URI);
	xoap::bind(this,
	           &GatewaySupervisor::supervisorSystemLogbookEntry,
	           "SupervisorSystemLogbookEntry",
	           XDAQ_NS_URI);
	xoap::bind(this,
	           &GatewaySupervisor::supervisorLastTableGroupRequest,
	           "SupervisorLastTableGroupRequest",
	           XDAQ_NS_URI);
	xoap::bind(this,
	           &GatewaySupervisor::TRACESupervisorRequest,
	           "TRACESupervisorRequest",
	           XDAQ_NS_URI);

	init();

	__SUP_COUT__ << "Constructed. getpid()=" << getpid() << " gettid()=" << gettid()
	             << __E__;
}  // end constructor

//==============================================================================
///	TODO: Lore needs to detect program quit through killall or ctrl+c so that Logbook
/// entry is made when ots is killed
GatewaySupervisor::~GatewaySupervisor(void)
{
	delete CorePropertySupervisorBase::theConfigurationManager_;

	bool doLog = false;
	try
	{
		doLog = __ENV__("OTS_LOG_INTERMEDIATE_STATES") == std::string("1");
	}
	catch(...)
	{ /* ignore errors */
		;
	}

	if(doLog)
		makeSystemLogEntry("ots shutdown.");
}  // end destructor

//==============================================================================
///For Wizard Supervisor to call
void GatewaySupervisor::indicateOtsAlive(const CorePropertySupervisorBase* properties)
{
	CorePropertySupervisorBase::indicateOtsAlive(properties);
}  //end indicateOtsAlive()

//==============================================================================
void GatewaySupervisor::init(void)
{
	supervisorGuiHasBeenLoaded_ = false;

	//Initialize the variable used by the RunInfo plugin
	conditionID_ = (unsigned int)-1;

	// setting up thread for UDP thread to drive state machine
	{
		bool enableStateChanges = false;
		bool enableLoginVerify  = false;
		try
		{
			enableStateChanges = CorePropertySupervisorBase::getSupervisorTableNode()
			                         .getNode("EnableStateChangesOverUDP")
			                         .getValue<bool>();
			enableLoginVerify = CorePropertySupervisorBase::getSupervisorTableNode()
			                        .getNode("EnableAckForStateChangesOverUDP")
			                        .getValue<bool>();
		}
		catch(...)
		{
			;
		}  // ignore errors

		if(enableStateChanges || enableLoginVerify)
		{
			if(enableStateChanges)
				__COUT_INFO__ << "Enabling state changes over UDP..." << __E__;
			else if(enableLoginVerify)
			{
				__COUT_INFO__ << "Enabling this Gateway as source of primary login "
				                 "verification over UDP..."
				              << __E__;

				std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
				loadRemoteGatewaySettings(remoteGatewayApps_);
			}

			// start state changer UDP listener thread
			std::thread(
			    [](GatewaySupervisor* s) { GatewaySupervisor::StateChangerWorkLoop(s); },
			    this)
			    .detach();
		}
		else
			__COUT__ << "State changes over UDP are disabled." << __E__;
	}  // end setting up thread for UDP drive of state machine

	// setting up checking of App Status
	{
		bool checkAppStatus = false;
		try
		{
			checkAppStatus = CorePropertySupervisorBase::getSupervisorTableNode()
			                     .getNode("EnableApplicationStatusMonitoring")
			                     .getValue<bool>();
		}
		catch(...)
		{
			;
		}  // ignore errors

		if(checkAppStatus)
		{
			__COUT__ << "Enabling App Status checking..." << __E__;

			//make one thread for rapid status checking
			// and one for disconnected status checking (with long timeouts)
			std::thread(
			    [](GatewaySupervisor* s) { GatewaySupervisor::AppStatusWorkLoop(s); },
			    this)
			    .detach();

			std::thread(
			    [](GatewaySupervisor* s) {
				    GatewaySupervisor::AppStatusWorkLoop(s, true /* doDisconnected */);
			    },
			    this)
			    .detach();
		}
		else
		{
			__COUT__ << "App Status checking is disabled." << __E__;

			// set all app status to "Not Monitored" so that FSM changes ignore missing app status
			for(const auto& it : allSupervisorInfo_.getAllSupervisorInfo())
			{
				auto appInfo = it.second;
				allSupervisorInfo_.setSupervisorStatus(
				    appInfo,
				    SupervisorInfo::APP_STATUS_NOT_MONITORED,
				    0 /* progressInteger */,
				    "" /* detail */);
			}
		}

	}  // end checking of Application Status

	// setup port translation
	{
		std::string portTranslationPath = "";
		try
		{
			///	portTranslationMap_ ~ used by GatewaySupervisor::translateURLForRequestOrigin
			///
			///		Converts url host:port to a new host:port based on the translation
			///			table (to be provided by system admin prior to starting ots in normal mode).
			///
			///	File format is:
			///		- each line: requestOrigin host:port | url host:port | translation host:port
			///
			///	Steps:
			/// 	if requestOrigin host matches translation host
			/// 		then look for url host+port combo in translation map
			/// 		if combo found, then return translation host+port + rest of url
			/// 		else return url unchanged
			/// 	else return url unchanged
			///
			///  for example, requestOrigin == "https://gateway1:8443" and executable url = "http://host:2016/urnblah"
			///  	(the user needs the host:port accessible to them, which might be forwarded through a firewall or NAT)
			///		and so translation might return "https://gateway1:8444"
			///		... in which case, the entry in file would be: https://gateway1:8443 | host:2016 | https://gateway1:8444
			///
			///	Note!! that the priority matters for host+ports that are substrings of each other,
			///		such that the longer one is replaced first.
			///	 	For example, if there are host+ports translations for both "host:2016" and "host:201",
			///		then "host:2016" should be listed first, so it is replaced first,
			///			to avoid partial replacement that would block the full replacement later.
			portTranslationPath = __ENV__("OTS_PORT_TRANSLATION_MAP_FILE");
		}
		catch(...)
		{
			__COUT__ << "OTS_PORT_TRANSLATION_MAP_FILE not set; no port translation "
			            "will be used."
			         << __E__;
		}

		portTranslationMap_.clear();
		if(portTranslationPath != "")
		{
			// example load outcome:
			//	portTranslationMap_["http://localhost:3075"]["http://hostname1:3076"] = "http://localhost:3079";
			//	portTranslationMap_["http://localhost:3075"]["http://hostname2:3077"] = "http://localhost:3078"

			__COUTV__(portTranslationPath);
			FILE* fp = fopen(portTranslationPath.c_str(), "r");
			if(fp)
			{
				// Read and process the port translation file
				char     line[5000];
				uint32_t lineNumber = 0;
				while(fgets(line, sizeof(line), fp))
				{
					++lineNumber;

					if(strlen(line) == 0 ||
					   line[0] == '#')  //skip empty or commented lines
						continue;

					// Process each line to populate portTranslationMap_
					std::vector<std::string> parts =
					    StringMacros::getVectorFromString(std::string(line), {'|'});

					__SUP_COUTTV__(StringMacros::vectorToString(parts));

					if(parts.size() == 3)
					{
						std::string requestOrigin      = parts[0];
						std::string requestUrlHostPort = parts[1];
						std::string translatedHostPort = parts[2];
						portTranslationMap_[requestOrigin][requestUrlHostPort] =
						    translatedHostPort;
					}
					else if(parts.size() < 2)
					{
						__SUP_COUT__
						    << "Ignoring (and treating as comment) line #" << lineNumber
						    << " in port translation file (length = " << strlen(line)
						    << "): " << line << __E__;
						continue;
					}
					else
					{
						__SS__ << "Invalid line #" << lineNumber
						       << " in port translation file with too many args "
						          "(count = "
						       << parts.size() << ", expected 3): " << line << __E__;
						__SS_THROW__;
					}
				}
				fclose(fp);
			}
			else
			{
				__COUT_ERR__ << "Could not open port translation file at "
				             << portTranslationPath
				             << "; no port translation will be used." << __E__;
			}
			__SUP_COUTTV__(StringMacros::mapToString(portTranslationMap_));
		}
	}

}  // end init()

//==============================================================================
/// AppStatusWorkLoop
///	child thread
void GatewaySupervisor::AppStatusWorkLoop(GatewaySupervisor* theSupervisor,
                                          const bool         doDisconnected /* = false */)
try
{
	sleep(5);  // wait for apps to get started

	bool   firstError = true;
	size_t loopCount  = -1;  //first time through loop will be 0

	std::map<std::string /* appName */, bool /* lastStatusGood */> appLastStatusGood;

	std::unique_ptr<TransceiverSocket>
	       remoteGatewaySocket;  //use to get remote gateway status
	bool   commandingRemoteGatewayApps = false;
	size_t commandRemoteIdleCount      = 0;
	int    portForReverseLoginOverUDP  = 0;  //if 0, then not reverse login not enabled
	std::string ipAddressForStateChangesOverUDP = "";  //if "", then not enabled

	std::map<std::string /* context uid */,
	         std::pair<int64_t /* available log space KB */,
	                   int64_t /* available data space KB */>>
	    availableDiskSpaceKB_map;
	// Single shared "last alert" timestamp per disk per context — any window firing
	// resets it, so a flood of correlated alerts is suppressed. Faster-window
	// thresholds still get more chances because they use shorter silence periods.
	std::map<std::string /* context uid */, time_t /* last alert */>
	    rateToLogDiskAlert_map, rateToDataDiskAlert_map;
	// Per-disk "first time the trip condition was observed" — used to require the
	// condition to be sustained for a few seconds before firing. Cleared on any
	// pass that does not see a trip, so brief transients reset the clock.
	std::map<std::string /* context uid */, time_t /* first trip seen */>
	    firstTripLogObserved_map, firstTripDataObserved_map;
	// Time-suppression for the hard-low "available disk space low" alarm so a
	// disk hovering near MIN does not spam every status pass.
	std::map<std::string /* context uid */, time_t /* last alert */> hardLowLogAlert_map,
	    hardLowDataAlert_map;
	// Workloop start time — used to skip the rate alarms during a warmup window
	// while the historical-sample deque is dominated by the seed value (which is
	// usually captured during the noisy startup burst).
	const time_t workloopStartTime = time(0);

	int64_t availableLogSpaceKB_MIN = 0, availableDataSpaceKB_MIN = 0;

	try  // Note!! User can prevent data check by export OTSDAQ_LOG_DISK_MINIMUM=0
	{
		availableLogSpaceKB_MIN = std::stoull(__ENV__("OTSDAQ_LOG_DISK_MINIMUM"));
	}
	catch(...)
	{
		availableLogSpaceKB_MIN = 1000000;  //1 GB default in KBs;
	}
	__COUTV__(availableLogSpaceKB_MIN);

	try  // Note!! User can prevent data check by export OTSDAQ_DATA_DISK_MINIMUM=0
	{
		availableDataSpaceKB_MIN = std::stoull(__ENV__("OTSDAQ_DATA_DISK_MINIMUM"));
	}
	catch(...)
	{
		availableDataSpaceKB_MIN = 1000000;  //1 GB default in KBs;
	}
	__COUTV__(availableDataSpaceKB_MIN);

	auto formatRateKBps = [](float rateKBps) -> std::string {
		float       absRate = rateKBps < 0 ? -rateKBps : rateKBps;
		float       value;
		const char* unit;
		if(absRate < 1024.f)
		{
			value = rateKBps;
			unit  = " KB/s";
		}
		else if(absRate < 1024.f * 1024.f)
		{
			value = rateKBps / 1024.f;
			unit  = " MB/s";
		}
		else
		{
			value = rateKBps / (1024.f * 1024.f);
			unit  = " GB/s";
		}
		char buf[32];
		snprintf(buf, sizeof(buf), "%.1f", value);
		return std::string(buf) + unit;
	};
	const std::string otsdaq_log_dir  = __ENV__("OTSDAQ_LOG_DIR");
	const std::string otsdaq_data_dir = __ENV__("OTSDAQ_DATA");

	if(doDisconnected)
		sleep(5);  // stagger the two loops a bit
	__COUTV__(doDisconnected);

	std::chrono::_V2::system_clock::time_point lastStatus =
	    std::chrono::high_resolution_clock::now();
	time_t lastSlowStatusWarnTime = 0;
	size_t statusWasSlowCount     = 0;

	std::string value;

	ConfigurationManager
	    cfgMgr;  //for local use handling latest icons and remote subsystem info

	while(1)
	{
		bool oneStatusReqHasFailed = false;

		++loopCount;
		usleep(500000 /* 0.5 seconds */);

		//lock to access appLastStatusGood_ map (between disconnected and connected handling threads)
		{
			std::lock_guard<std::mutex> lock(theSupervisor->dualStatusThreadMutex_);
			appLastStatusGood = theSupervisor->appLastStatusGood_;
		}

		// workloop procedure
		//	Loop through all Apps and request status
		//	sleep

		__COUTS__(TLVL_StatusWorkloop)
		    << "App status checking, doDisconnected = " << doDisconnected
		    << " loopCount=" << loopCount << __E__;

		//if last status was more than 1.5 seconds ago, then warn
		if(!doDisconnected)
		{
			auto now = std::chrono::high_resolution_clock::now();
			auto timeElapsed =
			    std::chrono::duration_cast<std::chrono::milliseconds>(now - lastStatus)
			        .count();  //milliseconds
			__COUTVS__(TLVL_StatusWorkloop, timeElapsed);
			if(timeElapsed > 1500)  //then status was too slow!
			{
				++statusWasSlowCount;

				if(statusWasSlowCount >
				   2)  //1 or 2 might occur if target is disconnected or during large system calls (e.g. configuration dumps)
				{
					//if it has been more than 15 minutes, then do System Alert
					time_t now_time_t = time(0);
					__COUT_WARN__
					    << "App status checking loop time elapsed = " << timeElapsed
					    << " ms (expected ~500 ms). Time since last system alert for "
					       "slow status = "
					    << now_time_t - lastSlowStatusWarnTime << " seconds." << __E__;
					if(now_time_t - lastSlowStatusWarnTime > 15 * 60)
					{
						std::stringstream errSs;
						errSs << "GatewaySupervisor App Status checking loop is taking "
						         "longer than expected - "
						      << "there may be too many TRACE slow path messages "
						         "enabled. Time elapsed = "
						      << timeElapsed << " ms (expected ~500 ms).";
						theSupervisor->addSystemMessage("*", errSs.str());
						__COUT_ERR__ << errSs.str() << __E__;
						lastSlowStatusWarnTime = now_time_t;
					}
				}
			}
			else
				statusWasSlowCount = 0;  //reset counter if back to normal speed
			lastStatus = now;
		}  //end slow status monitoring

		if(TTEST(1) ||
		   doDisconnected)  //printout the true/false handling of apps (emulating anticipated flow)
		{
			uint32_t handlingAppCount = 0;
			for(const auto& it : theSupervisor->allSupervisorInfo_.getAllSupervisorInfo())
			{
				auto               appInfo = it.second;
				const std::string& appName = appInfo.getName();

				bool isDisconnected =
				    appLastStatusGood.find(appName) != appLastStatusGood.end() &&
				    !appLastStatusGood.at(appName);

				if(appInfo.isGatewaySupervisor())  // get gateway status
				{
					std::vector<GatewaySupervisor::RemoteGatewayInfo>
					    remoteApps;  //local copy
					{                //lock for remainder of scope
						std::lock_guard<std::mutex> lock(
						    theSupervisor->remoteGatewayAppsMutex_);
						remoteApps = theSupervisor->remoteGatewayApps_;
					}

					for(auto& remoteGatewayApp : remoteApps)
					{
						bool isRemoteAppDisconnected =
						    appLastStatusGood.find(remoteGatewayApp.appInfo.url +
						                           remoteGatewayApp.appInfo.name) !=
						        appLastStatusGood.end() &&
						    !appLastStatusGood.at(remoteGatewayApp.appInfo.url +
						                          remoteGatewayApp.appInfo.name);

						//skip based on disconnected status
						if(doDisconnected && !isRemoteAppDisconnected)
							continue;
						if(!doDisconnected && isRemoteAppDisconnected)
							continue;

						++handlingAppCount;
						__COUTT__
						    << "Status loopcount=" << loopCount << " remote apps #"
						    << handlingAppCount
						    << ", (doDisconnected = " << doDisconnected
						    << ") Remote subapp = '" << remoteGatewayApp.appInfo.name
						    << "' [URL=" << remoteGatewayApp.appInfo.url
						    << "] isRemoteAppDisconnected = " << isRemoteAppDisconnected
						    << ".\n\n";
					}  //end remote app loop
				}

				//skip based on disconnected status
				if(doDisconnected && !isDisconnected)
					continue;
				if(!doDisconnected && isDisconnected)
					continue;

				++handlingAppCount;
				__COUTT__ << "Status loopcount=" << loopCount << " apps #"
				          << handlingAppCount << ", (doDisconnected = " << doDisconnected
				          << ") Supervisor instance = '" << appName
				          << "' [LID=" << appInfo.getId() << "] in Context '"
				          << appInfo.getContextName() << "' [URL=" << appInfo.getURL()
				          << "] isDisconnected = " << isDisconnected << ".\n\n";
			}  //end app loop

			if(doDisconnected)
			{
				__COUTTV__(handlingAppCount);
				//when nothinig to do, still proceed to reset remote gateway apps and get icon updates
			}
		}  //end debugging of handling of apps

		for(const auto& it : theSupervisor->allSupervisorInfo_.getAllSupervisorInfo())
		{
			std::string                             status, progress, detail;
			std::vector<SupervisorInfo::SubappInfo> subapps;
			int                                     progressInteger;

			int64_t            availableLogSpaceKB = 0, availableDataSpaceKB = 0;
			auto               appInfo = it.second;
			const std::string& appName = appInfo.getName();

			bool isDisconnected =
			    appLastStatusGood.find(appName) != appLastStatusGood.end() &&
			    !appLastStatusGood.at(appName);

			__COUTS__(TLVL_StatusWorkloop)
			    << "Start of status loop, doDisconnected = " << doDisconnected
			    << " Supervisor instance = '" << appName << "' [LID=" << appInfo.getId()
			    << "] in Context '" << appInfo.getContextName()
			    << "' [URL=" << appInfo.getURL()
			    << "] isDisconnected = " << isDisconnected << ".\n\n";

			//if doDisconnected is true, only check disconnected apps
			//	AND disconnected subapps within gateway!
			//skip all connected non-gateway supervisors

			// if the application is the gateway supervisor, we do not send a SOAP message
			if(appInfo.isGatewaySupervisor())  // get gateway status
			{
				bool resetRemoteGatewayApps = false;

				try
				{
					if(!doDisconnected)  //primary gateway (self) is never disconnected
					{
						availableLogSpaceKB =
						    theSupervisor
						        ->CorePropertySupervisorBase::getAvailableLogSpaceKB();
						availableDataSpaceKB =
						    theSupervisor
						        ->CorePropertySupervisorBase::getAvailableDataSpaceKB();

						__COUTVS__(TLVL_StatusWorkloop, availableLogSpaceKB);
						__COUTVS__(TLVL_StatusWorkloop, availableDataSpaceKB);

						// send back status and progress parameters
						const std::string& err =
						    theSupervisor->theStateMachine_.getErrorMessage();
						try
						{
							__COUTVS__(TLVL_StatusWorkloop,
							           theSupervisor->theStateMachine_.isInTransition());
							if(theSupervisor->theStateMachine_.isInTransition())
								__COUTVS__(TLVL_StatusWorkloop,
								           theSupervisor->theStateMachine_
								               .getCurrentTransitionName());
							__COUTVS__(
							    TLVL_StatusWorkloop,
							    theSupervisor->theStateMachine_.getProvenanceStateName());
							__COUTVS__(
							    TLVL_StatusWorkloop,
							    theSupervisor->theStateMachine_.getCurrentStateName());
						}
						catch(...)
						{
							;
						}

						if(err == "")
						{
							if(theSupervisor->theStateMachine_
							       .isInTransition())  // || theSupervisor->theProgressBar_.read() < 100)
							{
								// attempt to get transition name, otherwise give provenance state
								try
								{
									status = theSupervisor->theStateMachine_
									             .getCurrentTransitionName();
								}
								catch(...)
								{
									status = theSupervisor->theStateMachine_
									             .getProvenanceStateName();
								}
								progress =
								    theSupervisor->theProgressBar_.readPercentageString();
							}
							else
							{
								status =
								    theSupervisor->theStateMachine_.getCurrentStateName();
								progress = "100";  //if not in transition, then 100
							}
						}
						else
						{
							status =
							    (theSupervisor->theStateMachine_.getCurrentStateName() ==
							             RunControlStateMachine::PAUSED_STATE_NAME
							         ? "Soft-Error:::"
							         : "Failed:::") +
							    err;
							progress =
							    theSupervisor->theProgressBar_.readPercentageString();
						}

						__COUTVS__(TLVL_StatusWorkloop, status);
						__COUTVS__(TLVL_StatusWorkloop, progress);

						try
						{
							detail = (theSupervisor->theStateMachine_.isInTransition()
							              ? theSupervisor->theStateMachine_
							                    .getCurrentTransitionName(
							                        theSupervisor
							                            ->stateMachineLastCommandInput_)
							              : (std::string("Uptime: ") +
							                 StringMacros::encodeURIComponent(
							                     StringMacros::getTimeDurationString(
							                         theSupervisor
							                             ->CorePropertySupervisorBase::
							                                 getSupervisorUptime())) +
							                 ", Time-in-state: " +
							                 StringMacros::encodeURIComponent(
							                     StringMacros::getTimeDurationString(
							                         theSupervisor->theStateMachine_
							                             .getTimeInState()))));
							// make sure broadcast message status is not being updated
							std::lock_guard<std::mutex> lock(
							    theSupervisor->broadcastCommandStatusUpdateMutex_);
							if(detail != "" &&
							   theSupervisor->broadcastCommandStatus_ != "")
								detail += " - " + theSupervisor->broadcastCommandStatus_;

							if(!theSupervisor->theStateMachine_.isInTransition() &&
							   (theSupervisor->theStateMachine_.getCurrentStateName() ==
							        RunControlStateMachine::CONFIGURED_STATE_NAME ||
							    theSupervisor->theStateMachine_.getCurrentStateName() ==
							        RunControlStateMachine::RUNNING_STATE_NAME ||
							    theSupervisor->theStateMachine_.getCurrentStateName() ==
							        RunControlStateMachine::PAUSED_STATE_NAME))
							{
								//add Configuration details
								detail += " - Configured";
								{
									bool hasCommon =
									    theSupervisor
									        ->activeStateMachineSubsystemCommonList_
									        .size();
									bool hasOverride =
									    theSupervisor
									        ->activeStateMachineSubsystemCommonOverrideList_
									        .size();
									if(hasCommon && hasOverride)
										detail +=
										    ", with SubsystemCommon tables and "
										    "SubsystemCommonOverride tables,";
									else if(hasCommon)
										detail += ", with SubsystemCommon tables,";
									else if(hasOverride)
										detail +=
										    ", with SubsystemCommonOverride tables,";
								}
								detail +=
								    " with System Configuration Alias '" +
								    theSupervisor->activeStateMachineConfigurationAlias_ +
								    "' which translates to " +
								    theSupervisor->theConfigurationTableGroup_.first +
								    "(" +
								    theSupervisor->theConfigurationTableGroup_.second
								        .str() +
								    "). Active Context Group " +
								    theSupervisor
								        ->CorePropertySupervisorBase::
								            theConfigurationManager_->getActiveGroupName(
								                ConfigurationManager::GroupType::
								                    CONTEXT_TYPE) +
								    "(" +
								    theSupervisor
								        ->CorePropertySupervisorBase::
								            theConfigurationManager_
								        ->getActiveGroupKey(
								            ConfigurationManager::GroupType::CONTEXT_TYPE)
								        .str() +
								    ").";

								if(theSupervisor->theConfigurationTableGroup_ !=
								   theSupervisor->cachedGlobalFieldsGroup_)
								{
									theSupervisor->cachedGlobalFieldsGroup_ =
									    theSupervisor->theConfigurationTableGroup_;
									theSupervisor->cachedGlobalFieldsString_ =
									    getGlobalFieldsString(
									        theSupervisor->CorePropertySupervisorBase::
									            theConfigurationManager_);
								}
								detail += theSupervisor->cachedGlobalFieldsString_;
							}

							if(!theSupervisor->theStateMachine_.isInTransition() &&
							   (theSupervisor->theStateMachine_.getCurrentStateName() ==
							        RunControlStateMachine::INITIAL_STATE_NAME ||
							    theSupervisor->theStateMachine_.getCurrentStateName() ==
							        RunControlStateMachine::HALTED_STATE_NAME))
							{
								std::lock_guard<std::mutex> ctxLock(
								    theSupervisor->contextCommonMutex_);
								if(theSupervisor->appliedContextCommonList_.size())
									detail += " | ContextCommon: " +
									          theSupervisor->appliedContextCommonList_;
								if(theSupervisor->appliedContextCommonOverrideList_
								       .size())
									detail +=
									    " | ContextCommonOverride: " +
									    theSupervisor->appliedContextCommonOverrideList_;
							}
						}
						catch(...)
						{
							detail = "";
						}
					}  //end gateway supervisor primary status retrieval

					//now handle remote gateway info gathering

					std::vector<GatewaySupervisor::RemoteGatewayInfo>
					    remoteApps;  //local copy to avoid long mutex lock
					{                //lock for remainder of scope
						std::lock_guard<std::mutex> lock(
						    theSupervisor->remoteGatewayAppsMutex_);
						remoteApps = theSupervisor->remoteGatewayApps_;

						//check for commands
						if(!commandingRemoteGatewayApps)
						{
							size_t ri = 0;
							for(const auto& remoteApp : remoteApps)
							{
								__COUTT__ << "#" << ri++ << " - Checking remote app '"
								          << remoteApp.appInfo.name
								          << "' [URL=" << remoteApp.appInfo.url
								          << "] for command: '" << remoteApp.command
								          << "'." << __E__;

								if(remoteApp.command != "")
								{
									//latch until all remote apps are stable
									commandingRemoteGatewayApps =
									    doDisconnected
									        ? false
									        : true;  //only command in primary (non-disconnected-handling) status thread
									break;
								}
							}
						}
					}  //end copy of remote apps
					__COUTS__(TLVL_StatusRemoteWorkloop)
					    << "doDisconnected=" << doDisconnected << " commanding? "
					    << commandingRemoteGatewayApps
					    << " remoteApps.size()=" << remoteApps.size()
					    << " loopCount=" << loopCount << __E__;

					//Add sub-apps for each Remote Gateway specified as a Remote Desktop Icon
					if(  //periodically refresh Remote Gateway list based on icon list
					    //disconnect version will handle refreshing
					    //primary version does first time to init apps and socket
					    (loopCount ==
					     0) ||  //must init socket first time! (for both thread types)
					    (remoteApps.size() &&
					     !remoteGatewaySocket) ||  //if there are app (from other loop) but socket not init'd
					    (doDisconnected &&
					     loopCount))  //% 20 == 0) ) //!commandingRemoteGatewayApps &&
					{
						__COUTS__(TLVL_StatusWorkloop)
						    << "Doing remote gateway icon/subapp refresh, doDisconnected "
						       "= "
						    << doDisconnected << " Supervisor instance = '" << appName
						    << "' [LID=" << appInfo.getId() << "] in Context '"
						    << appInfo.getContextName() << "' [URL=" << appInfo.getURL()
						    << "].\n\n";

						// use latest context always from temporary configuration manager,
						//	to get updated icons (and remote subsystem info) every time...
						bool        useLatestIcons = false;
						std::string timeString;
						std::pair<std::string /*group name*/, TableGroupKey> latestGroup;
						std::vector<DesktopIconTable::DesktopIcon>           icons;
						{  //start lock scope
							std::lock_guard<std::mutex> lock(
							    theSupervisor->latestGatewayIconsMutex_);
							latestGroup = theSupervisor->latestGatewayIconsContextGroup_;
						}  //end lock scope
						if(latestGroup.first.size())
						{
							std::pair<std::string /*group name*/, TableGroupKey>
							    theGroup = ConfigurationManager::loadGroupNameAndKey(
							        ConfigurationManager::
							            LAST_ACTIVATED_CONTEXT_GROUP_FILE,
							        timeString);
							if(theGroup == latestGroup)
							{
								useLatestIcons = true;
								__COUTS__(TLVL_StatusWorkloop)
								    << "Using cached latest icons for context group '"
								    << theGroup.first << "(" << theGroup.second << ")"
								    << __E__;
								std::lock_guard<std::mutex> lock(
								    theSupervisor->latestGatewayIconsMutex_);
								icons = theSupervisor->latestGatewayIcons_;
							}
						}  //end check for active context changing

						if(!useLatestIcons)  //then need to load latest icons
						{
							try
							{
								// Restoring active backbone/context group, note: not using Gateway instance's member CorePropertySupervisorBase::theConfigurationManager_
								cfgMgr.restoreActiveTableGroups(
								    true /*throwErrors*/,
								    "" /*pathToActiveGroupsFile*/,
								    ConfigurationManager::LoadGroupType::
								        ONLY_BACKBONE_OR_CONTEXT_TYPES /*onlyLoadIfBackboneOrContext*/
								);

								const DesktopIconTable* iconTable =
								    cfgMgr.__GET_CONFIG__(DesktopIconTable);
								{  //start lock scope
									std::lock_guard<std::mutex> lock(
									    theSupervisor->latestGatewayIconsMutex_);
									theSupervisor->latestGatewayIcons_ =
									    iconTable
									        ->getAllDesktopIcons();  //cache latest icons (for use, e.g., in remote login verify)
									icons =
									    theSupervisor
									        ->latestGatewayIcons_;  //use for this loop
									theSupervisor->latestGatewayIconsContextGroup_ =
									    cfgMgr.getActiveTableGroups()
									        [ConfigurationManager::
									             GROUP_TYPE_NAME_CONTEXT];
								}  //end lock scope
							}
							catch(...)
							{
								__COUT_ERR__
								    << "Error loading latest context for remote gateway "
								       "icon refresh. Sticking with old icons."
								    << __E__;
								std::lock_guard<std::mutex> lock(
								    theSupervisor->latestGatewayIconsMutex_);
								icons = theSupervisor->latestGatewayIcons_;
							}
						}

						for(auto& remoteGatewayApp : remoteApps)
							remoteGatewayApp.appInfo.status =
							    "";  //clear status, to be used to remove remote gateways no longer targeted

						resetRemoteGatewayApps = true;

						for(const auto& icon : icons)
						{
							__COUTTV__(icon.windowContentURL_);
							if(icon.windowContentURL_.size() > 4 &&
							   icon.windowContentURL_[0] == 'o' &&
							   icon.windowContentURL_[1] == 't' &&
							   icon.windowContentURL_[2] == 's' &&
							   icon.windowContentURL_[3] == ':')
							{
								__COUTT__ << "Found '" << icon.recordUID_
								          << "' remote gateway icons url: "
								          << icon.windowContentURL_ << __E__;

								GatewaySupervisor::RemoteGatewayInfo thisInfo;

								std::string remoteURL         = icon.windowContentURL_;
								std::string remoteLandingPage = "";
								std::string remoteSetupType   = "";
								//remote ? parameters from remoteURL
								if(remoteURL.find('?') != std::string::npos)
								{
									__COUTT__
									    << "Extracting GET ? parameters from remote url."
									    << __E__;
									std::vector<std::string> urlSplit =
									    StringMacros::getVectorFromString(remoteURL,
									                                      {'?'});
									if(urlSplit.size() > 0)
										remoteURL = urlSplit[0];
									if(urlSplit.size() > 1)
									{
										//look for 'LandingPage' parameter
										std::vector<std::string> parameterPairs =
										    StringMacros::getVectorFromString(urlSplit[1],
										                                      {'&'});
										for(const auto& parameterPair : parameterPairs)
										{
											std::vector<std::string> parameterPairSplit =
											    StringMacros::getVectorFromString(
											        parameterPair, {'='});
											if(parameterPairSplit.size() == 2)
											{
												__COUTT__ << "Found remote URL parameter "
												          << parameterPairSplit[0] << ", "
												          << parameterPairSplit[1]
												          << __E__;
												if(parameterPairSplit[0] == "LandingPage")
												{
													remoteLandingPage =
													    StringMacros::decodeURIComponent(
													        parameterPairSplit[1]);
													if(remoteLandingPage.find(
													       icon.folderPath_) != 0)
														remoteLandingPage =
														    icon.folderPath_ + "/" +
														    remoteLandingPage;
													__COUTT__ << "Found landing page "
													          << remoteLandingPage
													          << " for "
													          << icon.recordUID_ << __E__;
												}
												if(parameterPairSplit[0] == "SetupType")
												{
													remoteSetupType =
													    StringMacros::decodeURIComponent(
													        parameterPairSplit[1]);

													__COUTT__
													    << "Found setup_ots.sh type "
													    << remoteSetupType << " for "
													    << icon.recordUID_ << __E__;
												}
											}
										}
									}
								}  //end remote URL parameter handling

								thisInfo.appInfo.name   = icon.recordUID_;
								thisInfo.appInfo.status = SupervisorInfo::
								    APP_STATUS_UNKNOWN;  //non-empty string indicates this app exists
								thisInfo.appInfo.progress = 0;
								thisInfo.appInfo.detail   = "";
								thisInfo.appInfo.url =
								    remoteURL;  //icon.windowContentURL_;
								thisInfo.appInfo.class_name     = "Remote Gateway";
								thisInfo.appInfo.lastStatusTime = time(0);

								thisInfo.user_data_path_record = icon.alternateText_;
								thisInfo.parentIconFolderPath  = icon.folderPath_;
								thisInfo.permissionThresholdString =
								    icon.permissionThresholdString_;
								thisInfo.landingPage = remoteLandingPage;
								thisInfo.setupType   = remoteSetupType;

								try
								{
									cfgMgr.getOtherSubsystemInstanceInfo(
									    thisInfo.user_data_path_record,
									    &thisInfo.instancePath,
									    &thisInfo.instanceHost,
									    &thisInfo.instanceUser,
									    &thisInfo.fullName);
								}
								catch(...)
								{
									;
								}  //ignore any errors getting full name and instance info

								//replace or add to local copy of supervisor remote gateway list (control info will be protected later, after status update, with final copy to real list)
								bool   found = false;
								size_t r     = 0;
								for(; r < remoteApps.size(); ++r)
								{
									if(thisInfo.appInfo.name ==
									   remoteApps[r].appInfo.name)
									{
										found = true;

										// clang-format off
										//overwrite with refreshed info
										remoteApps[r].appInfo 					= thisInfo.appInfo;
										remoteApps[r].user_data_path_record 	= thisInfo.user_data_path_record;
										remoteApps[r].parentIconFolderPath 		= thisInfo.parentIconFolderPath;
										remoteApps[r].permissionThresholdString = thisInfo.permissionThresholdString;
										remoteApps[r].landingPage 				= thisInfo.landingPage;
										remoteApps[r].setupType   				= thisInfo.setupType;
										remoteApps[r].fullName    				= thisInfo.fullName;
										remoteApps[r].instancePath 				= thisInfo.instancePath;
										remoteApps[r].instanceHost 				= thisInfo.instanceHost;
										remoteApps[r].instanceUser 				= thisInfo.instanceUser;
										// clang-format on

										break;
									}
								}

								if(!found)  //add
									remoteApps.push_back(thisInfo);

								//if possible, get CSV list of potential config_aliases (for dropdown)
								try
								{
									__COUTTV__(remoteApps[r].fsmName);

									remoteApps[r].config_aliases.clear();
									remoteApps[r].config_aliases =
									    cfgMgr.getOtherSubsystemConfigAliases(
									        remoteApps[r]
									            .user_data_path_record);  //getOtherSubsystemFilteredConfigAliases(remoteApps[i].user_data_path_record, remoteApps[i].fsmName);

									__COUTTV__(StringMacros::setToString(
									    thisInfo.config_aliases));
								}
								catch(const std::runtime_error& e)
								{
									__SS__
									    << "Error at "
									    << StringMacros::getTimestampString() << ":"
									    << "\nFailed to retrieve the list of "
									       "Configuration "
									       "Aliases for Remote Subsystem '"
									    << remoteApps[r].appInfo.name
									    << ".' Remote Subsystems are specified through "
									       "their Desktop Icon record. "
									       "Please specify a valid User Data Path record "
									       "as the Desktop Icon AlternateText field, "
									       "targeting a UID in the "
									       "SubsystemUserDataPathsTable."
									    << __E__;
									if(std::string(e.what()).find("Backbone") !=
									   std::string::npos)
										ss << "\n\n" << REMOTE_BACKBONE_ERR;
									ss << "\n\nHere was the error getting the list of "
									      "Remote Subsystem "
									      "Configuration Aliases:\n"
									   << e.what() << __E__;
									__COUT__ << ss.str();
									remoteApps[r].setError(ss.str());
								}

								// Since there are two threads, need to give feedback immediately to primary gateway struct
								//	 for the updated fields in this section (i.e., the disconnected thread will not update these fields in the final copy for connected apps):
								//		- error
								//		- config_aliases,
								//		- (not selected_config_alias because it should be updated in final copy by correct thread)
								//		- user_data_path_record
								//		- parentIconFolderPath
								//		- permissionThresholdString
								//		- landingPage
								//		- setupType
								//		- fullName
								//		- instancePath
								//		- instanceHost
								//		- instanceUser
								{  //handle copy into primary gateway structure of updated info
									bool found = false;
									//lock for remainder of scope
									std::lock_guard<std::mutex> lock(
									    theSupervisor->remoteGatewayAppsMutex_);
									for(size_t i = 0;
									    i < theSupervisor->remoteGatewayApps_.size();
									    ++i)
										if(remoteApps[r].appInfo.name ==
										   theSupervisor->remoteGatewayApps_[i]
										       .appInfo.name)
										{
											found = true;

											// clang-format off
											if(remoteApps[r].getError() != "")
												theSupervisor->remoteGatewayApps_[i].copyError(remoteApps[r]);

											//overwrite with refreshed info
											theSupervisor->remoteGatewayApps_[i].config_aliases 			= remoteApps[r].config_aliases;
											theSupervisor->remoteGatewayApps_[i].user_data_path_record 		= remoteApps[r].user_data_path_record;
											theSupervisor->remoteGatewayApps_[i].parentIconFolderPath 		= remoteApps[r].parentIconFolderPath;
											theSupervisor->remoteGatewayApps_[i].permissionThresholdString 	= remoteApps[r].permissionThresholdString;
											theSupervisor->remoteGatewayApps_[i].landingPage 				= remoteApps[r].landingPage;
											theSupervisor->remoteGatewayApps_[i].setupType   				= remoteApps[r].setupType;
											theSupervisor->remoteGatewayApps_[i].fullName    				= remoteApps[r].fullName;
											theSupervisor->remoteGatewayApps_[i].instancePath 				= remoteApps[r].instancePath;
											theSupervisor->remoteGatewayApps_[i].instanceHost 				= remoteApps[r].instanceHost;
											theSupervisor->remoteGatewayApps_[i].instanceUser 				= remoteApps[r].instanceUser;
											// clang-format on
											__COUTTV__(StringMacros::setToString(
											    theSupervisor->remoteGatewayApps_[i]
											        .config_aliases));
											__COUTTV__(StringMacros::setToString(
											    remoteApps[r].config_aliases));
											break;
										}  //end copy into primary gateway structure of updated info

									if(!found)
									{
										__COUT__ << "Adding new remote gateway app '"
										         << remoteApps[r].appInfo.name
										         << "' to primary gateway structure."
										         << __E__;
										theSupervisor->remoteGatewayApps_.push_back(
										    remoteApps[r]);
									}
								}  //end handle copy into primary gateway structure of updated info

							}  //end remote icon handling

						}  //end Gateway icon loop searching for remote subsystems

						//clean up stale remoteGatewayApps with blank status
						bool remoteAppsExist = false;
						for(size_t r = 0; r < remoteApps.size(); ++r)
						{
							__COUTT__ << "#" << r << " - Checking remote app '"
							          << remoteApps[r].appInfo.name
							          << "' [URL=" << remoteApps[r].appInfo.url
							          << "] for status: '" << remoteApps[r].appInfo.status
							          << "'." << __E__;

							if(remoteApps[r].appInfo.status == "")
							{
								__COUT__ << "Removing stale remote gateway app '"
								         << remoteApps[r].appInfo.name
								         << "' from Gateway app list." << __E__;

								{  //handle remove from primary gateway structure
									bool found = false;
									//lock for remainder of scope
									std::lock_guard<std::mutex> lock(
									    theSupervisor->remoteGatewayAppsMutex_);
									for(size_t i = 0;
									    i < theSupervisor->remoteGatewayApps_.size();
									    ++i)
										if(remoteApps[r].appInfo.name ==
										   theSupervisor->remoteGatewayApps_[i]
										       .appInfo.name)
										{
											found = true;

											theSupervisor->remoteGatewayApps_.erase(
											    theSupervisor->remoteGatewayApps_
											        .begin() +
											    i);
											break;
										}  //end removal from primary gateway structure

									if(!found)
									{
										__COUT_WARN__
										    << "Could not find stale remote gateway app '"
										    << remoteApps[r].appInfo.name
										    << "' to remove from primary gateway "
										       "structure!"
										    << __E__;
									}
								}  //end handle remove from primary gateway structure

								//rewind and erase also locally
								remoteApps.erase(remoteApps.begin() + r);
								--r;
							}  //end removal of stale remote app with blank status
						}      //end clean up stale remoteGatewayApps with blank status
						remoteAppsExist = remoteApps.size();

						if(remoteAppsExist &&
						   !remoteGatewaySocket)  //instantiate socket first time there are remote apps
						{
							__COUT_INFO__ << "Instantiating Remote Gateway App Status "
							                 "Socket (doDisconnected = "
							              << doDisconnected << ")!" << __E__;
							ConfigurationTree configLinkNode =
							    theSupervisor->CorePropertySupervisorBase::
							        getSupervisorTableNode();
							ipAddressForStateChangesOverUDP =
							    configLinkNode.getNode("IPAddressForStateChangesOverUDP")
							        .getValue<std::string>();
							__COUTTV__(ipAddressForStateChangesOverUDP);
							theSupervisor->ipAddressForStateChangesOverUDP_ =
							    ipAddressForStateChangesOverUDP;

							//check if allowing reverse login verification from remote Gateways to this Gateway
							if(theSupervisor->theWebUsers_.getSecurity() ==
							   WebUsers::SECURITY_TYPE_DIGEST_ACCESS)
							{
								bool enableLoginVerify =
								    configLinkNode
								        .getNode("EnableAckForStateChangesOverUDP")
								        .getValue<bool>();

								if(enableLoginVerify)
								{
									portForReverseLoginOverUDP =
									    configLinkNode
									        .getNode("PortForStateChangesOverUDP")
									        .getValue<int>();
									theSupervisor->portForReverseLoginOverUDP_ =
									    portForReverseLoginOverUDP;
									if(portForReverseLoginOverUDP)
										__COUT_INFO__
										    << "Enabling reverse login verification for "
										       "Remote Gateways at "
										    << ipAddressForStateChangesOverUDP << ":"
										    << portForReverseLoginOverUDP << __E__;
								}
								else
								{
									__COUT_WARN__
									    << "Remote login verification at this Gateway, "
									       "for other Remote Gateways, is not enabled "
									       "unless the GatewaySupervisor has "
									       "'EnableStateChangesOverUDP' enabled."
									    << __E__;
								}
							}
							remoteGatewaySocket = std::make_unique<TransceiverSocket>(
							    ipAddressForStateChangesOverUDP);
							remoteGatewaySocket->initialize(
							    8 * 1024 * 1024 /*socketReceiveBufferSize=8MB*/);

							__COUTT__
							    << "Remote Gateway App Status Socket initialized. Port: "
							    << remoteGatewaySocket->getPort()
							    << ", doDisconnected=" << doDisconnected << __E__;
						}  //end initializing remote gateway socket

					}  //end periodic Remote Gateway refresh

					//assert socket is initialized if there are remote apps
					if(remoteApps.size() && !remoteGatewaySocket)
					{
						__SS__ << "Impossible no remote gateway socket available, "
						          "doDisconnected = "
						       << doDisconnected << __E__;
						__SS_THROW__;
					}

					//request icons more often from disconnected thread, and not from primary thread
					__COUTT__ << "(doDisconnected = " << doDisconnected
					          << ") resetRemoteGatewayApps = " << resetRemoteGatewayApps
					          << __E__;

					std::set<std::string /* appName */>
					    remoteAppsHandledByThread;  //track which apps are handled in this pass, so they can be updated at the end

					//refresh Context Common Table lists from active Backbone for status requests to remote gateways
					//  Only the connected thread refreshes the cache; both threads read from cached values.
					std::string contextCommonList, contextCommonOverrideList;
					if(!doDisconnected)
					{
						try
						{
							std::string timeString;
							auto        activeBackbone =
							    ConfigurationManager::loadGroupNameAndKey(
							        ConfigurationManager::
							            LAST_ACTIVATED_BACKBONE_GROUP_FILE,
							        timeString);
							std::string backboneKey = activeBackbone.first + ":" +
							                          activeBackbone.second.toString();

							if(backboneKey !=
							   theSupervisor->cachedSubsystemCommonBackboneKey_)
							{
								ConfigurationManager temporaryConfigMgr;
								theSupervisor->cachedSubsystemCommonList_         = "";
								theSupervisor->cachedSubsystemCommonOverrideList_ = "";
								theSupervisor->cachedSubsystemCommonContextList_  = "";
								theSupervisor->cachedSubsystemCommonContextOverrideList_ =
								    "";
								try
								{
									theSupervisor->cachedSubsystemCommonList_ =
									    StringMacros::setToString(
									        temporaryConfigMgr.getVersionAliases(
									            ConfigurationManager::
									                SUBSYSTEM_COMMON_VERSION_ALIAS));
								}
								catch(...)
								{
								}
								try
								{
									theSupervisor
									    ->cachedSubsystemCommonOverrideList_ = StringMacros::
									    setToString(temporaryConfigMgr.getVersionAliases(
									        ConfigurationManager::
									            SUBSYSTEM_COMMON_OVERRIDE_VERSION_ALIAS));
								}
								catch(...)
								{
								}
								try
								{
									theSupervisor
									    ->cachedSubsystemCommonContextList_ = StringMacros::
									    setToString(temporaryConfigMgr.getVersionAliases(
									        ConfigurationManager::
									            SUBSYSTEM_COMMON_CONTEXT_VERSION_ALIAS));
								}
								catch(...)
								{
								}
								try
								{
									theSupervisor
									    ->cachedSubsystemCommonContextOverrideList_ =
									    StringMacros::setToString(
									        temporaryConfigMgr.getVersionAliases(
									            ConfigurationManager::
									                SUBSYSTEM_COMMON_CONTEXT_OVERRIDE_VERSION_ALIAS));
								}
								catch(...)
								{
								}
								theSupervisor->cachedSubsystemCommonBackboneKey_ =
								    backboneKey;
							}
						}
						catch(...)
						{
						}
						theSupervisor->activeSubsystemCommonContextList_ =
						    theSupervisor->cachedSubsystemCommonContextList_;
						theSupervisor->activeSubsystemCommonContextOverrideList_ =
						    theSupervisor->cachedSubsystemCommonContextOverrideList_;
					}
					contextCommonList = theSupervisor->cachedSubsystemCommonContextList_;
					contextCommonOverrideList =
					    theSupervisor->cachedSubsystemCommonContextOverrideList_;

					//for each remote gateway, request app status with "GetRemoteAppStatus"
					bool gettingRemoteStatus = false;
					if(1 || loopCount % 3 == 0 ||    //most frequent
					   resetRemoteGatewayApps ||     //a little less frequently
					   commandingRemoteGatewayApps)  //least frequent
					{
						gettingRemoteStatus = true;
						__COUTT__ << "(doDisconnected = " << doDisconnected
						          << ") gettingRemoteStatus = " << gettingRemoteStatus
						          << __E__;

						//check for commands first
						bool commandSent = false;

						if(!doDisconnected)  //only primary sends commands
							for(auto& remoteGatewayApp : remoteApps)
								if(remoteGatewayApp.command != "")
								{
									remoteAppsHandledByThread
									    .emplace(  //mark handled by this thread
									        remoteGatewayApp.appInfo.url +
									        remoteGatewayApp.appInfo.name);

									GatewaySupervisor::SendRemoteGatewayCommand(
									    remoteGatewayApp, remoteGatewaySocket);
									if(remoteGatewayApp.getError() == "")
									{
										remoteGatewayApp.ignoreStatusCount =
										    0;  //if non-zero, do not ask for status
										commandSent = true;
									}

									//give feedback immediately to user!!
									{
										__COUT__
										    << "remoteGatewayApp (doDisconnected="
										    << doDisconnected << ") "
										    << remoteGatewayApp.appInfo.name
										    << " error: " << remoteGatewayApp.getError()
										    << __E__;
										//lock for remainder of scope
										std::lock_guard<std::mutex> lock(
										    theSupervisor->remoteGatewayAppsMutex_);
										for(size_t i = 0;
										    i < theSupervisor->remoteGatewayApps_.size();
										    ++i)
											if(remoteGatewayApp.appInfo.name ==
											   theSupervisor->remoteGatewayApps_[i]
											       .appInfo.name)
											{
												theSupervisor->remoteGatewayApps_[i]
												    .copyError(remoteGatewayApp);
												break;
											}
									}
								}  //end primary command handling loop

						if(commandSent)
						{
							commandRemoteIdleCount = 0;  //reset
						}

						//then get status (primary and disconnected threads)
						bool allAppsAreIdle    = true;
						bool allApssAreUnknown = true;
						for(auto& remoteGatewayApp : remoteApps)
						{
							bool isRemoteAppDisconnected =
							    appLastStatusGood.find(remoteGatewayApp.appInfo.url +
							                           remoteGatewayApp.appInfo.name) !=
							        appLastStatusGood.end() &&
							    !appLastStatusGood.at(remoteGatewayApp.appInfo.url +
							                          remoteGatewayApp.appInfo.name);

							__COUTS__(TLVL_StatusWorkloop)
							    << "Status needed? doDisconnected = " << doDisconnected
							    << " Remote subapp = '" << remoteGatewayApp.appInfo.name
							    << "' [URL=" << remoteGatewayApp.appInfo.url
							    << "] isRemoteAppDisconnected = "
							    << isRemoteAppDisconnected << ".\n\n";

							//skip based on disconnected status
							bool skipApp = false;
							if(doDisconnected && !isRemoteAppDisconnected)
								skipApp = true;
							if(!doDisconnected && isRemoteAppDisconnected)
								skipApp = true;

							if(remoteAppsHandledByThread
							       .find(  //already handled by command send, so get status!
							           remoteGatewayApp.appInfo.url +
							           remoteGatewayApp.appInfo.name) !=
							   remoteAppsHandledByThread.end())
								skipApp = false;

							if(skipApp)
								continue;

							remoteAppsHandledByThread
							    .emplace(  //mark handled by this thread
							        remoteGatewayApp.appInfo.url +
							        remoteGatewayApp.appInfo.name);

							auto start = std::chrono::high_resolution_clock::now();
							__COUTS__(TLVL_StatusWorkloop)
							    << "Calling CheckRemoteGatewayStatus, doDisconnected = "
							    << doDisconnected << " Remote subapp = '"
							    << remoteGatewayApp.appInfo.name
							    << "' [URL=" << remoteGatewayApp.appInfo.url
							    << "] isRemoteAppDisconnected = "
							    << isRemoteAppDisconnected << ".\n\n";

							GatewaySupervisor::CheckRemoteGatewayStatus(
							    remoteGatewayApp,
							    remoteGatewaySocket,
							    ipAddressForStateChangesOverUDP,
							    portForReverseLoginOverUDP,
							    contextCommonList,
							    contextCommonOverrideList);

							{
								auto statusMs =
								    std::chrono::duration_cast<std::chrono::milliseconds>(
								        std::chrono::high_resolution_clock::now() - start)
								        .count();
								if(statusMs > 200)
									__COUTT__ << "CheckRemoteGatewayStatus for '"
									          << remoteGatewayApp.appInfo.name
									          << "' took " << statusMs << " ms" << __E__;
							}

							usleep(
							    50 *
							    1000 /*50ms inter-gateway stagger to avoid UDP buffer overflow*/);

							if(remoteGatewayApp.appInfo.status.size() &&
							   remoteGatewayApp.appInfo.status !=
							       SupervisorInfo::APP_STATUS_UNKNOWN)
							{
								allApssAreUnknown = false;
								if(!appLastStatusGood[remoteGatewayApp.appInfo.url +
								                      remoteGatewayApp.appInfo.name])
								{
									__COUT_INFO__
									    << "First good status received (doDisconnected = "
									    << doDisconnected << ") from Remote subapp = '"
									    << remoteGatewayApp.appInfo.name
									    << "' [URL=" << remoteGatewayApp.appInfo.url
									    << "]. Status: '" +
									           remoteGatewayApp.appInfo.status
									    << "'" << __E__;
								}
								appLastStatusGood[remoteGatewayApp.appInfo.url +
								                  remoteGatewayApp.appInfo.name] = true;
								{  //propagate status change to list of truth
									std::lock_guard<std::mutex> lock(
									    theSupervisor->dualStatusThreadMutex_);
									theSupervisor->appLastStatusGood_
									    [remoteGatewayApp.appInfo.url +
									     remoteGatewayApp.appInfo.name] = true;
								}

								if(!(remoteGatewayApp.appInfo.progress ==
								         0 ||  //if !(idle)
								     remoteGatewayApp.appInfo.progress == 100 ||
								     remoteGatewayApp.appInfo.status.find("Error") !=
								         std::string::
								             npos ||  //	case "Failed", "Error", "Soft-Error"
								     remoteGatewayApp.appInfo.status.find("Fail") !=
								         std::string::npos))
								{
									__COUTT__
									    << remoteGatewayApp.appInfo.name << " not idle: "
									    << remoteGatewayApp.appInfo.status
									    << " progress: "
									    << remoteGatewayApp.appInfo.progress << __E__;
									allAppsAreIdle = false;
								}
							}
							else  //status is unknown (make sure in disconnected pile)
							{
								//mark so could ignore/skip absent subsystems for a while
								remoteGatewayApp.ignoreStatusCount =
								    3;  //if non-zero, do not ask for status

								//alert on new failure of status retrieval
								if(appLastStatusGood.find(
								       remoteGatewayApp.appInfo.url +
								       remoteGatewayApp.appInfo.name) ==
								       appLastStatusGood.end() ||
								   appLastStatusGood[remoteGatewayApp.appInfo.url +
								                     remoteGatewayApp.appInfo.name])
								{
									//lookup context name (hostname)
									std::string contextName = "";
									for(const auto& it : theSupervisor->allSupervisorInfo_
									                         .getAllSupervisorInfo())
									{
										const auto& appInfo = it.second;

										if(remoteGatewayApp.appInfo.url ==
										   appInfo.getURL())
										{
											contextName = appInfo.getContextName();
											break;
										}
									}

									std::stringstream ss;
									ss << "New failure getting '"
									   << remoteGatewayApp.appInfo.name
									   << "' Remote Gateway App status [URL="
									   << remoteGatewayApp.appInfo.url << "].";
									if(contextName != "")
										ss << " (" << contextName << ")" << __E__;

									__COUT_WARN__
									    << "(doDisconnected = " << doDisconnected << ") "
									    << ss.str();
									if(appLastStatusGood.find(
									       remoteGatewayApp.appInfo.url +
									       remoteGatewayApp.appInfo.name) !=
									       appLastStatusGood.end() &&
									   //startup lull: suppress bad-status spam in the first 30 s
									   //while remote apps are still coming up.
									   time(0) - workloopStartTime > 30)
										theSupervisor->addSystemMessage("*", ss.str());
								}

								//mark last status bad
								appLastStatusGood[remoteGatewayApp.appInfo.url +
								                  remoteGatewayApp.appInfo.name] = false;
								{  //propagate status change to list of truth
									std::lock_guard<std::mutex> lock(
									    theSupervisor->dualStatusThreadMutex_);
									theSupervisor->appLastStatusGood_
									    [remoteGatewayApp.appInfo.url +
									     remoteGatewayApp.appInfo.name] = false;
								}
							}
							auto duration =
							    std::chrono::duration_cast<std::chrono::milliseconds>(
							        std::chrono::high_resolution_clock::now() - start)
							        .count();
							__COUTS__(TLVL_StatusRemoteWorkloop)
							    << "Time taken to calling CheckRemoteGatewayStatus, "
							       "doDisconnected = "
							    << doDisconnected << " Remote subapp = '"
							    << remoteGatewayApp.appInfo.name
							    << "' [URL=" << remoteGatewayApp.appInfo.url
							    << "] isRemoteAppDisconnected = "
							    << isRemoteAppDisconnected << " --> " << duration
							    << " milliseconds." << std::endl;

						}  //end remote app status update loop

						if(allApssAreUnknown)  //then remove ignore status, and give user feedback faster
						{
							for(auto& remoteGatewayApp : remoteApps)
								remoteGatewayApp.ignoreStatusCount =
								    0;  //if non-zero, do not ask for status
						}

						if(commandingRemoteGatewayApps && allAppsAreIdle)
						{
							++commandRemoteIdleCount;
							if(commandRemoteIdleCount >= 3)
							{
								__COUTT__ << "Back to idle statusing (doDisconnected = "
								          << doDisconnected << ")" << __E__;
								commandingRemoteGatewayApps = false;
							}
						}

						__COUTS__(TLVL_StatusRemoteWorkloop)
						    << "(doDisconnected = " << doDisconnected
						    << ") commandRemoteIdleCount=" << commandRemoteIdleCount
						    << " allAppsAreIdle=" << allAppsAreIdle
						    << " commandingRemoteGatewayApps="
						    << commandingRemoteGatewayApps << __E__;

					}  //end remote app status update

					//if possible, get remote icon list for desktop from each remote app
					//skip icon-gathering while a command cycle is active: GetRemoteGatewayIcons
					//is a blocking UDP call with a 10 s timeout per unreachable subsystem,
					//which can stall the status copy-back long enough that the SubsystemLaunch
					//UI poll loop (10 attempts x 2 s) gives up before the cached "Launching X"
					//flips to the real state.
					if(resetRemoteGatewayApps && !commandingRemoteGatewayApps)
					{
						__COUTS__(TLVL_RemoteDesktopIcons)
						    << "Attempting to get Remote Desktop Icons (doDisconnected = "
						    << doDisconnected << ")... size=" << remoteApps.size()
						    << __E__;

						for(auto& remoteGatewayApp : remoteApps)
						{
							__COUTVS__(TLVL_RemoteDesktopIcons,
							           remoteGatewayApp.appInfo.name);
							__COUTVS__(TLVL_RemoteDesktopIcons, remoteGatewayApp.command);

							if(remoteGatewayApp.command != "")
								continue;  //skip if command to be sent

							bool isRemoteAppDisconnected =
							    appLastStatusGood.find(remoteGatewayApp.appInfo.url +
							                           remoteGatewayApp.appInfo.name) !=
							        appLastStatusGood.end() &&
							    !appLastStatusGood.at(remoteGatewayApp.appInfo.url +
							                          remoteGatewayApp.appInfo.name);

							__COUTVS__(TLVL_RemoteDesktopIcons, isRemoteAppDisconnected);

							if(isRemoteAppDisconnected)
								continue;  //skip if no status (probably means subsystem is down, so icons would not be available)

							//clear any previous icon error
							if(remoteGatewayApp.getError().find("desktop icons") !=
							   std::string::npos)
							{
								__COUTV__(remoteGatewayApp.getError());
								//lock for remainder of scope
								std::lock_guard<std::mutex> lock(
								    theSupervisor->remoteGatewayAppsMutex_);
								for(size_t i = 0;
								    i < theSupervisor->remoteGatewayApps_.size();
								    ++i)
									if(remoteGatewayApp.appInfo.name ==
									   theSupervisor->remoteGatewayApps_[i].appInfo.name)
									{
										theSupervisor->remoteGatewayApps_[i].clearError();
										__COUTV__(theSupervisor->remoteGatewayApps_[i]
										              .getError());
										break;
									}
								remoteGatewayApp.clearError();
							}

							if(remoteGatewayApp.getError() ==
							   "")  //only request icons if no errors
							{
								//only sets iconString or error!
								GatewaySupervisor::GetRemoteGatewayIcons(
								    remoteGatewayApp, remoteGatewaySocket);

								if(remoteGatewayApp.getError() !=
								   "")  //give feedback immediately to user!!
								{
									__COUTV__(remoteGatewayApp.getError());
									//lock for remainder of scope
									std::lock_guard<std::mutex> lock(
									    theSupervisor->remoteGatewayAppsMutex_);
									for(size_t i = 0;
									    i < theSupervisor->remoteGatewayApps_.size();
									    ++i)
										if(remoteGatewayApp.appInfo.name ==
										   theSupervisor->remoteGatewayApps_[i]
										       .appInfo.name)
										{
											theSupervisor->remoteGatewayApps_[i]
											    .copyError(remoteGatewayApp);
											break;
										}
								}
								else  //give icon feedback immediately
								{
									__COUTVS__(TLVL_RemoteDesktopIcons,
									           remoteGatewayApp.iconString);
									//lock for remainder of scope
									std::lock_guard<std::mutex> lock(
									    theSupervisor->remoteGatewayAppsMutex_);
									for(size_t i = 0;
									    i < theSupervisor->remoteGatewayApps_.size();
									    ++i)
										if(remoteGatewayApp.appInfo.name ==
										   theSupervisor->remoteGatewayApps_[i]
										       .appInfo.name)
										{
											theSupervisor->remoteGatewayApps_[i]
											    .iconString = remoteGatewayApp.iconString;
											break;
										}
								}
							}  //end remote app icon request handling
						}      //end remote app icon request loop
					}          //end remote desktop icon gathering

					//for each remote gateway, copy info to Gateway supervisor remote gateway structure
					if(gettingRemoteStatus)
					{
						__COUTT__ << "(doDisconnected = " << doDisconnected
						          << ") copy over... gettingRemoteStatus = "
						          << gettingRemoteStatus << __E__;

						//replace info in supervisor remote gateway list
						{
							//lock for remainder of scope
							auto lockWaitStart =
							    std::chrono::high_resolution_clock::now();
							std::lock_guard<std::mutex> lock(
							    theSupervisor->remoteGatewayAppsMutex_);
							auto lockWaitMs =
							    std::chrono::duration_cast<std::chrono::milliseconds>(
							        std::chrono::high_resolution_clock::now() -
							        lockWaitStart)
							        .count();
							if(lockWaitMs > 50)
								__COUT_WARN__ << "AppStatusWorkLoop "
								                 "remoteGatewayAppsMutex_ lock wait = "
								              << lockWaitMs << " ms" << __E__;

							__COUTT__ << "(doDisconnected = " << doDisconnected
							          << ") size?... "
							             "theSupervisor->remoteGatewayApps_.size() = "
							          << theSupervisor->remoteGatewayApps_.size()
							          << __E__;

							//first clear any stale status info, if in correct thread role
							for(size_t i = 0;
							    !commandingRemoteGatewayApps &&
							    i < theSupervisor->remoteGatewayApps_.size();
							    ++i)
							{
								//only clear status if status was handled by this thread
								if(remoteAppsHandledByThread.find(
								       theSupervisor->remoteGatewayApps_[i].appInfo.url +
								       theSupervisor->remoteGatewayApps_[i]
								           .appInfo.name) ==
								   remoteAppsHandledByThread.end())
									continue;

								__COUTVS__(TLVL_StatusFullDetail,
								           theSupervisor->remoteGatewayApps_[i].command);
								if(theSupervisor->remoteGatewayApps_[i].command ==
								   "")  //make sure not mid-command
									theSupervisor->remoteGatewayApps_[i].appInfo.status =
									    "";  //clear status as indicator to be erased
							}                //end clear stale status loop

							//now copy over updated status info, if in correct thread role
							for(auto& remoteGatewayApp : remoteApps)
							{
								//only copy status if status was handled by this thread
								if(remoteAppsHandledByThread.find(
								       remoteGatewayApp.appInfo.url +
								       remoteGatewayApp.appInfo.name) ==
								   remoteAppsHandledByThread.end())
									continue;

								bool found = false;
								for(size_t i = 0;
								    i < theSupervisor->remoteGatewayApps_.size();
								    ++i)
								{
									if(remoteGatewayApp.appInfo.name ==
									   theSupervisor->remoteGatewayApps_[i].appInfo.name)
									{
										found = true;

										//copy over updated status (but not control info, which may be have been changed while mutex was dropped)

										// clang-format off
										if(remoteGatewayApp.command ==
										   "")  //if there is action on command, then error is being set (request()) or cleared (send) somewhere else
											theSupervisor->remoteGatewayApps_[i].copyError(remoteGatewayApp);

										theSupervisor->remoteGatewayApps_[i].ignoreStatusCount 			= remoteGatewayApp.ignoreStatusCount;
										theSupervisor->remoteGatewayApps_[i].consoleErrCount 			= remoteGatewayApp.consoleErrCount;
										theSupervisor->remoteGatewayApps_[i].consoleWarnCount 			= remoteGatewayApp.consoleWarnCount;

										theSupervisor->remoteGatewayApps_[i].usernameWithLock 			= remoteGatewayApp.usernameWithLock;

										theSupervisor->remoteGatewayApps_[i].config_dump 				= remoteGatewayApp.config_dump;

										// do not overwrite icon string here (it's updated immediately above)!
										// theSupervisor->remoteGatewayApps_[i].iconString 					= remoteGatewayApp.iconString;

										// do not overwrite refresh info here that was updated immediately above!
										// theSupervisor->remoteGatewayApps_[i].config_aliases 				= remoteGatewayApp.config_aliases;
										// theSupervisor->remoteGatewayApps_[i].user_data_path_record 		= remoteGatewayApp.user_data_path_record;
										// theSupervisor->remoteGatewayApps_[i].parentIconFolderPath 		= remoteGatewayApp.parentIconFolderPath;
										// theSupervisor->remoteGatewayApps_[i].permissionThresholdString 	= remoteGatewayApp.permissionThresholdString;
										// theSupervisor->remoteGatewayApps_[i].landingPage 				= remoteGatewayApp.landingPage;
										// theSupervisor->remoteGatewayApps_[i].setupType 					= remoteGatewayApp.setupType;
										// theSupervisor->remoteGatewayApps_[i].fullName 					= remoteGatewayApp.fullName;
										// theSupervisor->remoteGatewayApps_[i].instancePath 				= remoteGatewayApp.instancePath;
										// theSupervisor->remoteGatewayApps_[i].instanceHost 				= remoteGatewayApp.instanceHost;
										// theSupervisor->remoteGatewayApps_[i].instanceUser 				= remoteGatewayApp.instanceUser;

										//fix selected_config_alias
										//	if invalid selected_config_alias, reinitialize for user
										if(theSupervisor->remoteGatewayApps_[i].config_aliases.size() &&
											theSupervisor->remoteGatewayApps_[i].config_aliases.find(
											theSupervisor->remoteGatewayApps_[i].selected_config_alias) ==
												theSupervisor->remoteGatewayApps_[i].config_aliases.end())
										{
											__COUT__ << "Resetting invalid selected_config_alias '"
											         << theSupervisor->remoteGatewayApps_[i]
											                .selected_config_alias
											         << "' for Remote Gateway App '"
											         << theSupervisor->remoteGatewayApps_[i]
											                .appInfo.name
											         << "' to first available config_alias."
											         << __E__;
											theSupervisor->remoteGatewayApps_[i].selected_config_alias =
												theSupervisor->remoteGatewayApps_[i].config_aliases.size() ?
													(*theSupervisor->remoteGatewayApps_[i].config_aliases.begin()) : "";
										}

										// clang-format on

										__COUTTV__(theSupervisor->remoteGatewayApps_[i]
										               .selected_config_alias);
										__COUTTV__(StringMacros::setToString(
										    theSupervisor->remoteGatewayApps_[i]
										        .config_aliases));

										__COUTT__
										    << remoteGatewayApp.appInfo.name
										    << " -- Command: " << remoteGatewayApp.command
										    << " Command-old: "
										    << theSupervisor->remoteGatewayApps_[i]
										           .command
										    << " Status: "
										    << remoteGatewayApp.appInfo.status
										    << " Status_old: "
										    << theSupervisor->remoteGatewayApps_[i]
										           .appInfo.status
										    << " Error: " << remoteGatewayApp.getError()
										    << " progress: "
										    << remoteGatewayApp.appInfo.progress << __E__;

										bool justCompletedSend =
										    (remoteGatewayApp.command == "Sent");
										if(justCompletedSend)  //apply command clear
										{
											theSupervisor->remoteGatewayApps_[i].command =
											    "";
											//also clear the forced "Launching X" placeholder
											//(set by broadcastMessageToRemoteGateways() or the
											//setRemoteSubsystemCommand handler) so it can't
											//re-arm the stale-status guard below against a
											//fresh response whose Done we already received.
											if(theSupervisor->remoteGatewayApps_[i]
											       .appInfo.status.find("Launching") == 0)
												theSupervisor->remoteGatewayApps_[i]
												    .appInfo.status = "";
										}

										if(theSupervisor->remoteGatewayApps_[i].command !=
										       "" ||
										   (commandingRemoteGatewayApps &&
										    !justCompletedSend &&  //trust fresh status for the app whose Done we just got
										    theSupervisor->remoteGatewayApps_[i]
										            .appInfo.status.find("Launching") ==
										        0 &&
										    remoteGatewayApp.appInfo.progress ==
										        100))  //dont trust done progress while still 'commanding'
											__COUT__ << "Ignoring '"
											         << remoteGatewayApp.appInfo.name
											         << "' assumed stale status: "
											         << remoteGatewayApp.appInfo.status
											         << __E__;
										else
											theSupervisor->remoteGatewayApps_[i].appInfo =
											    remoteGatewayApp.appInfo;

										theSupervisor->remoteGatewayApps_[i].subapps =
										    remoteGatewayApp.subapps;
										break;
									}
								}
								if(!found)  //add
								{
									__COUT__ << "Adding '"
									         << remoteGatewayApp.appInfo.name
									         << "' to Gateway app list." << __E__;
									theSupervisor->remoteGatewayApps_.push_back(
									    remoteGatewayApp);
								}
							}  //end copy over updated status info, if in correct thread role

							__COUTT__ << "(doDisconnected = " << doDisconnected
							          << ") done copy over... "
							             "theSupervisor->remoteGatewayApps_.size() = "
							          << theSupervisor->remoteGatewayApps_.size()
							          << __E__;
							//cleanup unused remoteGatewayApps_
							for(size_t i = 0;
							    i < theSupervisor->remoteGatewayApps_.size();
							    ++i)
							{
								//only delete if status was handled by this thread
								if(remoteAppsHandledByThread.find(
								       theSupervisor->remoteGatewayApps_[i].appInfo.url +
								       theSupervisor->remoteGatewayApps_[i]
								           .appInfo.name) ==
								   remoteAppsHandledByThread.end())
									continue;

								if(theSupervisor->remoteGatewayApps_[i].appInfo.status ==
								   "")
								{
									__COUT__ << "(doDisconnected = " << doDisconnected
									         << ") Erasing stale '"
									         << theSupervisor->remoteGatewayApps_[i]
									                    .appInfo.url +
									                theSupervisor->remoteGatewayApps_[i]
									                    .appInfo.name
									         << "' from Gateway app list." << __E__;
									//rewind and erase
									theSupervisor->remoteGatewayApps_.erase(
									    theSupervisor->remoteGatewayApps_.begin() + i);
									--i;
								}
							}

							//copy to subapps for display of primary Gateway
							for(const auto& remoteGatewayApp :
							    theSupervisor->remoteGatewayApps_)
								subapps.push_back(remoteGatewayApp.appInfo);

							__COUTT__ << "(doDisconnected = " << doDisconnected
							          << ") done copy over... "
							             "theSupervisor->remoteGatewayApps_.size() = "
							          << theSupervisor->remoteGatewayApps_.size()
							          << __E__;

						}  //end scope lock for copying over remote app status

					}  //end handling of copy info to Gateway supervisor remote gateway structure

				}  //end main status try
				catch(const std::runtime_error& e)
				{
					status                = SupervisorInfo::APP_STATUS_UNKNOWN;
					progress              = "0";
					detail                = "SOAP Message Error";
					oneStatusReqHasFailed = true;

					__COUT__ << "Failed getting status from Gateway"
					         << " Supervisor instance = '" << appName
					         << "' [LID=" << appInfo.getId() << "] in Context '"
					         << appInfo.getContextName() << "' [URL=" << appInfo.getURL()
					         << "].\n\n";
					__COUT_WARN__ << "Failed to retrieve Gateway Supervisor status. Here "
					                 "is the error: "
					              << e.what() << __E__;
				}
				catch(...)
				{
					status                = SupervisorInfo::APP_STATUS_UNKNOWN;
					progress              = "0";
					detail                = "Unknown SOAP Message Error";
					oneStatusReqHasFailed = true;

					__COUT__ << "Failed getting status from Gateway "
					         << " Supervisor instance = '" << appName
					         << "' [LID=" << appInfo.getId() << "] in Context '"
					         << appInfo.getContextName() << "' [URL=" << appInfo.getURL()
					         << "].\n\n";
					__COUT_WARN__ << "Failed to retrieve Gateway Supervisor status to "
					                 "unknown error."
					              << __E__;
				}

				//disconnected thread only handles remote gateway apps, do not proceed with setting Gateway Supervisor app status
				if(doDisconnected)
					continue;
			}
			else  // get non-gateway status
			{
				//skip based on disconnected status
				if(doDisconnected && !isDisconnected)
					continue;
				if(!doDisconnected && isDisconnected)
					continue;

				// pass the application as a parameter to tempMessage
				SOAPParameters appPointer;
				appPointer.addParameter("ApplicationPointer");

				xoap::MessageReference tempMessage =
				    SOAPUtilities::makeSOAPMessageReference("ApplicationStatusRequest");

				__COUTS__(TLVL_StatusWorkloop)
				    << "tempMessage... " << SOAPUtilities::translate(tempMessage)
				    << std::endl;

				try
				{
					xoap::MessageReference statusMessage =
					    theSupervisor->sendWithSOAPReply(appInfo.getDescriptor(),
					                                     tempMessage);

					if("ContextARTDAQ" == appInfo.getContextName())
						__COUTS__(TLVL_DebugArtdaqStatus)
						    << " Supervisor instance = '" << appName
						    << "' [LID=" << appInfo.getId() << "] in Context '"
						    << appInfo.getContextName() << " statusMessage... "
						    << SOAPUtilities::translate(statusMessage) << std::endl;
					else
						__COUTS__(TLVL_DebugStatusWorkloop)
						    << " Supervisor instance = '" << appName
						    << "' [LID=" << appInfo.getId() << "] in Context '"
						    << appInfo.getContextName() << " statusMessage... "
						    << SOAPUtilities::translate(statusMessage) << std::endl;

					SOAPParameters parameters;
					parameters.addParameter("Status");
					parameters.addParameter("Progress");
					parameters.addParameter("Detail");
					parameters.addParameter("Subapps");
					parameters.addParameter("AvailableLogSpaceKB");
					parameters.addParameter("AvailableDataSpaceKB");
					SOAPUtilities::receive(statusMessage, parameters);

					status = parameters.getValue("Status");
					if(status.empty())
						status = SupervisorInfo::APP_STATUS_UNKNOWN;

					progress = parameters.getValue("Progress");
					if(progress.empty())
						progress = "100";

					detail = parameters.getValue("Detail");
					if(appInfo.isTypeConsoleSupervisor())
					{
						//parse detail

						//Note: do not printout detail, because custom counts will fire recursively
						// std::cout << __COUT_HDR__ << (detail);

						//Console Supervisor status detatil format is (from otsdaq-utilities/otsdaq-utilities/Console/ConsoleSupervisor.cc:1722):
						//	uptime, Err count, Warn count, Last Error msg, Last Warn msg
						std::vector<std::string> parseDetail =
						    StringMacros::getVectorFromString(detail, {','});

						__COUTVS__(TLVL_StatusFullDetail, detail);
						__COUTVS__(TLVL_StatusFullDetail, parseDetail.size());
						__COUTVS__(TLVL_StatusFullDetail,
						           StringMacros::vectorToString(parseDetail));

						std::lock_guard<std::mutex> lock(
						    theSupervisor->systemStatusMutex_);  //lock for rest of scope
						if(parseDetail.size() > 1)
							theSupervisor->systemConsoleErrCount_ =
							    atoi(parseDetail[1]
							             .substr(parseDetail[1].find(':') + 1)
							             .c_str());
						if(parseDetail.size() > 2)
							theSupervisor->systemConsoleWarnCount_ =
							    atoi(parseDetail[2]
							             .substr(parseDetail[2].find(':') + 1)
							             .c_str());
						__COUTVS__(TLVL_DebugStatusDetail,
						           theSupervisor->systemConsoleErrCount_);
						__COUTVS__(TLVL_DebugStatusDetail,
						           theSupervisor->systemConsoleWarnCount_);
						if(parseDetail.size() >
						   3)  //e.g. Last Err (Mon Sep 30 14:38:20 2024 CDT): Remote%20lo
						{
							size_t closeTimePos = parseDetail[3].find(')');
							__COUTVS__(TLVL_DebugStatusDetail, closeTimePos);
							theSupervisor->lastConsoleErr_ =
							    parseDetail[3].substr(closeTimePos + 2);
							size_t openTimePos = parseDetail[3].find('(');
							__COUTVS__(TLVL_DebugStatusDetail, openTimePos);
							theSupervisor->lastConsoleErrTime_ = parseDetail[3].substr(
							    openTimePos, closeTimePos - openTimePos + 1);
							__COUTVS__(TLVL_DebugStatusDetail,
							           theSupervisor->lastConsoleErrTime_);
						}
						if(parseDetail.size() >
						   4)  //e.g. Last Warn (Mon Sep 30 14:38:20 2024 CDT): Remote%20lo
						{
							size_t closeTimePos = parseDetail[4].find(')');
							theSupervisor->lastConsoleWarn_ =
							    parseDetail[4].substr(closeTimePos + 2);
							size_t openTimePos = parseDetail[4].find('(');
							theSupervisor->lastConsoleWarnTime_ = parseDetail[4].substr(
							    openTimePos, closeTimePos - openTimePos + 1);
							__COUTVS__(TLVL_DebugStatusDetail,
							           theSupervisor->lastConsoleWarnTime_);
						}
						if(parseDetail.size() >
						   5)  //e.g. Last Info (Mon Sep 30 14:38:20 2024 CDT): Remote%20lo
						{
							size_t closeTimePos = parseDetail[5].find(')');
							theSupervisor->lastConsoleInfo_ =
							    parseDetail[5].substr(closeTimePos + 2);
							size_t openTimePos = parseDetail[5].find('(');
							theSupervisor->lastConsoleInfoTime_ = parseDetail[5].substr(
							    openTimePos, closeTimePos - openTimePos + 1);
							__COUTVS__(TLVL_DebugStatusDetail,
							           theSupervisor->lastConsoleInfoTime_);
						}
						if(parseDetail.size() > 6)
							theSupervisor->systemConsoleInfoCount_ =
							    atoi(parseDetail[6]
							             .substr(parseDetail[6].find(':') + 1)
							             .c_str());

						if(parseDetail.size() >
						   7)  //e.g. First Err (Mon Sep 30 14:38:20 2024 CDT): Remote%20lo
						{
							size_t closeTimePos = parseDetail[7].find(')');
							theSupervisor->firstConsoleErr_ =
							    parseDetail[7].substr(closeTimePos + 2);
							size_t openTimePos = parseDetail[7].find('(');
							theSupervisor->firstConsoleErrTime_ = parseDetail[7].substr(
							    openTimePos, closeTimePos - openTimePos + 1);
							__COUTVS__(TLVL_DebugStatusDetail,
							           theSupervisor->firstConsoleErrTime_);
						}
						if(parseDetail.size() >
						   8)  //e.g. First Warn (Mon Sep 30 14:38:20 2024 CDT): Remote%20lo
						{
							size_t closeTimePos = parseDetail[8].find(')');
							theSupervisor->firstConsoleWarn_ =
							    parseDetail[8].substr(closeTimePos + 2);
							size_t openTimePos = parseDetail[8].find('(');
							theSupervisor->firstConsoleWarnTime_ = parseDetail[8].substr(
							    openTimePos, closeTimePos - openTimePos + 1);
							__COUTVS__(TLVL_DebugStatusDetail,
							           theSupervisor->firstConsoleWarnTime_);
						}
						if(parseDetail.size() >
						   9)  //e.g. First Info (Mon Sep 30 14:38:20 2024 CDT): Remote%20lo
						{
							size_t closeTimePos = parseDetail[9].find(')');
							theSupervisor->firstConsoleInfo_ =
							    parseDetail[9].substr(closeTimePos + 2);
							size_t openTimePos = parseDetail[9].find('(');
							theSupervisor->firstConsoleInfoTime_ = parseDetail[9].substr(
							    openTimePos, closeTimePos - openTimePos + 1);
							__COUTVS__(TLVL_DebugStatusDetail,
							           theSupervisor->firstConsoleInfoTime_);
						}
					}

					subapps = SupervisorInfo::deserializeSubappInfos(
					    parameters.getValue("Subapps"));

					value = parameters.getValue("AvailableLogSpaceKB");
					if(!value.size())
						availableLogSpaceKB = 0;
					else
						availableLogSpaceKB = std::stoull(value);
					__COUTVS__(TLVL_DebugStatusDetail, availableLogSpaceKB);
					value = parameters.getValue("AvailableDataSpaceKB");
					if(!value.size())
						availableDataSpaceKB = 0;
					else
						availableDataSpaceKB = std::stoull(value);
					__COUTVS__(TLVL_DebugStatusDetail, availableDataSpaceKB);

					if(!appLastStatusGood[appName])
					{
						__COUT_INFO__ << "First good status from "
						              << " Supervisor instance = '" << appName
						              << "' [LID=" << appInfo.getId() << "] in Context '"
						              << appInfo.getContextName()
						              << "' [URL=" << appInfo.getURL() << "].\n\n";
						__COUTTV__(SOAPUtilities::translate(tempMessage));
					}
					appLastStatusGood[appName] = true;
					{  //propagate status change to list of truth
						std::lock_guard<std::mutex> lock(
						    theSupervisor->dualStatusThreadMutex_);
						theSupervisor->appLastStatusGood_[appName] = true;
					}
				}
				catch(const xdaq::exception::Exception& e)
				{
					status                = SupervisorInfo::APP_STATUS_UNKNOWN;
					progress              = "0";
					detail                = "SOAP Message Error";
					oneStatusReqHasFailed = true;
					if(firstError)  // first error, give some more time for apps to boot
					{
						firstError = false;
						break;
					}

					if(appLastStatusGood[appName])
					{
						std::stringstream errSs;
						errSs << "Failed getting status from "
						      << " Supervisor instance = '" << appName
						      << "' [LID=" << appInfo.getId() << "] in Context '"
						      << appInfo.getContextName() << "' [URL=" << appInfo.getURL()
						      << "].\n\n";
						__COUT_WARN__ << errSs.str();
						//startup lull: suppress bad-status spam in the first 30 s
						//while supervisor apps are still coming up.
						if(time(0) - workloopStartTime > 30)
							theSupervisor->addSystemMessage("*", errSs.str());

						__COUTTV__(SOAPUtilities::translate(tempMessage));
						__COUT_WARN__ << "Failed to send getStatus SOAP Message - will "
						                 "suppress repeat errors: "
						              << e.what() << __E__;
					}     // else quiet repeat error messages
					else  //check if should throw state machine error
					{
						bool shouldTriggerError = false;
						{
							std::lock_guard<std::mutex> lock(
							    theSupervisor->stateMachineAccessMutex_);

							std::string currentState =
							    theSupervisor->theStateMachine_.getCurrentStateName();
							if(currentState !=
							       RunControlStateMachine::FAILED_STATE_NAME &&
							   (currentState !=
							        RunControlStateMachine::HALTED_STATE_NAME ||
							    theSupervisor->theStateMachine_.isInTransition()) &&
							   currentState !=
							       RunControlStateMachine::SHUTDOWN_STATE_NAME &&
							   (currentState !=
							        RunControlStateMachine::INITIAL_STATE_NAME ||
							    theSupervisor->theStateMachine_.isInTransition()))
							{
								__COUTV__(currentState);
								__SS__ << "\nDid a supervisor crash? Failed getting "
								          "status from "
								       << " Supervisor instance = '" << appName
								       << "' [LID=" << appInfo.getId() << "] in Context '"
								       << appInfo.getContextName()
								       << "' [URL=" << appInfo.getURL() << "]." << __E__;
								__COUT_ERR__ << "\n" << ss.str();

								if(!appInfo.isTypeConsoleSupervisor())
								{
									__COUT_WARN__
									    << "Unexpected failure getting status from "
									    << " Supervisor instance = '" << appName
									    << "' [LID=" << appInfo.getId()
									    << "] in Context '" << appInfo.getContextName()
									    << "' [URL=" << appInfo.getURL()
									    << "]. Attempting to send 'Error' "
									       "transition to target now!"
									    << __E__;
									if(theSupervisor->theStateMachine_.getErrorMessage()
									       .find(ss.str()) == std::string::npos)
										theSupervisor->theStateMachine_.setErrorMessage(
										    ss.str());
									shouldTriggerError = true;
								}
								else
									__COUT__ << "Ignoring that Console type supervisor "
									            "crashed."
									         << __E__;
							}
						}  // mutex released here — do not hold during broadcast
						if(shouldTriggerError)
						{
							std::lock_guard<std::mutex> lock(
							    theSupervisor->stateMachineAccessMutex_);
							try
							{
								theSupervisor->runControlMessageHandler(
								    SOAPUtilities::makeSOAPMessageReference(
								        RunControlStateMachine::ERROR_TRANSITION_NAME));
							}
							catch(...)
							{
							}       //ignore any errors
							break;  //only send one Error, then restart status loop
						}
					}
					appLastStatusGood[appName] = false;
					{  //propagate status change to list of truth
						std::lock_guard<std::mutex> lock(
						    theSupervisor->dualStatusThreadMutex_);
						theSupervisor->appLastStatusGood_[appName] = false;
					}
				}
				catch(...)
				{
					try
					{
						throw;
					}  //one more try to printout extra info
					catch(const std::runtime_error& e)
					{
						__COUT_ERR__ << "Exception of type runtime_error message: "
						             << e.what();
					}
					catch(const std::exception& e)
					{
						__COUT_ERR__ << "Exception of type " << typeid(e).name()
						             << " message: " << e.what();
					}
					catch(...)
					{
					}

					status                = SupervisorInfo::APP_STATUS_UNKNOWN;
					progress              = "0";
					detail                = "Unknown SOAP Message Error";
					oneStatusReqHasFailed = true;
					if(firstError)  // first error, give some more time for apps to boot
					{
						firstError = false;
						break;
					}
					if(appLastStatusGood[appName])
					{
						__COUT__ << "Failed getting status from "
						         << " Supervisor instance = '" << appName
						         << "' [LID=" << appInfo.getId() << "] in Context '"
						         << appInfo.getContextName()
						         << "' [URL=" << appInfo.getURL() << "].\n\n";
						__COUTV__(SOAPUtilities::translate(tempMessage));
						__COUT_WARN__ << "Failed to send getStatus SOAP Message due to "
						                 "unknown error. Will suppress repeat errors "
						                 "from Supervisor instance = '"
						              << appName << "' [LID=" << appInfo.getId()
						              << "] in Context '" << appInfo.getContextName()
						              << "' [URL=" << appInfo.getURL() << "]." << __E__;
					}     // else quiet repeat error messages
					else  //check if should throw state machine error
					{
						bool shouldTriggerError2 = false;
						{
							std::lock_guard<std::mutex> lock(
							    theSupervisor->stateMachineAccessMutex_);

							std::string currentState =
							    theSupervisor->theStateMachine_.getCurrentStateName();
							if(currentState !=
							       RunControlStateMachine::FAILED_STATE_NAME &&
							   currentState !=
							       RunControlStateMachine::HALTED_STATE_NAME &&
							   currentState != RunControlStateMachine::INITIAL_STATE_NAME)
							{
								__SS__
								    << "\nDid a supervisor crash? Failed getting Status "
								    << " Supervisor instance = '" << appName
								    << "' [LID=" << appInfo.getId() << "] in Context '"
								    << appInfo.getContextName()
								    << "' [URL=" << appInfo.getURL() << "]." << __E__;
								__COUT_ERR__ << "\n" << ss.str();

								if(theSupervisor->theStateMachine_.getErrorMessage().find(
								       ss.str()) == std::string::npos)
									theSupervisor->theStateMachine_.setErrorMessage(
									    ss.str());
								shouldTriggerError2 = true;
							}
						}  // mutex released here — do not hold during broadcast
						if(shouldTriggerError2)
						{
							std::lock_guard<std::mutex> lock(
							    theSupervisor->stateMachineAccessMutex_);
							try
							{
								theSupervisor->runControlMessageHandler(
								    SOAPUtilities::makeSOAPMessageReference(
								        RunControlStateMachine::ERROR_TRANSITION_NAME));
							}
							catch(...)
							{
							}       //ignore any errors
							break;  //only send one Error, then restart status loop
						}
					}
					appLastStatusGood[appName] = false;
					{  //propagate status change to list of truth
						std::lock_guard<std::mutex> lock(
						    theSupervisor->dualStatusThreadMutex_);
						theSupervisor->appLastStatusGood_[appName] = false;
					}
				}
			}  // end with non-gateway status request handling

			__COUTVS__(TLVL_StatusRemoteWorkloop, status);
			__COUTVS__(TLVL_StatusRemoteWorkloop, progress);

			if(progress.empty())
			{
				__SS__ << "Empty progress string should not happen (doDisconnected = "
				       << doDisconnected << ")! Supervisor instance = '" << appName
				       << "' [LID=" << appInfo.getId() << "] in Context '"
				       << appInfo.getContextName() << __E__;
				__SS_THROW__;
			}

			// set status and progress
			// convert the progress string into an integer in order to call
			// appInfo.setProgress() function
			std::istringstream ssProgress(progress);
			ssProgress >> progressInteger;

			if("ContextARTDAQ" == appInfo.getContextName())
				__COUTVS__(TLVL_DebugArtdaqStatus, progressInteger);
			else
			{
				__COUTVS__(TLVL_DebugStatusWorkloop, progressInteger);
				if(progressInteger > 100)
					__COUT__ << "What happened? " << progressInteger << __E__;
			}

			__COUTVS__(TLVL_DebugStatusWorkloop, availableLogSpaceKB);
			__COUTVS__(TLVL_DebugStatusWorkloop, availableDataSpaceKB);

			//alert and record available disk space
			auto spaceIt = availableDiskSpaceKB_map.find(appInfo.getContextName());
			const time_t hardLowSilenceSecs = 5 * 60;  //rate-limit hard-low alarms
			const time_t nowForHardLow      = time(0);
			if(availableLogSpaceKB)  //if non-zero, then assume is latest valid value
			{
				if((spaceIt == availableDiskSpaceKB_map.end() ||  //and new value
				    spaceIt->second.first > availableLogSpaceKB) &&
				   availableLogSpaceKB < availableLogSpaceKB_MIN)  //and below threshold
				{
					//rate-limit: do not fire if we already alerted recently for this context
					auto lastIt = hardLowLogAlert_map.find(appInfo.getContextName());
					if(lastIt == hardLowLogAlert_map.end() ||
					   nowForHardLow - lastIt->second > hardLowSilenceSecs)
					{
						theSupervisor->addSystemMessage(
						    "*",
						    "LOG disk space low (at host='" + appInfo.getHostname() +
						        "' and path='" + otsdaq_log_dir +
						        "/'): " + std::to_string(availableLogSpaceKB / 1024) +
						        " MB remaining.");
						hardLowLogAlert_map[appInfo.getContextName()] = nowForHardLow;
					}
				}
				availableDiskSpaceKB_map[appInfo.getContextName()].first =
				    availableLogSpaceKB;
			}
			else if(spaceIt !=
			        availableDiskSpaceKB_map.end())  //else use last known value
				availableLogSpaceKB = spaceIt->second.first;

			if(availableDataSpaceKB)  //if non-zero, then assume is latest valid value
			{
				if((spaceIt == availableDiskSpaceKB_map.end() ||  //and new value
				    spaceIt->second.second > availableDataSpaceKB) &&
				   availableDataSpaceKB < availableDataSpaceKB_MIN)  //and below threshold
				{
					auto lastIt = hardLowDataAlert_map.find(appInfo.getContextName());
					if(lastIt == hardLowDataAlert_map.end() ||
					   nowForHardLow - lastIt->second > hardLowSilenceSecs)
					{
						theSupervisor->addSystemMessage(
						    "*",
						    "DATA disk space low (at host='" + appInfo.getHostname() +
						        "' and path='" + otsdaq_data_dir +
						        "/'): " + std::to_string(availableDataSpaceKB / 1024) +
						        " MB remaining.");
						hardLowDataAlert_map[appInfo.getContextName()] = nowForHardLow;
					}
				}
				availableDiskSpaceKB_map[appInfo.getContextName()].second =
				    availableDataSpaceKB;
			}
			else if(spaceIt !=
			        availableDiskSpaceKB_map.end())  //else use last known value
				availableDataSpaceKB = spaceIt->second.second;

			__COUTVS__(TLVL_DebugStatusWorkloop, availableLogSpaceKB);
			__COUTVS__(TLVL_DebugStatusWorkloop, availableDataSpaceKB);

			theSupervisor->allSupervisorInfo_
			    .setSupervisorStatus(  //====================================== set supervisor status
			        appInfo,
			        status,
			        progressInteger,
			        detail,
			        subapps,
			        availableLogSpaceKB,
			        availableDataSpaceKB);

			//if no recent alert, check if rate to disk is too high ------------
			// Windows go from longest-and-quietest to shortest-and-loudest. A
			// single shared timestamp per disk means the first window to fire
			// silences all the others in this pass — so flooding all 4 alerts
			// at once is impossible. Shorter windows still get more chances
			// because their silence periods (below) are shorter.
			//
			// Three protections against false alarms:
			//   (1) WARMUP: skip all rate checks for the first 5 min after the
			//       workloop starts, so the seed sample (captured during the noisy
			//       startup burst) has time to age out of the historical deque.
			//   (2) MIN LOOKBACK: each window requires its historical sample to
			//       actually be at least N seconds old before its rate is trusted
			//       (otherwise a very young sample produces a misleading rate
			//       that gets multiplied by a much larger projection window).
			//   (3) SUSTAINED TRIP: a trip condition must be observed continuously
			//       for ~30 s before we fire, so a brief transient (a one-off log
			//       flush, file rotation) is filtered out.
			time_t       now                = time(0);
			const time_t warmupSecs         = 5 * 60;  //5-minute startup grace
			const time_t sustainSecs        = 30;      //must trip for this long
			const size_t slotForWindow[4]   = {9, 7, 5, 1};
			const time_t minLookbackSecs[4] = {300, 300, 300, 60};  //5,5,5,1 min
			const int    windowSecs[4]      = {3600, 1800, 900, 450};
			const int    silenceSecs[4]     = {
                30 * 60, 15 * 60, 15 * 30, 15 * 15};  //30, 15, 7.5, 3.75 minutes
			const char* const windowLabels[4] = {
			    "last hour", "last half-hour", "last quarter-hour", "last few minutes"};

			const bool inWarmup = (now - workloopStartTime) < warmupSecs;

			const auto& info =
			    theSupervisor->allSupervisorInfo_.getAllSupervisorInfo().at(
			        appInfo.getId());
			const float  logRates[4]  = {info.getLogUsageRateLastHourKBps(),
			                             info.getLogUsageRateLastHalfHourKBps(),
			                             info.getLogUsageRateLastQuarterHourKBps(),
			                             info.getLogUsageRateNowKBps()};
			const float  dataRates[4] = {info.getDataUsageRateLastHourKBps(),
			                             info.getDataUsageRateLastHalfHourKBps(),
			                             info.getDataUsageRateLastQuarterHourKBps(),
			                             info.getDataUsageRateNowKBps()};
			const time_t logSpans[4]  = {
                info.getLogSpaceSampleAgeSeconds(slotForWindow[0]),
                info.getLogSpaceSampleAgeSeconds(slotForWindow[1]),
                info.getLogSpaceSampleAgeSeconds(slotForWindow[2]),
                info.getLogSpaceSampleAgeSeconds(slotForWindow[3])};
			const time_t dataSpans[4] = {
			    info.getDataSpaceSampleAgeSeconds(slotForWindow[0]),
			    info.getDataSpaceSampleAgeSeconds(slotForWindow[1]),
			    info.getDataSpaceSampleAgeSeconds(slotForWindow[2]),
			    info.getDataSpaceSampleAgeSeconds(slotForWindow[3])};

			auto checkDiskRateAlerts = [&](const std::string& diskTag,   //"LOG" or "DATA"
			                               const std::string& diskWord,  //"log" or "data"
			                               const std::string& diskPath,
			                               int64_t            availableSpaceKB,
			                               int64_t            availableSpaceKB_MIN,
			                               const float(&rates)[4],
			                               const time_t(&spans)[4],
			                               std::map<std::string, time_t>& alertMap,
			                               std::map<std::string, time_t>& firstTripMap) {
				const std::string& ctx = appInfo.getContextName();
				if(inWarmup || !availableSpaceKB)
				{
					firstTripMap.erase(ctx);
					return;
				}

				//find the first (longest) window that meets criteria and is in a trip
				int trippedW = -1;
				for(int w = 0; w < 4; ++w)
				{
					auto it = alertMap.find(ctx);
					if(it != alertMap.end() && now - it->second <= silenceSecs[w])
						continue;  //still inside this window's silence period
					if(spans[w] < minLookbackSecs[w])
						continue;  //not enough real lookback to trust this rate
					if(availableSpaceKB - rates[w] * windowSecs[w] >=
					   availableSpaceKB_MIN)
						continue;  //projection stays above MIN — no trip
					trippedW = w;
					break;
				}

				if(trippedW < 0)
				{
					//no trip this pass — reset the sustained-trip clock
					firstTripMap.erase(ctx);
					return;
				}

				//trip seen — require it to persist for sustainSecs before firing
				auto firstIt = firstTripMap.find(ctx);
				if(firstIt == firstTripMap.end())
				{
					firstTripMap[ctx] = now;
					return;
				}
				if(now - firstIt->second < sustainSecs)
					return;  //not sustained long enough yet

				theSupervisor->addSystemMessage(
				    "*",
				    diskTag + " disk space low ALARM (at host='" + appInfo.getHostname() +
				        "' and path='" + diskPath + "/'): " +
				        std::to_string(availableSpaceKB / 1024) + " MB remaining and " +
				        diskWord + " usage rate over " + windowLabels[trippedW] + " is " +
				        formatRateKBps(rates[trippedW]) + ".");
				alertMap[ctx] = now;
				firstTripMap.erase(ctx);  //re-arm: next trip must sustain again
			};

			checkDiskRateAlerts("LOG",
			                    "log",
			                    otsdaq_log_dir,
			                    availableLogSpaceKB,
			                    availableLogSpaceKB_MIN,
			                    logRates,
			                    logSpans,
			                    rateToLogDiskAlert_map,
			                    firstTripLogObserved_map);

			//if data disk looks identical to log disk (same free space and same
			//rates across all windows), treat them as the same physical disk and
			//skip the data alerts to avoid a duplicate noisy alarm.
			bool dataIsSameAsLog =
			    (availableDataSpaceKB == availableLogSpaceKB) &&
			    (dataRates[0] == logRates[0]) && (dataRates[1] == logRates[1]) &&
			    (dataRates[2] == logRates[2]) && (dataRates[3] == logRates[3]);
			if(!dataIsSameAsLog)
				checkDiskRateAlerts("DATA",
				                    "data",
				                    otsdaq_data_dir,
				                    availableDataSpaceKB,
				                    availableDataSpaceKB_MIN,
				                    dataRates,
				                    dataSpans,
				                    rateToDataDiskAlert_map,
				                    firstTripDataObserved_map);
			else
				firstTripDataObserved_map.erase(appInfo.getContextName());
		}  // end of app loop

		if(oneStatusReqHasFailed)
		{
			__COUTT__ << "oneStatusReqHasFailed" << __E__;
			// sleep(5);  // sleep to not overwhelm server with errors
		}

	}  // end of infinite status checking loop
}  // end AppStatusWorkLoop()
catch(...)
{
	__COUT_ERR__ << "Unhandled exception in GatewaySupervisor::AppStatusWorkLoop "
	                "(doDisconnected = "
	             << doDisconnected << "). Exiting thread." << __E__;
}  //end AppStatusWorkLoop() catch

//==============================================================================
/// GetRemoteGatewayIcons
///	static function
void GatewaySupervisor::GetRemoteGatewayIcons(
    GatewaySupervisor::RemoteGatewayInfo& remoteGatewayApp,
    const std::unique_ptr<TransceiverSocket>& /* not transferring ownership */
        remoteGatewaySocket)
{
	// comma-separated icon string, 7 fields:
	//				0 - caption 		= text below icon
	//				1 - altText 		= text icon if no image given
	//				2 - uniqueWin 		= if true, only one window is allowed,
	// 										else  multiple instances of window
	//				3 - permissions 	= security level needed to see icon
	//				4 - picfn 			= icon image filename
	//				5 - linkurl 		= url of the window to open
	// 				6 - folderPath 		= folder and subfolder location '/' separated
	//	for example:  State Machine,FSM,1,200,icon-Physics.gif,/WebPath/html/StateMachine.html?fsm_name=OtherRuns0,,Chat,CHAT,1,1,icon-Chat.png,/urn:xdaq-application:lid=250,,Visualizer,VIS,0,10,icon-Visualizer.png,/WebPath/html/Visualization.html?urn=270,,Configure,CFG,0,10,icon-Configure.png,/urn:xdaq-application:lid=281,,Front-ends,CFG,0,15,icon-Configure.png,/WebPath/html/ConfigurationGUI_subset.html?urn=281&subsetBasePath=FEInterfaceTable&groupingFieldList=Status%2CFEInterfacePluginName&recordAlias=Front%2Dends&editableFieldList=%21%2ACommentDescription%2C%21SlowControls%2A,Config Subsets

	std::string iconString = "";

	std::string command = "GetRemoteDesktopIcons";

	__COUTT__ << "Sending remote gateway command '" << command << "' to target '"
	          << remoteGatewayApp.appInfo.name
	          << "' at url: " << remoteGatewayApp.appInfo.url << __E__;

	auto start = std::chrono::high_resolution_clock::now();
	try
	{
		std::vector<std::string> parsedFields =
		    StringMacros::getVectorFromString(remoteGatewayApp.appInfo.url, {':'});
		__COUTVS__(TLVL_RemoteIcons, StringMacros::vectorToString(parsedFields));
		__COUTVS__(TLVL_RemoteIcons, command);

		{
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
			                    std::chrono::high_resolution_clock::now() - start)
			                    .count();
			__COUTS__(TLVL_RemoteIcons)
			    << " Icons ----> Time pre sendAndReceive check ==> " << duration
			    << " milliseconds." << std::endl;
		}

		Socket gatewayRemoteSocket(parsedFields[1], atoi(parsedFields[2].c_str()));

		{
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
			                    std::chrono::high_resolution_clock::now() - start)
			                    .count();
			__COUTS__(TLVL_RemoteIcons)
			    << " Icons ----> Time pre2 sendAndReceive check ==> " << duration
			    << " milliseconds." << std::endl;
		}

		std::string remoteIconString = remoteGatewaySocket->sendAndReceive(
		    gatewayRemoteSocket, command, 10 /*timeoutSeconds*/);

		{
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
			                    std::chrono::high_resolution_clock::now() - start)
			                    .count();
			__COUTS__(TLVL_RemoteIcons) << " Icons ----> Time sendAndReceive check ==> "
			                            << duration << " milliseconds." << std::endl;
		}

		__COUTVS__(TLVL_RemoteIcons, remoteIconString);

		bool firstIcon = true;

		//now have remote icon string, append icons to list
		std::vector<std::string> remoteIconsCSV = StringMacros::getVectorFromString(
		    remoteIconString + ",",  //add 1 just in case last folder string is empty
		    {','});
		const size_t numOfIconFields = 7;
		for(size_t i = 0; i + numOfIconFields < remoteIconsCSV.size();
		    i += numOfIconFields)
		{
			if(firstIcon)
				firstIcon = false;
			else
				iconString += ",";

			__COUTVS__(TLVL_RemoteIcons, remoteIconsCSV[i + 0]);
			if(remoteGatewayApp.parentIconFolderPath ==
			   "")  //icon.folderPath_ == "") //if not in folder, distinguish remote icon somehow
				iconString +=
				    remoteGatewayApp.user_data_path_record  //icon.alternateText_
				    + " " + remoteIconsCSV[i + 0];          //icon.caption_;
			else
				iconString += remoteIconsCSV[i + 0];    //icon.caption_;
			iconString += "," + remoteIconsCSV[i + 1];  //icon.alternateText_;
			iconString +=
			    "," +
			    remoteIconsCSV
			        [i + 2];  //std::string(icon.enforceOneWindowInstance_ ? "1" : "0");
			iconString += "," + std::string("1");  // set permission to 1 so the
			                                       // desktop shows every icon that the
			                                       // server allows (i.e., trust server
			                                       // security, ignore client security)
			iconString += "," + remoteIconsCSV[i + 4];  //icon.imageURL_;
			iconString += "," + remoteIconsCSV[i + 5];  //icon.windowContentURL_;

			iconString += "," + remoteGatewayApp.parentIconFolderPath  //icon.folderPath_
			              + "/" + remoteIconsCSV[i + 6];

		}  //end append remote icons

	}  //end GetRemoteGatewayIcons()
	catch(const std::runtime_error& e)
	{
		__SS__ << "Failure gathering Remote Gateway desktop icons with command '"
		       << command << "' from target '" << remoteGatewayApp.appInfo.name
		       << "' at url: " << remoteGatewayApp.appInfo.url
		       << " due to error: " << e.what() << __E__;
		__COUT_ERR__ << ss.str();
		remoteGatewayApp.setError(ss.str());
		return;
	}  //end GetRemoteGatewayIcons() catch

	__COUTVS__(TLVL_RemoteIcons, iconString);
	remoteGatewayApp.iconString = iconString;

	{
		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
		                    std::chrono::high_resolution_clock::now() - start)
		                    .count();
		__COUTS__(TLVL_RemoteIcons) << " End Icons ----> Time sendAndReceive check ==> "
		                            << duration << " milliseconds." << std::endl;
	}
}  //end GetRemoteGatewayIcons()

//==============================================================================
/// SendRemoteGatewayCommand
///	static function
///		Format is FiniteStateMachineName,Command,Parameter(s)
void GatewaySupervisor::SendRemoteGatewayCommand(
    GatewaySupervisor::RemoteGatewayInfo& remoteGatewayApp,
    const std::unique_ptr<TransceiverSocket>& /* not transferring ownership */
        remoteGatewaySocket)
{
	remoteGatewayApp.clearError();  //clear error for new command

	__COUT__ << "Sending remote gateway command '" << remoteGatewayApp.command
	         << "' to target '" << remoteGatewayApp.appInfo.name
	         << "' at url: " << remoteGatewayApp.appInfo.url << __E__;

	std::string tmpCommand = remoteGatewayApp.command;
	try
	{
		std::string command;

		//for non-FSM commands, do not use fsmName
		if(remoteGatewayApp.command == "ResetConsoleCounts")
			command = "ResetConsoleCounts";
		else
			command = remoteGatewayApp.fsmName + "," + remoteGatewayApp.command;

		remoteGatewayApp.command = "Sent";  //Mark that send is being attempted
		if(tmpCommand == "Reboot")          //do nothing for reboot command
		{
			__COUT__ << "Reboot command handled by ots script command." << __E__;
			sleep(5);
			return;
		}

		std::vector<std::string> parsedFields =
		    StringMacros::getVectorFromString(remoteGatewayApp.appInfo.url, {':'});
		__COUTTV__(StringMacros::vectorToString(parsedFields));
		if(parsedFields.size() < 3)
		{
			__SS__ << "URL field of Remote Gateway app '" << remoteGatewayApp.appInfo.name
			       << "' is not in expected format (or has not been initialized yet): "
			          "'protocol:host:port' - here is the "
			          "URL: '"
			       << remoteGatewayApp.appInfo.url << "'" << __E__;
			__SS_THROW__;
		}
		__COUT__ << "Sending to subsystem '" << remoteGatewayApp.appInfo.name
		         << "' the command: " << command << __E__;

		Socket gatewayRemoteSocket(parsedFields[1], atoi(parsedFields[2].c_str()));

		// Use retransmission-mode sendAndReceiveAll for reliable multi-packet
		// config dump transfer. This replaces the old sendAndReceive + manual
		// receive loop, providing automatic packet ordering, dropped packet
		// detection, and retransmit requests.
		std::string commandResponseString =
		    remoteGatewaySocket->sendAndReceiveAll(gatewayRemoteSocket,
		                                           command,
		                                           10 /*timeoutSeconds*/,
		                                           10 /*retransmitMaxRetries*/,
		                                           false /*verbose*/);
		__COUT__ << "Response from subsystem '" << remoteGatewayApp.appInfo.name
		         << "' received: " << commandResponseString.size() << " bytes" << __E__;

		size_t donePos = commandResponseString.find("Done");
		if(donePos != 0)  //then error
		{
			size_t rootPos = commandResponseString.find("<ROOT>");
			if(rootPos == 0)
			{
				//assume accidental collision with Status response
				// check if DONE response appended, or try receiving again
				rootPos = commandResponseString.find("</ROOT>");
				if(rootPos > 0)
				{
					rootPos += 7;
					donePos = commandResponseString.find("Done", rootPos);
				}
				if(donePos > 0 && (donePos == rootPos || donePos == rootPos + 1))
				{
					__COUT__ << "Found DONE appended after status xml!" << __E__;
					commandResponseString = commandResponseString.substr(donePos);
					__COUTV__(commandResponseString);
					donePos = 0;  //mark good
				}
				else
				{
					donePos               = std::string::npos;  //clear
					commandResponseString = "";                 //clear
					if(remoteGatewaySocket->receive(commandResponseString,
					                                10 /*timeoutSeconds*/) ==
					   0 /* success */)
					{
						__COUT__ << "Response 2 from subsystem '"
						         << remoteGatewayApp.appInfo.name
						         << "' received: " << commandResponseString << __E__;
						donePos = commandResponseString.find("Done");
					}
					else  //timeout occurred
					{
						donePos               = std::string::npos;  //clear
						commandResponseString = "TIMEOUT!";
					}
				}
			}

			if(donePos != 0)  //then error
			{
				__SS__ << "Unsuccessful response received from Remote Gateway '"
				       << remoteGatewayApp.appInfo.name + "' - here was the response: "
				       << commandResponseString << __E__;
				__SS_THROW__;
			}
		}

		if(commandResponseString.size() > strlen("Done") + 1)
		{
			// With retransmission mode, the full response is already assembled
			// by sendAndReceiveAll(). Verify the END--- marker is present.
			if(commandResponseString.size() > 10 &&
			   !commandResponseString.ends_with("END---"))
			{
				__SS__ << "Config dump response from Remote Gateway '"
				       << remoteGatewayApp.appInfo.name
				       << "' is missing END--- termination marker. "
				       << "Received " << commandResponseString.size() << " bytes."
				       << __E__;
				const size_t maxPrint = 500;
				if(commandResponseString.size() <= maxPrint)
					ss << " Full text: [" << commandResponseString << "]";
				else
					ss << " Last " << maxPrint << " chars: ["
					   << commandResponseString.substr(commandResponseString.size() -
					                                   maxPrint)
					   << "]";
				ss << __E__;
				__SS_THROW__;
			}

			//assume have config dump response!
			// Extract dump content (everything after "Done,")
			std::string dumpContent = commandResponseString.substr(strlen("Done") + 1);

			// Remove 'END---' suffix if present
			if(dumpContent.size() > 6 && dumpContent.ends_with("END---"))
				dumpContent = dumpContent.substr(0, dumpContent.size() - 6);

			// Check if it's JSON by looking for opening brace
			if(!dumpContent.empty() && dumpContent[0] == '{')
			{
				__COUT__ << "Found JSON all dump type" << __E__;
				remoteGatewayApp.config_dump_type =
				    RemoteGatewayInfo::ConfigDumpTypes::JSON_all;
				remoteGatewayApp.config_dump = dumpContent;
			}
			else
			{
				__COUT__ << "Found text dump type" << __E__;
				remoteGatewayApp.config_dump_type =
				    RemoteGatewayInfo::ConfigDumpTypes::Text;

				remoteGatewayApp.config_dump = "\n\n************************\n";
				remoteGatewayApp.config_dump +=
				    "* Remote Subsystem Dump from '" + remoteGatewayApp.appInfo.name +
				    "' at url: " + remoteGatewayApp.appInfo.url + "\n";
				remoteGatewayApp.config_dump += "************************ \n";
				remoteGatewayApp.config_dump += "\n\n";
				remoteGatewayApp.config_dump += dumpContent;
			}
			__COUTTV__(remoteGatewayApp.config_dump);
			__COUT__ << "Successfully received config dump from remote gateway '"
			         << remoteGatewayApp.appInfo.name
			         << "' dump size: " << remoteGatewayApp.config_dump.size() << " bytes"
			         << __E__;
		}

	}  //end SendRemoteGatewayCommand()
	catch(const std::runtime_error& e)
	{
		__SS__ << "Failure sending Remote Gateway App '" << remoteGatewayApp.appInfo.name
		       << "' the command '"
		       << (tmpCommand.size() > 100
		               ? (tmpCommand.substr(0, 100) + "<truncated>...")
		               : tmpCommand)
		       << "' at url: " << remoteGatewayApp.appInfo.url
		       << " due to error: " << e.what() << __E__;
		__COUT_ERR__ << ss.str();
		remoteGatewayApp.setError(ss.str());
	}  //end SendRemoteGatewayCommand() catch

}  //end SendRemoteGatewayCommand()

//==============================================================================
/// CheckRemoteGatewayStatus
///	static function
///		Just need status, progress, and detail of ots::GatewaySupervisor extracted from GetRemoteGatewayStatus
void GatewaySupervisor::CheckRemoteGatewayStatus(
    GatewaySupervisor::RemoteGatewayInfo& remoteGatewayApp,
    const std::unique_ptr<TransceiverSocket>& /* not transferring ownership */
                       remoteGatewaySocket,
    const std::string& ipForReverseLoginOverUDP,
    int                portForReverseLoginOverUDP,
    const std::string& contextCommonList,
    const std::string& contextCommonOverrideList)
try
{
	//initialize to unknown in case of error
	remoteGatewayApp.appInfo.status         = SupervisorInfo::APP_STATUS_UNKNOWN;
	remoteGatewayApp.appInfo.progress       = 0;
	remoteGatewayApp.appInfo.detail         = "";
	remoteGatewayApp.appInfo.lastStatusTime = time(0);
	remoteGatewayApp.subapps
	    .clear();  //clear stale subapps before repopulating so removed subapps do not persist as UNKNOWN

	__COUTT__ << "Checking remote gateway status of '" << remoteGatewayApp.appInfo.name
	          << "'" << __E__;

	std::vector<std::string> parsedFields =
	    StringMacros::getVectorFromString(remoteGatewayApp.appInfo.url, {':'});
	__COUTTV__(StringMacros::vectorToString(parsedFields));

	if(parsedFields.size() == 3)
	{
		Socket      gatewayRemoteSocket(parsedFields[1], atoi(parsedFields[2].c_str()));
		std::string requestString = "GetRemoteGatewayStatus";
		if(portForReverseLoginOverUDP)
			requestString += "," + ipForReverseLoginOverUDP + "," +
			                 std::to_string(portForReverseLoginOverUDP) + "," +
			                 remoteGatewayApp.appInfo.name;
		requestString += "|" + COMMAND_PARAM_SUBSYSTEM_COMMON_CONTEXT_PREAMBLE +
		                 StringMacros::encodeURIComponent(contextCommonList);
		requestString += "|" + COMMAND_PARAM_SUBSYSTEM_COMMON_CONTEXT_OVERRIDE_PREAMBLE +
		                 StringMacros::encodeURIComponent(contextCommonOverrideList);
		__COUTS__(TLVL_RemoteStatusVerbose)
		    << "requestString = " << requestString << __E__;

		auto start = std::chrono::high_resolution_clock::now();

		std::string remoteStatusString = remoteGatewaySocket->sendAndReceive(
		    gatewayRemoteSocket,
		    requestString,
		    2 /*timeoutSeconds*/);  //Note: When TRACE slow path is over utilized on some systems, we see 3 second slow down frequently

		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
		                    std::chrono::high_resolution_clock::now() - start)
		                    .count();
		__COUTS__(TLVL_StatusRemoteWorkloop)
		    << "Time taken for send+receive of CheckRemoteGatewayStatus to '"
		    << remoteGatewayApp.appInfo.name << "' ==> " << duration << " milliseconds."
		    << std::endl;

		__COUTS__(TLVL_RemoteStatusVerbose)
		    << "remoteStatusString = " << remoteStatusString << __E__;

		std::string value, name;
		bool        foundGateway = false;
		size_t      after = 0, lastAfter = 0;
		while((name = StringMacros::extractXmlField(
		           remoteStatusString, "name", 0, after, &after)) != "")
		{
			after += std::string("name").size();  //move beyond found pos
			lastAfter = after;

			//find class associated with record
			value = StringMacros::extractXmlField(remoteStatusString, "class", 0, after);
			if(value == XDAQContextTable::GATEWAY_SUPERVISOR_CLASS)
			{
				foundGateway = true;

				//found remote gateway
				__COUTVS__(TLVL_RemoteStatusParams, remoteStatusString.size());
				__COUTVS__(TLVL_RemoteStatusParams, after);
				__COUTVS__(TLVL_RemoteStatusParams, value);

				//get gateway status
				value =
				    StringMacros::extractXmlField(remoteStatusString, "status", 0, after);
				__COUTVS__(TLVL_RemoteStatusParams, value);
				remoteGatewayApp.appInfo.status = value;

				value = StringMacros::extractXmlField(
				    remoteStatusString, "progress", 0, after);
				__COUTVS__(TLVL_RemoteStatusParams, value);
				remoteGatewayApp.appInfo.progress = atoi(value.c_str());

				value =
				    StringMacros::extractXmlField(remoteStatusString, "detail", 0, after);
				__COUTVS__(TLVL_RemoteStatusParams, value);
				remoteGatewayApp.appInfo.detail =
				    value;  //StringMacros::decodeURIComponent(value);

				value = StringMacros::extractXmlField(
				    remoteStatusString, "availableLogSpaceKB", 0, after);
				__COUTVS__(TLVL_RemoteStatusParams, value);
				if(!value.size())
					remoteGatewayApp.appInfo.availableLogSpaceKB = 0;
				else
					remoteGatewayApp.appInfo.availableLogSpaceKB = std::stoull(value);

				value = StringMacros::extractXmlField(
				    remoteStatusString, "availableDataSpaceKB", 0, after);
				__COUTVS__(TLVL_RemoteStatusParams, value);
				if(!value.size())
					remoteGatewayApp.appInfo.availableDataSpaceKB = 0;
				else
					remoteGatewayApp.appInfo.availableDataSpaceKB = std::stoull(value);

				value = StringMacros::extractXmlField(
				    remoteStatusString, "logUsageRateKBps", 0, after);
				__COUTVS__(TLVL_RemoteStatusParams, value);
				if(!value.size())
					value = "0";
				remoteGatewayApp.appInfo.logUsageRateKBps = std::stof(value);

				value = StringMacros::extractXmlField(
				    remoteStatusString, "dataUsageRateKBps", 0, after);
				__COUTVS__(TLVL_RemoteStatusParams, value);
				if(!value.size())
					value = "0";
				remoteGatewayApp.appInfo.dataUsageRateKBps = std::stof(value);

				value =
				    StringMacros::extractXmlField(remoteStatusString, "time", 0, after);
				__COUTVS__(TLVL_RemoteStatusParams, value);
				remoteGatewayApp.appInfo.lastStatusTime = atoi(value.c_str());

				value =
				    StringMacros::extractXmlField(remoteStatusString, "url", 0, after);
				__COUTVS__(TLVL_RemoteStatusParams, value);
				remoteGatewayApp.appInfo.parent_url = value;

				value = StringMacros::extractXmlField(remoteStatusString, "id", 0, after);
				__COUTVS__(TLVL_RemoteStatusParams, value);
				remoteGatewayApp.appInfo.id = atoi(value.c_str());

			}     //end found Remote Gateway status
			else  //found remote subapp
			{
				//get remote subapp class name
				remoteGatewayApp.subapps[name].class_name = value;
				__COUTVS__(TLVL_RemoteStatusParams, value);

				//get remote subapp status
				value =
				    StringMacros::extractXmlField(remoteStatusString, "status", 0, after);
				__COUTVS__(TLVL_RemoteStatusParams, value);
				remoteGatewayApp.subapps[name].status = value;

				value = StringMacros::extractXmlField(
				    remoteStatusString, "progress", 0, after);
				__COUTVS__(TLVL_RemoteStatusParams, value);
				remoteGatewayApp.subapps[name].progress = atoi(value.c_str());

				value =
				    StringMacros::extractXmlField(remoteStatusString, "detail", 0, after);
				__COUTVS__(TLVL_RemoteStatusParams, value);
				remoteGatewayApp.subapps[name].detail =
				    value;  //StringMacros::decodeURIComponent(value);

				value = StringMacros::extractXmlField(
				    remoteStatusString, "availableLogSpaceKB", 0, after);
				__COUTVS__(TLVL_RemoteStatusParams, value);
				if(!value.size())
					value = "0";
				remoteGatewayApp.subapps[name].availableLogSpaceKB = std::stoull(value);

				value = StringMacros::extractXmlField(
				    remoteStatusString, "availableDataSpaceKB", 0, after);
				__COUTVS__(TLVL_RemoteStatusParams, value);
				if(!value.size())
					value = "0";
				remoteGatewayApp.subapps[name].availableDataSpaceKB = std::stoull(value);

				value = StringMacros::extractXmlField(
				    remoteStatusString, "logUsageRateKBps", 0, after);
				__COUTVS__(TLVL_RemoteStatusParams, value);
				if(!value.size())
					value = "0";
				remoteGatewayApp.subapps[name].logUsageRateKBps = std::stof(value);

				value = StringMacros::extractXmlField(
				    remoteStatusString, "dataUsageRateKBps", 0, after);
				__COUTVS__(TLVL_RemoteStatusParams, value);
				if(!value.size())
					value = "0";
				remoteGatewayApp.subapps[name].dataUsageRateKBps = std::stof(value);

				value =
				    StringMacros::extractXmlField(remoteStatusString, "time", 0, after);
				__COUTVS__(TLVL_RemoteStatusParams, value);
				remoteGatewayApp.subapps[name].lastStatusTime = atoi(value.c_str());

				value =
				    StringMacros::extractXmlField(remoteStatusString, "url", 0, after);
				__COUTVS__(TLVL_RemoteStatusParams, value);
				remoteGatewayApp.subapps[name].parent_url = value;

				value = StringMacros::extractXmlField(remoteStatusString, "id", 0, after);
				__COUTVS__(TLVL_RemoteStatusParams, value);
				remoteGatewayApp.subapps[name].id = atoi(value.c_str());
			}
		}  //end primary loop

		if(!foundGateway)
		{
			__SS__ << "Failure encountered while checking remote gateway status of '"
			       << remoteGatewayApp.appInfo.name
			       << "' - no Gateway app status reported!" << __E__;
			__SS_THROW__;
		}
		after = lastAfter;
		__COUTVS__(TLVL_RemoteStatusParams, after);

		//get system messages
		value = StringMacros::extractXmlField(
		    remoteStatusString, "systemMessages", 0, after, &after);
		__COUTS__(TLVL_RemoteStatusParams) << "Remote System Messages:" << value << __E__;
		std::vector<std::string> parsedSysMsgs;
		StringMacros::getVectorFromString(value, parsedSysMsgs, {'|'});

		//Format: targetUser | time | msg | targetUser | time | msg...etc
		for(size_t i = 0; i + 2 < parsedSysMsgs.size(); i += 3)
		{
			GatewaySupervisor::addSystemMessage(
			    parsedSysMsgs[i],
			    "Remote System Message from '" + remoteGatewayApp.appInfo.name +
			        "' at url: " + remoteGatewayApp.appInfo.url + " ... " +
			        StringMacros::decodeURIComponent(parsedSysMsgs[i + 2]));
		}  //end System Message handling loop

		//get user with lock
		value = StringMacros::extractXmlField(
		    remoteStatusString, "usernameWithLock", 0, after, &after);
		__COUTS__(TLVL_RemoteStatusParams) << "Remote User with Lock:" << value << __E__;
		remoteGatewayApp.usernameWithLock = value;

		//get Console err/warn count
		value = StringMacros::extractXmlField(
		    remoteStatusString, "console_err_count", 0, after, &after);
		__COUTVS__(TLVL_RemoteStatusParams, value);
		remoteGatewayApp.consoleErrCount = atoi(value.c_str());

		value = StringMacros::extractXmlField(
		    remoteStatusString, "console_warn_count", 0, after);
		__COUTVS__(TLVL_RemoteStatusParams, value);
		remoteGatewayApp.consoleWarnCount = atoi(value.c_str());
	}
	else
		__COUT_WARN__ << "Illegal Remote Gateawy App URL for name='"
		              << remoteGatewayApp.appInfo.name
		              << "' (must be ots:<IP>:<PORT>): [URL="
		              << remoteGatewayApp.appInfo.url << "]" << __E__;
}  //end CheckRemoteGatewayStatus()
catch(const std::runtime_error& e)
{
	__COUTT__ << "Failure getting '" << remoteGatewayApp.appInfo.name
	          << "' Remote Gateway App status at url: " << remoteGatewayApp.appInfo.url
	          << " due to error: " << e.what() << __E__;

	remoteGatewayApp.appInfo.status         = SupervisorInfo::APP_STATUS_UNKNOWN;
	remoteGatewayApp.appInfo.progress       = 0;
	remoteGatewayApp.appInfo.detail         = "Unknown UDP Message Error";
	remoteGatewayApp.appInfo.lastStatusTime = time(0);
}  //end CheckRemoteGatewayStatus() catch
catch(...)
{
	__COUTT__ << "Failure getting '" << remoteGatewayApp.appInfo.name
	          << "' Remote Gateway App status at url: " << remoteGatewayApp.appInfo.url
	          << " due to unknown error." << __E__;

	remoteGatewayApp.appInfo.status         = SupervisorInfo::APP_STATUS_UNKNOWN;
	remoteGatewayApp.appInfo.progress       = 0;
	remoteGatewayApp.appInfo.detail         = "Unknown Error";
	remoteGatewayApp.appInfo.lastStatusTime = time(0);
}  //end CheckRemoteGatewayStatus() catch

//==============================================================================
/// applyContextCommonTables
///	static function
///		Parses CSV table-name/version strings and calls ConfigurationManager::applyContextCommonTables
///		to override/merge Context group tables (e.g. StateMachineTable) at remote subsystems.
void GatewaySupervisor::applyContextCommonTables(
    GatewaySupervisor* supervisor,
    const std::string& contextCommonList,
    const std::string& contextCommonOverrideList)
{
	__COUT__ << "Applying Context Common Tables from top-level..." << __E__;
	__COUTV__(contextCommonList);
	__COUTV__(contextCommonOverrideList);

	std::map<std::string, TableVersion> mergeInTables, overrideTables;

	if(!contextCommonList.empty())
		StringMacros::getMapFromString(contextCommonList, mergeInTables);
	if(!contextCommonOverrideList.empty())
		StringMacros::getMapFromString(contextCommonOverrideList, overrideTables);

	supervisor->CorePropertySupervisorBase::theConfigurationManager_
	    ->restoreActiveTableGroups(
	        false /*throwErrors*/,
	        "" /*pathToActiveGroupsFile*/,
	        ConfigurationManager::LoadGroupType::ONLY_BACKBONE_OR_CONTEXT_TYPES);
	if(mergeInTables.empty() && overrideTables.empty())
		return;

	supervisor->CorePropertySupervisorBase::theConfigurationManager_
	    ->applyContextCommonTables(mergeInTables, overrideTables);
}  //end applyContextCommonTables()

//==============================================================================
/// StateChangerWorkLoop
///	child thread
void GatewaySupervisor::StateChangerWorkLoop(GatewaySupervisor* theSupervisor)
{
	ConfigurationTree configLinkNode =
	    theSupervisor->CorePropertySupervisorBase::getSupervisorTableNode();

	std::string ipAddressForStateChangesOverUDP =
	    configLinkNode.getNode("IPAddressForStateChangesOverUDP").getValue<std::string>();
	int portForStateChangesOverUDP =
	    configLinkNode.getNode("PortForStateChangesOverUDP").getValue<int>();
	bool acknowledgementEnabled =
	    configLinkNode.getNode("EnableAckForStateChangesOverUDP").getValue<bool>();
	bool enableStateChanges =
	    configLinkNode.getNode("EnableStateChangesOverUDP").getValue<bool>();

	__COUTV__(ipAddressForStateChangesOverUDP);
	__COUTV__(portForStateChangesOverUDP);
	__COUTV__(acknowledgementEnabled);
	__COUTV__(enableStateChanges);

	TransceiverSocket sock(ipAddressForStateChangesOverUDP,
	                       portForStateChangesOverUDP);  // Take Port from Table
	try
	{
		sock.initialize(8 * 1024 * 1024 /*socketReceiveBufferSize=8MB*/);
	}
	catch(...)
	{
		// generate special message to indicate failed socket
		__SS__ << "FATAL Console error. Could not initialize socket at ip '"
		       << ipAddressForStateChangesOverUDP << "' and port "
		       << portForStateChangesOverUDP
		       << ". Perhaps it is already in use? Exiting State Changer "
		          "SOAPUtilities::receive loop."
		       << __E__;
		__SS_THROW__;
		return;
	}

	std::map<unsigned int /* lid */, SupervisorInfo>
	    localAllSupervisorInfo;  //only use in this workloop thread, stable copy of app status
	std::size_t              commaPosition;
	unsigned int             commaCounter = 0;
	std::size_t              begin        = 0;
	std::string              buffer;
	std::string              errorStr;
	std::string              fsmName;
	std::string              command;
	std::vector<std::string> parameters;

	using clock = std::chrono::steady_clock;
	auto start  = clock::now();

	while(1)
	{
		// workloop procedure
		//	if SOAPUtilities::receive a UDP command
		//		execute command
		//	else
		//		sleep

		{
			auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
			                    clock::now() - start)
			                    .count();
			if(duration > 20 /* ms */)
				__COUTS__(TLVL_StateChangerStatus)
				    << " ----> Check status start receive loop ==> " << duration
				    << " milliseconds since last. PID=" << getpid()
				    << " TID=" << std::this_thread::get_id() << " buffer=" << buffer
				    << std::endl;
			start = clock::now();
		}

		if(sock.receive(
		       buffer, 2 /*timeoutSeconds*/, 0 /*timeoutUSeconds*/, false /*verbose*/) !=
		   -1)
		{
			__COUTS__(TLVL_StateChanger)
			    << "UDP State Changer packet received from ip:port "
			    << sock.getLastIncomingIPAddress() << ":" << sock.getLastIncomingPort()
			    << " of size = " << buffer.size() << __E__;
			__COUTVS__(TLVL_StateChangerDetail, buffer);

			__COUTS__(TLVL_StateChangerStatus)
			    << " ----> Check status idle receive loop ==> "
			    << std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() -
			                                                             start)
			           .count()
			    << " milliseconds time idle. PID=" << getpid()
			    << " TID=" << std::this_thread::get_id() << std::endl;

			try
			{
				if(buffer.find("Help") == 0 || buffer.find("help") == 0)
				{
					std::stringstream out;

					out << "Supported Commands:\nHelp (this message)"
					    << "\n"
					    << "GetRemoteGatewayStatus(XML) - The XML version sends real "
					       "XML, without sends the format Gateways use to communicate "
					       "with each other"
					    << "\n"
					    << "GetRemoteAppStatus(XML)"
					    << "\n"
					    << "GetStateMachineNames"
					    << "\n"
					    << "ResetConsoleCounts"
					    << "\n"
					    << "loginVerify"
					    << "\n"
					    << "GetRemoteDesktopIcons"
					    << "\n"
					    << "GetAliasGlobalFields,<configAlias>"
					    << "\n"
					    << "FiniteStateMachineName,Command,Parameter(s)"
					    << "\n";

					sock.acknowledge(out.str(), false /* verbose */);
					continue;
				}

				bool remoteGatewayStatus = buffer.find("GetRemoteGatewayStatus") == 0;
				bool remoteGatewayStatusXML =
				    buffer.find("GetRemoteGatewayStatusXML") == 0;
				if(remoteGatewayStatusXML || buffer.find("GetRemoteAppStatusXML") == 0)
				{
					__COUT_TYPE__(TLVL_DEBUG + TLVL_StateChangerStatus)
					    << "Giving app status to remote monitor..." << __E__;

					//split buffer on pipe to separate comma-separated params from Context Common Table data
					std::string              commaSectionXML = buffer;
					std::vector<std::string> pipeSectionsXML;
					{
						size_t pipePos = buffer.find('|');
						if(pipePos != std::string::npos)
						{
							commaSectionXML = buffer.substr(0, pipePos);
							while(pipePos != std::string::npos)
							{
								size_t nextPipe = buffer.find('|', pipePos + 1);
								pipeSectionsXML.push_back(buffer.substr(
								    pipePos + 1,
								    nextPipe != std::string::npos ? nextPipe - pipePos - 1
								                                  : std::string::npos));
								pipePos = nextPipe;
							}
						}
					}

					if(remoteGatewayStatus &&
					   commaSectionXML.size() > strlen("GetRemoteGatewayStatusXML") + 1)
					{
						std::vector<std::string> params =
						    StringMacros::getVectorFromString(commaSectionXML, {','});
						if(params.size() == 4)
						{
							//Parameters are 	"," + ipForReverseLoginOverUDP +
							// 					"," + std::to_string(portForReverseLoginOverUDP) +
							// 					"," + remoteGatewayApp.appInfo.name;

							__COUTVS__(TLVL_StatusParams,
							           StringMacros::vectorToString(params));
							std::string tmpIP   = params[1];
							int         tmpPort = atoi(params[2].c_str());

							if(!theSupervisor->theWebUsers_
							        .remoteLoginVerificationEnabled_ ||
							   theSupervisor->theWebUsers_.remoteLoginVerificationIP_ !=
							       tmpIP ||
							   theSupervisor->theWebUsers_.remoteLoginVerificationPort_ !=
							       tmpPort)
							{
								theSupervisor->theWebUsers_.remoteLoginVerificationIP_ =
								    tmpIP;
								theSupervisor->theWebUsers_.remoteLoginVerificationPort_ =
								    tmpPort;
								theSupervisor->theWebUsers_.remoteGatewaySelfName_ =
								    params[3];
								theSupervisor->theWebUsers_
								    .remoteLoginVerificationEnabled_ =
								    true;  //mark as under remote control
								__COUT_INFO__
								    << "This Gateway '"
								    << theSupervisor->theWebUsers_.remoteGatewaySelfName_
								    << "' is now under remote control and will validate "
								       "logins through remote Gateway Supervisor at "
								    << theSupervisor->theWebUsers_
								           .remoteLoginVerificationIP_
								    << ":"
								    << theSupervisor->theWebUsers_
								           .remoteLoginVerificationPort_
								    << __E__;
							}
						}
						else
							__COUT_ERR__ << "Parameter count is not 4, it is "
							             << params.size() << __E__;
					}

					//handle Context Common Table data from pipe-delimited sections
					if(pipeSectionsXML.size() &&
					   theSupervisor->theWebUsers_.remoteLoginVerificationEnabled_)
					{
						std::string contextCommonList, contextCommonOverrideList;
						for(const auto& section : pipeSectionsXML)
						{
							if(section.find(
							       COMMAND_PARAM_SUBSYSTEM_COMMON_CONTEXT_PREAMBLE) == 0)
								contextCommonList =
								    StringMacros::decodeURIComponent(section.substr(
								        COMMAND_PARAM_SUBSYSTEM_COMMON_CONTEXT_PREAMBLE
								            .length()));
							else if(
							    section.find(
							        COMMAND_PARAM_SUBSYSTEM_COMMON_CONTEXT_OVERRIDE_PREAMBLE) ==
							    0)
								contextCommonOverrideList =
								    StringMacros::decodeURIComponent(section.substr(
								        COMMAND_PARAM_SUBSYSTEM_COMMON_CONTEXT_OVERRIDE_PREAMBLE
								            .length()));
						}

						bool changed = false;
						{
							std::lock_guard<std::mutex> lock(
							    theSupervisor->contextCommonMutex_);
							if(contextCommonList !=
							       theSupervisor->appliedContextCommonList_ ||
							   contextCommonOverrideList !=
							       theSupervisor->appliedContextCommonOverrideList_)
								changed = true;
						}

						if(changed && !theSupervisor->theStateMachine_.isInTransition())
						{
							try
							{
								GatewaySupervisor::applyContextCommonTables(
								    theSupervisor,
								    contextCommonList,
								    contextCommonOverrideList);
								std::lock_guard<std::mutex> lock(
								    theSupervisor->contextCommonMutex_);
								theSupervisor->appliedContextCommonList_ =
								    contextCommonList;
								theSupervisor->appliedContextCommonOverrideList_ =
								    contextCommonOverrideList;
							}
							catch(const std::exception& e)
							{
								__COUT_ERR__ << "Failed to apply context common tables: "
								             << e.what() << __E__;
							}
							catch(...)
							{
								__COUT_ERR__ << "Failed to apply context common tables."
								             << __E__;
							}
						}
					}

					XmlDocument xmlOut;
					auto        rootNode = xmlOut.getRootElement();

					for(const auto& it :
					    theSupervisor->allSupervisorInfo_.getAllSupervisorInfo())
					{
						// non-blocking here, it's ok if the status is stale
						if(theSupervisor->allSupervisorInfo_
						       .getSupervisorInfoMutex(it.second.getId())
						       .try_lock())
						{
							//if doesnt exist, create it
							if(localAllSupervisorInfo.find(it.second.getId()) ==
							   localAllSupervisorInfo.end())
								localAllSupervisorInfo.emplace(
								    std::pair<unsigned int, SupervisorInfo>(
								        it.second.getId(),  // descriptor.first,
								        SupervisorInfo(0 /* descriptor */,
								                       it.second.getName(),
								                       it.second.getContextName())));

							//copy if have lock
							localAllSupervisorInfo.at(it.second.getId()) = it.second;
							theSupervisor->allSupervisorInfo_
							    .getSupervisorInfoMutex(it.second.getId())
							    .unlock();
						}  //else use stale status already in
						else if(localAllSupervisorInfo.find(it.second.getId()) ==
						        localAllSupervisorInfo.end())
							continue;  //unless no stale value, then skip for now

						// const auto& appInfo = it.second;
						const auto& appInfo =
						    localAllSupervisorInfo.at(it.second.getId());

						if(0 &&  //always return all app status
						   remoteGatewayStatus &&
						   appInfo.getClass() !=
						       XDAQContextTable::GATEWAY_SUPERVISOR_CLASS)
							continue;  //only return Gateway status

						auto supervisorNode =
						    xmlOut.createChildElement("supervisor", rootNode);
						xmlOut.addAttributeToNode(
						    "name",
						    appInfo.getName(),
						    supervisorNode);  // get application name
						xmlOut.addAttributeToNode("id",
						                          std::to_string(appInfo.getId()),
						                          supervisorNode);  // get application id
						xmlOut.addAttributeToNode(
						    "status", appInfo.getStatus(), supervisorNode);  // get status
						xmlOut.addAttributeToNode(
						    "time",
						    std::to_string(appInfo.getLastStatusTime()),
						    supervisorNode);  // ? StringMacros::getTimestampString(appInfo.getLastStatusTime()) : "0");  // get time stamp
						xmlOut.addAttributeToNode(
						    "stale",
						    std::to_string(time(0) - appInfo.getLastStatusTime()),
						    supervisorNode);  // time since update
						xmlOut.addAttributeToNode("progress",
						                          std::to_string(appInfo.getProgress()),
						                          supervisorNode);  // get progress
						xmlOut.addTextElementToParent(
						    "detail", appInfo.getDetail(), supervisorNode);  // get detail
						xmlOut.addAttributeToNode(
						    "availableLogSpaceKB",
						    std::to_string(appInfo.getAvailableLogSpaceKB()),
						    supervisorNode);  // get log space
						xmlOut.addAttributeToNode(
						    "availableDataSpaceKB",
						    std::to_string(appInfo.getAvailableDataSpaceKB()),
						    supervisorNode);  // get data space
						float rate = appInfo.getLogUsageRateLastHourKBps();
						if(rate == 0)
							rate = appInfo.getLogUsageRateLastHalfHourKBps();
						if(rate == 0)
							rate = appInfo.getLogUsageRateLastQuarterHourKBps();
						if(rate == 0)
							rate = appInfo.getLogUsageRateNowKBps();
						xmlOut.addAttributeToNode("logUsageRateKBps",
						                          std::to_string(rate),
						                          supervisorNode);  // get log usage rate
						rate = appInfo.getDataUsageRateLastHourKBps();
						if(rate == 0)
							rate = appInfo.getDataUsageRateLastHalfHourKBps();
						if(rate == 0)
							rate = appInfo.getDataUsageRateLastQuarterHourKBps();
						if(rate == 0)
							rate = appInfo.getDataUsageRateNowKBps();
						xmlOut.addAttributeToNode("dataUsageRateKBps",
						                          std::to_string(rate),
						                          supervisorNode);  // get data usage rate
						xmlOut.addAttributeToNode(
						    "class",
						    appInfo.getClass(),
						    supervisorNode);  // get application class
						xmlOut.addAttributeToNode("url",
						                          appInfo.getURL(),
						                          supervisorNode);  // get application url
						xmlOut.addAttributeToNode("context",
						                          appInfo.getContextName(),
						                          supervisorNode);  // get context

						for(auto& subappInfoPair : appInfo.getSubappInfo())
						{
							auto subappElement =
							    xmlOut.createChildElement("subapp", supervisorNode);
							xmlOut.addAttributeToNode(
							    "name", subappInfoPair.first, subappElement);
							xmlOut.addAttributeToNode("status",
							                          subappInfoPair.second.status,
							                          subappElement);  // get status
							xmlOut.addAttributeToNode(
							    "time",
							    subappInfoPair.second.lastStatusTime
							        ? StringMacros::getTimestampString(
							              subappInfoPair.second.lastStatusTime)
							        : "0",
							    subappElement);  // get time stamp
							xmlOut.addAttributeToNode(
							    "stale",
							    std::to_string(time(0) -
							                   subappInfoPair.second.lastStatusTime),
							    subappElement);  // time since update
							xmlOut.addAttributeToNode(
							    "progress",
							    std::to_string(subappInfoPair.second.progress),
							    subappElement);  // get progress
							xmlOut.addTextElementToParent("detail",
							                              subappInfoPair.second.detail,
							                              subappElement);  // get detail
							xmlOut.addAttributeToNode("url",
							                          subappInfoPair.second.url,
							                          subappElement);  // get url
							xmlOut.addAttributeToNode("class",
							                          subappInfoPair.second.class_name,
							                          subappElement);  // get class
						}
					}

					if(remoteGatewayStatus)  //also return System Messages and console count and user-with-lock
					{
						__COUT_TYPE__(TLVL_DEBUG + TLVL_StateChangerStatus)
						    << "Giving extra Gateway info to remote monitor..." << __E__;

						xmlOut.addTextElementToParent("systemMessages",
						                              theWebUsers_.getAllSystemMessages(),
						                              rootNode);
						xmlOut.addTextElementToParent(
						    "usernameWithLock", theWebUsers_.getUserWithLock(), rootNode);

						std::lock_guard<std::mutex> lock(
						    theSupervisor->systemStatusMutex_);  //lock for rest of scope
						xmlOut.addTextElementToParent(
						    "console_err_count",
						    std::to_string(theSupervisor->systemConsoleErrCount_),
						    rootNode);
						xmlOut.addTextElementToParent(
						    "console_warn_count",
						    std::to_string(theSupervisor->systemConsoleWarnCount_),
						    rootNode);
					}

					std::stringstream out;
					xmlOut.outputXmlDocument((std::ostringstream*)&out,
					                         false /*dispStdOut*/);
					__COUTS__(TLVL_StatusParams)
					    << "App status to monitor: " << out.str() << __E__;
					sock.acknowledge(out.str(), false /* verbose */);
					continue;
				}
				else if(remoteGatewayStatus || buffer.find("GetRemoteAppStatus") == 0)
				{
					auto start = clock::now();

					__COUT_TYPE__(TLVL_DEBUG + TLVL_StateChangerStatus)
					    << "Giving app status to remote monitor..." << __E__;

					//split buffer on pipe to separate comma-separated params from Context Common Table data
					std::string              commaSection = buffer;
					std::vector<std::string> pipeSections;
					{
						size_t pipePos = buffer.find('|');
						if(pipePos != std::string::npos)
						{
							commaSection = buffer.substr(0, pipePos);
							while(pipePos != std::string::npos)
							{
								size_t nextPipe = buffer.find('|', pipePos + 1);
								pipeSections.push_back(buffer.substr(
								    pipePos + 1,
								    nextPipe != std::string::npos ? nextPipe - pipePos - 1
								                                  : std::string::npos));
								pipePos = nextPipe;
							}
						}
					}

					if(remoteGatewayStatus &&
					   commaSection.size() > strlen("GetRemoteGatewayStatus") + 1)
					{
						std::vector<std::string> params =
						    StringMacros::getVectorFromString(commaSection, {','});
						if(params.size() == 4)
						{
							//Parameters are 	"," + ipForReverseLoginOverUDP +
							// 					"," + std::to_string(portForReverseLoginOverUDP) +
							// 					"," + remoteGatewayApp.appInfo.name;

							__COUTVS__(TLVL_StatusParams,
							           StringMacros::vectorToString(params));
							std::string tmpIP   = params[1];
							int         tmpPort = atoi(params[2].c_str());

							if(!theSupervisor->theWebUsers_
							        .remoteLoginVerificationEnabled_ ||
							   theSupervisor->theWebUsers_.remoteLoginVerificationIP_ !=
							       tmpIP ||
							   theSupervisor->theWebUsers_.remoteLoginVerificationPort_ !=
							       tmpPort)
							{
								theSupervisor->theWebUsers_.remoteLoginVerificationIP_ =
								    tmpIP;
								theSupervisor->theWebUsers_.remoteLoginVerificationPort_ =
								    tmpPort;
								theSupervisor->theWebUsers_.remoteGatewaySelfName_ =
								    params[3];
								theSupervisor->theWebUsers_
								    .remoteLoginVerificationEnabled_ =
								    true;  //mark as under remote control
								__COUT_INFO__
								    << "This Gateway '"
								    << theSupervisor->theWebUsers_.remoteGatewaySelfName_
								    << "' is now under remote control and will validate "
								       "logins through remote Gateway Supervisor at "
								    << theSupervisor->theWebUsers_
								           .remoteLoginVerificationIP_
								    << ":"
								    << theSupervisor->theWebUsers_
								           .remoteLoginVerificationPort_
								    << __E__;
							}
						}
						else
							__COUT_ERR__ << "Parameter count is not 4, it is "
							             << params.size() << __E__;
					}

					//handle Context Common Table data from pipe-delimited sections
					if(pipeSections.size() &&
					   theSupervisor->theWebUsers_.remoteLoginVerificationEnabled_)
					{
						std::string contextCommonList, contextCommonOverrideList;
						for(const auto& section : pipeSections)
						{
							if(section.find(
							       COMMAND_PARAM_SUBSYSTEM_COMMON_CONTEXT_PREAMBLE) == 0)
								contextCommonList =
								    StringMacros::decodeURIComponent(section.substr(
								        COMMAND_PARAM_SUBSYSTEM_COMMON_CONTEXT_PREAMBLE
								            .length()));
							else if(
							    section.find(
							        COMMAND_PARAM_SUBSYSTEM_COMMON_CONTEXT_OVERRIDE_PREAMBLE) ==
							    0)
								contextCommonOverrideList =
								    StringMacros::decodeURIComponent(section.substr(
								        COMMAND_PARAM_SUBSYSTEM_COMMON_CONTEXT_OVERRIDE_PREAMBLE
								            .length()));
						}

						bool changed = false;
						{
							std::lock_guard<std::mutex> lock(
							    theSupervisor->contextCommonMutex_);
							if(contextCommonList !=
							       theSupervisor->appliedContextCommonList_ ||
							   contextCommonOverrideList !=
							       theSupervisor->appliedContextCommonOverrideList_)
								changed = true;
						}

						if(changed && !theSupervisor->theStateMachine_.isInTransition())
						{
							try
							{
								GatewaySupervisor::applyContextCommonTables(
								    theSupervisor,
								    contextCommonList,
								    contextCommonOverrideList);
								std::lock_guard<std::mutex> lock(
								    theSupervisor->contextCommonMutex_);
								theSupervisor->appliedContextCommonList_ =
								    contextCommonList;
								theSupervisor->appliedContextCommonOverrideList_ =
								    contextCommonOverrideList;
							}
							catch(const std::exception& e)
							{
								__COUT_ERR__ << "Error applying Context Common Tables: "
								             << e.what() << __E__;
							}
							catch(...)
							{
								__COUT_ERR__
								    << "Unknown error applying Context Common Tables."
								    << __E__;
							}
						}
					}

					HttpXmlDocument xmlOut;
					for(const auto& it :
					    theSupervisor->allSupervisorInfo_.getAllSupervisorInfo())
					{
						const auto& appInfo = it.second;
						if(0 &&  //always return full status
						   remoteGatewayStatus &&
						   appInfo.getClass() !=
						       XDAQContextTable::GATEWAY_SUPERVISOR_CLASS)
							continue;  //only return Gateway status

						xmlOut.addTextElementToData(
						    "name",
						    appInfo.getName());  // get application name
						xmlOut.addTextElementToData(
						    "id", std::to_string(appInfo.getId()));  // get application id
						xmlOut.addTextElementToData("status",
						                            appInfo.getStatus());  // get status
						xmlOut.addTextElementToData(
						    "time",
						    std::to_string(
						        appInfo
						            .getLastStatusTime()));  // ? StringMacros::getTimestampString(appInfo.getLastStatusTime()) : "0");  // get time stamp
						xmlOut.addTextElementToData(
						    "stale",
						    std::to_string(
						        time(0) -
						        appInfo.getLastStatusTime()));  // time since update
						xmlOut.addTextElementToData(
						    "progress",
						    std::to_string(appInfo.getProgress()));  // get progress
						xmlOut.addTextElementToData("detail",
						                            appInfo.getDetail());  // get detail
						xmlOut.addNumberElementToData(
						    "availableLogSpaceKB",
						    appInfo.getAvailableLogSpaceKB());  // get log space
						xmlOut.addNumberElementToData(
						    "availableDataSpaceKB",
						    appInfo.getAvailableDataSpaceKB());  // get data space
						float rate = appInfo.getLogUsageRateLastHourKBps();
						if(rate == 0)
							rate = appInfo.getLogUsageRateLastHalfHourKBps();
						if(rate == 0)
							rate = appInfo.getLogUsageRateLastQuarterHourKBps();
						if(rate == 0)
							rate = appInfo.getLogUsageRateNowKBps();
						xmlOut.addNumberElementToData("logUsageRateKBps",
						                              rate);  // get log usage rate
						rate = appInfo.getDataUsageRateLastHourKBps();
						if(rate == 0)
							rate = appInfo.getDataUsageRateLastHalfHourKBps();
						if(rate == 0)
							rate = appInfo.getDataUsageRateLastQuarterHourKBps();
						if(rate == 0)
							rate = appInfo.getDataUsageRateNowKBps();
						xmlOut.addNumberElementToData("dataUsageRateKBps",
						                              rate);  // get data usage rate
						xmlOut.addTextElementToData(
						    "class",
						    appInfo.getClass());  // get application class
						xmlOut.addTextElementToData(
						    "url",
						    appInfo.getURL());  // get application url
						xmlOut.addTextElementToData(
						    "context",
						    appInfo.getContextName());  // get context
						auto subappElement = xmlOut.addTextElementToData("subapps", "");
						auto copySubappVector = appInfo.getSubappInfo();
						for(auto& subappInfoPair : copySubappVector)
						{
							xmlOut.addTextElementToParent(
							    "subapp_name", subappInfoPair.first, subappElement);
							xmlOut.addTextElementToParent("subapp_status",
							                              subappInfoPair.second.status,
							                              subappElement);  // get status
							xmlOut.addTextElementToParent(
							    "subapp_time",
							    subappInfoPair.second.lastStatusTime
							        ? StringMacros::getTimestampString(
							              subappInfoPair.second.lastStatusTime)
							        : "0",
							    subappElement);  // get time stamp
							xmlOut.addTextElementToParent(
							    "subapp_stale",
							    std::to_string(time(0) -
							                   subappInfoPair.second.lastStatusTime),
							    subappElement);  // time since update
							xmlOut.addTextElementToParent(
							    "subapp_progress",
							    std::to_string(subappInfoPair.second.progress),
							    subappElement);  // get progress
							xmlOut.addTextElementToParent("subapp_detail",
							                              subappInfoPair.second.detail,
							                              subappElement);  // get detail
							xmlOut.addTextElementToParent("subapp_url",
							                              subappInfoPair.second.url,
							                              subappElement);  // get url
							xmlOut.addTextElementToParent(
							    "subapp_class",
							    subappInfoPair.second.class_name,
							    subappElement);  // get class
						}
					}

					if(remoteGatewayStatus)  //also return System Messages and console count and user-with-lock
					{
						__COUT_TYPE__(TLVL_DEBUG + TLVL_StateChangerStatus)
						    << "Giving extra Gateway info to remote monitor..." << __E__;

						xmlOut.addTextElementToData("systemMessages",
						                            theWebUsers_.getAllSystemMessages());
						xmlOut.addTextElementToData("usernameWithLock",
						                            theWebUsers_.getUserWithLock());

						std::lock_guard<std::mutex> lock(
						    theSupervisor->systemStatusMutex_);  //lock for rest of scope
						xmlOut.addTextElementToData(
						    "console_err_count",
						    std::to_string(theSupervisor->systemConsoleErrCount_));
						xmlOut.addTextElementToData(
						    "console_warn_count",
						    std::to_string(theSupervisor->systemConsoleWarnCount_));
					}

					std::stringstream out;
					xmlOut.outputXmlDocument((std::ostringstream*)&out,
					                         false /*dispStdOut*/,
					                         false /*allowWhiteSpace*/);

					__COUTS__(TLVL_StateChangerStatus)
					    << "Time taken for xml response to GetRemoteGatewayStatus "
					       "==> "
					    << std::chrono::duration_cast<std::chrono::milliseconds>(
					           clock::now() - start)
					           .count()
					    << " milliseconds." << std::endl;

					__COUTS__(TLVL_StatusParams)
					    << "App status to monitor: " << out.str() << __E__;
					sock.acknowledge(out.str(), false /* verbose */);

					__COUTS__(TLVL_StateChangerStatus)
					    << "Time taken for receive+send response to "
					       "GetRemoteGatewayStatus ==> "
					    << std::chrono::duration_cast<std::chrono::milliseconds>(
					           clock::now() - start)
					           .count()
					    << " milliseconds." << std::endl;

					continue;
				}  //end GetRemoteAppStatus
				if(buffer.find("GetStateMachineNames") == 0)
				{
					__COUT_TYPE__(TLVL_DEBUG + TLVL_StateChangerStatus)
					    << "Giving state machine names to remote monitor..." << __E__;
					std::vector<std::string> fsmNames;
					if(!configLinkNode.isDisconnected())
					{
						fsmNames = configLinkNode.getNode("LinkToStateMachineTable")
						               .getChildrenNames();
					}

					HttpXmlDocument xmlOut;
					for(auto& fsm : fsmNames)
					{
						xmlOut.addTextElementToData("fsm", fsm);
					}
					if(theSupervisor->activeStateMachineName_ != "")
					{
						xmlOut.addTextElementToData(
						    "active", theSupervisor->activeStateMachineName_);
					}

					std::stringstream out;
					xmlOut.outputXmlDocument((std::ostringstream*)&out,
					                         false /*dispStdOut*/,
					                         false /*allowWhiteSpace*/);
					__COUTS__(TLVL_StatusParams)
					    << "State machines to monitor: " << out.str() << __E__;
					sock.acknowledge(out.str(), false /* verbose */);
					continue;
				}
				if(buffer.find("ResetConsoleCounts") == 0)
				{
					__COUT__ << "Remote request to reset Console Counts..." << __E__;

					//zero out console count and retake first messages

					for(const auto& it :
					    theSupervisor->allSupervisorInfo_.getAllSupervisorInfo())
					{
						const auto& appInfo = it.second;
						if(appInfo.isTypeConsoleSupervisor())
						{
							xoap::MessageReference tempMessage =
							    SOAPUtilities::makeSOAPMessageReference(
							        "ResetConsoleCounts");
							std::string reply =
							    theSupervisor->send(appInfo.getDescriptor(), tempMessage);

							if(reply != "Done")
							{
								__SS__ << "Error while resetting console counts of "
								          "Supervisor instance = '"
								       << appInfo.getName()
								       << "' [LID=" << appInfo.getId() << "] in Context '"
								       << appInfo.getContextName()
								       << "' [URL=" << appInfo.getURL() << "].\n\n"
								       << reply << __E__;
								__SS_THROW__;
							}
							__COUT__ << "Reset console counts of Supervisor instance = '"
							         << appInfo.getName() << "' [LID=" << appInfo.getId()
							         << "] in Context '" << appInfo.getContextName()
							         << "' [URL=" << appInfo.getURL() << "]." << __E__;
						}
					}  //end loop for Console Supervisors

					//for user display feedback, clear local cached values also
					std::lock_guard<std::mutex> lock(
					    theSupervisor->systemStatusMutex_);  //lock for rest of scope
					theSupervisor->lastConsoleErrTime_   = "0";
					theSupervisor->lastConsoleErr_       = "";
					theSupervisor->lastConsoleWarnTime_  = "0";
					theSupervisor->lastConsoleWarn_      = "";
					theSupervisor->lastConsoleInfoTime_  = "0";
					theSupervisor->lastConsoleInfo_      = "";
					theSupervisor->firstConsoleErrTime_  = "0";
					theSupervisor->firstConsoleErr_      = "";
					theSupervisor->firstConsoleWarnTime_ = "0";
					theSupervisor->firstConsoleWarn_     = "";
					theSupervisor->firstConsoleInfoTime_ = "0";
					theSupervisor->firstConsoleInfo_     = "";

					sock.acknowledge("Done", false /* verbose */);
					continue;
				}  //end ResetConsoleCounts
				else if(buffer.find("loginVerify") == 0)
				{
					__COUTT__
					    << "Checking login verification request from remote gateway..."
					    << __E__;

					//Lookup cookie code and return refreshed cookie code and user info
					// command = loginVerify
					// parameters.addParameter("CookieCode");
					// parameters.addParameter("RefreshOption");
					// parameters.addParameter("IPAddress");
					//	-- Use remote gateway self name to lookup access level conversion for user
					//  -- if Desktop Icon has a special permission type, then modify userGroupPermissionsMap's allUsers to match
					//		parameters.addParameter("RemoteGatewaySelfName");
					std::vector<std::string> rxParams =
					    StringMacros::getVectorFromString(buffer, {','});
					__COUTVS__(TLVL_StatusParams, StringMacros::vectorToString(rxParams));

					if(rxParams.size() != 5)
					{
						__COUT_ERR__ << "Invalid remote login verify attempt! Expected 5 "
						                "parameters, got "
						             << rxParams.size() << __E__;
						sock.acknowledge("0", false /* verbose */);
						continue;
					}

					// If TRUE, cookie code is good, and refreshed code is in cookieCode, also pointers
					// optionally for uint8_t userPermissions, uint64_t uid  Else, error message is
					// returned in cookieCode
					std::map<std::string /*groupName*/, WebUsers::permissionLevel_t>
					            userGroupPermissionsMap;
					std::string userWithLock = "";
					uint64_t    uid, userSessionIndex;
					std::string cookieCode = rxParams[1];
					if(!theWebUsers_.cookieCodeIsActiveForRequest(
					       cookieCode /*cookieCode*/,
					       &userGroupPermissionsMap,
					       &uid /*uid is not given to remote users*/,
					       "0" /* check at remote location because ip addresses change from subsystem to subsystem depending on tunnels,... rxParams[3] */
					       /*ip*/,
					       rxParams[2] /*refresh*/ == "1",
					       false /* doNotGoRemote */,
					       &userWithLock,
					       &userSessionIndex))
					{
						__COUT_ERR__ << "Remote login failed!" << __E__;
						sock.acknowledge("0", false /* verbose */);
						continue;
					}

					//Modify Permission Map based on Desktop Icon permission requirement
					const std::string& remoteName = rxParams[4];
					__COUTVS__(TLVL_Permissions, remoteName);

					//lookup RequiredPermissionLevel of Icon with this remoteName as FolderPath
					std::map<std::string /*groupName*/, WebUsers::permissionLevel_t>
					    subsystemIconPermissionLevelMap;
					{
						std::string subsystemIconPermissionLevel = "";

						{  //mutex scope
							std::lock_guard<std::mutex> lock(
							    theSupervisor->latestGatewayIconsMutex_);
							const std::vector<DesktopIconTable::DesktopIcon>& icons =
							    theSupervisor->latestGatewayIcons_;

							for(const auto& icon : icons)
							{
								if(icon.recordUID_ == remoteName)
								{
									subsystemIconPermissionLevel =
									    icon.permissionThresholdString_;
									__COUTVS__(
									    TLVL_Permissions,
									    subsystemIconPermissionLevel);  //the permission threshold for the icon that matches the remote subsystem name
									break;
								}
							}  //search for icon that matches subsystem name
						}
						StringMacros::getMapFromString(subsystemIconPermissionLevel,
						                               subsystemIconPermissionLevelMap);
					}  //end subsystemIconPermissionLevelMap construction
					__COUTVS__(
					    TLVL_Permissions,
					    StringMacros::mapToString(subsystemIconPermissionLevelMap));

					std::vector<GatewaySupervisor::RemoteGatewayInfo>
					    remoteGatewayApps;  //local copy
					{                       //lock for remainder of scope
						std::lock_guard<std::mutex> lock(
						    theSupervisor->remoteGatewayAppsMutex_);
						remoteGatewayApps = theSupervisor->remoteGatewayApps_;
						__COUTVS__(TLVL_Permissions, remoteGatewayApps.size());
					}

					bool found = false;
					for(const auto& remoteGatewayApp : remoteGatewayApps)
						if(remoteName == remoteGatewayApp.appInfo.name)
						{
							found = true;
							__COUTVS__(TLVL_Permissions,
							           remoteGatewayApp.permissionThresholdString);

							std::map<std::string /*groupName*/,
							         WebUsers::permissionLevel_t>
							    remoteIconPermissionsMap;
							StringMacros::getMapFromString(
							    remoteGatewayApp.permissionThresholdString,
							    remoteIconPermissionsMap);

							//if permission map is only size 1,
							//	then modify WebUsers::DEFAULT_USER_GROUP for user with the icon's group level of the user
							//	e.g. if user is 'HW: 255, allUsers: 1'
							//		and icon is 'HW: 1'
							//	then give to remote subsystem the user permission as 'HW: 255, allUsers: 255'
							//
							//	... this way the user is considered an expert at the remote subsystem
							//
							//	if requesting remote gateway side has not specified advanced permissions threshold, e.g. allUsers: 1, then
							//		if user is 'HW: 255, allUsers: 1' and there is 'HW: 1' for the icon, then give user
							//			HW: 255, allUsers: 255'

							if(  //if permission map is only size 1,
							    //	then modify WebUsers::DEFAULT_USER_GROUP for user with the icon's group level of the user
							    //	e.g. if user is 'HW: 255, allUsers: 1'
							    //		and icon is 'HW: 1'
							    //	then give to remote subsystem the user permission as 'HW: 255, allUsers: 255'
							    remoteIconPermissionsMap.size() == 1 &&
							    remoteIconPermissionsMap.begin()->first !=
							        WebUsers::DEFAULT_USER_GROUP)
							{
								__COUTVS__(
								    TLVL_Permissions,
								    remoteIconPermissionsMap.begin()->first);  //the group

								auto it = userGroupPermissionsMap.find(
								    remoteIconPermissionsMap.begin()->first);
								std::map<std::string /*groupName*/,
								         WebUsers::permissionLevel_t>::iterator it2 =
								    userGroupPermissionsMap.find(
								        WebUsers::DEFAULT_USER_GROUP);
								if(it != userGroupPermissionsMap.end() &&
								   it2 != userGroupPermissionsMap.end())
								{
									__COUTS__(TLVL_Permissions)
									    << "Found user group '" << it->first
									    << "' to modify: " << (uint16_t)it2->second
									    << " --> " << (uint16_t)it->second << __E__;
									it2->second = it->second;
									__COUTVS__(TLVL_Permissions, (uint16_t)it2->second);
								}
								else if(
								    it ==
								    userGroupPermissionsMap
								        .end())  //if special group not found, then no access
									userGroupPermissionsMap
									    [remoteIconPermissionsMap.begin()->first] =
									        WebUsers::PERMISSION_LEVEL_INACTIVE;
							}
							else if(  //	if requesting remote gateway side has not specified advanced permissions threshold, e.g. allUsers: 1, then
							    //		if user is 'HW: 255, allUsers: 1' and there is 'HW: 1' for the icon, then give user
							    //			HW: 255, allUsers: 255'
							    remoteIconPermissionsMap.size() == 1 &&
							    remoteIconPermissionsMap.begin()->first ==
							        WebUsers::DEFAULT_USER_GROUP &&
							    subsystemIconPermissionLevelMap.size() == 1 &&
							    subsystemIconPermissionLevelMap.begin()->first !=
							        WebUsers::DEFAULT_USER_GROUP)
							{
								__COUTVS__(TLVL_Permissions,
								           subsystemIconPermissionLevelMap.begin()
								               ->first);  //the group

								auto it = userGroupPermissionsMap.find(
								    subsystemIconPermissionLevelMap.begin()->first);
								std::map<std::string /*groupName*/,
								         WebUsers::permissionLevel_t>::iterator it2 =
								    userGroupPermissionsMap.find(
								        WebUsers::DEFAULT_USER_GROUP);
								if(it != userGroupPermissionsMap.end() &&
								   it2 != userGroupPermissionsMap.end())
								{
									__COUTS__(TLVL_Permissions)
									    << "Found user group '" << it->first
									    << "' to modify: " << (uint16_t)it2->second
									    << " --> " << (uint16_t)it->second << __E__;
									it2->second = it->second;
									__COUTVS__(TLVL_Permissions, (uint16_t)it2->second);
								}
								else if(
								    it ==
								    userGroupPermissionsMap
								        .end())  //if special group not found, then no access on default user group!
								{
									__COUTS__(TLVL_Permissions)
									    << "Did not find user group '"
									    << subsystemIconPermissionLevelMap.begin()->first
									    << "' when required by subsystem icon, so deny "
									       "access for user: "
									    << WebUsers::DEFAULT_USER_GROUP << " --> "
									    << WebUsers::PERMISSION_LEVEL_INACTIVE << __E__;
									userGroupPermissionsMap
									    [WebUsers::DEFAULT_USER_GROUP] =
									        WebUsers::PERMISSION_LEVEL_INACTIVE;
								}
							}

							break;
						}  //end handling of matching remote subsystem for login verification

					if(!found)
					{
						__COUT_ERR__ << "Did not find any matching subsystems for remote "
						                "login verify from '"
						             << remoteName << "' attempted!" << __E__;
					}

					// Returned user info:
					// retParameters.addParameter("CookieCode", cookieCode);
					// MODIFIED FOR SUBSYSTEM "Permissions", StringMacros::mapToString(userGroupPermissionsMap).c_str());
					// "UserWithLock", userWithLock);
					// "Username", theWebUsers_.getUsersUsername(uid));
					// "DisplayName", theWebUsers_.getUsersDisplayName(uid));
					// "UserSessionIndex"

					__COUTVS__(TLVL_Permissions,
					           StringMacros::mapToString(userGroupPermissionsMap));

					std::string retStr   = "";
					std::string username = theWebUsers_.getUsersUsername(uid);
					retStr += cookieCode;
					retStr +=
					    "," + StringMacros::encodeURIComponent(
					              StringMacros::mapToString(userGroupPermissionsMap));
					retStr += "," + userWithLock;
					retStr += "," + username;
					retStr += "," + theWebUsers_.getUsersDisplayName(uid);
					retStr += "," + std::to_string(userSessionIndex);

					__COUTVS__(TLVL_Permissions, retStr);
					__COUTT__ << "Remote login successful for " << username
					          << ", userWithLock = " << userWithLock << __E__;
					sock.acknowledge(retStr, false /* verbose */);
					continue;
				}
				else if(buffer.find("GetRemoteDesktopIcons") == 0)
				{
					__COUTS__(TLVL_RemoteIcons)
					    << "Giving desktop icons to remote gateway..." << __E__;

					// get icons and create comma-separated string based on user permissions
					//	note: each icon has own permission threshold, so each user can have
					//		a unique desktop icon experience.

					// use latest context always from temporary configuration manager,
					//	to get updated icons every time...
					//(so icon changes do no require an ots restart)
					//no need for mutex, because remote icons only accessed here!
					std::pair<std::string /*group name*/, TableGroupKey> latestGroup =
					    theSupervisor->latestGatewayRemoteIconsContextGroup_;
					if(latestGroup.first.size())
					{
						std::string                                          timeString;
						std::pair<std::string /*group name*/, TableGroupKey> theGroup =
						    ConfigurationManager::loadGroupNameAndKey(
						        ConfigurationManager::LAST_ACTIVATED_CONTEXT_GROUP_FILE,
						        timeString);
						if(theGroup == latestGroup)
						{
							__COUTT__
							    << "Using cached latest remote icons for context group '"
							    << theGroup.first << "(" << theGroup.second << ")"
							    << __E__;

							__COUTVS__(TLVL_RemoteIcons,
							           theSupervisor->latestGatewayRemoteIconsString_);

							sock.acknowledge(
							    theSupervisor->latestGatewayRemoteIconsString_,
							    true /* verbose */);
							continue;
						}
					}  //end check for active context changing

					//else then need to load latest icons

					ConfigurationManager
					    tmpCfgMgr;  // Creating new temporary instance so that constructor will activate latest context, note: not using member CorePropertySupervisorBase::theConfigurationManager_
					const DesktopIconTable* iconTable =
					    tmpCfgMgr.__GET_CONFIG__(DesktopIconTable);
					const std::vector<DesktopIconTable::DesktopIcon>& icons =
					    iconTable->getAllDesktopIcons();

					//store latest context group for next time
					theSupervisor->latestGatewayRemoteIconsContextGroup_ =
					    tmpCfgMgr.getActiveTableGroups()
					        [ConfigurationManager::GROUP_TYPE_NAME_CONTEXT];

					//at this point icons is correctly populated

					theSupervisor->latestGatewayRemoteIconsString_ =
					    "";  //will be populated below for caching
					std::string& iconString =
					    theSupervisor
					        ->latestGatewayRemoteIconsString_;  //will be populated below
					// comma-separated icon string, 7 fields:
					//				0 - caption 		= text below icon
					//				1 - altText 		= text icon if no image given
					//				2 - uniqueWin 		= if true, only one window is allowed,
					// 										else  multiple instances of window
					//				3 - permissions 	= security level needed to see icon
					//				4 - picfn 			= icon image filename
					//				5 - linkurl 		= url of the window to open
					// 				6 - folderPath 		= folder and subfolder location '/' separated
					//	for example:  State Machine,FSM,1,200,icon-Physics.gif,/WebPath/html/StateMachine.html?fsm_name=OtherRuns0,,Chat,CHAT,1,1,icon-Chat.png,/urn:xdaq-application:lid=250,,Visualizer,VIS,0,10,icon-Visualizer.png,/WebPath/html/Visualization.html?urn=270,,Configure,CFG,0,10,icon-Configure.png,/urn:xdaq-application:lid=281,,Front-ends,CFG,0,15,icon-Configure.png,/WebPath/html/ConfigurationGUI_subset.html?urn=281&subsetBasePath=FEInterfaceTable&groupingFieldList=Status%2CFEInterfacePluginName&recordAlias=Front%2Dends&editableFieldList=%21%2ACommentDescription%2C%21SlowControls%2A,Config Subsets

					bool getRemoteIcons = true;
					bool firstIcon      = true;

					//always force insert UserSettings so that the lock can be managed
					{
						if(firstIcon)
							firstIcon = false;
						else
							iconString += ",";

						iconString += "User Settings";            //icon.caption_;
						iconString += "," + std::string("User");  //icon.alternateText_;
						iconString +=
						    "," +
						    std::string(
						        "1");  //std::string(icon.enforceOneWindowInstance_ ? "1" : "0");
						iconString +=
						    "," + std::string("1");  // set permission to 1 so the
						                             // desktop shows every icon that the
						                             // server allows (i.e., trust server
						                             // security, ignore client security)
						iconString += "," + std::string(
						                        "/WebPath/images/dashboardImages/"
						                        "icon-Settings.png");  //icon.imageURL_;
						iconString +=
						    "," + iconTable->getRemoteURL(
						              &tmpCfgMgr, "/WebPath/html/UserSettings.html");
						iconString += "," + std::string("");  //icon.folderPath_;
					}

					for(const auto& icon : icons)
					{
						__COUTVS__(TLVL_DebugStatusWorkloop, icon.caption_);
						__COUTVS__(TLVL_DebugStatusWorkloop,
						           icon.permissionThresholdString_);

						//ignore permission level, and give all icons

						if(getRemoteIcons)
						{
							__COUTVS__(10, icon.windowContentURL_);
							if(icon.windowContentURL_.size() > 4 &&
							   icon.windowContentURL_[0] == 'o' &&
							   icon.windowContentURL_[1] == 't' &&
							   icon.windowContentURL_[2] == 's' &&
							   icon.windowContentURL_[3] == ':')
							{
								__COUTS__(10) << "Retrieving remote icons at "
								              << icon.windowContentURL_ << __E__;

								std::vector<std::string> parsedFields =
								    StringMacros::getVectorFromString(
								        icon.windowContentURL_, {':'});
								__COUTVS__(10,
								           StringMacros::vectorToString(parsedFields));

								if(parsedFields.size() == 3)
								{
									Socket iconRemoteSocket(
									    parsedFields[1], atoi(parsedFields[2].c_str()));

									// ConfigurationTree configLinkNode = theSupervisor->CorePropertySupervisorBase::getSupervisorTableNode();
									// std::string ipAddressForStateChangesOverUDP = configLinkNode.getNode("IPAddressForStateChangesOverUDP").getValue<std::string>();
									__COUTVS__(10, ipAddressForStateChangesOverUDP);
									TransceiverSocket iconSocket(
									    ipAddressForStateChangesOverUDP);
									std::string remoteIconString =
									    iconSocket.sendAndReceive(iconRemoteSocket,
									                              "GetRemoteDesktopIcons",
									                              10 /*timeoutSeconds*/);
									__COUTVS__(10, remoteIconString);
									continue;
								}
							}
						}  //end remote icon handling

						// have icon access, so add to CSV string
						if(firstIcon)
							firstIcon = false;
						else
							iconString += ",";

						__COUTVS__(TLVL_RemoteIcons, icon.caption_);
						iconString += icon.caption_;
						iconString += "," + icon.alternateText_;
						iconString +=
						    "," + std::string(icon.enforceOneWindowInstance_ ? "1" : "0");
						iconString +=
						    "," + std::string("1");  // set permission to 1 so the
						                             // desktop shows every icon that the
						                             // server allows (i.e., trust server
						                             // security, ignore client security)
						iconString += "," + icon.imageURL_;
						iconString += "," + iconTable->getRemoteURL(
						                        &tmpCfgMgr, icon.windowContentURL_);
						iconString += "," + icon.folderPath_;
					}

					//always force insert Wiz Mode Config view so users can access from top-level
					{
						if(firstIcon)
							firstIcon = false;
						else
							iconString += ",";

						iconString += "Wiz-Mode Config";         //icon.caption_;
						iconString += "," + std::string("WIZ");  //icon.alternateText_;
						iconString +=
						    "," +
						    std::string(
						        "0");  //std::string(icon.enforceOneWindowInstance_ ? "1" : "0");
						iconString += "," + std::string("255");  // set permission to 255
						iconString += "," + std::string(
						                        "/WebPath/images/dashboardImages/"
						                        "icon-Settings.png");  //icon.imageURL_;
						iconString +=
						    "," +
						    iconTable->getRemoteURL(
						        &tmpCfgMgr,
						        "/WebPath/html/ConfigurationGUI.html?urn=" +
						            std::to_string(XDAQContextTable::XDAQApplication::
						                               WIZMODE_CONFIG_APP_ID),
						        true /* forWizMode */);
						iconString += "," + std::string("");  //icon.folderPath_;
					}

					__COUTVS__(TLVL_RemoteIcons, iconString);

					sock.acknowledge(iconString, true /* verbose */);
					continue;
				}  //end GetRemoteDesktopIcons
				else if(buffer.find("GetAliasGlobalFields,") == 0)
				{
					std::vector<std::string> params =
					    StringMacros::getVectorFromString(buffer, {','});
					if(params.size() < 2)
					{
						__COUT_ERR__
						    << "GetAliasGlobalFields requires a config alias parameter."
						    << __E__;
						sock.acknowledge("", false /* verbose */);
						continue;
					}

					std::string configAlias = params[1];
					__COUT__ << "GetAliasGlobalFields for alias '" << configAlias << "'"
					         << __E__;

					std::string globalFieldsResult = "";
					try
					{
						ConfigurationManager tmpCfgMgr;
						auto groupPair = tmpCfgMgr.getTableGroupFromAlias(configAlias);
						if(groupPair.first != "")
						{
							std::map<std::string, TableVersion> groupMembers;
							tmpCfgMgr.loadTableGroup(groupPair.first,
							                         groupPair.second,
							                         false /*doActivate*/,
							                         &groupMembers,
							                         0 /*progressBar*/,
							                         0 /*accumulateWarnings*/,
							                         0 /*groupComment*/,
							                         0 /*groupAuthor*/,
							                         0 /*groupCreateTime*/,
							                         true /*doNotLoadMembers*/);

							std::map<std::string, TableVersion> globalMembers;
							for(const auto& member : groupMembers)
								if(member.first.find("Global") != std::string::npos)
									globalMembers.emplace(member);

							__COUT__ << "GetAliasGlobalFields - found "
							         << globalMembers.size() << " Global table(s) out of "
							         << groupMembers.size() << " total members." << __E__;

							if(globalMembers.size())
							{
								tmpCfgMgr.loadMemberMap(globalMembers);
								globalFieldsResult =
								    getGlobalFieldsString(&tmpCfgMgr, globalMembers);
							}
						}
						else
							__COUT_WARN__ << "Could not find group for alias '"
							              << configAlias << "'." << __E__;
					}
					catch(const std::runtime_error& e)
					{
						__COUT_WARN__ << "Error getting Global fields for alias '"
						              << configAlias << "': " << e.what() << __E__;
					}
					catch(...)
					{
						__COUT_WARN__ << "Unknown error getting Global fields for alias '"
						              << configAlias << "'." << __E__;
					}

					sock.acknowledge(globalFieldsResult, false /* verbose */);
					continue;
				}                             //end GetAliasGlobalFields
				else if(!enableStateChanges)  //else it is an FSM Command!
				{
					__COUT_WARN__ << "Skipping potential FSM Command because "
					                 "enableStateChanges=false"
					              << __E__;
					continue;
				}

				__COUT__ << "Received a remote FSM Command attempt!" << __E__;

				size_t nCommas = std::count(buffer.begin(), buffer.end(), ',');
				if(nCommas == 0)
				{
					__SS__ << "Unrecognized State Machine command :-" << buffer
					       << "-. Format is FiniteStateMachineName,Command,Parameter(s). "
					          "Where Parameter(s) is/are optional."
					       << __E__;
					__COUT_ERR__ << ss.str();
					if(acknowledgementEnabled)
					{
						__COUTT__ << "Ack'ing" << __E__;
						sock.acknowledge(ss.str(), true /* verbose */);
					}
					continue;
				}
				begin        = 0;
				commaCounter = 0;
				parameters.clear();
				while((commaPosition = buffer.find(',', begin)) != std::string::npos ||
				      commaCounter == nCommas)
				{
					if(commaCounter == nCommas)
						commaPosition = buffer.size();
					if(commaCounter == 0)
						fsmName = buffer.substr(begin, commaPosition - begin);
					else if(commaCounter == 1)
						command = buffer.substr(begin, commaPosition - begin);
					else
						parameters.push_back(buffer.substr(begin, commaPosition - begin));
					__COUT__ << "Word[" << commaCounter
					         << "]: " << buffer.substr(begin, commaPosition - begin)
					         << __E__;

					begin = commaPosition + 1;
					++commaCounter;
				}
				__COUTV__(fsmName);
				__COUTV__(command);
				__COUTV__(StringMacros::vectorToString(parameters));

				// Check if this is an iteration re-send for a subsystem already in transition
				// (top-level is driving iterations and re-sending with IterationIndex:N)
				{
					bool isIterationResend = false;
					if(theSupervisor->theStateMachine_.isInTransition() &&
					   theSupervisor->isRemoteSubsystemIteration_.load())
					{
						for(const auto& param : parameters)
						{
							if(param.find(COMMAND_PARAM_ITERATION_INDEX_PREAMBLE) == 0)
							{
								unsigned int iterIdx = 0;
								try
								{
									iterIdx = std::stoul(param.substr(
									    COMMAND_PARAM_ITERATION_INDEX_PREAMBLE.length()));
								}
								catch(...)
								{
									__COUT_WARN__
									    << "Failed to parse IterationIndex from '"
									    << param << "' -- ignoring malformed parameter."
									    << __E__;
									break;
								}
								__COUT__
								    << "Received iteration re-send with IterationIndex:"
								    << iterIdx
								    << " while mid-transition -- signaling "
								       "broadcastMessage()"
								    << __E__;

								{
									std::lock_guard<std::mutex> lock(
									    theSupervisor->remoteIterationMutex_);
									theSupervisor->remoteIterationIndex_ = iterIdx;
								}
								theSupervisor->remoteIterationCV_.notify_one();
								isIterationResend = true;
								break;
							}
						}
					}

					if(isIterationResend)
					{
						if(acknowledgementEnabled)
							sock.acknowledge("Done", true /* verbose */);
						continue;
					}
				}

				// set scope of mutex
				std::string extraDoneContent = "";
				{
					// should be mutually exclusive with GatewaySupervisor main thread state
					// machine accesses  lockout the messages array for the remainder of the
					// scope  this guarantees the reading thread can safely access the
					// messages
					if(theSupervisor->VERBOSE_MUTEX)
						__COUT__ << "Waiting for FSM access" << __E__;
					std::lock_guard<std::mutex> lock(
					    theSupervisor->stateMachineAccessMutex_);
					if(theSupervisor->VERBOSE_MUTEX)
						__COUT__ << "Have FSM access" << __E__;

					errorStr = theSupervisor->attemptStateMachineTransition(
					    0,
					    0,
					    command,
					    fsmName,
					    WebUsers::DEFAULT_STATECHANGER_USERNAME /*fsmWindowName*/,
					    WebUsers::DEFAULT_STATECHANGER_USERNAME,
					    parameters);

					if(0 &&  //no longer returning dump on configure (it takes too long, and is incorrect if subsystems configure multiple times)
					   errorStr == "" &&
					   command == RunControlStateMachine::CONFIGURE_TRANSITION_NAME)
						extraDoneContent =
						    theSupervisor->activeStateMachineSystemDumpOnConfigure_;

					if(errorStr ==
					       "" &&  //start transition is where subusystem configure dump is aggregated!
					   command == RunControlStateMachine::START_TRANSITION_NAME)
						extraDoneContent =
						    theSupervisor->activeStateMachineSystemDumpOnRun_;
				}
				if(extraDoneContent.size())
					extraDoneContent += "END---";

				if(errorStr != "")
				{
					__SS__
					    << "UDP State Changer failed to execute command because of the "
					       "following error: "
					    << errorStr;
					__COUT_ERR__ << ss.str();
					if(acknowledgementEnabled)
						sock.acknowledge(errorStr, true /* verbose */);
				}
				else
				{
					__SS__ << "Successfully executed state change command '" << command
					       << ".'" << __E__;
					__COUT_INFO__ << ss.str();
					if(acknowledgementEnabled)
						sock.acknowledge(
						    "Done" + (extraDoneContent.size()
						                  ? ("," + extraDoneContent)
						                  : ""  //append extra done content, if any
						              ),
						    true /* verbose */,
						    extraDoneContent.size() ? 65500 : 1500 /*maxChunkSize*/,
						    0 /*interPacketGapUSeconds*/,
						    extraDoneContent.size() >
						        0 /*enableRetransmission - use retransmit protocol for large config dump transfers*/);
				}
			}
			catch(...)
			{
				__SS__ << "Error was caught handling UDP command." << __E__;
				try
				{
					throw;
				}
				catch(const std::runtime_error& e)
				{
					ss << "Here is the error: " << e.what() << __E__;
				}
				catch(...)
				{
					ss << "Unrecognized error." << __E__;
				}

				__COUT_ERR__ << ss.str();
				if(acknowledgementEnabled)
					sock.acknowledge(ss.str(), true /* verbose */);
			}
		}
		else
		{
			__COUTS__(TLVL_StateChangerDetail)
			    << "Waiting for UDP State Changer packet on "
			    << ipAddressForStateChangesOverUDP << ":" << portForStateChangesOverUDP
			    << "..." << __E__;
			usleep(1000 /* 1ms */);
		}
	}  // end while(1) loop
}  // end StateChangerWorkLoop()

//==============================================================================
/// makeSystemLogEntry
///	makes a logbook entry into all Logbook supervisors
///		and specifically the current active experiments within the logbook
///	escape entryText to make it html/xml safe!!
////      reserved: ", ', &, <, >, \n, double-space
void GatewaySupervisor::makeSystemLogEntry(const std::string& entryText,
                                           const std::string& subjectText /* = "" */)
{
	__COUT__ << "Making System Logbook Entry: " << entryText << __E__;
	if(subjectText.size())
		__COUTV__(subjectText);
	lastLogbookEntry_     = entryText;
	lastLogbookEntryTime_ = time(0);

	SupervisorInfoMap logbookInfoMap =
	    allSupervisorInfo_.getAllLogbookTypeSupervisorInfo();

	if(logbookInfoMap.size() == 0)
	{
		__COUT__ << "No logbooks found! Here is entry: " << entryText << __E__;
		return;
	}
	else
	{
		__COUT__ << "Making logbook entry: " << entryText << __E__;
	}

	SOAPParameters parameters("EntryText", StringMacros::encodeURIComponent(entryText));
	parameters.addParameter("SubjectText", StringMacros::encodeURIComponent(subjectText));

	for(auto& logbookInfo : logbookInfoMap)
	{
		try
		{
			xoap::MessageReference retMsg = SOAPMessenger::sendWithSOAPReply(
			    logbookInfo.second.getDescriptor(), "MakeSystemLogEntry", parameters);

			SOAPParameters retParameters("Status");
			SOAPUtilities::receive(retMsg, retParameters);

			std::string status = retParameters.getValue("Status");
			__COUT__ << "Returned Status: " << status
			         << __E__;  // retParameters[0].getValue() << __E__ << __E__;
			if(status != "Success")
			{
				__SS__ << "Invalid return status on MakeSystemLogEntry: " << status
				       << __E__;
				__SS_THROW__;
			}
		}
		catch(const xdaq::exception::Exception& e)  // due to xoap send failure
		{
			__SS__ << "Failed to send system log SOAP entry to "
			       << logbookInfo.second.getContextName() << "/"
			       << logbookInfo.second.getName() << " w/app ID=" << logbookInfo.first
			       << __E__ << e.what();

			__SS_THROW__;
		}
		catch(std::runtime_error& e)
		{
			__SS__ << "Error during handling of system log SOAP entry at "
			       << logbookInfo.second.getContextName() << "/"
			       << logbookInfo.second.getName() << " w/app ID=" << logbookInfo.first
			       << __E__ << e.what();
			__SS_THROW__;
		}
	}
}  // end makeSystemLogEntry()

//==============================================================================
void GatewaySupervisor::Default(xgi::Input* /*in*/, xgi::Output* out)
{
	if(!supervisorGuiHasBeenLoaded_ &&
	   (supervisorGuiHasBeenLoaded_ =
	        true))  // make system logbook entry that ots has been started
	{
		bool doLog = false;
		try
		{
			doLog = __ENV__("OTS_LOG_INTERMEDIATE_STATES") == std::string("1");
		}
		catch(...)
		{ /* ignore errors */
			;
		}

		if(doLog)
			makeSystemLogEntry("ots started.");
	}

	*out << "<!DOCTYPE HTML><html lang='en'><head><title>ots</title>"
	     << GatewaySupervisor::getIconHeaderString() <<
	    // end show ots icon
	    "</head>"
	     << "<frameset col='100%' row='100%'>"
	     << "<frame src='/WebPath/html/Desktop.html?urn="
	     << this->getApplicationDescriptor()->getLocalId()
	     << "&securityType=" << securityType_ << "'></frameset></html>";
}  // end Default()

//==============================================================================
std::string GatewaySupervisor::getIconHeaderString(void)
{
	// show ots icon
	//	from http://www.favicon-generator.org/
	return "<link rel='apple-touch-icon' sizes='57x57' href='/WebPath/images/otsdaqIcons/apple-icon-57x57.png'>\
	<link rel='apple-touch-icon' sizes='60x60' href='/WebPath/images/otsdaqIcons/apple-icon-60x60.png'>\
	<link rel='apple-touch-icon' sizes='72x72' href='/WebPath/images/otsdaqIcons/apple-icon-72x72.png'>\
	<link rel='apple-touch-icon' sizes='76x76' href='/WebPath/images/otsdaqIcons/apple-icon-76x76.png'>\
	<link rel='apple-touch-icon' sizes='114x114' href='/WebPath/images/otsdaqIcons/apple-icon-114x114.png'>\
	<link rel='apple-touch-icon' sizes='120x120' href='/WebPath/images/otsdaqIcons/apple-icon-120x120.png'>\
	<link rel='apple-touch-icon' sizes='144x144' href='/WebPath/images/otsdaqIcons/apple-icon-144x144.png'>\
	<link rel='apple-touch-icon' sizes='152x152' href='/WebPath/images/otsdaqIcons/apple-icon-152x152.png'>\
	<link rel='apple-touch-icon' sizes='180x180' href='/WebPath/images/otsdaqIcons/apple-icon-180x180.png'>\
	<link rel='icon' type='image/png' sizes='192x192'  href='/WebPath/images/otsdaqIcons/android-icon-192x192.png'>\
	<link rel='icon' type='image/png' sizes='144x144'  href='/WebPath/images/otsdaqIcons/android-icon-144x144.png'>\
	<link rel='icon' type='image/png' sizes='48x48'  href='/WebPath/images/otsdaqIcons/android-icon-48x48.png'>\
	<link rel='icon' type='image/png' sizes='72x72'  href='/WebPath/images/otsdaqIcons/android-icon-72x72.png'>\
	<link rel='icon' type='image/png' sizes='32x32' href='/WebPath/images/otsdaqIcons/favicon-32x32.png'>\
	<link rel='icon' type='image/png' sizes='96x96' href='/WebPath/images/otsdaqIcons/favicon-96x96.png'>\
	<link rel='icon' type='image/png' sizes='16x16' href='/WebPath/images/otsdaqIcons/favicon-16x16.png'>\
	<link rel='manifest' href='/WebPath/images/otsdaqIcons/manifest.json'>\
	<meta name='msapplication-TileColor' content='#ffffff'>\
	<meta name='msapplication-TileImage' content='/WebPath/images/otsdaqIcons/ms-icon-144x144.png'>\
	<meta name='theme-color' content='#ffffff'>";

}  // end getIconHeaderString()

//==============================================================================
////////////////////////////////////////////////////////////////////////
void GatewaySupervisor::XGI_Turtle(xgi::Input* /*in*/, xgi::Output* out)
{
	//test if ImageMagick is installed to do convert, if not just return existing png
	if(!picGen_.imageMagickInstallChecked)
	{
		//to install on AL9, sudo dnf install -y ImageMagick
		std::string ret =
		    StringMacros::exec("convert --version");  //check if ImageMagick is installed
		__COUTVS__(TLVL_StatusFullDetail, ret);
		picGen_.imageMagickInstallChecked = true;
		picGen_.imageMagickInstalled      = ret == "" ? false : true;
	}

	std::string filepath =
	    __ENV__("OTSDAQ_WEB_PATH") + std::string("/images/otsdaqIcons/");

	std::string filename = filepath + "generated/turtle.png";
	if(picGen_.imageMagickInstalled)
		picGen_.generateTurtle(filepath);
	else
		filename = filepath + "turtle.png";

	//insertPngRawData(out,"images/generated/turtle.png");
	{
		//write raw picture data to output stream
		std::ifstream is;
		is.open(filename.c_str());

		*out << "data:image/png;charset=US-ASCII,";

		char CodeURL[4];
		while(is.good())
		{  //print out safe Ascii equivalent
			sprintf(CodeURL, "%%%2.2X", (unsigned char)(is.get()));
			*out << CodeURL;
		}

		is.close();
	}
}  //end XGI_Turtle()

//==============================================================================
/// stateMachineIterationBreakpoint
///		get/set the state machine iteration breakpoint
///		If the iteration index >= breakpoint, then pause.
void GatewaySupervisor::stateMachineIterationBreakpoint(xgi::Input* in, xgi::Output* out)
try
{
	cgicc::Cgicc cgiIn(in);

	std::string requestType = CgiDataUtilities::getData(cgiIn, "Request");

	HttpXmlDocument           xmlOut;
	WebUsers::RequestUserInfo userInfo(requestType,
	                                   CgiDataUtilities::postData(cgiIn, "CookieCode"));

	CorePropertySupervisorBase::getRequestUserInfo(userInfo);

	if(!theWebUsers_.xmlRequestOnGateway(cgiIn, out, &xmlOut, userInfo))
		return;  // access failed

	__COUTV__(requestType);

	try
	{
		if(requestType == "get")
		{
			std::stringstream v;
			{  // start mutex scope
				std::lock_guard<std::mutex> lock(broadcastIterationBreakpointMutex_);
				v << broadcastIterationBreakpoint_;
			}  // end mutex scope

			xmlOut.addTextElementToData("iterationBreakpoint", v.str());
		}
		else if(requestType == "set")
		{
			unsigned int breakpointSetValue =
			    CgiDataUtilities::getDataAsInt(cgiIn, "breakpointSetValue");
			__COUTV__(breakpointSetValue);

			{  // start mutex scope
				std::lock_guard<std::mutex> lock(broadcastIterationBreakpointMutex_);
				broadcastIterationBreakpoint_ = breakpointSetValue;
			}  // end mutex scope

			// return the value that was set
			std::stringstream v;
			v << breakpointSetValue;
			xmlOut.addTextElementToData("iterationBreakpoint", v.str());
		}
		else
		{
			__SS__ << "Unknown iteration breakpoint request type = " << requestType
			       << __E__;
			__SS_THROW__;
		}
	}
	catch(const std::runtime_error& e)
	{
		__SS__ << "Error caught handling iteration breakpoint command: " << e.what()
		       << __E__;
		__COUT_ERR__ << ss.str();
		xmlOut.addTextElementToData("Error", ss.str());
	}
	catch(...)
	{
		__SS__ << "Unknown error caught handling iteration breakpoint command." << __E__;
		try
		{
			throw;
		}  //one more try to printout extra info
		catch(const std::exception& e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...)
		{
		}
		__COUT_ERR__ << ss.str();
		xmlOut.addTextElementToData("Error", ss.str());
	}  // end stateMachineIterationBreakpoint() catch

	xmlOut.outputXmlDocument((std::ostringstream*)out, false, true);

}  // end stateMachineIterationBreakpoint()
catch(const std::runtime_error& e)
{
	__SS__ << "Error caught handling iteration breakpoint command: " << e.what() << __E__;
	__COUT_ERR__ << ss.str();
}
catch(...)
{
	__SS__ << "Unknown error caught handling iteration breakpoint command." << __E__;
	try
	{
		throw;
	}  //one more try to printout extra info
	catch(const std::exception& e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...)
	{
	}
	__COUT_ERR__ << ss.str();
}  // end stateMachineIterationBreakpoint() catch

//==============================================================================
void GatewaySupervisor::stateMachineXgiHandler(xgi::Input* in, xgi::Output* out)
{
	// for simplicity assume all commands should be mutually exclusive with iterator
	// thread state machine accesses (really should just be careful with
	// RunControlStateMachine access)
	if(VERBOSE_MUTEX)
		__COUT__ << "Waiting for FSM access" << __E__;
	std::lock_guard<std::mutex> lock(stateMachineAccessMutex_);
	if(VERBOSE_MUTEX)
		__COUT__ << "Have FSM access" << __E__;

	out->getHTTPResponseHeader().addHeader(
	    "Access-Control-Allow-Origin",
	    "*");  // to avoid block by blocked by CORS policy of browser
	cgicc::Cgicc cgiIn(in);

	std::string command = CgiDataUtilities::getData(cgiIn, "StateMachine");
	std::string requestType =
	    "StateMachine-" + command;  // prepend StateMachine to request type
	__COUTV__(requestType);

	HttpXmlDocument           xmlOut;
	WebUsers::RequestUserInfo userInfo(requestType,
	                                   CgiDataUtilities::postData(cgiIn, "CookieCode"));

	CorePropertySupervisorBase::getRequestUserInfo(userInfo);

	if(!theWebUsers_.xmlRequestOnGateway(cgiIn, out, &xmlOut, userInfo))
		return;  // access failed

	std::string fsmName       = CgiDataUtilities::getData(cgiIn, "fsmName");
	std::string fsmWindowName = CgiDataUtilities::getOrPostData(cgiIn, "fsmWindowName");
	fsmWindowName             = StringMacros::decodeURIComponent(fsmWindowName);
	std::string currentState  = theStateMachine_.getCurrentStateName();

	__COUT__ << "Check for Handled by theIterator_, activeStateMachineWindowName_ = "
	         << activeStateMachineWindowName_ << __E__;

	// check if Iterator should handle
	if((activeStateMachineWindowName_ == "" ||
	    activeStateMachineWindowName_ == "iterator" ||
	    (activeStateMachineName_ == fsmName &&
	     command.find("iterate") ==
	         0) /* for combo iterate/fsm GUIs like SubsystemLaunch.js */) &&
	   theIterator_.handleCommandRequest(xmlOut, command, fsmWindowName))
	{
		__COUT__ << "Handled by theIterator_" << __E__;
		xmlOut.outputXmlDocument((std::ostringstream*)out, false);
		return;
	}

	// Do not allow transition while in transition
	if(theStateMachine_.isInTransition())
	{
		__SS__ << "Error - Can not accept request because the State Machine is already "
		          "in transition!"
		       << __E__;
		__COUT_ERR__ << "\n" << ss.str();

		xmlOut.addTextElementToData("state_transition_attempted",
		                            "0");  // indicate to GUI transition NOT attempted
		xmlOut.addTextElementToData(
		    "state_transition_attempted_err",
		    ss.str());  // indicate to GUI transition NOT attempted
		xmlOut.outputXmlDocument((std::ostringstream*)out, false, true);
		return;
	}

	// At this point, attempting transition!

	std::vector<std::string> parameters;

	if(command == "Configure")
		parameters.push_back(CgiDataUtilities::postData(cgiIn, "ConfigurationAlias"));

	std::string logEntry =
	    StringMacros::decodeURIComponent(CgiDataUtilities::postData(cgiIn, "logEntry"));

	attemptStateMachineTransition(&xmlOut,
	                              out,
	                              command,
	                              fsmName,
	                              fsmWindowName,
	                              userInfo.username_,
	                              parameters,
	                              logEntry);

}  // end stateMachineXgiHandler()

//==============================================================================
std::string GatewaySupervisor::attemptStateMachineTransition(
    HttpXmlDocument*                xmldoc,
    std::ostringstream*             out,
    const std::string&              command,
    const std::string&              fsmName,
    const std::string&              fsmWindowName,
    const std::string&              username,
    const std::vector<std::string>& commandParameters,
    std::string                     logEntry /* = "" */)
try
{
	std::string errorStr = "";

	std::string currentState = theStateMachine_.getCurrentStateName();
	__COUT_INFO__ << "State Machine attempted command = " << command << __E__;
	__COUTV__(fsmName);
	__COUTV__(fsmWindowName);
	__COUTV__(username);
	__COUTV__(activeStateMachineName_);
	__COUTV__(logEntry);
	__COUTV__(command);
	__COUTV__(commandParameters.size());
	__COUTV__(StringMacros::vectorToString(commandParameters));

	//check if logEntry is in parameters
	if(!logEntry.size() && commandParameters.size() &&
	   commandParameters.back().find(COMMAND_PARAM_LOG_ENTRY_PREAMBLE) == 0 &&
	   commandParameters.back().size() > COMMAND_PARAM_LOG_ENTRY_PREAMBLE.size())
	{
		logEntry =
		    commandParameters.back().substr(COMMAND_PARAM_LOG_ENTRY_PREAMBLE.size());
		__COUTV__(logEntry);
	}

	//check if IterationIndex is in parameters (sent by top-level Gateway for subsystem iteration)
	{
		bool foundIterationIndex = false;
		for(size_t i = 0; i < commandParameters.size(); ++i)
		{
			if(commandParameters[i].find(COMMAND_PARAM_ITERATION_INDEX_PREAMBLE) == 0)
			{
				unsigned int remoteIterationIndex = 0;
				try
				{
					remoteIterationIndex = std::stoul(commandParameters[i].substr(
					    COMMAND_PARAM_ITERATION_INDEX_PREAMBLE.length()));
				}
				catch(...)
				{
					__COUT_WARN__ << "Failed to parse IterationIndex from '"
					              << commandParameters[i]
					              << "' -- ignoring malformed parameter." << __E__;
					break;
				}
				__COUT__ << "Received iteration index from top-level Gateway: "
				         << remoteIterationIndex << __E__;

				{
					std::lock_guard<std::mutex> lock(remoteIterationMutex_);
					isRemoteSubsystemIteration_ = true;
					remoteIterationIndex_       = remoteIterationIndex;
				}
				foundIterationIndex = true;
				break;
			}
		}
		if(!foundIterationIndex)
		{
			std::lock_guard<std::mutex> lock(remoteIterationMutex_);
			isRemoteSubsystemIteration_ = false;
			remoteIterationIndex_       = 0;
		}
	}

	/////////////////
	// Validate FSM name (do here because remote commands bypass stateMachineXgiHandler)
	//	if fsm name != active fsm name
	//		only allow, if current state is halted or init
	//		take active fsm name when configured
	//	else, allow
	if(activeStateMachineName_ != "" && activeStateMachineName_ != fsmName)
	{
		__COUT__ << "Validating... currentFSM = " << activeStateMachineName_
		         << ", currentState = " << currentState << ", newFSM = " << fsmName
		         << ", command = " << command << __E__;
		if(currentState != RunControlStateMachine::HALTED_STATE_NAME &&
		   currentState != RunControlStateMachine::INITIAL_STATE_NAME &&
		   currentState != RunControlStateMachine::FAILED_STATE_NAME &&
		   currentState != RunControlStateMachine::SHUTDOWN_STATE_NAME)
		{
			// illegal for this FSM name to attempt transition

			__SS__ << "Error - Can not accept request because the State Machine "
			       << "with window name '" << activeStateMachineWindowName_
			       << "' (UID: " << activeStateMachineName_
			       << ") "
			          "is currently "
			       << "in control of State Machine progress. ";
			ss << "\n\nIn order for this State Machine with window name '"
			   << fsmWindowName << "' (UID: " << fsmName
			   << ") "
			      "to control progress, please transition to "
			   << RunControlStateMachine::HALTED_STATE_NAME << " using the active "
			   << "State Machine '" << activeStateMachineWindowName_
			   << "' (UID: " << activeStateMachineName_
			   << "). Current state = " << currentState << __E__;
			__SS_THROW__;
		}
		else  // clear active state machine
		{
			__COUT__
			    << "Clearing activeStateMachineName_ at safe-transition currentState = "
			    << currentState << __E__;
			activeStateMachineName_       = "";
			activeStateMachineWindowName_ = "";
		}
	}
	//FSM name validated

	stateMachineTransitionUsername_ =
	    username;  // set the username for this transition attempt (used for logging and logbook entry)

	if(logEntry != "")
	{
		logEntry += " (" + StringMacros::getTimestampString(time(0)) + ")";

		if(command == RunControlStateMachine::START_TRANSITION_NAME &&
		   getLastLogEntry(RunControlStateMachine::CONFIGURE_TRANSITION_NAME).size())
		{
			//add configure log entry to start log entry

			logEntry +=
			    "\n\nThe last Configure transition log entry was this:\n" +
			    getLastLogEntry(RunControlStateMachine::CONFIGURE_TRANSITION_NAME);
		}

		bool doLog = false;
		try
		{
			doLog = __ENV__("OTS_LOG_TRANSITION_STARTS") == std::string("1");
		}
		catch(...)
		{ /* ignore errors */
			;
		}

		if(doLog)
			makeSystemLogEntry("Attempting FSM command '" + command + "' from state '" +
			                   currentState + "' with user log entry: " + logEntry);
	}

	setLastLogEntry(command, logEntry);

	SOAPParameters parameters;
	if(command == RunControlStateMachine::CONFIGURE_TRANSITION_NAME)
	{
		activeStateMachineSystemDumpOnConfigure_ =
		    "";  //clear (and set if enabled during configure transition)
		activeStateMachineSystemDumpOnRun_ =
		    "";  //clear (and set if enabled during configure transition)
		activeStateMachineSystemDumpOnRunEnable_ = false,
		activeStateMachineSystemDumpOnConfigureEnable_ =
		    false;  //clear (and set if enabled during configure transition)
		activeStateMachineSystemDumpOnConfigureFilename_ =
		    "";  //clear (and set if enabled during configure transition)
		activeStateMachineSystemDumpOnRunFilename_ =
		    "";  //clear (and set if enabled during configure transition)

		activeStateMachineRequireUserLogOnRun_ = false,
		activeStateMachineRequireUserLogOnConfigure_ =
		    false;  //clear (and set if enabled during configure transition)
		activeStateMachineRunInfoPluginType_ = TableViewColumnInfo::
		    DATATYPE_STRING_DEFAULT;  //clear (and set if enabled during configure transition)

		if(currentState != RunControlStateMachine::HALTED_STATE_NAME &&
		   currentState != RunControlStateMachine::
		                       INITIAL_STATE_NAME)  // check if out of sync command
		{
			__SS__ << "Error - Can only transition to Configured if the current "
			       << "state is Initial or Halted. The current state is '" << currentState
			       << ".' Perhaps your state machine is out of sync, or you need to Halt "
			          "before Configuring."
			       << __E__;
			__SS_THROW__;
		}

		// Note: Original name of the configuration key was RUN_KEY
		// parameters.addParameter("RUN_KEY",CgiDataUtilities::postData(cgi,"ConfigurationAlias"));
		if(commandParameters.size() == 0)
		{
			__SS__ << "Error - Can only transition to Configured if a Configuration "
			          "Alias parameter is provided."
			       << __E__;
			__SS_THROW__;
		}

		// check if configuration dump is enabled on configure transition
		activeStateMachineDumpFormatOnRun_       = "";  //clear
		activeStateMachineDumpFormatOnConfigure_ = "";  //clear
		{
			ConfigurationTree configLinkNode =
			    CorePropertySupervisorBase::theConfigurationManager_
			        ->getSupervisorTableNode(supervisorContextUID_,
			                                 supervisorApplicationUID_);
			if(!configLinkNode.isDisconnected())
			{
				bool doThrow = false;
				try  // for backwards compatibility
				{
					ConfigurationTree fsmLinkNode =
					    configLinkNode.getNode("LinkToStateMachineTable")
					        .getNode(fsmName);

					try
					{
						activeStateMachineRequireUserLogOnConfigure_ =
						    fsmLinkNode
						        .getNode("RequireUserLogInputOnConfigureTransition")
						        .getValue<bool>();
					}
					catch(...)
					{
						;
					}
					try
					{
						activeStateMachineRequireUserLogOnRun_ =
						    fsmLinkNode.getNode("RequireUserLogInputOnRunTransition")
						        .getValue<bool>();
					}
					catch(...)
					{
						;
					}
					try
					{
						activeStateMachineRunAlias_ =
						    fsmLinkNode.getNode("RunDisplayAlias")
						        .getValueWithDefault<std::string>(
						            "Run" /* defaultValue */);
					}
					catch(...)
					{
						activeStateMachineRunAlias_ = "Run";
					}
					try
					{
						activeStateMachineRollOverLogOnConfigure_ =
						    fsmLinkNode.getNode("RollOverLogOnConfigure")
						        .getValueWithDefault<bool>(false /* defaultValue */);
					}
					catch(...)
					{
						activeStateMachineRollOverLogOnConfigure_ = false;
					}
					try
					{
						activeStateMachineRollOverLogOnStart_ =
						    fsmLinkNode.getNode("RollOverLogOnStart")
						        .getValueWithDefault<bool>(false /* defaultValue */);
					}
					catch(...)
					{
						activeStateMachineRollOverLogOnStart_ = false;
					}

					try
					{
						activeStateMachineRunInfoPluginType_ =
						    fsmLinkNode.getNode("RunInfoPluginType")
						        .getValue<std::string>();
					}
					catch(...)  //ignore missing RunInfoPluginType
					{
						__COUT__ << "RunInfoPluginType not defined for FSM name '"
						         << fsmName
						         << "' - please setup a valid run info plugin type to "
						            "enable external Run Number coordination and dumping "
						            "configuration info to an external location."
						         << __E__;
					}

					activeStateMachineSystemDumpOnConfigureEnable_ =
					    fsmLinkNode.getNode("EnableSystemDumpOnConfigureTransition")
					        .getValue<bool>();
					activeStateMachineSystemDumpOnRunEnable_ =
					    fsmLinkNode.getNode("EnableSystemDumpOnRunTransition")
					        .getValue<bool>();

					doThrow = true;  // at this point throw the exception!

					activeStateMachineDumpFormatOnConfigure_ =
					    fsmLinkNode.getNode("SystemDumpOnConfigureFormat")
					        .getValue<std::string>();
					activeStateMachineDumpFormatOnRun_ =
					    fsmLinkNode.getNode("SystemDumpOnRunFormat")
					        .getValue<std::string>();

					std::string dumpFilePath, dumpFileRadix;
					dumpFilePath =
					    fsmLinkNode.getNode("SystemDumpOnConfigureFilePath")
					        .getValueWithDefault<std::string>(__ENV__("OTSDAQ_LOG_DIR"));
					dumpFileRadix = fsmLinkNode.getNode("SystemDumpOnConfigureFileRadix")
					                    .getValueWithDefault<std::string>(
					                        "ConfigTransitionSystemDump");
					activeStateMachineSystemDumpOnConfigureFilename_ =
					    dumpFilePath + "/" + dumpFileRadix;
					dumpFilePath =
					    fsmLinkNode.getNode("SystemDumpOnRunFilePath")
					        .getValueWithDefault<std::string>(__ENV__("OTSDAQ_LOG_DIR"));
					dumpFileRadix =
					    fsmLinkNode.getNode("SystemDumpOnRunFileRadix")
					        .getValueWithDefault<std::string>("RunTransitionSystemDump");
					activeStateMachineSystemDumpOnRunFilename_ =
					    dumpFilePath + "/" + dumpFileRadix;
				}
				catch(
				    std::runtime_error&
				        e)  // throw exception on missing fields if dumpConfiguration set
				{
					__COUTTV__(e.what());

					if(doThrow && (activeStateMachineSystemDumpOnConfigureEnable_ ||
					               activeStateMachineSystemDumpOnRunEnable_))
					{
						__SS__ << "Configuration Dump was enabled, but there are missing "
						          "fields! "
						       << e.what() << __E__;
						__SS_THROW__;
					}
					else
						__COUT_INFO__
						    << "FSM configuration dump Link disconnected at '"
						    << ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME << "/"
						    << supervisorContextUID_ << "/" << supervisorApplicationUID_
						    << "/"
						    << "LinkToStateMachineTable/" << fsmName
						    << "... check the link from the Gateway Superivsor to the "
						       "State Machine table. Looking for FSM fields "
						    << "EnableSystemDumpOnConfigureTransition "
						       "and/or EnableSystemDumpOnRunTransition"
						    << __E__;
				}
			}  //end configuration dump check/handling
			else
				__COUT_INFO__ << "No Gateway Supervisor configuration record found at '"
				              << ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME << "/"
				              << supervisorContextUID_ << "/" << supervisorApplicationUID_
				              << "' - consider adding one to control configuration dumps "
				                 "and state machine properties."
				              << __E__;
		}  //end check if configuration dump is enabled on configure transition

		__COUTTV__(activeStateMachineRequireUserLogOnConfigure_);
		__COUTTV__(activeStateMachineRequireUserLogOnRun_);
		__COUTTV__(activeStateMachineRunAlias_);
		__COUTTV__(activeStateMachineRunInfoPluginType_);
		__COUTTV__(activeStateMachineSystemDumpOnConfigureEnable_);
		__COUTTV__(activeStateMachineSystemDumpOnRunEnable_);
		__COUTTV__(activeStateMachineDumpFormatOnConfigure_);
		__COUTTV__(activeStateMachineDumpFormatOnRun_);
		__COUTTV__(activeStateMachineSystemDumpOnConfigureFilename_);
		__COUTTV__(activeStateMachineSystemDumpOnRunFilename_);
		__COUTTV__(activeStateMachineRollOverLogOnConfigure_);
		__COUTTV__(activeStateMachineRollOverLogOnStart_);

		if(activeStateMachineRequireUserLogOnConfigure_ &&
		   getLastLogEntry(RunControlStateMachine::CONFIGURE_TRANSITION_NAME).size() < 3)
		{
			__SS__ << "Error - the state machine property "
			          "'RequireUserLogInputOnConfigureTransition' has been enabled which "
			          "requires the user to enter "
			          "at least 3 characters of log info to proceed with the Configure "
			          "transition."
			       << __E__;
			__SS_THROW__;
		}

		parameters.addParameter("ConfigurationAlias", commandParameters[0]);

		std::string configurationAlias = parameters.getValue("ConfigurationAlias");
		__COUT__ << "Configure --> Name: ConfigurationAlias Value: " << configurationAlias
		         << __E__;

		// save last used config alias
		std::string fn = ConfigurationManager::LAST_TABLE_GROUP_SAVE_PATH + "/" +
		                 FSM_LAST_GROUP_ALIAS_FILE_START + fsmName + "." +
		                 FSM_USERS_PREFERENCES_FILETYPE;

		__COUT__ << "Save FSM preferences: " << fn << __E__;
		FILE* fp = fopen(fn.c_str(), "w");
		if(!fp)
		{
			__SS__ << ("Could not open file: " + fn) << __E__;
			__SS_THROW__;
		}
		fprintf(fp, "FSM_last_configuration_alias %s", configurationAlias.c_str());
		fclose(fp);

		activeStateMachineName_       = fsmName;
		activeStateMachineWindowName_ = fsmWindowName;

		//Note: Remote Subsystems must respond with Configuration Dump immediately in the udp reply.
		//	Since Configuration Dump can take a long time and since a subsystem might configure multiple times
		//		asynchronously, only collect the pre-assemble configuration dump on the Start transition.
		//	Configure transition Configuration Dumps can be saved independently by subsystem
		//		(including to their own Run Info Plugin) if desired.

		//Based on Config Tree settings, the configuration dump is cached into these in transitionConfiguring():
		//	activeStateMachineSystemDumpOnRun_, activeStateMachineSystemDumpOnConfigure_

		//if ActiveStateMachineSubsystemCommonList is in parameters, then add to message
		bool foundSubsystemCommon = false, foundSubsystemCommonOverride = false;
		for(size_t i = 1; i < commandParameters.size(); ++i)
		{
			if(commandParameters[i].find(COMMAND_PARAM_SUBSYSTEM_COMMON_PREAMBLE) == 0)
			{
				std::string subsystemCommonList = commandParameters[i].substr(
				    COMMAND_PARAM_SUBSYSTEM_COMMON_PREAMBLE.length());
				parameters.addParameter("SubsystemCommonList", subsystemCommonList);
				__COUTV__(subsystemCommonList);
				foundSubsystemCommon = true;
			}
			else if(commandParameters[i].find(
			            COMMAND_PARAM_SUBSYSTEM_COMMON_OVERRIDE_PREAMBLE) == 0)
			{
				std::string subsystemCommonOverrideList = commandParameters[i].substr(
				    COMMAND_PARAM_SUBSYSTEM_COMMON_OVERRIDE_PREAMBLE.length());
				parameters.addParameter("SubsystemCommonOverrideList",
				                        subsystemCommonOverrideList);
				__COUTV__(subsystemCommonOverrideList);
				foundSubsystemCommonOverride = true;
			}
			if(foundSubsystemCommon && foundSubsystemCommonOverride)
				break;
		}

	}  //end Configure transition
	else if(command == RunControlStateMachine::START_TRANSITION_NAME)
	{
		if(currentState !=
		   RunControlStateMachine::CONFIGURED_STATE_NAME)  // check if out of sync command
		{
			__SS__
			    << "Error - Can only transition to Configured if the current "
			    << "state is Halted. Perhaps your state machine is out of sync. "
			    << "(Likely the server was restarted or another user changed the state)"
			    << __E__;
			__SS_THROW__;
		}

		if(activeStateMachineRequireUserLogOnRun_ &&
		   getLastLogEntry(RunControlStateMachine::START_TRANSITION_NAME).size() < 3)
		{
			__SS__
			    << "Error - the state machine property "
			       "'RequireUserLogInputOnRunTransition' has been enabled which requires "
			       "the user to enter "
			       "at least 3 characters of log info to proceed with the Run transition."
			    << __E__;
			__SS_THROW__;
		}

		unsigned long runNumber;
		if(commandParameters.size() == 0)
		{
			runNumber = getNextRunNumber();
			// Check if run number should come from db, if so create run info record into database

			__COUTV__(activeStateMachineRunInfoPluginType_);

			if(activeStateMachineRunInfoPluginType_ !=
			       TableViewColumnInfo::DATATYPE_STRING_DEFAULT &&
			   activeStateMachineRunInfoPluginType_ !=
			       TableViewColumnInfo::DATATYPE_STRING_ALT_DEFAULT &&
			   activeStateMachineRunInfoPluginType_ != "No Run Info Plugin")
			{
				std::unique_ptr<RunInfoVInterface> runInfoInterface = nullptr;
				try
				{
					runInfoInterface.reset(makeRunInfo(
					    activeStateMachineRunInfoPluginType_, activeStateMachineName_));
				}
				catch(...)
				{
					;
				}
				if(runInfoInterface == nullptr)
				{
					__SS__ << "Run Info interface plugin construction failed of type "
					       << activeStateMachineRunInfoPluginType_
					       << " for claiming next run number!" << __E__;
					__SS_THROW__;
				}

				// Claim the next run number from the Run Info plugin (pre-start transition).
				runNumber = runInfoInterface->claimNextRunNumber(
				    activeStateMachineConfigureConditionID_,
				    getLastLogEntry(RunControlStateMachine::START_TRANSITION_NAME));

			}  // end Run Info Plugin handling

			setNextRunNumber(runNumber + 1);
		}
		else
		{
			sscanf(commandParameters[0].c_str(), "%lu", &runNumber);
			// __COUT__(runNumber);
			setNextRunNumber(runNumber + 1);
		}

		setLastLogEntry(command, "Run #" + std::to_string(runNumber) + ": " + logEntry);
		parameters.addParameter(
		    "RunNumber",
		    runNumber);  // will be cached in activeStateMachineRunNumber_ in transitionStarting()

		if(activeStateMachineWindowName_ != fsmWindowName)
		{
			__SUP_COUT__ << "Running fsm window name '" << fsmWindowName
			             << "' is now the active state machine window." << __E__;
			activeStateMachineWindowName_ = fsmWindowName;
		}
	}  //end Start transition
	else if(!(command == RunControlStateMachine::HALT_TRANSITION_NAME ||
	          command == RunControlStateMachine::SHUTDOWN_TRANSITION_NAME ||
	          command == RunControlStateMachine::ERROR_TRANSITION_NAME ||
	          command == RunControlStateMachine::FAIL_TRANSITION_NAME ||
	          command == RunControlStateMachine::STARTUP_TRANSITION_NAME ||
	          command == RunControlStateMachine::INIT_TRANSITION_NAME ||
	          command == RunControlStateMachine::ABORT_TRANSITION_NAME ||
	          command == RunControlStateMachine::PAUSE_TRANSITION_NAME ||
	          command == RunControlStateMachine::RESUME_TRANSITION_NAME ||
	          command == RunControlStateMachine::STOP_TRANSITION_NAME))
	{
		__SS__ << "Error - illegal state machine command received '" << command << ".'"
		       << __E__;
		__SS_THROW__;
	}

	if(activeStateMachineName_ == "")
		__COUT_WARN__
		    << "The active state machine is an empty string, this is allowed for "
		       "backwards compatibility, but may not be intentional! "
		    << "Make sure you or your system admins understand why the active FSM "
		       "name is blank."
		    << "\n\n"
		    << "(Current state = " << currentState << ", attempted command = " << command
		    << ")" << __E__;

	theStateMachine_.setErrorMessage(
	    "");  //clear State Machine error message in prep for transition
	RunControlStateMachine::asyncFailureReceived_ =
	    false;  //clear any stale cancel flag from a previous transition
	xoap::MessageReference message =
	    SOAPUtilities::makeSOAPMessageReference(command, parameters);
	// Maybe we return an acknowledgment that the message has been received and processed
	xoap::MessageReference reply = stateMachineXoapHandler(message);
	// stateMachineWorkLoopManager_.removeProcessedRequests();
	// stateMachineWorkLoopManager_.processRequest(message);

	if(xmldoc)
		xmldoc->addTextElementToData("state_transition_attempted",
		                             "1");  // indicate to GUI transition attempted
	if(out)
		xmldoc->outputXmlDocument((std::ostringstream*)out, false);
	__COUT__ << "FSM state transition launched!" << __E__;

	stateMachineLastCommandInput_ = command;
	return errorStr;
}  // end attemptStateMachineTransition()
catch(...)
{
	__SS__ << "Error - transition '" << command << "' attempt failed!" << __E__;
	try
	{
		throw;
	}
	catch(const std::runtime_error& e)
	{
		ss << "\nHere is the error: " << e.what() << __E__;
	}
	catch(...)
	{
		ss << "Uknown error caught." << __E__;
	}

	__COUT_ERR__ << "\n" << ss.str();

	if(xmldoc)
		xmldoc->addTextElementToData("state_transition_attempted",
		                             "0");  // indicate to GUI transition NOT attempted
	if(xmldoc)
		xmldoc->addTextElementToData(
		    "state_transition_attempted_err",
		    ss.str());  // indicate to GUI transition NOT attempted
	if(out)
		xmldoc->outputXmlDocument(
		    (std::ostringstream*)out, false /*dispStdOut*/, true /*allowWhiteSpace*/);

	return ss.str();
}  // end attemptStateMachineTransition() error handling

//==============================================================================
xoap::MessageReference GatewaySupervisor::stateMachineXoapHandler(
    xoap::MessageReference message)

{
	__COUT__ << "FSM Soap Handler!" << __E__;
	stateMachineWorkLoopManager_.removeProcessedRequests();
	stateMachineWorkLoopManager_.processRequest(message);
	__COUT__ << "Done - FSM Soap Handler!" << __E__;
	return message;
}  // end stateMachineXoapHandler()

//==============================================================================
/// stateMachineThread
///		This asynchronously sends the xoap message to its own RunControlStateMachine
///			(that the Gateway inherits from), which then calls the Gateway
///			transition functions and eventually the broadcast to transition the global
/// state  machine.
bool GatewaySupervisor::stateMachineThread(toolbox::task::WorkLoop* workLoop)
{
	stateMachineSemaphore_.take();
	std::string command =
	    SOAPUtilities::translate(stateMachineWorkLoopManager_.getMessage(workLoop))
	        .getCommand();

	__COUT__ << "Propagating FSM command '" << command
	         << "'... activeStateMachineName_ = " << activeStateMachineName_ << __E__;

	std::string reply = send(allSupervisorInfo_.getGatewayDescriptor(),
	                         stateMachineWorkLoopManager_.getMessage(workLoop));
	stateMachineWorkLoopManager_.report(workLoop, reply, 100, true);

	__COUT__ << "Done with FSM command '" << command << ".' Reply = " << reply << __E__;
	stateMachineSemaphore_.give();

	if(reply == "Fault")
	{
		__SS__ << "Failure to send Workloop transition command '" << command
		       << "!' An error response '" << reply << "' was received." << __E__;
		__COUT_ERR__ << ss.str();
	}
	return false;  // execute once and automatically remove the workloop so in
	               // WorkLoopManager the try workLoop->remove(job_) could be commented
	               // out return true;//go on and then you must do the
	               // workLoop->remove(job_) in WorkLoopManager
}  // end stateMachineThread()

//==============================================================================
void GatewaySupervisor::stateInitial(toolbox::fsm::FiniteStateMachine& /*fsm*/)

{
	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName() << __E__;

}  // end stateInitial()

//==============================================================================
void GatewaySupervisor::statePaused(toolbox::fsm::FiniteStateMachine& /*fsm*/)

{
	auto        pause      = std::chrono::system_clock::now();
	std::time_t pause_time = std::chrono::system_clock::to_time_t(pause);
	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName() << " at "
	         << std::ctime(&pause_time) << __E__;

	if(theStateMachine_.getProvenanceStateName() ==
	   RunControlStateMachine::RUNNING_STATE_NAME)
	{
		try
		{
			ConfigurationTree configLinkNode =
			    CorePropertySupervisorBase::theConfigurationManager_
			        ->getSupervisorTableNode(supervisorContextUID_,
			                                 supervisorApplicationUID_);
			if(!configLinkNode.isDisconnected())
			{
				ConfigurationTree fsmLinkNode =
				    configLinkNode.getNode("LinkToStateMachineTable")
				        .getNode(activeStateMachineName_);
				std::string runInfoPluginType =
				    fsmLinkNode.getNode("RunInfoPluginType").getValue<std::string>();
				__COUTV__(runInfoPluginType);
				if(runInfoPluginType != TableViewColumnInfo::DATATYPE_STRING_DEFAULT &&
				   runInfoPluginType !=
				       TableViewColumnInfo::DATATYPE_STRING_ALT_DEFAULT &&
				   runInfoPluginType != "No Run Info Plugin")
				{
					std::unique_ptr<RunInfoVInterface> runInfoInterface = nullptr;
					try
					{
						runInfoInterface.reset(
						    makeRunInfo(runInfoPluginType, activeStateMachineName_));
					}
					catch(...)
					{
					}

					if(runInfoInterface == nullptr)
					{
						__SS__ << "Run Info interface plugin construction failed of type "
						       << runInfoPluginType << __E__;
						__SS_THROW__;
					}

					runInfoInterface->updateRunInfo(
					    activeStateMachineRunConditionID_,
					    RunInfoVInterface::RunTransitionType::PAUSE,
					    getLastLogEntry(RunControlStateMachine::PAUSE_TRANSITION_NAME));
				}
			}
		}
		catch(const std::runtime_error& e)
		{
			__SS__ << "RUN INFO PAUSE TRANSITION UPDATE INTO DATABASE FAILED!!! "
			       << e.what() << __E__;
			__SS_THROW__;
		}
		catch(...)
		{
			__SS__ << "RUN INFO PAUSE TRANSITION UPDATE INTO DATABASE FAILED!!! "
			       << __E__;
			try
			{
				throw;
			}  //one more try to printout extra info
			catch(const std::exception& e)
			{
				ss << "Exception message: " << e.what();
			}
			catch(...)
			{
			}
			__SS_THROW__;
		}  // End update pause time into run info db
	}      // end update Run Info handling
}  // end statePaused()

//==============================================================================
void GatewaySupervisor::stateRunning(toolbox::fsm::FiniteStateMachine& /*fsm*/)
{
	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName() << __E__;

	if(theStateMachine_.getProvenanceStateName() ==
	   RunControlStateMachine::PAUSED_STATE_NAME)
	{
		__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName()
		         << " coming from resume" << __E__;

		try
		{
			ConfigurationTree configLinkNode =
			    CorePropertySupervisorBase::theConfigurationManager_
			        ->getSupervisorTableNode(supervisorContextUID_,
			                                 supervisorApplicationUID_);
			if(!configLinkNode.isDisconnected())
			{
				ConfigurationTree fsmLinkNode =
				    configLinkNode.getNode("LinkToStateMachineTable")
				        .getNode(activeStateMachineName_);
				std::string runInfoPluginType =
				    fsmLinkNode.getNode("RunInfoPluginType").getValue<std::string>();
				__COUTV__(runInfoPluginType);
				if(runInfoPluginType != TableViewColumnInfo::DATATYPE_STRING_DEFAULT &&
				   runInfoPluginType !=
				       TableViewColumnInfo::DATATYPE_STRING_ALT_DEFAULT &&
				   runInfoPluginType != "No Run Info Plugin")
				{
					std::unique_ptr<RunInfoVInterface> runInfoInterface = nullptr;
					try
					{
						runInfoInterface.reset(
						    makeRunInfo(runInfoPluginType, activeStateMachineName_));
					}
					catch(...)
					{
					}

					if(runInfoInterface == nullptr)
					{
						__SS__ << "Run Info interface plugin construction failed of type "
						       << runInfoPluginType << __E__;
						__SS_THROW__;
					}

					runInfoInterface->updateRunInfo(
					    activeStateMachineRunConditionID_,
					    RunInfoVInterface::RunTransitionType::RESUME,
					    getLastLogEntry(RunControlStateMachine::RESUME_TRANSITION_NAME));
				}
			}
		}
		catch(const std::runtime_error& e)
		{
			__SS__ << "RUN INFO RESUME TRANSITION UPDATE INTO DATABASE FAILED!!! "
			       << e.what() << __E__;
			__SS_THROW__;
		}
		catch(...)
		{
			__SS__ << "RUN INFO RESUME TRANSITION UPDATE INTO DATABASE FAILED!!! "
			       << __E__;
			try
			{
				throw;
			}  //one more try to printout extra info
			catch(const std::exception& e)
			{
				ss << "Exception message: " << e.what();
			}
			catch(...)
			{
			}
			__SS_THROW__;
		}  // End update pause time into run info db
	}      // end update Run Info handling
}  // end stateRunning()

//==============================================================================
void GatewaySupervisor::stateHalted(toolbox::fsm::FiniteStateMachine& /*fsm*/)
{
	__SUP_COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName()
	             << " from " << theStateMachine_.getProvenanceStateName() << __E__;
	__SUP_COUT__ << "Fsm is in transition? "
	             << (theStateMachine_.isInTransition() ? "yes" : "no") << __E__;

	__SUP_COUTV__(
	    SOAPUtilities::translate(theStateMachine_.getCurrentMessage()).getCommand());

	// if coming from Running or Paused, update Run Info	w/HALT
	if(theStateMachine_.getProvenanceStateName() ==
	       RunControlStateMachine::RUNNING_STATE_NAME ||
	   theStateMachine_.getProvenanceStateName() ==
	       RunControlStateMachine::PAUSED_STATE_NAME)
	{
		try
		{
			ConfigurationTree configLinkNode =
			    CorePropertySupervisorBase::theConfigurationManager_
			        ->getSupervisorTableNode(supervisorContextUID_,
			                                 supervisorApplicationUID_);
			if(!configLinkNode.isDisconnected())
			{
				ConfigurationTree fsmLinkNode =
				    configLinkNode.getNode("LinkToStateMachineTable")
				        .getNode(activeStateMachineName_);
				std::string runInfoPluginType =
				    fsmLinkNode.getNode("RunInfoPluginType").getValue<std::string>();
				__SUP_COUTV__(runInfoPluginType);
				if(runInfoPluginType != TableViewColumnInfo::DATATYPE_STRING_DEFAULT &&
				   runInfoPluginType !=
				       TableViewColumnInfo::DATATYPE_STRING_ALT_DEFAULT &&
				   runInfoPluginType != "No Run Info Plugin")
				{
					std::unique_ptr<RunInfoVInterface> runInfoInterface = nullptr;
					try
					{
						runInfoInterface.reset(
						    makeRunInfo(runInfoPluginType, activeStateMachineName_));
					}
					catch(...)
					{
					}

					if(runInfoInterface == nullptr)
					{
						__SS__ << "Run Info interface plugin construction failed of type "
						       << runInfoPluginType << __E__;
						__SS_THROW__;
					}

					runInfoInterface->updateRunInfo(
					    activeStateMachineRunConditionID_,
					    RunInfoVInterface::RunTransitionType::HALT,
					    getLastLogEntry(RunControlStateMachine::HALT_TRANSITION_NAME));
				}
			}
		}
		catch(const std::runtime_error& e)
		{
			__SS__ << "RUN INFO HALT TRANSITION UPDATE INTO DATABASE FAILED!!! "
			       << e.what() << __E__;
			__SS_THROW__;
		}
		catch(...)
		{
			__SS__ << "RUN INFO HALT TRANSITION UPDATE INTO DATABASE FAILED!!! " << __E__;
			try
			{
				throw;
			}  //one more try to printout extra info
			catch(const std::exception& e)
			{
				ss << "Exception message: " << e.what();
			}
			catch(...)
			{
			}
			__SS_THROW__;
		}  // End write run info into db
	}      // end update Run Info handling

	activeStateMachineWindowName_ =
	    "";  //clear window name to indicate that no window (including Iterator) is in control, which allows GUIs to change cleanup strategy
	// do not clear the activeStateMachineName_ here (in case there is some asynchronous halting needed, i.e. multiple Halts from top-level Gateway Supervisor), let it be reassigned on next Configure.
	__SUP_COUT_INFO__ << "Gateway Supervisor is halted." << __E__;
}  // end stateHalted()

//==============================================================================
void GatewaySupervisor::stateConfigured(toolbox::fsm::FiniteStateMachine& /*fsm*/)
{
	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName()
	         << " from " << theStateMachine_.getProvenanceStateName() << __E__;
	__COUT__ << "Fsm is in transition? "
	         << (theStateMachine_.isInTransition() ? "yes" : "no") << __E__;

	__COUTV__(
	    SOAPUtilities::translate(theStateMachine_.getCurrentMessage()).getCommand());

	// if coming from Running or Paused, update Run Info w/STOP
	if(theStateMachine_.getProvenanceStateName() ==
	       RunControlStateMachine::RUNNING_STATE_NAME ||
	   theStateMachine_.getProvenanceStateName() ==
	       RunControlStateMachine::PAUSED_STATE_NAME)
	{
		try
		{
			ConfigurationTree configLinkNode =
			    CorePropertySupervisorBase::theConfigurationManager_
			        ->getSupervisorTableNode(supervisorContextUID_,
			                                 supervisorApplicationUID_);
			if(!configLinkNode.isDisconnected())
			{
				__COUTV__(activeStateMachineName_);
				ConfigurationTree fsmLinkNode =
				    configLinkNode.getNode("LinkToStateMachineTable")
				        .getNode(activeStateMachineName_);
				std::string runInfoPluginType =
				    fsmLinkNode.getNode("RunInfoPluginType").getValue<std::string>();
				__COUTV__(runInfoPluginType);
				if(runInfoPluginType != TableViewColumnInfo::DATATYPE_STRING_DEFAULT &&
				   runInfoPluginType !=
				       TableViewColumnInfo::DATATYPE_STRING_ALT_DEFAULT &&
				   runInfoPluginType != "No Run Info Plugin")
				{
					std::unique_ptr<RunInfoVInterface> runInfoInterface = nullptr;
					try
					{
						runInfoInterface.reset(
						    makeRunInfo(runInfoPluginType, activeStateMachineName_));
					}
					catch(...)
					{
					}

					if(runInfoInterface == nullptr)
					{
						__SS__ << "Run Info interface plugin construction failed of type "
						       << runInfoPluginType << __E__;
						__SS_THROW__;
					}

					runInfoInterface->updateRunInfo(
					    activeStateMachineRunConditionID_,
					    RunInfoVInterface::RunTransitionType::STOP,
					    getLastLogEntry(RunControlStateMachine::STOP_TRANSITION_NAME));
				}
			}
			else
				__COUT__ << "Gateway Supervisor configuration record not found at '"
				         << ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME << "/"
				         << supervisorContextUID_ << "/" << supervisorApplicationUID_
				         << "' - consider adding one to control configuration dumps "
				            "and state machine properties."
				         << __E__;
		}
		catch(const std::runtime_error& e)
		{
			__SS__
			    << "RUN INFO CONFIGURED STATE INSERT OR UPDATE INTO DATABASE FAILED!!! "
			    << e.what() << __E__;
			__SS_THROW__;
		}
		catch(...)
		{
			__SS__
			    << "RUN INFO CONFIGURED STATE INSERT OR UPDATE INTO DATABASE FAILED!!! "
			    << __E__;
			try
			{
				throw;
			}  //one more try to printout extra info
			catch(const std::exception& e)
			{
				ss << "Exception message: " << e.what();
			}
			catch(...)
			{
			}
			__SS_THROW__;
		}  // End write run info into db
	}      // end update Run Info handling

}  // end stateConfigured()

//==============================================================================
void GatewaySupervisor::inError(toolbox::fsm::FiniteStateMachine& /*fsm*/)
{
	__COUT__ << "Error occured - FSM current state: "
	         << "Failed? = " << theStateMachine_.getCurrentStateName()
	         <<  // There may be a race condition here
	    //	when async errors occur (e.g. immediately in running)
	    " from " << theStateMachine_.getProvenanceStateName() << __E__;

	__COUT__
	    << "Error occured on command: "
	    << (SOAPUtilities::translate(theStateMachine_.getCurrentMessage()).getCommand())
	    << __E__;

	// if coming from Running or Paused, update Run Info w/ERROR
	if(theStateMachine_.getProvenanceStateName() ==
	       RunControlStateMachine::RUNNING_STATE_NAME ||
	   theStateMachine_.getProvenanceStateName() ==
	       RunControlStateMachine::PAUSED_STATE_NAME)
	{
		try
		{
			ConfigurationTree configLinkNode =
			    CorePropertySupervisorBase::theConfigurationManager_
			        ->getSupervisorTableNode(supervisorContextUID_,
			                                 supervisorApplicationUID_);
			if(!configLinkNode.isDisconnected())
			{
				ConfigurationTree fsmLinkNode =
				    configLinkNode.getNode("LinkToStateMachineTable")
				        .getNode(activeStateMachineName_);
				std::string runInfoPluginType =
				    fsmLinkNode.getNode("RunInfoPluginType").getValue<std::string>();
				__COUTV__(runInfoPluginType);
				if(runInfoPluginType != TableViewColumnInfo::DATATYPE_STRING_DEFAULT &&
				   runInfoPluginType !=
				       TableViewColumnInfo::DATATYPE_STRING_ALT_DEFAULT &&
				   runInfoPluginType != "No Run Info Plugin")
				{
					std::unique_ptr<RunInfoVInterface> runInfoInterface = nullptr;
					try
					{
						runInfoInterface.reset(
						    makeRunInfo(runInfoPluginType, activeStateMachineName_));
					}
					catch(...)
					{
					}

					if(runInfoInterface == nullptr)
					{
						__SS__ << "Run Info interface plugin construction failed of type "
						       << runInfoPluginType << __E__;
						__SS_THROW__;
					}

					runInfoInterface->updateRunInfo(
					    activeStateMachineRunConditionID_,
					    RunInfoVInterface::RunTransitionType::ERROR,
					    getLastLogEntry(RunControlStateMachine::ERROR_TRANSITION_NAME));
				}
			}
		}
		catch(const std::runtime_error& e)
		{
			__SS__
			    << "RUN INFO ERROR TRANSITION INSERT OR UPDATE INTO DATABASE FAILED!!! "
			    << e.what() << __E__;
			__SS_THROW__;
		}
		catch(...)
		{
			__SS__
			    << "RUN INFO ERROR TRANSITION INSERT OR UPDATE INTO DATABASE FAILED!!! "
			    << __E__;
			try
			{
				throw;
			}  //one more try to printout extra info
			catch(const std::exception& e)
			{
				ss << "Exception message: " << e.what();
			}
			catch(...)
			{
			}
			__SS_THROW__;
		}  // End write run info into db
	}      // end update Run Info handling

}  // end inError()

//==============================================================================
void GatewaySupervisor::enteringError(toolbox::Event::Reference event)
{
	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName()
	         << ", Error event type: " << event->type() << __E__;

	// xdaq 15_14_0_3 broke what() by return c_str() on a temporary string
	//  https://gitlab.cern.ch/cmsos/core/-/blob/release_15_14_0_3/xcept/src/common/Exception.cc

	// extract error message and save for user interface access
	toolbox::fsm::FailedEvent& failedEvent =
	    dynamic_cast<toolbox::fsm::FailedEvent&>(*event);
	xcept::Exception& failedException = failedEvent.getException();
	//__COUT__ << "History of errors: " << failedException.size() << __E__;
	//__COUT__ << "Failed Message: " << failedException.rbegin()->at("message") << __E__;
	//__COUT__ << "Failed Message: " << failedException.message() << __E__;
	//__COUT__ << "Failed Message: " << failedException.what() << __E__;

	bool asyncFailureIdentified = false;
	__SS__;
	// handle async error message differently
	if(RunControlStateMachine::asyncFailureReceived_)
	{
		ss << "\nAn asynchronous failure was encountered."
		   << ".\n\nException:\n"
		   << failedException.message() << __E__;  // rbegin()->at("message") << __E__;
		//<< failedEvent.getException().what() << __E__;
		RunControlStateMachine::asyncFailureReceived_ = false;  // clear async error
		asyncFailureIdentified                        = true;
	}
	else
	{
		ss << "\nFailure performing transition from " << failedEvent.getFromState() << "-"
		   << theStateMachine_.getStateName(failedEvent.getFromState()) << " to "
		   << failedEvent.getToState() << "-"
		   << theStateMachine_.getStateName(failedEvent.getToState())
		   << ".\n\nException:\n"
		   << failedException.message() << __E__;  // rbegin()->at("message") << __E__;
		    //<< failedEvent.getException().what() << __E__;
	}

	__COUT_ERR__ << "\n" << ss.str();

	theStateMachine_.setErrorMessage(ss.str());

	if(!asyncFailureIdentified && theStateMachine_.getCurrentStateName() ==
	                                  RunControlStateMachine::FAILED_STATE_NAME)
		__COUT__ << "Already in failed state, so not broadcasting Error transition again."
		         << __E__;
	else  // move everything else to Error!
	{
		try
		{
			broadcastMessage(SOAPUtilities::makeSOAPMessageReference(
			    RunControlStateMachine::ERROR_TRANSITION_NAME));
		}
		catch(const std::exception& e)
		{
			__COUT_ERR__ << "Error broadcast did not fully complete: " << e.what()
			             << " — Gateway will still enter Failed state." << __E__;
		}
		catch(...)
		{
			__COUT_ERR__ << "Error broadcast did not fully complete (unknown exception)"
			             << " — Gateway will still enter Failed state." << __E__;
		}
	}

	RunControlStateMachine::theProgressBar_.complete();
}  // end enteringError()

//==============================================================================
void GatewaySupervisor::checkForAsyncError()
{
	if(RunControlStateMachine::asyncFailureReceived_)
	{
		__COUTV__(RunControlStateMachine::asyncFailureReceived_);

		XCEPT_RAISE(toolbox::fsm::exception::Exception,
		            RunControlStateMachine::getErrorMessage());
		return;
	}
}  // end checkForAsyncError()

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// FSM State Transition Functions //////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

//==============================================================================
void GatewaySupervisor::transitionConfiguring(toolbox::Event::Reference /* event*/)
try
{
	checkForAsyncError();

	RunControlStateMachine::theProgressBar_.step();

	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName() << __E__;

	std::string configurationAlias =
	    SOAPUtilities::translate(theStateMachine_.getCurrentMessage())
	        .getParameters()
	        .getValue("ConfigurationAlias");

	__COUT__ << "Transition parameter ConfigurationAlias: " << configurationAlias
	         << __E__;

	// Assemble Subsystem Common Table List ----------------
	std::map<std::string /* tableName */, TableVersion> mergeInTables, overrideTables;
	std::string subsystemCommonList, subsystemCommonOverrideList;
	{
		{  //handle common merge-in list
			try
			{
				subsystemCommonList =
				    SOAPUtilities::translate(theStateMachine_.getCurrentMessage())
				        .getParameters()
				        .getValue("SubsystemCommonList");
			}
			catch(...)
			{
				__COUT__ << "Ignoring missing 'SubsystemCommonList' parameter." << __E__;
			}

			if(!subsystemCommonList.empty())
			{
				subsystemCommonList =
				    StringMacros::decodeURIComponent(subsystemCommonList);
				__COUT__ << "Transition parameter SubsystemCommonList: "
				         << subsystemCommonList << __E__;
				StringMacros::getMapFromString(subsystemCommonList, mergeInTables);
				__COUTV__(StringMacros::mapToString(mergeInTables));
			}
		}  //end handle common merge-in list

		{  //handle common override list
			try
			{
				subsystemCommonOverrideList =
				    SOAPUtilities::translate(theStateMachine_.getCurrentMessage())
				        .getParameters()
				        .getValue("SubsystemCommonOverrideList");
			}
			catch(...)
			{
				__COUT__ << "Ignoring missing 'SubsystemCommonOverrideList' parameter."
				         << __E__;
			}

			if(!subsystemCommonOverrideList.empty())
			{
				subsystemCommonOverrideList =
				    StringMacros::decodeURIComponent(subsystemCommonOverrideList);
				__COUT__ << "Transition parameter SubsystemCommonOverrideList: "
				         << subsystemCommonOverrideList << __E__;
				StringMacros::getMapFromString(subsystemCommonOverrideList,
				                               overrideTables);
				__COUTV__(StringMacros::mapToString(overrideTables));
			}
		}  //end handle common override list
	}      // end Assemble Subsystem Common Table List ----------------

	{  //do configuration dump handling
		try
		{
			CorePropertySupervisorBase::theConfigurationManager_
			    ->init();  // completely reset to re-align with any changes
		}
		catch(...)
		{
			__SS__ << "\nTransition to Configuring interrupted! "
			       << "The Configuration Manager could not be initialized." << __E__;
			__SS_THROW__;
		}

		RunControlStateMachine::theProgressBar_.step();

		// Translate the system alias to a group name/key
		try
		{
			theConfigurationTableGroup_ =
			    CorePropertySupervisorBase::theConfigurationManager_
			        ->getTableGroupFromAlias(configurationAlias);
		}
		catch(...)
		{
			__COUT_INFO__
			    << "Exception occurred translating the Configuration System Alias."
			    << __E__;
		}

		if(theConfigurationTableGroup_.second.isInvalid())
		{
			__SS__
			    << "\nTransition to Configuring interrupted! System Configuration Alias '"
			    << configurationAlias
			    << "' could not be translated to a group name and key." << __E__;
			__SS_THROW__;
		}

		__COUT_INFO__
		    << "Attempting Configure transition with Configuration table group name: "
		    << theConfigurationTableGroup_.first
		    << ", key: " << theConfigurationTableGroup_.second << __E__;

		//record attempted configure in group history (might fail to configure after this)
		ConfigurationManager::saveGroupNameAndKey(
		    std::make_pair(configurationAlias, TableGroupKey()),
		    ConfigurationManager::LAST_ATTEMPTED_CONFIGURE_CONFIG_ALIAS_FILE,
		    false /* appendMode */,
		    stateMachineTransitionUsername_);
		ConfigurationManager::saveGroupNameAndKey(
		    std::make_pair(configurationAlias, TableGroupKey()),
		    ConfigurationManager::ATTEMPTED_CONFIGURE_CONFIG_ALIASES_FILE,
		    true /* appendMode */,
		    stateMachineTransitionUsername_);

		ConfigurationManager::saveGroupNameAndKey(
		    theConfigurationTableGroup_,
		    ConfigurationManager::LAST_ATTEMPTED_CONFIGURE_CONFIG_GROUP_FILE,
		    false /* appendMode */,
		    stateMachineTransitionUsername_);
		ConfigurationManager::saveGroupNameAndKey(
		    theConfigurationTableGroup_,
		    ConfigurationManager::ATTEMPTED_CONFIGURE_CONFIGS_FILE,
		    true /* appendMode */,
		    stateMachineTransitionUsername_);

		// load and activate Configuration Alias
		try
		{
			//first get group type - it must be Configuration type!
			std::string groupTypeString;
			CorePropertySupervisorBase::theConfigurationManager_->loadTableGroup(
			    theConfigurationTableGroup_.first,
			    theConfigurationTableGroup_.second,
			    false /*doActivate*/,
			    0 /*groupMembers      */,
			    0 /*progressBar       */,
			    0 /*accumulateWarnings*/,
			    0 /*groupComment      */,
			    0 /*groupAuthor       */,
			    0 /*groupCreateTime   */,
			    true /*doNotLoadMember */,
			    &groupTypeString,
			    0 /*groupAliases */,
			    ConfigurationManager::LoadGroupType::ALL_TYPES /*groupTypeToLoad */,
			    false /* ignoreVersionTracking*/
			);

			RunControlStateMachine::theProgressBar_.step();

			if(groupTypeString != ConfigurationManager::GROUP_TYPE_NAME_CONFIGURATION)
			{
				__SS__ << "Illegal attempted configuration group type. The table group '"
				       << theConfigurationTableGroup_.first << "("
				       << theConfigurationTableGroup_.second << ")' is of type "
				       << groupTypeString << ". It must be "
				       << ConfigurationManager::GROUP_TYPE_NAME_CONFIGURATION << "."
				       << __E__;
				__SS_THROW__;
			}

			//now activate (and merge-in tables)
			{
				ConfigurationManager::ConfigureTransitionGuard configureGuard(
				    CorePropertySupervisorBase::theConfigurationManager_);
				CorePropertySupervisorBase::theConfigurationManager_->loadTableGroup(
				    theConfigurationTableGroup_.first,
				    theConfigurationTableGroup_.second,
				    true /*doActivate*/,
				    0 /*groupMembers      */,
				    0 /*progressBar       */,
				    0 /*accumulateWarnings*/,
				    0 /*groupComment      */,
				    0 /*groupAuthor       */,
				    0 /*groupCreateTime   */,
				    false /*doNotLoadMember */,
				    0 /*groupTypeString */,
				    0 /*groupAliases */,
				    ConfigurationManager::LoadGroupType::ALL_TYPES,
				    true /*ignoreVersionTracking*/,
				    mergeInTables /* mergeInTables */,
				    overrideTables /* overrideTables */
				);
			}

			__COUT__ << "Done loading and activating Configuration Alias (and merging-in "
			            "tables)."
			         << __E__;

			{
				std::lock_guard<std::mutex> lock(contextCommonMutex_);
				appliedContextCommonList_ = "";
				appliedContextCommonOverrideList_ = "";
			}

			RunControlStateMachine::theProgressBar_.step();

			// mark the translated group as the last activated group
			std::pair<std::string /*group name*/, TableGroupKey> activatedGroup(
			    std::string(theConfigurationTableGroup_.first),
			    theConfigurationTableGroup_.second);

			ConfigurationManager::saveGroupNameAndKey(
			    activatedGroup,
			    ConfigurationManager::LAST_ACTIVATED_CONFIG_GROUP_FILE,
			    false /* appendMode */,
			    stateMachineTransitionUsername_);
			ConfigurationManager::saveGroupNameAndKey(
			    activatedGroup,
			    ConfigurationManager::ACTIVATED_CONFIGS_FILE,
			    true /* appendMode */,
			    stateMachineTransitionUsername_);

			__COUT__ << "Done marking activated Configuration Alias." << __E__;
		}
		catch(const std::runtime_error& e)
		{
			__SS__
			    << "\nTransition to Configuring interrupted! System Configuration Alias "
			    << configurationAlias << " was translated to "
			    << theConfigurationTableGroup_.first << " ("
			    << theConfigurationTableGroup_.second
			    << ") but could not be loaded and initialized." << __E__;
			ss << "\n\nHere was the error: " << e.what()
			   << "\n\nTo help debug this problem, try activating this group in the "
			      "Configuration "
			      "GUI "
			   << " and detailed errors will be shown." << __E__;
			__SS_THROW__;
		}
		catch(...)
		{
			__SS__
			    << "\nTransition to Configuring interrupted! System Configuration Alias "
			    << configurationAlias << " was translated to "
			    << theConfigurationTableGroup_.first << " ("
			    << theConfigurationTableGroup_.second
			    << ") but could not be loaded and initialized." << __E__;
			try
			{
				throw;
			}  //one more try to printout extra info
			catch(const std::exception& e)
			{
				ss << "Exception message: " << e.what();
			}
			catch(...)
			{
			}
			ss << "\n\nTo help debug this problem, try activating this group in the "
			      "Configuration "
			      "GUI "
			   << " and detailed errors will be shown." << __E__;
			__SS_THROW__;
		}

		RunControlStateMachine::theProgressBar_.step();

		//at this point Configuration Tree is fully loaded

		//handle configuration dump if enabled on configure transition
		try  // errors in dump are not tolerated
		{
			//get/cache Run transition dump
			if(activeStateMachineSystemDumpOnRunEnable_ ||
			   ((activeStateMachineRunInfoPluginType_ !=
			         TableViewColumnInfo::DATATYPE_STRING_DEFAULT &&
			     activeStateMachineRunInfoPluginType_ !=
			         TableViewColumnInfo::DATATYPE_STRING_ALT_DEFAULT &&
			     activeStateMachineRunInfoPluginType_ != "No Run Info Plugin")))
			{
				__COUT_INFO__
				    << "Caching the System Configuration Dump for the Run transition..."
				    << __E__;

				// dump configuration
				std::stringstream dumpSs;
				CorePropertySupervisorBase::theConfigurationManager_
				    ->dumpActiveConfiguration(
				        "",  //dumpFilePath + "/" + dumpFileRadix + "_" + std::to_string(time(0)) + ".dump",
				        activeStateMachineDumpFormatOnRun_,
				        configurationAlias,
				        subsystemCommonList,
				        subsystemCommonOverrideList,
				        getLastLogEntry(
				            RunControlStateMachine::CONFIGURE_TRANSITION_NAME),
				        theWebUsers_.getActiveUsernamesString(),
				        theStateMachine_.getCurrentStateName(),
				        dumpSs);

				activeStateMachineSystemDumpOnRun_ = dumpSs.str();

				__COUT__ << "Active State Machine Config Dump on Run " << __E__;
				__COUTTV__(activeStateMachineSystemDumpOnRun_) << __E__;
				__COUT_MULTI__(TLVL_SystemDump, activeStateMachineSystemDumpOnRun_);
			}
			else
				__COUT_INFO__
				    << "Not caching the System Configuration Dump on the Run transition."
				    << __E__;

			//get/cache Configuration transition dump
			if(activeStateMachineSystemDumpOnConfigureEnable_ ||
			   ((activeStateMachineRunInfoPluginType_ !=
			         TableViewColumnInfo::DATATYPE_STRING_DEFAULT &&
			     activeStateMachineRunInfoPluginType_ !=
			         TableViewColumnInfo::DATATYPE_STRING_ALT_DEFAULT &&
			     activeStateMachineRunInfoPluginType_ != "No Run Info Plugin")))
			{
				__COUT_INFO__ << "Caching the System Configuration Dump for the "
				                 "Configure transition..."
				              << __E__;

				// dump configuration
				std::stringstream dumpSs;
				CorePropertySupervisorBase::theConfigurationManager_
				    ->dumpActiveConfiguration(
				        "",  //dumpFilePath + "/" + dumpFileRadix + "_" + std::to_string(time(0)) + ".dump",
				        activeStateMachineDumpFormatOnConfigure_,
				        configurationAlias,
				        subsystemCommonList,
				        subsystemCommonOverrideList,
				        getLastLogEntry(
				            RunControlStateMachine::CONFIGURE_TRANSITION_NAME),
				        theWebUsers_.getActiveUsernamesString(),
				        theStateMachine_.getCurrentStateName(),
				        dumpSs);

				activeStateMachineSystemDumpOnConfigure_ = dumpSs.str();

				__COUT__ << "Active State Machine Config Dump on Configure " << __E__;
				__COUTTV__(activeStateMachineSystemDumpOnConfigure_) << __E__;
				__COUT_MULTI__(TLVL_SystemDump, activeStateMachineSystemDumpOnConfigure_);
			}
			else
				__COUT_INFO__ << "Not caching the System Configuration Dump on the "
				                 "Configure transition."
				              << __E__;

		}  //end handle configuration dump if enabled on configure transition
		catch(const std::runtime_error& e)
		{
			__SS__ << "Error encountered during system configuration dump. Here is the "
			          "error: "
			       << e.what();
			__SS_THROW__;
		}
		catch(...)
		{
			__SS__ << "Unknown error encountered during system configuration dump.";
			__SS_THROW__;
		}
	}  //end configuration dump handling

	RunControlStateMachine::theProgressBar_.step();

	__COUT__ << "Configuration table group name: " << theConfigurationTableGroup_.first
	         << " key: " << theConfigurationTableGroup_.second << __E__;

	//Roll over log file if enabled
	if(activeStateMachineRollOverLogOnConfigure_)
	{
		__COUT_INFO__ << "Rolling over log file on Configure transition..." << __E__;
		std::stringstream runSs;
		runSs << "LOG_ROLLOVER";
		runSs << ";"
		      << "Configure"
		      << "_" << theConfigurationTableGroup_.first << "_v"
		      << theConfigurationTableGroup_.second;

		GatewaySupervisor::launchStartOTSCommand(
		    runSs.str(), CorePropertySupervisorBase::theConfigurationManager_);
	}

	RunControlStateMachine::theProgressBar_.step();

	// make logbook entry
	bool doLog = false;
	try
	{
		doLog = __ENV__("OTS_LOG_TRANSITION_STARTS") == std::string("1");
	}
	catch(...)
	{ /* ignore errors */
		;
	}
	if(doLog)
	{
		std::stringstream ss;
		ss << "Configuring with System Configuration Alias '" << configurationAlias
		   << "' which translates to " << theConfigurationTableGroup_.first << "("
		   << theConfigurationTableGroup_.second << "). Active Context Group "
		   << CorePropertySupervisorBase::theConfigurationManager_->getActiveGroupName(
		          ConfigurationManager::GroupType::CONTEXT_TYPE)
		   << "("
		   << CorePropertySupervisorBase::theConfigurationManager_->getActiveGroupKey(
		          ConfigurationManager::GroupType::CONTEXT_TYPE)
		   << ").";

		if(getLastLogEntry(RunControlStateMachine::CONFIGURE_TRANSITION_NAME) != "")
			ss << "\n\n-----------------\nUser log entry:\n"
			   << getLastLogEntry(RunControlStateMachine::CONFIGURE_TRANSITION_NAME)
			   << "\n-----------------\n";
		else
			ss << " No user log entry.";
		makeSystemLogEntry(ss.str());
	}  // end make logbook entry

	RunControlStateMachine::theProgressBar_.step();

	try
	{
		CorePropertySupervisorBase::theConfigurationManager_->dumpMacroMakerModeFhicl();
	}
	catch(...)  // ignore error for now
	{
		__COUT_ERR__ << "Failed to dump MacroMaker mode fhicl." << __E__;
	}

	RunControlStateMachine::theProgressBar_.step();

	// update Macro Maker front end list
	if(CorePropertySupervisorBase::allSupervisorInfo_.getAllMacroMakerTypeSupervisorInfo()
	       .size())
	{
		__COUT__ << "Initializing Macro Maker." << __E__;
		xoap::MessageReference message =
		    SOAPUtilities::makeSOAPMessageReference("FECommunication");

		SOAPParameters parameters;
		parameters.addParameter("type", "initFElist");
		parameters.addParameter("groupName", theConfigurationTableGroup_.first);
		parameters.addParameter("groupKey",
		                        theConfigurationTableGroup_.second.toString());
		if(!subsystemCommonList.empty())
			parameters.addParameter("SubsystemCommonList", subsystemCommonList);
		if(!subsystemCommonOverrideList.empty())
			parameters.addParameter("SubsystemCommonOverrideList",
			                        subsystemCommonOverrideList);
		SOAPUtilities::addParameters(message, parameters);

		__COUT__ << "Sending FE communication: " << SOAPUtilities::translate(message)
		         << __E__;

		xoap::MessageReference replyMessage = SOAPMessenger::sendWithSOAPReply(
		    CorePropertySupervisorBase::allSupervisorInfo_
		        .getAllMacroMakerTypeSupervisorInfo()
		        .begin()
		        ->second.getDescriptor(),
		    message);
		std::string reply =
		    SOAPUtilities::receive(replyMessage);  //get primary message response

		__COUT__ << "Macro Maker init reply: " << reply << __E__;
		if(reply == "Error")
		{
			__SS__ << "\nTransition to Configuring interrupted! There was an error "
			          "identified initializing Macro Maker.\n\n ";

			//extract full error message
			SOAPParameters retParameters("Error");
			SOAPUtilities::receive(replyMessage, retParameters);
			ss << "Here was the error: " << retParameters.getValue("Error") << __E__;

			__COUT_ERR__ << "\n" << ss.str();
			XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
			return;
		}
	}  // end update Macro Maker front end list

	StringMacros::systemVariables_["ActiveStateMachine"]["name"] =
	    activeStateMachineName_;
	StringMacros::systemVariables_["ActiveStateMachine"]["windowName"] =
	    activeStateMachineWindowName_;
	StringMacros::systemVariables_["ActiveStateMachine"]["runAlias"] =
	    activeStateMachineRunAlias_;
	__SUP_COUTV__(StringMacros::mapToString(StringMacros::systemVariables_));

	SOAPParameters parameters;
	parameters.addParameter("ConfigurationTableGroupName",
	                        theConfigurationTableGroup_.first);
	parameters.addParameter("ConfigurationTableGroupKey",
	                        theConfigurationTableGroup_.second.toString());
	parameters.addParameter("ActiveStateMachineName", activeStateMachineName_);
	parameters.addParameter("ActiveStateMachineWindowName",
	                        activeStateMachineWindowName_);
	parameters.addParameter("ActiveStateMachineRunAlias", activeStateMachineRunAlias_);

	RunControlStateMachine::theProgressBar_.step();

	xoap::MessageReference message = theStateMachine_.getCurrentMessage();
	SOAPUtilities::addParameters(message, parameters);
	//Note: Must save configuration dump after this point!! In case there are remote subsystems responding with string
	broadcastMessage(message);  // ---------------------------------- broadcast!
	RunControlStateMachine::theProgressBar_.step();

	if(activeStateMachineSystemDumpOnConfigureEnable_)
	{
		//write local configuration dump file
		std::string fullfilename = activeStateMachineSystemDumpOnConfigureFilename_ +
		                           "_" + std::to_string(time(0)) + ".dump";
		FILE* fp = fopen(fullfilename.c_str(), "w");
		if(!fp)
		{
			__SS__ << "Configuration dump failed to file: " << fullfilename << __E__;
			__SS_THROW__;
		}

		//(a la ConfigurationManager::dumpActiveConfiguration)
		fullfilename = __ENV__("HOSTNAME") + std::string(":") + fullfilename;
		fprintf(
		    fp, "Original location of dump:               %s\n", fullfilename.c_str());

		if(activeStateMachineSystemDumpOnConfigure_.size())
			fwrite(&activeStateMachineSystemDumpOnConfigure_[0],
			       1,
			       activeStateMachineSystemDumpOnConfigure_.size(),
			       fp);
		__COUT__ << "Wrote configuration dump of char count "
		         << activeStateMachineSystemDumpOnConfigure_.size()
		         << " to file: " << fullfilename << __E__;

		fclose(fp);

		__COUT_INFO__ << "Configure transition Configuration Dump saved to file: "
		              << fullfilename << __E__;
	}  //done with local config dump
	RunControlStateMachine::theProgressBar_.step();

	// Check if Run Plugin is defined and, if so, create a new condition record into database
	// leave as repeated code in case dumpFormat is different for Run Plugin (in the future)
	activeStateMachineConfigureConditionID_ = -1;  //clear attempted Run Info Plugin use
	try
	{
		if(activeStateMachineRunInfoPluginType_ !=
		       TableViewColumnInfo::DATATYPE_STRING_DEFAULT &&
		   activeStateMachineRunInfoPluginType_ !=
		       TableViewColumnInfo::DATATYPE_STRING_ALT_DEFAULT &&
		   activeStateMachineRunInfoPluginType_ != "No Run Info Plugin")
		{
			__COUT_INFO__ << "Instantiating Run Info plugin '"
			              << activeStateMachineRunInfoPluginType_
			              << "' to insert Configure run condition entry." << __E__;
			std::unique_ptr<RunInfoVInterface> runInfoInterface = nullptr;
			try
			{
				runInfoInterface.reset(makeRunInfo(activeStateMachineRunInfoPluginType_,
				                                   activeStateMachineName_));
			}
			catch(const std::runtime_error& e)
			{
				__SS__ << "Run Info interface plugin construction failed of type "
				       << activeStateMachineRunInfoPluginType_
				       << " with error: " << e.what() << __E__;
				__SS_THROW__;
			}
			catch(const std::exception& e)
			{
				__SS__ << "Run Info interface plugin construction failed of type "
				       << activeStateMachineRunInfoPluginType_
				       << " with error: " << e.what() << __E__;
				__SS_THROW__;
			}
			catch(...)
			{
				__SS__ << "Run Info interface plugin construction failed of type "
				       << activeStateMachineRunInfoPluginType_
				       << " with unknown error. Run Condition record of char size "
				       << activeStateMachineSystemDumpOnConfigure_.size() << __E__;
				__SS_THROW__;
			}
			if(runInfoInterface == nullptr)
			{
				__SS__ << "Run Info interface plugin construction failed of type "
				       << activeStateMachineRunInfoPluginType_
				       << " for inserting Run Condition record of char size "
				       << activeStateMachineSystemDumpOnConfigure_.size() << __E__;
				__SS_THROW__;
			}

			//in case user wants, insert local configuration blob at each configure transition
			activeStateMachineConfigureConditionID_ =
			    runInfoInterface->insertConfigureCondition(
			        activeStateMachineSystemDumpOnConfigure_,
			        getLastLogEntry(RunControlStateMachine::CONFIGURE_TRANSITION_NAME));

		}  // end Run Info Plugin handling
	}
	catch(const std::runtime_error& e)
	{
		__SS__ << "RUN CONDITION INSERT INTO DATABASE FAILED!!! " << e.what() << __E__;
		__SS_THROW__;
	}
	catch(...)
	{
		__SS__ << "RUN CONDITION INSERT INTO DATABASE FAILED!!! " << __E__;
		try
		{
			throw;
		}  //one more try to printout extra info
		catch(const std::exception& e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...)
		{
		}
		__SS_THROW__;
	}  // End write run condition into db
	RunControlStateMachine::theProgressBar_.step();

	// save last configured group names/keys
	{
		ConfigurationManager::saveGroupNameAndKey(
		    std::make_pair(configurationAlias, TableGroupKey()),
		    ConfigurationManager::LAST_CONFIGURED_CONFIG_ALIAS_FILE,
		    false /* appendMode */,
		    stateMachineTransitionUsername_);
		ConfigurationManager::saveGroupNameAndKey(
		    std::make_pair(configurationAlias, TableGroupKey()),
		    ConfigurationManager::CONFIGURED_CONFIG_ALIASES_FILE,
		    true /* appendMode */,
		    stateMachineTransitionUsername_);
		ConfigurationManager::saveGroupNameAndKey(
		    std::make_pair(configurationAlias, TableGroupKey()),
		    ConfigurationManager::CONFIGURED_OR_STARTED_CONFIG_ALIASES_FILE,
		    true /* appendMode */,
		    stateMachineTransitionUsername_);

		ConfigurationManager::saveGroupNameAndKey(
		    theConfigurationTableGroup_,
		    ConfigurationManager::LAST_CONFIGURED_CONFIG_GROUP_FILE,
		    false /* appendMode */,
		    stateMachineTransitionUsername_);
		ConfigurationManager::saveGroupNameAndKey(
		    theConfigurationTableGroup_,
		    ConfigurationManager::CONFIGURED_CONFIGS_FILE,
		    true /* appendMode */,
		    stateMachineTransitionUsername_);
		ConfigurationManager::saveGroupNameAndKey(
		    theConfigurationTableGroup_,
		    ConfigurationManager::CONFIGURED_OR_STARTED_CONFIGS_FILE,
		    true /* appendMode */,
		    stateMachineTransitionUsername_);

		auto activeGroupMap =
		    CorePropertySupervisorBase::theConfigurationManager_->getActiveTableGroups();

		ConfigurationManager::saveGroupNameAndKey(
		    activeGroupMap.at(ConfigurationManager::GROUP_TYPE_NAME_CONTEXT),
		    ConfigurationManager::LAST_CONFIGURED_CONTEXT_GROUP_FILE,
		    false /* appendMode */,
		    stateMachineTransitionUsername_);
		ConfigurationManager::saveGroupNameAndKey(
		    activeGroupMap.at(ConfigurationManager::GROUP_TYPE_NAME_CONTEXT),
		    ConfigurationManager::CONFIGURED_CONTEXTS_FILE,
		    true /* appendMode */,
		    stateMachineTransitionUsername_);
		ConfigurationManager::saveGroupNameAndKey(
		    activeGroupMap.at(ConfigurationManager::GROUP_TYPE_NAME_CONTEXT),
		    ConfigurationManager::CONFIGURED_OR_STARTED_CONTEXTS_FILE,
		    true /* appendMode */,
		    stateMachineTransitionUsername_);

		ConfigurationManager::saveGroupNameAndKey(
		    activeGroupMap.at(ConfigurationManager::GROUP_TYPE_NAME_CONTEXT),
		    ConfigurationManager::LAST_CONFIGURED_BACKBONE_GROUP_FILE,
		    false /* appendMode */,
		    stateMachineTransitionUsername_);
		ConfigurationManager::saveGroupNameAndKey(
		    activeGroupMap.at(ConfigurationManager::GROUP_TYPE_NAME_BACKBONE),
		    ConfigurationManager::CONFIGURED_BACKBONES_FILE,
		    true /* appendMode */,
		    stateMachineTransitionUsername_);
		ConfigurationManager::saveGroupNameAndKey(
		    activeGroupMap.at(ConfigurationManager::GROUP_TYPE_NAME_BACKBONE),
		    ConfigurationManager::CONFIGURED_OR_STARTED_BACKBONES_FILE,
		    true /* appendMode */,
		    stateMachineTransitionUsername_);

		if(activeGroupMap.at(ConfigurationManager::GROUP_TYPE_NAME_ITERATE)
		       .second.isValid())
		{
			ConfigurationManager::saveGroupNameAndKey(
			    activeGroupMap.at(ConfigurationManager::GROUP_TYPE_NAME_ITERATE),
			    ConfigurationManager::LAST_CONFIGURED_ITERATE_GROUP_FILE,
			    false /* appendMode */,
			    stateMachineTransitionUsername_);
			ConfigurationManager::saveGroupNameAndKey(
			    activeGroupMap.at(ConfigurationManager::GROUP_TYPE_NAME_ITERATE),
			    ConfigurationManager::CONFIGURED_ITERATES_FILE,
			    true /* appendMode */,
			    stateMachineTransitionUsername_);
			ConfigurationManager::saveGroupNameAndKey(
			    activeGroupMap.at(ConfigurationManager::GROUP_TYPE_NAME_ITERATE),
			    ConfigurationManager::CONFIGURED_OR_STARTED_ITERATES_FILE,
			    true /* appendMode */,
			    stateMachineTransitionUsername_);
		}
	}  //end save last configured group names/keys

	RunControlStateMachine::theProgressBar_.step();

	activeStateMachineConfigurationAlias_ = configurationAlias;
	doLog                                 = true;  //default to true
	try
	{
		doLog = __ENV__("OTS_LOG_INTERMEDIATE_STATES") == std::string("1");
	}
	catch(...)
	{ /* ignore errors */
		;
	}
	if(doLog)
	{
		std::stringstream ss;
		ss << "Configured with System Configuration Alias '"
		   << activeStateMachineConfigurationAlias_ << "' which translates to "
		   << theConfigurationTableGroup_.first << "("
		   << theConfigurationTableGroup_.second << "). Active Context Group "
		   << CorePropertySupervisorBase::theConfigurationManager_->getActiveGroupName(
		          ConfigurationManager::GroupType::CONTEXT_TYPE)
		   << "("
		   << CorePropertySupervisorBase::theConfigurationManager_->getActiveGroupKey(
		          ConfigurationManager::GroupType::CONTEXT_TYPE)
		   << ").";

		if(getLastLogEntry(RunControlStateMachine::CONFIGURE_TRANSITION_NAME) != "")
			ss << "\n\n-----------------\nUser log entry:\n"
			   << getLastLogEntry(RunControlStateMachine::CONFIGURE_TRANSITION_NAME)
			   << "\n-----------------\n";
		else
			ss << " No user log entry.";

		//insert system and remote subsystem status/detail
		{
			ss << "\n\n~~~ System Status and Detail ~~~\n";
			for(const auto& it : allSupervisorInfo_.getAllSupervisorInfo())
			{
				const auto& appInfo = it.second;
				if(appInfo.getClass() != XDAQContextTable::GATEWAY_SUPERVISOR_CLASS)
					continue;  //only give Gateway status
				ss << "\tStatus: " << appInfo.getStatus() << __E__
				   << "\tDetail: " << appInfo.getDetail() << __E__;
			}

			//also return remote gateways as apps
			std::vector<GatewaySupervisor::RemoteGatewayInfo> remoteApps;  //local copy
			{  //lock for remainder of scope
				std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
				remoteApps = remoteGatewayApps_;
			}

			if(remoteApps.size())
			{
				ss << "\n\n~~~ Subsystem Status and Detail ~~~\n";

				for(const auto& remoteApp : remoteApps)
				{
					const auto& appInfo = remoteApp.appInfo;
					ss << "Subsystem Name: " << appInfo.name << __E__
					   << "\tStatus: " << appInfo.status << __E__
					   << "\tDetail: " << appInfo.detail << __E__;
				}
			}
		}

		makeSystemLogEntry(ss.str());
	}
	__COUT__ << "Done configuring." << __E__;
	RunControlStateMachine::theProgressBar_.complete();
}  // end transitionConfiguring()
catch(const xdaq::exception::Exception& e)  // due to xoap send failure
{
	__SS__ << "\nTransition to Configuring interrupted! There was a system communication "
	          "error "
	          "identified. "
	       << __E__ << e.what();
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}
catch(std::runtime_error& e)
{
	__SS__ << "\nTransition to Configuring interrupted! There was an error "
	          "identified. "
	       << __E__ << e.what();
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}
catch(toolbox::fsm::exception::Exception& e)
{
	throw;  // just rethrow exceptions of already the correct type
}
catch(...)
{
	__SS__ << "\nTransition to Configuring interrupted! There was an unknown error "
	          "identified. "
	       << __E__;
	try
	{
		throw;
	}  //one more try to printout extra info
	catch(const std::exception& e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...)
	{
	}
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}  // end transitionConfiguring() catch

//==============================================================================
void GatewaySupervisor::transitionHalting(toolbox::Event::Reference /*event*/)
try
{
	checkForAsyncError();

	RunControlStateMachine::theProgressBar_.step();

	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName() << __E__;

	bool doLog = false;
	try
	{
		doLog = __ENV__("OTS_LOG_TRANSITION_STARTS") == std::string("1");
	}
	catch(...)
	{ /* ignore errors */
		;
	}
	bool doLogIntermediate = false;
	try
	{
		doLogIntermediate = __ENV__("OTS_LOG_INTERMEDIATE_STATES") == std::string("1");
	}
	catch(...)
	{ /* ignore errors */
		;
	}

	if(doLog && doLogIntermediate)
		makeSystemLogEntry("System halting.");

	RunControlStateMachine::theProgressBar_.step();

	broadcastMessage(theStateMachine_.getCurrentMessage());

	if(doLogIntermediate)
		makeSystemLogEntry("System halted.");

	// Auto-compress stale logs if threshold is set and > 24 hours
	try
	{
		const std::string envThreshold = __ENV__("OTSDAQ_LOG_COMPRESS_THRESHOLD");
		if(!envThreshold.empty())
		{
			int64_t compressThresholdSeconds = std::stoll(envThreshold);
			if(compressThresholdSeconds > 86400)
			{
				std::string cmd = "ots -lxz " + std::to_string(compressThresholdSeconds) +
				                  " seconds --logcompress-noprompt";
				__COUT__ << "Auto-compressing stale logs: " << cmd << __E__;
				std::thread([cmd]() {
					try
					{
						std::string result = StringMacros::exec(cmd.c_str());
						__COUT__ << "Auto-compress result:\n" << result << __E__;
					}
					catch(const std::exception& e)
					{
						__COUT_ERR__ << "Auto-compress failed: " << e.what() << __E__;
					}
				}).detach();
			}
		}
	}
	catch(const std::exception& e)
	{
		__COUT_WARN__
		    << "Log auto-compress skipped (invalid OTSDAQ_LOG_COMPRESS_THRESHOLD): "
		    << e.what() << __E__;
	}

	__COUT__ << "Done halting." << __E__;
	RunControlStateMachine::theProgressBar_.complete();
}  // end transitionHalting()
catch(const xdaq::exception::Exception& e)  // due to xoap send failure
{
	__SS__
	    << "\nTransition to Halting interrupted! There was a system communication error "
	       "identified. "
	    << __E__ << e.what();
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}
catch(std::runtime_error& e)
{
	__SS__ << "\nTransition to Halting interrupted! There was an error "
	          "identified. "
	       << __E__ << e.what();
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}
catch(toolbox::fsm::exception::Exception& e)
{
	throw;  // just rethrow exceptions of already the correct type
}
catch(...)
{
	__SS__ << "\nTransition to Halting interrupted! There was an unknown error "
	          "identified. "
	       << __E__;
	try
	{
		throw;
	}  //one more try to printout extra info
	catch(const std::exception& e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...)
	{
	}
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}  // end transitionHalting() catch

//==============================================================================
void GatewaySupervisor::transitionShuttingDown(toolbox::Event::Reference /*event*/)
try
{
	checkForAsyncError();

	__COUT__ << "transitionShuttingDown -- Fsm current state: "
	         << theStateMachine_.getCurrentStateName()
	         << " message: " << theStateMachine_.getCurrentStateName() << __E__;

	RunControlStateMachine::theProgressBar_.step();
	bool doLog = false;
	try
	{
		doLog = __ENV__("OTS_LOG_TRANSITION_STARTS") == std::string("1");
	}
	catch(...)
	{ /* ignore errors */
		;
	}
	bool doLogIntermediate = false;
	try
	{
		doLogIntermediate = __ENV__("OTS_LOG_INTERMEDIATE_STATES") == std::string("1");
	}
	catch(...)
	{ /* ignore errors */
		;
	}

	if(doLog && doLogIntermediate)
		makeSystemLogEntry("System shutting down.");
	RunControlStateMachine::theProgressBar_.step();

	// kill all non-gateway contexts
	GatewaySupervisor::launchStartOTSCommand(
	    "OTS_APP_SHUTDOWN", CorePropertySupervisorBase::theConfigurationManager_);
	RunControlStateMachine::theProgressBar_.step();

	// important to give time for StartOTS script to recognize command (before user does
	// Startup again)
	for(int i = 0; i < 5; ++i)
	{
		sleep(1);
		RunControlStateMachine::theProgressBar_.step();
	}

	broadcastMessage(theStateMachine_.getCurrentMessage());

	if(doLogIntermediate)
		makeSystemLogEntry("System shutdown complete.");
	__COUT__ << "Done shutting down." << __E__;
	RunControlStateMachine::theProgressBar_.complete();
}  // end transitionShuttingDown()
catch(const xdaq::exception::Exception& e)  // due to xoap send failure
{
	__SS__ << "\nTransition to Shutting Down interrupted! There was a system "
	          "communication error "
	          "identified. "
	       << __E__ << e.what();
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}
catch(std::runtime_error& e)
{
	__SS__ << "\nTransition to Shutting Down interrupted! There was an error "
	          "identified. "
	       << __E__ << e.what();
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}
catch(toolbox::fsm::exception::Exception& e)
{
	throw;  // just rethrow exceptions of already the correct type
}
catch(...)
{
	__SS__ << "\nTransition to Shutting Down interrupted! There was an unknown error "
	          "identified. "
	       << __E__;
	try
	{
		throw;
	}  //one more try to printout extra info
	catch(const std::exception& e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...)
	{
	}
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}  // end transitionShuttingDown() catch

//==============================================================================
void GatewaySupervisor::transitionStartingUp(toolbox::Event::Reference /*event*/)
try
{
	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName() << __E__;

	RunControlStateMachine::theProgressBar_.step();
	bool doLog = false;
	try
	{
		doLog = __ENV__("OTS_LOG_TRANSITION_STARTS") == std::string("1");
	}
	catch(...)
	{ /* ignore errors */
		;
	}
	bool doLogIntermediate = false;
	try
	{
		doLogIntermediate = __ENV__("OTS_LOG_INTERMEDIATE_STATES") == std::string("1");
	}
	catch(...)
	{ /* ignore errors */
		;
	}

	if(doLog && doLogIntermediate)
		makeSystemLogEntry("System starting up.");
	RunControlStateMachine::theProgressBar_.step();

	// start all non-gateway contexts
	GatewaySupervisor::launchStartOTSCommand(
	    "OTS_APP_STARTUP", CorePropertySupervisorBase::theConfigurationManager_);
	RunControlStateMachine::theProgressBar_.step();

	// important to give time for StartOTS script to recognize command and for apps to
	// instantiate things (before user does Initialize)
	for(int i = 0; i < 10; ++i)
	{
		sleep(1);
		RunControlStateMachine::theProgressBar_.step();
	}

	broadcastMessage(theStateMachine_.getCurrentMessage());

	if(doLogIntermediate)
		makeSystemLogEntry("System startup complete.");
	__COUT__ << "Done starting up." << __E__;
	RunControlStateMachine::theProgressBar_.complete();

}  // end transitionStartingUp()
catch(const xdaq::exception::Exception& e)  // due to xoap send failure
{
	__SS__ << "\nTransition to Starting Up interrupted! There was a system communication "
	          "error "
	          "identified. "
	       << __E__ << e.what();
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}
catch(std::runtime_error& e)
{
	__SS__ << "\nTransition to Starting Up interrupted! There was an error "
	          "identified. "
	       << __E__ << e.what();
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}
catch(toolbox::fsm::exception::Exception& e)
{
	throw;  // just rethrow exceptions of already the correct type
}
catch(...)
{
	__SS__ << "\nTransition to Starting Up interrupted! There was an unknown error "
	          "identified. "
	       << __E__;
	try
	{
		throw;
	}  //one more try to printout extra info
	catch(const std::exception& e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...)
	{
	}
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}  // end transitionStartingUp() catch

//==============================================================================
void GatewaySupervisor::transitionInitializing(toolbox::Event::Reference event)
try
{
	__COUT__ << theStateMachine_.getCurrentStateName() << __E__;

	broadcastMessage(theStateMachine_.getCurrentMessage());

	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName() << __E__;
	__COUT__ << "Fsm current transition: "
	         << theStateMachine_.getCurrentTransitionName(event->type()) << __E__;
	__COUT__ << "Fsm final state: "
	         << theStateMachine_.getTransitionFinalStateName(event->type()) << __E__;

	bool doLog = false;
	try
	{
		doLog = __ENV__("OTS_LOG_INTERMEDIATE_STATES") == std::string("1");
	}
	catch(...)
	{ /* ignore errors */
		;
	}
	if(doLog)
		makeSystemLogEntry("System initialized.");

	__COUT__ << "Done initializing." << __E__;
	RunControlStateMachine::theProgressBar_.complete();

}  // end transitionInitializing()
catch(const xdaq::exception::Exception& e)  // due to xoap send failure
{
	__SS__ << "\nTransition to Initializing interrupted! There was a system "
	          "communication error "
	          "identified. "
	       << __E__ << e.what();
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}
catch(std::runtime_error& e)
{
	__SS__ << "\nTransition to Initializing interrupted! There was an error "
	          "identified. "
	       << __E__ << e.what();
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}
catch(toolbox::fsm::exception::Exception& e)
{
	throw;  // just rethrow exceptions of already the correct type
}
catch(...)
{
	__SS__ << "\nTransition to Initializing interrupted! There was an unknown error "
	          "identified. "
	       << __E__;
	try
	{
		throw;
	}  //one more try to printout extra info
	catch(const std::exception& e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...)
	{
	}
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}  // end transitionInitializing() catch

//==============================================================================
void GatewaySupervisor::transitionPausing(toolbox::Event::Reference /*event*/)
try
{
	checkForAsyncError();

	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName() << __E__;

	RunControlStateMachine::theProgressBar_.step();

	// calculate run duration and post system log entry
	bool doLog = false;
	try
	{
		doLog = __ENV__("OTS_LOG_TRANSITION_STARTS") == std::string("1");
	}
	catch(...)
	{ /* ignore errors */
		;
	}
	bool doLogRuns = true;  //default to logging runs
	try
	{
		doLogRuns = __ENV__("OTS_LOG_RUNS") == std::string("1");
	}
	catch(...)
	{ /* ignore errors */
		;
	}

	std::ostringstream dur_ss;
	{
		int dur = std::chrono::duration_cast<std::chrono::milliseconds>(
		              std::chrono::steady_clock::now() - activeStateMachineRunStartTime)
		              .count() +
		          activeStateMachineRunDuration_ms;
		int dur_s = dur / 1000;
		dur       = dur % 1000;
		int dur_m = dur_s / 60;
		dur_s     = dur_s % 60;
		int dur_h = dur_m / 60;
		dur_m     = dur_m % 60;
		dur_ss << activeStateMachineRunAlias_ << " '" << activeStateMachineRunNumber_
		       << "' duration so far of " << std::setw(2) << std::setfill('0') << dur_h
		       << ":" << std::setw(2) << std::setfill('0') << dur_m << ":" << std::setw(2)
		       << std::setfill('0')
		       << dur_s;  //too much detail "." << dur << " seconds.";
		if(dur_h == 0 && dur_m == 0 && dur_s < 5)  //if very short, add the detail
			dur_ss << "." << dur << " seconds.";
		else
			dur_ss << ".";

		if(doLog)
			makeSystemLogEntry("Run pausing. " + dur_ss.str());
	}

	activeStateMachineRunDuration_ms +=
	    std::chrono::duration_cast<std::chrono::milliseconds>(
	        std::chrono::steady_clock::now() - activeStateMachineRunStartTime)
	        .count();

	// the current message is not for Pause if its due to async exception, so rename
	if(RunControlStateMachine::asyncPauseExceptionReceived_)
	{
		__COUT_ERR__ << "Broadcasting pause for async PAUSE exception!" << __E__;
		broadcastMessage(SOAPUtilities::makeSOAPMessageReference("Pause"));
	}
	else
		broadcastMessage(theStateMachine_.getCurrentMessage());

	if(doLogRuns)
		makeSystemLogEntry("Run paused. " + dur_ss.str(),
		                   activeStateMachineRunAlias_ + " '" +
		                       activeStateMachineRunNumber_ + "' paused");
	__COUT__ << "Done pausing." << __E__;
	RunControlStateMachine::theProgressBar_.complete();

}  // end transitionPausing()
catch(const xdaq::exception::Exception& e)  // due to xoap send failure
{
	__SS__
	    << "\nTransition to Pausing interrupted! There was a system communication error "
	       "identified. "
	    << __E__ << e.what();
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}
catch(std::runtime_error& e)
{
	__SS__ << "\nTransition to Pausing interrupted! There was an error "
	          "identified. "
	       << __E__ << e.what();
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}
catch(toolbox::fsm::exception::Exception& e)
{
	throw;  // just rethrow exceptions of already the correct type
}
catch(...)
{
	__SS__ << "\nTransition to Pausing interrupted! There was an unknown error "
	          "identified. "
	       << __E__;
	try
	{
		throw;
	}  //one more try to printout extra info
	catch(const std::exception& e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...)
	{
	}
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}  // end transitionPausing() catch

//==============================================================================
void GatewaySupervisor::transitionResuming(toolbox::Event::Reference /*event*/)
try
{
	if(RunControlStateMachine::asyncPauseExceptionReceived_)
	{
		// clear async pause error
		__COUT_INFO__ << "Clearing async PAUSE exception!" << __E__;
		RunControlStateMachine::asyncPauseExceptionReceived_ = false;
	}
	else if(RunControlStateMachine::asyncStopExceptionReceived_)
	{
		// clear async stop error
		__COUT_INFO__ << "Clearing async STOP exception!" << __E__;
		RunControlStateMachine::asyncStopExceptionReceived_ = false;
	}

	checkForAsyncError();

	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName() << __E__;

	bool doLog = false;
	try
	{
		doLog = __ENV__("OTS_LOG_TRANSITION_STARTS") == std::string("1");
	}
	catch(...)
	{ /* ignore errors */
		;
	}
	bool doLogRuns = true;  //default to logging runs
	try
	{
		doLogRuns = __ENV__("OTS_LOG_RUNS") == std::string("1");
	}
	catch(...)
	{ /* ignore errors */
		;
	}
	if(doLog)
	{
		std::stringstream ss;
		ss << activeStateMachineRunAlias_ << " '" << activeStateMachineRunNumber_
		   << "' resuming.";

		if(getLastLogEntry(RunControlStateMachine::RESUME_TRANSITION_NAME) != "")
			ss << "\n\n-----------------\nUser log entry:\n"
			   << getLastLogEntry(RunControlStateMachine::RESUME_TRANSITION_NAME)
			   << "\n-----------------\n";
		else
			ss << " No user log entry.";

		makeSystemLogEntry(ss.str());
	}  //end make logbook entry

	activeStateMachineRunStartTime = std::chrono::steady_clock::now();

	broadcastMessage(theStateMachine_.getCurrentMessage());

	// make logbook entry
	if(doLogRuns)
	{
		std::stringstream ss;
		ss << activeStateMachineRunAlias_ << " '" << activeStateMachineRunNumber_
		   << "' resumed.";

		if(getLastLogEntry(RunControlStateMachine::RESUME_TRANSITION_NAME) != "")
			ss << "\n\n-----------------\nUser log entry:\n"
			   << getLastLogEntry(RunControlStateMachine::RESUME_TRANSITION_NAME)
			   << "\n-----------------\n";
		else
			ss << " No user log entry.";

		makeSystemLogEntry(ss.str(),
		                   activeStateMachineRunAlias_ + " '" +
		                       activeStateMachineRunNumber_ + "' resumed");
	}  // end make logbook entry

	__COUT__ << "Done resuming." << __E__;
	RunControlStateMachine::theProgressBar_.complete();
}  // end transitionResuming()
catch(const xdaq::exception::Exception& e)  // due to xoap send failure
{
	__SS__
	    << "\nTransition to Resuming interrupted! There was a system communication error "
	       "identified. "
	    << __E__ << e.what();
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}
catch(std::runtime_error& e)
{
	__SS__ << "\nTransition to Resuming interrupted! There was an error "
	          "identified. "
	       << __E__ << e.what();
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}
catch(toolbox::fsm::exception::Exception& e)
{
	throw;  // just rethrow exceptions of already the correct type
}
catch(...)
{
	__SS__ << "\nTransition to Resuming interrupted! There was an unknown error "
	          "identified. "
	       << __E__;
	try
	{
		throw;
	}  //one more try to printout extra info
	catch(const std::exception& e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...)
	{
	}
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}  // end transitionResuming() catch

//==============================================================================
void GatewaySupervisor::transitionStarting(toolbox::Event::Reference /*event*/)
try
{
	if(RunControlStateMachine::asyncPauseExceptionReceived_)
	{
		// clear async soft error
		__COUT_INFO__ << "Clearing async PAUSE exception!" << __E__;
		RunControlStateMachine::asyncPauseExceptionReceived_ = false;
	}
	else if(RunControlStateMachine::asyncStopExceptionReceived_)
	{
		// clear async stop error
		__COUT_INFO__ << "Clearing async STOP exception!" << __E__;
		RunControlStateMachine::asyncStopExceptionReceived_ = false;
	}

	checkForAsyncError();

	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName() << __E__;

	RunControlStateMachine::theProgressBar_.step();

	SOAPParameters parameters("RunNumber");
	SOAPUtilities::receive(theStateMachine_.getCurrentMessage(), parameters);

	activeStateMachineRunNumber_ = parameters.getValue("RunNumber");
	__COUTV__(activeStateMachineRunNumber_);

	RunControlStateMachine::theProgressBar_.step();

	//Roll over log file if enabled
	if(activeStateMachineRollOverLogOnStart_)
	{
		__COUT_INFO__ << "Rolling over log file on Start transition..." << __E__;
		std::stringstream runSs;
		runSs << "LOG_ROLLOVER";
		runSs << ";" << activeStateMachineRunAlias_ << "_"
		      << activeStateMachineRunNumber_;

		GatewaySupervisor::launchStartOTSCommand(
		    runSs.str(), CorePropertySupervisorBase::theConfigurationManager_);
	}

	RunControlStateMachine::theProgressBar_.step();

	// make logbook entry
	bool doLog = false;
	try
	{
		doLog = __ENV__("OTS_LOG_TRANSITION_STARTS") == std::string("1");
	}
	catch(...)
	{ /* ignore errors */
		;
	}
	bool doLogRuns = true;  //default to logging runs
	try
	{
		doLogRuns = __ENV__("OTS_LOG_RUNS") == std::string("1");
	}
	catch(...)
	{ /* ignore errors */
		;
	}
	if(doLog)
	{
		std::stringstream ss;
		ss << activeStateMachineRunAlias_ << " '" << activeStateMachineRunNumber_
		   << "' starting.";

		if(getLastLogEntry(RunControlStateMachine::START_TRANSITION_NAME) != "")
			ss << "\n\n-----------------\nUser log entry:\n"
			   << getLastLogEntry(RunControlStateMachine::START_TRANSITION_NAME)
			   << "\n-----------------\n";
		else
			ss << " No user log entry.";

		makeSystemLogEntry(ss.str());
	}  // end make logbook entry
	RunControlStateMachine::theProgressBar_.step();

	activeStateMachineRunStartTime   = std::chrono::steady_clock::now();
	activeStateMachineRunDuration_ms = 0;
	broadcastMessage(
	    theStateMachine_
	        .getCurrentMessage());  // ---------------------------------- broadcast!
	RunControlStateMachine::theProgressBar_.step();

	//now that broadcast message done (all subsystems are done with transition!),
	//	check for remote subsystem dumps (after broadcast!)
	__COUT__ << "Broadcast done. Check for remote subsystem dumps." << __E__;

	std::map<std::string /* subsystem */,
	         std::map<std::string /*type/name/field */, std::string /* value */>>
	    gatewayDumpMap;
	{
		std::vector<GatewaySupervisor::RemoteGatewayInfo> remoteGatewayApps;  //local copy
		{  //lock for remainder of scope
			std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
			__SUP_COUTVS__(TLVL_RemoteFSMRequests, remoteGatewayApps_.size());
			remoteGatewayApps = remoteGatewayApps_;
			if(remoteGatewayApps_.size())
				__SUP_COUT_TYPE__(TLVL_DEBUG + TLVL_RemoteFSMRequests)
				    << __COUT_HDR__ << remoteGatewayApps_[0].command << " "
				    << (remoteGatewayApps_[0].appInfo.status) << __E__;
		}

		//include self
		gatewayDumpMap["Gateway"]["name"] = getSupervisorUID();
		gatewayDumpMap["Gateway"]["url"]  = allSupervisorInfo_.getGatewayInfo().getURL();
		gatewayDumpMap["Gateway"]["config_alias"] = activeStateMachineConfigurationAlias_;
		gatewayDumpMap["Gateway"]["console"] =
		    std::string("{") + "\"error_count\": \"" +
		    std::to_string(systemConsoleErrCount_) + "\", " + "\"warning_count\": \"" +
		    std::to_string(systemConsoleWarnCount_) + "\" }";
		//gatewayDumpMap["Gateway"]["consoleErrCount"] =
		//    std::to_string(systemConsoleErrCount_);
		//gatewayDumpMap["Gateway"]["consoleWarnCount"] =
		//    std::to_string(systemConsoleWarnCount_);
		gatewayDumpMap["Gateway"]["fsm"] = std::string("{") +
		                                   "\"mode\": " + "\"Follow FSM\", " +
		                                   "\"follow\": " + "\"1\"}";
		//gatewayDumpMap["Gateway"]["fsmMode"]     = "Follow FSM"; // needed?
		//gatewayDumpMap["Gateway"]["fsmIncluded"] = "1";
		gatewayDumpMap["Gateway"]["config"] = activeStateMachineSystemDumpOnRun_;
		//gatewayDumpMap["Gateway"]["dumpType"] = activeStateMachineDumpFormatOnRun_; // not needed, part of dump.dump_type

		//include environment variables in dumpMap (as escaped key:value pairs, skip functions)
		{
			std::string        envOutput = StringMacros::exec("env");
			std::istringstream envStream(envOutput);
			std::string        envLine;
			std::string        envJson = "{";
			bool               first   = true;
			while(std::getline(envStream, envLine))
			{
				// Skip lines without '=' (continuation lines from multi-line values) or empty names
				size_t eqPos = envLine.find('=');
				if(eqPos == std::string::npos || eqPos == 0)
					continue;

				std::string varName  = envLine.substr(0, eqPos);
				std::string varValue = envLine.substr(eqPos + 1);

				// Skip bash functions, internal variables, and other non-standard entries
				if(varName[0] == '_' ||
				   std::isspace(static_cast<unsigned char>(varName[0])) ||
				   varName.find("BASH_FUNC_") == 0 ||
				   (varValue.size() > 0 && varValue[0] == '('))
					continue;

				if(!first)
					envJson += ", ";
				envJson += "\"" + StringMacros::escapeJSONStringEntities(varName) +
				           "\": \"" + StringMacros::escapeJSONStringEntities(varValue) +
				           "\"";
				first = false;
			}
			envJson += "}";
			gatewayDumpMap["Gateway"]["env"] = envJson;
		}

		//include system variables in dumpMap
		{
			std::string variablesJson = "{";
			bool        firstType     = true;
			for(const auto& typePair : StringMacros::systemVariables_)
			{
				std::string varJson = "{";
				bool        first   = true;
				for(const auto& [key, value] : typePair.second)
				{
					if(!first)
						varJson += ", ";
					varJson += "\"" + StringMacros::escapeJSONStringEntities(key) +
					           "\": \"" + StringMacros::escapeJSONStringEntities(value) +
					           "\"";
					first = false;
				}
				varJson += "}";

				if(!firstType)
					variablesJson += ", ";
				variablesJson += "\"" + typePair.first + "\": " + varJson;
				firstType = false;
			}
			variablesJson += "}";
			gatewayDumpMap["Gateway"]["system_variables"] = variablesJson;
		}

		for(auto& remoteGatewayApp : remoteGatewayApps)
		{
			__COUT__ << "Remote app " << remoteGatewayApp.fullName
			         << " included: " << remoteGatewayApp.fsm_included << __E__;

			if(!remoteGatewayApp.fsm_included)
				continue;  //skip if not included

			gatewayDumpMap[remoteGatewayApp.fullName]["name"] =
			    remoteGatewayApp.appInfo.name;
			gatewayDumpMap[remoteGatewayApp.fullName]["url"] =
			    remoteGatewayApp.appInfo.url;
			gatewayDumpMap[remoteGatewayApp.fullName]["config_alias"] =
			    remoteGatewayApp.selected_config_alias;
			gatewayDumpMap[remoteGatewayApp.fullName]["console"] =
			    std::string("{") + "\"error_count\": \"" +
			    std::to_string(remoteGatewayApp.consoleErrCount) + "\", " +
			    "\"warning_count\": \"" +
			    std::to_string(remoteGatewayApp.consoleWarnCount) + "\" }";
			//gatewayDumpMap[remoteGatewayApp.fullName]["consoleErrCount"] =
			//    std::to_string(remoteGatewayApp.consoleErrCount);
			//gatewayDumpMap[remoteGatewayApp.fullName]["consoleWarnCount"] =
			//    std::to_string(remoteGatewayApp.consoleWarnCount);
			gatewayDumpMap[remoteGatewayApp.fullName]["fsm"] =
			    std::string("{") + "\"mode\": \"" + remoteGatewayApp.getFsmMode() +
			    "\", " + "\"follow\": \"" + (remoteGatewayApp.fsm_included ? "1" : "0") +
			    "\" }";
			//gatewayDumpMap[remoteGatewayApp.fullName]["fsmMode"] =
			//    remoteGatewayApp.getFsmMode();
			//gatewayDumpMap[remoteGatewayApp.fullName]["fsmIncluded"] =
			//    std::string(remoteGatewayApp.fsm_included ? "1" : "0");

			const std::string& dumpStr = remoteGatewayApp.config_dump;
			__COUT__ << "Config dump for remote app " << remoteGatewayApp.fullName
			         << " is:" << dumpStr << __E__;

			gatewayDumpMap[remoteGatewayApp.fullName]["config"] = dumpStr;
			//gatewayDumpMap[remoteGatewayApp.fullName]["dumpType"] =
			//    remoteGatewayApp.getConfigDumpType(); // not needed since inside dump
		}  //end remote app loop

		if(TTEST(2))
		{
			__COUTVS__(2, gatewayDumpMap.size());
			std::string mapDumpStr = "";
			for(const auto& mapPair : gatewayDumpMap)
				for(const auto& [key, value] : mapPair.second)
				{
					mapDumpStr = mapPair.first + " ~~ \n" + key + " : " + value + "\n" +
					             key + "-END!!!";
					__COUT_MULTI__(2, mapDumpStr);
				}
		}

		__COUTV__(activeStateMachineRunInfoPluginType_);

		if(activeStateMachineRunInfoPluginType_ !=
		       TableViewColumnInfo::DATATYPE_STRING_DEFAULT &&
		   activeStateMachineRunInfoPluginType_ !=
		       TableViewColumnInfo::DATATYPE_STRING_ALT_DEFAULT &&
		   activeStateMachineRunInfoPluginType_ != "No Run Info Plugin")
		{
			std::unique_ptr<RunInfoVInterface> runInfoInterface = nullptr;
			try
			{
				runInfoInterface.reset(makeRunInfo(activeStateMachineRunInfoPluginType_,
				                                   activeStateMachineName_));
			}
			catch(...)
			{
				;
			}
			if(runInfoInterface == nullptr)
			{
				__SS__ << "Run Info interface plugin construction failed of type "
				       << activeStateMachineRunInfoPluginType_
				       << " for claiming next run number!" << __E__;
				__SS_THROW__;
			}

			activeStateMachineRunConditionID_ = runInfoInterface->insertRunCondition(
			    static_cast<unsigned int>(std::stoul(
			        activeStateMachineRunNumber_)),  //claimNextRunNumber() returns unsigned int
			    gatewayDumpMap,
			    activeStateMachineConfigureConditionID_,
			    getLastLogEntry(RunControlStateMachine::START_TRANSITION_NAME));

		}  // end Run Info Plugin handling

	}  //end check for remote subsystem dumps
	RunControlStateMachine::theProgressBar_.step();

	if(activeStateMachineSystemDumpOnRunEnable_)
	{
		//write local configuration dump file
		std::string fullfilename = activeStateMachineSystemDumpOnRunFilename_ + "_" +
		                           std::to_string(time(0)) + "_run" +
		                           activeStateMachineRunNumber_ + ".dump";
		FILE* fp = fopen(fullfilename.c_str(), "w");
		if(!fp)
		{
			__SS__ << "Configuration dump failed to file: " << fullfilename << __E__;
			__SS_THROW__;
		}

		//(a la ConfigurationManager::dumpActiveConfiguration)
		fullfilename = __ENV__("HOSTNAME") + std::string(":") + fullfilename;
		fprintf(
		    fp, "Original location of dump:               %s\n", fullfilename.c_str());

		for(auto& gatewayApp : gatewayDumpMap)
			fprintf(fp,
			        "Includes subsytem:               %s\n",
			        gatewayApp.second["name"].c_str());

		for(auto& gatewayApp : gatewayDumpMap)
		{
			fprintf(fp,
			        "\n--- start Dump from subsytem (%zu bytes):               %s\n",
			        gatewayApp.second["config"].size(),
			        gatewayApp.second["name"].c_str());
			if(gatewayApp.second["config"].size())
				fwrite(&gatewayApp.second["config"][0],
				       1,
				       gatewayApp.second["config"].size(),
				       fp);
			__COUT__ << "Wrote configuration subsystem '" << gatewayApp.second["name"]
			         << "' dump of char count " << gatewayApp.second["config"].size()
			         << " to file: " << fullfilename << __E__;
			fprintf(fp,
			        "--- end Dump from subsytem (%zu bytes):               %s\n",
			        gatewayApp.second["config"].size(),
			        gatewayApp.second["name"].c_str());
		}  //end subsystem dump loop

		fclose(fp);

		__COUT_INFO__ << "Run transition Configuration Dump saved to file: "
		              << fullfilename << __E__;
	}  //done with local config dump
	RunControlStateMachine::theProgressBar_.step();

	// save last started group names/keys
	{
		ConfigurationManager::saveGroupNameAndKey(
		    std::make_pair(activeStateMachineConfigurationAlias_, TableGroupKey()),
		    ConfigurationManager::LAST_STARTED_CONFIG_ALIAS_FILE,
		    false /* appendMode */,
		    stateMachineTransitionUsername_);
		ConfigurationManager::saveGroupNameAndKey(
		    std::make_pair(activeStateMachineConfigurationAlias_, TableGroupKey()),
		    ConfigurationManager::STARTED_CONFIG_ALIASES_FILE,
		    true /* appendMode */,
		    stateMachineTransitionUsername_);
		ConfigurationManager::saveGroupNameAndKey(
		    std::make_pair(activeStateMachineConfigurationAlias_, TableGroupKey()),
		    ConfigurationManager::CONFIGURED_OR_STARTED_CONFIG_ALIASES_FILE,
		    true /* appendMode */,
		    stateMachineTransitionUsername_);

		ConfigurationManager::saveGroupNameAndKey(
		    theConfigurationTableGroup_,
		    ConfigurationManager::LAST_STARTED_CONFIG_GROUP_FILE,
		    false /* appendMode */,
		    stateMachineTransitionUsername_);
		ConfigurationManager::saveGroupNameAndKey(
		    theConfigurationTableGroup_,
		    ConfigurationManager::STARTED_CONFIGS_FILE,
		    true /* appendMode */,
		    stateMachineTransitionUsername_);
		ConfigurationManager::saveGroupNameAndKey(
		    theConfigurationTableGroup_,
		    ConfigurationManager::CONFIGURED_OR_STARTED_CONFIGS_FILE,
		    true /* appendMode */,
		    stateMachineTransitionUsername_);

		auto activeGroupMap =
		    CorePropertySupervisorBase::theConfigurationManager_->getActiveTableGroups();

		ConfigurationManager::saveGroupNameAndKey(
		    activeGroupMap.at(ConfigurationManager::GROUP_TYPE_NAME_CONTEXT),
		    ConfigurationManager::LAST_STARTED_CONTEXT_GROUP_FILE,
		    false /* appendMode */,
		    stateMachineTransitionUsername_);
		ConfigurationManager::saveGroupNameAndKey(
		    activeGroupMap.at(ConfigurationManager::GROUP_TYPE_NAME_CONTEXT),
		    ConfigurationManager::STARTED_CONTEXTS_FILE,
		    true /* appendMode */,
		    stateMachineTransitionUsername_);
		ConfigurationManager::saveGroupNameAndKey(
		    activeGroupMap.at(ConfigurationManager::GROUP_TYPE_NAME_CONTEXT),
		    ConfigurationManager::CONFIGURED_OR_STARTED_CONTEXTS_FILE,
		    true /* appendMode */,
		    stateMachineTransitionUsername_);

		ConfigurationManager::saveGroupNameAndKey(
		    activeGroupMap.at(ConfigurationManager::GROUP_TYPE_NAME_CONTEXT),
		    ConfigurationManager::LAST_STARTED_BACKBONE_GROUP_FILE,
		    false /* appendMode */,
		    stateMachineTransitionUsername_);
		ConfigurationManager::saveGroupNameAndKey(
		    activeGroupMap.at(ConfigurationManager::GROUP_TYPE_NAME_BACKBONE),
		    ConfigurationManager::STARTED_BACKBONES_FILE,
		    true /* appendMode */,
		    stateMachineTransitionUsername_);
		ConfigurationManager::saveGroupNameAndKey(
		    activeGroupMap.at(ConfigurationManager::GROUP_TYPE_NAME_BACKBONE),
		    ConfigurationManager::CONFIGURED_OR_STARTED_BACKBONES_FILE,
		    true /* appendMode */,
		    stateMachineTransitionUsername_);

		if(activeGroupMap.at(ConfigurationManager::GROUP_TYPE_NAME_ITERATE)
		       .second.isValid())
		{
			ConfigurationManager::saveGroupNameAndKey(
			    activeGroupMap.at(ConfigurationManager::GROUP_TYPE_NAME_ITERATE),
			    ConfigurationManager::LAST_STARTED_ITERATE_GROUP_FILE,
			    false /* appendMode */,
			    stateMachineTransitionUsername_);
			ConfigurationManager::saveGroupNameAndKey(
			    activeGroupMap.at(ConfigurationManager::GROUP_TYPE_NAME_ITERATE),
			    ConfigurationManager::STARTED_ITERATES_FILE,
			    true /* appendMode */,
			    stateMachineTransitionUsername_);
			ConfigurationManager::saveGroupNameAndKey(
			    activeGroupMap.at(ConfigurationManager::GROUP_TYPE_NAME_ITERATE),
			    ConfigurationManager::CONFIGURED_OR_STARTED_ITERATES_FILE,
			    true /* appendMode */,
			    stateMachineTransitionUsername_);
		}

	}  //end save last started group names/keys

	__COUT__ << "Updating Run Controls State Machine progress bar" << __E__;

	RunControlStateMachine::theProgressBar_.step();

	// make logbook entry
	if(doLogRuns)
	{
		std::stringstream ss;
		ss << activeStateMachineRunAlias_ << " '" << activeStateMachineRunNumber_
		   << "' started.";

		if(getLastLogEntry(RunControlStateMachine::START_TRANSITION_NAME) != "")
			ss << "\n\n-----------------\nUser log entry:\n"
			   << getLastLogEntry(RunControlStateMachine::START_TRANSITION_NAME)
			   << "\n-----------------\n";
		else
			ss << " No user log entry.";

		//insert system and remote subsystem status/detail
		{
			ss << "\n\n~~~ System Status and Detail ~~~\n";
			for(const auto& it : allSupervisorInfo_.getAllSupervisorInfo())
			{
				const auto& appInfo = it.second;
				if(appInfo.getClass() != XDAQContextTable::GATEWAY_SUPERVISOR_CLASS)
					continue;  //only give Gateway status
				ss << "\tStatus: " << appInfo.getStatus() << __E__
				   << "\tDetail: " << appInfo.getDetail() << __E__;
			}

			//also return remote gateways as apps
			std::vector<GatewaySupervisor::RemoteGatewayInfo> remoteApps;  //local copy
			{  //lock for remainder of scope
				std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
				remoteApps = remoteGatewayApps_;
			}

			__COUT__ << "Remote apps size " << remoteApps.size() << __E__;

			if(remoteApps.size())
			{
				ss << "\n\n~~~ Subsystem Status and Detail ~~~\n";

				for(const auto& remoteApp : remoteApps)
				{
					const auto& appInfo = remoteApp.appInfo;
					ss << "Subsystem Name: " << appInfo.name << __E__
					   << "\tStatus: " << appInfo.status << __E__
					   << "\tDetail: " << appInfo.detail << __E__;
				}
			}
		}

		if(0)  //full configuration dump too verbose for ECL (?)
		{
			ss << "\n\nConfigured with System Configuration Alias '"
			   << activeStateMachineConfigurationAlias_ << "' which translates to "
			   << theConfigurationTableGroup_.first << "("
			   << theConfigurationTableGroup_.second << "). Active Context Group "
			   << CorePropertySupervisorBase::theConfigurationManager_
			          ->getActiveGroupName(ConfigurationManager::GroupType::CONTEXT_TYPE)
			   << "("
			   << CorePropertySupervisorBase::theConfigurationManager_->getActiveGroupKey(
			          ConfigurationManager::GroupType::CONTEXT_TYPE)
			   << ").";

			if(activeStateMachineSystemDumpOnRunEnable_)
			{
				ss << "\n\n-----------------\nConfiguration dump:\n"
				   << activeStateMachineSystemDumpOnRun_;
				ss << "\nEND Remote Configuration dump:\n-----------------\n";
			}
		}

		makeSystemLogEntry(ss.str(),
		                   activeStateMachineRunAlias_ + " '" +
		                       activeStateMachineRunNumber_ + "' started");
	}  // end make logbook entry
	__COUT__ << "Done starting run." << __E__;
	RunControlStateMachine::theProgressBar_.complete();

}  // end transitionStarting()
catch(const xdaq::exception::Exception& e)  // due to xoap send failure
{
	__SS__ << "\nTransition to Starting Run interrupted! There was a system "
	          "communication error "
	          "identified. "
	       << __E__ << e.what();
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}
catch(std::runtime_error& e)
{
	__SS__ << "\nTransition to Starting Run interrupted! There was an error "
	          "identified. "
	       << __E__ << e.what();
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}
catch(toolbox::fsm::exception::Exception& e)
{
	throw;  // just rethrow exceptions of already the correct type
}
catch(...)
{
	__SS__ << "\nTransition to Starting Run interrupted! There was an unknown error "
	          "identified. "
	       << __E__;
	try
	{
		throw;
	}  //one more try to printout extra info
	catch(const std::exception& e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...)
	{
	}
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}  // end transitionStarting() catch

//==============================================================================
void GatewaySupervisor::transitionStopping(toolbox::Event::Reference /*event*/)
try
{
	checkForAsyncError();

	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName() << __E__;

	activeStateMachineRunDuration_ms +=
	    std::chrono::duration_cast<std::chrono::milliseconds>(
	        std::chrono::steady_clock::now() - activeStateMachineRunStartTime)
	        .count();

	RunControlStateMachine::theProgressBar_.step();

	bool doLog = false;
	try
	{
		doLog = __ENV__("OTS_LOG_TRANSITION_STARTS") == std::string("1");
	}
	catch(...)
	{ /* ignore errors */
		;
	}
	bool doLogRuns = true;  //default to logging runs
	try
	{
		doLogRuns = __ENV__("OTS_LOG_RUNS") == std::string("1");
	}
	catch(...)
	{ /* ignore errors */
		;
	}

	// calculate run duration and make system log entry
	std::ostringstream dur_ss;
	{
		int dur   = activeStateMachineRunDuration_ms;
		int dur_s = dur / 1000;
		dur       = dur % 1000;
		int dur_m = dur_s / 60;
		dur_s     = dur_s % 60;
		int dur_h = dur_m / 60;
		dur_m     = dur_m % 60;
		dur_ss << activeStateMachineRunAlias_ << " '" << activeStateMachineRunNumber_
		       << "' duration of " << std::setw(2) << std::setfill('0') << dur_h << ":"
		       << std::setw(2) << std::setfill('0') << dur_m << ":" << std::setw(2)
		       << std::setfill('0')
		       << dur_s;  //too much detail "." << dur << " seconds.";
		if(dur_h == 0 && dur_m == 0 && dur_s < 5)  //if very short, add the detail
			dur_ss << "." << dur << " seconds.";
		else
			dur_ss << ".";

		if(doLog)
		{
			std::stringstream ss;
			ss << dur_ss.str();
			if(getLastLogEntry(RunControlStateMachine::STOP_TRANSITION_NAME) != "")
				ss << "\n\n-----------------\nUser log entry:\n"
				   << getLastLogEntry(RunControlStateMachine::STOP_TRANSITION_NAME)
				   << "\n-----------------\n";
			else
				ss << " No user log entry.";

			makeSystemLogEntry("Run stopping. " + ss.str());
		}
	}

	// the current message is not for Stop if its due to async exception, so rename
	if(RunControlStateMachine::asyncStopExceptionReceived_)
	{
		__COUT_ERR__ << "Broadcasting stop for async STOP exception!" << __E__;
		broadcastMessage(SOAPUtilities::makeSOAPMessageReference("Stop"));
	}
	else
		broadcastMessage(theStateMachine_.getCurrentMessage());

	// make logbook entry
	if(doLogRuns)
	{
		std::stringstream ss;
		ss << dur_ss.str();
		if(getLastLogEntry(RunControlStateMachine::STOP_TRANSITION_NAME) != "")
			ss << "\n\n-----------------\nUser log entry:\n"
			   << getLastLogEntry(RunControlStateMachine::STOP_TRANSITION_NAME)
			   << "\n-----------------\n";
		else
			ss << " No user log entry.";

		makeSystemLogEntry("Run stopped.\n" + ss.str(),
		                   activeStateMachineRunAlias_ + " '" +
		                       activeStateMachineRunNumber_ + "' stopped");
	}  // end make logbook entry

	__COUT__ << "Done stopping run." << __E__;
	RunControlStateMachine::theProgressBar_.complete();

	//Roll over log file if enabled
	if(activeStateMachineRollOverLogOnStart_)
	{
		__COUT_INFO__ << "Rolling over log file on Stop transition..." << __E__;
		std::stringstream runSs;
		runSs << "LOG_ROLLOVER";
		runSs << ";Post" << activeStateMachineRunAlias_ << "_"
		      << activeStateMachineRunNumber_;

		GatewaySupervisor::launchStartOTSCommand(
		    runSs.str(), CorePropertySupervisorBase::theConfigurationManager_);
	}

	RunControlStateMachine::theProgressBar_.step();

}  // end transitionStopping()
catch(const xdaq::exception::Exception& e)  // due to xoap send failure
{
	__SS__ << "\nTransition to Stopping Run interrupted! There was a system "
	          "communication error "
	          "identified. "
	       << __E__ << e.what();
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}
catch(std::runtime_error& e)
{
	__SS__ << "\nTransition to Stopping Run interrupted! There was an error "
	          "identified. "
	       << __E__ << e.what();
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}
catch(toolbox::fsm::exception::Exception& e)
{
	throw;  // just rethrow exceptions of already the correct type
}
catch(...)
{
	__SS__ << "\nTransition to Stopping Run interrupted! There was an unknown error "
	          "identified. "
	       << __E__;
	try
	{
		throw;
	}  //one more try to printout extra info
	catch(const std::exception& e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...)
	{
	}
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}  // end transitionStopping() catch

////////////////////////////////////////////////////////////////////////////////////////////
//////////////      MESSAGES ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

//==============================================================================
/// handleBroadcastMessageTarget
///	Sends message and gets reply
///	Handles sub-iterations at same target
///		if failure, THROW state machine exception
///	returns true if iterations are done, else false
bool GatewaySupervisor::handleBroadcastMessageTarget(const SupervisorInfo&    appInfo,
                                                     xoap::MessageReference   message,
                                                     const std::string&       command,
                                                     const unsigned int&      iteration,
                                                     std::string&             reply,
                                                     unsigned int             threadIndex,
                                                     const std::atomic<bool>* exitFlag)
try
{
	unsigned int subIteration      = 0;  // reset for next subIteration loop
	bool         subIterationsDone = false;
	bool         iterationsDone    = true;

	while(!subIterationsDone)  // start subIteration handling loop
	{
		__COUT__ << "Broadcast thread " << threadIndex << "\t"
		         << "Supervisor instance = '" << appInfo.getName()
		         << "' [LID=" << appInfo.getId() << "] in Context '"
		         << appInfo.getContextName() << "' [URL=" << appInfo.getURL()
		         << "] Command = " << command << __E__;

		// Before accessing any supervisor state on this sub-iteration,
		// check if the thread has been told to exit.
		if(exitFlag && exitFlag->load(std::memory_order_acquire))
		{
			__COUT__ << "Broadcast thread " << threadIndex
			         << " exitFlag set at top of sub-iteration loop; abandoning work."
			         << __E__;
			return true;
		}

		checkForAsyncError();

		subIterationsDone = true;
		RunControlStateMachine::theProgressBar_.step();

		// add subIteration index to message
		if(subIteration)
		{
			SOAPParameters parameters;
			parameters.addParameter("subIterationIndex", subIteration);
			SOAPUtilities::addParameters(message, parameters);
		}

		if(iteration || subIteration)
			__COUT__ << "Broadcast thread " << threadIndex << "\t"
			         << "Adding iteration parameters " << iteration << "." << subIteration
			         << __E__;

		RunControlStateMachine::theProgressBar_.step();

		std::string givenAppStatus = SupervisorInfo::APP_STATUS_UNKNOWN;
		try
		{
			givenAppStatus = theStateMachine_.getCurrentTransitionName(command);
		}
		catch(...)
		{
			//ignoring invalid transition tranistion name error
		}

		unsigned int givenAppProgress = appInfo.getProgress();
		std::string  givenAppDetail   = appInfo.getDetail();
		if(givenAppProgress >= 100)
		{
			givenAppProgress = 0;  // reset
			givenAppDetail   = "";
		}

		if(iteration == 0 && subIteration == 0)  //first time through the supervisors
		{
			__COUT__ << "Broadcast thread " << threadIndex << "\t"
			         << "Sending message to Supervisor " << appInfo.getName()
			         << " [LID=" << appInfo.getId() << "]: " << command << __E__;

			givenAppDetail = "";
		}
		else  // else this not the first time through the supervisors
		{
			if(givenAppDetail == "")
				givenAppDetail =
				    std::to_string(iteration) + ":" + std::to_string(subIteration);
			if(subIteration == 0)
			{
				for(unsigned int j = 0; j < 4; ++j)
					__COUT__ << "Broadcast thread " << threadIndex << "\t"
					         << "Sending message to Supervisor " << appInfo.getName()
					         << " [LID=" << appInfo.getId() << "]: " << command
					         << " (iteration: " << iteration << ")" << __E__;
			}
			else
			{
				for(unsigned int j = 0; j < 4; ++j)
					__COUT__ << "Broadcast thread " << threadIndex << "\t"
					         << "Sending message to Supervisor " << appInfo.getName()
					         << " [LID=" << appInfo.getId() << "]: " << command
					         << " (iteration: " << iteration
					         << ", sub-iteration: " << subIteration << ")" << __E__;
			}
		}

		{
			// add the message index
			SOAPParameters parameters;
			{  // mutex scope
				std::lock_guard<std::mutex> lock(broadcastCommandMessageIndexMutex_);
				parameters.addParameter("commandId", broadcastCommandMessageIndex_++);
			}  // end mutex scope
			SOAPUtilities::addParameters(message, parameters);
		}

		__COUT__ << "Broadcast thread " << threadIndex << "\t"
		         << "Sending... \t" << SOAPUtilities::translate(message) << std::endl;

		try  // attempt transmit of transition command
		{
			__COUT__ << "Broadcast thread " << threadIndex
			         << "\t givenAppStatus=" << givenAppStatus << __E__;
			__COUT__ << "Broadcast thread " << threadIndex
			         << "\t appInfo.getStatus()=" << appInfo.getStatus() << __E__;

			// wait for app to exist in status before sending commands
			int waitAttempts = 0;
			while(appInfo.getStatus() == SupervisorInfo::APP_STATUS_UNKNOWN)
			{
				__COUT__ << "Broadcast thread " << threadIndex << "\t"
				         << "Waiting for Supervisor " << appInfo.getName()
				         << " [LID=" << appInfo.getId()
				         << "] in unknown state. waitAttempts of 10 = " << waitAttempts
				         << __E__;
				++waitAttempts;
				if(waitAttempts == 10)
				{
					__SS__ << "Error! Gateway Supervisor failed to send message to app "
					          "in unknown state "
					          "Supervisor instance = '"
					       << appInfo.getName() << "' [LID=" << appInfo.getId()
					       << "] in Context '" << appInfo.getContextName()
					       << "' [URL=" << appInfo.getURL() << "].\n\n";
					__COUT_ERR__ << ss.str();
					XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
				}
				sleep(2);
			}

			__COUTT__ << "Grabbing lock " << appInfo.getId() << __E__;
			// start recursive mutex scope (same thread can lock multiple times, but needs to unlock the same)
			std::lock_guard<std::recursive_mutex> lock(
			    allSupervisorInfo_.getSupervisorInfoMutex(appInfo.getId()));
			__COUTT__ << "Have lock " << appInfo.getId() << __E__;
			// set app status, but leave progress and detail alone
			allSupervisorInfo_.setSupervisorStatus(
			    appInfo, givenAppStatus, givenAppProgress, givenAppDetail);

			__COUTT__ << "here in lock " << appInfo.getId() << __E__;

			__COUT__ << "Broadcast thread in lock " << threadIndex << "\t"
			         << "Sending... \t" << SOAPUtilities::translate(message) << std::endl;
			// for transition attempt, set status for app, in case the request occupies the target app
			std::string tmpReply = send(appInfo.getDescriptor(), message);
			__COUTV__(tmpReply);
			//using the intermediate temporary string seems to possibly help when there are multiple crashes of FSM entities
			reply = tmpReply;

			// After the blocking SOAP call, check if the thread has been told to
			// exit (e.g., broadcastMessage() timed out and unwound).  If so, bail
			// out immediately – supervisorPtr / `this` may already be invalid.
			if(exitFlag && exitFlag->load(std::memory_order_acquire))
			{
				__COUT__ << "Broadcast thread " << threadIndex
				         << " exitFlag set after send(); abandoning work." << __E__;
				return true;  // report "done" so caller does not touch supervisor state
			}

			// then release mutex here using scope change, to allow the app to start giving its own updates
		}
		catch(const xdaq::exception::Exception& e)  // due to xoap send failure
		{
			// Check exit flag before using any more supervisor members in the retry path.
			if(exitFlag && exitFlag->load(std::memory_order_acquire))
			{
				__COUT__ << "Broadcast thread " << threadIndex
				         << " exitFlag set after send() failure; abandoning work."
				         << __E__;
				return true;
			}

			// do not kill whole system if xdaq xoap failure
			__SS__ << "Error! Gateway Supervisor can NOT " << command
			       << " Supervisor instance = '" << appInfo.getName()
			       << "' [LID=" << appInfo.getId() << "] in Context '"
			       << appInfo.getContextName() << "' [URL=" << appInfo.getURL()
			       << "].\n\n"
			       << "Xoap message failure. Did the target Supervisor crash? Try "
			          "re-initializing or restarting otsdaq."
			       << __E__;
			__COUT_ERR__ << ss.str();

			try
			{
				__COUT__ << "Broadcast thread " << threadIndex << "\t"
				         << "Try again.." << __E__;

				{
					// add a second try parameter flag
					SOAPParameters parameters;
					parameters.addParameter("retransmission", "1");
					SOAPUtilities::addParameters(message, parameters);
				}

				{
					// add the message index
					SOAPParameters parameters;
					{  // mutex scope
						std::lock_guard<std::mutex> lock(
						    broadcastCommandMessageIndexMutex_);
						parameters.addParameter("commandId",
						                        broadcastCommandMessageIndex_++);
					}  // end mutex scope
					SOAPUtilities::addParameters(message, parameters);
				}

				__COUT__ << "Broadcast thread " << threadIndex << "\t"
				         << "Re-Sending... " << SOAPUtilities::translate(message)
				         << std::endl;

				reply = send(appInfo.getDescriptor(), message);

				// Check exit flag after blocking retry send.
				if(exitFlag && exitFlag->load(std::memory_order_acquire))
				{
					__COUT__ << "Broadcast thread " << threadIndex
					         << " exitFlag set after retry send(); abandoning work."
					         << __E__;
					return true;
				}
			}
			catch(const xdaq::exception::Exception& e)  // due to xoap send failure
			{
				// Check exit flag before touching supervisor state in the throw path.
				if(exitFlag && exitFlag->load(std::memory_order_acquire))
				{
					__COUT__
					    << "Broadcast thread " << threadIndex
					    << " exitFlag set after retry send() failure; abandoning work."
					    << __E__;
					return true;
				}
				__COUT_ERR__ << "Broadcast thread " << threadIndex << "\t"
				             << "Second try failed.." << __E__;
				XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
			}
			__COUT__ << "Broadcast thread " << threadIndex << "\t"
			         << "2nd try passed.." << __E__;
		}  // end send catch

		__COUT__ << "Broadcast thread " << threadIndex << "\t"
		         << "Reply received from " << appInfo.getName()
		         << " [LID=" << appInfo.getId() << "]: " << reply << __E__;

		// Before processing the reply (which accesses supervisor state),
		// check if the thread has been told to exit.
		if(exitFlag && exitFlag->load(std::memory_order_acquire))
		{
			__COUT__ << "Broadcast thread " << threadIndex
			         << " exitFlag set after SOAP reply; abandoning work." << __E__;
			return true;
		}

		if((reply != command + "Done") && (reply != command + "Response") &&
		   (reply != command + "Iterate") && (reply != command + "SubIterate"))
		{
			__SS__ << "Error! Gateway Supervisor can NOT " << command
			       << " Supervisor instance = '" << appInfo.getName()
			       << "' [LID=" << appInfo.getId() << "] in Context '"
			       << appInfo.getContextName() << "' [URL=" << appInfo.getURL()
			       << "].\n\n"
			       << reply;
			__COUT_ERR__ << ss.str() << __E__;

			__COUT__ << "Broadcast thread " << threadIndex << "\t"
			         << "Getting error message..." << __E__;
			try
			{
				xoap::MessageReference errorMessage =
				    sendWithSOAPReply(appInfo.getDescriptor(),
				                      SOAPUtilities::makeSOAPMessageReference(
				                          "StateMachineErrorMessageRequest"));

				// Check exit flag after the error-retrieval SOAP call.
				if(exitFlag && exitFlag->load(std::memory_order_acquire))
				{
					__COUT__
					    << "Broadcast thread " << threadIndex
					    << " exitFlag set after error-retrieval send(); abandoning work."
					    << __E__;
					return true;
				}

				SOAPParameters parameters;
				parameters.addParameter("ErrorMessage");
				SOAPUtilities::receive(errorMessage, parameters);

				std::string error = parameters.getValue("ErrorMessage");
				if(error == "")
				{
					std::stringstream err;
					err << "Unknown error from Supervisor instance = '"
					    << appInfo.getName() << "' [LID=" << appInfo.getId()
					    << "] in Context '" << appInfo.getContextName()
					    << "' [URL=" << appInfo.getURL()
					    << "]. If the problem persists or is repeatable, please notify "
					       "admins.\n\n";
					error = err.str();
				}

				__SS__ << "Received error message from Supervisor instance = '"
				       << appInfo.getName() << "' [LID=" << appInfo.getId()
				       << "] in Context '" << appInfo.getContextName()
				       << "' [URL=" << appInfo.getURL()
				       << "].\n\n Error Message = " << error << __E__;

				__COUT_ERR__ << ss.str() << __E__;

				if(command == RunControlStateMachine::ERROR_TRANSITION_NAME)
					return true;  // do not throw exception and exit loop if informing all
					              // apps about error
				// else throw exception and go into Error
				XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
			}
			catch(const xdaq::exception::Exception& e)  // due to xoap send failure
			{
				// Check exit flag before touching supervisor state in the throw path.
				if(exitFlag && exitFlag->load(std::memory_order_acquire))
				{
					__COUT__
					    << "Broadcast thread " << threadIndex
					    << " exitFlag set after error-retrieval failure; abandoning work."
					    << __E__;
					return true;
				}
				// do not kill whole system if xdaq xoap failure
				__SS__ << "Error! Gateway Supervisor failed to read error message from "
				          "Supervisor instance = '"
				       << appInfo.getName() << "' [LID=" << appInfo.getId()
				       << "] in Context '" << appInfo.getContextName()
				       << "' [URL=" << appInfo.getURL() << "].\n\n"
				       << "Xoap message failure. Did the target Supervisor crash? Try "
				          "re-initializing or restarting otsdaq."
				       << __E__;
				__COUT_ERR__ << ss.str();
				XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
			}
		}  // end error response handling
		else if(reply == command + "Iterate")
		{
			// when 'Working' this front-end is expecting
			//	to get the same command again with an incremented iteration index
			//	after all other front-ends see the same iteration index, and all
			// 	front-ends with higher priority see the incremented iteration index.

			iterationsDone = false;
			__COUT__ << "Broadcast thread " << threadIndex << "\t"
			         << "Supervisor instance = '" << appInfo.getName()
			         << "' [LID=" << appInfo.getId() << "] in Context '"
			         << appInfo.getContextName() << "' [URL=" << appInfo.getURL()
			         << "] flagged for another iteration to " << command
			         << "... (iteration: " << iteration << ")" << __E__;

		}  // end still working response handling
		else if(reply == command + "SubIterate")
		{
			// when 'Working' this front-end is expecting
			//	to get the same command again with an incremented sub-iteration index
			//	without any other front-ends taking actions or seeing the sub-iteration
			// index.

			subIterationsDone = false;
			__COUT__ << "Broadcast thread " << threadIndex << "\t"
			         << "Supervisor instance = '" << appInfo.getName()
			         << "' [LID=" << appInfo.getId() << "] in Context '"
			         << appInfo.getContextName() << "' [URL=" << appInfo.getURL()
			         << "] flagged for another sub-iteration to " << command
			         << "... (iteration: " << iteration
			         << ", sub-iteration: " << subIteration << ")" << __E__;
		}
		else  // else success response
		{
			__COUT__ << "Broadcast thread " << threadIndex << "\t"
			         << "Supervisor instance = '" << appInfo.getName()
			         << "' [LID=" << appInfo.getId() << "] in Context '"
			         << appInfo.getContextName() << "' [URL=" << appInfo.getURL()
			         << "] was " << command << "'d correctly!" << __E__;
		}

		if(subIteration)
			__COUT__ << "Broadcast thread " << threadIndex << "\t"
			         << "Completed sub-iteration: " << subIteration << __E__;
		++subIteration;

	}  // end subIteration handling loop

	return iterationsDone;

}  // end handleBroadcastMessageTarget()
catch(const toolbox::fsm::exception::Exception& e)
{
	throw;
}  //keep existing FSM execptions intact
catch(...)
{
	// do not kill whole system if unexpected exception
	__SS__ << "Error! Gateway Supervisor failed to broadcast message '" << command
	       << "' to "
	          "Supervisor instance = '"
	       << appInfo.getName() << "' [LID=" << appInfo.getId() << "] in Context '"
	       << appInfo.getContextName() << "' [URL=" << appInfo.getURL()
	       << "]. Try re-initializing or restarting otsdaq." << __E__;
	__COUT_ERR__ << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}

//==============================================================================
/// broadcastMessageThread
///	Sends transition command message and gets reply
///		if failure, THROW
void GatewaySupervisor::broadcastMessageThread(
    GatewaySupervisor*                                        supervisorPtr,
    std::shared_ptr<GatewaySupervisor::BroadcastThreadStruct> threadStruct)
{
	// Cancellation is disabled – pthread_cancel() with PTHREAD_CANCEL_ASYNCHRONOUS
	// is fundamentally unsafe in C++ code because the abi::__forced_unwind
	// exception it injects can be caught (and not rethrown) by intermediate
	// catch-all handlers in the SOAP stack or standard library, causing a
	// "FATAL: exception not rethrown" process abort.  Instead of cancellation,
	// stuck threads are abandoned (detached) by the main thread after a timeout;
	// they will eventually unblock when their SOAP call times out on its own.
	pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, nullptr);

	__COUT__ << "Broadcast thread " << threadStruct->threadIndex_ << "\t"
	         << "established..." << __E__;

	while(!threadStruct->exitThread_)
	{
		// sleep to give time to main thread to dole out work
		usleep(1000 /* 1ms */);

		// take lock for remainder of scope
		std::lock_guard<std::mutex> lock(threadStruct->threadMutex_);
		if(threadStruct->workToDo_)
		{
			__COUT__ << "Broadcast thread " << threadStruct->threadIndex_ << "\t"
			         << "starting work... command = " << threadStruct->getCommand()
			         << __E__;

			try
			{
				if(supervisorPtr->handleBroadcastMessageTarget(
				       threadStruct->getAppInfo(),
				       threadStruct->getMessage(),
				       threadStruct->getCommand(),
				       threadStruct->getIteration(),
				       threadStruct->getReply(),
				       threadStruct->threadIndex_,
				       &threadStruct->exitThread_))
					threadStruct->getIterationsDone() = true;
			}
			catch(const toolbox::fsm::exception::Exception& e)
			{
				// If exitThread_ is set, supervisorPtr may already be
				// invalid — do not touch any supervisor state; just exit.
				if(threadStruct->exitThread_)
				{
					__COUT__ << "Broadcast thread " << threadStruct->threadIndex_
					         << " caught exception after exitThread_ set; "
					         << "abandoning without touching supervisor state." << __E__;
					threadStruct->workToDo_ = false;
					threadStruct->working_  = false;
					return;
				}

				__COUT__ << "Broadcast thread " << threadStruct->threadIndex_ << "\t"
				         << "going into error: " << e.what() << __E__;

				threadStruct->getReply() = e.what();
				threadStruct->error_     = true;
				threadStruct->workToDo_  = false;
				threadStruct->working_   = false;  // indicate exiting
				return;
			}

			// After handleBroadcastMessageTarget() returns, re-check
			// exitThread_ before touching supervisorPtr – the main thread
			// may have timed out and unwound broadcastMessage(), making
			// supervisorPtr potentially invalid.
			if(threadStruct->exitThread_)
			{
				__COUT__ << "Broadcast thread " << threadStruct->threadIndex_
				         << " exitThread_ set after work completed; "
				         << "skipping supervisor state update." << __E__;
				threadStruct->workToDo_ = false;
				// Do NOT touch supervisorPtr from here on.
				break;  // exit the primary while loop
			}

			if(!threadStruct->getIterationsDone())
			{
				__COUT__ << "Broadcast thread " << threadStruct->threadIndex_ << "\t"
				         << "flagged for another iteration." << __E__;

				// set global iterationsDone
				supervisorPtr->broadcastIterationsDone_ = false;
			}

			__COUT__ << "Broadcast thread " << threadStruct->threadIndex_ << "\t"
			         << "done with work." << __E__;

			threadStruct->workToDo_ = false;
		}  // end work

	}  // end primary while loop

	__COUT__ << "Broadcast thread " << threadStruct->threadIndex_ << "\t"
	         << "exited." << __E__;
	threadStruct->working_ = false;  // indicate exiting
}  // end broadcastMessageThread()

//==============================================================================
/// broadcastMessage
///	Broadcast state transition to all xdaq Supervisors and remote Gateway Supervisors.
///		- Transition in order of, remote Gateways first, then priority as given by AllSupervisorInfo
///	Update Supervisor Info based on result of transition.
void GatewaySupervisor::broadcastMessage(xoap::MessageReference message)
{
	{  // create lock scope and clear status
		std::lock_guard<std::mutex> lock(broadcastCommandStatusUpdateMutex_);
		broadcastCommandStatus_ = "";
	}

	RunControlStateMachine::theProgressBar_.step();

	// transition of Gateway Supervisor is assumed successful so update status
	allSupervisorInfo_.setSupervisorStatus(this, theStateMachine_.getCurrentStateName());

	std::string command = SOAPUtilities::translate(message).getCommand();

	std::string reply;
	broadcastIterationsDone_ = false;
	bool assignedJob;

	std::vector<std::vector<const SupervisorInfo*>> orderedSupervisors;

	try
	{
		orderedSupervisors = allSupervisorInfo_.getOrderedSupervisorDescriptors(
		    command,
		    // only gateway apps for special shutdown and startup command broadcast
		    command == RunControlStateMachine::SHUTDOWN_TRANSITION_NAME ||
		        command == RunControlStateMachine::STARTUP_TRANSITION_NAME);
	}
	catch(const std::runtime_error& e)
	{
		__SS__
		    << "Error getting supervisor priority. Was there a change in the context?"
		    << " Remember, if the context was changed, it is recommended to relaunch the "
		       "ots script. "
		    << e.what() << __E__;
		XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
	}

	RunControlStateMachine::theProgressBar_.step();

	// std::vector<std::vector<uint8_t/*bool*/>> supervisorIterationsDone; //Note: can not
	// use bool because std::vector does not allow access by reference of type bool
	auto supervisorIterationsDone =
	    std::make_shared<GatewaySupervisor::BroadcastMessageIterationsDoneStruct>();

	// initialize to false (not done)
	for(const auto& vectorAtPriority : orderedSupervisors)
		supervisorIterationsDone->push(vectorAtPriority.size());  // push_back(
		    // std::vector<uint8_t>(vectorAtPriority.size(),
		    // false /*initial value*/));

	unsigned int iteration = 0;
	// unsigned int subIteration;
	unsigned int iterationBreakpoint;

	// send command to all supervisors (for multiple iterations) until all are done

	// make a copy of the message to use as starting point for iterations
	xoap::MessageReference originalMessage =
	    SOAPUtilities::makeSOAPMessageReference(SOAPUtilities::translate(message));

	__COUT__ << "=========> Broadcasting state machine command = " << command << __E__;

	unsigned int numberOfThreads = 1;

	try
	{
		numberOfThreads = CorePropertySupervisorBase::getSupervisorTableNode()
		                      .getNode("NumberOfStateMachineBroadcastThreads")
		                      .getValue<unsigned int>();
	}
	catch(...)
	{
		// ignore error for backwards compatibility
		__COUT__ << "Number of threads not in configuration, so defaulting to "
		         << numberOfThreads << __E__;
	}

	// Note: if 1 thread, then create no threads
	// i.e. only create threads if 2 or more.
	if(numberOfThreads == 1)
		numberOfThreads = 0;

	__COUTV__(numberOfThreads);

	// std::vector<GatewaySupervisor::BroadcastThreadStruct> broadcastThreadStructs_(numberOfThreads);
	broadcastThreadStructs_.clear();

	// only launch threads if more than 1
	//	if 1, just use main thread
	for(unsigned int i = 0; i < numberOfThreads; ++i)
	{
		broadcastThreadStructs_.push_back(
		    std::make_shared<GatewaySupervisor::BroadcastThreadStruct>());
		broadcastThreadStructs_[i]->threadIndex_ = i;

		std::thread(
		    [](GatewaySupervisor*                                        supervisorPtr,
		       std::shared_ptr<GatewaySupervisor::BroadcastThreadStruct> threadStruct) {
			    GatewaySupervisor::broadcastMessageThread(supervisorPtr, threadStruct);
		    },
		    this,
		    broadcastThreadStructs_[i])
		    .detach();
	}  // end broadcast thread creation loop

	RunControlStateMachine::theProgressBar_.step();

	broadcastMessageToRemoteGateways(originalMessage);

	RunControlStateMachine::theProgressBar_.step();

	try
	{
		//:::::::::::::::::::::::::::::::::::::::::::::::::::::
		// Send a SOAP message to every Supervisor in order by priority
		do  // while !iterationsDone
		{
			__COUT__ << "Iteration loop pass: iteration=" << iteration << " for command '"
			         << command << "'" << __E__;

			broadcastIterationsDone_ = true;

			{  // start mutex scope
				std::lock_guard<std::mutex> lock(broadcastIterationBreakpointMutex_);
				iterationBreakpoint = broadcastIterationBreakpoint_;  // get breakpoint
			}                                                         // end mutex scope

			if(iterationBreakpoint < (unsigned int)-1)
				__COUT__ << "Iteration breakpoint currently is " << iterationBreakpoint
				         << __E__;
			if(iteration >= iterationBreakpoint)
			{
				broadcastIterationsDone_ = false;
				__COUT__ << "Waiting at transition breakpoint - iteration = " << iteration
				         << __E__;
				usleep(5 * 1000 * 1000 /*5 s*/);
				continue;  // wait until breakpoint moved
			}

			if(iteration)
			{
				__COUT__ << "Starting iteration: " << iteration << __E__;

				// Re-send command to non-done remote gateways with updated iteration index
				broadcastMessageToRemoteGateways(originalMessage, iteration);
			}

			for(unsigned int i = 0; i < supervisorIterationsDone->size(); ++i)
			{
				for(unsigned int j = 0; j < supervisorIterationsDone->size(i); ++j)
				{
					checkForAsyncError();

					if((*supervisorIterationsDone)[i][j])
						continue;  // skip if supervisor is already done

					const SupervisorInfo& appInfo = *(orderedSupervisors[i][j]);

					// re-acquire original message
					message = SOAPUtilities::makeSOAPMessageReference(
					    SOAPUtilities::translate(originalMessage));

					// add iteration index to message
					if(iteration)
					{
						// add the iteration index as a parameter to message
						SOAPParameters parameters;
						parameters.addParameter("iterationIndex", iteration);
						SOAPUtilities::addParameters(message, parameters);
					}

					if(numberOfThreads)
					{
						// schedule message to first open thread
						assignedJob = false;
						do
						{
							for(unsigned int k = 0; k < numberOfThreads; ++k)
							{
								if(!broadcastThreadStructs_[k]->workToDo_)
								{
									// found our thread!
									assignedJob = true;
									__COUT__ << "Giving work to thread " << k
									         << ", command = " << command << __E__;

									std::lock_guard<std::mutex> lock(
									    broadcastThreadStructs_[k]->threadMutex_);
									broadcastThreadStructs_[k]->setMessage(
									    appInfo,
									    message,
									    command,
									    iteration,
									    (*supervisorIterationsDone)[i][j],
									    supervisorIterationsDone);

									break;
								}
							}  // end thread assigning search

							if(!assignedJob)
							{
								__COUT__ << "No free broadcast threads, "
								         << "waiting for an available thread..." << __E__;
								usleep(100 * 1000 /*100 ms*/);
							}
						} while(!assignedJob);
					}
					else  // no thread
					{
						if(handleBroadcastMessageTarget(
						       appInfo, message, command, iteration, reply))
							(*supervisorIterationsDone)[i][j] = true;
						else
							broadcastIterationsDone_ = false;
					}

				}  // end supervisors at same priority broadcast loop

				unsigned int numberOfEndpointsAtPriority =
				    supervisorIterationsDone->size(i);

				// before proceeding to next priority,
				//	make sure all threads have completed
				if(numberOfThreads)
				{
					__COUT__ << "Iteration priority level command work has been "
					            "broadcast to threads. Waiting for threads to finish..."
					         << __E__;
					bool      done;
					const int timeoutSeconds  = 4 * 60;  //4 minutes for each iteration
					uint32_t  lastMinutesLeft = -1;
					time_t    start;
					time(&start);
					uint32_t waitIt = 0;
					do
					{
						done                              = true;
						unsigned int numOfThreadsWithWork = 0;
						unsigned int lastUnfinishedThread = -1;

						for(unsigned int i = 0; i < numberOfThreads; ++i)
							if(broadcastThreadStructs_[i]->workToDo_)
							{
								done = false;
								++numOfThreadsWithWork;
								lastUnfinishedThread = i;
							}
							else if(broadcastThreadStructs_[i]->error_)
							{
								__COUT__ << "Found thread in error! Throwing state "
								            "machine error: "
								         << broadcastThreadStructs_[i]->getReply()
								         << __E__;
								XCEPT_RAISE(toolbox::fsm::exception::Exception,
								            broadcastThreadStructs_[i]->getReply());
							}

						if(!done)  // update status and sleep
						{
							if(difftime(time(0), start) > timeoutSeconds)
							{
								__SS__ << "Timeout (" << timeoutSeconds / 60
								       << " minutes) waiting for threads to finish "
								          "command = "
								       << command << "!" << __E__;

								ss << "\n"
								   << numOfThreadsWithWork << " of "
								   << numberOfEndpointsAtPriority
								   << " endpoint(s) timed out:\n";
								for(unsigned int ti = 0; ti < numberOfThreads; ++ti)
									if(broadcastThreadStructs_[ti]->workToDo_)
									{
										const auto& failingAppInfo =
										    broadcastThreadStructs_[ti]->getAppInfo();
										ss << "  - App: " << failingAppInfo.getName()
										   << " (ID: " << failingAppInfo.getId() << ")"
										   << ", Context: "
										   << failingAppInfo.getContextName()
										   << ", Hostname: "
										   << failingAppInfo.getHostname() << __E__;
									}

								ss << "\n"
								   << "Please review the failing endpoint. Each "
								      "transition iteration must finish in under "
								   << timeoutSeconds / 60
								   << " minutes. If a transition must take longer, "
								      "please review the endpoint code, and break up the "
								      "transition into multiple steps (i.e. iterations)."
								   << __E__;
								__SS_THROW__;
							}

							std::stringstream waitSs;
							waitSs << "Waiting on " << numOfThreadsWithWork << " of "
							       << numberOfThreads
							       << " threads to finish. Command = " << command;
							if(command ==
							   RunControlStateMachine::CONFIGURE_TRANSITION_NAME)
								waitSs << " w/" + RunControlStateMachine::
								                      getLastAttemptedConfigureGroup();
							if(numOfThreadsWithWork == 1)
							{
								waitSs << ".. "
								       << broadcastThreadStructs_[lastUnfinishedThread]
								              ->getAppInfo()
								              .getName()
								       << ":"
								       << broadcastThreadStructs_[lastUnfinishedThread]
								              ->getAppInfo()
								              .getId();
							}
							waitSs << __E__;

							time_t secondsLeft =
							    (timeoutSeconds - difftime(time(0), start));
							uint32_t minutesLeft = secondsLeft / 60;
							if(secondsLeft < 10)
								__COUT_WARN__
								    << waitSs.str() << "\n"
								    << "Timeout threshold (for iteration #" << iteration
								    << ") is " << timeoutSeconds / 60 << " minutes... "
								    << secondsLeft << " seconds remaining before timeout!"
								    << __E__;
							else if(lastMinutesLeft != minutesLeft && minutesLeft < 3)
								__COUT_WARN__
								    << waitSs.str() << "\n"
								    << "Timeout threshold (for iteration #" << iteration
								    << ") is " << timeoutSeconds / 60 << " minutes... "
								    << minutesLeft << " minutes remaining before timeout!"
								    << __E__;
							else if((waitIt++) % 20 == 0)
								__COUT__ << waitSs.str() << "\n"
								         << "Timeout threshold (for iteration #"
								         << iteration << ") is " << timeoutSeconds / 60
								         << " minutes (" << secondsLeft
								         << " seconds remaining before timeout)."
								         << __E__;
							else
								__COUTT__ << waitSs.str();
							lastMinutesLeft = minutesLeft;

							waitSs << "\n"
							       << "Timeout threshold (for iteration #" << iteration
							       << ") is " << timeoutSeconds / 60 << " minutes ("
							       << secondsLeft << " seconds remaining before timeout)."
							       << __E__;

							{  // create lock scope that does not include sleep
								std::lock_guard<std::mutex> lock(
								    broadcastCommandStatusUpdateMutex_);
								broadcastCommandStatus_ = waitSs.str();
							}
							usleep(100 * 1000 /*100ms*/);
						}

					} while(!done);
					__COUT__ << "All threads done with priority level work." << __E__;
				}  // end thread complete verification

			}  // end supervisor broadcast loop for each priority

			//			if (!proceed)
			//			{
			//				__COUT__ << "Breaking out of primary loop." << __E__;
			//				break;
			//			}

			// Wait for remote gateways to complete this iteration pass
			// (may set broadcastIterationsDone_ = false if any remote requests another iteration)
			broadcastMessageToRemoteGatewaysComplete(originalMessage, iteration);

			__COUT__ << "After iteration=" << iteration << " for command '" << command
			         << "': broadcastIterationsDone_=" << broadcastIterationsDone_
			         << __E__;

			if(iteration || !broadcastIterationsDone_)
			{
				if(!broadcastIterationsDone_ && isRemoteSubsystemIteration_)
				{
					// Subsystem iteration driven by top-level: signal needNextIteration and wait
					unsigned int nextIteration = iteration + 1;
					{
						std::lock_guard<std::mutex> lock(
						    broadcastCommandStatusUpdateMutex_);
						broadcastCommandStatus_ =
						    "needNextIteration:" + std::to_string(nextIteration);
					}
					__COUT__ << "Top-level driven subsystem iteration: signaling "
					            "needNextIteration:"
					         << nextIteration << ", waiting for re-send..." << __E__;

					{
						std::unique_lock<std::mutex> lock(remoteIterationMutex_);
						if(remoteIterationIndex_ < nextIteration)
						{
							auto deadline = std::chrono::steady_clock::now() +
							                std::chrono::minutes(4);
							while(remoteIterationIndex_ < nextIteration &&
							      !RunControlStateMachine::asyncFailureReceived_)
							{
								remoteIterationCV_.wait_for(lock,
								                            std::chrono::seconds(1));
								if(std::chrono::steady_clock::now() >= deadline &&
								   remoteIterationIndex_ < nextIteration)
								{
									__SS__ << "Timeout (4 min) waiting for top-level to "
									          "send "
									          "IterationIndex:"
									       << nextIteration
									       << " -- top-level may have lost communication."
									       << __E__;
									__SS_THROW__;
								}
							}
						}
						if(RunControlStateMachine::asyncFailureReceived_)
						{
							__SS__ << "Async failure received while waiting for "
							          "iteration re-send!"
							       << __E__;
							__SS_THROW__;
						}
						if(remoteIterationIndex_ != nextIteration)
						{
							__SS__ << "Unexpected IterationIndex re-send: got "
							       << remoteIterationIndex_ << " but expected "
							       << nextIteration << __E__;
							__SS_THROW__;
						}
						__COUT__ << "Received iteration re-send: IterationIndex:"
						         << remoteIterationIndex_ << __E__;
					}

					{
						std::lock_guard<std::mutex> lock(
						    broadcastCommandStatusUpdateMutex_);
						broadcastCommandStatus_ =
						    "Completed iteration: " + std::to_string(iteration) +
						    " (top-level driven, continuing)";
					}
				}
				else
				{
					std::stringstream ss;
					if(iteration > 0)
						ss << "Iteration " << iteration << ": ";
					ss << (broadcastIterationsDone_ ? "complete (all done)"
					                                : "complete (need more iterations)")
					   << __E__;
					__COUT__ << ss.str();

					std::lock_guard<std::mutex> lock(broadcastCommandStatusUpdateMutex_);
					broadcastCommandStatus_ = ss.str();
				}
			}
			++iteration;

		} while(!broadcastIterationsDone_);

		__COUT__ << "Iteration loop complete after " << iteration
		         << " iteration(s) for command '" << command << "'" << __E__;

		{
			std::lock_guard<std::mutex> lock(remoteIterationMutex_);
			isRemoteSubsystemIteration_ = false;
			remoteIterationIndex_       = 0;
		}

		// Check for a user cancel that arrived during the final SOAP call of the loop,
		// which would not have been caught by the per-supervisor checkForAsyncError() call.
		checkForAsyncError();

		RunControlStateMachine::theProgressBar_.step();
	}  // end main transition broadcast try
	catch(...)
	{
		__COUT__ << "Exception caught, exiting broadcast threads..." << __E__;

		{
			std::lock_guard<std::mutex> lock(remoteIterationMutex_);
			isRemoteSubsystemIteration_ = false;
			remoteIterationIndex_       = 0;
		}

		// Signal all threads to exit and wait for them to finish gracefully.
		// supervisorIterationsDone is heap-allocated (shared_ptr) and each
		// BroadcastMessageStruct holds a shared_ptr copy, so the underlying
		// bool arrays remain valid for as long as any thread holds a reference.
		// This prevents UAF even if the timeout below expires before threads exit.
		signalAndWaitForBroadcastThreads(numberOfThreads);

		throw;  // re-throw
	}

	if(numberOfThreads)
	{
		std::stringstream ss;
		ss << "All local transitions completed. Wrapping up, exiting broadcast threads..."
		   << __E__;
		__COUT__ << ss.str();

		{  // create lock scope and clear status
			std::lock_guard<std::mutex> lock(broadcastCommandStatusUpdateMutex_);
			broadcastCommandStatus_ = ss.str();
		}

		// Signal all threads to exit and wait for them to finish gracefully.
		// supervisorIterationsDone is heap-allocated (shared_ptr) and each
		// BroadcastMessageStruct holds a shared_ptr copy, so the underlying
		// bool arrays remain valid for as long as any thread holds a reference.
		// This prevents UAF even if the timeout below expires before threads exit.
		signalAndWaitForBroadcastThreads(numberOfThreads);
	}

	RunControlStateMachine::theProgressBar_.step();

	{  // create lock scope and clear status
		std::lock_guard<std::mutex> lock(broadcastCommandStatusUpdateMutex_);
		broadcastCommandStatus_ = "";
	}

	RunControlStateMachine::theProgressBar_.step();
	__COUT__ << "Broadcast complete." << __E__;
}  // end broadcastMessage()

//==============================================================================
// signalAndWaitForBroadcastThreads
//	Signal all broadcast threads to exit, then wait (with a timeout) for
//	every thread to set working_=false.  Called from both the normal-completion
//	and exception paths in broadcastMessage() to avoid duplicating this logic.
//
//	If threads are still stuck after the timeout, they are abandoned (not
//	pthread_cancel'd).  pthread_cancel with PTHREAD_CANCEL_ASYNCHRONOUS is
//	unsafe in C++ because __forced_unwind caught by intermediate catch-all
//	handlers causes "FATAL: exception not rethrown".  The stuck detached
//	threads hold shared_ptr copies of their BroadcastThreadStruct and of
//	supervisorIterationsDone, so all heap data remains valid until the
//	thread's blocking call eventually returns and the thread exits on its own.
void GatewaySupervisor::signalAndWaitForBroadcastThreads(unsigned int numberOfThreads)
{
	for(unsigned int i = 0; i < numberOfThreads; ++i)
		broadcastThreadStructs_[i]->exitThread_ = true;

	const int timeoutSeconds =
	    3;  //time for threads to finish (short: threads are detached and safe to abandon quickly)
	time_t start;
	time(&start);
	bool allExited = false;
	while(!allExited)
	{
		allExited = true;
		for(unsigned int i = 0; i < numberOfThreads; ++i)
			if(broadcastThreadStructs_[i]->working_)
			{
				allExited = false;
				break;
			}
		if(!allExited)
		{
			if(difftime(time(0), start) > timeoutSeconds)
			{
				__COUT_WARN__ << "Timeout waiting for broadcast threads to exit! "
				              << "Abandoning stuck threads (they are detached and will "
				              << "exit on their own when their blocking calls return)."
				              << __E__;

				for(unsigned int i = 0; i < numberOfThreads; ++i)
					if(broadcastThreadStructs_[i]->working_)
					{
						__COUT_WARN__ << "Broadcast thread " << i
						              << " is still running and will be abandoned."
						              << __E__;
					}

				break;
			}
			usleep(100 * 1000 /*100ms*/);
		}  //end handling of remaining threads
	}      //end wait while loop
}  // end signalAndWaitForBroadcastThreads()

//==============================================================================
void GatewaySupervisor::broadcastMessageToRemoteGateways(
    const xoap::MessageReference message, unsigned int iteration)
{
	if(!remoteGatewayApps_.size())
		return;

	SOAPCommand commandObj = SOAPUtilities::translate(message);
	std::string command    = commandObj.getCommand();
	__COUTV__(command);

	activeStateMachineSubsystemCommonList_         = "";  // clear
	activeStateMachineSubsystemCommonOverrideList_ = "";  // clear
	if(command == RunControlStateMachine::CONFIGURE_TRANSITION_NAME)
	{
		//build "SubsystemCommon" and "SubsystemCommonOverride" table list:
		//	Cached at Configure transition CSV list of Table/Versions
		//	specified as table alias "SubsystemCommon" and "SubsystemCommonOverride" by user at top-level Primary Gateway,
		//	to be merged into the configuration for all subsystems (e.g. for DCS/DQM) when configuring.
		activeStateMachineSubsystemCommonList_ =
		    StringMacros::setToString(theConfigurationManager_->getVersionAliases(
		        ConfigurationManager::SUBSYSTEM_COMMON_VERSION_ALIAS));
		__SUP_COUTV__(activeStateMachineSubsystemCommonList_);
		activeStateMachineSubsystemCommonOverrideList_ =
		    StringMacros::setToString(theConfigurationManager_->getVersionAliases(
		        ConfigurationManager::SUBSYSTEM_COMMON_OVERRIDE_VERSION_ALIAS));
		__SUP_COUTV__(activeStateMachineSubsystemCommonOverrideList_);
	}

	// Brief lock to snapshot remoteGatewayApps_ for processing without holding mutex
	__COUT__ << "broadcastMessageToRemoteGateways v2 (copy-process-writeback) iteration="
	         << iteration << __E__;
	std::vector<GatewaySupervisor::RemoteGatewayInfo> localApps;
	{
		std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
		localApps = remoteGatewayApps_;
	}

	std::set<std::string> commandedApps;

	for(auto& remoteGatewayApp : localApps)
	{
		if(!remoteGatewayApp.fsm_included)
		{
			__COUT__ << "Skipping excluded Remote gateway '"
			         << remoteGatewayApp.appInfo.name << "' for FSM command = " << command
			         << __E__;
			continue;  //skip if not included
		}

		if(iteration == 0)
		{
			remoteGatewayApp.iterationsDone =
			    false;  //reset iteration state on initial send
		}
		else if(remoteGatewayApp.iterationsDone)
		{
			__COUT__ << "Skipping Remote gateway '" << remoteGatewayApp.appInfo.name
			         << "' - already done with iterations for FSM command = " << command
			         << __E__;
			continue;  //skip if already done with all iterations
		}

		std::string localCommand =
		    command;  //copy local command in case of modifications specific to remoteGatewayApp

		if(remoteGatewayApp.fsm_mode == RemoteGatewayInfo::FSM_ModeTypes::DoNotHalt &&
		   //do not allow halt/err transitions:
		   (command == RunControlStateMachine::ERROR_TRANSITION_NAME ||
		    command == RunControlStateMachine::FAIL_TRANSITION_NAME ||
		    command == RunControlStateMachine::HALT_TRANSITION_NAME ||
		    command == RunControlStateMachine::ABORT_TRANSITION_NAME))
		{
			//send Stop to DoNotHalt subsystems that are in Running/Paused when Halt or Abort is requested
			bool sendStop = command == RunControlStateMachine::ABORT_TRANSITION_NAME ||
			                (command == RunControlStateMachine::HALT_TRANSITION_NAME &&
			                 (remoteGatewayApp.appInfo.status ==
			                      RunControlStateMachine::RUNNING_STATE_NAME ||
			                  remoteGatewayApp.appInfo.status ==
			                      RunControlStateMachine::PAUSED_STATE_NAME));
			if(sendStop)
			{
				localCommand = RunControlStateMachine::STOP_TRANSITION_NAME;

				__COUT_INFO__ << "Modifying '" << remoteGatewayApp.getFsmMode()
				              << "' Remote gateway '" << remoteGatewayApp.appInfo.name
				              << "' for FSM command = " << command << " --> "
				              << localCommand << __E__;
			}
			else
			{
				__COUT_INFO__ << "Skipping '" << remoteGatewayApp.getFsmMode()
				              << "' Remote gateway '" << remoteGatewayApp.appInfo.name
				              << "' for FSM command = " << command << __E__;
				continue;  //skip if not included
			}
		}

		if(remoteGatewayApp.fsm_mode == RemoteGatewayInfo::FSM_ModeTypes::OnlyConfigure &&
		   !  //invert of allowed situations:
		   (remoteGatewayApp.appInfo.status ==
		        RunControlStateMachine::INITIAL_STATE_NAME ||
		    remoteGatewayApp.appInfo.status ==
		        RunControlStateMachine::HALTED_STATE_NAME ||
		    remoteGatewayApp.appInfo.status.find(
		        RunControlStateMachine::FAILED_STATE_NAME) == 0 ||
		    remoteGatewayApp.appInfo.status.find("Error") !=
		        std::string::npos ||  //	case "Error", "Soft-Error"
		    (remoteGatewayApp.appInfo.status ==
		         RunControlStateMachine::HALTED_STATE_NAME &&
		     command == RunControlStateMachine::CONFIGURE_TRANSITION_NAME)))
		{
			__COUT_INFO__ << "Skipping '" << remoteGatewayApp.getFsmMode()
			              << "' Remote gateway '" << remoteGatewayApp.appInfo.name
			              << "' w/status = " << remoteGatewayApp.appInfo.status
			              << "... for FSM command = " << command << __E__;
			continue;  //skip if not included
		}

		//Likely top level does not want to reinitialize a configured subsystem (just wants self and other subsystems to catch up)
		if(remoteGatewayApp.appInfo.status ==
		       RunControlStateMachine::CONFIGURED_STATE_NAME &&
		   remoteGatewayApp.appInfo.progress == 100 &&
		   (command == RunControlStateMachine::INIT_TRANSITION_NAME ||
		    command == RunControlStateMachine::CONFIGURE_TRANSITION_NAME))
		{
			__COUT_INFO__ << "Ignoring '" << command
			              << "' transition for Remote subsystem '"
			              << remoteGatewayApp.appInfo.name
			              << ".' It is already configured (assuming top-level does not "
			                 "mean to re-initialize subsystem)."
			              << __E__;
			continue;
		}

		//construct command params based on remote gateway settings
		std::string commandAndParams = localCommand;
		if(commandObj.hasParameters())
		{
			//parameters over UDP are much simpler than xoap message, so filter
			for(const auto& param : commandObj.getParameters())
			{
				__COUTTV__(param.first);
				__COUTTV__(param.second);
				if(param.first == "ConfigurationAlias")
				{
					if(remoteGatewayApp.selected_config_alias != "")  //replace
						commandAndParams += "," + remoteGatewayApp.selected_config_alias;
					else
						commandAndParams += "," + param.second;
				}
				else if(param.first == "RunNumber")
				{
					commandAndParams += "," +  //param.first + ":" +
					                    param.second;
				}

				// else
				// 	commandAndParams += "," + param.first + ":" + param.second;
			}
			__COUTV__(commandAndParams);
		}
		__COUT__ << "Launching FSM command '" << commandAndParams
		         << "' on Remote gateway '" << remoteGatewayApp.appInfo.name << "'..."
		         << __E__;

		if(remoteGatewayApp.command != "" && remoteGatewayApp.command != "Sent" &&
		   iteration == 0)
		{
			__SUP_SS__ << "Can not target the remote subsystem '"
			           << remoteGatewayApp.appInfo.name << "' with command '"
			           << localCommand << "' which already has a pending command '"
			           << remoteGatewayApp.command
			           << ".' Please try again after the pending command is sent."
			           << __E__;
			__SUP_SS_THROW__;
		}

		commandedApps.emplace(remoteGatewayApp.fullName);

		remoteGatewayApp.config_dump = "";  //clear, must come from new command completion
		remoteGatewayApp.command     = commandAndParams;

		remoteGatewayApp.command +=
		    "," + COMMAND_PARAM_ITERATION_INDEX_PREAMBLE + std::to_string(iteration);

		if(activeStateMachineSubsystemCommonList_.size())
			remoteGatewayApp.command +=
			    "," + COMMAND_PARAM_SUBSYSTEM_COMMON_PREAMBLE +
			    StringMacros::encodeURIComponent(activeStateMachineSubsystemCommonList_);

		if(activeStateMachineSubsystemCommonOverrideList_.size())
			remoteGatewayApp.command +=
			    "," + COMMAND_PARAM_SUBSYSTEM_COMMON_OVERRIDE_PREAMBLE +
			    StringMacros::encodeURIComponent(
			        activeStateMachineSubsystemCommonOverrideList_);

		//note: LogEntry must be last parameter!
		std::string logEntry = getLastLogEntry(localCommand);
		if(logEntry.size())
			remoteGatewayApp.command += "," + COMMAND_PARAM_LOG_ENTRY_PREAMBLE +
			                            StringMacros::encodeURIComponent(logEntry);

		remoteGatewayApp.fsmName =
		    activeStateMachineName_;  //fsmName will be prepended during command send
		//force status for immediate user feedback
		remoteGatewayApp.appInfo.status   = "Launching " + commandAndParams;
		remoteGatewayApp.appInfo.progress = 0;

		__SUP_COUTV__(remoteGatewayApp.command);
	}  //end remote gateway broadcast loop

	// Brief lock to write back only entries that were actually modified above
	{
		std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
		for(const auto& localApp : localApps)
		{
			bool wasCommanded =
			    commandedApps.find(localApp.fullName) != commandedApps.end();
			if(!wasCommanded && iteration != 0)
				continue;

			for(auto& rga : remoteGatewayApps_)
				if(rga.fullName == localApp.fullName)
				{
					rga.iterationsDone = localApp.iterationsDone;
					if(wasCommanded)
					{
						rga.command          = localApp.command;
						rga.fsmName          = localApp.fsmName;
						rga.config_dump      = localApp.config_dump;
						rga.appInfo.status   = localApp.appInfo.status;
						rga.appInfo.progress = localApp.appInfo.progress;
					}
					break;
				}
		}
	}
}  // end broadcastMessageToRemoteGateways()

//==============================================================================
void GatewaySupervisor::broadcastMessageToRemoteGatewaysComplete(
    const xoap::MessageReference message, unsigned int iterationIndex)
{
	std::string command = SOAPUtilities::translate(message).getCommand();
	__COUTV__(command);
	std::string destinationState = theStateMachine_.getTransitionFinalStateName(command);
	__COUTV__(destinationState);

	size_t countOfRemoteGateways = 0;

	std::map<std::string /* fullName */, int /* unknownCount */> unknownResponseCounts;

	bool         done      = command == "Error";  //dont check for done if Error'ing
	size_t       iteration = 0;
	const size_t secsPerIteration = 1;
	const size_t maxIterations =
	    10 * 60 / secsPerIteration;  //roughly 10 minutes (1s per iteration)
	std::map<std::string /* name */, size_t /* progress100cnt */>
	    progress100cnt;  // make sure remote subsystem is not in unanticipated state
	while(!done)
	{
		++iteration;
		__COUTT__ << "Checking " << remoteGatewayApps_.size()
		          << " remote gateway(s) completion for command = " << command
		          << " .. check #" << iteration << __E__;

		done                                 = true;
		size_t       remainingRemoteGateways = 0;
		size_t       totalRemoteGateways     = 0;
		std::string  lastRemainingName;
		std::string  lastRemainingStatus;
		unsigned int lastRemainingProgress = 0;

		std::vector<GatewaySupervisor::RemoteGatewayInfo> remoteGatewayApps;  //local copy
		{  //lock for remainder of scope
			std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
			__SUP_COUTVS__(TLVL_RemoteFSMRequests, remoteGatewayApps_.size());
			countOfRemoteGateways = remoteGatewayApps_.size();
			remoteGatewayApps     = remoteGatewayApps_;
			if(remoteGatewayApps_.size())
				__SUP_COUT_TYPE__(TLVL_DEBUG + TLVL_RemoteFSMRequests)
				    << __COUT_HDR__ << remoteGatewayApps_[0].command << " "
				    << (remoteGatewayApps_[0].appInfo.status) << __E__;
		}

		for(auto& remoteGatewayApp : remoteGatewayApps)
		{
			//skip remote gateways that were not commanded
			if(!remoteGatewayApp.fsm_included)
				continue;
			if(remoteGatewayApp.iterationsDone)
				continue;  //skip if already done with all iterations
			if(remoteGatewayApp.fsm_mode == RemoteGatewayInfo::FSM_ModeTypes::DoNotHalt &&
			   //do not allow halt/err transitions:
			   (command == RunControlStateMachine::ERROR_TRANSITION_NAME ||
			    command == RunControlStateMachine::FAIL_TRANSITION_NAME ||
			    command == RunControlStateMachine::HALT_TRANSITION_NAME ||
			    command == RunControlStateMachine::ABORT_TRANSITION_NAME))
				continue;
			if(remoteGatewayApp.fsm_mode ==
			       RemoteGatewayInfo::FSM_ModeTypes::OnlyConfigure &&
			   !  //invert of allowed situations:
			   (remoteGatewayApp.appInfo.status ==
			        RunControlStateMachine::INITIAL_STATE_NAME ||
			    remoteGatewayApp.appInfo.status ==
			        RunControlStateMachine::HALTED_STATE_NAME ||
			    remoteGatewayApp.appInfo.status.find(
			        RunControlStateMachine::FAILED_STATE_NAME) == 0 ||
			    remoteGatewayApp.appInfo.status.find("Error") !=
			        std::string::npos ||  //	case "Error", "Soft-Error"
			    (remoteGatewayApp.appInfo.status ==
			         RunControlStateMachine::HALTED_STATE_NAME &&
			     command == RunControlStateMachine::CONFIGURE_TRANSITION_NAME)))
				continue;

			if(remoteGatewayApp.appInfo.status ==
			       RunControlStateMachine::CONFIGURED_STATE_NAME &&
			   remoteGatewayApp.appInfo.progress == 100 &&
			   (command == RunControlStateMachine::INIT_TRANSITION_NAME ||
			    command == RunControlStateMachine::CONFIGURE_TRANSITION_NAME))
				continue;  //Likely top level does not want to reinitialize a configured subsystem (just wants self and other subsystems to catch up)

			++totalRemoteGateways;

			//if here, was commanded, so check status

			if(!((remoteGatewayApp.appInfo.status == destinationState &&
			      remoteGatewayApp.appInfo.progress == 100) ||
			     remoteGatewayApp.appInfo.status.find("Error") != std::string::npos ||
			     remoteGatewayApp.appInfo.status.find("Fail") != std::string::npos))
			{
				// Check if remote gateway is requesting another iteration
				// Format: "needNextIteration:N" where N is the next iteration index wanted
				// Must be lock-step: after sending iteration I, only accept needNextIteration:(I+1)
				const std::string needNextIterationPrefix = "needNextIteration:";
				size_t            needNextIterationPos =
				    remoteGatewayApp.appInfo.detail.find(needNextIterationPrefix);
				if(needNextIterationPos != std::string::npos)
				{
					unsigned int requestedIteration = 0;
					try
					{
						requestedIteration =
						    std::stoul(remoteGatewayApp.appInfo.detail.substr(
						        needNextIterationPos + needNextIterationPrefix.length()));
					}
					catch(...)
					{
						__COUT_WARN__ << "Failed to parse iteration index from detail '"
						              << remoteGatewayApp.appInfo.detail
						              << "' for Remote gateway '"
						              << remoteGatewayApp.appInfo.name
						              << "' -- ignoring, will keep polling." << __E__;
						done = false;
						++remainingRemoteGateways;
						lastRemainingName     = remoteGatewayApp.appInfo.name;
						lastRemainingStatus   = remoteGatewayApp.appInfo.status;
						lastRemainingProgress = remoteGatewayApp.appInfo.progress;
						continue;
					}

					unsigned int expectedIteration = iterationIndex + 1;
					if(requestedIteration != expectedIteration)
					{
						if(requestedIteration < expectedIteration)
						{
							// Stale needNextIteration from a previous iteration pass -- ignore and keep polling
							__COUT__ << "Ignoring stale needNextIteration:"
							         << requestedIteration << " from '"
							         << remoteGatewayApp.appInfo.name << "' (expecting "
							         << expectedIteration
							         << "), waiting for fresh status..." << __E__;
							done = false;
							++remainingRemoteGateways;
							lastRemainingName     = remoteGatewayApp.appInfo.name;
							lastRemainingStatus   = remoteGatewayApp.appInfo.status;
							lastRemainingProgress = remoteGatewayApp.appInfo.progress;
							continue;
						}
						else
						{
							__SS__ << "Unexpected iteration index mismatch from Remote "
							          "gateway '"
							       << remoteGatewayApp.appInfo.name
							       << "': requested iteration " << requestedIteration
							       << " but expected " << expectedIteration
							       << " (top-level sent iteration " << iterationIndex
							       << ")" << __E__;
							__SS_THROW__;
						}
					}

					progress100cnt[remoteGatewayApp.fullName] = 0;

					__COUT__ << "Remote gateway '" << remoteGatewayApp.appInfo.name
					         << "' requesting next iteration " << requestedIteration
					         << " (matches expected after sending iteration "
					         << iterationIndex << ") for command '" << command << "'"
					         << __E__;

					broadcastIterationsDone_ = false;
					// This gateway is done with this iteration pass;
					// do not count as remaining (will be re-sent in next iteration pass)
					continue;
				}

				if(progress100cnt.find(remoteGatewayApp.fullName) == progress100cnt.end())
					progress100cnt[remoteGatewayApp.fullName] = 0;

				if(remoteGatewayApp.appInfo.progress == 100)
					progress100cnt[remoteGatewayApp.fullName]++;
				else
					progress100cnt[remoteGatewayApp.fullName] = 0;

				if(progress100cnt[remoteGatewayApp.fullName] >
				   7)  //roughly 15 seconds not moving
				{
					__SS__ << "Something is wrong with FSM command '" << command
					       << "' at Remote gateway '" << remoteGatewayApp.appInfo.name
					       << "' - it seems command was ignored or an unanticipated "
					          "state was reached."
					       << __E__;
					__SS_THROW__;
				}

				//not done
				if(remoteGatewayApp.appInfo.status == SupervisorInfo::APP_STATUS_UNKNOWN)
				{
					unknownResponseCounts[remoteGatewayApp.fullName]++;
					if(unknownResponseCounts[remoteGatewayApp.fullName] > 2)
					{
						__SS__ << "Can not complete FSM command '" << command
						       << "' with unknown status from Remote gateway '"
						       << remoteGatewayApp.appInfo.name
						       << "' - it seems communication was lost. Please check the "
						          "connection or notify admins."
						       << __E__;
						__SS_THROW__;
					}
				}
				else
					unknownResponseCounts[remoteGatewayApp.fullName] = 0;
				__COUT__ << "Remote gateway '" << remoteGatewayApp.appInfo.name
				         << "' not done w/command '" << command
				         << "' status = " << remoteGatewayApp.appInfo.status
				         << ",... progress = " << remoteGatewayApp.appInfo.progress
				         << ",... unkCnt = "
				         << unknownResponseCounts.at(remoteGatewayApp.fullName) << __E__;

				done = false;
				++remainingRemoteGateways;
				lastRemainingName     = remoteGatewayApp.appInfo.name;
				lastRemainingStatus   = remoteGatewayApp.appInfo.status;
				lastRemainingProgress = remoteGatewayApp.appInfo.progress;

				if(iteration > maxIterations)  //roughly 10 minutes
				{
					__SS__ << "Can not complete FSM command '" << command
					       << "' with Remote gateway '" << remoteGatewayApp.appInfo.name
					       << "' - the command has taken too long. Please check the "
					          "Remote gateway or notify admins."
					       << __E__;
					__SS_THROW__;
				}
			}
			else
			{
				//done or error?
				if(remoteGatewayApp.appInfo.status.find("Error") != std::string::npos ||
				   remoteGatewayApp.appInfo.status.find("Fail") != std::string::npos)
				{
					__SS__ << "Command '" << command << "' failed at Remote gateway '"
					       << remoteGatewayApp.appInfo.name
					       << "' - here was the error:\n\n"
					       << remoteGatewayApp.appInfo.status << __E__;
					__SS_THROW__;
				}

				__COUT__ << "Done Remote gateway '" << remoteGatewayApp.appInfo.name
				         << "' w/command '" << command
				         << "' status = " << remoteGatewayApp.appInfo.status
				         << ",... progress = " << remoteGatewayApp.appInfo.progress
				         << " - marking iterationsDone=true" << __E__;

				// Mark as done on the actual member so future iteration passes skip it
				{
					std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
					for(auto& rga : remoteGatewayApps_)
						if(rga.fullName == remoteGatewayApp.fullName)
						{
							rga.iterationsDone = true;
							break;
						}
				}
			}
		}

		if(!done)
		{
			std::stringstream waitSs;
			if(iterationIndex > 0)
				waitSs << "Iteration " << iterationIndex << ": ";
			waitSs << "Waiting on " << remainingRemoteGateways << " of "
			       << totalRemoteGateways << " remote gateways to finish command '"
			       << command << "'";
			if(remainingRemoteGateways == 1)
				waitSs << ".. Last is '" << lastRemainingName << "' w/progress='"
				       << lastRemainingProgress << "' and status='" << lastRemainingStatus
				       << "'";
			waitSs << __E__;
			uint32_t timeUntilTimeout = (maxIterations - iteration) *
			                            secsPerIteration;  // x seconds per iteration
			if(timeUntilTimeout / 60 < 1)
				waitSs << "(wait count = " << iteration << ", " << timeUntilTimeout
				       << " seconds until timeout)" << __E__;
			else
				waitSs << "(wait count = " << iteration << ", " << timeUntilTimeout / 60
				       << " minutes " << timeUntilTimeout % 60
				       << " seconds until timeout)" << __E__;

			if(iteration % 2 == 1)
				__COUT__ << waitSs.str();

			{  // create lock scope that does not include sleep
				std::lock_guard<std::mutex> lock(broadcastCommandStatusUpdateMutex_);
				broadcastCommandStatus_ = waitSs.str();
			}
		}
		if(!done)
			sleep(secsPerIteration);

		checkForAsyncError();
	}  //end primary while loop

	__COUT__ << "Done with " << countOfRemoteGateways
	         << " remote gateway(s) command = " << command << __E__;
}  // end broadcastMessageToRemoteGatewaysComplete()

//==============================================================================
/// LoginRequest
///  handles all users login/logout actions from web GUI.
///  NOTE: there are two ways for a user to be logged out: timeout or manual logout
///      System logbook messages are generated for login and logout
void GatewaySupervisor::loginRequest(xgi::Input* in, xgi::Output* out)
{
	std::chrono::steady_clock::time_point startClock = std::chrono::steady_clock::now();
	cgicc::Cgicc                          cgi(in);
	std::string Command = CgiDataUtilities::getData(cgi, "RequestType");
	__COUT__ << "*** Login RequestType = " << Command << " time=" << time(0) << __E__;

	// RequestType Commands:
	// login
	// sessionId
	// checkCookie
	// logout

	try
	{
		// always cleanup expired entries and get a vector std::string of logged out users
		std::vector<std::string> loggedOutUsernames;
		theWebUsers_.cleanupExpiredEntries(&loggedOutUsernames);
		bool doLog = false;
		if(loggedOutUsernames.size())
		{
			try
			{
				doLog = __ENV__("OTS_LOG_LOGIN_LOGOUT") == std::string("1");
			}
			catch(...)
			{ /* ignore errors */
				;
			}
		}
		for(unsigned int i = 0; i < loggedOutUsernames.size();
		    ++i)  // Log logout for logged out users
			if(doLog)
				makeSystemLogEntry(loggedOutUsernames[i] + " login timed out.");

		if(Command == "sessionId")
		{
			//	When client loads page, client submits unique user id and receives random
			// sessionId from server 	Whenever client submits user name and password it is
			// jumbled by sessionId when sent to server and sent along with UUID. Server uses
			// sessionId to unjumble.
			//
			//	Server maintains list of active sessionId by UUID
			//	sessionId expires after set time if no login attempt (e.g. 5 minutes)
			std::string uuid = CgiDataUtilities::postData(cgi, "uuid");

			std::string sid = theWebUsers_.createNewLoginSession(
			    uuid, cgi.getEnvironment().getRemoteAddr() /* ip */);

			//		__COUT__ << "uuid = " << uuid << __E__;
			//		__COUT__ << "SessionId = " << sid.substr(0, 10) << __E__;
			*out << sid;
		}
		else if(Command == "checkCookie")
		{
			uint64_t    uid;
			std::string uuid;
			std::string jumbledUser;
			std::string cookieCode;

			//	If client has a cookie, client submits cookie and username, jumbled, to see if
			// cookie and user are still active 	if active, valid cookie code is returned
			// and  name to display, in XML
			// 	if not, return 0
			// 	params:
			//		uuid 			- unique user id, to look up sessionId
			//		ju 				- jumbled user name
			//		CookieCode 		- cookie code to check

			uuid        = CgiDataUtilities::postData(cgi, "uuid");
			jumbledUser = CgiDataUtilities::postData(cgi, "ju");
			cookieCode  = CgiDataUtilities::postData(cgi, "cc");

			//		__COUT__ << "uuid = " << uuid << __E__;
			//		__COUT__ << "Cookie Code = " << cookieCode.substr(0, 10) << __E__;
			//		__COUT__ << "jumbledUser = " << jumbledUser.substr(0, 10) << __E__;

			// If cookie code is good, then refresh and return with display name, else return
			// 0 as CookieCode value
			uid = theWebUsers_.isCookieCodeActiveForLogin(
			    uuid,
			    cookieCode,
			    jumbledUser);  // after call jumbledUser holds displayName on success

			if(uid == theWebUsers_.NOT_FOUND_IN_DATABASE)
			{
				__COUT__ << "cookieCode invalid" << __E__;
				jumbledUser = "";   // clear display name if failure
				cookieCode  = "0";  // clear cookie code if failure
			}
			else
				__COUT__ << "cookieCode is good." << __E__;

			// return xml holding cookie code and display name
			HttpXmlDocument xmldoc(cookieCode, jumbledUser);

			theWebUsers_.insertSettingsForUser(uid, &xmldoc);  // insert settings

			xmldoc.outputXmlDocument((std::ostringstream*)out);
		}
		else if(Command == "login")
		{
			//	If login attempt or create account, jumbled user and pw are submitted
			//	if successful, valid cookie code and display name returned.
			// 	if not, return 0
			// 	params:
			//		uuid 			- unique user id, to look up sessionId
			//		nac				- new account code for first time logins
			//		ju 				- jumbled user name
			//		jp		 		- jumbled password

			std::string uuid           = CgiDataUtilities::postData(cgi, "uuid");
			std::string newAccountCode = CgiDataUtilities::postData(cgi, "nac");
			std::string jumbledUser    = CgiDataUtilities::postData(cgi, "ju");
			std::string jumbledPw      = CgiDataUtilities::postData(cgi, "jp");

			//		__COUT__ << "jumbledUser = " << jumbledUser.substr(0, 10) << __E__;
			//		__COUT__ << "jumbledPw = " << jumbledPw.substr(0, 10) << __E__;
			//		__COUT__ << "uuid = " << uuid << __E__;
			//		__COUT__ << "nac =-" << newAccountCode << "-" << __E__;

			uint64_t uid = theWebUsers_.attemptActiveSession(
			    uuid,
			    jumbledUser,
			    jumbledPw,
			    newAccountCode,
			    cgi.getEnvironment()
			        .getRemoteAddr());  // after call jumbledUser holds displayName on success

			if(uid >= theWebUsers_.ACCOUNT_ERROR_THRESHOLD)
			{
				__COUT__ << "Login invalid." << __E__;
				jumbledUser = "";          // clear display name if failure
				if(newAccountCode != "1")  // indicates uuid not found
					newAccountCode = "0";  // clear cookie code if failure
			}
			else  // Log login in logbook for active experiment
			{
				bool doLog = false;
				try
				{
					doLog = __ENV__("OTS_LOG_LOGIN_LOGOUT") == std::string("1");
				}
				catch(...)
				{ /* ignore errors */
					;
				}

				if(doLog)
					makeSystemLogEntry(theWebUsers_.getUsersUsername(uid) +
					                   " logged in.");
			}

			//__COUT__ << "new cookieCode = " << newAccountCode.substr(0, 10) << __E__;

			HttpXmlDocument xmldoc(newAccountCode, jumbledUser);

			// include extra error detail
			if(uid == theWebUsers_.ACCOUNT_INACTIVE)
				xmldoc.addTextElementToData("Error",
				                            "Account is inactive. Notify admins.");
			else if(uid == theWebUsers_.ACCOUNT_BLACKLISTED)
				xmldoc.addTextElementToData("Error",
				                            "Account is blacklisted. Notify admins.");

			theWebUsers_.insertSettingsForUser(uid, &xmldoc);  // insert settings

			// insert active session count for user

			if(uid != theWebUsers_.NOT_FOUND_IN_DATABASE)
			{
				uint64_t asCnt =
				    theWebUsers_.getActiveSessionCountForUser(uid) -
				    1;  // subtract 1 to remove just started session from count
				char asStr[20];
				sprintf(asStr, "%lu", asCnt);
				xmldoc.addTextElementToData("user_active_session_count", asStr);
			}

			xmldoc.outputXmlDocument((std::ostringstream*)out);
		}
		else if(Command == "cert")
		{
			//	If login attempt or create account, jumbled user and pw are submitted
			//	if successful, valid cookie code and display name returned.
			// 	if not, return 0
			// 	params:
			//		uuid 			- unique user id, to look up sessionId
			//		nac				- new account code for first time logins
			//		ju 				- jumbled user name
			//		jp		 		- jumbled password

			std::string uuid = CgiDataUtilities::postData(cgi, "uuid");
			std::string jumbledEmail =
			    cgicc::form_urldecode(CgiDataUtilities::getData(cgi, "httpsUser"));
			std::string username   = "";
			std::string cookieCode = "";

			//		__COUT__ << "CERTIFICATE LOGIN REUEST RECEVIED!!!" << __E__;
			//		__COUT__ << "jumbledEmail = " << jumbledEmail << __E__;
			//		__COUT__ << "uuid = " << uuid << __E__;

			uint64_t uid = theWebUsers_.attemptActiveSessionWithCert(
			    uuid,
			    jumbledEmail,
			    cookieCode,
			    username,
			    cgi.getEnvironment()
			        .getRemoteAddr());  // after call jumbledUser holds displayName on success

			if(uid == theWebUsers_.NOT_FOUND_IN_DATABASE)
			{
				__COUT__ << "cookieCode invalid" << __E__;
				jumbledEmail = "";     // clear display name if failure
				if(cookieCode != "1")  // indicates uuid not found
					cookieCode = "0";  // clear cookie code if failure
			}
			else  // Log login in logbook for active experiment
			{
				bool doLog = false;
				try
				{
					doLog = __ENV__("OTS_LOG_LOGIN_LOGOUT") == std::string("1");
				}
				catch(...)
				{ /* ignore errors */
					;
				}

				if(doLog)
					makeSystemLogEntry(theWebUsers_.getUsersUsername(uid) +
					                   " logged in.");
			}

			//__COUT__ << "new cookieCode = " << cookieCode.substr(0, 10) << __E__;

			HttpXmlDocument xmldoc(cookieCode, jumbledEmail);

			theWebUsers_.insertSettingsForUser(uid, &xmldoc);  // insert settings

			// insert active session count for user

			if(uid != theWebUsers_.NOT_FOUND_IN_DATABASE)
			{
				uint64_t asCnt =
				    theWebUsers_.getActiveSessionCountForUser(uid) -
				    1;  // subtract 1 to remove just started session from count
				char asStr[20];
				sprintf(asStr, "%lu", asCnt);
				xmldoc.addTextElementToData("user_active_session_count", asStr);
			}

			xmldoc.outputXmlDocument((std::ostringstream*)out);
		}
		else if(Command == "logout")
		{
			std::string cookieCode   = CgiDataUtilities::postData(cgi, "CookieCode");
			std::string logoutOthers = CgiDataUtilities::postData(cgi, "LogoutOthers");

			//		__COUT__ << "Cookie Code = " << cookieCode.substr(0, 10) << __E__;
			//		__COUT__ << "logoutOthers = " << logoutOthers << __E__;

			uint64_t uid;  // get uid for possible system logbook message
			if(theWebUsers_.cookieCodeLogout(cookieCode,
			                                 logoutOthers == "1",
			                                 &uid,
			                                 cgi.getEnvironment().getRemoteAddr()) !=
			   theWebUsers_.NOT_FOUND_IN_DATABASE)  // user logout
			{
				// if did some logging out, check if completely logged out
				// if so, system logbook message should be made.
				if(!theWebUsers_.isUserIdActive(uid))
				{
					bool doLog = false;
					try
					{
						doLog = __ENV__("OTS_LOG_LOGIN_LOGOUT") == std::string("1");
					}
					catch(...)
					{ /* ignore errors */
						;
					}

					if(doLog)
						makeSystemLogEntry(theWebUsers_.getUsersUsername(uid) +
						                   " logged out.");
				}
			}
		}
		else
		{
			__COUT_WARN__ << "Invalid Command" << __E__;
			*out << "0";
		}
	}
	catch(const std::runtime_error& e)
	{
		__SS__ << "An error was encountered handling Command '" << Command
		       << "':" << e.what() << __E__;
		__COUT__ << "\n" << ss.str();
		HttpXmlDocument xmldoc;
		xmldoc.addTextElementToData("Error", ss.str());
		xmldoc.outputXmlDocument(
		    (std::ostringstream*)out, false /*dispStdOut*/, true /*allowWhiteSpace*/);
	}
	catch(...)
	{
		__SS__ << "An unknown error was encountered handling Command '" << Command
		       << ".' "
		       << "Please check the printouts to debug." << __E__;
		try
		{
			throw;
		}  //one more try to printout extra info
		catch(const std::exception& e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...)
		{
		}
		__COUT__ << "\n" << ss.str();
		HttpXmlDocument xmldoc;
		xmldoc.addTextElementToData("Error", ss.str());
		xmldoc.outputXmlDocument(
		    (std::ostringstream*)out, false /*dispStdOut*/, true /*allowWhiteSpace*/);
	}

	__COUTT__ << "Login end clock=" << artdaq::TimeUtils::GetElapsedTime(startClock)
	          << __E__;
}  // end loginRequest()

//==============================================================================
void GatewaySupervisor::tooltipRequest(xgi::Input* in, xgi::Output* out)
{
	cgicc::Cgicc cgi(in);

	std::string Command = CgiDataUtilities::getData(cgi, "RequestType");
	__COUTT__ << "Tooltip RequestType = " << Command << __E__;

	try
	{
		//**** start LOGIN GATEWAY CODE ***//
		// If TRUE, cookie code is good, and refreshed code is in cookieCode, also pointers
		// optionally for uint8_t userPermissions, uint64_t uid  Else, error message is
		// returned in cookieCode  Notes: cookie code not refreshed if RequestType is in AutomatedRequestTypes
		std::string cookieCode = CgiDataUtilities::postData(cgi, "CookieCode");
		uint64_t    uid;

		if(!theWebUsers_.cookieCodeIsActiveForRequest(cookieCode,
		                                              0 /*userPermissions*/,
		                                              &uid,
		                                              "0" /*dummy ip*/,
		                                              false /*refresh*/,
		                                              false /*doNotGoRemote*/))
		{
			*out << cookieCode;
			return;
		}

		//**** end LOGIN GATEWAY CODE ***//

		HttpXmlDocument xmldoc(cookieCode);

		if(Command == "check")
		{
			WebUsers::tooltipCheckForUsername(theWebUsers_.getUsersUsername(uid),
			                                  &xmldoc,
			                                  CgiDataUtilities::getData(cgi, "srcFile"),
			                                  CgiDataUtilities::getData(cgi, "srcFunc"),
			                                  CgiDataUtilities::getData(cgi, "srcId"));
		}
		else if(Command == "setNeverShow")
		{
			WebUsers::tooltipSetNeverShowForUsername(
			    theWebUsers_.getUsersUsername(uid),
			    &xmldoc,
			    CgiDataUtilities::getData(cgi, "srcFile"),
			    CgiDataUtilities::getData(cgi, "srcFunc"),
			    CgiDataUtilities::getData(cgi, "srcId"),
			    CgiDataUtilities::getData(cgi, "doNeverShow") == "1" ? true : false,
			    CgiDataUtilities::getData(cgi, "temporarySilence") == "1" ? true : false);
		}
		else
			__COUT__ << "Command Request, " << Command << ", not recognized." << __E__;

		xmldoc.outputXmlDocument((std::ostringstream*)out, false, true);
	}
	catch(const std::runtime_error& e)
	{
		__SS__ << "An error was encountered handling Tooltip Command '" << Command
		       << "':" << e.what() << __E__;
		__COUT__ << "\n" << ss.str();
		HttpXmlDocument xmldoc;
		xmldoc.addTextElementToData("Error", ss.str());
		xmldoc.outputXmlDocument(
		    (std::ostringstream*)out, false /*dispStdOut*/, true /*allowWhiteSpace*/);
	}
	catch(...)
	{
		__SS__ << "An unknown error was encountered handling Tooltip Command '" << Command
		       << ".' "
		       << "Please check the printouts to debug." << __E__;
		try
		{
			throw;
		}  //one more try to printout extra info
		catch(const std::exception& e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...)
		{
		}
		__COUT__ << "\n" << ss.str();
		HttpXmlDocument xmldoc;
		xmldoc.addTextElementToData("Error", ss.str());
		xmldoc.outputXmlDocument(
		    (std::ostringstream*)out, false /*dispStdOut*/, true /*allowWhiteSpace*/);
	}

	//__COUT__ << "Done" << __E__;
}  // end tooltipRequest()

//==============================================================================
/// setSupervisorPropertyDefaults
///		override to set defaults for supervisor property values (before user settings
/// override)
void GatewaySupervisor::setSupervisorPropertyDefaults()
{
	CorePropertySupervisorBase::setSupervisorProperty(
	    CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.UserPermissionsThreshold,
	    std::string() +
	        "*=1 | gatewayLaunchOTS=-1 | gatewayLaunchWiz=-1"
	        " | gatewayLaunchOTSInstance=-1"
	        " | StateMachine-*=10"  //state machine transitions through stateMachineXgiHandler
	        " | cancelStateMachineTransition=10"
	        " | resetConsoleCounts=10"
	        " | commandRemoteSubsystem=10 | setRemoteSubsystemFsmControl=10"  //remote subsystem control
	        " | propagateLoginToSubsystem=10"  //force login cookie propagation to a restarted subsystem
	);

	CorePropertySupervisorBase::setSupervisorProperty(
	    CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.AllowNoLoginRequestTypes,
	    "getCurrentState "
	    " | getAppStatus | getRemoteSubsystems | getRemoteSubsystemStatus");

}  // end setSupervisorPropertyDefaults()

//==============================================================================
/// forceSupervisorPropertyValues
///		override to force supervisor property values (and ignore user settings)
void GatewaySupervisor::forceSupervisorPropertyValues()
{
	// note used by these handlers:
	//	request()
	//	stateMachineXgiHandler() -- prepend StateMachine to request type

	CorePropertySupervisorBase::setSupervisorProperty(
	    CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.AutomatedRequestTypes,
	    "getSystemMessages | getCurrentState | getIterationPlanStatus"
	    " | getAppStatus | getRemoteSubsystems | getRemoteSubsystemStatus | getAppId");

	CorePropertySupervisorBase::addSupervisorProperty(
	    CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.RequireUserLockRequestTypes,
	    "gatewayLaunchOTS | gatewayLaunchWiz | gatewayLaunchOTSInstance"
	    " | commandRemoteSubsystem");

	CorePropertySupervisorBase::addSupervisorProperty(
	    CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.CheckUserLockRequestTypes,
	    "StateMachine-*");  //for all stateMachineXgiHandler requests

	if(CorePropertySupervisorBase::isReadOnly())
	{
		CorePropertySupervisorBase::setSupervisorProperty(
		    CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.UserPermissionsThreshold,
		    "*=0 | getSystemMessages=1 | getDesktopIcons=1");  // block users from writing if no write access
		__COUT_INFO__ << "readOnly true in setSupervisorProperty" << __E__;
	}
}  // end forceSupervisorPropertyValues()

//==============================================================================
void GatewaySupervisor::request(xgi::Input* in, xgi::Output* out)
try
{
	out->getHTTPResponseHeader().addHeader(
	    "Access-Control-Allow-Origin",
	    "*");  // to avoid block by blocked by CORS policy of browser

	// for simplicity assume all commands should be mutually exclusive with iterator
	// thread state machine accesses (really should just be careful with
	// RunControlStateMachine access)
	if(VERBOSE_MUTEX)
		__COUT__ << "Waiting for FSM access" << __E__;
	std::lock_guard<std::mutex> lock(stateMachineAccessMutex_);
	if(VERBOSE_MUTEX)
		__COUT__ << "Have FSM access" << __E__;

	cgicc::Cgicc cgiIn(in);

	std::string requestType = CgiDataUtilities::getData(cgiIn, "RequestType");
	__COUTVS__(TLVL_DebugRequests, requestType);

	HttpXmlDocument           xmlOut;
	WebUsers::RequestUserInfo userInfo(requestType,
	                                   CgiDataUtilities::postData(cgiIn, "CookieCode"));

	CorePropertySupervisorBase::getRequestUserInfo(userInfo);

	if(!theWebUsers_.xmlRequestOnGateway(cgiIn, out, &xmlOut, userInfo))
		return;  // access failed

	// RequestType Commands:
	// getSettings
	// setSettings
	// accountSettings
	// getAliasList
	// getAppStatus
	// getAppId					-- convert URL/host:port based on RequestOrigin
	// getContextNames
	// getSystemMessages
	// setUserWithLock
	// getStateMachine
	// getStateMachineLastLogEntry
	// stateMachinePreferences
	// getStateMachineNames
	// getCurrentState
	// cancelStateMachineTransition
	// getIterationPlanStatus
	// getErrorInStateMatchine

	// getDesktopIcons 			-- convert icon URL/host:port based on RequestOrigin
	// addDesktopIcon

	// resetConsoleCounts

	// getRemoteSubsystems
	// getRemoteSubsystemStatus
	// commandRemoteSubsystem
	// setRemoteSubsystemFsmControl
	// getSubsystemConfigAliasSelectInfo
	// getAliasGlobalFields

	// resetUserTooltips
	// silenceAllUserTooltips

	// gatewayLaunchOTS
	// gatewayLaunchWiz
	// gatewayLaunchOTSInstance

	if(0)  // leave for debugging
	{
		ConfigurationTree configLinkNode =
		    CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(
		        supervisorContextUID_, supervisorApplicationUID_);

		ConfigurationTree fsmLinkNode = configLinkNode.getNode("LinkToStateMachineTable");

		__COUT__ << "requestType " << requestType << " v"
		         << (fsmLinkNode.getTableVersion()) << __E__;
	}

	try
	{
		if(requestType == "getSettings")
		{
			std::string accounts = CgiDataUtilities::getData(cgiIn, "accounts");

			__COUT__ << "Get Settings Request" << __E__;
			__COUT__ << "accounts = " << accounts << __E__;
			theWebUsers_.insertSettingsForUser(
			    userInfo.uid_,
			    &xmlOut,
			    accounts == "1",  //include user accounts if requested
			    userInfo.getGroupPermissionLevels());
		}
		else if(requestType == "setSettings")
		{
			std::string bgcolor     = CgiDataUtilities::postData(cgiIn, "bgcolor");
			std::string dbcolor     = CgiDataUtilities::postData(cgiIn, "dbcolor");
			std::string wincolor    = CgiDataUtilities::postData(cgiIn, "wincolor");
			std::string layout      = CgiDataUtilities::postData(cgiIn, "layout");
			std::string syslayout   = CgiDataUtilities::postData(cgiIn, "syslayout");
			std::string aliasLayout = CgiDataUtilities::postData(cgiIn, "aliaslayout");
			std::string sysAliaslayout =
			    CgiDataUtilities::postData(cgiIn, "sysAliaslayout");

			__COUT__ << "Set Settings Request" << __E__;
			__COUT__ << "bgcolor = " << bgcolor << __E__;
			__COUT__ << "dbcolor = " << dbcolor << __E__;
			__COUT__ << "wincolor = " << wincolor << __E__;
			__COUT__ << "layout = " << layout << __E__;
			__COUT__ << "syslayout = " << syslayout << __E__;
			__COUT__ << "aliasLayout = " << aliasLayout << __E__;
			__COUT__ << "sysAliaslayout = " << sysAliaslayout << __E__;

			theWebUsers_.changeSettingsForUser(userInfo.uid_,
			                                   bgcolor,
			                                   dbcolor,
			                                   wincolor,
			                                   layout,
			                                   syslayout,
			                                   aliasLayout,
			                                   sysAliaslayout);
			theWebUsers_.insertSettingsForUser(userInfo.uid_,
			                                   &xmlOut,
			                                   true,  // include user accounts
			                                   userInfo.getGroupPermissionLevels());
		}
		else if(requestType == "accountSettings")
		{
			std::string type = CgiDataUtilities::postData(
			    cgiIn, "type");  // updateAccount, createAccount, deleteAccount
			int type_int = -1;

			if(type == "updateAccount")
				type_int = theWebUsers_.MOD_TYPE_UPDATE;
			else if(type == "createAccount")
				type_int = theWebUsers_.MOD_TYPE_ADD;
			else if(type == "deleteAccount")
				type_int = theWebUsers_.MOD_TYPE_DELETE;

			std::string username    = CgiDataUtilities::postData(cgiIn, "username");
			std::string displayname = CgiDataUtilities::postData(cgiIn, "displayname");
			std::string email       = CgiDataUtilities::postData(cgiIn, "useremail");
			std::string permissions = CgiDataUtilities::postData(cgiIn, "permissions");
			std::string accounts    = CgiDataUtilities::getData(cgiIn, "accounts");

			__COUT__ << "accountSettings Request" << __E__;
			__COUT__ << "type = " << type << " - " << type_int << __E__;
			__COUT__ << "username = " << username << __E__;
			__COUT__ << "useremail = " << email << __E__;
			__COUT__ << "displayname = " << displayname << __E__;
			__COUT__ << "permissions = " << permissions << __E__;

			theWebUsers_.modifyAccountSettings(
			    userInfo.uid_, type_int, username, displayname, email, permissions);

			__COUT__ << "accounts = " << accounts << __E__;

			theWebUsers_.insertSettingsForUser(
			    userInfo.uid_,
			    &xmlOut,
			    accounts == "1",  //include user accounts if requested
			    userInfo.getGroupPermissionLevels());
		}
		else if(requestType == "stateMachinePreferences")
		{
			std::string       set              = CgiDataUtilities::getData(cgiIn, "set");
			const std::string DEFAULT_FSM_VIEW = "Default_FSM_View";
			const std::string DEFAULT_FSM_NAME = "Default_FSM_Name";
			if(set == "1")
			{
				if(CgiDataUtilities::getData(cgiIn, DEFAULT_FSM_VIEW) != "")
					theWebUsers_.setGenericPreference(
					    userInfo.uid_,
					    DEFAULT_FSM_VIEW,
					    CgiDataUtilities::getData(cgiIn, DEFAULT_FSM_VIEW));

				if(CgiDataUtilities::getData(cgiIn, DEFAULT_FSM_NAME) != "")
					theWebUsers_.setGenericPreference(
					    userInfo.uid_,
					    DEFAULT_FSM_NAME,
					    CgiDataUtilities::getData(cgiIn, DEFAULT_FSM_NAME));
			}
			else
			{
				theWebUsers_.getGenericPreference(
				    userInfo.uid_, DEFAULT_FSM_VIEW, &xmlOut);

				theWebUsers_.getGenericPreference(
				    userInfo.uid_, DEFAULT_FSM_NAME, &xmlOut);
			}
		}
		else if(requestType == "getAliasList")
		{
			// std::string username = userInfo.username_;
			std::string fsmName = CgiDataUtilities::getData(cgiIn, "fsmName");
			__SUP_COUTV__(fsmName);

			addFilteredConfigAliasesToXML(xmlOut, fsmName);
			if(0)
			{
				std::string stateMachineAliasFilter = "*";  // default to all

				// IMPORTANT -- use temporary ConfigurationManager to get the Active Group Aliases,
				//	 to avoid changing the Context Configuration tree for the Gateway Supervisor
				ConfigurationManager temporaryConfigMgr;
				std::map<std::string /*alias*/,
				         std::pair<std::string /*group name*/, TableGroupKey>>
				    aliasMap;
				aliasMap = temporaryConfigMgr.getActiveGroupAliases();

				// AND IMPORTANT -- to use ConfigurationManager to get the Context settings for the Gateway Supervisor
				// get stateMachineAliasFilter if possible
				ConfigurationTree configLinkNode =
				    CorePropertySupervisorBase::theConfigurationManager_
				        ->getSupervisorTableNode(supervisorContextUID_,
				                                 supervisorApplicationUID_);

				if(!configLinkNode.isDisconnected())
				{
					try  // for backwards compatibility
					{
						ConfigurationTree fsmLinkNode =
						    configLinkNode.getNode("LinkToStateMachineTable");
						if(!fsmLinkNode.isDisconnected() &&
						   !fsmLinkNode.getNode(fsmName + "/SystemAliasFilter")
						        .isDefaultValue())
							stateMachineAliasFilter =
							    fsmLinkNode.getNode(fsmName + "/SystemAliasFilter")
							        .getValue<std::string>();
						else
							__COUT_INFO__ << "FSM Link disconnected." << __E__;
					}
					catch(std::runtime_error& e)
					{
						__COUT_INFO__ << e.what() << __E__;
					}
					catch(...)
					{
						__COUT_ERR__ << "Unknown error. Should never happen." << __E__;
					}
				}
				else
					__COUT_INFO__ << "FSM Link disconnected." << __E__;

				__COUT__ << "For FSM '" << fsmName
				         << ",' stateMachineAliasFilter  = " << stateMachineAliasFilter
				         << __E__;

				// filter list of aliases based on stateMachineAliasFilter
				//  ! as first character means choose those that do NOT match filter
				//	* can be used as wild card.
				{
					bool invertFilter = stateMachineAliasFilter.size() &&
					                    stateMachineAliasFilter[0] == '!';
					std::vector<std::string> filterArr;

					size_t i = 0;
					if(invertFilter)
						++i;
					size_t      f;
					std::string tmp;
					while((f = stateMachineAliasFilter.find('*', i)) != std::string::npos)
					{
						tmp = stateMachineAliasFilter.substr(i, f - i);
						i   = f + 1;
						filterArr.push_back(tmp);
						//__COUT__ << filterArr[filterArr.size()-1] << " " << i <<
						//		" of " << stateMachineAliasFilter.size() << __E__;
					}
					if(i <= stateMachineAliasFilter.size())
					{
						tmp = stateMachineAliasFilter.substr(i);
						filterArr.push_back(tmp);
						//__COUT__ << filterArr[filterArr.size()-1] << " last." << __E__;
					}

					bool filterMatch;

					for(auto& aliasMapPair : aliasMap)
					{
						//__COUT__ << "aliasMapPair.first: " << aliasMapPair.first << __E__;

						filterMatch = true;

						if(filterArr.size() == 1)
						{
							if(filterArr[0] != "" && filterArr[0] != "*" &&
							   aliasMapPair.first != filterArr[0])
								filterMatch = false;
						}
						else
						{
							i = -1;
							for(f = 0; f < filterArr.size(); ++f)
							{
								if(!filterArr[f].size())
									continue;  // skip empty filters

								if(f == 0)  // must start with this filter
								{
									if((i = aliasMapPair.first.find(filterArr[f])) != 0)
									{
										filterMatch = false;
										break;
									}
								}
								else if(f == filterArr.size() -
								                 1)  // must end with this filter
								{
									if(aliasMapPair.first.rfind(filterArr[f]) !=
									   aliasMapPair.first.size() - filterArr[f].size())
									{
										filterMatch = false;
										break;
									}
								}
								else if((i = aliasMapPair.first.find(filterArr[f])) ==
								        std::string::npos)
								{
									filterMatch = false;
									break;
								}
							}
						}

						if(invertFilter)
							filterMatch = !filterMatch;

						//__COUT__ << "filterMatch=" << filterMatch  << __E__;

						if(!filterMatch)
							continue;

						xmlOut.addTextElementToData("config_alias", aliasMapPair.first);
						xmlOut.addTextElementToData(
						    "config_key",
						    TableGroupKey::getFullGroupString(aliasMapPair.second.first,
						                                      aliasMapPair.second.second,
						                                      /*decorate as (<key>)*/ "(",
						                                      ")"));

						// __COUT__ << "config_alias_comment" << " " <<  temporaryConfigMgr.getNode(
						// 	ConfigurationManager::GROUP_ALIASES_TABLE_NAME).getNode(aliasMapPair.first).getNode(
						// 		TableViewColumnInfo::COL_NAME_COMMENT).getValue<std::string>() << __E__;
						xmlOut.addTextElementToData(
						    "config_alias_comment",
						    temporaryConfigMgr
						        .getNode(ConfigurationManager::GROUP_ALIASES_TABLE_NAME)
						        .getNode(aliasMapPair.first)
						        .getNode(TableViewColumnInfo::COL_NAME_COMMENT)
						        .getValue<std::string>());

						std::string groupComment, groupAuthor, groupCreationTime;
						try
						{
							temporaryConfigMgr.loadTableGroup(aliasMapPair.second.first,
							                                  aliasMapPair.second.second,
							                                  false,
							                                  0,
							                                  0,
							                                  0,
							                                  &groupComment,
							                                  &groupAuthor,
							                                  &groupCreationTime,
							                                  true /*doNotLoadMembers*/);

							xmlOut.addTextElementToData("config_comment", groupComment);
							xmlOut.addTextElementToData("config_author", groupAuthor);
							xmlOut.addTextElementToData("config_create_time",
							                            groupCreationTime);
						}
						catch(...)
						{
							__COUT_WARN__ << "Failed to load group metadata." << __E__;
						}
					}
				}

				// return last group alias by user
				std::string fn = ConfigurationManager::LAST_TABLE_GROUP_SAVE_PATH + "/" +
				                 FSM_LAST_GROUP_ALIAS_FILE_START + fsmName + "." +
				                 FSM_USERS_PREFERENCES_FILETYPE;
				__COUT__ << "Load preferences: " << fn << __E__;
				FILE* fp = fopen(fn.c_str(), "r");
				if(fp)
				{
					char tmpLastAlias[500];
					fscanf(fp, "%*s %s", tmpLastAlias);
					__COUT__ << "tmpLastAlias: " << tmpLastAlias << __E__;

					xmlOut.addTextElementToData("UserLastConfigAlias", tmpLastAlias);
					fclose(fp);
				}
				else if(aliasMap.size())  //if not set, return first
					xmlOut.addTextElementToData("UserLastConfigAlias",
					                            aliasMap.begin()->first);
			}  //end if 0
		}
		else if(requestType == "getAppStatus")
		{
			//loop through all apps and return status
			for(const auto& it : allSupervisorInfo_.getAllSupervisorInfo())
			{
				// non-blocking here, it's ok if the status is stale
				if(allSupervisorInfo_.getSupervisorInfoMutex(it.second.getId())
				       .try_lock())
				{
					//if doesnt exist, create it
					if(localAllSupervisorInfo_.find(it.second.getId()) ==
					   localAllSupervisorInfo_.end())
						localAllSupervisorInfo_.emplace(
						    std::pair<unsigned int, SupervisorInfo>(
						        it.second.getId(),  // descriptor.first,
						        SupervisorInfo(0 /* descriptor */,
						                       it.second.getName(),
						                       it.second.getContextName())));

					//copy if have lock
					localAllSupervisorInfo_.at(it.second.getId()) = it.second;
					allSupervisorInfo_.getSupervisorInfoMutex(it.second.getId()).unlock();
				}  //else use stale status already in
				else if(localAllSupervisorInfo_.find(it.second.getId()) ==
				        localAllSupervisorInfo_.end())
					continue;  //unless no stale value, then skip for now

				// const auto& appInfo = it.second;
				const auto& appInfo = localAllSupervisorInfo_.at(it.second.getId());

				if(appInfo.getProgress() != 100 &&
				   appInfo.getClass() == XDAQContextTable::GATEWAY_SUPERVISOR_CLASS)
				{
					__COUTT__ << "In transition? " << appInfo.getName()
					          << " status=" << appInfo.getStatus()
					          << " progress=" << appInfo.getProgress() << __E__;
				}

				if(appInfo.getName() == "ConsoleSupervisor" &&
				   appInfo.getSubappInfo().size())
				{
					__SUP_COUTT__ << "ConsoleSupervisor subapp count="
					              << appInfo.getSubappInfo().size() << __E__;
				}

				xmlOut.addTextElementToData("name",
				                            appInfo.getName());  // get application name
				xmlOut.addNumberElementToData("id",
				                              appInfo.getId());  // get application id
				xmlOut.addTextElementToData("status", appInfo.getStatus());  // get status
				xmlOut.addTextElementToData(
				    "time",
				    appInfo.getLastStatusTime()
				        ? StringMacros::getTimestampString(appInfo.getLastStatusTime())
				        : "0");  // get time stamp
				xmlOut.addNumberElementToData(
				    "stale", time(0) - appInfo.getLastStatusTime());  // time since update
				xmlOut.addNumberElementToData("progress",
				                              appInfo.getProgress());  // get progress
				xmlOut.addTextElementToData("detail", appInfo.getDetail());  // get detail
				xmlOut
				    .addNumberElementToData(  //in TB systems, number of KBs is too big for javascript
				        "availableLogSpaceGB",
				        appInfo.getAvailableLogSpaceKB() / 1000.0f /
				            1000.0f);  // get log space
				xmlOut.addNumberElementToData("availableDataSpaceGB",
				                              appInfo.getAvailableDataSpaceKB() /
				                                  1000.0f / 1000.0f);  // get data space
				float rate = appInfo.getLogUsageRateLastHourKBps();
				if(rate == 0)
					rate = appInfo.getLogUsageRateLastHalfHourKBps();
				if(rate == 0)
					rate = appInfo.getLogUsageRateLastQuarterHourKBps();
				if(rate == 0)
					rate = appInfo.getLogUsageRateNowKBps();
				xmlOut.addNumberElementToData("logUsageRateKBps",
				                              rate);  // get log usage rate
				rate = appInfo.getDataUsageRateLastHourKBps();
				if(rate == 0)
					rate = appInfo.getDataUsageRateLastHalfHourKBps();
				if(rate == 0)
					rate = appInfo.getDataUsageRateLastQuarterHourKBps();
				if(rate == 0)
					rate = appInfo.getDataUsageRateNowKBps();
				__SUP_COUTT__ << appInfo.getName() << " rate=" << rate << __E__;
				xmlOut.addNumberElementToData("dataUsageRateKBps",
				                              rate);  // get data usage rate
				xmlOut.addTextElementToData("class",
				                            appInfo.getClass());  // get application class
				xmlOut.addTextElementToData("url",
				                            appInfo.getURL());  // get application url
				xmlOut.addTextElementToData("context",
				                            appInfo.getContextName());  // get context
				auto subappElement = xmlOut.addTextElementToData("subapps", "");
				for(auto& subappInfoPair : appInfo.getSubappInfo())
				{
					xmlOut.addTextElementToParent(
					    "subapp_name", subappInfoPair.first, subappElement);
					xmlOut.addTextElementToParent("subapp_status",
					                              subappInfoPair.second.status,
					                              subappElement);  // get status
					xmlOut.addTextElementToParent(
					    "subapp_time",
					    subappInfoPair.second.lastStatusTime
					        ? StringMacros::getTimestampString(
					              subappInfoPair.second.lastStatusTime)
					        : "0",
					    subappElement);  // get timestamp
					xmlOut.addNumberElementToParent(
					    "subapp_stale",
					    time(0) - subappInfoPair.second.lastStatusTime,
					    subappElement);  // time since update
					xmlOut.addNumberElementToParent("subapp_progress",
					                                subappInfoPair.second.progress,
					                                subappElement);  // get progress
					xmlOut.addTextElementToParent("subapp_detail",
					                              subappInfoPair.second.detail,
					                              subappElement);  // get detail
					xmlOut
					    .addNumberElementToParent(  //in TB systems, number of KBs is too big for javascript
					        "subapp_availableLogSpaceGB",
					        subappInfoPair.second.availableLogSpaceKB / 1000.0f / 1000.0f,
					        subappElement);  // get log space
					xmlOut.addNumberElementToParent(
					    "subapp_availableDataSpaceGB",
					    subappInfoPair.second.availableDataSpaceKB / 1000.0f / 1000.0f,
					    subappElement);  // get data space
					xmlOut.addNumberElementToParent(
					    "subapp_logUsageRateKBps",
					    subappInfoPair.second.logUsageRateKBps,
					    subappElement);  // get log usage rate
					xmlOut.addNumberElementToParent(
					    "subapp_dataUsageRateKBps",
					    subappInfoPair.second.dataUsageRateKBps,
					    subappElement);  // get data usage rate
					xmlOut.addTextElementToParent("subapp_url",
					                              subappInfoPair.second.url,
					                              subappElement);  // get detail
					xmlOut.addNumberElementToParent(
					    "subapp_id", subappInfoPair.second.id, subappElement);  // get url
					xmlOut.addTextElementToParent("subapp_class",
					                              subappInfoPair.second.class_name,
					                              subappElement);  // get class
				}
			}  //end app info loop

			//also return remote gateways as apps
			std::vector<GatewaySupervisor::RemoteGatewayInfo> remoteApps;  //local copy
			{  //lock for remainder of scope
				std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
				remoteApps = remoteGatewayApps_;
			}

			for(const auto& remoteApp : remoteApps)
			{
				const auto& appInfo = remoteApp.appInfo;

				//skip if no status (will show up as subapp of Gateway)
				if(appInfo.status == SupervisorInfo::APP_STATUS_UNKNOWN)
					continue;

				xmlOut.addTextElementToData("name",
				                            appInfo.name);        // get application name
				xmlOut.addNumberElementToData("id", appInfo.id);  // get application id
				xmlOut.addTextElementToData("status", appInfo.status);  // get status
				xmlOut.addTextElementToData(
				    "time",
				    appInfo.lastStatusTime
				        ? StringMacros::getTimestampString(appInfo.lastStatusTime)
				        : "0");  // get timestamp
				xmlOut.addNumberElementToData(
				    "stale",
				    time(0) - appInfo.lastStatusTime);  // time since update
				xmlOut.addNumberElementToData("progress",
				                              appInfo.progress);        // get progress
				xmlOut.addTextElementToData("detail", appInfo.detail);  // get detail
				xmlOut
				    .addNumberElementToData(  //in TB systems, number of KBs is too big for javascript
				        "availableLogSpaceGB",
				        appInfo.availableLogSpaceKB / 1000.0f /
				            1000.0f);  // get log space
				xmlOut.addNumberElementToData(
				    "availableDataSpaceGB",
				    appInfo.availableDataSpaceKB / 1000.0f / 1000.0f);  // get data space
				xmlOut.addNumberElementToData(
				    "logUsageRateKBps", appInfo.logUsageRateKBps);  // get log usage rate
				xmlOut.addNumberElementToData(
				    "dataUsageRateKBps",
				    appInfo.dataUsageRateKBps);  // get data usage rate
				xmlOut.addTextElementToData("class",
				                            appInfo.class_name);  // get application class
				xmlOut.addTextElementToData("url",
				                            appInfo.parent_url);  // get application url
				xmlOut.addTextElementToData(
				    "context", appInfo.name + " at " + appInfo.url);  // get context
				auto subappElement = xmlOut.addTextElementToData("subapps", "");
				for(auto& subappInfoPair : remoteApp.subapps)
				{
					xmlOut.addTextElementToParent(
					    "subapp_name", subappInfoPair.first, subappElement);
					xmlOut.addTextElementToParent("subapp_status",
					                              subappInfoPair.second.status,
					                              subappElement);  // get status
					xmlOut.addTextElementToParent(
					    "subapp_time",
					    subappInfoPair.second.lastStatusTime
					        ? StringMacros::getTimestampString(
					              subappInfoPair.second.lastStatusTime)
					        : "0",
					    subappElement);  // get time stamp
					xmlOut.addNumberElementToParent(
					    "subapp_stale",
					    time(0) - subappInfoPair.second.lastStatusTime,
					    subappElement);  // time since update
					xmlOut.addNumberElementToParent("subapp_progress",
					                                subappInfoPair.second.progress,
					                                subappElement);  // get progress
					xmlOut.addTextElementToParent("subapp_detail",
					                              subappInfoPair.second.detail,
					                              subappElement);  // get detail
					xmlOut
					    .addNumberElementToParent(  //in TB systems, number of KBs is too big for javascript
					        "subapp_availableLogSpaceGB",
					        subappInfoPair.second.availableLogSpaceKB / 1000.0f / 1000.0f,
					        subappElement);  // get log space
					xmlOut.addNumberElementToParent(
					    "subapp_availableDataSpaceGB",
					    subappInfoPair.second.availableDataSpaceKB / 1000.0f / 1000.0f,
					    subappElement);  // get data space
					xmlOut.addNumberElementToParent(
					    "subapp_logUsageRateKBps",
					    subappInfoPair.second.logUsageRateKBps,
					    subappElement);  // get log usage rate
					xmlOut.addNumberElementToParent(
					    "subapp_dataUsageRateKBps",
					    subappInfoPair.second.dataUsageRateKBps,
					    subappElement);  // get data usage rate
					xmlOut.addTextElementToParent("subapp_url",
					                              subappInfoPair.second.parent_url,
					                              subappElement);  // get detail
					xmlOut.addNumberElementToParent(
					    "subapp_id", subappInfoPair.second.id, subappElement);  // get url
					xmlOut.addTextElementToParent("subapp_class",
					                              subappInfoPair.second.class_name,
					                              subappElement);  // get class
				}
			}  //end remote app info loop
		}
		else if(requestType == "getAppId")
		{
			GatewaySupervisor::handleGetApplicationIdRequest(
			    &allSupervisorInfo_, cgiIn, xmlOut, &portTranslationMap_);
		}
		else if(requestType == "getContextNames")
		{
			const XDAQContextTable* contextTable =
			    CorePropertySupervisorBase::theConfigurationManager_->__GET_CONFIG__(
			        XDAQContextTable);

			auto contexts = contextTable->getContexts();
			for(const auto& context : contexts)
			{
				xmlOut.addTextElementToData(
				    "ContextMember", context.contextUID_);  // get context member name
			}

			//Also add Remote Subystems and consider them Context Member Names!
			std::vector<GatewaySupervisor::RemoteGatewayInfo>
			    remoteGatewayApps;  //local copy
			{                       //lock for remainder of scope
				std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
				remoteGatewayApps = remoteGatewayApps_;
			}

			for(const auto& remoteGatewayApp : remoteGatewayApps)
				xmlOut.addTextElementToData("RemoteGateway",
				                            remoteGatewayApp.appInfo.name + " at " +
				                                remoteGatewayApp.appInfo.url);
		}
		else if(requestType == "getSystemMessages")
		{
			xmlOut.addTextElementToData(
			    "systemMessages", theWebUsers_.getSystemMessage(userInfo.displayName_));

			xmlOut.addTextElementToData(
			    "username_with_lock",
			    theWebUsers_.getUserWithLock());  // always give system lock update

			__COUTVS__(TLVL_Permissions, theWebUsers_.getUserWithLock());

			//Also add Remote Subystems users-with-lock!
			std::vector<GatewaySupervisor::RemoteGatewayInfo>
			    remoteGatewayApps;  //local copy
			{                       //lock for remainder of scope
				std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
				remoteGatewayApps = remoteGatewayApps_;
			}

			for(const auto& remoteGatewayApp : remoteGatewayApps)
			{
				__COUTVS__(TLVL_StatusFullDetail, remoteGatewayApp.appInfo.status);

				//skip disconnected remote gateways
				if(remoteGatewayApp.appInfo.status == SupervisorInfo::APP_STATUS_UNKNOWN)
					continue;

				xmlOut.addTextElementToData("RemoteGateway_name",
				                            remoteGatewayApp.appInfo.name);
				xmlOut.addTextElementToData("RemoteGateway_usernameWithLock",
				                            remoteGatewayApp.usernameWithLock);
				//attempt to find associated icon desktop folder (for use with focus view)
				std::string desktopFolderIcon = "";
				{
					std::lock_guard<std::mutex> lock(latestGatewayIconsMutex_);
					const std::vector<DesktopIconTable::DesktopIcon>& icons =
					    latestGatewayIcons_;

					for(const auto& icon : icons)
						if(icon.recordUID_ == remoteGatewayApp.appInfo.name)
						{
							desktopFolderIcon = icon.folderPath_;
							break;
						}
				}
				xmlOut.addTextElementToData("RemoteGateway_desktopFolderIcon",
				                            desktopFolderIcon);

			}  //end remote subsystem loop
		}
		else if(requestType == "setUserWithLock")
		{
			std::string username = CgiDataUtilities::postData(cgiIn, "username");
			std::string lock     = CgiDataUtilities::postData(cgiIn, "lock");
			std::string accounts = CgiDataUtilities::getData(cgiIn, "accounts");

			__COUTV__(username);
			__COUTV__(lock);
			__COUTV__(accounts);
			__COUTV__(userInfo.uid_);

			std::string tmpUserWithLock = theWebUsers_.getUserWithLock();
			if(!theWebUsers_.setUserWithLock(userInfo.uid_, lock == "1", username))
				xmlOut.addTextElementToData(
				    "server_alert",
				    std::string("Set user lock action failed. You must have valid "
				                "permissions and ") +
				        "locking user must be currently logged in.");

			theWebUsers_.insertSettingsForUser(
			    userInfo.uid_,
			    &xmlOut,
			    accounts == "1",  // include accounts if admin
			    userInfo.getGroupPermissionLevels());

			if(tmpUserWithLock !=
			   theWebUsers_
			       .getUserWithLock())  // if there was a change, broadcast system message
				theWebUsers_.addSystemMessage(
				    "*",
				    theWebUsers_.getUserWithLock() == ""
				        ? tmpUserWithLock + " has unlocked ots."
				        : theWebUsers_.getUserWithLock() + " has locked ots.");

			//Also add Remote Subystems users-with-lock!
			std::vector<GatewaySupervisor::RemoteGatewayInfo>
			    remoteGatewayApps;  //local copy
			{                       //lock for remainder of scope
				std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
				remoteGatewayApps = remoteGatewayApps_;
			}

			for(const auto& remoteGatewayApp : remoteGatewayApps)
			{
				//skip disconnected remote gateways
				if(remoteGatewayApp.appInfo.status == SupervisorInfo::APP_STATUS_UNKNOWN)
					continue;

				xmlOut.addTextElementToData("RemoteGateway_name",
				                            remoteGatewayApp.appInfo.name);
				xmlOut.addTextElementToData("RemoteGateway_usernameWithLock",
				                            remoteGatewayApp.usernameWithLock);
				//attempt to find associated icon desktop folder (for use with focus view)
				std::string desktopFolderIcon = "";
				{
					std::lock_guard<std::mutex> lock(latestGatewayIconsMutex_);
					const std::vector<DesktopIconTable::DesktopIcon>& icons =
					    latestGatewayIcons_;

					for(const auto& icon : icons)
						if(icon.recordUID_ == remoteGatewayApp.appInfo.name)
						{
							desktopFolderIcon = icon.folderPath_;
							break;
						}
				}
				xmlOut.addTextElementToData("RemoteGateway_desktopFolderIcon",
				                            desktopFolderIcon);
			}  //end remote subsystem loop
		}
		else if(requestType == "getStateMachineLastLogEntry")
		{
			std::string fsmName    = CgiDataUtilities::getData(cgiIn, "fsmName");
			std::string transition = CgiDataUtilities::getData(cgiIn, "transition");
			__SUP_COUTV__(fsmName);
			__SUP_COUTV__(transition);

			//remove appended date and, for start, remove prepended run #
			std::string lastLog = getLastLogEntry(transition, fsmName);
			__SUP_COUTTV__(lastLog);
			size_t i = lastLog.rfind('(');
			if(i != std::string::npos && i > 1)  //remove appended date
			{
				lastLog = lastLog.substr(0, i - 1);
				__SUP_COUTTV__(lastLog);
			}

			if(transition == RunControlStateMachine::START_TRANSITION_NAME)
			{
				i = lastLog.find(':');
				if(i != std::string::npos &&
				   i + 2 < lastLog.size())  //remove prepended run #
				{
					lastLog = lastLog.substr(i + 2);
					__SUP_COUTTV__(lastLog);
				}
			}

			xmlOut.addTextElementToData("lastLogEntry", lastLog);
		}
		else if(requestType == "getStateMachine")
		{
			std::string fsmName = CgiDataUtilities::getData(cgiIn, "fsmName");
			__SUP_COUTVS__(TLVL_RemoteFSMRequests, fsmName);

			addRequiredFsmLogInputToXML(xmlOut, fsmName);

			std::vector<toolbox::fsm::State> states;
			states = theStateMachine_.getStates();
			char stateStr[2];
			stateStr[1] = '\0';
			std::string transName;
			std::string transParameter;

			for(unsigned int i = 0; i < states.size(); ++i)  // get all states
			{
				stateStr[0]             = states[i];
				DOMElement* stateParent = xmlOut.addTextElementToData("state", stateStr);

				xmlOut.addTextElementToParent(
				    "state_name", theStateMachine_.getStateName(states[i]), stateParent);

				// get all transition final states, transitionNames and actionNames from
				// state
				std::map<std::string, toolbox::fsm::State, std::less<std::string>> trans =
				    theStateMachine_.getTransitions(states[i]);
				std::set<std::string> actionNames = theStateMachine_.getInputs(states[i]);

				std::map<std::string, toolbox::fsm::State, std::less<std::string>>::
				    iterator                    it  = trans.begin();
				std::set<std::string>::iterator ait = actionNames.begin();

				// handle hacky way to keep "forward" moving states on right of FSM
				// display.. must be first!

				for(; it != trans.end() && ait != actionNames.end(); ++it, ++ait)
				{
					stateStr[0] = it->second;

					if(stateStr[0] == 'R')
					{
						xmlOut.addTextElementToParent(
						    "state_transition", stateStr, stateParent);
						xmlOut.addTextElementToParent(
						    "state_transition_action", *ait, stateParent);
						transName = theStateMachine_.getTransitionName(states[i], *ait);
						xmlOut.addTextElementToParent(
						    "state_transition_name", transName, stateParent);
						transParameter =
						    theStateMachine_.getTransitionParameter(states[i], *ait);
						xmlOut.addTextElementToParent(
						    "state_transition_parameter", transParameter, stateParent);
						break;
					}
					else if(stateStr[0] == 'C')
					{
						xmlOut.addTextElementToParent(
						    "state_transition", stateStr, stateParent);
						xmlOut.addTextElementToParent(
						    "state_transition_action", *ait, stateParent);
						transName = theStateMachine_.getTransitionName(states[i], *ait);
						xmlOut.addTextElementToParent(
						    "state_transition_name", transName, stateParent);
						transParameter =
						    theStateMachine_.getTransitionParameter(states[i], *ait);
						xmlOut.addTextElementToParent(
						    "state_transition_parameter", transParameter, stateParent);
						break;
					}
				}

				// reset for 2nd pass (on left of FSM display)
				it  = trans.begin();
				ait = actionNames.begin();

				// other states
				for(; it != trans.end() && ait != actionNames.end(); ++it, ++ait)
				{
					stateStr[0] = it->second;

					if(stateStr[0] == 'R')
						continue;
					else if(stateStr[0] == 'C')
						continue;

					xmlOut.addTextElementToParent(
					    "state_transition", stateStr, stateParent);
					xmlOut.addTextElementToParent(
					    "state_transition_action", *ait, stateParent);
					transName = theStateMachine_.getTransitionName(states[i], *ait);
					xmlOut.addTextElementToParent(
					    "state_transition_name", transName, stateParent);
					transParameter =
					    theStateMachine_.getTransitionParameter(states[i], *ait);
					xmlOut.addTextElementToParent(
					    "state_transition_parameter", transParameter, stateParent);
				}
			}  //end state traversal loop
		}
		else if(requestType == "getStateMachineNames")
		{
			// get stateMachineAliasFilter if possible
			ConfigurationTree configLinkNode =
			    CorePropertySupervisorBase::theConfigurationManager_
			        ->getSupervisorTableNode(supervisorContextUID_,
			                                 supervisorApplicationUID_);

			try
			{
				auto fsmNodes =
				    configLinkNode.getNode("LinkToStateMachineTable").getChildren();
				for(const auto& fsmNode : fsmNodes)
					xmlOut.addTextElementToData("stateMachineName", fsmNode.first);
			}
			catch(...)  // else empty set of state machines.. can always choose ""
			{
				__COUT__ << "Caught exception, assuming no valid FSM names." << __E__;
				xmlOut.addTextElementToData("stateMachineName", "");
			}
		}
		else if(requestType == "getIterationPlanStatus")
		{
			//__COUT__ << "checking it status" << __E__;
			theIterator_.handleCommandRequest(xmlOut, requestType, "");
		}
		else if(requestType == "getCurrentState")
		{
			std::string fsmName = CgiDataUtilities::getData(cgiIn, "fsmName");
			addStateMachineStatusToXML(xmlOut, fsmName);
		}
		else if(requestType == "cancelStateMachineTransition")
		{
			if(!theStateMachine_.isInTransition())
			{
				__COUT__ << "Cancel requested but not in transition - ignoring." << __E__;
			}
			else
			{
				__SS__ << "State transition was cancelled by user!" << __E__;
				__COUTV__(ss.str());
				RunControlStateMachine::theStateMachine_.setErrorMessage(ss.str());
				RunControlStateMachine::asyncFailureReceived_ = true;
			}
		}
		else if(requestType == "getErrorInStateMatchine")
		{
			xmlOut.addTextElementToData("FSM_Error", theStateMachine_.getErrorMessage());
		}
		else if(requestType == "getDesktopIcons")
		{
			// get icons and create comma-separated string based on user permissions
			//	note: each icon has own permission threshold, so each user can have
			//		a unique desktop icon experience.

			// use latest context always from temporary configuration manager,
			//	to get updated icons every time...
			//(so icon changes do no require an ots restart)

			bool                                                 useLatestIcons = false;
			std::string                                          timeString;
			std::pair<std::string /*group name*/, TableGroupKey> latestGroup;
			std::vector<DesktopIconTable::DesktopIcon>           icons;
			{  //start lock scope
				std::lock_guard<std::mutex> lock(latestGatewayIconsMutex_);
				latestGroup = latestGatewayIconsContextGroup_;
			}  //end lock scope
			if(latestGroup.first.size())
			{
				std::pair<std::string /*group name*/, TableGroupKey> theGroup =
				    ConfigurationManager::loadGroupNameAndKey(
				        ConfigurationManager::LAST_ACTIVATED_CONTEXT_GROUP_FILE,
				        timeString);
				if(theGroup == latestGroup)
				{
					useLatestIcons = true;
					__COUTT__ << "Using cached latest icons for context group '"
					          << theGroup.first << "(" << theGroup.second << ")" << __E__;
					std::lock_guard<std::mutex> lock(latestGatewayIconsMutex_);
					icons = latestGatewayIcons_;
				}
			}  //end check for active context changing

			if(!useLatestIcons)  //then need to load latest icons
			{
				ConfigurationManager
				    tmpCfgMgr;  // Creating new temporary instance so that constructor will activate latest context, note: not using member CorePropertySupervisorBase::theConfigurationManager_
				const DesktopIconTable* iconTable =
				    tmpCfgMgr.__GET_CONFIG__(DesktopIconTable);
				{
					std::lock_guard<std::mutex> lock(latestGatewayIconsMutex_);
					latestGatewayIcons_ =
					    iconTable
					        ->getAllDesktopIcons();  //cache latest icons (for use, e.g., in remote login verify)
					icons = latestGatewayIcons_;  //use for this request
					latestGatewayIconsContextGroup_ =
					    tmpCfgMgr.getActiveTableGroups()
					        [ConfigurationManager::GROUP_TYPE_NAME_CONTEXT];
				}  //end lock scope
			}      //end load of active context icons
			//at this point icons is correctly populated

			std::string iconString = "";
			// comma-separated icon string, 7 fields:
			//				0 - caption 		= text below icon
			//				1 - altText 		= text icon if no image given
			//				2 - uniqueWin 		= if true, only one window is allowed,
			// 										else  multiple instances of window
			//				3 - permissions 	= security level needed to see icon
			//				4 - picfn 			= icon image filename
			//				5 - linkurl 		= url of the window to open
			// 				6 - folderPath 		= folder and subfolder location '/' separated
			//	for example:  State Machine,FSM,1,200,icon-Physics.gif,/WebPath/html/StateMachine.html?fsm_name=OtherRuns0,,Chat,CHAT,1,1,icon-Chat.png,/urn:xdaq-application:lid=250,,Visualizer,VIS,0,10,icon-Visualizer.png,/WebPath/html/Visualization.html?urn=270,,Configure,CFG,0,10,icon-Configure.png,/urn:xdaq-application:lid=281,,Front-ends,CFG,0,15,icon-Configure.png,/WebPath/html/ConfigurationGUI_subset.html?urn=281&subsetBasePath=FEInterfaceTable&groupingFieldList=Status%2CFEInterfacePluginName&recordAlias=Front%2Dends&editableFieldList=%21%2ACommentDescription%2C%21SlowControls%2A,Config Subsets

			std::map<std::string, WebUsers::permissionLevel_t> userPermissionLevelsMap =
			    theWebUsers_.getPermissionsForUser(userInfo.uid_);
			std::map<std::string, WebUsers::permissionLevel_t>
			    iconPermissionThresholdsMap;

			__COUTVS__(TLVL_Permissions,
			           StringMacros::mapToString(userPermissionLevelsMap));

			bool getRemoteIcons =
			    true;  //could potentially enable from configuration in future
			//also return remote gateway icons from cache
			std::vector<GatewaySupervisor::RemoteGatewayInfo>
			    remoteGatewayApps;  //local copy
			if(getRemoteIcons)
			{  //lock for remainder of scope
				std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
				remoteGatewayApps = remoteGatewayApps_;
			}

			std::string requestOrigin = StringMacros::decodeURIComponent(
			    CgiDataUtilities::postData(cgiIn, "RequestOrigin"));
			bool doAddressTranslation = false;
			if(requestOrigin.size() && portTranslationMap_.size())
			{
				__SUP_COUTTV__(StringMacros::mapToString(portTranslationMap_));

				__SUP_COUTTV__(requestOrigin);
				if(portTranslationMap_.find(requestOrigin) != portTranslationMap_.end())
				{
					__SUP_COUT__
					    << "Doing address translation for icons from request origin: "
					    << requestOrigin << __E__;
					doAddressTranslation = true;
				}
			}

			std::string ipAddressForRemoteIconsOverUDP = "";

			bool firstIcon = true;
			for(const auto& icon : icons)
			{
				__SUP_COUTVS__(TLVL_GetDesktopIcons, icon.caption_);
				__SUP_COUTVS__(TLVL_GetDesktopIcons, icon.permissionThresholdString_);

				CorePropertySupervisorBase::extractPermissionsMapFromString(
				    icon.permissionThresholdString_, iconPermissionThresholdsMap);

				if(!CorePropertySupervisorBase::doPermissionsGrantAccess(
				       userPermissionLevelsMap, iconPermissionThresholdsMap))
				{
					__COUTT__ << "No user access to icon '" << icon.caption_ << "'"
					          << __E__;
					continue;  // skip icon if no access
				}

				__SUP_COUTVS__(TLVL_GetDesktopIcons, icon.caption_);

				if(getRemoteIcons)
				{
					__COUTV__(icon.windowContentURL_);
					if(icon.windowContentURL_.size() > 4 &&
					   icon.windowContentURL_[0] == 'o' &&
					   icon.windowContentURL_[1] == 't' &&
					   icon.windowContentURL_[2] == 's' &&
					   icon.windowContentURL_[3] == ':')
					{
						//retrieval from cache!
						bool found = false;
						for(const auto& remoteGatewayApp : remoteGatewayApps)
						{
							if(icon.recordUID_ != remoteGatewayApp.appInfo.name)
								continue;
							__SUP_COUTVS__(TLVL_GetDesktopIcons, icon.caption_);
							found = true;

							if(remoteGatewayApp.iconString ==
							   "")  //then either error or still loading...
							{
								__SUP_COUTVS__(TLVL_GetDesktopIcons,
								               remoteGatewayApp.getError());
								__SUP_COUTVS__(TLVL_GetDesktopIcons,
								               remoteGatewayApp.appInfo.status);

								//add error if it has to do with icons
								if(remoteGatewayApp.getError().find("desktop icons") !=
								       std::string::npos ||
								   remoteGatewayApp.getError().find(
								       REMOTE_BACKBONE_ERR) != std::string::npos)
								{
									xmlOut.addTextElementToData(
									    "Error", remoteGatewayApp.getError());

									//and is this case, clear the error and consider it reported
									// if it remains an issue, the error will be created again anyway

									//lock for remainder of scope
									std::lock_guard<std::mutex> lock(
									    remoteGatewayAppsMutex_);
									for(size_t i = 0; i < remoteGatewayApps_.size(); ++i)
										if(remoteGatewayApp.appInfo.name ==
										   remoteGatewayApps_[i].appInfo.name)
										{
											__SUP_COUT_WARN__
											    << "Clearing remote subsystem error that "
											       "was delivered to the user: "
											    << remoteGatewayApps_[i].getError()
											    << __E__;
											remoteGatewayApps_[i].clearError();
											break;
										}
								}
								else if(remoteGatewayApp.appInfo.status ==
								        SupervisorInfo::APP_STATUS_UNKNOWN)
								{
									__SS__ << "Connection failed for '"
									       << remoteGatewayApp.parentIconFolderPath
									       << "' icon retrieval from Remote Gateway '"
									       << remoteGatewayApp.appInfo.name << "' at "
									       << remoteGatewayApp.appInfo.url
									       << ". Please check that the target is up and "
									          "running at the correct IP address."
									       << __E__;
									xmlOut.addTextElementToData("Error", ss.str());
								}

								//add placeholder "Loading icon"
								if(firstIcon)
									firstIcon = false;
								else
									iconString += ",";

								if(remoteGatewayApp.parentIconFolderPath != "")
									iconString += remoteGatewayApp.parentIconFolderPath +
									              " loading...";  //icon.caption_;
								else if(remoteGatewayApp.user_data_path_record != "")
									iconString += remoteGatewayApp.user_data_path_record +
									              " loading...";  //icon.caption_;
								else
									iconString += remoteGatewayApp.appInfo.name +
									              " loading...";  //icon.caption_;

								iconString += ",X";  //icon.alternateText_;
								iconString +=
								    ",1";  //std::string(icon.enforceOneWindowInstance_ ? "1" : "0");
								iconString +=
								    ",0";  //std::string("1");  // set permission to 1 so the
								           // desktop shows every icon that the
								           // server allows (i.e., trust server
								           // security, ignore client security)
								iconString += ",";  //icon.imageURL_;
								iconString += ",";  //icon.windowContentURL_;
								iconString += ",";  //icon.folderPath_;

								break;  //done adding error/loading icon
							}
							__SUP_COUTVS__(TLVL_GetDesktopIcons,
							               remoteGatewayApp.iconString);

							if(firstIcon)
								firstIcon = false;
							else
								iconString += ",";

							if(doAddressTranslation)
							{
								__COUTTV__(requestOrigin);
								__COUTTV__(remoteGatewayApp.iconString);
								std::string translatedIconString =
								    translateRemoteIconStringForRequestOrigin(
								        remoteGatewayApp.iconString,
								        requestOrigin,
								        portTranslationMap_);
								__COUTTV__(translatedIconString);
								iconString += translatedIconString;
							}
							else
								iconString += remoteGatewayApp.iconString;
							break;  //done with cache retrieval
						}           //end loop retrieval

						if(!found)
						{
							__SUP_COUT_ERR__
							    << "Illegal missing remote icon definition for "
							       "icon record UID '"
							    << icon.recordUID_
							    << ".' If the corresponding subsystem should "
							       "exist, perhaps Gateway Application Status "
							       "Monitoring is disabled. It must be enabled "
							       "for subsystem management. Please notify "
							       "admins if the problem persists."
							    << __E__;
							// __SUP_SS_THROW__;
						}

						continue;  //done with remote icon string retrieval
					}
				}  //end remote icon handling

				// have icon access, so add to CSV string
				if(firstIcon)
					firstIcon = false;
				else
					iconString += ",";

				iconString += icon.caption_;
				iconString += "," + icon.alternateText_;
				iconString +=
				    "," + std::string(icon.enforceOneWindowInstance_ ? "1" : "0");
				iconString += "," + std::string("1");  // set permission to 1 so the
				    // desktop shows every icon that the
				    // server allows (i.e., trust server
				    // security, ignore client security)
				iconString += "," + icon.imageURL_;

				if(doAddressTranslation)
				{
					__COUTTV__(requestOrigin);
					__COUTTV__(icon.windowContentURL_);
					std::string translatedURL = translateURLForRequestOrigin(
					    icon.windowContentURL_, requestOrigin, portTranslationMap_);
					__COUTTV__(translatedURL);
					iconString += "," + translatedURL;
				}
				else
					iconString += "," + icon.windowContentURL_;

				iconString += "," + icon.folderPath_;
			}
			__COUTVS__(TLVL_StatusParams, iconString);

			xmlOut.addTextElementToData("iconList", iconString);
		}
		else if(requestType == "addDesktopIcon")
		{
			std::vector<DesktopIconTable::DesktopIcon> newIcons;

			bool success = GatewaySupervisor::handleAddDesktopIconRequest(
			    userInfo.username_, cgiIn, xmlOut, &newIcons);

			if(success)
			{
				__COUT__ << "Attempting dynamic icon change..." << __E__;

				DesktopIconTable* iconTable =
				    (DesktopIconTable*)
				        CorePropertySupervisorBase::theConfigurationManager_
				            ->getDesktopIconTable();
				iconTable->setAllDesktopIcons(newIcons);
			}
			else
				__COUT__ << "Failed dynamic icon add." << __E__;
		}  //end addDesktopIcon
		else if(requestType == "resetConsoleCounts")
		{
			//zero out console count and retake first messages

			for(const auto& it : allSupervisorInfo_.getAllSupervisorInfo())
			{
				const auto& appInfo = it.second;
				if(appInfo.isTypeConsoleSupervisor())
				{
					xoap::MessageReference tempMessage =
					    SOAPUtilities::makeSOAPMessageReference("ResetConsoleCounts");
					std::string reply = send(appInfo.getDescriptor(), tempMessage);

					if(reply != "Done")
					{
						__SUP_SS__ << "Error while resetting console counts of "
						              "Supervisor instance = '"
						           << appInfo.getName() << "' [LID=" << appInfo.getId()
						           << "] in Context '" << appInfo.getContextName()
						           << "' [URL=" << appInfo.getURL() << "].\n\n"
						           << reply << __E__;
						__SUP_SS_THROW__;
					}
					__SUP_COUT__ << "Reset console counts of Supervisor instance = '"
					             << appInfo.getName() << "' [LID=" << appInfo.getId()
					             << "] in Context '" << appInfo.getContextName()
					             << "' [URL=" << appInfo.getURL() << "]." << __E__;
				}
			}  //end loop for Console Supervisors

			//for user display feedback, clear local cached values also
			std::lock_guard<std::mutex> lock(
			    systemStatusMutex_);  //lock for rest of scope
			lastConsoleErrTime_   = "0";
			lastConsoleErr_       = "";
			lastConsoleWarnTime_  = "0";
			lastConsoleWarn_      = "";
			lastConsoleInfoTime_  = "0";
			lastConsoleInfo_      = "";
			firstConsoleErrTime_  = "0";
			firstConsoleErr_      = "";
			firstConsoleWarnTime_ = "0";
			firstConsoleWarn_     = "";
			firstConsoleInfoTime_ = "0";
			firstConsoleInfo_     = "";

		}  //end resetConsoleCounts
		else if(requestType == "getRemoteSubsystems" ||
		        requestType == "getRemoteSubsystemStatus")
		{
			std::string fsmName = CgiDataUtilities::getData(cgiIn, "fsmName");
			bool getRunTypeNote = CgiDataUtilities::getDataAsInt(cgiIn, "getRunTypeNote");
			bool getFullInfo    = (requestType == "getRemoteSubsystems");
			bool getRunNumber   = CgiDataUtilities::getDataAsInt(cgiIn, "getRunNumber");

			//if full info, then add:
			//	- system and remote aliases, translations, group notes
			//	- run type note
			// - log file rollover mode

			if(getFullInfo)
			{
				__SUP_COUTV__(fsmName);
				__SUP_COUTV__(getRunTypeNote);

				//get system and remote aliases, translations, group notes (a la getAliasList request)
				addFilteredConfigAliasesToXML(xmlOut, fsmName);

				addRequiredFsmLogInputToXML(xmlOut,
				                            fsmName);  //(a la getStateMachine request)

			}  //end getFullInfo prepend

			{  //emit SubsystemCommon lists, cached by backbone key
				try
				{
					std::string timeString;
					auto activeBackbone = ConfigurationManager::loadGroupNameAndKey(
					    ConfigurationManager::LAST_ACTIVATED_BACKBONE_GROUP_FILE,
					    timeString);
					std::string backboneKey =
					    activeBackbone.first + ":" + activeBackbone.second.toString();

					if(backboneKey != cachedSubsystemCommonBackboneKey_)
					{
						ConfigurationManager temporaryConfigMgr;
						cachedSubsystemCommonList_                = "";
						cachedSubsystemCommonOverrideList_        = "";
						cachedSubsystemCommonContextList_         = "";
						cachedSubsystemCommonContextOverrideList_ = "";
						try
						{
							cachedSubsystemCommonList_ = StringMacros::setToString(
							    temporaryConfigMgr.getVersionAliases(
							        ConfigurationManager::
							            SUBSYSTEM_COMMON_VERSION_ALIAS));
						}
						catch(...)
						{
						}
						try
						{
							cachedSubsystemCommonOverrideList_ =
							    StringMacros::setToString(
							        temporaryConfigMgr.getVersionAliases(
							            ConfigurationManager::
							                SUBSYSTEM_COMMON_OVERRIDE_VERSION_ALIAS));
						}
						catch(...)
						{
						}
						try
						{
							cachedSubsystemCommonContextList_ = StringMacros::setToString(
							    temporaryConfigMgr.getVersionAliases(
							        ConfigurationManager::
							            SUBSYSTEM_COMMON_CONTEXT_VERSION_ALIAS));
						}
						catch(...)
						{
						}
						try
						{
							cachedSubsystemCommonContextOverrideList_ = StringMacros::
							    setToString(temporaryConfigMgr.getVersionAliases(
							        ConfigurationManager::
							            SUBSYSTEM_COMMON_CONTEXT_OVERRIDE_VERSION_ALIAS));
						}
						catch(...)
						{
						}
						cachedSubsystemCommonBackboneKey_ = backboneKey;
					}

					xmlOut.addTextElementToData("SubsystemCommonList",
					                            cachedSubsystemCommonList_);
					xmlOut.addTextElementToData("SubsystemCommonOverrideList",
					                            cachedSubsystemCommonOverrideList_);
					xmlOut.addTextElementToData("SubsystemCommonContextList",
					                            cachedSubsystemCommonContextList_);
					xmlOut.addTextElementToData(
					    "SubsystemCommonContextOverrideList",
					    cachedSubsystemCommonContextOverrideList_);
				}
				catch(...)
				{
					__SUP_COUT_WARN__
					    << "Failed to retrieve SubsystemCommon lists for status poll."
					    << __E__;
				}
			}  //end SubsystemCommon lists

			{  //get system status

				xmlOut.addTextElementToData(
				    "last_run_log_entry",
				    getLastLogEntry(RunControlStateMachine::START_TRANSITION_NAME,
				                    fsmName));

				//getIterationPlanStatus returns iterator status and does not request next run number (which is expensive)
				//	.. so only get run number 1:10
				//getIterationPlanStatus will repeat a few fields, but js will just take first field, so doesnt matter
				addStateMachineStatusToXML(
				    xmlOut, fsmName, getRunNumber);  //(a la getCurrentState request)
				theIterator_.handleCommandRequest(xmlOut, "getIterationPlanStatus", "");

				std::lock_guard<std::mutex> lock(
				    systemStatusMutex_);  //lock for rest of scope

				xmlOut.addTextElementToData("last_logbook_entry", lastLogbookEntry_);
				xmlOut.addTextElementToData(
				    "last_logbook_entry_time",
				    lastLogbookEntryTime_
				        ? StringMacros::getTimestampString(lastLogbookEntryTime_)
				        : "0");
				auto msgPair = theWebUsers_.getLastSystemMessage();
				xmlOut.addTextElementToData("last_system_message", msgPair.first);
				xmlOut.addTextElementToData(
				    "last_system_message_time",
				    msgPair.second ? StringMacros::getTimestampString(msgPair.second)
				                   : "0");
				xmlOut.addNumberElementToData("active_user_count",
				                              theWebUsers_.getActiveUserCount());
				xmlOut.addTextElementToData(
				    "active_user_list", theWebUsers_.getActiveUserDisplayNamesString());
				xmlOut.addNumberElementToData("console_err_count",
				                              systemConsoleErrCount_);
				xmlOut.addNumberElementToData("console_warn_count",
				                              systemConsoleWarnCount_);
				xmlOut.addNumberElementToData("console_info_count",
				                              systemConsoleInfoCount_);
				xmlOut.addTextElementToData("last_console_err_msg", lastConsoleErr_);
				xmlOut.addTextElementToData("last_console_warn_msg", lastConsoleWarn_);
				xmlOut.addTextElementToData("last_console_info_msg", lastConsoleInfo_);
				xmlOut.addTextElementToData("last_console_err_msg_time",
				                            lastConsoleErrTime_);
				xmlOut.addTextElementToData("last_console_warn_msg_time",
				                            lastConsoleWarnTime_);
				xmlOut.addTextElementToData("last_console_info_msg_time",
				                            lastConsoleInfoTime_);
				xmlOut.addTextElementToData("first_console_err_msg", firstConsoleErr_);
				xmlOut.addTextElementToData("first_console_warn_msg", firstConsoleWarn_);
				xmlOut.addTextElementToData("first_console_info_msg", firstConsoleInfo_);
				xmlOut.addTextElementToData("first_console_err_msg_time",
				                            firstConsoleErrTime_);
				xmlOut.addTextElementToData("first_console_warn_msg_time",
				                            firstConsoleWarnTime_);
				xmlOut.addTextElementToData("first_console_info_msg_time",
				                            firstConsoleInfoTime_);

			}  //end get system status

			std::vector<GatewaySupervisor::RemoteGatewayInfo>
			    remoteGatewayApps;  //local copy
			{                       //lock for remainder of scope
				std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
				__SUP_COUTVS__(TLVL_RemoteFSMRequests, remoteGatewayApps_.size());
				remoteGatewayApps = remoteGatewayApps_;
				if(remoteGatewayApps_.size())
					__SUP_COUTS__(TLVL_RemoteFSMRequests)
					    << __COUT_HDR__ << remoteGatewayApps_[0].command << " "
					    << (remoteGatewayApps_[0].appInfo.status) << __E__;
			}

			std::string accumulateErrors = "";
			for(const auto& remoteSubsystem : remoteGatewayApps)
			{
				xmlOut.addTextElementToData("subsystem_name",
				                            remoteSubsystem.appInfo.name);
				xmlOut.addTextElementToData("subsystem_url", remoteSubsystem.appInfo.url);
				xmlOut.addTextElementToData("subsystem_landingPage",
				                            remoteSubsystem.landingPage);
				xmlOut.addTextElementToData("subsystem_status",
				                            remoteSubsystem.appInfo.status);
				xmlOut.addTextElementToData(
				    "subsystem_progress",
				    std::to_string(remoteSubsystem.appInfo.progress));
				xmlOut.addTextElementToData("subsystem_detail",
				                            remoteSubsystem.appInfo.detail);
				xmlOut.addTextElementToData("subsystem_lastStatusTime",
				                            StringMacros::getTimestampString(
				                                remoteSubsystem.appInfo.lastStatusTime));
				xmlOut.addTextElementToData(
				    "subsystem_consoleErrCount",
				    std::to_string(remoteSubsystem.consoleErrCount));
				xmlOut.addTextElementToData(
				    "subsystem_consoleWarnCount",
				    std::to_string(remoteSubsystem.consoleWarnCount));

				if(remoteSubsystem.command == "" && remoteSubsystem.getError() != "")
				{
					__SUP_COUTS__(TLVL_RemoteFSMRequests)
					    << "Error from Subsystem '" << remoteSubsystem.appInfo.name
					    << "' = " << remoteSubsystem.getError() << __E__;

					if(remoteSubsystem.getError().find(
					       "Failure gathering Remote Gateway desktop icons") ==
					   std::string::npos)  //only add if not Icon error
					{
						if(accumulateErrors.size())
							accumulateErrors += "\n";
						accumulateErrors += "Error at " +
						                    remoteSubsystem.getErrorTimestamp() + ":\n" +
						                    remoteSubsystem.getError();
					}
				}

				//special values for managing remote subsystems
				xmlOut.addTextElementToData("subsystem_configAlias",
				                            remoteSubsystem.selected_config_alias);

				if(getFullInfo)
				{
					if(remoteSubsystem.user_data_path_record == "")
					{
						// __SUP_SS__;
						__SUP_COUT_WARN__
						    << "Remote Subsystem '" << remoteSubsystem.appInfo.name
						    << "' user data path is empty. Perhaps the system is still "
						       "booting up. If the problem persists, note that Remote "
						       "Subsystems are specified through their Desktop Icon "
						       "record. "
						       "Please specify a valid User Data Path record as the "
						       "Desktop Icon AlternateText field, targeting a UID in the "
						       "SubsystemUserDataPathsTable (or contact system admins "
						       "for assitance)."
						    << __E__;
						// __SUP_SS_THROW__;
					}
				}
				xmlOut.addTextElementToData(
				    "subsystem_configAliasChoices",
				    StringMacros::setToString(remoteSubsystem.config_aliases,
				                              {','}));  //CSV list of aliases
				xmlOut.addTextElementToData("subsystem_fsmMode",
				                            remoteSubsystem.getFsmMode());
				xmlOut.addTextElementToData("subsystem_fsmIncluded",
				                            remoteSubsystem.fsm_included ? "1" : "0");
			}  //end remote app loop

			if(accumulateErrors != "")
				xmlOut.addTextElementToData("system_error", accumulateErrors);

		}  //end getRemoteSubsystems
		else if(requestType == "setRemoteSubsystemFsmControl")
		{
			std::string targetSubsystem =
			    CgiDataUtilities::getData(cgiIn, "targetSubsystem");  // * for all
			std::string setValue = CgiDataUtilities::getData(cgiIn, "setValue");
			std::string controlType =
			    CgiDataUtilities::getData(cgiIn, "controlType");  // include, mode

			setValue = StringMacros::decodeURIComponent(setValue);
			__SUP_COUTV__(targetSubsystem);
			__SUP_COUTV__(setValue);
			__SUP_COUTV__(controlType);

			bool                        changedSomething = false;
			std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
			for(auto& remoteGatewayApp : remoteGatewayApps_)
				if(targetSubsystem == "*" ||
				   targetSubsystem == remoteGatewayApp.appInfo.name)
				{
					changedSomething = true;
					if(controlType == "include")
						remoteGatewayApp.fsm_included = setValue == "1" ? true : false;
					if(controlType == "configAlias")
					{
						if(remoteGatewayApp.config_aliases.find(setValue) ==
						   remoteGatewayApp.config_aliases.end())
						{
							__SUP_SS__
							    << "Configuration Alias value '" << setValue
							    << "' for target Subsystem '"
							    << remoteGatewayApp.appInfo.name
							    << "' is not found in list of Configuration Aliases: "
							    << StringMacros::setToString(
							           remoteGatewayApp.config_aliases)
							    << __E__;
							__SUP_SS_THROW__;
						}
						remoteGatewayApp.selected_config_alias = setValue;
					}
					else if(controlType == "mode")
						remoteGatewayApp.fsm_mode =
						    setValue == "Do Not Halt"
						        ? RemoteGatewayInfo::FSM_ModeTypes::DoNotHalt
						        : (setValue == "Only Configure"
						               ? RemoteGatewayInfo::FSM_ModeTypes::OnlyConfigure
						               : RemoteGatewayInfo::FSM_ModeTypes::Follow_FSM);
				}

			if(!changedSomething)
			{
				__SUP_SS__ << "Did not find any matching subsystems for target '"
				           << targetSubsystem << "' attempted!" << __E__;
				__SUP_SS_THROW__;
			}

			saveRemoteGatewaySettings();

		}  //end setRemoteSubsystemFsmControl
		else if(requestType == "getSubsystemConfigAliasSelectInfo")
		{
			std::string targetSubsystem =
			    CgiDataUtilities::getData(cgiIn, "targetSubsystem");
			__SUP_COUTV__(targetSubsystem);
			//return info on selected_config_alias

			bool found = false;
			std::vector<GatewaySupervisor::RemoteGatewayInfo>
			    remoteGatewayApps;  //local copy
			{                       //lock for remainder of scope
				std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
				__SUP_COUTVS__(TLVL_RemoteFSMRequests, remoteGatewayApps_.size());
				remoteGatewayApps = remoteGatewayApps_;
				if(remoteGatewayApps_.size())
					__SUP_COUT_TYPE__(TLVL_DEBUG + TLVL_RemoteFSMRequests)
					    << __COUT_HDR__ << remoteGatewayApps_[0].command << " "
					    << (remoteGatewayApps_[0].appInfo.status) << __E__;
			}
			std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
			for(auto& remoteGatewayApp : remoteGatewayApps)
				if(targetSubsystem == remoteGatewayApp.appInfo.name)
				{
					found = true;

					if(remoteGatewayApp.selected_config_alias == "")
					{
						__SUP_SS__ << "No selected Configuration Alias found for target "
						              "Subsystem '"
						           << remoteGatewayApp.appInfo.name
						           << "' - please select one before requesting info."
						           << __E__;
						__SUP_SS_THROW__;
					}

					std::pair<std::string, TableGroupKey> groupTranslation;
					std::string groupComment, groupAuthor, groupCreationTime;

					ConfigurationManager
					    tmpCfgMgr;  // Creating new temporary instance to not mess up member CorePropertySupervisorBase::theConfigurationManager_
					tmpCfgMgr.getOtherSubsystemConfigAliasInfo(
					    remoteGatewayApp.user_data_path_record,
					    remoteGatewayApp.selected_config_alias,
					    groupTranslation,
					    groupComment,
					    groupAuthor,
					    groupCreationTime);

					std::stringstream returnInfo;
					returnInfo << "At remote Subsystem <b>'"
					           << remoteGatewayApp.appInfo.name
					           << ",'</b> the Configure Alias <b>'"
					           << remoteGatewayApp.selected_config_alias
					           << "'</b>  translates to <b>" << groupTranslation.first
					           << "(" << groupTranslation.second
					           << ")</b>  w/comment: <br><br><i>"
					           << StringMacros::decodeURIComponent(groupComment);
					if(groupCreationTime != "" && groupCreationTime != "0")
						returnInfo << "<br><br>";
					returnInfo << "<b>" << groupTranslation.first << "("
					           << groupTranslation.second << ")</b> was created by "
					           << groupAuthor << " ("
					           << StringMacros::getTimestampString(groupCreationTime)
					           << ")";
					returnInfo << "</i>";

					// request Global fields from remote subsystem via UDP
					try
					{
						std::vector<std::string> parsedUrl =
						    StringMacros::getVectorFromString(
						        remoteGatewayApp.appInfo.url, {':'});
						if(parsedUrl.size() == 3)
						{
							Socket            gatewayRemoteSocket(parsedUrl[1],
                                                       atoi(parsedUrl[2].c_str()));
							TransceiverSocket tmpSocket(ipAddressForStateChangesOverUDP_);
							tmpSocket.initialize();

							std::string udpRequest =
							    "GetAliasGlobalFields," +
							    remoteGatewayApp.selected_config_alias;
							__SUP_COUT__
							    << "Sending GetAliasGlobalFields to '"
							    << remoteGatewayApp.appInfo.name << "' for alias '"
							    << remoteGatewayApp.selected_config_alias << "'" << __E__;

							std::string globalFieldsResponse = tmpSocket.sendAndReceive(
							    gatewayRemoteSocket, udpRequest, 5 /*timeoutSeconds*/);

							__SUP_COUT__ << "GetAliasGlobalFields response from '"
							             << remoteGatewayApp.appInfo.name
							             << "': " << globalFieldsResponse << __E__;

							if(globalFieldsResponse.size())
								returnInfo << "<br><br><b>Global Fields:</b>"
								           << StringMacros::escapeString(
								                  globalFieldsResponse,
								                  true /*allowWhiteSpace*/,
								                  true /*forHtml*/);
						}
						else
							__SUP_COUT_WARN__ << "Could not parse remote subsystem URL '"
							                  << remoteGatewayApp.appInfo.url
							                  << "' for Global fields UDP request."
							                  << __E__;
					}
					catch(const std::runtime_error& e)
					{
						__SUP_COUT_WARN__ << "Error requesting Global fields from remote "
						                     "subsystem '"
						                  << remoteGatewayApp.appInfo.name
						                  << "': " << e.what() << __E__;
					}
					catch(...)
					{
						__SUP_COUT_WARN__
						    << "Unknown error requesting Global fields from "
						       "remote subsystem '"
						    << remoteGatewayApp.appInfo.name << "'." << __E__;
					}

					xmlOut.addTextElementToData("alias_info", returnInfo.str());
					break;
				}

			if(!found)
			{
				__SUP_SS__ << "Did not find any matching subsystems for target '"
				           << targetSubsystem << "' attempted!" << __E__;
				__SUP_SS_THROW__;
			}

		}  //end getSubsystemConfigAliasSelectInfo
		else if(requestType == "getAliasGlobalFields")
		{
			std::string configAlias = CgiDataUtilities::getData(cgiIn, "configAlias");
			__SUP_COUTV__(configAlias);

			ConfigurationManager tmpCfgMgr;
			auto groupPair = tmpCfgMgr.getTableGroupFromAlias(configAlias);
			if(groupPair.first == "")
			{
				__SUP_SS__ << "Could not find group for alias '" << configAlias << "'."
				           << __E__;
				__SUP_SS_THROW__;
			}

			__SUP_COUT__ << "getAliasGlobalFields - loading group '" << groupPair.first
			             << "(" << groupPair.second << ")' for alias '" << configAlias
			             << "'" << __E__;

			std::map<std::string, TableVersion> groupMembers;
			tmpCfgMgr.loadTableGroup(groupPair.first,
			                         groupPair.second,
			                         false /*doActivate*/,
			                         &groupMembers,
			                         0 /*progressBar*/,
			                         0 /*accumulateWarnings*/,
			                         0 /*groupComment*/,
			                         0 /*groupAuthor*/,
			                         0 /*groupCreateTime*/,
			                         true /*doNotLoadMembers*/);

			std::map<std::string, TableVersion> globalMembers;
			for(const auto& member : groupMembers)
				if(member.first.find("Global") != std::string::npos)
					globalMembers.emplace(member);

			__SUP_COUT__ << "getAliasGlobalFields - found " << globalMembers.size()
			             << " Global table(s) out of " << groupMembers.size()
			             << " total members." << __E__;

			if(globalMembers.size())
				tmpCfgMgr.loadMemberMap(globalMembers);

			xmlOut.addTextElementToData("global_fields_string",
			                            getGlobalFieldsString(&tmpCfgMgr, globalMembers));

		}  //end getAliasGlobalFields
		else if(requestType == "commandRemoteSubsystem")
		{
			std::string targetSubsystem =
			    CgiDataUtilities::getData(cgiIn, "targetSubsystem");
			std::string command   = CgiDataUtilities::getData(cgiIn, "command");
			std::string parameter = CgiDataUtilities::getData(cgiIn, "parameter");
			std::string fsmName   = CgiDataUtilities::getData(cgiIn, "fsmName");

			__SUP_COUTV__(targetSubsystem);
			__SUP_COUTV__(command);
			__SUP_COUTV__(parameter);
			__SUP_COUTV__(fsmName);

			if(command == "")
			{
				__SUP_SS__
				    << "Illegal empty command received to target remote subsystem '"
				    << targetSubsystem << "' attempted!" << __E__;
				__SUP_SS_THROW__;
			}
			if(targetSubsystem == "")
			{
				__SUP_SS__ << "Illegal empty targetSubsystem received for remote "
				              "subsystem command '"
				           << command << "' attempted!" << __E__;
				__SUP_SS_THROW__;
			}

			bool                        found = false;
			std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
			for(auto& remoteGatewayApp : remoteGatewayApps_)
			{
				if(targetSubsystem == remoteGatewayApp.appInfo.name)
				{
					if(remoteGatewayApp.command != "")
					{
						__SUP_SS__
						    << "Can not target the remote subsystem '" << targetSubsystem
						    << "' with command '" << command
						    << "' which already has a pending command '"
						    << remoteGatewayApp.command
						    << ".' Please try again after the pending command is sent."
						    << __E__;
						__SUP_SS_THROW__;
					}
					remoteGatewayApp.clearError();  //clear to see result of this command
					remoteGatewayApp.command =
					    command + (parameter != "" ? ("," + parameter) : "");

					if(command == RunControlStateMachine::CONFIGURE_TRANSITION_NAME)
					{
						std::string subsystemCommonList = StringMacros::setToString(
						    theConfigurationManager_->getVersionAliases(
						        ConfigurationManager::SUBSYSTEM_COMMON_VERSION_ALIAS));
						if(subsystemCommonList.size())
							remoteGatewayApp.command +=
							    "," + COMMAND_PARAM_SUBSYSTEM_COMMON_PREAMBLE +
							    StringMacros::encodeURIComponent(subsystemCommonList);

						std::string subsystemCommonOverrideList =
						    StringMacros::setToString(
						        theConfigurationManager_->getVersionAliases(
						            ConfigurationManager::
						                SUBSYSTEM_COMMON_OVERRIDE_VERSION_ALIAS));
						if(subsystemCommonOverrideList.size())
							remoteGatewayApp.command +=
							    "," + COMMAND_PARAM_SUBSYSTEM_COMMON_OVERRIDE_PREAMBLE +
							    StringMacros::encodeURIComponent(
							        subsystemCommonOverrideList);
					}

					{
						std::string logEntry = getLastLogEntry(command);
						if(logEntry.size())
							remoteGatewayApp.command +=
							    "," + COMMAND_PARAM_LOG_ENTRY_PREAMBLE +
							    StringMacros::encodeURIComponent(logEntry);
					}

					//for non-FSM commands, do not modify fsmName
					if(command != "ResetConsoleCounts")
						remoteGatewayApp.fsmName = fsmName;

					//force status for immediate user feedback
					remoteGatewayApp.appInfo.status   = "Launching " + command;
					remoteGatewayApp.appInfo.progress = 0;
					found                             = true;
				}
			}  //end search for targetSubsystem

			if(!found)
			{
				__SUP_SS__ << "Target remote subsystem '" << targetSubsystem
				           << "' was not found for attempted command '" << command << "!'"
				           << __E__;
				__SUP_SS_THROW__;
			}
		}
		else if(requestType == "propagateLoginToSubsystem")
		{
			// Force re-propagation of the primary gateway's login verification parameters
			// (IP, port, name) to a named remote subsystem via UDP.
			// This is needed when a subsystem is restarted and has lost its
			// remoteLoginVerificationEnabled_ state — normally recovered only on the next
			// periodic AppStatusWorkLoop poll, but this call forces it immediately.

			std::string targetSubsystem =
			    CgiDataUtilities::getData(cgiIn, "targetSubsystem");

			__SUP_COUTV__(targetSubsystem);

			if(targetSubsystem == "")
			{
				__SUP_SS__
				    << "Illegal empty targetSubsystem for propagateLoginToSubsystem!"
				    << __E__;
				__SUP_SS_THROW__;
			}

			if(!portForReverseLoginOverUDP_)
			{
				__SUP_SS__ << "Reverse login propagation over UDP is not enabled at this "
				              "Gateway (portForReverseLoginOverUDP_ == 0). Check "
				              "'EnableAckForStateChangesOverUDP' and "
				              "'PortForStateChangesOverUDP' configuration."
				           << __E__;
				__SUP_SS_THROW__;
			}

			bool        found = false;
			std::string remoteGatewayUrl;
			{
				std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
				for(auto& remoteGatewayApp : remoteGatewayApps_)
				{
					if(targetSubsystem == remoteGatewayApp.appInfo.name)
					{
						found            = true;
						remoteGatewayUrl = remoteGatewayApp.appInfo.url;
						break;
					}
				}
			}

			if(found)
			{
				std::vector<std::string> parsedFields =
				    StringMacros::getVectorFromString(remoteGatewayUrl, {':'});
				if(parsedFields.size() != 3)
				{
					__SUP_SS__ << "Malformed URL for subsystem '" << targetSubsystem
					           << "': " << remoteGatewayUrl << __E__;
					__SUP_SS_THROW__;
				}

				Socket      gatewayRemoteSocket(parsedFields[1],
                                           atoi(parsedFields[2].c_str()));
				std::string requestString =
				    "GetRemoteGatewayStatus," + ipAddressForStateChangesOverUDP_ + "," +
				    std::to_string(portForReverseLoginOverUDP_) + "," + targetSubsystem;

				__SUP_COUT_INFO__ << "Propagating login verification to subsystem '"
				                  << targetSubsystem << "' via UDP: " << requestString
				                  << __E__;

				TransceiverSocket tmpSocket(ipAddressForStateChangesOverUDP_);
				tmpSocket.initialize();
				std::string response = tmpSocket.sendAndReceive(
				    gatewayRemoteSocket, requestString, 5 /*timeoutSeconds*/);

				__SUP_COUT_INFO__ << "Response from '" << targetSubsystem
				                  << "': " << response.substr(0, 200) << __E__;
				xmlOut.addTextElementToData("response", response.substr(0, 200));
			}

			if(!found)
			{
				__SUP_SS__ << "Target remote subsystem '" << targetSubsystem
				           << "' was not found for propagateLoginToSubsystem!" << __E__;
				__SUP_SS_THROW__;
			}
		}
		else if(requestType == "gatewayLaunchOTS" || requestType == "gatewayLaunchWiz")
		{
			// NOTE: similar to ConfigurationGUI version but DOES keep active login
			// sessions

			__COUT_WARN__ << requestType << " requestType received! " << __E__;

			// gateway launch is different, in that it saves user sessions
			theWebUsers_.saveActiveSessions();

			// now launch

			if(requestType == "gatewayLaunchOTS")
				GatewaySupervisor::launchStartOTSCommand(
				    "LAUNCH_OTS", CorePropertySupervisorBase::theConfigurationManager_);
			else if(requestType == "gatewayLaunchWiz")
				GatewaySupervisor::launchStartOneServerCommand(
				    "LAUNCH_WIZ",
				    CorePropertySupervisorBase::theConfigurationManager_,
				    getContextUID());
		}
		else if(requestType == "gatewayLaunchOTSInstance")
		{
			__COUT_WARN__ << requestType << " requestType received! " << __E__;

			std::string targetSubsystem =
			    CgiDataUtilities::getData(cgiIn, "targetSubsystem");
			__SUP_COUTV__(targetSubsystem);
			//launch Target Subsystem's remote ots instance

			bool                        found = false;
			std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
			for(auto& remoteGatewayApp : remoteGatewayApps_)
				if(targetSubsystem == remoteGatewayApp.appInfo.name)
				{
					found = true;

					std::stringstream commandSs;
					commandSs << "LAUNCH_INSTANCE";
					commandSs << ";" << remoteGatewayApp.instanceUser;
					commandSs << ";" << remoteGatewayApp.instanceHost;
					//assume ots path is parent of USER_DATA
					size_t i = remoteGatewayApp.instancePath.rfind('/');
					if(i != std::string::npos)
						commandSs << ";" << remoteGatewayApp.instancePath.substr(0, i);
					else
						commandSs << ";" << remoteGatewayApp.instancePath;
					commandSs << ";"
					          << "Normal";
					commandSs << ";" << remoteGatewayApp.setupType;
					commandSs << ";"
					          << remoteGatewayApp.instancePath;  //full USER_DATA path
					__SUP_COUTV__(commandSs.str());

					GatewaySupervisor::launchStartOneServerCommand(
					    commandSs.str(),
					    //"LAUNCH_INSTANCE;user;hostname;/home/user/ots_spack_fast;Normal;shift1",
					    CorePropertySupervisorBase::theConfigurationManager_,
					    getContextUID());

					//force status for immediate user feedback
					remoteGatewayApp.command =
					    "Reboot";  //use command process for getting updated status
					remoteGatewayApp.appInfo.status   = "Rebooting... ";
					remoteGatewayApp.appInfo.progress = 1;
				}

			if(!found)
			{
				__SUP_SS__ << "Did not find any matching subsystems for target '"
				           << targetSubsystem << "' attempted!" << __E__;
				__SUP_SS_THROW__;
			}
		}
		else if(requestType == "resetUserTooltips")
		{
			WebUsers::resetAllUserTooltips(userInfo.username_);
		}
		else if(requestType == "silenceUserTooltips")
		{
			WebUsers::silenceAllUserTooltips(userInfo.username_);
		}
		else if(requestType == "restartApps") /*NEW: ADDED FOR APPS RESTART*/
		{
			std::string contextName = CgiDataUtilities::getData(cgiIn, "contextName");
			__COUT__ << "launch ots script Command = "
			         << "OTS_APP_SHUTDOWN" << __E__;
			GatewaySupervisor::launchStartOneServerCommand(
			    "OTS_APP_SHUTDOWN",
			    CorePropertySupervisorBase::theConfigurationManager_,
			    contextName);
			sleep(5);
			GatewaySupervisor::launchStartOneServerCommand(
			    "OTS_APP_STARTUP",
			    CorePropertySupervisorBase::theConfigurationManager_,
			    contextName);

			xmlOut.addTextElementToData("status", "restarted");
		}
		else
		{
			__SS__ << "requestType Request, " << requestType
			       << ", not recognized by the Gateway Supervisor (was it intended for "
			          "another Supervisor?)."
			       << __E__;
			__SS_THROW__;
		}
	}
	catch(const std::runtime_error& e)
	{
		__SS__ << "An error was encountered handling requestType '" << requestType
		       << "':" << e.what() << __E__;
		__COUT__ << "\n" << ss.str();
		xmlOut.addTextElementToData("Error", ss.str());
	}
	catch(...)
	{
		__SS__ << "An unknown error was encountered handling requestType '" << requestType
		       << ".' "
		       << "Please check the printouts to debug." << __E__;
		try
		{
			throw;
		}  //one more try to printout extra info
		catch(const std::exception& e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...)
		{
		}
		__COUT__ << "\n" << ss.str();
		xmlOut.addTextElementToData("Error", ss.str());
	}

	// return xml doc holding server response
	xmlOut.outputXmlDocument(
	    (std::ostringstream*)out,
	    false /*dispStdOut*/,
	    true /*allowWhiteSpace*/);  // Note: allow white space need for error response

	//__COUT__ << "Done" << __E__;
}  // end request()
catch(const std::runtime_error& e)
{
	__COUT_ERR__ << "Caught error at request(): " << e.what() << __E__;
	throw;
}  // end request() exception handling

//==============================================================================
void GatewaySupervisor::addStateMachineStatusToXML(HttpXmlDocument&   xmlOut,
                                                   const std::string& fsmName,
                                                   bool getRunNumber /* = true */)
{
	xmlOut.addTextElementToData("active_fsmName", activeStateMachineName_);
	xmlOut.addTextElementToData("active_fsmWindowName", activeStateMachineWindowName_);
	xmlOut.addTextElementToData("current_state", theStateMachine_.getCurrentStateName());
	const std::string& gatewayStatus = allSupervisorInfo_.getGatewayInfo().getStatus();
	if(gatewayStatus.size() >
	       std::string(RunControlStateMachine::FAILED_STATE_NAME).length() &&
	   (gatewayStatus[0] == 'F' ||
	    gatewayStatus[0] ==
	        'E'))  //assume it is Failed or Error and send to state machine
		xmlOut.addTextElementToData("current_error", gatewayStatus);

	if(theStateMachine_.isInTransition())
	{  // create lock scope to read stable value
		std::lock_guard<std::mutex> lock(broadcastCommandStatusUpdateMutex_);
		xmlOut.addTextElementToData("active_fsmStatus", broadcastCommandStatus_);
	}
	else
		xmlOut.addTextElementToData("active_fsmStatus", gatewayStatus);

	xmlOut.addTextElementToData("in_transition",
	                            theStateMachine_.isInTransition() ? "1" : "0");
	if(theStateMachine_.isInTransition())
	{
		xmlOut.addTextElementToData(
		    "transition_progress",
		    RunControlStateMachine::theProgressBar_.readPercentageString());
		xmlOut.addTextElementToData("current_transition",
		                            theStateMachine_.getCurrentTransitionName());
	}
	else
	{
		xmlOut.addTextElementToData("transition_progress", "100");
		xmlOut.addTextElementToData("current_transition", "");
	}
	xmlOut.addTextElementToData("time_in_state",
	                            std::to_string(theStateMachine_.getTimeInState()));

	// char tmp[20]; old size before adding db run number
	char tmp
	    [50];  // for a 6 digits run number from the DB, this needs to be at least 34 chars

	//__COUT__ << "current state: " << theStateMachine_.getCurrentStateName() <<
	//__E__;

	//// ======================== get run alias based on fsm name ====

	// std::string fsmName = CgiDataUtilities::getData(cgiIn, "fsmName");
	//		__COUT__ << "fsmName = " << fsmName << __E__;
	//		__COUT__ << "activeStateMachineName_ = " << activeStateMachineName_ <<
	//__E__;
	//		__COUT__ << "theStateMachine_.getProvenanceStateName() = " <<
	//				theStateMachine_.getProvenanceStateName() << __E__;
	//		__COUT__ << "theStateMachine_.getCurrentStateName() = " <<
	//				theStateMachine_.getCurrentStateName() << __E__;
	bool useRunInfoDb = false;

	if(!theStateMachine_.isInTransition())
	{
		if(RunControlStateMachine::asyncStopExceptionReceived_)
		{
			//__COUTV__(RunControlStateMachine::asyncPauseExceptionReceived_);
			//__COUTV__(RunControlStateMachine::getErrorMessage());
			xmlOut.addTextElementToData("soft_error",
			                            RunControlStateMachine::getErrorMessage());
		}

		std::string stateMachineRunAlias   = "Run";  // default to "Run"
		bool        rollOverLogOnConfigure = false, rollOverLogOnStart = false;
		std::string rollOverLogOnSize = "";

		// get stateMachineAliasFilter if possible
		ConfigurationTree configLinkNode =
		    CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(
		        supervisorContextUID_, supervisorApplicationUID_);

		if(!configLinkNode.isDisconnected())
		{
			try  // for backwards compatibility
			{
				ConfigurationTree fsmLinkNode =
				    configLinkNode.getNode("LinkToStateMachineTable");
				if(!fsmLinkNode.isDisconnected())
				{
					if(!fsmLinkNode.getNode(fsmName + "/RunDisplayAlias")
					        .isDefaultValue())
						stateMachineRunAlias =
						    fsmLinkNode.getNode(fsmName + "/RunDisplayAlias")
						        .getValue<std::string>();
					std::string runInfoPluginType =
					    fsmLinkNode.getNode(fsmName + "/RunInfoPluginType")
					        .getValue<std::string>();
					if(runInfoPluginType !=
					       TableViewColumnInfo::DATATYPE_STRING_DEFAULT &&
					   runInfoPluginType !=
					       TableViewColumnInfo::DATATYPE_STRING_ALT_DEFAULT &&
					   runInfoPluginType != "No Run Info Plugin")
						useRunInfoDb = true;

					try
					{
						rollOverLogOnConfigure =
						    fsmLinkNode.getNode("RollOverLogOnConfigure")
						        .getValueWithDefault<bool>(false /* defaultValue */);
					}
					catch(...)
					{
						rollOverLogOnConfigure = false;
					}
					try
					{
						rollOverLogOnStart =
						    fsmLinkNode.getNode("RollOverLogOnConfigure")
						        .getValueWithDefault<bool>(false /* defaultValue */);
					}
					catch(...)
					{
						rollOverLogOnStart = false;
					}
				}
			}
			catch(std::runtime_error& e)
			{
				;  //ignoring error
			}
			catch(...)
			{
				__COUT_ERR__
				    << "Unknown error looking for Run alias. Should never happen."
				    << __E__;
			}
		}

		xmlOut.addTextElementToData("stateMachineRunAlias", stateMachineRunAlias);

		//generate log rollover string
		{
			rollOverLogOnSize =
			    (getenv("OTS_LOG_ROLLOVER") ? getenv("OTS_LOG_ROLLOVER") : "");
			std::stringstream ss;
			if(rollOverLogOnConfigure || rollOverLogOnStart || rollOverLogOnSize != "")
			{
				ss << "ots log files will rollover ";
				if(!rollOverLogOnConfigure &&
				   !rollOverLogOnStart)  //the rollOverLogOnSize != ""
					ss << " on size-in-bytes: " << rollOverLogOnSize
					   << ". To enable on FSM transitions set RollOverLogOnConfigure "
					      "and/or RollOverLogOnStart in the FSM Configuration Tree.";
				else
				{
					if(rollOverLogOnConfigure && rollOverLogOnStart)
						ss << " on the Configure and Start FSM transitions";
					else if(rollOverLogOnConfigure)
						ss << " on the Configure FSM transition";
					else if(rollOverLogOnStart)
						ss << " on the Start FSM transition";

					if(rollOverLogOnSize != "")
						ss << ". To enable rollover on 100MB size, for example, export "
						      "OTS_LOG_ROLLOVER=100000000.";
					else
						ss << ", and also on size-in-bytes: " << rollOverLogOnSize;
				}
			}
			else
				ss << "ots log files will not rollover. "
				      "To enable rollover on 100MB size, for example, export "
				      "OTS_LOG_ROLLOVER=100000000; "
				      "to enable on FSM transitions set RollOverLogOnConfigure and/or "
				      "RollOverLogOnStart in the FSM Configuration Tree.";

			xmlOut.addTextElementToData("stateMachineLogRollover", ss.str());
		}

		//// ======================== get run number based on fsm name ====

		if(theStateMachine_.getCurrentStateName() ==
		       RunControlStateMachine::RUNNING_STATE_NAME ||
		   theStateMachine_.getCurrentStateName() ==
		       RunControlStateMachine::PAUSED_STATE_NAME)
		{
			if(useRunInfoDb)
				sprintf(tmp,
				        "Current %s Number from DB: %s",
				        activeStateMachineRunAlias_.c_str(),
				        activeStateMachineRunNumber_.c_str());
			//%u // getNextRunNumber(activeStateMachineName_) - 1);
			else
				sprintf(
				    tmp,
				    "Current %s Number: %s",
				    activeStateMachineRunAlias_.c_str(),
				    activeStateMachineRunNumber_
				        .c_str());  //%u //getNextRunNumber(activeStateMachineName_) - 1);
			xmlOut.addTextElementToData("run_number", tmp);

			if(RunControlStateMachine::asyncPauseExceptionReceived_)
			{
				//__COUTV__(RunControlStateMachine::asyncPauseExceptionReceived_);
				//__COUTV__(RunControlStateMachine::getErrorMessage());
				xmlOut.addTextElementToData("soft_error",
				                            RunControlStateMachine::getErrorMessage());
			}
		}
		else if(
		    getRunNumber)  //only periodically get next run number (expensive from file, and shouldnt change much)
		{
			if(useRunInfoDb)
				sprintf(tmp, "Next %s Number from DB.", stateMachineRunAlias.c_str());
			else
				sprintf(tmp,
				        "Next %s Number: %u",
				        stateMachineRunAlias.c_str(),
				        getNextRunNumber(fsmName));

			xmlOut.addTextElementToData("run_number", tmp);
		}

	}  //end not-in-transition handling

	try
	{
		auto fsmNodes =
		    CorePropertySupervisorBase::theConfigurationManager_
		        ->getSupervisorTableNode(supervisorContextUID_, supervisorApplicationUID_)
		        .getNode("LinkToStateMachineTable")
		        .getChildren();
		for(const auto& fsmNode : fsmNodes)
			xmlOut.addTextElementToData("stateMachineName", fsmNode.first);
	}
	catch(...)
	{
		__COUTS__(2) << "Failed to add state machine names to XML status." << __E__;
	}
	{
		std::lock_guard<std::mutex> lock(contextCommonMutex_);
		xmlOut.addTextElementToData("AppliedContextCommonList", appliedContextCommonList_);
		xmlOut.addTextElementToData("AppliedContextCommonOverrideList", appliedContextCommonOverrideList_);
	}
}  // end addStateMachineStatusToXML()

//==============================================================================
void GatewaySupervisor::addRequiredFsmLogInputToXML(HttpXmlDocument&   xmlOut,
                                                    const std::string& fsmName)
{
	bool requireUserLogInputOnConfigure = false, requireUserLogInputOnRun = false;
	//if fsmName specified, return log entry requirements from config tree
	if(fsmName != "")
	{
		//------------------
		ConfigurationTree configLinkNode =
		    CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(
		        supervisorContextUID_, supervisorApplicationUID_);
		if(!configLinkNode.isDisconnected())
		{
			// clang-format off
			try //ignore errors
			{
				ConfigurationTree fsmLinkNode = configLinkNode.getNode("LinkToStateMachineTable").getNode(fsmName);
				try { requireUserLogInputOnConfigure = fsmLinkNode.getNode("RequireUserLogInputOnConfigureTransition").getValue<bool>(); } catch(...) { __SUP_COUTT__ << "RequireUserLogInputOnConfigureTransition not set."; }
				try { requireUserLogInputOnRun = fsmLinkNode.getNode("RequireUserLogInputOnRunTransition").getValue<bool>(); } catch(...) { __SUP_COUTT__ << "RequireUserLogInputOnRunTransition not set."; }
			}
			catch(...)
			{ __SUP_COUTT__ << "Settings not set for fsm name = " << fsmName << __E__; }
			// clang-format on
		}
	}  //end log entry requirements gathering

	xmlOut.addTextElementToData("RequireUserLogInputOnConfigureTransition",
	                            requireUserLogInputOnConfigure ? "1" : "0");
	xmlOut.addTextElementToData("RequireUserLogInputOnRunTransition",
	                            requireUserLogInputOnRun ? "1" : "0");
}  // end request()

//==============================================================================
void GatewaySupervisor::addFilteredConfigAliasesToXML(HttpXmlDocument&   xmlOut,
                                                      const std::string& fsmName)
{
	__SUP_COUTV__(fsmName);

	// IMPORTANT -- use temporary ConfigurationManager to get the Active Group Aliases,
	//	 to avoid changing the Context Configuration tree for the Gateway Supervisor
	ConfigurationManager temporaryConfigMgr;
	std::map<std::string /*alias*/, std::pair<std::string /*group name*/, TableGroupKey>>
	    aliasMap;
	aliasMap = temporaryConfigMgr.getActiveGroupAliases();

	// also IMPORTANT -- to use theConfigurationManager_ to get the Context settings for the Gateway Supervisor
	// get stateMachineAliasFilter if possible
	ConfigurationTree configLinkNode =
	    CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(
	        supervisorContextUID_, supervisorApplicationUID_);

	std::string stateMachineAliasFilter = "*";  // default to all
	if(fsmName != "" && !configLinkNode.isDisconnected())
	{
		try  // for backwards compatibility
		{
			ConfigurationTree fsmLinkNode =
			    configLinkNode.getNode("LinkToStateMachineTable");
			if(!fsmLinkNode.isDisconnected() &&
			   !fsmLinkNode.getNode(fsmName + "/SystemAliasFilter").isDefaultValue())
				stateMachineAliasFilter =
				    fsmLinkNode.getNode(fsmName + "/SystemAliasFilter")
				        .getValue<std::string>();
			else
				__COUT_INFO__ << "FSM Link disconnected." << __E__;
		}
		catch(std::runtime_error& e)
		{
			__COUT_INFO__ << e.what() << __E__;
		}
		catch(...)
		{
			__COUT_ERR__ << "Unknown error. Should never happen." << __E__;
		}
	}
	else
		__COUT_INFO__ << "FSM Link disconnected." << __E__;

	__COUT__ << "For FSM '" << fsmName
	         << ",' stateMachineAliasFilter  = " << stateMachineAliasFilter << __E__;

	// filter list of aliases based on stateMachineAliasFilter
	//  ! as first character means choose those that do NOT match filter
	//	* can be used as wild card.
	{
		bool invertFilter =
		    stateMachineAliasFilter.size() && stateMachineAliasFilter[0] == '!';
		std::vector<std::string> filterArr;

		size_t i = 0;
		if(invertFilter)
			++i;
		size_t      f;
		std::string tmp;
		while((f = stateMachineAliasFilter.find('*', i)) != std::string::npos)
		{
			tmp = stateMachineAliasFilter.substr(i, f - i);
			i   = f + 1;
			filterArr.push_back(tmp);
			//__COUT__ << filterArr[filterArr.size()-1] << " " << i <<
			//		" of " << stateMachineAliasFilter.size() << __E__;
		}
		if(i <= stateMachineAliasFilter.size())
		{
			tmp = stateMachineAliasFilter.substr(i);
			filterArr.push_back(tmp);
			//__COUT__ << filterArr[filterArr.size()-1] << " last." << __E__;
		}

		bool filterMatch;

		for(auto& aliasMapPair : aliasMap)
		{
			//__COUT__ << "aliasMapPair.first: " << aliasMapPair.first << __E__;

			filterMatch = true;

			if(filterArr.size() == 1)
			{
				if(filterArr[0] != "" && filterArr[0] != "*" &&
				   aliasMapPair.first != filterArr[0])
					filterMatch = false;
			}
			else
			{
				i = -1;
				for(f = 0; f < filterArr.size(); ++f)
				{
					if(!filterArr[f].size())
						continue;  // skip empty filters

					if(f == 0)  // must start with this filter
					{
						if((i = aliasMapPair.first.find(filterArr[f])) != 0)
						{
							filterMatch = false;
							break;
						}
					}
					else if(f == filterArr.size() - 1)  // must end with this filter
					{
						if(aliasMapPair.first.rfind(filterArr[f]) !=
						   aliasMapPair.first.size() - filterArr[f].size())
						{
							filterMatch = false;
							break;
						}
					}
					else if((i = aliasMapPair.first.find(filterArr[f])) ==
					        std::string::npos)
					{
						filterMatch = false;
						break;
					}
				}
			}

			if(invertFilter)
				filterMatch = !filterMatch;

			//__COUT__ << "filterMatch=" << filterMatch  << __E__;

			if(!filterMatch)
				continue;

			xmlOut.addTextElementToData("config_alias", aliasMapPair.first);
			xmlOut.addTextElementToData(
			    "config_key",
			    TableGroupKey::getFullGroupString(aliasMapPair.second.first,
			                                      aliasMapPair.second.second,
			                                      /*decorate as (<key>)*/ "(",
			                                      ")"));

			// __COUT__ << "config_alias_comment" << " " <<  temporaryConfigMgr.getNode(
			// 	ConfigurationManager::GROUP_ALIASES_TABLE_NAME).getNode(aliasMapPair.first).getNode(
			// 		TableViewColumnInfo::COL_NAME_COMMENT).getValue<std::string>() << __E__;
			xmlOut.addTextElementToData(
			    "config_alias_comment",
			    temporaryConfigMgr.getNode(ConfigurationManager::GROUP_ALIASES_TABLE_NAME)
			        .getNode(aliasMapPair.first)
			        .getNode(TableViewColumnInfo::COL_NAME_COMMENT)
			        .getValue<std::string>());

			std::string groupComment, groupAuthor, groupCreationTime;
			try
			{
				temporaryConfigMgr.loadTableGroup(aliasMapPair.second.first,
				                                  aliasMapPair.second.second,
				                                  false /*doActivate*/,
				                                  0 /*groupMembers*/,
				                                  0 /*progressBar*/,
				                                  0 /*accumulateWarnings*/,
				                                  &groupComment,
				                                  &groupAuthor,
				                                  &groupCreationTime,
				                                  true /*doNotLoadMembers*/);

				xmlOut.addTextElementToData("config_comment", groupComment);
				xmlOut.addTextElementToData("config_author", groupAuthor);
				xmlOut.addTextElementToData("config_create_time", groupCreationTime);
			}
			catch(...)
			{
				__COUT_WARN__ << "Failed to load group metadata." << __E__;
				xmlOut.addTextElementToData("config_comment", "");
				xmlOut.addTextElementToData("config_author", "");
				xmlOut.addTextElementToData("config_create_time", "");
			}
		}
	}

	// return last group alias by user
	std::string fn = ConfigurationManager::LAST_TABLE_GROUP_SAVE_PATH + "/" +
	                 FSM_LAST_GROUP_ALIAS_FILE_START + fsmName + "." +
	                 FSM_USERS_PREFERENCES_FILETYPE;
	__COUT__ << "Load preferences: " << fn << __E__;
	FILE* fp = fopen(fn.c_str(), "r");
	if(fp)
	{
		char tmpLastAlias[500];
		fscanf(fp, "%*s %s", tmpLastAlias);
		__COUT__ << "tmpLastAlias: " << tmpLastAlias << __E__;

		xmlOut.addTextElementToData("UserLastConfigAlias", tmpLastAlias);
		fclose(fp);
	}
	else if(aliasMap.size())  //if not set, return first
		xmlOut.addTextElementToData("UserLastConfigAlias", aliasMap.begin()->first);

	try
	{
		std::string subsystemCommonList =
		    StringMacros::setToString(temporaryConfigMgr.getVersionAliases(
		        ConfigurationManager::SUBSYSTEM_COMMON_VERSION_ALIAS));
		xmlOut.addTextElementToData("SubsystemCommonList", subsystemCommonList);

		std::string subsystemCommonOverrideList =
		    StringMacros::setToString(temporaryConfigMgr.getVersionAliases(
		        ConfigurationManager::SUBSYSTEM_COMMON_OVERRIDE_VERSION_ALIAS));
		xmlOut.addTextElementToData("SubsystemCommonOverrideList",
		                            subsystemCommonOverrideList);

		std::string subsystemCommonContextList =
		    StringMacros::setToString(temporaryConfigMgr.getVersionAliases(
		        ConfigurationManager::SUBSYSTEM_COMMON_CONTEXT_VERSION_ALIAS));
		xmlOut.addTextElementToData("SubsystemCommonContextList",
		                            subsystemCommonContextList);

		std::string subsystemCommonContextOverrideList =
		    StringMacros::setToString(temporaryConfigMgr.getVersionAliases(
		        ConfigurationManager::SUBSYSTEM_COMMON_CONTEXT_OVERRIDE_VERSION_ALIAS));
		xmlOut.addTextElementToData("SubsystemCommonContextOverrideList",
		                            subsystemCommonContextOverrideList);
	}
	catch(const std::runtime_error& e)
	{
		__COUT_WARN__ << "Failed to retrieve SubsystemCommon alias lists: " << e.what()
		              << __E__;
	}
	catch(const std::exception& e)
	{
		__COUT_WARN__ << "Failed to retrieve SubsystemCommon alias lists: " << e.what()
		              << __E__;
	}
	catch(...)
	{
		__COUT_WARN__
		    << "Failed to retrieve SubsystemCommon alias lists (unknown exception)."
		    << __E__;
	}
}  //end addFilteredConfigAliasesToXML()

//==============================================================================
std::string GatewaySupervisor::getGlobalFieldsString(
    ConfigurationManager*                      cfgMgr,
    const std::map<std::string, TableVersion>& memberMap /* = {} */)
{
	std::string result = "";
	try
	{
		// if memberMap provided, use it directly (tables loaded a la carte, not activated);
		// otherwise fall back to active versions (for status workloop where tables are activated)
		std::map<std::string, TableVersion> tablesToCheck =
		    memberMap.size() ? memberMap : cfgMgr->getActiveVersions();
		__COUTT__ << "getGlobalFieldsString() - tablesToCheck count = "
		          << tablesToCheck.size() << __E__;

		for(const auto& tablePair : tablesToCheck)
		{
			if(tablePair.first.find("Global") == std::string::npos)
				continue;

			__COUTT__ << "getGlobalFieldsString() - found Global table: '"
			          << tablePair.first << "' v" << tablePair.second << __E__;

			try
			{
				const TableBase* table = cfgMgr->getTableByName(tablePair.first);
				// use specific version view (works for non-activated tables loaded a la carte)
				const TableView& view = table->getView(tablePair.second);

				__COUTT__ << "getGlobalFieldsString() - table '" << tablePair.first
				          << "' has " << view.getNumberOfColumns() << " columns, "
				          << view.getNumberOfRows() << " rows." << __E__;

				const auto& columnsInfo = view.getColumnsInfo();
				for(unsigned int col = 0; col < columnsInfo.size(); ++col)
				{
					const std::string& colName = columnsInfo[col].getName();
					if(colName.find("Global") == std::string::npos)
						continue;
					if(colName == TableViewColumnInfo::COL_NAME_STATUS ||
					   colName == TableViewColumnInfo::COL_NAME_COMMENT ||
					   colName == TableViewColumnInfo::COL_NAME_AUTHOR ||
					   colName == TableViewColumnInfo::COL_NAME_CREATION)
						continue;

					__COUTT__ << "getGlobalFieldsString() - matched column: '" << colName
					          << "'" << __E__;

					std::string displayName = colName;
					if(displayName.rfind("Global", 0) == 0)
						displayName = displayName.substr(sizeof("Global") - 1);
					for(unsigned int row = 0; row < view.getNumberOfRows(); ++row)
					{
						result +=
						    " | " + displayName + ": " + view.getValueAsString(row, col);
					}
				}
			}
			catch(const std::runtime_error& e)
			{
				__COUT_WARN__ << "Error reading Global fields from table '"
				              << tablePair.first << "': " << e.what() << __E__;
			}
			catch(...)
			{
				__COUT_WARN__ << "Unknown error reading Global fields from table '"
				              << tablePair.first << "'." << __E__;
			}
		}
	}
	catch(const std::runtime_error& e)
	{
		__COUT_WARN__ << "Error getting Global fields: " << e.what() << __E__;
	}
	catch(...)
	{
		__COUT_WARN__ << "Unknown error getting Global fields." << __E__;
	}
	__COUTT__ << "getGlobalFieldsString() result = '" << result << "'" << __E__;
	return result;
}  //end getGlobalFieldsString()

//==============================================================================
/// launchStartOneServerCommand
///	static function (so WizardSupervisor can use it)
///	throws exception if command fails to start a server
/// Note: to get the Gateway's Context name: getContextUID()
void GatewaySupervisor::launchStartOneServerCommand(const std::string&    command,
                                                    ConfigurationManager* cfgMgr,
                                                    const std::string&    contextName)
{
	__COUT__ << "launch ots script Command = " << command << __E__;
	__COUT__ << "Extracting target context hostname... " << __E__;

	std::string hostname;
	try
	{
		cfgMgr->init();  // completely reset to re-align with any changes
		const XDAQContextTable* contextTable = cfgMgr->__GET_CONFIG__(XDAQContextTable);
		auto                    contexts     = contextTable->getContexts();

		unsigned int i, j;
		for(const auto& context : contexts)
		{
			if(context.contextUID_ != contextName)
				continue;

			__COUT__ << "contextUID_ is: " << context.contextUID_ << __E__;

			// find last slash
			j = 0;  // default to whole string
			for(i = 0; i < context.address_.size(); ++i)
				if(context.address_[i] == '/')
					j = i + 1;
			hostname = context.address_.substr(j);
			__COUT__ << "ots script command '" << command
			         << "' launching on hostname = " << hostname << " in context name "
			         << context.contextUID_ << __E__;
		}
	}
	catch(...)
	{
		__SS__ << "\nRelaunch of otsdaq interrupted! "
		       << "The Configuration Manager could not be initialized." << __E__;

		__SS_THROW__;
	}

	std::string fn = (std::string(__ENV__("SERVICE_DATA_PATH")) + "/StartOTS_action_" +
	                  hostname + ".cmd");
	FILE*       fp = fopen(fn.c_str(), "w");
	if(fp)
	{
		fprintf(fp, "%s", command.c_str());
		fclose(fp);
	}
	else
	{
		__SS__ << "Unable to open command file: " << fn << __E__;
		__SS_THROW__;
	}

	sleep(2 /*seconds*/);  // then verify that the commands were read
	// note: StartOTS.sh has a sleep of 1 second

	fn = (std::string(__ENV__("SERVICE_DATA_PATH")) + "/StartOTS_action_" + hostname +
	      ".cmd");
	fp = fopen(fn.c_str(), "r");
	if(fp)
	{
		char line[100];
		fgets(line, 100, fp);
		fclose(fp);

		if(strncmp(line, command.c_str(), 90) == 0)
		{
			__SS__ << "The command looks to have been ignored by " << hostname
			       << ". Is the ots launch script still running on that node?" << __E__;
			__SS_THROW__;
		}
		__COUTV__(line);
	}
	else
	{
		__SS__ << "Unable to open command file for verification: " << fn << __E__;
		__SS_THROW__;
	}
}  // end launchStartOneServerCommand

//==============================================================================
/// launchStartOTSCommand
///	static function (so WizardSupervisor can use it)
///	throws exception if command fails to start
void GatewaySupervisor::launchStartOTSCommand(const std::string&    command,
                                              ConfigurationManager* cfgMgr)
{
	__COUT__ << "launch ots script Command = " << command << __E__;
	__COUT__ << "Extracting target context hostnames... " << __E__;

	std::vector<std::string> hostnames;
	try
	{
		cfgMgr->init();  // completely reset to re-align with any changes

		const XDAQContextTable* contextTable = cfgMgr->__GET_CONFIG__(XDAQContextTable);

		auto         contexts = contextTable->getContexts();
		unsigned int i, j;
		for(const auto& context : contexts)
		{
			if(!context.status_)
				continue;

			// find last slash
			j = 0;  // default to whole string
			for(i = 0; i < context.address_.size(); ++i)
				if(context.address_[i] == '/')
					j = i + 1;
			hostnames.push_back(context.address_.substr(j));
			__COUT__ << "ots script command '" << command
			         << "' launching on hostname = " << hostnames.back() << __E__;
		}
	}
	catch(...)
	{
		__SS__ << "Launch of command '" << command << "' interrupted! "
		       << "The Configuration Manager could not be initialized to find targets."
		       << __E__;

		__SS_THROW__;
	}

	for(const auto& hostname : hostnames)
	{
		std::string fn = (std::string(__ENV__("SERVICE_DATA_PATH")) +
		                  "/StartOTS_action_" + hostname + ".cmd");
		FILE*       fp = fopen(fn.c_str(), "w");
		if(fp)
		{
			fprintf(fp, "%s", command.c_str());
			fclose(fp);
		}
		else
		{
			__SS__ << "Unable to open command file: " << fn << __E__;
			__SS_THROW__;
		}
	}

	sleep(2 /*seconds*/);  // then verify that the commands were read
	// note: ots script has a sleep of 1 second

	for(const auto& hostname : hostnames)
	{
		std::string fn = (std::string(__ENV__("SERVICE_DATA_PATH")) +
		                  "/StartOTS_action_" + hostname + ".cmd");
		FILE*       fp = fopen(fn.c_str(), "r");
		if(fp)
		{
			char line[100];
			fgets(line, 100, fp);
			fclose(fp);

			if(strncmp(line, command.c_str(), 90) == 0)
			{
				__SS__ << "The command '" << command << "' looks to have been ignored by "
				       << hostname
				       << ". Is the ots launch script still running on that node?"
				       << __E__;
				__SS_THROW__;
			}
			__COUTV__(line);
		}
		else
		{
			__SS__ << "Unable to open command file for verification: " << fn << __E__;
			__SS_THROW__;
		}
	}
}  // end launchStartOTSCommand

//==============================================================================
/// xoap::supervisorCookieCheck
///	verify cookie
xoap::MessageReference GatewaySupervisor::supervisorCookieCheck(
    xoap::MessageReference message)

{
	__COUTT__
	    << "request from remote Supervisor for GatewaySupervisor::supervisorCookieCheck()"
	    << __E__;

	// SOAPUtilities::receive request parameters
	SOAPParameters parameters;
	parameters.addParameter("CookieCode");
	parameters.addParameter("RefreshOption");
	parameters.addParameter("IPAddress");
	parameters.addParameter("RequireLock");
	SOAPUtilities::receive(message, parameters);
	std::string cookieCode = parameters.getValue("CookieCode");
	std::string refreshOption =
	    parameters.getValue("RefreshOption");  // give external supervisors option to
	                                           // refresh cookie or not, "1" to refresh
	std::string ipAddress =
	    parameters.getValue("IPAddress");  // give external supervisors option to refresh
	                                       // cookie or not, "1" to refresh
	bool requireLock =
	    parameters.getValue("RequireLock") == "1";  // auto-take lock if needed

	// If TRUE, cookie code is good, and refreshed code is in cookieCode, also pointers
	// optionally for uint8_t userPermissions, uint64_t uid  Else, error message is
	// returned in cookieCode
	std::map<std::string /*groupName*/, WebUsers::permissionLevel_t>
	            userGroupPermissionsMap;
	std::string userWithLock     = "";
	uint64_t    uid              = WebUsers::NOT_FOUND_IN_DATABASE;
	uint64_t    userSessionIndex = WebUsers::NOT_FOUND_IN_DATABASE;
	__COUTTV__(refreshOption);
	bool cookieIsActive = theWebUsers_.cookieCodeIsActiveForRequest(
	    cookieCode,
	    &userGroupPermissionsMap,
	    &uid /*uid is not given to remote users*/,
	    ipAddress,
	    refreshOption == "1",
	    false /* doNotGoRemote */,
	    &userWithLock,
	    &userSessionIndex);

	__COUTTV__(userWithLock);

	if(cookieIsActive)
	{
		// Mirror the auto-take logic from xmlRequestOnGateway: if request requires the
		// lock and no user currently holds it, auto-take on behalf of the remote
		// supervisor.
		if(requireLock && userWithLock == "" && uid != WebUsers::NOT_FOUND_IN_DATABASE)
		{
			std::string username = theWebUsers_.getUsersUsername(uid);
			__COUT_INFO__
			    << "Auto-taking lock for user '" << username
			    << "' on behalf of remote supervisor (lock required, none held)."
			    << __E__;
			if(theWebUsers_.setUserWithLock(uid, true /*lock*/, username))
				userWithLock = username;
		}
	}
	else
	{
		// Cookie validation failed; clear identity to avoid returning stale data.
		uid              = WebUsers::NOT_FOUND_IN_DATABASE;
		userSessionIndex = WebUsers::NOT_FOUND_IN_DATABASE;
		userWithLock     = "";
	}

	// fill return parameters
	SOAPParameters retParameters;
	retParameters.addParameter("CookieCode", cookieCode);
	retParameters.addParameter(
	    "Permissions", StringMacros::mapToString(userGroupPermissionsMap).c_str());
	retParameters.addParameter("UserWithLock", userWithLock);
	retParameters.addParameter("Username",
	                           cookieIsActive ? theWebUsers_.getUsersUsername(uid) : "");
	retParameters.addParameter(
	    "DisplayName", cookieIsActive ? theWebUsers_.getUsersDisplayName(uid) : "");
	retParameters.addParameter("UserSessionIndex",
	                           cookieIsActive ? std::to_string(userSessionIndex) : "");

	__COUTT__ << "Login response: " << retParameters.getValue("Username") << __E__;

	return SOAPUtilities::makeSOAPMessageReference("CookieResponse", retParameters);
}  // end supervisorCookieCheck()

//==============================================================================
/// xoap::supervisorGetActiveUsers
///	get display names for all active users
xoap::MessageReference GatewaySupervisor::supervisorGetActiveUsers(
    xoap::MessageReference /*message*/)
{
	__COUT__ << __E__;

	SOAPParameters parameters("UserList", theWebUsers_.getActiveUserDisplayNamesString());
	return SOAPUtilities::makeSOAPMessageReference("ActiveUserResponse", parameters);
}  // end supervisorGetActiveUsers()

//==============================================================================
/// xoap::supervisorSystemMessage
///	SOAPUtilities::receive a new system Message from a supervisor
///	ToUser wild card * is to all users
///	or comma-separated variable  (CSV) to multiple users
xoap::MessageReference GatewaySupervisor::supervisorSystemMessage(
    xoap::MessageReference message)
{
	SOAPParameters parameters;
	parameters.addParameter("ToUser");
	parameters.addParameter("Subject");
	parameters.addParameter("Message");
	parameters.addParameter("DoEmail");
	SOAPUtilities::receive(message, parameters);

	std::string toUserCSV     = parameters.getValue("ToUser");
	std::string subject       = parameters.getValue("Subject");
	std::string systemMessage = parameters.getValue("Message");
	std::string doEmail       = parameters.getValue("DoEmail");

	//do not uncomment if using custom counts - they will fire recursively if set to generate System Messages
	// __COUT__ << "systemMessage -- toUserCSV: " << toUserCSV << ", doEmail: " << doEmail << ", subject: " << subject << ", msg: " << systemMessage << __E__;

	theWebUsers_.addSystemMessage(toUserCSV, subject, systemMessage, doEmail == "1");

	return SOAPUtilities::makeSOAPMessageReference("SystemMessageResponse");
}  // end supervisorSystemMessage()

//===================================================================================================================
///static add system message (e.g. from remote monitoring)
///	toUserCSV can be "*" for all users
/// Note: intended to be thread safe.
void GatewaySupervisor::addSystemMessage(std::string toUserCSV, std::string message)
{
	__COUTTV__(toUserCSV);
	__COUTVS__(45, message);
	GatewaySupervisor::theWebUsers_.addSystemMessage(toUserCSV, message);
}  //end addSystemMessage

//===================================================================================================================
/// xoap::supervisorSystemLogbookEntry
///	SOAPUtilities::receive a new system Message from a supervisor
///	ToUser wild card * is to all users
xoap::MessageReference GatewaySupervisor::supervisorSystemLogbookEntry(
    xoap::MessageReference message)
{
	SOAPParameters parameters;
	parameters.addParameter("EntryText");
	SOAPUtilities::receive(message, parameters);

	__COUT__ << "EntryText: " << parameters.getValue("EntryText").substr(0, 10) << __E__;

	makeSystemLogEntry(parameters.getValue("EntryText"));

	return SOAPUtilities::makeSOAPMessageReference("SystemLogbookResponse");
}  //end supervisorSystemLogbookEntry()

//===================================================================================================================
/// supervisorLastTableGroupRequest
///	return the group name and key for the last state machine activity
///
///	Note: same as OtsConfigurationWizardSupervisor::supervisorLastTableGroupRequest
xoap::MessageReference GatewaySupervisor::supervisorLastTableGroupRequest(
    xoap::MessageReference message)
{
	SOAPParameters parameters;
	parameters.addParameter("ActionOfLastGroup");
	SOAPUtilities::receive(message, parameters);

	return GatewaySupervisor::lastTableGroupRequestHandler(parameters);
}  //end supervisorLastTableGroupRequest()

//===================================================================================================================
/// xoap::lastTableGroupRequestHandler
///	handles last config group request.
///	called by both:
///		GatewaySupervisor::supervisorLastTableGroupRequest
///		OtsConfigurationWizardSupervisor::supervisorLastTableGroupRequest
xoap::MessageReference GatewaySupervisor::lastTableGroupRequestHandler(
    const SOAPParameters& parameters)
{
	std::string action = parameters.getValue("ActionOfLastGroup");
	__COUT__ << "ActionOfLastGroup: " << action.substr(0, 30) << __E__;

	std::vector<std::string> actions;
	std::vector<std::string> fileNames;
	if(action == "ALL")
	{
		actions   = std::vector<std::string>({"Configured",
		                                      "Started",
		                                      "ActivatedConfig",
		                                      "ActivatedContext",
		                                      "ActivatedBackbone",
		                                      "ActivatedIterator",
		                                      "ConfiguredAlias",
		                                      "AttemptedAlias",
		                                      "AttemptedConfig"});
		fileNames = std::vector<std::string>(
		    {ConfigurationManager::LAST_CONFIGURED_CONFIG_GROUP_FILE,
		     ConfigurationManager::LAST_STARTED_CONFIG_GROUP_FILE,
		     ConfigurationManager::LAST_ACTIVATED_CONFIG_GROUP_FILE,
		     ConfigurationManager::LAST_ACTIVATED_CONTEXT_GROUP_FILE,
		     ConfigurationManager::LAST_ACTIVATED_BACKBONE_GROUP_FILE,
		     ConfigurationManager::LAST_ACTIVATED_ITERATE_GROUP_FILE,
		     ConfigurationManager::LAST_CONFIGURED_CONFIG_ALIAS_FILE,
		     ConfigurationManager::LAST_ATTEMPTED_CONFIGURE_CONFIG_ALIAS_FILE,
		     ConfigurationManager::LAST_ATTEMPTED_CONFIGURE_CONFIG_GROUP_FILE});
	}
	else
	{
		actions.push_back(action);

		if(action == "Configured")
			fileNames.push_back(ConfigurationManager::LAST_CONFIGURED_CONFIG_GROUP_FILE);
		else if(action == "Started")
			fileNames.push_back(ConfigurationManager::LAST_STARTED_CONFIG_GROUP_FILE);
		else if(action == "ActivatedConfig")
			fileNames.push_back(ConfigurationManager::LAST_ACTIVATED_CONFIG_GROUP_FILE);
		else if(action == "ActivatedContext")
			fileNames.push_back(ConfigurationManager::LAST_ACTIVATED_CONTEXT_GROUP_FILE);
		else if(action == "ActivatedBackbone")
			fileNames.push_back(ConfigurationManager::LAST_ACTIVATED_BACKBONE_GROUP_FILE);
		else if(action == "ActivatedIterator")
			fileNames.push_back(ConfigurationManager::LAST_ACTIVATED_ITERATE_GROUP_FILE);
		else if(action == "ConfiguredAlias")
			fileNames.push_back(ConfigurationManager::LAST_CONFIGURED_CONFIG_ALIAS_FILE);
		else if(action == "AttemptedAlias")
			fileNames.push_back(
			    ConfigurationManager::LAST_ATTEMPTED_CONFIGURE_CONFIG_ALIAS_FILE);
		else if(action == "AttemptedConfig")
			fileNames.push_back(
			    ConfigurationManager::LAST_ATTEMPTED_CONFIGURE_CONFIG_GROUP_FILE);
		else
		{
			__COUT_ERR__ << "Invalid last group action requested." << __E__;
			return SOAPUtilities::makeSOAPMessageReference(
			    "LastConfigGroupResponseFailure");
		}
	}

	std::string groupNames   = "";
	std::string groupKeys    = "";
	std::string groupActions = "";
	std::string groupTimes   = "";
	for(size_t i = 0; i < fileNames.size(); ++i)
	{
		if(i)
		{
			groupNames += ",";
			groupKeys += ",";
			groupActions += ",";
			groupTimes += ",";
		}

		std::string                                          timeString;
		std::pair<std::string /*group name*/, TableGroupKey> theGroup =
		    ConfigurationManager::loadGroupNameAndKey(fileNames[i], timeString);

		groupNames += theGroup.first;
		groupKeys += theGroup.second.toString();
		groupActions += actions[i];
		groupTimes += timeString;
	}
	// fill return parameters
	SOAPParameters retParameters;
	retParameters.addParameter("GroupName", groupNames);  //theGroup.first);
	retParameters.addParameter("GroupKey", groupKeys);    //theGroup.second.toString());
	retParameters.addParameter("GroupAction", groupActions);    //action);
	retParameters.addParameter("GroupActionTime", groupTimes);  //timeString);

	return SOAPUtilities::makeSOAPMessageReference("LastConfigGroupResponse",
	                                               retParameters);
}  //end lastTableGroupRequestHandler()

//==============================================================================
/// getNextRunNumber
///
///	If fsmName is passed, then get next run number for that FSM name
///	Else get next run number for the active FSM name, activeStateMachineName_
///
/// 	Note: the FSM name is sanitized of special characters and used in the filename.
unsigned int GatewaySupervisor::getNextRunNumber(const std::string& fsmNameIn)
{
	std::string runNumberFileName = RUN_NUMBER_PATH + "/";
	std::string fsmName           = fsmNameIn == "" ? activeStateMachineName_ : fsmNameIn;
	// prepend sanitized FSM name
	for(unsigned int i = 0; i < fsmName.size(); ++i)
		if((fsmName[i] >= 'a' && fsmName[i] <= 'z') ||
		   (fsmName[i] >= 'A' && fsmName[i] <= 'Z') ||
		   (fsmName[i] >= '0' && fsmName[i] <= '9'))
			runNumberFileName += fsmName[i];
	runNumberFileName += RUN_NUMBER_FILE_NAME;
	//__COUT__ << "runNumberFileName: " << runNumberFileName << __E__;

	std::ifstream runNumberFile(runNumberFileName.c_str());
	if(!runNumberFile.is_open())
	{
		__COUT__ << "Cannot open file: " << runNumberFileName << __E__;

		__COUT__ << "Creating file and setting Run Number to 1: " << runNumberFileName
		         << __E__;
		FILE* fp = fopen(runNumberFileName.c_str(), "w");
		fprintf(fp, "1");
		fclose(fp);

		runNumberFile.open(runNumberFileName.c_str());
		if(!runNumberFile.is_open())
		{
			__SS__ << "Error. Cannot create file: " << runNumberFileName << __E__;
			__SS_THROW__;
		}
	}
	std::string runNumberString;
	runNumberFile >> runNumberString;
	runNumberFile.close();
	return atoi(runNumberString.c_str());
}  // end getNextRunNumber()

//==============================================================================
void GatewaySupervisor::setNextRunNumber(unsigned int       runNumber,
                                         const std::string& fsmNameIn)
{
	std::string runNumberFileName = RUN_NUMBER_PATH + "/";
	std::string fsmName           = fsmNameIn == "" ? activeStateMachineName_ : fsmNameIn;
	// prepend sanitized FSM name
	for(unsigned int i = 0; i < fsmName.size(); ++i)
		if((fsmName[i] >= 'a' && fsmName[i] <= 'z') ||
		   (fsmName[i] >= 'A' && fsmName[i] <= 'Z') ||
		   (fsmName[i] >= '0' && fsmName[i] <= '9'))
			runNumberFileName += fsmName[i];
	runNumberFileName += RUN_NUMBER_FILE_NAME;
	__COUTTV__(runNumberFileName);

	std::ofstream runNumberFile(runNumberFileName.c_str());
	if(!runNumberFile.is_open())
	{
		__SS__ << "Cannot open file: " << runNumberFileName << __E__;
		__SS_THROW__;
	}
	std::stringstream runNumberStream;
	runNumberStream << runNumber;
	runNumberFile << runNumberStream.str().c_str();
	runNumberFile.close();
}  // end setNextRunNumber()

//==============================================================================
/// getLastLogEntry
///
///	If fsmName is passed, then get last log entry for that FSM name and transition type
///	Else for the active FSM name, activeStateMachineName_
///
/// 	Note: the FSM name is sanitized of special characters and used in the filename.
std::string GatewaySupervisor::getLastLogEntry(const std::string& logType,
                                               const std::string& fsmNameIn /* = "" */)
{
	std::string logEntryFileName = LOG_ENTRY_PATH + "/";
	std::string fsmName          = fsmNameIn == "" ? activeStateMachineName_ : fsmNameIn;

	if(logType == RunControlStateMachine::START_TRANSITION_NAME &&
	   stateMachineStartLogEntry_.find(fsmName) != stateMachineStartLogEntry_.end())
		return stateMachineStartLogEntry_.at(fsmName);
	else if(logType == RunControlStateMachine::CONFIGURE_TRANSITION_NAME &&
	        stateMachineConfigureLogEntry_.find(fsmName) !=
	            stateMachineConfigureLogEntry_.end())
		return stateMachineConfigureLogEntry_.at(fsmName);
	else if(logType == RunControlStateMachine::STOP_TRANSITION_NAME &&
	        stateMachineStopLogEntry_.find(fsmName) != stateMachineStopLogEntry_.end())
		return stateMachineStopLogEntry_.at(fsmName);

	// prepend sanitized FSM name
	for(unsigned int i = 0; i < fsmName.size(); ++i)
		if((fsmName[i] >= 'a' && fsmName[i] <= 'z') ||
		   (fsmName[i] >= 'A' && fsmName[i] <= 'Z') ||
		   (fsmName[i] >= '0' && fsmName[i] <= '9'))
			logEntryFileName += fsmName[i];
	logEntryFileName += "_" + logType + "_" + LOG_ENTRY_FILE_NAME;
	__SUP_COUTTV__(logEntryFileName);

	std::string contents;
	std::FILE*  fp = std::fopen(logEntryFileName.c_str(), "rb");
	if(!fp)
	{
		__SUP_COUTT__ << "Could not open file at " << logEntryFileName
		              << ". Error: " << errno << " - " << strerror(errno) << __E__;
		contents = "";
	}
	else
	{
		std::fseek(fp, 0, SEEK_END);
		contents.resize(std::ftell(fp));
		std::rewind(fp);
		std::fread(&contents[0], 1, contents.size(), fp);
		std::fclose(fp);
	}

	__SUP_COUTTV__(contents);

	if(logType == RunControlStateMachine::START_TRANSITION_NAME)
		stateMachineStartLogEntry_[fsmName] = contents;
	else if(logType == RunControlStateMachine::CONFIGURE_TRANSITION_NAME)
		stateMachineConfigureLogEntry_[fsmName] = contents;
	else if(logType == RunControlStateMachine::STOP_TRANSITION_NAME)
		stateMachineStopLogEntry_[fsmName] = contents;

	return contents;
}  // end getLastLogEntry()

//==============================================================================
/// setLastLogEntry
///
///	If fsmName is passed, then get last log entry for that FSM name and transition type
///	Else for the active FSM name, activeStateMachineName_
///
/// 	Note: the FSM name is sanitized of special characters and used in the filename.
void GatewaySupervisor::setLastLogEntry(const std::string& logType,
                                        const std::string& logEntry,
                                        const std::string& fsmNameIn /* = "" */)
{
	std::string logEntryFileName = LOG_ENTRY_PATH + "/";
	std::string fsmName          = fsmNameIn == "" ? activeStateMachineName_ : fsmNameIn;

	if(logType == RunControlStateMachine::START_TRANSITION_NAME)
		stateMachineStartLogEntry_[fsmName] = logEntry;
	else if(logType == RunControlStateMachine::CONFIGURE_TRANSITION_NAME)
		stateMachineConfigureLogEntry_[fsmName] = logEntry;
	else if(logType == RunControlStateMachine::STOP_TRANSITION_NAME)
		stateMachineStopLogEntry_[fsmName] = logEntry;
	else
	{
		if(logEntry != "")
			__COUT_WARN__ << "Log entry for log type '" << logType
			              << "' not implemented for saving." << __E__;
		return;  //for now,  do not save other types of transitions
	}

	// prepend sanitized FSM name
	for(unsigned int i = 0; i < fsmName.size(); ++i)
		if((fsmName[i] >= 'a' && fsmName[i] <= 'z') ||
		   (fsmName[i] >= 'A' && fsmName[i] <= 'Z') ||
		   (fsmName[i] >= '0' && fsmName[i] <= '9'))
			logEntryFileName += fsmName[i];
	logEntryFileName += "_" + logType + "_" + LOG_ENTRY_FILE_NAME;
	__COUTTV__(logEntryFileName);
	__COUTTV__(logType);
	__COUTTV__(logEntry);

	std::FILE* fp = std::fopen(logEntryFileName.c_str(), "w");
	if(!fp)
	{
		__SUP_SS__ << "Could not open file at " << logEntryFileName
		           << ". Error: " << errno << " - " << strerror(errno) << __E__;
		__SUP_SS_THROW__;
	}
	if(logEntry.size())
		std::fwrite(&logEntry[0], 1, logEntry.size(), fp);
	fclose(fp);
}  // end setLastLogEntry()

//==============================================================================
/// loadRemoteGatewaySettings
///
///	 If editing remoteGatewayApps_, assume already locked remoteGatewayAppsMutex_
///
///	Load from file into vector of Remote Gateways passed by reference.
///	onlyNotFound := load only settings for remoteGateways not currently in vector (keep existing settings, e.g. right before a save)
void GatewaySupervisor::loadRemoteGatewaySettings(
    std::vector<GatewaySupervisor::RemoteGatewayInfo>& remoteGateways,
    bool                                               onlyNotFound /* = false */) const
{
	std::string filepath = std::string(__ENV__("SERVICE_DATA_PATH")) + "/" +
	                       REMOTE_SUBSYSTEM_SETTINGS_FILE_NAME;
	__SUP_COUTV__(filepath);

	std::ifstream settingsFile(filepath.c_str());
	if(!settingsFile.is_open())
	{
		__SUP_COUT__
		    << "Cannot open Remote Gateway settings file (assuming no settings yet!): "
		    << filepath << __E__;

		__SUP_COUT__ << "Creating empty Remote Gateway settings file: " << filepath
		             << __E__;
		FILE* fp = fopen(filepath.c_str(), "w");
		fprintf(fp, "\n");
		fclose(fp);

		settingsFile.open(filepath.c_str());
		if(!settingsFile.is_open())
		{
			__SUP_SS__ << "Error. Cannot create or load Remote Gateway settings file: "
			           << filepath << __E__;
			__SUP_SS_THROW__;
		}
	}

	size_t                   NUM_FIELDS = 4;  //name, fsmMode, included, selected alias
	std::vector<std::string> values;
	float                    formatVersion = 0.0;
	bool                     done          = false;
	do  // Read each line from the file
	{
		size_t i = 0;
		for(i = 0; i < NUM_FIELDS; ++i)
		{
			if(i >= values.size())
				values.push_back("");  //init values vector

			if(!std::getline(settingsFile, values[i]))
			{
				//no more lines left
				if(i)  //at illegal moment mid-record?
				{
					settingsFile.close();
					__SUP_SS__
					    << "Error. Illegal file format in Remote Gateway settings file: "
					    << filepath << __E__;
					__SUP_SS_THROW__;
				}
				//else end is correctly at record boundary
				done = true;
				break;
			}
			__SUP_COUTVS__(TLVL_RemoteFSMRequests, values[i]);

			if(i < 3 &&
			   values[i] == "")  //do not allow blank lines, except for selected alias
			{
				//rewind
				--i;
				continue;
			}
			else if(
			    values[i].find(
			        "Remote Gateway Settings, file format v") !=  //grab format version if present
			    std::string::npos)
			{
				sscanf(values[i].c_str(),
				       "Remote Gateway Settings, file format v%f",
				       &formatVersion);
				__SUP_COUTV__(formatVersion);

				if(formatVersion > 0.5)
					NUM_FIELDS = 4;  //name, fsmMode, included, selected_config_alias

				__SUP_COUTV__(NUM_FIELDS);
				//rewind
				--i;
				continue;
			}

		}  //end record value load
		if(done)
			break;

		//at this point values vector complete for Remote Gateway

		bool found = false;
		for(i = 0; i < remoteGateways.size(); ++i)
			if(values[0] == remoteGateways[i].appInfo.name)
			{
				found = true;
				break;
			}

		if(!found)  //create Remote Gateway (and i will be correctly pointing to back())
		{
			remoteGateways.push_back(GatewaySupervisor::RemoteGatewayInfo());
			remoteGateways[i].appInfo.name = values[0];
		}
		else if(onlyNotFound)
			continue;  //skip modifying current settings

		remoteGateways[i].fsm_mode =
		    values[1] == "Do Not Halt"
		        ? RemoteGatewayInfo::FSM_ModeTypes::DoNotHalt
		        : (values[1] == "Only Configure"
		               ? RemoteGatewayInfo::FSM_ModeTypes::OnlyConfigure
		               : RemoteGatewayInfo::FSM_ModeTypes::Follow_FSM);
		remoteGateways[i].fsm_included = values[2] == "1" ? true : false;
		if(values.size() > 3)
			remoteGateways[i].selected_config_alias = values[3];

		__SUP_COUT__ << "Loaded Remote Gateway '" << remoteGateways[i].appInfo.name
		             << "' ==> " << remoteGateways[i].getFsmMode() << " :"
		             << remoteGateways[i].fsm_included
		             << "configAlias=" << remoteGateways[i].selected_config_alias
		             << __E__;

	} while(1);  //end file read loop

	settingsFile.close();
}  //end loadRemoteGatewaySettings()

//==============================================================================
void GatewaySupervisor::saveRemoteGatewaySettings() const
{
	std::string filepath = std::string(__ENV__("SERVICE_DATA_PATH")) + "/" +
	                       REMOTE_SUBSYSTEM_SETTINGS_FILE_NAME;
	__SUP_COUTV__(filepath);

	std::vector<GatewaySupervisor::RemoteGatewayInfo> remoteGateways = remoteGatewayApps_;

	//load existing settings for remote gateways not present
	loadRemoteGatewaySettings(remoteGateways, true /* onlyNotFound*/);

	std::ofstream settingsFile(filepath.c_str());
	if(!settingsFile.is_open())
	{
		__SUP_SS__ << "Cannot open Remote Gateway settings file: " << filepath << __E__;
		__SUP_SS_THROW__;
	}
	settingsFile << "Remote Gateway Settings, file format v1.0"
	             << __E__;  //save file format version first
	for(size_t i = 0; i < remoteGateways.size(); ++i)
	{
		settingsFile << remoteGateways[i].appInfo.name << __E__;
		settingsFile << remoteGateways[i].getFsmMode() << __E__;
		settingsFile << std::string(remoteGateways[i].fsm_included ? "1" : "0") << __E__;
		settingsFile << remoteGateways[i].selected_config_alias << __E__;
	}

	settingsFile.close();
}  // end saveRemoteGatewaySettings()

//==============================================================================
/// translateURLForRequestOrigin
///		Converts url host:port to a new host:port based on the translation
///			table (to be provided by system admin prior to starting ots in normal mode).
///
///	Note: requestOrigin must be parsed in advance to be http(s)://host:port
///
///	Steps:
/// 	if requestOrigin host matches translation host
/// 		then look for url host+port combo in translation map
/// 		if combo found, then return translation host+port + rest of url
/// 		else return url unchanged
/// 	else return url unchanged
///
///  for example, requestOrigin == "https://gateway1:8443" and url = "http://host:2016/urnblah"
///  requestHost = requestOrigin.substr(0,pos(:))
///  portTranslationMap_.find(requestHost) then 'host matches translation host'
///
///
///  or for example, requestOrigin == "http://host:2015"  and url = "http://host:2016/urnblah"
///
///  or for example, requestOrigin == "http://localhost:2015"  and url = "http://host:2016/urnblah"
///
///	Note!! that the priority matters for host+ports that are substrings of each other,
///	 such that the longer one is replaced first.
///	 For example, if there are host+ports translations for both "host:2016" and "host:201",
///		then "host:2016" should be listed first, so it is replaced first,
///		to avoid partial replacement that would block the full replacement later.
std::string GatewaySupervisor::translateURLForRequestOrigin(
    const std::string&                                        url,
    const std::string&                                        requestOrigin,
    std::map<std::string /* requestOrigin */,
             std::map<std::string /* requestUrlHostPort */,
                      std::string /* translatedHostPort */>>& portTranslationMap)
{
	__COUT__ << "Translating URL '" << url << "' for request origin: " << requestOrigin
	         << __E__;

	// Have: std::map<std::string /* requestOrigin */, std::map<std::string /* requestUrlHostPort */,
	// 		std::string /* translatedHostPort */>>
	// 						portTranslationMap_
	__COUTVS__(2, StringMacros::mapToString(portTranslationMap));

	auto it = portTranslationMap.find(requestOrigin);
	if(it == portTranslationMap.end())
	{
		__COUTT__ << "No port translation found for request origin: " << requestOrigin
		          << __E__;
		return url;
	}

	//extract before get parameters and after
	size_t      getParamPos = url.find("?");
	std::string preUrl      = url.substr(0, getParamPos);
	std::string getParams   = "";
	if(getParamPos != std::string::npos)
	{
		getParams = url.substr(getParamPos + 1);
		__COUTT__ << "Translating encoded get parameters: " << getParams << __E__;

		//for each encoded host port, search and replace all instances in get parameters
		for(auto it3 = it->second.begin(); it3 != it->second.end(); ++it3)
		{
			std::string encodedUrlHostPort = StringMacros::encodeURIComponent(it3->first);
			__COUTS__(2) << "Searching params for encoded url host+port: "
			             << encodedUrlHostPort << __E__;
			size_t pos = 0;
			//Note!! that the priority matters for encoded host+ports that are substrings of each other, so that the longer one is replaced first.
			// For example, if there are encoded host+ports for both "host:2016" and "host:201",
			//	then the encoded "host:2016" should be replaced first to avoid partial replacement that would block the full replacement later.
			while((pos = getParams.find(encodedUrlHostPort, pos)) != std::string::npos)
			{
				__COUTT__ << "Found encoded url host+port: " << encodedUrlHostPort
				          << " at pos " << pos << __E__;
				getParams.replace(pos,
				                  encodedUrlHostPort.size(),
				                  StringMacros::encodeURIComponent(it3->second));
				pos += StringMacros::encodeURIComponent(it3->second).size();

				__COUTT__ << "Replaced with: "
				          << StringMacros::encodeURIComponent(it3->second) << __E__;
			}
		}
		__COUTTV__(getParams);
	}  //end handling get parameters

	size_t pos = 0;  //url host+port end position
	if(url.size() > 7 && url[0] == 'h' && url[1] == 't' && url[2] == 't' &&
	   url[3] == 'p' &&
	   ((url[4] == ':' && url[5] == '/' && url[6] == '/') ||
	    (url[4] == 's' && url[5] == ':' && url[6] == '/' && url[7] == '/')))
		pos = url.find("/", 7);  //after "http(s)://"
	else
		pos = url.find("/", 0);  //from beginning
	std::string urlHostPort = url.substr(0, pos);
	auto        it2         = it->second.find(urlHostPort);
	if(it2 == it->second.end())
	{
		__COUTT__ << "No port translation found for URL host+port '" << urlHostPort
		          << "' for request origin: " << requestOrigin << __E__;
		return preUrl + (getParams.size() ? ("?" + getParams) : "");
	}
	__COUTT__ << "Port translation found: " << urlHostPort << " --> " << it2->second
	          << " for request origin: " << requestOrigin << __E__;

	return it2->second + (pos != std::string::npos ? preUrl.substr(pos) : "") +
	       (getParams.size() ? ("?" + getParams) : "");
}  // end translateURLForRequestOrigin()

//==============================================================================
/// translateRemoteIconStringForRequestOrigin
std::string GatewaySupervisor::translateRemoteIconStringForRequestOrigin(
    const std::string&                                        iconString,
    const std::string&                                        requestOrigin,
    std::map<std::string /* requestOrigin */,
             std::map<std::string /* requestUrlHostPort */,
                      std::string /* translatedHostPort */>>& portTranslationMap)
{
	__COUT__ << "Translating Remote Icon String for request origin: " << requestOrigin
	         << __E__;
	auto parts = StringMacros::getVectorFromString(iconString, {','});

	// comma-separated icon string, 7 fields:
	//				0 - caption 		= text below icon
	//				1 - altText 		= text icon if no image given
	//				2 - uniqueWin 		= if true, only one window is allowed,
	// 										else  multiple instances of window
	//				3 - permissions 	= security level needed to see icon
	//				4 - picfn 			= icon image filename
	//				5 - linkurl 		= url of the window to open
	// 				6 - folderPath 		= folder and subfolder location '/' separated
	//	for example:  State Machine,FSM,1,200,icon-Physics.gif,/WebPath/html/StateMachine.html?fsm_name=OtherRuns0,,Chat,CHAT,1,1,icon-Chat.png,/urn:xdaq-application:lid=250,,Visualizer,VIS,0,10,icon-Visualizer.png,/WebPath/html/Visualization.html?urn=270,,Configure,CFG,0,10,icon-Configure.png,/urn:xdaq-application:lid=281,,Front-ends,CFG,0,15,icon-Configure.png,/WebPath/html/ConfigurationGUI_subset.html?urn=281&subsetBasePath=FEInterfaceTable&groupingFieldList=Status%2CFEInterfacePluginName&recordAlias=Front%2Dends&editableFieldList=%21%2ACommentDescription%2C%21SlowControls%2A,Config Subsets

	std::string result = "";
	for(size_t i = 0; i < parts.size(); i += 7)
	{
		if(TTEST(TLVL_RemoteDesktopIcons))
		{
			__COUTS__(TLVL_RemoteDesktopIcons)
			    << "Translating icon string part: " << parts[i] << "," << parts[i + 1]
			    << "," << parts[i + 2] << "," << parts[i + 3] << "," << parts[i + 4]
			    << "," << parts[i + 5] << "," << parts[i + 6] << __E__;
			__COUTVS__(TLVL_RemoteDesktopIcons, parts[i + 5]);
		}
		std::string translatedLinkURL =
		    translateURLForRequestOrigin(parts[i + 5], requestOrigin, portTranslationMap);
		if(TTEST(TLVL_RemoteDesktopIcons))
		{
			__COUTS__(TLVL_RemoteDesktopIcons)
			    << "Translated icon string part: " << parts[i] << "," << parts[i + 1]
			    << "," << parts[i + 2] << "," << parts[i + 3] << "," << parts[i + 4]
			    << "," << translatedLinkURL << "," << parts[i + 6] << __E__;
			__COUTVS__(TLVL_RemoteDesktopIcons, translatedLinkURL);
		}

		if(i)
			result += ",";  //add separator if not first entry
		result += parts[i] + "," + parts[i + 1] + "," + parts[i + 2] + "," +
		          parts[i + 3] + "," + parts[i + 4] + "," + translatedLinkURL + "," +
		          parts[i + 6];
	}  //end primary translation loop

	return result;
}  // end translateRemoteIconStringForRequestOrigin()

//==============================================================================
/// static function to lookup the XDAQ Application LID
void GatewaySupervisor::handleGetApplicationIdRequest(
    AllSupervisorInfo* allSupervisorInfo,
    cgicc::Cgicc&      cgiIn,
    HttpXmlDocument&   xmlOut,
    std::map<std::string /* requestOrigin */,
             std::map<std::string /* requestUrlHostPort */,
                      std::string /* translatedHostPort */>>*
        portTranslationMap /* = nullptr */)
{
	std::string requestOrigin = StringMacros::decodeURIComponent(
	    CgiDataUtilities::postData(cgiIn, "RequestOrigin"));
	bool doAddressTranslation = false;
	if(portTranslationMap && portTranslationMap->size())
	{
		__COUTTV__(StringMacros::mapToString(*portTranslationMap));
		__COUTTV__(requestOrigin);
		if(portTranslationMap->find(requestOrigin) != portTranslationMap->end())
		{
			__COUTT__
			    << "Doing address translation for application ID request from origin: "
			    << requestOrigin << __E__;
			doAddressTranslation = true;
		}
	}

	std::string classNeedle =
	    StringMacros::decodeURIComponent(CgiDataUtilities::getData(cgiIn, "classNeedle"));
	__COUTV__(classNeedle);

	bool                  found = false;
	std::set<std::string> setOfClasses;
	for(auto it : allSupervisorInfo->getAllSupervisorInfo())
	{
		auto appInfo = it.second;

		setOfClasses.emplace(appInfo.getClass());
		if(classNeedle != appInfo.getClass())
			continue;  // skip non-matches

		found = true;
		xmlOut.addTextElementToData("name",
		                            appInfo.getName());  // get application name
		xmlOut.addTextElementToData(
		    "id", std::to_string(appInfo.getId()));  // get application id
		xmlOut.addTextElementToData("class",
		                            appInfo.getClass());  // get application class

		if(doAddressTranslation)
		{
			__COUTTV__(requestOrigin);
			__COUTTV__(appInfo.getURL());
			std::string translatedURL = translateURLForRequestOrigin(
			    appInfo.getURL(), requestOrigin, *portTranslationMap);
			__COUTTV__(translatedURL);
			xmlOut.addTextElementToData("url",
			                            translatedURL);  // get application url
		}
		else
			xmlOut.addTextElementToData("url",
			                            appInfo.getURL());  // get application url

		xmlOut.addTextElementToData("context",
		                            appInfo.getContextName());  // get context
	}                                                           //end app search loop

	if(!found)
	{
		__SS__ << "Could not find any XDAQ Applications with classname '" << classNeedle
		       << "' - does the app exist in the active Context?" << __E__;
		ss << "\n\nHere are the instantiated app classes:";
		for(auto appClass : setOfClasses)
			ss << "\n\t - " << appClass;
		ss << "\n";
		__SS_THROW__;
	}

}  // end handleGetApplicationIdRequest()

//==============================================================================
bool GatewaySupervisor::handleAddDesktopIconRequest(
    const std::string&                          author,
    cgicc::Cgicc&                               cgiIn,
    HttpXmlDocument&                            xmlOut,
    std::vector<DesktopIconTable::DesktopIcon>* newIcons /* = nullptr*/)
{
	std::string iconCaption =
	    CgiDataUtilities::getData(cgiIn, "iconCaption");  // from GET
	std::string iconAltText =
	    CgiDataUtilities::getData(cgiIn, "iconAltText");  // from GET
	std::string iconFolderPath =
	    CgiDataUtilities::getData(cgiIn, "iconFolderPath");  // from GET
	std::string iconImageURL =
	    CgiDataUtilities::getData(cgiIn, "iconImageURL");  // from GET
	std::string iconWindowURL =
	    CgiDataUtilities::getData(cgiIn, "iconWindowURL");  // from GET
	std::string iconPermissions =
	    CgiDataUtilities::getData(cgiIn, "iconPermissions");  // from GET
	// windowLinkedApp is one of the only fields that needs to be decoded before write into table cells, because the app class name might be here
	std::string windowLinkedApp =
	    CgiDataUtilities::getData(cgiIn, "iconLinkedApp");  // from GET
	unsigned int windowLinkedAppLID =
	    CgiDataUtilities::getDataAsInt(cgiIn, "iconLinkedAppLID");  // from GET
	bool enforceOneWindowInstance =
	    CgiDataUtilities::getData(cgiIn, "iconEnforceOneWindowInstance") == "1"
	        ? true
	        : false;  // from GET

	std::string windowParameters = StringMacros::decodeURIComponent(
	    CgiDataUtilities::postData(cgiIn, "iconParameters"));  // from POST

	__COUTV__(author);
	__COUTV__(iconCaption);
	__COUTV__(iconAltText);
	__COUTV__(iconFolderPath);
	__COUTV__(iconImageURL);
	__COUTV__(iconWindowURL);
	__COUTV__(iconPermissions);
	__COUTV__(windowLinkedApp);
	__COUTV__(windowLinkedAppLID);
	__COUTV__(enforceOneWindowInstance);

	__COUTV__(windowParameters);  // map: CSV list

	ConfigurationManagerRW tmpCfgMgr(author);

	bool success = ConfigurationSupervisorBase::handleAddDesktopIconXML(
	    xmlOut,
	    &tmpCfgMgr,
	    iconCaption,
	    iconAltText,
	    iconFolderPath,
	    iconImageURL,
	    iconWindowURL,
	    iconPermissions,
	    windowLinkedApp /*= ""*/,
	    windowLinkedAppLID /*= 0*/,
	    enforceOneWindowInstance /*= false*/,
	    windowParameters /*= ""*/);

	if(newIcons && success)
	{
		__COUT__ << "Passing new icons back to caller..." << __E__;

		const std::vector<DesktopIconTable::DesktopIcon>& tmpNewIcons =
		    tmpCfgMgr.__GET_CONFIG__(DesktopIconTable)->getAllDesktopIcons();

		newIcons->clear();
		for(const auto& tmpNewIcon : tmpNewIcons)
			newIcons->push_back(tmpNewIcon);
	}

	return success;
}  // end handleAddDesktopIconRequest()

//==============================================================================
xoap::MessageReference GatewaySupervisor::TRACESupervisorRequest(
    xoap::MessageReference message)
{
	return CorePropertySupervisorBase::TRACESupervisorRequest(message);
}  // end TRACESupervisorRequest()
