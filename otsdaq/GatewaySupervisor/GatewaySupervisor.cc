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

#include <sys/stat.h>  // for mkdir
#include <chrono>      // std::chrono::seconds
#include <fstream>
#include <thread>  // std::this_thread::sleep_for

using namespace ots;

#define RUN_NUMBER_PATH std::string(__ENV__("SERVICE_DATA_PATH")) + "/RunNumber/"
#define RUN_NUMBER_FILE_NAME "NextRunNumber.txt"
#define LOG_ENTRY_PATH std::string(__ENV__("SERVICE_DATA_PATH")) + "/FSM_LastLogEntry/"
#define LOG_ENTRY_FILE_NAME "LastLogEntry.txt"
#define FSM_LAST_GROUP_ALIAS_FILE_START std::string("FSMLastGroupAlias-")
#define FSM_USERS_PREFERENCES_FILETYPE "pref"


#define REMOTE_SUBSYSTEM_SETTINGS_FILE_NAME "RemoteSubsystems.txt"

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "GatewaySupervisor"

XDAQ_INSTANTIATOR_IMPL(GatewaySupervisor)

WebUsers GatewaySupervisor::theWebUsers_ = WebUsers();
std::vector<std::shared_ptr<GatewaySupervisor::BroadcastThreadStruct>> GatewaySupervisor::broadcastThreadStructs_;

//==============================================================================
GatewaySupervisor::GatewaySupervisor(xdaq::ApplicationStub* s)
    : xdaq::Application(s)
    , SOAPMessenger(this)
    , RunControlStateMachine("GatewaySupervisor")
    , CorePropertySupervisorBase(this)
    , stateMachineWorkLoopManager_(toolbox::task::bind(this, &GatewaySupervisor::stateMachineThread, "StateMachine"))
    , stateMachineSemaphore_(toolbox::BSem::FULL)
    , activeStateMachineName_("")
    , theIterator_(this)
    , broadcastCommandMessageIndex_(0)
    , broadcastIterationBreakpoint_(-1)  // for standard transitions, ignore the breakpoint
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
	xgi::bind(this, &GatewaySupervisor::stateMachineIterationBreakpoint, "StateMachineIterationBreakpoint");
	xgi::bind(this, &GatewaySupervisor::tooltipRequest, "TooltipRequest");

	xoap::bind(this, &GatewaySupervisor::supervisorCookieCheck, "SupervisorCookieCheck", XDAQ_NS_URI);
	xoap::bind(this, &GatewaySupervisor::supervisorGetActiveUsers, "SupervisorGetActiveUsers", XDAQ_NS_URI);
	xoap::bind(this, &GatewaySupervisor::supervisorSystemMessage, "SupervisorSystemMessage", XDAQ_NS_URI);
	xoap::bind(this, &GatewaySupervisor::supervisorSystemLogbookEntry, "SupervisorSystemLogbookEntry", XDAQ_NS_URI);
	xoap::bind(this, &GatewaySupervisor::supervisorLastTableGroupRequest, "SupervisorLastTableGroupRequest", XDAQ_NS_URI);
	xoap::bind(this, &GatewaySupervisor::TRACESupervisorRequest, "TRACESupervisorRequest", XDAQ_NS_URI);

	init();

}  // end constructor

//==============================================================================
//	TODO: Lore needs to detect program quit through killall or ctrl+c so that Logbook
// entry is made when ots is killed
GatewaySupervisor::~GatewaySupervisor(void)
{
	delete CorePropertySupervisorBase::theConfigurationManager_;
	makeSystemLogEntry("ots shutdown.");
}  // end destructor

//==============================================================================
//For Wizard Supervisor to call
void GatewaySupervisor::indicateOtsAlive(const CorePropertySupervisorBase* properties) { CorePropertySupervisorBase::indicateOtsAlive(properties); }

//==============================================================================
void GatewaySupervisor::init(void)
{
	supervisorGuiHasBeenLoaded_ = false;
	
	//Initialize the variable used by the RunInfo plugin
	conditionID_ = (unsigned int)-1;

	// setting up thread for UDP thread to drive state machine
	{
		bool enableStateChanges = false;
		bool enableLoginVerify = false;
		try
		{
			enableStateChanges = CorePropertySupervisorBase::getSupervisorTableNode().getNode("EnableStateChangesOverUDP").getValue<bool>();
			enableLoginVerify = CorePropertySupervisorBase::getSupervisorTableNode().getNode("EnableAckForStateChangesOverUDP").getValue<bool>();
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
				__COUT_INFO__ << "Enabling this Gateway as source of primary login verification over UDP..." << __E__;

				std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
				loadRemoteGatewaySettings(remoteGatewayApps_);
			}

			// start state changer UDP listener thread
			std::thread([](GatewaySupervisor* s) { GatewaySupervisor::StateChangerWorkLoop(s); }, this).detach();
		}
		else
			__COUT__ << "State changes over UDP are disabled." << __E__;
	}  // end setting up thread for UDP drive of state machine

	// setting up checking of App Status
	{
		bool checkAppStatus = false;
		try
		{
			checkAppStatus = CorePropertySupervisorBase::getSupervisorTableNode().getNode("EnableApplicationStatusMonitoring").getValue<bool>();
		}
		catch(...)
		{
			;
		}  // ignore errors

		if(checkAppStatus)
		{
			__COUT__ << "Enabling App Status checking..." << __E__;
			//
			std::thread([](GatewaySupervisor* s) { GatewaySupervisor::AppStatusWorkLoop(s); }, this).detach();
		}
		else
		{
			__COUT__ << "App Status checking is disabled." << __E__;

			// set all app status to "Not Monitored" so that FSM changes ignore missing app status
			for(const auto& it : allSupervisorInfo_.getAllSupervisorInfo())
			{
				auto appInfo = it.second;
				allSupervisorInfo_.setSupervisorStatus(appInfo, SupervisorInfo::APP_STATUS_NOT_MONITORED, 0 /* progressInteger */, "" /* detail */);
			}
		}

	}  // end checking of Application Status

}  // end init()

//==============================================================================
// AppStatusWorkLoop
//	child thread
void GatewaySupervisor::AppStatusWorkLoop(GatewaySupervisor* theSupervisor)
{
	sleep(5);  // wait for apps to get started

	bool        firstError = true;
	std::string status, progress, detail, appName;
	std::vector<SupervisorInfo::SubappInfo> subapps;
	int         progressInteger;
	bool        oneStatusReqHasFailed = false;
	size_t		loopCount = -1; //first time through loop will be 0

	std::map<std::string /* appName */, bool /* lastStatusGood */> appLastStatusGood;

	std::unique_ptr<TransceiverSocket> remoteGatewaySocket; //use to get remote gateway status
	bool resetRemoteGatewayApps = false;
	bool commandingRemoteGatewayApps = false;
	size_t commandRemoteIdleCount = 0;
	int portForReverseLoginOverUDP = 0; //if 0, then not enabled

	while(1)
	{
		++loopCount;
		sleep(1);

		// workloop procedure
		//	Loop through all Apps and request status
		//	sleep

		oneStatusReqHasFailed = false;
		// __COUT__ << "Just debugging App status checking" << __E__;
		for(const auto& it : theSupervisor->allSupervisorInfo_.getAllSupervisorInfo())
		{
			auto appInfo = it.second;
			appName      = appInfo.getName();
			// __COUT__ << "Getting Status "
			// 		<< " Supervisor instance = '" << appName
			// 		<< "' [LID=" << appInfo.getId() << "] in Context '"
			// 		<< appInfo.getContextName() << "' [URL=" <<
			// 	appInfo.getURL()
			// 		<< "].\n\n";

			// if the application is the gateway supervisor, we do not send a SOAP message
			if(appInfo.isGatewaySupervisor())  // get gateway status
			{
				try
				{
					// send back status and progress parameters
					const std::string& err = theSupervisor->theStateMachine_.getErrorMessage();

					// status = err == "" ? (theSupervisor->theStateMachine_.isInTransition() ? theSupervisor->theStateMachine_.getProvenanceStateName()
					//                                                                        : theSupervisor->theStateMachine_.getCurrentStateName())
					//                    : (theSupervisor->theStateMachine_.getCurrentStateName() == "Paused" ? "Soft-Error:::" : "Failed:::") + err;

					try
					{
						__COUTVS__(39,theSupervisor->theStateMachine_.isInTransition());
						if(theSupervisor->theStateMachine_.isInTransition())
							__COUTVS__(39,theSupervisor->theStateMachine_.getCurrentTransitionName());
						__COUTVS__(39,theSupervisor->theStateMachine_.getProvenanceStateName());
						__COUTVS__(39,theSupervisor->theStateMachine_.getCurrentStateName());
					}
					catch(...)
					{;}
					
					if(err == "")
					{
						if(theSupervisor->theStateMachine_.isInTransition())// || theSupervisor->theProgressBar_.read() < 100)
						{
							// attempt to get transition name, otherwise give provenance state
							try
							{
								status = theSupervisor->theStateMachine_.getCurrentTransitionName();
							}
							catch(...)
							{
								status = theSupervisor->theStateMachine_.getProvenanceStateName();
							}
							progress = theSupervisor->theProgressBar_.readPercentageString();
						}
						else
						{
							status = theSupervisor->theStateMachine_.getCurrentStateName();
							progress = "100"; //if not in transition, then 100
						}
					}
					else
					{
						status = (theSupervisor->theStateMachine_.getCurrentStateName() == "Paused" ? "Soft-Error:::" : "Failed:::") + err;
						progress = theSupervisor->theProgressBar_.readPercentageString();
					}

					__COUTVS__(39,status);
					__COUTVS__(39,progress);

					try
					{
						detail = (theSupervisor->theStateMachine_.isInTransition()
									? theSupervisor->theStateMachine_.getCurrentTransitionName(theSupervisor->stateMachineLastCommandInput_)
									: theSupervisor->getSupervisorUptimeString());
						// make sure broadcast message status is not being updated
						std::lock_guard<std::mutex> lock(theSupervisor->broadcastCommandStatusUpdateMutex_);
						if(detail != "" && theSupervisor->broadcastCommandStatus_ != "")
							detail += " - " + theSupervisor->broadcastCommandStatus_;
					}
					catch(...)
					{
						detail = "";
					}

					std::vector<GatewaySupervisor::RemoteGatewayInfo> remoteApps; //local copy to avoid long mutex lock
					{ //lock for remainder of scope
						std::lock_guard<std::mutex> lock(theSupervisor->remoteGatewayAppsMutex_);		
						remoteApps = theSupervisor->remoteGatewayApps_;

						//check for commands
						if(!commandingRemoteGatewayApps)
						{
							for(const auto& remoteApp : remoteApps)
							{
								if(remoteApp.command != "")
								{
									//latch until all remote apps are stable
									commandingRemoteGatewayApps = true;
									break;
								}
							}
						}	
					}
					__COUTVS__(38,commandingRemoteGatewayApps);

					//Add sub-apps for each Remote Gateway specified as a Remote Desktop Icon
					if((!commandingRemoteGatewayApps && loopCount % 20 == 0) ||
						loopCount == 0) //periodically refresh Remote Gateway list based on icon list
					{					

						// use latest context always from temporary configuration manager,
						//	to get updated icons every time...
						//(so icon changes do no require an ots restart)
						ConfigurationManager                              tmpCfgMgr; // Creating new temporary instance so that constructor will activate latest context, note: not using member CorePropertySupervisorBase::theConfigurationManager_
						const DesktopIconTable*                           iconTable = tmpCfgMgr.__GET_CONFIG__(DesktopIconTable);
						const std::vector<DesktopIconTable::DesktopIcon>& icons     = iconTable->getAllDesktopIcons();
																		
						for(auto& remoteGatewayApp : remoteApps)
							remoteGatewayApp.appInfo.status = ""; //clear status, to be used to remove remote gateways no longer targeted

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
								__COUT__ << "Found remote gateway from icons at " << icon.windowContentURL_ << __E__;
								
								GatewaySupervisor::RemoteGatewayInfo thisInfo;

								std::string remoteURL = icon.windowContentURL_;
								std::string remoteLandingPage = "";
								//remote ? parameters from remoteURL
								if(remoteURL.find('?') != std::string::npos)
								{
									__COUT__ << "Extracting GET ? parameters from remote url." << __E__;
									std::vector<std::string> urlSplit = StringMacros::getVectorFromString(
										remoteURL, {'?'});
									if(urlSplit.size() > 0)
										remoteURL = urlSplit[0]; 
									if(urlSplit.size() > 1)
									{
										//look for 'LandingPage' parameter										
										std::vector<std::string> parameterPairs = StringMacros::getVectorFromString(
												urlSplit[1], {'&'});
										for(const auto& parameterPair : parameterPairs)
										{
											std::vector<std::string> parameterPairSplit = StringMacros::getVectorFromString(
												parameterPair, {'='});
											if(parameterPairSplit.size() == 2)
											{
												__COUT__ << "Found remote URL parameter " << parameterPairSplit[0] << 
													", " << parameterPairSplit[1] << __E__;
												if(parameterPairSplit[0] == "LandingPage")
												{
													remoteLandingPage = StringMacros::decodeURIComponent(parameterPairSplit[1]);
													if(remoteLandingPage.find(icon.folderPath_) != 0)
														remoteLandingPage = icon.folderPath_ + "/" + remoteLandingPage;
													__COUT__ << "Found landing page " << remoteLandingPage << 
														" for " << icon.recordUID_ << __E__;
												}
											}
										}
									}
								} //end remote URL parameter handling

								thisInfo.appInfo.name = icon.recordUID_;							
								thisInfo.appInfo.status = SupervisorInfo::APP_STATUS_UNKNOWN;		
								thisInfo.appInfo.progress = 0;			
								thisInfo.appInfo.detail = "";
								thisInfo.appInfo.url = remoteURL; //icon.windowContentURL_;
								thisInfo.appInfo.class_name = "Remote Gateway";
								thisInfo.appInfo.lastStatusTime = time(0);

								thisInfo.user_data_path_record = icon.alternateText_;
								thisInfo.parentIconFolderPath = icon.folderPath_;		
								thisInfo.landingPage = remoteLandingPage;								

								//replace or add to local copy of supervisor remote gateway list (control info will be protected later, after status update, with final copy to real list)
								bool found = false;
								size_t i = 0;
								for(; i < remoteApps.size(); ++i)
								{	
									if(thisInfo.appInfo.name == remoteApps[i].appInfo.name)
									{
										found = true;

										//overwrite with refreshed info
										remoteApps[i].appInfo = thisInfo.appInfo;
										remoteApps[i].user_data_path_record = thisInfo.user_data_path_record;
										remoteApps[i].parentIconFolderPath = thisInfo.parentIconFolderPath;
										remoteApps[i].landingPage = thisInfo.landingPage;
										break;
									}
								}

								if(!found) //add
									remoteApps.push_back(thisInfo);

								//if possible, get CSV list of potential config_aliases (for dropdown)
								try
								{
									__COUTTV__(remoteApps[i].fsmName);
									remoteApps[i].config_aliases = tmpCfgMgr.getOtherSubsystemConfigAliases(remoteApps[i].user_data_path_record);//getOtherSubsystemFilteredConfigAliases(remoteApps[i].user_data_path_record, remoteApps[i].fsmName);
									__COUTV__(StringMacros::setToString(thisInfo.config_aliases));
									if(remoteApps[i].config_aliases.size()) //initialize selected alias in case it is needed (selected alias will be verified at final copy)
										remoteApps[i].selected_config_alias = *(remoteApps[i].config_aliases.begin());
								}
								catch(const std::runtime_error& e)
								{
									__SS__ << "Failed to retrieve the list of Configuration Aliases for Remote Subsystem '" <<
										remoteApps[i].appInfo.name << ".' Remote Subsystems are specified through their Desktop Icon record. "
										"Please specify a valid User Data Path record as the Desktop Icon AlternateText field, targeting a UID in the SubsystemUserDataPathsTable." << __E__;
									ss << "\n\nHere was the error: " << e.what() << __E__;
									__COUT__ << ss.str();
									remoteApps[i].error = ss.str();
									
									//give feedback immediately to user!!
									{
										__COUTV__(remoteApps[i].error);
										//lock for remainder of scope
										std::lock_guard<std::mutex> lock(theSupervisor->remoteGatewayAppsMutex_);							
										for(size_t i = 0; i < theSupervisor->remoteGatewayApps_.size(); ++i)
											if(remoteApps[i].appInfo.name == theSupervisor->remoteGatewayApps_[i].appInfo.name)
											{
												theSupervisor->remoteGatewayApps_[i].error = remoteApps[i].error; 							
												break;
											}
									}
								}			


							} //end remote icon handling

						} //end icon loop

						//clean up stale remoteGatewayApps with blank status
						bool remoteAppsExist = false;
						for(size_t i = 0; i < remoteApps.size(); ++i)
						{	
							if(remoteApps[i].appInfo.status == "")
							{
								//rewind and erase
								remoteApps.erase(remoteApps.begin() + i);
								--i;
							}
						}
						remoteAppsExist = remoteApps.size();

						if(remoteAppsExist && !remoteGatewaySocket) //instantiate socket first time there are remote apps
						{
							__COUT_INFO__ << "Instantiating Remote Gateway App Status Socket!" << __E__;
							ConfigurationTree configLinkNode = theSupervisor->CorePropertySupervisorBase::getSupervisorTableNode();
							std::string ipAddressForStateChangesOverUDP = configLinkNode.getNode("IPAddressForStateChangesOverUDP").getValue<std::string>();
							__COUTTV__(ipAddressForStateChangesOverUDP);
							
							//check if allowing reverse login verification from remote Gateways to this Gateway							
							if(theSupervisor->theWebUsers_.getSecurity() == WebUsers::SECURITY_TYPE_DIGEST_ACCESS)
							{
								bool enableLoginVerify = configLinkNode.getNode("EnableAckForStateChangesOverUDP").getValue<bool>();

								if(enableLoginVerify)
								{
									portForReverseLoginOverUDP = configLinkNode.getNode("PortForStateChangesOverUDP").getValue<int>();
									if(portForReverseLoginOverUDP)
										__COUT_INFO__ << "Enabling reverse login verification for Remote Gateways at " <<
											ipAddressForStateChangesOverUDP << ":" << 
											portForReverseLoginOverUDP << __E__;
								}
								else
								{
									__COUT_WARN__ << "Remote login verification at this Gateway, for other Remote Gateways, is not enabled unless the GatewaySupervisor has 'EnableStateChangesOverUDP' enabled." << __E__;
								}
							}
							remoteGatewaySocket = std::make_unique<TransceiverSocket>(ipAddressForStateChangesOverUDP);
							remoteGatewaySocket->initialize();
						}

					} //end periodic Remote Gateway refresh

					//if possible, get remote icon list for desktop from each remote app
					if(resetRemoteGatewayApps)
					{
						__COUT_TYPE__(TLVL_DEBUG+35) << __COUT_HDR__ << "Attempting to get Remote Desktop Icons..." << __E__; 
						
						for(auto& remoteGatewayApp : remoteApps)
						{
							if(remoteGatewayApp.command != "") continue; //skip if command to be sent

					
							//clear any previous icon error
							if(remoteGatewayApp.error.find("desktop icons") != std::string::npos)
							{
								__COUTV__(remoteGatewayApp.error);
								//lock for remainder of scope
								std::lock_guard<std::mutex> lock(theSupervisor->remoteGatewayAppsMutex_);							
								for(size_t i = 0; i < theSupervisor->remoteGatewayApps_.size(); ++i)
									if(remoteGatewayApp.appInfo.name == theSupervisor->remoteGatewayApps_[i].appInfo.name)
									{
										theSupervisor->remoteGatewayApps_[i].error = ""; 		
										__COUTV__(theSupervisor->remoteGatewayApps_[i].error);											
										break;
									}
							}

							GatewaySupervisor::GetRemoteGatewayIcons(remoteGatewayApp, remoteGatewaySocket);
							if(remoteGatewayApp.error != "")//give feedback immediately to user!!
							{
								__COUTV__(remoteGatewayApp.error);
								//lock for remainder of scope
								std::lock_guard<std::mutex> lock(theSupervisor->remoteGatewayAppsMutex_);							
								for(size_t i = 0; i < theSupervisor->remoteGatewayApps_.size(); ++i)
									if(remoteGatewayApp.appInfo.name == theSupervisor->remoteGatewayApps_[i].appInfo.name)
									{
										theSupervisor->remoteGatewayApps_[i].error = remoteGatewayApp.error; 					
										break;
									}
							}
						}

					} //end remote desktop icon gathering

					//for each remote gateway, request app status with "GetRemoteAppStatus"	
					if(loopCount % 3 == 0 || resetRemoteGatewayApps || //a little less frequently
						commandingRemoteGatewayApps)
					{
						if(theSupervisor->remoteGatewayApps_.size()) __COUTVS__(38,theSupervisor->remoteGatewayApps_[0].error);

						//check for commands first
						bool commandSent = false;

						for(auto& remoteGatewayApp : remoteApps)	
							if(remoteGatewayApp.command != "")
							{
								GatewaySupervisor::SendRemoteGatewayCommand(remoteGatewayApp, remoteGatewaySocket);
								if(remoteGatewayApp.error == "")
								{
									remoteGatewayApp.ignoreStatusCount = 0; //if non-zero, do not ask for status
									commandSent = true;
								}
								else //give feedback immediately to user!!
								{
									__COUTV__(remoteGatewayApp.error);
									//lock for remainder of scope
									std::lock_guard<std::mutex> lock(theSupervisor->remoteGatewayAppsMutex_);							
									for(size_t i = 0; i < theSupervisor->remoteGatewayApps_.size(); ++i)
										if(remoteGatewayApp.appInfo.name == theSupervisor->remoteGatewayApps_[i].appInfo.name)
										{
											theSupervisor->remoteGatewayApps_[i].error = remoteGatewayApp.error; 										
											break;
										}
								}
							}

						if(commandSent)
						{
							commandRemoteIdleCount = 0; //reset
							sleep(1); //gives some time for command to sink in
						}
						
						if(theSupervisor->remoteGatewayApps_.size()) __COUTVS__(38,theSupervisor->remoteGatewayApps_[0].error);

						//then get status
						bool allAppsAreIdle = true;
						bool allApssAreUnknown = true;
						for(auto& remoteGatewayApp : remoteApps)		
						{
							GatewaySupervisor::CheckRemoteGatewayStatus(remoteGatewayApp, remoteGatewaySocket, portForReverseLoginOverUDP);
							if(remoteGatewayApp.appInfo.status != SupervisorInfo::APP_STATUS_UNKNOWN)
							{
								allApssAreUnknown = false;
								if(!appLastStatusGood[remoteGatewayApp.appInfo.url + remoteGatewayApp.appInfo.name])
								{
									__COUT_INFO__ << "First good status from "
											<< " Remote subapp = '" << remoteGatewayApp.appInfo.name 
											<< "' [URL=" << remoteGatewayApp.appInfo.url << "].\n\n";							
								}
								appLastStatusGood[remoteGatewayApp.appInfo.url + remoteGatewayApp.appInfo.name] = true;

								if(!(remoteGatewayApp.appInfo.progress == 0 ||  //if !(idle)
								 	remoteGatewayApp.appInfo.progress == 100 ||
									remoteGatewayApp.appInfo.status.find("Error") != std::string::npos || //	case "Failed", "Error", "Soft-Error"
									remoteGatewayApp.appInfo.status.find("Fail") != std::string::npos))
								{
									__COUT__ << remoteGatewayApp.appInfo.name << " not idle: " << 
										remoteGatewayApp.appInfo.status << " progress: " << remoteGatewayApp.appInfo.progress << __E__;
									allAppsAreIdle = false;
								}
							}
							else //skip absent subsystems for a while
								remoteGatewayApp.ignoreStatusCount = 3; //if non-zero, do not ask for status
						} //end remote app status update loop

						if(allApssAreUnknown) //then remove ignore status, and give user feedback faster
						{
							for(auto& remoteGatewayApp : remoteApps)
								remoteGatewayApp.ignoreStatusCount = 0; //if non-zero, do not ask for status
						}

						if(commandingRemoteGatewayApps && allAppsAreIdle)
						{
							++commandRemoteIdleCount;
							if(commandRemoteIdleCount >= 2)
							{
								__COUTT__ << "Back to idle statusing" << __E__;
								commandingRemoteGatewayApps = false;
							}
						}

						__COUT_TYPE__(TLVL_DEBUG+38) << __COUT_HDR__ << "commandRemoteIdleCount " << commandRemoteIdleCount << " " << allAppsAreIdle << " " << commandingRemoteGatewayApps << __E__;

						if(theSupervisor->remoteGatewayApps_.size()) __COUTVS__(37,theSupervisor->remoteGatewayApps_[0].error);

						//replace info in supervisor remote gateway list
						{
							//lock for remainder of scope
							std::lock_guard<std::mutex> lock(theSupervisor->remoteGatewayAppsMutex_);							
							for(size_t i = 0; !commandingRemoteGatewayApps && i < theSupervisor->remoteGatewayApps_.size(); ++i)
							{
								__COUTV__(theSupervisor->remoteGatewayApps_[i].command);
								if(theSupervisor->remoteGatewayApps_[i].command == "") //make sure not mid-command
									theSupervisor->remoteGatewayApps_[i].appInfo.status = ""; //clear status as indicator to be erased
							}
								

							for(auto& remoteGatewayApp : remoteApps)	
							{
								bool found = false;
								for(size_t i = 0; i < theSupervisor->remoteGatewayApps_.size(); ++i)
								{	
									if(remoteGatewayApp.appInfo.name == theSupervisor->remoteGatewayApps_[i].appInfo.name)
									{
										found = true;
										//copy over updated status (but not control info, which may be have been changed while mutex was dropped)		
										
										if(remoteGatewayApp.command == "") //if there is action on command, then error is being set (request()) or cleared (send) somewhere else
											theSupervisor->remoteGatewayApps_[i].error = remoteGatewayApp.error;	

										theSupervisor->remoteGatewayApps_[i].ignoreStatusCount = remoteGatewayApp.ignoreStatusCount;	
										theSupervisor->remoteGatewayApps_[i].consoleErrCount = remoteGatewayApp.consoleErrCount;	
										theSupervisor->remoteGatewayApps_[i].consoleWarnCount = remoteGatewayApp.consoleWarnCount;	

										theSupervisor->remoteGatewayApps_[i].config_dump = remoteGatewayApp.config_dump;

										theSupervisor->remoteGatewayApps_[i].user_data_path_record = remoteGatewayApp.user_data_path_record;
										theSupervisor->remoteGatewayApps_[i].iconString = remoteGatewayApp.iconString;	
										theSupervisor->remoteGatewayApps_[i].parentIconFolderPath = remoteGatewayApp.parentIconFolderPath;	
										theSupervisor->remoteGatewayApps_[i].landingPage = remoteGatewayApp.landingPage;	
										
										//fix config_aliases and selected_config_alias
										theSupervisor->remoteGatewayApps_[i].config_aliases = remoteGatewayApp.config_aliases;										
										//if invalid selected_config_alias, renitialize for user
										if(remoteGatewayApp.config_aliases.find(theSupervisor->remoteGatewayApps_[i].selected_config_alias) == remoteGatewayApp.config_aliases.end())
											theSupervisor->remoteGatewayApps_[i].selected_config_alias = remoteGatewayApp.selected_config_alias;

										__COUTTV__(theSupervisor->remoteGatewayApps_[i].selected_config_alias);

										__COUTT__ << "Command: " << remoteGatewayApp.command << 
											" Command-old: " << theSupervisor->remoteGatewayApps_[i].command <<
											" Status: " << remoteGatewayApp.appInfo.status <<
											" Status_old: " << theSupervisor->remoteGatewayApps_[i].appInfo.status << 
											" Error: " << remoteGatewayApp.error << 
											" progress: " << remoteGatewayApp.appInfo.progress << __E__;

										if(remoteGatewayApp.command == "Sent") //apply command clear	
											theSupervisor->remoteGatewayApps_[i].command = ""; 

										if(theSupervisor->remoteGatewayApps_[i].command != "" || (commandingRemoteGatewayApps && 
												theSupervisor->remoteGatewayApps_[i].appInfo.status.find("Launching") == 0 &&
												remoteGatewayApp.appInfo.progress == 100)) //dont trust done progress while still 'commanding'
											__COUT__ << "Ignoring '" << remoteGatewayApp.appInfo.name << "' assumed stale status: " << remoteGatewayApp.appInfo.status << __E__;
										else										
											theSupervisor->remoteGatewayApps_[i].appInfo = remoteGatewayApp.appInfo;
										
										theSupervisor->remoteGatewayApps_[i].subapps = remoteGatewayApp.subapps;
										break;
									}
								}
								if(!found) //add
									theSupervisor->remoteGatewayApps_.push_back(remoteGatewayApp);
							}

							//cleanup unused remoteGatewayApps_
							for(size_t i = 0; i < theSupervisor->remoteGatewayApps_.size(); ++i)
							{	
								if(theSupervisor->remoteGatewayApps_[i].appInfo.status == "")
								{
									//rewind and erase
									theSupervisor->remoteGatewayApps_.erase(theSupervisor->remoteGatewayApps_.begin() + i);
									--i;
								}
							}							
						}

						if(theSupervisor->remoteGatewayApps_.size()) __COUTVS__(38,theSupervisor->remoteGatewayApps_[0].error);
					} //end remote app status update

					//copy to subapps for display of primary Gateway
					{
						std::lock_guard<std::mutex> lock(theSupervisor->remoteGatewayAppsMutex_);	
						for(const auto& remoteGatewayApp : theSupervisor->remoteGatewayApps_)
							subapps.push_back(remoteGatewayApp.appInfo);			
					}
					
					resetRemoteGatewayApps = false; //reset
				}
				catch(const std::runtime_error& e)
				{
					status                = SupervisorInfo::APP_STATUS_UNKNOWN;
					progress              = "0";
					detail                = "SOAP Message Error";
					oneStatusReqHasFailed = true;
					
					__COUT__ << "Failed getting status from Gateway"
								<< " Supervisor instance = '" << appName << "' [LID=" << appInfo.getId() << "] in Context '" << appInfo.getContextName()
								<< "' [URL=" << appInfo.getURL() << "].\n\n";					
					__COUT_WARN__ << "Failed to retrieve Gateway Supervisor status. Here is the error: " << e.what() << __E__;					
				}
				catch(...)
				{
					status                = SupervisorInfo::APP_STATUS_UNKNOWN;
					progress              = "0";
					detail                = "Unknown SOAP Message Error";
					oneStatusReqHasFailed = true;
					
					__COUT__ << "Failed getting status from Gateway "
								<< " Supervisor instance = '" << appName << "' [LID=" << appInfo.getId() << "] in Context '" << appInfo.getContextName()
								<< "' [URL=" << appInfo.getURL() << "].\n\n";
					__COUT_WARN__ << "Failed to retrieve Gateway Supervisor status to unknown error." << __E__;					
				}

			}
			else  // get non-gateway status
			{
				// pass the application as a parameter to tempMessage
				SOAPParameters appPointer;
				appPointer.addParameter("ApplicationPointer");

				xoap::MessageReference tempMessage = SOAPUtilities::makeSOAPMessageReference("ApplicationStatusRequest");
				// print tempMessage
				//				__COUT__ << "tempMessage... " <<
				// SOAPUtilities::translate(tempMessage)
				//				         << std::endl;

				try
				{
					xoap::MessageReference statusMessage = theSupervisor->sendWithSOAPReply(appInfo.getDescriptor(), tempMessage);

					// if("ContextARTDAQ" == appInfo.getContextName() )
					// 	__COUT__ << " Supervisor instance = '" << appName
					// 			<< "' [LID=" << appInfo.getId() << "] in Context '"
					// 			<< appInfo.getContextName() << " statusMessage... "
					// 			<<
					// 			SOAPUtilities::translate(statusMessage)
					// 		<<  std::endl;
					// else
					// 	__COUT__ << " Supervisor instance = '" << appName
					// 			<< "' [LID=" << appInfo.getId() << "] in Context '"
					// 			<< appInfo.getContextName() << " statusMessage... "
					// 			<<
					// 			SOAPUtilities::translate(statusMessage)
					// 		<<  std::endl;

					SOAPParameters parameters;
					parameters.addParameter("Status");
					parameters.addParameter("Progress");
					parameters.addParameter("Detail");
					parameters.addParameter("Subapps");
					SOAPUtilities::receive(statusMessage, parameters);

					status = parameters.getValue("Status");
					if(status.empty())
						status = SupervisorInfo::APP_STATUS_UNKNOWN;

					progress = parameters.getValue("Progress");
					if(progress.empty())
						progress = "100";

					// if("ContextARTDAQ" == appInfo.getContextName() )
					// 	__COUTV__(progress);

					detail = parameters.getValue("Detail");
					if(appInfo.isTypeConsoleSupervisor())
					{
						//parse detail 


						//Note: do not printout detail, because custom counts will fire recursively
						//__COUTTV__(detail);

						//Console Supervisor status detatil format is (from otsdaq-utilities/otsdaq-utilities/Console/ConsoleSupervisor.cc:1722):
						//	uptime, Err count, Warn count, Last Error msg, Last Warn msg
						std::vector<std::string> parseDetail = StringMacros::getVectorFromString(detail,{','});

						std::lock_guard<std::mutex> lock(theSupervisor->systemStatusMutex_); //lock for rest of scope
						if(parseDetail.size() > 1)
							theSupervisor->systemConsoleErrCount_ = atoi(parseDetail[1].substr(parseDetail[1].find(':')+1).c_str());
						if(parseDetail.size() > 2)
							theSupervisor->systemConsoleWarnCount_ = atoi(parseDetail[2].substr(parseDetail[2].find(':')+1).c_str());
						__COUTVS__(36,theSupervisor->systemConsoleErrCount_);
						__COUTVS__(36,theSupervisor->systemConsoleWarnCount_);
						if(parseDetail.size() > 3) //e.g. Last Err (Mon Sep 30 14:38:20 2024 CDT): Remote%20lo
						{
							size_t closeTimePos = parseDetail[3].find(')');
							theSupervisor->lastConsoleErr_ = parseDetail[3].substr(closeTimePos+2);
							size_t openTimePos = parseDetail[3].find('(');
							theSupervisor->lastConsoleErrTime_ = parseDetail[3].substr(openTimePos,closeTimePos-openTimePos+1);
							__COUTVS__(36,theSupervisor->lastConsoleErrTime_);
						}
						if(parseDetail.size() > 4) //e.g. Last Warn (Mon Sep 30 14:38:20 2024 CDT): Remote%20lo
						{
							size_t closeTimePos = parseDetail[4].find(')');
							theSupervisor->lastConsoleWarn_ = parseDetail[4].substr(closeTimePos+2);
							size_t openTimePos = parseDetail[4].find('(');
							theSupervisor->lastConsoleWarnTime_ = parseDetail[4].substr(openTimePos,closeTimePos-openTimePos+1);
							__COUTVS__(36,theSupervisor->lastConsoleWarnTime_);
						}
						if(parseDetail.size() > 5) //e.g. Last Info (Mon Sep 30 14:38:20 2024 CDT): Remote%20lo
						{
							size_t closeTimePos = parseDetail[5].find(')');
							theSupervisor->lastConsoleInfo_ = parseDetail[5].substr(closeTimePos+2);
							size_t openTimePos = parseDetail[5].find('(');
							theSupervisor->lastConsoleInfoTime_ = parseDetail[5].substr(openTimePos,closeTimePos-openTimePos+1);
							__COUTVS__(36,theSupervisor->lastConsoleInfoTime_);
						}
						if(parseDetail.size() > 6)
							theSupervisor->systemConsoleInfoCount_ = atoi(parseDetail[6].substr(parseDetail[6].find(':')+1).c_str());
						
						if(parseDetail.size() > 7) //e.g. First Err (Mon Sep 30 14:38:20 2024 CDT): Remote%20lo
						{
							size_t closeTimePos = parseDetail[7].find(')');
							theSupervisor->firstConsoleErr_ = parseDetail[7].substr(closeTimePos+2);
							size_t openTimePos = parseDetail[7].find('(');
							theSupervisor->firstConsoleErrTime_ = parseDetail[7].substr(openTimePos,closeTimePos-openTimePos+1);
							__COUTVS__(36,theSupervisor->firstConsoleErrTime_);
						}
						if(parseDetail.size() > 8) //e.g. First Warn (Mon Sep 30 14:38:20 2024 CDT): Remote%20lo
						{
							size_t closeTimePos = parseDetail[8].find(')');
							theSupervisor->firstConsoleWarn_ = parseDetail[8].substr(closeTimePos+2);
							size_t openTimePos = parseDetail[8].find('(');
							theSupervisor->firstConsoleWarnTime_ = parseDetail[8].substr(openTimePos,closeTimePos-openTimePos+1);
							__COUTVS__(36,theSupervisor->firstConsoleWarnTime_);
						}
						if(parseDetail.size() > 9) //e.g. First Info (Mon Sep 30 14:38:20 2024 CDT): Remote%20lo
						{
							size_t closeTimePos = parseDetail[9].find(')');
							theSupervisor->firstConsoleInfo_ = parseDetail[9].substr(closeTimePos+2);
							size_t openTimePos = parseDetail[9].find('(');
							theSupervisor->firstConsoleInfoTime_ = parseDetail[9].substr(openTimePos,closeTimePos-openTimePos+1);
							__COUTVS__(36,theSupervisor->firstConsoleInfoTime_);
						}
					}

					subapps = SupervisorInfo::deserializeSubappInfos(parameters.getValue("Subapps"));

					if(!appLastStatusGood[appName])
					{
						__COUT_INFO__ << "First good status from "
						         << " Supervisor instance = '" << appName << "' [LID=" << appInfo.getId() << "] in Context '" << appInfo.getContextName()
						         << "' [URL=" << appInfo.getURL() << "].\n\n";
						__COUTTV__(SOAPUtilities::translate(tempMessage));
					}
					appLastStatusGood[appName] = true;
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
						__COUT__ << "Failed getting status from "
						         << " Supervisor instance = '" << appName << "' [LID=" << appInfo.getId() << "] in Context '" << appInfo.getContextName()
						         << "' [URL=" << appInfo.getURL() << "].\n\n";
						__COUTTV__(SOAPUtilities::translate(tempMessage));
						__COUT_WARN__ << "Failed to send getStatus SOAP Message - will suppress repeat errors: " << e.what() << __E__;
					}  // else quiet repeat error messages
					else  //check if should throw state machine error
					{						
						std::lock_guard<std::mutex> lock(theSupervisor->stateMachineAccessMutex_);

						std::string currentState  = theSupervisor->theStateMachine_.getCurrentStateName();
						if(currentState != RunControlStateMachine::FAILED_STATE_NAME && 
							currentState != RunControlStateMachine::HALTED_STATE_NAME && 
							currentState != RunControlStateMachine::INITIAL_STATE_NAME)
						{							
							__SS__ << "\nDid a supervisor crash? Failed getting status from "
						         << " Supervisor instance = '" << appName << "' [LID=" << appInfo.getId() << "] in Context '" << appInfo.getContextName()
						         << "' [URL=" << appInfo.getURL() << "]."
								<< __E__;
							__COUT_ERR__ << "\n" << ss.str();

							theSupervisor->theStateMachine_.setErrorMessage(ss.str());
							try
							{
								theSupervisor->runControlMessageHandler(SOAPUtilities::makeSOAPMessageReference(
									RunControlStateMachine::ERROR_TRANSITION_NAME));
							}
							catch(...) {} //ignore any errors
							
							break; //only send one Error, then restart status loop
						}
					}
					appLastStatusGood[appName] = false;
				}
				catch(...)
				{
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
						         << " Supervisor instance = '" << appName << "' [LID=" << appInfo.getId() << "] in Context '" << appInfo.getContextName()
						         << "' [URL=" << appInfo.getURL() << "].\n\n";
						__COUTV__(SOAPUtilities::translate(tempMessage));
						__COUT_WARN__ << "Failed to send getStatus SOAP Message due to unknown error. Will suppress repeat errors." << __E__;
					}  // else quiet repeat error messages
					else  //check if should throw state machine error
					{						
						std::lock_guard<std::mutex> lock(theSupervisor->stateMachineAccessMutex_);

						std::string currentState  = theSupervisor->theStateMachine_.getCurrentStateName();
						if(currentState != RunControlStateMachine::FAILED_STATE_NAME && 
							currentState != RunControlStateMachine::HALTED_STATE_NAME && 
							currentState != RunControlStateMachine::INITIAL_STATE_NAME)
						{							
							__SS__ << "\nDid a supervisor crash? Failed getting Status "
						         << " Supervisor instance = '" << appName << "' [LID=" << appInfo.getId() << "] in Context '" << appInfo.getContextName()
						         << "' [URL=" << appInfo.getURL() << "]."
								<< __E__;
							__COUT_ERR__ << "\n" << ss.str();

							theSupervisor->theStateMachine_.setErrorMessage(ss.str());
							try
							{
								theSupervisor->runControlMessageHandler(SOAPUtilities::makeSOAPMessageReference(
									RunControlStateMachine::ERROR_TRANSITION_NAME));
							}
							catch(...) {} //ignore any errors

							break; //only send one Error, then restart status loop
						}
					}
					appLastStatusGood[appName] = false;
				}
			}  // end with non-gateway status request handling

			//					__COUTV__(status);
			//					__COUTV__(progress);

			// set status and progress
			// convert the progress string into an integer in order to call
			// appInfo.setProgress() function
			std::istringstream ssProgress(progress);
			ssProgress >> progressInteger;

			// if("ContextARTDAQ" == appInfo.getContextName() )
			// 	__COUTV__(progressInteger);

			theSupervisor->allSupervisorInfo_.setSupervisorStatus(appInfo, status, progressInteger, detail, subapps);

		}  // end of app loop
				
		if(oneStatusReqHasFailed)
		{
			__COUTT__ << "oneStatusReqHasFailed" << __E__;
			sleep(5);  // sleep to not overwhelm server with errors
		}

	} // end of infinite status checking loop
}  // end AppStatusWorkLoop()

//==============================================================================
// SendRemoteGatewayCommand
//	static function
void GatewaySupervisor::GetRemoteGatewayIcons(GatewaySupervisor::RemoteGatewayInfo& remoteGatewayApp, 
	const std::unique_ptr<TransceiverSocket>& /* not transferring ownership */ remoteGatewaySocket)
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

	__COUT__ << "Sending remote gateway command '" << command << "' to target '" <<
		remoteGatewayApp.appInfo.name << "' at url: " << remoteGatewayApp.appInfo.url << __E__;

	try
	{
		std::vector<std::string> parsedFields = StringMacros::getVectorFromString(remoteGatewayApp.appInfo.url,{':'});
		__COUTTV__(StringMacros::vectorToString(parsedFields));
		__COUTV__(command);

		Socket      gatewayRemoteSocket(parsedFields[1],atoi(parsedFields[2].c_str()));		
		std::string remoteIconString = remoteGatewaySocket->sendAndReceive(gatewayRemoteSocket, command, 10 /*timeoutSeconds*/);
		__COUTV__(remoteIconString);

		bool firstIcon = true;

		//now have remote icon string, append icons to list
		std::vector<std::string> remoteIconsCSV = StringMacros::getVectorFromString(remoteIconString, {','});
		const size_t numOfIconFields = 7;
		for(size_t i = 0; i+numOfIconFields < remoteIconsCSV.size(); i += numOfIconFields)
		{
			if(firstIcon)
				firstIcon = false;
			else
				iconString += ",";					

			__COUTV__(remoteIconsCSV[i+0]);
			if(remoteGatewayApp.parentIconFolderPath == "")//icon.folderPath_ == "") //if not in folder, distinguish remote icon somehow
				iconString += remoteGatewayApp.user_data_path_record //icon.alternateText_ 
					+ " " + remoteIconsCSV[i+0]; //icon.caption_;
			else
				iconString += remoteIconsCSV[i+0]; //icon.caption_;
			iconString += "," + remoteIconsCSV[i+1]; //icon.alternateText_;
			iconString += "," + remoteIconsCSV[i+2]; //std::string(icon.enforceOneWindowInstance_ ? "1" : "0");
			iconString += "," + std::string("1");  // set permission to 1 so the
												// desktop shows every icon that the
												// server allows (i.e., trust server
												// security, ignore client security)
			iconString += "," + remoteIconsCSV[i+4]; //icon.imageURL_;
			iconString += "," + remoteIconsCSV[i+5]; //icon.windowContentURL_;
			
			iconString += "," + remoteGatewayApp.parentIconFolderPath //icon.folderPath_
				+ "/" + remoteIconsCSV[i+6];
			
		} //end append remote icons

	}  //end SendRemoteGatewayCommand()
	catch(const std::runtime_error& e)
	{
		__SS__ << "Failure gathering Remote Gateway desktop icons with command '" << command
			<< "' from target '" << remoteGatewayApp.appInfo.name << "' at url: " << remoteGatewayApp.appInfo.url << " due to error: " << e.what() << __E__;
		__COUT_ERR__ << ss.str();	
		remoteGatewayApp.error = ss.str();
		return;
	}  //end SendRemoteGatewayCommand() catch

	__COUTV__(iconString);
	remoteGatewayApp.iconString = iconString;
}  //end GetRemoteGatewayIcons()

//==============================================================================
// SendRemoteGatewayCommand
//	static function
//		Format is FiniteStateMachineName,Command,Parameter(s)
void GatewaySupervisor::SendRemoteGatewayCommand(GatewaySupervisor::RemoteGatewayInfo& remoteGatewayApp, 
	const std::unique_ptr<TransceiverSocket>& /* not transferring ownership */ remoteGatewaySocket)
{
	__COUT__ << "Sending remote gateway command '" << remoteGatewayApp.command << "' to target '" <<
		remoteGatewayApp.appInfo.name << "' at url: " << remoteGatewayApp.appInfo.url << __E__;

	std::string tmpCommand = remoteGatewayApp.command;	
	try
	{
		std::string command;

		//for non-FSM commands, do not use fsmName
		if(remoteGatewayApp.command == "ResetConsoleCounts")
			command = "ResetConsoleCounts";
		else
			command = remoteGatewayApp.fsmName + "," + remoteGatewayApp.command;
					
		remoteGatewayApp.command = "Sent"; //Mark that send is being attempted

		std::vector<std::string> parsedFields = StringMacros::getVectorFromString(remoteGatewayApp.appInfo.url,{':'});
		__COUTTV__(StringMacros::vectorToString(parsedFields));
		__COUTV__(command);

		Socket      gatewayRemoteSocket(parsedFields[1],atoi(parsedFields[2].c_str()));		
		std::string commandResponseString = remoteGatewaySocket->sendAndReceive(gatewayRemoteSocket, command, 10 /*timeoutSeconds*/);
		__COUTV__(commandResponseString);

		if(commandResponseString.find("Done") != 0) //then error
		{
			__SS__ << "Unsuccessful response received from Remote Gateway '" << remoteGatewayApp.appInfo.name +
				"' - here was the response: " << commandResponseString << __E__;
			__SS_THROW__;
		}

		if(commandResponseString.size() > strlen("Done")+1)
		{
			//assume have config dump response!
			remoteGatewayApp.config_dump = "\n\n************************\n";
			remoteGatewayApp.config_dump += "* Remote Subsystem Dump from '" + 
				remoteGatewayApp.appInfo.name + "' at url: " + remoteGatewayApp.appInfo.url + "\n";
			remoteGatewayApp.config_dump += "* \n";
			remoteGatewayApp.config_dump += "\n\n";
			remoteGatewayApp.config_dump += commandResponseString.substr(strlen("Done")+1);

			__COUTTV__(remoteGatewayApp.config_dump);
		}

	}  //end SendRemoteGatewayCommand()
	catch(const std::runtime_error& e)
	{
		__SS__ << "Failure sending Remote Gateway App command '" << 
			(tmpCommand.size() > 30?tmpCommand.substr(0,30):tmpCommand)
			<< "' at url: " << remoteGatewayApp.appInfo.url << " due to error: " << e.what() << __E__;
		__COUT_ERR__ << ss.str();	
		remoteGatewayApp.error = ss.str();
	}  //end SendRemoteGatewayCommand() catch

}  //end SendRemoteGatewayCommand()

//==============================================================================
// CheckRemoteGatewayStatus
//	static function
//		Just need status, progress, and detail of ots::GatewaySupervisor extracted from GetRemoteGatewayStatus
void GatewaySupervisor::CheckRemoteGatewayStatus(GatewaySupervisor::RemoteGatewayInfo& remoteGatewayApp, 
	const std::unique_ptr<TransceiverSocket>& /* not transferring ownership */ remoteGatewaySocket,
	int portForReverseLoginOverUDP)
try
{
	__COUTT__ << "Checking remote gateway status of '" << remoteGatewayApp.appInfo.name << "'" << __E__;
	std::vector<std::string> parsedFields = StringMacros::getVectorFromString(remoteGatewayApp.appInfo.url,{':'});
	__COUTTV__(StringMacros::vectorToString(parsedFields));

	if(parsedFields.size() == 3)
	{
		Socket      gatewayRemoteSocket(parsedFields[1],atoi(parsedFields[2].c_str()));		
		std::string	requestString = "GetRemoteGatewayStatus";
		if(portForReverseLoginOverUDP)
			requestString += "," + std::to_string(portForReverseLoginOverUDP);
		__COUT_TYPE__(TLVL_DEBUG+24) << __COUT_HDR__ << "requestString = " << requestString << __E__;	
		std::string remoteStatusString = remoteGatewaySocket->sendAndReceive(gatewayRemoteSocket,
			requestString, 10 /*timeoutSeconds*/);
		__COUT_TYPE__(TLVL_DEBUG+24) << __COUT_HDR__ << "remoteStatusString = " << remoteStatusString << __E__;	

		std::string value, name;
		size_t after = 0;
		while((name = StringMacros::extractXmlField(remoteStatusString, "name", 0, after, &after)) != "")
		{					
			//find class associated with record
			value = StringMacros::extractXmlField(remoteStatusString, "class", 0, after);
			if(value == XDAQContextTable::GATEWAY_SUPERVISOR_CLASS) 
			{
				//found remote gateway
				__COUTTV__(remoteStatusString.size());
				__COUTTV__(after);
				__COUTTV__(value);

				//get gateway status
				value = StringMacros::extractXmlField(remoteStatusString, "status", 0, after);
				__COUTTV__(value);
				remoteGatewayApp.appInfo.status = value;

				value = StringMacros::extractXmlField(remoteStatusString, "progress", 0, after);
				__COUTTV__(value);
				remoteGatewayApp.appInfo.progress = atoi(value.c_str());

				value = StringMacros::extractXmlField(remoteStatusString, "detail", 0, after);
				__COUTTV__(value);
				remoteGatewayApp.appInfo.detail = StringMacros::decodeURIComponent(value);

				value = StringMacros::extractXmlField(remoteStatusString, "time", 0, after);
				__COUTTV__(value);
				remoteGatewayApp.appInfo.lastStatusTime = atoi(value.c_str());

				value = StringMacros::extractXmlField(remoteStatusString, "console_err_count", 0, after);
				__COUTTV__(value);				
				remoteGatewayApp.consoleErrCount = atoi(value.c_str());

				value = StringMacros::extractXmlField(remoteStatusString, "console_warn_count", 0, after);
				__COUTTV__(value);
				remoteGatewayApp.consoleWarnCount = atoi(value.c_str());

			} //end found Remote Gateway status
			else //found remote subapp
			{
				remoteGatewayApp.subapps[name].class_name = value;
				__COUTTV__(value);
			}
		} //end primary loop

		//get system messages
		value = StringMacros::extractXmlField(remoteStatusString, "systemMessages", 0, after);	
		__COUTT__ << "Remote System Messages:" << value << __E__;
		std::vector<std::string> parsedSysMsgs;
		StringMacros::getVectorFromString(value,parsedSysMsgs,{'|'});

		//Format: targetUser | time | msg | targetUser | time | msg...etc
		for(size_t i = 0; i+2 < parsedSysMsgs.size(); i+=3)
		{
			GatewaySupervisor::addSystemMessage(parsedSysMsgs[i],
				"Remote System Message from '" + remoteGatewayApp.appInfo.name
				+ "' at url: " + remoteGatewayApp.appInfo.url + " ... " +
				StringMacros::decodeURIComponent(parsedSysMsgs[i+2]));
		} //end System Message handling loop
	}
	else
		__COUT_WARN__ << "Illegal Remote Gateawy App URL (must be ots:<IP>:<PORT>): " << remoteGatewayApp.appInfo.url << __E__;
} //end CheckRemoteGatewayStatus()
catch(const std::runtime_error& e)
{
	__COUT_WARN__ << "Failure getting Remote Gateway App status of '" << remoteGatewayApp.appInfo.name
		<< "' at url: " << remoteGatewayApp.appInfo.url << " due to error: " << e.what() << __E__;
	
	remoteGatewayApp.appInfo.status                = SupervisorInfo::APP_STATUS_UNKNOWN;
	remoteGatewayApp.appInfo.progress              = 0;
	remoteGatewayApp.appInfo.detail                = "Unknown UDP Message Error";
	remoteGatewayApp.appInfo.lastStatusTime 	   = time(0);
}  //end CheckRemoteGatewayStatus() catch

//==============================================================================
// StateChangerWorkLoop
//	child thread
void GatewaySupervisor::StateChangerWorkLoop(GatewaySupervisor* theSupervisor)
{
	ConfigurationTree configLinkNode = theSupervisor->CorePropertySupervisorBase::getSupervisorTableNode();

	std::string ipAddressForStateChangesOverUDP = configLinkNode.getNode("IPAddressForStateChangesOverUDP").getValue<std::string>();
	int         portForStateChangesOverUDP      = configLinkNode.getNode("PortForStateChangesOverUDP").getValue<int>();
	bool        acknowledgementEnabled          = configLinkNode.getNode("EnableAckForStateChangesOverUDP").getValue<bool>();
	bool		enableStateChanges				= configLinkNode.getNode("EnableStateChangesOverUDP").getValue<bool>();

	__COUTV__(ipAddressForStateChangesOverUDP);
	__COUTV__(portForStateChangesOverUDP);
	__COUTV__(acknowledgementEnabled);
	__COUTV__(enableStateChanges);

	TransceiverSocket sock(ipAddressForStateChangesOverUDP,
	                       portForStateChangesOverUDP);  // Take Port from Table
	try
	{
		sock.initialize();
	}
	catch(...)
	{
		// generate special message to indicate failed socket
		__SS__ << "FATAL Console error. Could not initialize socket at ip '" << ipAddressForStateChangesOverUDP << "' and port " << portForStateChangesOverUDP
		       << ". Perhaps it is already in use? Exiting State Changer "
		          "SOAPUtilities::receive loop."
		       << __E__;
		__SS_THROW__;
		return;
	}

	std::size_t              commaPosition;
	unsigned int             commaCounter = 0;
	std::size_t              begin        = 0;
	std::string              buffer;
	std::string              errorStr;
	std::string              fsmName;
	std::string              command;
	std::vector<std::string> parameters;
	while(1)
	{
		// workloop procedure
		//	if SOAPUtilities::receive a UDP command
		//		execute command
		//	else
		//		sleep

		if(sock.receive(buffer, 0 /*timeoutSeconds*/, 1 /*timeoutUSeconds*/, false /*verbose*/) != -1)
		{
			__COUT_TYPE__(TLVL_DEBUG+9) << __COUT_HDR__ << "UDP State Changer packet received from ip:port " <<
				sock.getLastIncomingIPAddress() << ":" << sock.getLastIncomingPort() <<
				" of size = " << buffer.size() << __E__;
			__COUTVS__(11,buffer);

			try
			{
				
			
				bool remoteGatewayStatus = buffer.find("GetRemoteGatewayStatus") == 0;
				if(remoteGatewayStatus || buffer == "GetRemoteAppStatus")
				{
					__COUT_TYPE__(TLVL_DEBUG+12) << "Giving app status to remote monitor..." << __E__;

					if(remoteGatewayStatus && buffer.size() > strlen("GetRemoteGatewayStatus")+1)
					{
						std::string tmpIP = sock.getLastIncomingIPAddress();
						int tmpPort = atoi(buffer.substr(strlen("GetRemoteGatewayStatus")+1).c_str());
						
						if(!theSupervisor->theWebUsers_.remoteLoginVerificationEnabled_ ||
							theSupervisor->theWebUsers_.remoteLoginVerificationIP_ != tmpIP ||
							theSupervisor->theWebUsers_.remoteLoginVerificationPort_ != tmpPort)
						{
							theSupervisor->theWebUsers_.remoteLoginVerificationIP_ = tmpIP;
							theSupervisor->theWebUsers_.remoteLoginVerificationPort_ = tmpPort;
							theSupervisor->theWebUsers_.remoteLoginVerificationEnabled_ = true; //mark as under remote control
							__COUT_INFO__ << "This Gateway is now under remote control and will validate logins through remote Gateway Supervisor at "
								<< theSupervisor->theWebUsers_.remoteLoginVerificationIP_ << ":" << 
								theSupervisor->theWebUsers_.remoteLoginVerificationPort_ << __E__;
						}
					}

					HttpXmlDocument xmlOut;
					for(const auto& it : theSupervisor->allSupervisorInfo_.getAllSupervisorInfo())
					{
						const auto& appInfo = it.second;
						if(remoteGatewayStatus && appInfo.getClass() != XDAQContextTable::GATEWAY_SUPERVISOR_CLASS)
							continue; //only return Gateway status

						xmlOut.addTextElementToData("name",
													appInfo.getName());                      // get application name
						xmlOut.addTextElementToData("id", std::to_string(appInfo.getId()));  // get application id
						xmlOut.addTextElementToData("status", appInfo.getStatus());          // get status
						xmlOut.addTextElementToData(
							"time", std::to_string(appInfo.getLastStatusTime())); // ? StringMacros::getTimestampString(appInfo.getLastStatusTime()) : "0");  // get time stamp
						xmlOut.addTextElementToData("stale",
													std::to_string(time(0) - appInfo.getLastStatusTime()));  // time since update
						xmlOut.addTextElementToData("progress", std::to_string(appInfo.getProgress()));      // get progress
						xmlOut.addTextElementToData("detail", appInfo.getDetail());                          // get detail
						xmlOut.addTextElementToData("class",
													appInfo.getClass());  // get application class
						xmlOut.addTextElementToData("url",
													appInfo.getURL());  // get application url
						xmlOut.addTextElementToData("context",
													appInfo.getContextName());  // get context
						auto subappElement = xmlOut.addTextElementToData("subapps", "");
						for(auto& subappInfoPair : appInfo.getSubappInfo())
						{
							xmlOut.addTextElementToParent("subapp_name", subappInfoPair.first, subappElement);
							xmlOut.addTextElementToParent("subapp_status", subappInfoPair.second.status, subappElement);  // get status
							xmlOut.addTextElementToParent("subapp_time",
								subappInfoPair.second.lastStatusTime ? StringMacros::getTimestampString(subappInfoPair.second.lastStatusTime) : "0",
														subappElement);  // get time stamp
							xmlOut.addTextElementToParent("subapp_stale", std::to_string(time(0) - subappInfoPair.second.lastStatusTime), subappElement);  // time since update
							xmlOut.addTextElementToParent("subapp_progress", std::to_string(subappInfoPair.second.progress), subappElement);               // get progress
							xmlOut.addTextElementToParent("subapp_detail", subappInfoPair.second.detail, subappElement);                                   // get detail
							xmlOut.addTextElementToParent("subapp_url", subappInfoPair.second.url, subappElement);                                   // get url
							xmlOut.addTextElementToParent("subapp_class", subappInfoPair.second.class_name, subappElement);                                // get class

						}
					}

					if(remoteGatewayStatus) //also return System Messages
					{
						__COUT_TYPE__(TLVL_DEBUG+12) << "Giving extra Gateway info to remote monitor..." << __E__;		
									
						xmlOut.addTextElementToData("systemMessages",
													theWebUsers_.getAllSystemMessages());  

						std::lock_guard<std::mutex> lock(theSupervisor->systemStatusMutex_); //lock for rest of scope
						xmlOut.addTextElementToData("console_err_count",
													std::to_string(theSupervisor->systemConsoleErrCount_));  
						xmlOut.addTextElementToData("console_warn_count",
													std::to_string(theSupervisor->systemConsoleWarnCount_));  
					}

					std::stringstream out;
					xmlOut.outputXmlDocument((std::ostringstream*)&out, false /*dispStdOut*/, false /*allowWhiteSpace*/);
					__COUT_TYPE__(TLVL_DEBUG+23) << __COUT_HDR__ << "App status to monitor: " << out.str() << __E__;
					sock.acknowledge(out.str(), false /* verbose */);
					continue;
				} //end GetRemoteAppStatus
				if(buffer == "ResetConsoleCounts") 
				{
					__COUT__ << "Remote request to reset Console Counts..." << __E__;

					//zero out console count and retake first messages

					for(const auto& it : theSupervisor->allSupervisorInfo_.getAllSupervisorInfo())
					{
						const auto& appInfo = it.second;
						if(appInfo.isTypeConsoleSupervisor())
						{
							xoap::MessageReference tempMessage = SOAPUtilities::makeSOAPMessageReference("ResetConsoleCounts");
							std::string reply = theSupervisor->send(appInfo.getDescriptor(), tempMessage);

							if(reply != "Done")
							{
								__SS__ << "Error while resetting console counts of Supervisor instance = '" << appInfo.getName() << "' [LID=" << appInfo.getId()
									<< "] in Context '" << appInfo.getContextName() << "' [URL=" << appInfo.getURL() << "].\n\n"
									<< reply << __E__;
								__SS_THROW__;
							}
							__COUT__ << "Reset console counts of Supervisor instance = '" << appInfo.getName() << "' [LID=" << appInfo.getId()
									<< "] in Context '" << appInfo.getContextName() << "' [URL=" << appInfo.getURL() << "]." << __E__;
						}
					} //end loop for Console Supervisors

					//for user display feedback, clear local cached values also
					std::lock_guard<std::mutex> lock(theSupervisor->systemStatusMutex_); //lock for rest of scope
					theSupervisor->lastConsoleErrTime_ = "0"; 	theSupervisor->lastConsoleErr_ = "";
					theSupervisor->lastConsoleWarnTime_ = "0"; 	theSupervisor->lastConsoleWarn_ = "";
					theSupervisor->lastConsoleInfoTime_ = "0";	theSupervisor->lastConsoleInfo_ = "";
					theSupervisor->firstConsoleErrTime_ = "0"; 	theSupervisor->firstConsoleErr_ = "";
					theSupervisor->firstConsoleWarnTime_ = "0"; theSupervisor->firstConsoleWarn_ = "";
					theSupervisor->firstConsoleInfoTime_ = "0"; theSupervisor->firstConsoleInfo_ = "";
					
					sock.acknowledge("Done", false /* verbose */);
					continue;
				} //end ResetConsoleCounts
				else if(buffer.find("loginVerify") == 0)
				{
					__COUTT__ << "Checking login verification request from remote gateway..." << __E__;

					//Lookup cookie code and return refreshed cookie code and user info
					// command = loginVerify
					// parameters.addParameter("CookieCode");
					// parameters.addParameter("RefreshOption");
					// parameters.addParameter("IPAddress");
					std::vector<std::string> rxParams = StringMacros::getVectorFromString(buffer,{','});
					__COUTVS__(23,StringMacros::vectorToString(rxParams));

					if(rxParams.size() != 4)
					{
						__COUT_ERR__ << "Invalid remote login verify attempt!" << __E__;
						sock.acknowledge("0", false /* verbose */);
						continue;	
					}

					// If TRUE, cookie code is good, and refreshed code is in cookieCode, also pointers
					// optionally for uint8_t userPermissions, uint64_t uid  Else, error message is
					// returned in cookieCode
					std::map<std::string /*groupName*/, WebUsers::permissionLevel_t> userGroupPermissionsMap;
					std::string                                                      userWithLock = "";
					uint64_t                                                         uid, userSessionIndex;
					std::string cookieCode = rxParams[1];
					if(!theWebUsers_.cookieCodeIsActiveForRequest(
						cookieCode /*cookieCode*/, &userGroupPermissionsMap, &uid /*uid is not given to remote users*/, 
						rxParams[3] /*ip*/, rxParams[2] /*refresh*/ == "1", &userWithLock, &userSessionIndex))
					{
						__COUT_ERR__ << "Remote login failed!" << __E__;
						sock.acknowledge("0", false /* verbose */);
						continue;	
					}

					// Returned user info:
					// retParameters.addParameter("CookieCode", cookieCode);
					// "Permissions", StringMacros::mapToString(userGroupPermissionsMap).c_str());
					// "UserWithLock", userWithLock);
					// "Username", theWebUsers_.getUsersUsername(uid));
					// "DisplayName", theWebUsers_.getUsersDisplayName(uid));
					// "UserSessionIndex" 

					std::string retStr = "";	
					std::string username = theWebUsers_.getUsersUsername(uid);
					retStr += cookieCode;
					retStr += "," + StringMacros::encodeURIComponent(StringMacros::mapToString(userGroupPermissionsMap));
					retStr += "," + userWithLock;
					retStr += "," + username;
					retStr += "," + theWebUsers_.getUsersDisplayName(uid);
					retStr += "," + std::to_string(userSessionIndex);

					__COUTVS__(23,retStr);
					__COUTT__ << "Remote login successful for " << username << ", userWithLock = " << userWithLock << __E__;
					sock.acknowledge(retStr, false /* verbose */);
					continue;
				}
				else if(buffer == "GetRemoteDesktopIcons")
				{
					__COUT__ << "Giving desktop icons to remote gateway..." << __E__;

					// get icons and create comma-separated string based on user permissions
					//	note: each icon has own permission threshold, so each user can have
					//		a unique desktop icon experience.

					// use latest context always from temporary configuration manager,
					//	to get updated icons every time...
					//(so icon changes do no require an ots restart)

					ConfigurationManager                              tmpCfgMgr; // Creating new temporary instance so that constructor will activate latest context, note: not using member CorePropertySupervisorBase::theConfigurationManager_
					const DesktopIconTable*                           iconTable = tmpCfgMgr.__GET_CONFIG__(DesktopIconTable);
					const std::vector<DesktopIconTable::DesktopIcon>& icons     = iconTable->getAllDesktopIcons();

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


					bool getRemoteIcons = true;

					bool firstIcon = true;
					for(const auto& icon : icons)
					{
						//__COUTV__(icon.caption_);
						//__COUTV__(icon.permissionThresholdString_);

						//ignore permission level, and give all icons

						//__COUTV__(icon.caption_);

						if(getRemoteIcons)
						{
							__COUTVS__(10,icon.windowContentURL_);
							if(icon.windowContentURL_.size() > 4 && 
								icon.windowContentURL_[0] == 'o' &&
								icon.windowContentURL_[1] == 't' &&
								icon.windowContentURL_[2] == 's' &&
								icon.windowContentURL_[3] == ':')
							{
								 __COUT_TYPE__(TLVL_DEBUG+10) << __COUT_HDR__ << "Retrieving remote icons at " << icon.windowContentURL_ << __E__;

								std::vector<std::string> parsedFields = StringMacros::getVectorFromString(icon.windowContentURL_,{':'});
								__COUTVS__(10,StringMacros::vectorToString(parsedFields));

								if(parsedFields.size() == 3)
								{
									Socket      iconRemoteSocket(parsedFields[1],atoi(parsedFields[2].c_str()));

									// ConfigurationTree configLinkNode = theSupervisor->CorePropertySupervisorBase::getSupervisorTableNode();
									// std::string ipAddressForStateChangesOverUDP = configLinkNode.getNode("IPAddressForStateChangesOverUDP").getValue<std::string>();
									__COUTVS__(10,ipAddressForStateChangesOverUDP);
									TransceiverSocket iconSocket(ipAddressForStateChangesOverUDP);
									std::string remoteIconString = iconSocket.sendAndReceive(iconRemoteSocket,"GetRemoteDesktopIcons", 10 /*timeoutSeconds*/);
									__COUTVS__(10,remoteIconString);
									continue;
								}
							}
						} //end remote icon handling

						// have icon access, so add to CSV string
						if(firstIcon)
							firstIcon = false;
						else
							iconString += ",";
							

						__COUTVS__(10,icon.caption_);
						iconString += icon.caption_;
						iconString += "," + icon.alternateText_;
						iconString += "," + std::string(icon.enforceOneWindowInstance_ ? "1" : "0");
						iconString += "," + std::string("1");  // set permission to 1 so the
																// desktop shows every icon that the
																// server allows (i.e., trust server
																// security, ignore client security)
						iconString += "," + icon.imageURL_;
						iconString += "," + iconTable->getRemoteURL(&tmpCfgMgr, icon.windowContentURL_);
						iconString += "," + icon.folderPath_;
					}
					__COUTVS__(10,iconString);

					sock.acknowledge(iconString, true /* verbose */);
					continue;
				} //end GetRemoteDesktopIcons
				else if(!enableStateChanges)//else it is an FSM Command!
				{
					__COUT_WARN__ << "Skipping potential FSM Command because enableStateChanges=false" << __E__;
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
				while((commaPosition = buffer.find(',', begin)) != std::string::npos || commaCounter == nCommas)
				{
					if(commaCounter == nCommas)
						commaPosition = buffer.size();
					if(commaCounter == 0)
						fsmName = buffer.substr(begin, commaPosition - begin);
					else if(commaCounter == 1)
						command = buffer.substr(begin, commaPosition - begin);
					else
						parameters.push_back(buffer.substr(begin, commaPosition - begin));
					__COUT__ << "Word[" << commaCounter << "]: " << 
						buffer.substr(begin, commaPosition - begin) << __E__;

					begin = commaPosition + 1;
					++commaCounter;
				}
				__COUTV__(fsmName);
				__COUTV__(command);
				__COUTV__(StringMacros::vectorToString(parameters));


				// set scope of mutex
				std::string extraDoneContent = "";
				{
					// should be mutually exclusive with GatewaySupervisor main thread state
					// machine accesses  lockout the messages array for the remainder of the
					// scope  this guarantees the reading thread can safely access the
					// messages
					if(theSupervisor->VERBOSE_MUTEX)
						__COUT__ << "Waiting for FSM access" << __E__;
					std::lock_guard<std::mutex> lock(theSupervisor->stateMachineAccessMutex_);
					if(theSupervisor->VERBOSE_MUTEX)
						__COUT__ << "Have FSM access" << __E__;

					errorStr = theSupervisor->attemptStateMachineTransition(
						0, 0, command, fsmName, WebUsers::DEFAULT_STATECHANGER_USERNAME /*fsmWindowName*/, 
						WebUsers::DEFAULT_STATECHANGER_USERNAME, parameters);

					if(errorStr == "" && command == RunControlStateMachine::CONFIGURE_TRANSITION_NAME)
						extraDoneContent = theSupervisor->activeStateMachineConfigurationDumpOnConfigure_;

					if(errorStr == "" && command == RunControlStateMachine::START_TRANSITION_NAME)
						extraDoneContent = theSupervisor->activeStateMachineConfigurationDumpOnRun_; 
				}

				if(errorStr != "")
				{
					__SS__ << "UDP State Changer failed to execute command because of the "
							"following error: "
						<< errorStr;
					__COUT_ERR__ << ss.str();
					if(acknowledgementEnabled)
						sock.acknowledge(errorStr, true /* verbose */);
				}
				else
				{
					__SS__ << "Successfully executed state change command '" << command << ".'" << __E__;
					__COUT_INFO__ << ss.str();
					if(acknowledgementEnabled)
						sock.acknowledge("Done" + (
							extraDoneContent.size()?("," + extraDoneContent):"" //append extra done content, if any
						), true /* verbose */);
				}
				
			}
			catch(...)
			{
				__SS__ << "Error was caught handling UDP command." << __E__;
				try
				{ throw; }
				catch(const std::runtime_error& e)
				{ ss << "Here is the error: " << e.what() << __E__;	}
				catch(...)
				{ ss << "Unrecognized error." << __E__;	}
				
				__COUT_ERR__ << ss.str();
				if(acknowledgementEnabled)
					sock.acknowledge(ss.str(), true /* verbose */);
			}
		}
		else
			sleep(1);
	}
}  // end StateChangerWorkLoop()

//==============================================================================
// makeSystemLogEntry
//	makes a logbook entry into all Logbook supervisors
//		and specifically the current active experiments within the logbook
//	escape entryText to make it html/xml safe!!
////      reserved: ", ', &, <, >, \n, double-space
void GatewaySupervisor::makeSystemLogEntry(std::string entryText)
{
	__COUT__ << "Making System Logbook Entry: " << entryText << __E__;
	lastLogbookEntry_ = entryText;
	lastLogbookEntryTime_ = time(0);

	SupervisorInfoMap logbookInfoMap = allSupervisorInfo_.getAllLogbookTypeSupervisorInfo();

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

	// SOAPParametersV parameters(1);
	// parameters[0].setName("EntryText"); parameters[0].setValue(entryText);

	for(auto& logbookInfo : logbookInfoMap)
	{
		try
		{
			xoap::MessageReference retMsg = SOAPMessenger::sendWithSOAPReply(logbookInfo.second.getDescriptor(), "MakeSystemLogEntry", parameters);

			SOAPParameters retParameters("Status");
			// SOAPParametersV retParameters(1);
			// retParameters[0].setName("Status");
			SOAPUtilities::receive(retMsg, retParameters);

			std::string status = retParameters.getValue("Status");
			__COUT__ << "Returned Status: " << status << __E__;  // retParameters[0].getValue() << __E__ << __E__;
			if(status != "Success")
			{
				__SS__ << "Invalid return status on MakeSystemLogEntry: " << status << __E__;
				__SS_THROW__;
			}
		}
		catch(const xdaq::exception::Exception& e)  // due to xoap send failure
		{
			__SS__ << "Failed to send system log SOAP entry to " << logbookInfo.second.getContextName() << "/" << logbookInfo.second.getName()
			       << " w/app ID=" << logbookInfo.first << __E__ << e.what();

			__SS_THROW__;
		}
		catch(std::runtime_error& e)
		{
			__SS__ << "Error during handling of system log SOAP entry at " << logbookInfo.second.getContextName() << "/" << logbookInfo.second.getName()
			       << " w/app ID=" << logbookInfo.first << __E__ << e.what();
			__SS_THROW__;
		}
	}
}  // end makeSystemLogEntry()

//==============================================================================
void GatewaySupervisor::Default(xgi::Input* /*in*/, xgi::Output* out)
{
	if(!supervisorGuiHasBeenLoaded_ && (supervisorGuiHasBeenLoaded_ = true))  // make system logbook entry that ots has been started
		makeSystemLogEntry("ots started.");

	*out << "<!DOCTYPE HTML><html lang='en'><head><title>ots</title>" << GatewaySupervisor::getIconHeaderString() <<
	    // end show ots icon
	    "</head>"
	     << "<frameset col='100%' row='100%'>"
	     << "<frame src='/WebPath/html/Desktop.html?urn=" << this->getApplicationDescriptor()->getLocalId() << "&securityType=" << securityType_
	     << "'></frameset></html>";
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
// stateMachineIterationBreakpoint
//		get/set the state machine iteration breakpoint
//		If the iteration index >= breakpoint, then pause.
void GatewaySupervisor::stateMachineIterationBreakpoint(xgi::Input* in, xgi::Output* out)
try
{
	cgicc::Cgicc cgiIn(in);

	std::string requestType = CgiDataUtilities::getData(cgiIn, "Request");

	HttpXmlDocument           xmlOut;
	WebUsers::RequestUserInfo userInfo(requestType, CgiDataUtilities::postData(cgiIn, "CookieCode"));

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
			unsigned int breakpointSetValue = CgiDataUtilities::getDataAsInt(cgiIn, "breakpointSetValue");
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
			__SS__ << "Unknown iteration breakpoint request type = " << requestType << __E__;
			__SS_THROW__;
		}
	}
	catch(const std::runtime_error& e)
	{
		__SS__ << "Error caught handling iteration breakpoint command: " << e.what() << __E__;
		__COUT_ERR__ << ss.str();
		xmlOut.addTextElementToData("Error", ss.str());
	}
	catch(...)
	{
		__SS__ << "Unknown error caught handling iteration breakpoint command." << __E__;
		try	{ throw; } //one more try to printout extra info
		catch(const std::exception &e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...){}
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
	try	{ throw; } //one more try to printout extra info
	catch(const std::exception &e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...){}
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

	out->getHTTPResponseHeader().addHeader("Access-Control-Allow-Origin", "*");  // to avoid block by blocked by CORS policy of browser
	cgicc::Cgicc cgiIn(in);

	std::string command     = CgiDataUtilities::getData(cgiIn, "StateMachine");
	std::string requestType = "StateMachine" + command;  // prepend StateMachine to request type

	HttpXmlDocument           xmlOut;
	WebUsers::RequestUserInfo userInfo(requestType, CgiDataUtilities::postData(cgiIn, "CookieCode"));

	CorePropertySupervisorBase::getRequestUserInfo(userInfo);

	if(!theWebUsers_.xmlRequestOnGateway(cgiIn, out, &xmlOut, userInfo))
		return;  // access failed

	std::string fsmName       = CgiDataUtilities::getData(cgiIn, "fsmName");
	std::string fsmWindowName = CgiDataUtilities::getData(cgiIn, "fsmWindowName");
	fsmWindowName             = StringMacros::decodeURIComponent(fsmWindowName);
	std::string currentState  = theStateMachine_.getCurrentStateName();

	__COUT__ << "Check for Handled by theIterator_" << __E__;

	// check if Iterator should handle
	if((activeStateMachineWindowName_ == "" || activeStateMachineWindowName_ == "iterator") &&
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

		xmlOut.addTextElementToData("state_tranisition_attempted",
		                            "0");  // indicate to GUI transition NOT attempted
		xmlOut.addTextElementToData("state_tranisition_attempted_err",
		                            ss.str());  // indicate to GUI transition NOT attempted
		xmlOut.outputXmlDocument((std::ostringstream*)out, false, true);
		return;
	}

	// At this point, attempting transition!

	std::vector<std::string> parameters;

	if(command == "Configure")
		parameters.push_back(CgiDataUtilities::postData(cgiIn, "ConfigurationAlias"));

	std::string logEntry = StringMacros::decodeURIComponent(
		CgiDataUtilities::postData(cgiIn, "logEntry"));

	attemptStateMachineTransition(&xmlOut, out, command, fsmName, fsmWindowName, userInfo.username_, parameters, logEntry);

}  // end stateMachineXgiHandler()

//==============================================================================
std::string GatewaySupervisor::attemptStateMachineTransition(HttpXmlDocument*                xmldoc,
                                                             std::ostringstream*             out,
                                                             const std::string&              command,
                                                             const std::string&              fsmName,
                                                             const std::string&              fsmWindowName,
                                                             const std::string&              username,
                                                             const std::vector<std::string>& commandParameters,
                                                             std::string             		 logEntry /* = "" */)
try
{
	std::string errorStr = "";

	std::string currentState = theStateMachine_.getCurrentStateName();
	__COUT__ << "State Machine command = " << command << __E__;	
	__COUT__ << "fsmName = " << fsmName << __E__;
	__COUT__ << "fsmWindowName = " << fsmWindowName << __E__;
	__COUTV__(username);
	__COUT__ << "activeStateMachineName_ = " << activeStateMachineName_ << __E__;
	__COUTV__(logEntry);
	__COUT__ << "command = " << command << __E__;
	__COUT__ << "commandParameters.size = " << commandParameters.size() << __E__;
	__COUTV__(StringMacros::vectorToString(commandParameters));

	//check if logEntry is in parameters
	if(!logEntry.size() && commandParameters.size() && 
		commandParameters.back().find("LogEntry:") == 0 && 
			commandParameters.back().size() > strlen("LogEntry:"))
	{
		logEntry = commandParameters.back().substr(strlen("LogEntry:"));
		__COUTV__(logEntry);	
	}

	/////////////////
	// Validate FSM name (do here because remote commands bypass stateMachineXgiHandler)
	//	if fsm name != active fsm name
	//		only allow, if current state is halted or init
	//		take active fsm name when configured
	//	else, allow
	if(activeStateMachineName_ != "" && activeStateMachineName_ != fsmName)
	{
		__COUT__ << "Validating... currentFSM = " << activeStateMachineName_ << ", currentState = " << currentState <<
			", newFSM = " << fsmName << ", command = " << command << __E__;
		if(currentState != RunControlStateMachine::HALTED_STATE_NAME && currentState != RunControlStateMachine::INITIAL_STATE_NAME)
		{
			// illegal for this FSM name to attempt transition

			__SS__ << "Error - Can not accept request because the State Machine "
				<< "with window name '" << activeStateMachineWindowName_ << "' (UID: " << activeStateMachineName_
				<< ") "
					"is currently "
				<< "in control of State Machine progress. ";
			ss << "\n\nIn order for this State Machine with window name '" << fsmWindowName << "' (UID: " << fsmName
				<< ") "
					"to control progress, please transition to " << 
					RunControlStateMachine::HALTED_STATE_NAME << " using the active "
				<< "State Machine '" << activeStateMachineWindowName_ << ".'" << __E__;
			__SS_THROW__;
		}
		else  // clear active state machine
		{
			activeStateMachineName_       = "";
			activeStateMachineWindowName_ = "";
		}
	}
	//FSM name validated

	if(logEntry != "")
		makeSystemLogEntry("Attempting FSM command '" + command + "' from state '" + 
			currentState + "' with user log entry: " + logEntry);

	setLastLogEntry(command,logEntry);

	SOAPParameters parameters;
	if(command == RunControlStateMachine::CONFIGURE_TRANSITION_NAME)
	{
		activeStateMachineConfigurationDumpOnConfigure_ = ""; //clear (and set if enabled during configure transition)
		activeStateMachineConfigurationDumpOnRun_ = ""; //clear (and set if enabled during configure transition)
		activeStateMachineConfigurationDumpOnRunEnable_ = false, activeStateMachineConfigurationDumpOnConfigureEnable_ = false; //clear (and set if enabled during configure transition)		
		activeStateMachineConfigurationDumpOnConfigureFilename_ = ""; //clear (and set if enabled during configure transition)
		activeStateMachineConfigurationDumpOnRunFilename_ = ""; //clear (and set if enabled during configure transition)
		
		activeStateMachineRequireUserLogOnRun_ = false, activeStateMachineRequireUserLogOnConfigure_ = false; //clear (and set if enabled during configure transition)
		activeStateMachineRunInfoPluginType_ = TableViewColumnInfo::DATATYPE_STRING_DEFAULT; //clear (and set if enabled during configure transition)

		if(currentState != RunControlStateMachine::HALTED_STATE_NAME &&
			currentState != RunControlStateMachine::INITIAL_STATE_NAME)  // check if out of sync command
		{
			__SS__ << "Error - Can only transition to Configured if the current "
			       << "state is Initial or Halted. The current state is '" << currentState << 
				   ".' Perhaps your state machine is out of sync, or you need to Halt before Configuring." << __E__;
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
		std::string  dumpFormatOnConfigure, dumpFormatOnRun;
		{
			ConfigurationTree configLinkNode =
					CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(supervisorContextUID_, supervisorApplicationUID_);
			if(!configLinkNode.isDisconnected())
			{
				bool doThrow = false;
				try  // for backwards compatibility
				{
					ConfigurationTree fsmLinkNode = configLinkNode.getNode("LinkToStateMachineTable").getNode(fsmName);
						
					try { activeStateMachineRequireUserLogOnConfigure_ = fsmLinkNode.getNode("RequireUserLogInputOnConfigureTransition").getValue<bool>(); } catch(...) {;}
					try { activeStateMachineRequireUserLogOnRun_ = fsmLinkNode.getNode("RequireUserLogInputOnRunTransition").getValue<bool>(); } catch(...) {;}

					try
					{ 
						activeStateMachineRunInfoPluginType_ = fsmLinkNode.getNode("RunInfoPluginType").getValue<std::string>(); 							
					}
					catch(...) //ignore missing RunInfoPluginType
					{ 
						__COUT__ << "RunInfoPluginType not defined for FSM name '" <<
							fsmName << "' - please setup a valid run info plugin type to enable external Run Number coordination and dumping configuration info to an external location." << __E__; 
					} 

					activeStateMachineConfigurationDumpOnConfigureEnable_  = fsmLinkNode.getNode("EnableConfigurationDumpOnConfigureTransition").getValue<bool>();						
					activeStateMachineConfigurationDumpOnRunEnable_        = fsmLinkNode.getNode("EnableConfigurationDumpOnRunTransition").getValue<bool>();						
										
					doThrow                       = true;  // at this point throw the exception!

					dumpFormatOnConfigure		  = fsmLinkNode.getNode("ConfigurationDumpOnConfigureFormat").getValue<std::string>();						
					dumpFormatOnRun				  = fsmLinkNode.getNode("ConfigurationDumpOnRunFormat").getValue<std::string>();

					std::string dumpFilePath, dumpFileRadix;
					dumpFilePath = fsmLinkNode.getNode("ConfigurationDumpOnConfigureFilePath").getValueWithDefault<std::string>(__ENV__("OTSDAQ_LOG_DIR"));
					dumpFileRadix = fsmLinkNode.getNode("ConfigurationDumpOnConfigureFileRadix").getValueWithDefault<std::string>("ConfigTransitionConfigurationDump");											
					activeStateMachineConfigurationDumpOnConfigureFilename_ = dumpFilePath + "/" + dumpFileRadix;
					dumpFilePath = fsmLinkNode.getNode("ConfigurationDumpOnRunFilePath").getValueWithDefault<std::string>(__ENV__("OTSDAQ_LOG_DIR"));
					dumpFileRadix = fsmLinkNode.getNode("ConfigurationDumpOnRunFileRadix").getValueWithDefault<std::string>("ConfigTransitionConfigurationDump");											
					activeStateMachineConfigurationDumpOnRunFilename_ = dumpFilePath + "/" + dumpFileRadix;

				}
				catch(std::runtime_error& e)  // throw exception on missing fields if dumpConfiguration set
				{						
					if(doThrow && (activeStateMachineConfigurationDumpOnConfigureEnable_ || 
						activeStateMachineConfigurationDumpOnRunEnable_))
					{
						__SS__ << "Configuration Dump was enabled, but there are missing fields! " << e.what() << __E__;
						__SS_THROW__;
					}
					else
						__COUT_INFO__ << "FSM configuration dump Link disconnected at '" << ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME << "/"
								<< supervisorContextUID_ << "/" << supervisorApplicationUID_ << "/"
								<< "LinkToStateMachineTable/" << fsmName << "/"
								<< "EnableConfigurationDumpOnConfigureTransition and/or EnableConfigurationDumpOnRunTransition" << __E__;						
				}
			} //end configuration dump check/handling
			else
				__COUT_INFO__ << "No Gateway Supervisor configuration record found at '" << ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME << "/"
							<< supervisorContextUID_ << "/" << supervisorApplicationUID_
							<< "' - consider adding one to control configuration dumps and state machine properties." << __E__;
		} //end check if configuration dump is enabled on configure transition

		if(activeStateMachineRequireUserLogOnConfigure_ && getLastLogEntry(RunControlStateMachine::CONFIGURE_TRANSITION_NAME).size() < 3)
		{
			__SS__ << "Error - the state machine property 'RequireUserLogInputOnConfigureTransition' has been enabled which requires the user to enter "
					"at least 3 characters of log info to proceed with the Configure transition." << __E__;
			__SS_THROW__;
		}

		parameters.addParameter("ConfigurationAlias", commandParameters[0]);

		std::string configurationAlias = parameters.getValue("ConfigurationAlias");
		__COUT__ << "Configure --> Name: ConfigurationAlias Value: " << configurationAlias << __E__;
		lastConfigurationAlias_ = configurationAlias;
		// save last used config alias
		std::string fn =
		    ConfigurationManager::LAST_TABLE_GROUP_SAVE_PATH + "/" + FSM_LAST_GROUP_ALIAS_FILE_START + fsmName + "." + FSM_USERS_PREFERENCES_FILETYPE;

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

		if(activeStateMachineName_ == "")
			__COUT_WARN__ << "The active state machine is an empty string, this is allowed for backwards compatibility, but may not be intentional! " <<
				"Make sure you or your system admins understand why the active FSM name is blank." << __E__;

		//Note: Must create configuration dump at this point!! In case this is a remote subsystem and must respond with string
		//Must define activeStateMachineConfigurationDumpOnRun_, activeStateMachineConfigurationDumpOnConfigure_; //cached at Configure transition

		try
		{
			CorePropertySupervisorBase::theConfigurationManager_->init();  // completely reset to re-align with any changes
		}
		catch(...)
		{
			__SS__ << "\nTransition to Configuring interrupted! "
				<< "The Configuration Manager could not be initialized." << __E__;
			__SS_THROW__;
		}

		// Translate the system alias to a group name/key
		try
		{
			theConfigurationTableGroup_ = CorePropertySupervisorBase::theConfigurationManager_->getTableGroupFromAlias(configurationAlias);
		}
		catch(...)
		{
			__COUT_INFO__ << "Exception occurred translating the Configuration System Alias." << __E__;
		}

		if(theConfigurationTableGroup_.second.isInvalid())
		{
			__SS__ << "\nTransition to Configuring interrupted! System Configuration Alias '" << 
				configurationAlias << "' could not be translated to a group name and key." << __E__;
			__SS_THROW__;
		}

		__COUT_INFO__ << "Configuration table group name: " << theConfigurationTableGroup_.first << " key: " << theConfigurationTableGroup_.second << __E__;

		// load and activate Configuration Alias
		try
		{
			//first get group type - it must be Configuration type!
			std::string groupTypeString;
			CorePropertySupervisorBase::theConfigurationManager_->loadTableGroup(
				theConfigurationTableGroup_.first, theConfigurationTableGroup_.second,
				false /*doActivate*/,
				0 /*groupMembers      */,
				0 /*progressBar       */,
				0 /*accumulateWarnings*/,
				0 /*groupComment      */,
				0 /*groupAuthor       */,
				0 /*groupCreateTime   */,
				true /*doNotLoadMember */,
				&groupTypeString
				);
			if(groupTypeString != ConfigurationManager::GROUP_TYPE_NAME_CONFIGURATION)
			{
				__SS__ << "Illegal attempted configuration group type. The table group '" <<
					theConfigurationTableGroup_.first << "(" << theConfigurationTableGroup_.second << ")' is of type " <<
					groupTypeString << ". It must be " << ConfigurationManager::GROUP_TYPE_NAME_CONFIGURATION << "." << __E__;
				__SS_THROW__;
			}

			CorePropertySupervisorBase::theConfigurationManager_->loadTableGroup(
				theConfigurationTableGroup_.first, theConfigurationTableGroup_.second, true /*doActivate*/);

			__COUT__ << "Done loading Configuration Alias." << __E__;

			// mark the translated group as the last activated group
			std::pair<std::string /*group name*/, TableGroupKey> activatedGroup(std::string(theConfigurationTableGroup_.first), theConfigurationTableGroup_.second);
			ConfigurationManager::saveGroupNameAndKey(activatedGroup, ConfigurationManager::LAST_ACTIVATED_CONFIG_GROUP_FILE);

			__COUT__ << "Done activating Configuration Alias." << __E__;
		}
		catch(const std::runtime_error& e)
		{
			__SS__ << "\nTransition to Configuring interrupted! System Configuration Alias " << configurationAlias << " was translated to " << theConfigurationTableGroup_.first << " ("
				<< theConfigurationTableGroup_.second << ") but could not be loaded and initialized." << __E__;
			ss << "\n\nHere was the error: " << e.what()
			<< "\n\nTo help debug this problem, try activating this group in the Configuration "
				"GUI "
			<< " and detailed errors will be shown." << __E__;
			__SS_THROW__;
		}
		catch(...)
		{
			__SS__ << "\nTransition to Configuring interrupted! System Configuration Alias " << configurationAlias << " was translated to " << theConfigurationTableGroup_.first << " ("
				<< theConfigurationTableGroup_.second << ") but could not be loaded and initialized." << __E__;
			try	{ throw; } //one more try to printout extra info
			catch(const std::exception &e)
			{
				ss << "Exception message: " << e.what();
			}
			catch(...){}
			ss << "\n\nTo help debug this problem, try activating this group in the Configuration "
				"GUI "
			<< " and detailed errors will be shown." << __E__;
			__SS_THROW__;
		}

		//at this point Configuration Tree is fully loaded


		//handle configuration dump if enabled on configure transition
		try  // errors in dump are not tolerated				
		{		
			//get/cache Run transition dump
			if(activeStateMachineConfigurationDumpOnRunEnable_ || 
				((activeStateMachineRunInfoPluginType_ != TableViewColumnInfo::DATATYPE_STRING_DEFAULT && 
					activeStateMachineRunInfoPluginType_ != "No Run Info Plugin")))
			{
				__COUT_INFO__ << "Caching the Configuration Dump for the Run transition..." << __E__;

				// dump configuration
				std::stringstream dumpSs;
				CorePropertySupervisorBase::theConfigurationManager_->dumpActiveConfiguration(
					"", //dumpFilePath + "/" + dumpFileRadix + "_" + std::to_string(time(0)) + ".dump",
					dumpFormatOnRun,
					lastConfigurationAlias_,
					getLastLogEntry(RunControlStateMachine::CONFIGURE_TRANSITION_NAME),
					theWebUsers_.getActiveUsersString(),
					dumpSs);

				activeStateMachineConfigurationDumpOnRun_ = dumpSs.str();						
			}
			else
				__COUT_INFO__ << "Not caching the Configuration Dump on the Run transition." << __E__;

			//get/cache Configuration transition dump
			if(activeStateMachineConfigurationDumpOnConfigureEnable_ || 
				((activeStateMachineRunInfoPluginType_ != TableViewColumnInfo::DATATYPE_STRING_DEFAULT && 
					activeStateMachineRunInfoPluginType_ != "No Run Info Plugin")))
			{
				__COUT_INFO__ << "Caching the Configuration Dump for the Configure transition..." << __E__;

				// dump configuration
				std::stringstream dumpSs;
				CorePropertySupervisorBase::theConfigurationManager_->dumpActiveConfiguration(
					"", //dumpFilePath + "/" + dumpFileRadix + "_" + std::to_string(time(0)) + ".dump",
					dumpFormatOnConfigure,
					lastConfigurationAlias_,
					getLastLogEntry(RunControlStateMachine::CONFIGURE_TRANSITION_NAME),
					theWebUsers_.getActiveUsersString(),
					dumpSs);

				activeStateMachineConfigurationDumpOnConfigure_ = dumpSs.str();						
			}
			else
				__COUT_INFO__ << "Not caching the Configuration Dump on the Configure transition." << __E__;
					
			
		} //end handle configuration dump if enabled on configure transition
		catch(const std::runtime_error& e)
		{
			__SS__ << "Error encoutered during configuration dump. Here is the error: " << e.what();
			__SS_THROW__;
		}
		catch(...)
		{
			__SS__ << "Unknown error encoutered during configuration dump.";
			__SS_THROW__;
		}

	} //end Configure transition
	else if(command == RunControlStateMachine::START_TRANSITION_NAME)
	{
		if(currentState != RunControlStateMachine::CONFIGURED_STATE_NAME)  // check if out of sync command
		{
			__SS__ << "Error - Can only transition to Configured if the current "
			       << "state is Halted. Perhaps your state machine is out of sync. "
			       << "(Likely the server was restarted or another user changed the state)" << __E__;
			__SS_THROW__;
		}

		if(activeStateMachineRequireUserLogOnRun_ && getLastLogEntry(RunControlStateMachine::START_TRANSITION_NAME).size() < 3)
		{
			__SS__ << "Error - the state machine property 'RequireUserLogInputOnRunTransition' has been enabled which requires the user to enter "
					"at least 3 characters of log info to proceed with the Run transition." << __E__;
			__SS_THROW__;
		}

		unsigned long runNumber;
		if(commandParameters.size() == 0)
		{
			runNumber = getNextRunNumber();
			// Check if run number should come from db, if so create run info record into database

			__COUTV__(activeStateMachineRunInfoPluginType_);

			if(activeStateMachineRunInfoPluginType_ != TableViewColumnInfo::DATATYPE_STRING_DEFAULT && 
				activeStateMachineRunInfoPluginType_ != "No Run Info Plugin")
			{
				std::unique_ptr<RunInfoVInterface> runInfoInterface = nullptr;
				try	{ runInfoInterface.reset(makeRunInfo(activeStateMachineRunInfoPluginType_, activeStateMachineName_));} catch(...) {;}
				if(runInfoInterface == nullptr)
				{
					__SS__ << "Run Info interface plugin construction failed of type " << activeStateMachineRunInfoPluginType_ << 
						" for claiming next run number!" << __E__;
					__SS_THROW__;
				}

				//FIXME -- October 2024, by rrivera (need future simplification from agioiosa) -  Should this 2nd param be activeStateMachineConfigurationDumpOnConfigure_?! What is the 2nd param for? Is conditionID_ enough?
				runNumber = runInfoInterface->claimNextRunNumber(conditionID_, activeStateMachineConfigurationDumpOnRun_);
			}  // end Run Info Plugin handling

			setNextRunNumber(runNumber + 1);
		}
		else
		{
			sscanf(commandParameters[0].c_str(),"%lu",&runNumber);
			__COUTV__(runNumber);		 
			setNextRunNumber(runNumber + 1);
		}
		parameters.addParameter("RunNumber", runNumber);
	} //end Start transition
	else if(!(command == RunControlStateMachine::HALT_TRANSITION_NAME || 
				command == RunControlStateMachine::SHUTDOWN_TRANSITION_NAME || 
				command == RunControlStateMachine::ERROR_TRANSITION_NAME ||		
				command == RunControlStateMachine::FAIL_TRANSITION_NAME || 		
				command == RunControlStateMachine::STARTUP_TRANSITION_NAME ||		
				command == RunControlStateMachine::INIT_TRANSITION_NAME || 		
				command == RunControlStateMachine::ABORT_TRANSITION_NAME ||
				command == RunControlStateMachine::PAUSE_TRANSITION_NAME || 
				command == RunControlStateMachine::RESUME_TRANSITION_NAME || 
				command == RunControlStateMachine::STOP_TRANSITION_NAME ))
	{		
		__SS__ << "Error - illegal state machine command received '" << command <<
					".'" << __E__;
		__SS_THROW__;		
	}

	xoap::MessageReference message = SOAPUtilities::makeSOAPMessageReference(command, parameters);
	// Maybe we return an acknowledgment that the message has been received and processed
	xoap::MessageReference reply = stateMachineXoapHandler(message);
	// stateMachineWorkLoopManager_.removeProcessedRequests();
	// stateMachineWorkLoopManager_.processRequest(message);

	if(xmldoc)
		xmldoc->addTextElementToData("state_tranisition_attempted",
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
		xmldoc->addTextElementToData("state_tranisition_attempted",
										"0");  // indicate to GUI transition NOT attempted
	if(xmldoc)
		xmldoc->addTextElementToData("state_tranisition_attempted_err",
										ss.str());  // indicate to GUI transition NOT attempted
	if(out)
		xmldoc->outputXmlDocument((std::ostringstream*)out, false /*dispStdOut*/, true /*allowWhiteSpace*/);

	return ss.str();
} // end attemptStateMachineTransition() error handling

//==============================================================================
xoap::MessageReference GatewaySupervisor::stateMachineXoapHandler(xoap::MessageReference message)

{
	__COUT__ << "FSM Soap Handler!" << __E__;
	stateMachineWorkLoopManager_.removeProcessedRequests();
	stateMachineWorkLoopManager_.processRequest(message);
	__COUT__ << "Done - FSM Soap Handler!" << __E__;
	return message;
}  // end stateMachineXoapHandler()

//==============================================================================
// stateMachineThread
//		This asynchronously sends the xoap message to its own RunControlStateMachine
//			(that the Gateway inherits from), which then calls the Gateway
//			transition functions and eventually the broadcast to transition the global
// state  machine.
bool GatewaySupervisor::stateMachineThread(toolbox::task::WorkLoop* workLoop)
{
	stateMachineSemaphore_.take();
	std::string command = SOAPUtilities::translate(stateMachineWorkLoopManager_.getMessage(workLoop)).getCommand();

	__COUT__ << "Propagating FSM command '" << command << "'..." << __E__;

	std::string reply = send(allSupervisorInfo_.getGatewayDescriptor(), stateMachineWorkLoopManager_.getMessage(workLoop));
	stateMachineWorkLoopManager_.report(workLoop, reply, 100, true);

	__COUT__ << "Done with FSM command '" << command << ".' Reply = " << reply << __E__;
	stateMachineSemaphore_.give();

	if(reply == "Fault")
	{
		__SS__ << "Failure to send Workloop transition command '" << command << "!' An error response '" << reply << "' was received." << __E__;
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
	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName() << " at " << std::ctime(&pause_time) << __E__;

	if(theStateMachine_.getProvenanceStateName() == RunControlStateMachine::RUNNING_STATE_NAME)
	{
		try
		{
			ConfigurationTree configLinkNode =
			    CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(supervisorContextUID_, supervisorApplicationUID_);
			if(!configLinkNode.isDisconnected())
			{
				ConfigurationTree fsmLinkNode       = configLinkNode.getNode("LinkToStateMachineTable").getNode(activeStateMachineName_);
				std::string       runInfoPluginType = fsmLinkNode.getNode("RunInfoPluginType").getValue<std::string>();
				__COUTV__(runInfoPluginType);
				if(runInfoPluginType != TableViewColumnInfo::DATATYPE_STRING_DEFAULT && runInfoPluginType != "No Run Info Plugin")
				{
					std::unique_ptr<RunInfoVInterface> runInfoInterface = nullptr;
					try
					{
						runInfoInterface.reset(makeRunInfo(runInfoPluginType, activeStateMachineName_));
					}
					catch(...)
					{
					}

					if(runInfoInterface == nullptr)
					{
						__SS__ << "Run Info interface plugin construction failed of type " << runInfoPluginType << __E__;
						__SS_THROW__;
					}

					runInfoInterface->updateRunInfo(getNextRunNumber(activeStateMachineName_) - 1, RunInfoVInterface::RunStopType::PAUSE);
				}
			}
		}
		catch(const std::runtime_error& e)
		{
			// ERROR
			__SS__ << "RUN INFO PAUSE TIME UPDATE INTO DATABASE FAILED!!! " << e.what() << __E__;
			__SS_THROW__;
		}
		catch(...)
		{
			// ERROR
			__SS__ << "RUN INFO PAUSE TIME UPDATE INTO DATABASE FAILED!!! " << __E__;
			try	{ throw; } //one more try to printout extra info
			catch(const std::exception &e)
			{
				ss << "Exception message: " << e.what();
			}
			catch(...){}
			__SS_THROW__;
		}  // End update pause time into run info db
	}      // end update Run Info handling
}  // end statePaused()

//==============================================================================
void GatewaySupervisor::stateRunning(toolbox::fsm::FiniteStateMachine& /*fsm*/)
{
	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName() << __E__;

	if(theStateMachine_.getProvenanceStateName() == RunControlStateMachine::PAUSED_STATE_NAME)
	{
		__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName() << " coming from resume" << __E__;

		try
		{
			ConfigurationTree configLinkNode =
			    CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(supervisorContextUID_, supervisorApplicationUID_);
			if(!configLinkNode.isDisconnected())
			{
				ConfigurationTree fsmLinkNode       = configLinkNode.getNode("LinkToStateMachineTable").getNode(activeStateMachineName_);
				std::string       runInfoPluginType = fsmLinkNode.getNode("RunInfoPluginType").getValue<std::string>();
				__COUTV__(runInfoPluginType);
				if(runInfoPluginType != TableViewColumnInfo::DATATYPE_STRING_DEFAULT && runInfoPluginType != "No Run Info Plugin")
				{
					std::unique_ptr<RunInfoVInterface> runInfoInterface = nullptr;
					try
					{
						runInfoInterface.reset(makeRunInfo(runInfoPluginType, activeStateMachineName_));
					}
					catch(...)
					{
					}

					if(runInfoInterface == nullptr)
					{
						__SS__ << "Run Info interface plugin construction failed of type " << runInfoPluginType << __E__;
						__SS_THROW__;
					}

					runInfoInterface->updateRunInfo(getNextRunNumber(activeStateMachineName_) - 1, RunInfoVInterface::RunStopType::RESUME);
				}
			}
		}
		catch(const std::runtime_error& e)
		{
			// ERROR
			__SS__ << "RUN INFO RESUME TIME UPDATE INTO DATABASE FAILED!!! " << e.what() << __E__;
			__SS_THROW__;
		}
		catch(...)
		{
			// ERROR
			__SS__ << "RUN INFO RESUME TIME UPDATE INTO DATABASE FAILED!!! " << __E__;
			try	{ throw; } //one more try to printout extra info
			catch(const std::exception &e)
			{
				ss << "Exception message: " << e.what();
			}
			catch(...){}
			__SS_THROW__;
		}  // End update pause time into run info db
	}      // end update Run Info handling
}  // end stateRunning()

//==============================================================================
void GatewaySupervisor::stateHalted(toolbox::fsm::FiniteStateMachine& /*fsm*/)
{
	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName() << " from " << theStateMachine_.getProvenanceStateName() << __E__;
	__COUT__ << "Fsm is in transition? " << (theStateMachine_.isInTransition() ? "yes" : "no") << __E__;

	__COUTV__(SOAPUtilities::translate(theStateMachine_.getCurrentMessage()).getCommand());

	// if coming from Running or Paused, update Run Info	w/HALT
	if(theStateMachine_.getProvenanceStateName() == RunControlStateMachine::RUNNING_STATE_NAME ||
	   theStateMachine_.getProvenanceStateName() == RunControlStateMachine::PAUSED_STATE_NAME)
	{
		try
		{
			ConfigurationTree configLinkNode =
			    CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(supervisorContextUID_, supervisorApplicationUID_);
			if(!configLinkNode.isDisconnected())
			{
				ConfigurationTree fsmLinkNode       = configLinkNode.getNode("LinkToStateMachineTable").getNode(activeStateMachineName_);
				std::string       runInfoPluginType = fsmLinkNode.getNode("RunInfoPluginType").getValue<std::string>();
				__COUTV__(runInfoPluginType);
				if(runInfoPluginType != TableViewColumnInfo::DATATYPE_STRING_DEFAULT && runInfoPluginType != "No Run Info Plugin")
				{
					std::unique_ptr<RunInfoVInterface> runInfoInterface = nullptr;
					try
					{
						runInfoInterface.reset(makeRunInfo(runInfoPluginType, activeStateMachineName_));
						// ,
						// CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(supervisorContextUID_, supervisorApplicationUID_),
						// CorePropertySupervisorBase::getSupervisorConfigurationPath());
					}
					catch(...)
					{
					}

					if(runInfoInterface == nullptr)
					{
						__SS__ << "Run Info interface plugin construction failed of type " << runInfoPluginType << __E__;
						__SS_THROW__;
					}

					runInfoInterface->updateRunInfo(getNextRunNumber(activeStateMachineName_) - 1, RunInfoVInterface::RunStopType::HALT);
				}
			}
		}
		catch(const std::runtime_error& e)
		{
			// ERROR
			__SS__ << "RUN INFO UPDATE INTO DATABASE FAILED!!! " << e.what() << __E__;
			__SS_THROW__;
		}
		catch(...)
		{
			// ERROR
			__SS__ << "RUN INFO UPDATE INTO DATABASE FAILED!!! " << __E__;
			try	{ throw; } //one more try to printout extra info
			catch(const std::exception &e)
			{
				ss << "Exception message: " << e.what();
			}
			catch(...){}
			__SS_THROW__;
		}  // End write run info into db
	}      // end update Run Info handling
}  // end stateHalted()

//==============================================================================
void GatewaySupervisor::stateConfigured(toolbox::fsm::FiniteStateMachine& /*fsm*/)
{
	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName() << " from " << theStateMachine_.getProvenanceStateName() << __E__;
	__COUT__ << "Fsm is in transition? " << (theStateMachine_.isInTransition() ? "yes" : "no") << __E__;

	__COUTV__(SOAPUtilities::translate(theStateMachine_.getCurrentMessage()).getCommand());

	// if coming from Running or Paused, update Run Info w/STOP
	if(theStateMachine_.getProvenanceStateName() == RunControlStateMachine::RUNNING_STATE_NAME ||
	   theStateMachine_.getProvenanceStateName() == RunControlStateMachine::PAUSED_STATE_NAME)
	{
		try
		{
			ConfigurationTree configLinkNode =
			    CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(supervisorContextUID_, supervisorApplicationUID_);
			if(!configLinkNode.isDisconnected())
			{
				ConfigurationTree fsmLinkNode       = configLinkNode.getNode("LinkToStateMachineTable").getNode(activeStateMachineName_);
				std::string       runInfoPluginType = fsmLinkNode.getNode("RunInfoPluginType").getValue<std::string>();
				__COUTV__(runInfoPluginType);
				if(runInfoPluginType != TableViewColumnInfo::DATATYPE_STRING_DEFAULT && runInfoPluginType != "No Run Info Plugin")
				{
					std::unique_ptr<RunInfoVInterface> runInfoInterface = nullptr;
					try
					{
						runInfoInterface.reset(makeRunInfo(runInfoPluginType, activeStateMachineName_));
						// ,
						// CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(supervisorContextUID_, supervisorApplicationUID_),
						// CorePropertySupervisorBase::getSupervisorConfigurationPath());
					}
					catch(...)
					{
					}

					if(runInfoInterface == nullptr)
					{
						__SS__ << "Run Info interface plugin construction failed of type " << runInfoPluginType << __E__;
						__SS_THROW__;
					}

					runInfoInterface->updateRunInfo(getNextRunNumber(activeStateMachineName_) - 1, RunInfoVInterface::RunStopType::STOP);
				}
			}
		}
		catch(const std::runtime_error& e)
		{
			// ERROR
			__SS__ << "RUN INFO INSERT OR UPDATE INTO DATABASE FAILED!!! " << e.what() << __E__;
			__SS_THROW__;
		}
		catch(...)
		{
			// ERROR
			__SS__ << "RUN INFO INSERT OR UPDATE INTO DATABASE FAILED!!! " << __E__;
			try	{ throw; } //one more try to printout extra info
			catch(const std::exception &e)
			{
				ss << "Exception message: " << e.what();
			}
			catch(...){}
			__SS_THROW__;
		}  // End write run info into db
	}      // end update Run Info handling

}  // end stateConfigured()

//==============================================================================
void GatewaySupervisor::inError(toolbox::fsm::FiniteStateMachine& /*fsm*/)
{
	__COUT__ << "Error occured - FSM current state: "
	         << "Failed? = " << theStateMachine_.getCurrentStateName() <<  // There may be a race condition here
	    //	when async errors occur (e.g. immediately in running)
	    " from " << theStateMachine_.getProvenanceStateName() << __E__;

	__COUT__ << "Error occured on command: " << (SOAPUtilities::translate(theStateMachine_.getCurrentMessage()).getCommand()) << __E__;

	// if coming from Running or Paused, update Run Info w/ERROR
	if(theStateMachine_.getProvenanceStateName() == RunControlStateMachine::RUNNING_STATE_NAME ||
	   theStateMachine_.getProvenanceStateName() == RunControlStateMachine::PAUSED_STATE_NAME)
	{
		try
		{
			ConfigurationTree configLinkNode =
			    CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(supervisorContextUID_, supervisorApplicationUID_);
			if(!configLinkNode.isDisconnected())
			{
				ConfigurationTree fsmLinkNode       = configLinkNode.getNode("LinkToStateMachineTable").getNode(activeStateMachineName_);
				std::string       runInfoPluginType = fsmLinkNode.getNode("RunInfoPluginType").getValue<std::string>();
				__COUTV__(runInfoPluginType);
				if(runInfoPluginType != TableViewColumnInfo::DATATYPE_STRING_DEFAULT && runInfoPluginType != "No Run Info Plugin")
				{
					std::unique_ptr<RunInfoVInterface> runInfoInterface = nullptr;
					try
					{
						runInfoInterface.reset(makeRunInfo(runInfoPluginType, activeStateMachineName_));
						// ,
						// CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(supervisorContextUID_, supervisorApplicationUID_),
						// CorePropertySupervisorBase::getSupervisorConfigurationPath());
					}
					catch(...)
					{
					}

					if(runInfoInterface == nullptr)
					{
						__SS__ << "Run Info interface plugin construction failed of type " << runInfoPluginType << __E__;
						__SS_THROW__;
					}

					runInfoInterface->updateRunInfo(getNextRunNumber(activeStateMachineName_) - 1, RunInfoVInterface::RunStopType::ERROR);
				}
			}
		}
		catch(const std::runtime_error& e)
		{
			// ERROR
			__SS__ << "RUN INFO INSERT OR UPDATE INTO DATABASE FAILED!!! " << e.what() << __E__;
			__SS_THROW__;
		}
		catch(...)
		{
			// ERROR
			__SS__ << "RUN INFO INSERT OR UPDATE INTO DATABASE FAILED!!! " << __E__;
			try	{ throw; } //one more try to printout extra info
			catch(const std::exception &e)
			{
				ss << "Exception message: " << e.what();
			}
			catch(...){}
			__SS_THROW__;
		}  // End write run info into db
	}      // end update Run Info handling

}  // end inError()

//==============================================================================
void GatewaySupervisor::enteringError(toolbox::Event::Reference e)
{
	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName() << ", Error event type: " << e->type() << __E__;

	// xdaq 15_14_0_3 broke what() by return c_str() on a temporary string
	//  https://gitlab.cern.ch/cmsos/core/-/blob/release_15_14_0_3/xcept/src/common/Exception.cc

	// extract error message and save for user interface access
	toolbox::fsm::FailedEvent& failedEvent     = dynamic_cast<toolbox::fsm::FailedEvent&>(*e);
	xcept::Exception&          failedException = failedEvent.getException();
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
		asyncFailureIdentified = true;
	}
	else
	{
		ss << "\nFailure performing transition from " << failedEvent.getFromState() << "-" << theStateMachine_.getStateName(failedEvent.getFromState())
		   << " to " << failedEvent.getToState() << "-" << theStateMachine_.getStateName(failedEvent.getToState()) << ".\n\nException:\n"
		   << failedException.message() << __E__;  // rbegin()->at("message") << __E__;
		                                           //<< failedEvent.getException().what() << __E__;
	}

	__COUT_ERR__ << "\n" << ss.str();

	theStateMachine_.setErrorMessage(ss.str());

	if(!asyncFailureIdentified && theStateMachine_.getCurrentStateName() == RunControlStateMachine::FAILED_STATE_NAME)
		__COUT__ << "Already in failed state, so not broadcasting Error transition again." << __E__;
	else 	// move everything else to Error!
		broadcastMessage(SOAPUtilities::makeSOAPMessageReference(RunControlStateMachine::ERROR_TRANSITION_NAME));
}  // end enteringError()

//==============================================================================
void GatewaySupervisor::checkForAsyncError()
{
	if(RunControlStateMachine::asyncFailureReceived_)
	{
		__COUTV__(RunControlStateMachine::asyncFailureReceived_);

		XCEPT_RAISE(toolbox::fsm::exception::Exception, RunControlStateMachine::getErrorMessage());
		return;
	}
}  // end checkForAsyncError()

/////////////////////////////////////////////////////////////////////////////////////
/////////////////////////// FSM State Transition Functions //////////////////////////
/////////////////////////////////////////////////////////////////////////////////////

//==============================================================================
void GatewaySupervisor::transitionConfiguring(toolbox::Event::Reference /* e*/)
try
{
	checkForAsyncError();

	RunControlStateMachine::theProgressBar_.step();

	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName() << __E__;

	std::string configurationAlias = SOAPUtilities::translate(theStateMachine_.getCurrentMessage()).getParameters().getValue("ConfigurationAlias");

	__COUT__ << "Transition parameter ConfigurationAlias: " << configurationAlias << __E__;

	RunControlStateMachine::theProgressBar_.step();

	__COUT__ << "Configuration table group name: " << theConfigurationTableGroup_.first << " key: " << theConfigurationTableGroup_.second << __E__;

	// make logbook entry
	{
		std::stringstream ss;
		ss << "Configuring with System Configuration Alias '" << configurationAlias << 
			"' which translates to " << theConfigurationTableGroup_.first << " (" << theConfigurationTableGroup_.second
		   << ").";

		if(getLastLogEntry(RunControlStateMachine::CONFIGURE_TRANSITION_NAME) != "")
			ss << " User log entry:\n " << 
				getLastLogEntry(RunControlStateMachine::CONFIGURE_TRANSITION_NAME);
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
	SOAPParameters parameters;
	parameters.addParameter("ConfigurationTableGroupName", theConfigurationTableGroup_.first);
	parameters.addParameter("ConfigurationTableGroupKey", theConfigurationTableGroup_.second.toString());

	// update Macro Maker front end list
	if(CorePropertySupervisorBase::allSupervisorInfo_.getAllMacroMakerTypeSupervisorInfo().size())
	{
		__COUT__ << "Initializing Macro Maker." << __E__;
		xoap::MessageReference message = SOAPUtilities::makeSOAPMessageReference("FECommunication");

		SOAPParameters parameters;
		parameters.addParameter("type", "initFElist");
		parameters.addParameter("groupName", theConfigurationTableGroup_.first);
		parameters.addParameter("groupKey", theConfigurationTableGroup_.second.toString());
		SOAPUtilities::addParameters(message, parameters);

		__COUT__ << "Sending FE communication: " << SOAPUtilities::translate(message) << __E__;

		std::string reply =
		    SOAPMessenger::send(CorePropertySupervisorBase::allSupervisorInfo_.getAllMacroMakerTypeSupervisorInfo().begin()->second.getDescriptor(), message);

		__COUT__ << "Macro Maker init reply: " << reply << __E__;
		if(reply == "Error")
		{
			__SS__ << "\nTransition to Configuring interrupted! There was an error "
			          "identified initializing Macro Maker.\n\n "
			       << __E__;
			__COUT_ERR__ << "\n" << ss.str();
			XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
			return;
		}
	}  // end update Macro Maker front end list
	RunControlStateMachine::theProgressBar_.step();

	xoap::MessageReference message = theStateMachine_.getCurrentMessage();
	SOAPUtilities::addParameters(message, parameters);
	//Note: Must save configuration dump after this point!! In case there are remote subsystems responding with string
	broadcastMessage(message); // ---------------------------------- broadcast!
	RunControlStateMachine::theProgressBar_.step();


	//check for remote subsystem dumps (after broadcast!)
	std::string remoteSubsystemDump = "";
	{
		std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
		for(auto& remoteGatewayApp : remoteGatewayApps_)
			remoteSubsystemDump += remoteGatewayApp.config_dump;

		if(remoteSubsystemDump.size())
			__COUTV__(remoteSubsystemDump);
	} //end check for remote subsystem dumps
	RunControlStateMachine::theProgressBar_.step();

	if(activeStateMachineConfigurationDumpOnConfigureEnable_)
	{
		//write local configuration dump file
		std::string fullfilename = activeStateMachineConfigurationDumpOnConfigureFilename_ +
			"_" + std::to_string(time(0)) + ".dump";
		FILE* fp = fopen(fullfilename.c_str(), "w");
		if(!fp)
		{
			__SS__ << "Configuration dump failed to file: " << fullfilename << __E__;
			__SS_THROW__;
		}

		//(a la ConfigurationManager::dumpActiveConfiguration)
		fullfilename = __ENV__("HOSTNAME") + std::string(":") + fullfilename;
		fprintf(fp,"Original location of dump:               %s\n",fullfilename.c_str());

		if(activeStateMachineConfigurationDumpOnConfigure_.size())
			fwrite(&activeStateMachineConfigurationDumpOnConfigure_[0], 1, 
				activeStateMachineConfigurationDumpOnConfigure_.size(), fp); 
		__COUT__ << "Wrote configuration dump of char count " << 
			activeStateMachineConfigurationDumpOnConfigure_.size() << 
			" to file: " << fullfilename << __E__;

		if(remoteSubsystemDump.size())
		{
			fwrite(&remoteSubsystemDump[0], 1, 
				remoteSubsystemDump.size(), fp); 

			__COUT__ << "Wrote remote subsystem configuration dump of char count " << 
				remoteSubsystemDump.size() << 
				" to file: " << fullfilename << __E__;
		}
		fclose(fp);

		__COUT_INFO__ << "Configure transition Configuration Dump saved to file: " << fullfilename << __E__;
	} //done with local config dump
	RunControlStateMachine::theProgressBar_.step();

	// Check if Run Plugin is defined and, if so, create a new condition record into database
	// leave as repeated code in case dumpFormat is different for Run Plugin (in the future)
	try
	{						
		if(activeStateMachineRunInfoPluginType_ != TableViewColumnInfo::DATATYPE_STRING_DEFAULT && 
			activeStateMachineRunInfoPluginType_ != "No Run Info Plugin")
		{
			std::unique_ptr<RunInfoVInterface> runInfoInterface = nullptr;
			try	{ runInfoInterface.reset(makeRunInfo(activeStateMachineRunInfoPluginType_, activeStateMachineName_));} catch(...) {;}
			if(runInfoInterface == nullptr)
			{
				__SS__ << "Run Info interface plugin construction failed of type " << activeStateMachineRunInfoPluginType_ << 
					" for inserting Run Condition record of char size " << 
					activeStateMachineConfigurationDumpOnConfigure_.size() << __E__;
				__SS_THROW__;
			}

			conditionID_ = runInfoInterface->insertRunCondition(
				activeStateMachineConfigurationDumpOnConfigure_ + 
				remoteSubsystemDump);
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
		try	{ throw; } //one more try to printout extra info
		catch(const std::exception &e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...){}
		__SS_THROW__;
	}  // End write run condition into db
	RunControlStateMachine::theProgressBar_.step();
		
	// save last configured group name/key
	ConfigurationManager::saveGroupNameAndKey(theConfigurationTableGroup_, FSM_LAST_CONFIGURED_GROUP_ALIAS_FILE);

	makeSystemLogEntry("System configured.");
	__COUT__ << "Done configuring." << __E__;
	RunControlStateMachine::theProgressBar_.complete();
}  // end transitionConfiguring()
catch(const xdaq::exception::Exception& e)  // due to xoap send failure
{
	__SS__ << "\nTransition to Configuring interrupted! There was a system communication error "
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
	try	{ throw; } //one more try to printout extra info
	catch(const std::exception &e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...){}
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}  // end transitionConfiguring() catch

//==============================================================================
void GatewaySupervisor::transitionHalting(toolbox::Event::Reference /*e*/)
try
{
	checkForAsyncError();

	RunControlStateMachine::theProgressBar_.step();

	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName() << __E__;

	makeSystemLogEntry("System halting.");

	RunControlStateMachine::theProgressBar_.step();

	broadcastMessage(theStateMachine_.getCurrentMessage());

	makeSystemLogEntry("System halted.");
	__COUT__ << "Done halting." << __E__;
	RunControlStateMachine::theProgressBar_.complete();
}  // end transitionHalting()
catch(const xdaq::exception::Exception& e)  // due to xoap send failure
{
	__SS__ << "\nTransition to Halting interrupted! There was a system communication error "
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
	try	{ throw; } //one more try to printout extra info
	catch(const std::exception &e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...){}
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}  // end transitionHalting() catch

//==============================================================================
void GatewaySupervisor::transitionShuttingDown(toolbox::Event::Reference /*e*/)
try
{
	checkForAsyncError();

	__COUT__ << "transitionShuttingDown -- Fsm current state: " << theStateMachine_.getCurrentStateName()
	         << " message: " << theStateMachine_.getCurrentStateName() << __E__;

	RunControlStateMachine::theProgressBar_.step();
	makeSystemLogEntry("System shutting down.");
	RunControlStateMachine::theProgressBar_.step();

	// kill all non-gateway contexts
	GatewaySupervisor::launchStartOTSCommand("OTS_APP_SHUTDOWN", CorePropertySupervisorBase::theConfigurationManager_);
	RunControlStateMachine::theProgressBar_.step();

	// important to give time for StartOTS script to recognize command (before user does
	// Startup again)
	for(int i = 0; i < 5; ++i)
	{
		sleep(1);
		RunControlStateMachine::theProgressBar_.step();
	}

	broadcastMessage(theStateMachine_.getCurrentMessage());

	makeSystemLogEntry("System shutdown complete.");
	__COUT__ << "Done shutting down." << __E__;
	RunControlStateMachine::theProgressBar_.complete();
}  // end transitionShuttingDown()
catch(const xdaq::exception::Exception& e)  // due to xoap send failure
{
	__SS__ << "\nTransition to Shutting Down interrupted! There was a system communication error "
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
	try	{ throw; } //one more try to printout extra info
	catch(const std::exception &e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...){}
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}  // end transitionShuttingDown() catch

//==============================================================================
void GatewaySupervisor::transitionStartingUp(toolbox::Event::Reference /*e*/)
try
{
	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName() << __E__;

	RunControlStateMachine::theProgressBar_.step();
	makeSystemLogEntry("System starting up.");
	RunControlStateMachine::theProgressBar_.step();

	// start all non-gateway contexts
	GatewaySupervisor::launchStartOTSCommand("OTS_APP_STARTUP", CorePropertySupervisorBase::theConfigurationManager_);
	RunControlStateMachine::theProgressBar_.step();

	// important to give time for StartOTS script to recognize command and for apps to
	// instantiate things (before user does Initialize)
	for(int i = 0; i < 10; ++i)
	{
		sleep(1);
		RunControlStateMachine::theProgressBar_.step();
	}

	broadcastMessage(theStateMachine_.getCurrentMessage());

	makeSystemLogEntry("System startup complete.");
	__COUT__ << "Done starting up." << __E__;
	RunControlStateMachine::theProgressBar_.complete();

}  // end transitionStartingUp()
catch(const xdaq::exception::Exception& e)  // due to xoap send failure
{
	__SS__ << "\nTransition to Starting Up interrupted! There was a system communication error "
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
	try	{ throw; } //one more try to printout extra info
	catch(const std::exception &e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...){}
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
	__COUT__ << "Fsm current transition: " << theStateMachine_.getCurrentTransitionName(event->type()) << __E__;
	__COUT__ << "Fsm final state: " << theStateMachine_.getTransitionFinalStateName(event->type()) << __E__;

	makeSystemLogEntry("System initialized.");
	__COUT__ << "Done initializing." << __E__;
	RunControlStateMachine::theProgressBar_.complete();

}  // end transitionInitializing()
catch(const xdaq::exception::Exception& e)  // due to xoap send failure
{
	__SS__ << "\nTransition to Initializing interrupted! There was a system communication error "
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
	try	{ throw; } //one more try to printout extra info
	catch(const std::exception &e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...){}
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}  // end transitionInitializing() catch

//==============================================================================
void GatewaySupervisor::transitionPausing(toolbox::Event::Reference /*e*/)
try
{
	checkForAsyncError();

	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName() << __E__;

	// calculate run duration and post system log entry
	{
		int dur = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - activeStateMachineRunStartTime).count() +
		          activeStateMachineRunDuration_ms;
		int dur_s = dur / 1000;
		dur       = dur % 1000;
		int dur_m = dur_s / 60;
		dur_s     = dur_s % 60;
		int dur_h = dur_m / 60;
		dur_m     = dur_m % 60;
		std::ostringstream dur_ss;
		dur_ss << "Run pausing. Duration so far of " << std::setw(2) << std::setfill('0') << dur_h << ":" << std::setw(2) << std::setfill('0') << dur_m << ":"
		       << std::setw(2) << std::setfill('0') << dur_s << "." << dur << " seconds.";

		makeSystemLogEntry(dur_ss.str());
	}

	activeStateMachineRunDuration_ms +=
	    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - activeStateMachineRunStartTime).count();

	// the current message is not for Pause if its due to async exception, so rename
	if(RunControlStateMachine::asyncPauseExceptionReceived_)
	{
		__COUT_ERR__ << "Broadcasting pause for async PAUSE exception!" << __E__;
		broadcastMessage(SOAPUtilities::makeSOAPMessageReference("Pause"));
	}
	else
		broadcastMessage(theStateMachine_.getCurrentMessage());

	makeSystemLogEntry("Run paused.");
	__COUT__ << "Done pausing." << __E__;
	RunControlStateMachine::theProgressBar_.complete();

}  // end transitionPausing()
catch(const xdaq::exception::Exception& e)  // due to xoap send failure
{
	__SS__ << "\nTransition to Pausing interrupted! There was a system communication error "
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
	try	{ throw; } //one more try to printout extra info
	catch(const std::exception &e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...){}
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}  // end transitionPausing() catch

//==============================================================================
void GatewaySupervisor::transitionResuming(toolbox::Event::Reference /*e*/)
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

	makeSystemLogEntry("Run resuming.");

	activeStateMachineRunStartTime = std::chrono::steady_clock::now();

	broadcastMessage(theStateMachine_.getCurrentMessage());

	__COUT__ << "Done resuming." << __E__;
	RunControlStateMachine::theProgressBar_.complete();
}  // end transitionResuming()
catch(const xdaq::exception::Exception& e)  // due to xoap send failure
{
	__SS__ << "\nTransition to Resuming interrupted! There was a system communication error "
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
	try	{ throw; } //one more try to printout extra info
	catch(const std::exception &e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...){}
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}  // end transitionResuming() catch

//==============================================================================
void GatewaySupervisor::transitionStarting(toolbox::Event::Reference /*e*/)
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

	// make logbook entry
	{
		std::stringstream ss;
		ss << "Run '" << activeStateMachineRunNumber_ << "' starting.";

		if(getLastLogEntry(RunControlStateMachine::START_TRANSITION_NAME) != "")
			ss << " User log entry:\n " << 
					getLastLogEntry(RunControlStateMachine::START_TRANSITION_NAME);
		else 
			ss << " No user log entry.";

		makeSystemLogEntry(ss.str());
	}  // end make logbook entry
	RunControlStateMachine::theProgressBar_.step();


	activeStateMachineRunStartTime   = std::chrono::steady_clock::now();
	activeStateMachineRunDuration_ms = 0;
	broadcastMessage(theStateMachine_.getCurrentMessage()); // ---------------------------------- broadcast!
	RunControlStateMachine::theProgressBar_.step();


	//check for remote subsystem dumps (after broadcast!)
	std::string remoteSubsystemDump = "";
	{
		std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
		for(auto& remoteGatewayApp : remoteGatewayApps_)
			remoteSubsystemDump += remoteGatewayApp.config_dump;

		if(remoteSubsystemDump.size())
			__COUTV__(remoteSubsystemDump);
	} //end check for remote subsystem dumps
	RunControlStateMachine::theProgressBar_.step();
	
	if(activeStateMachineConfigurationDumpOnRunEnable_)
	{
		//write local configuration dump file
		std::string fullfilename = activeStateMachineConfigurationDumpOnRunFilename_ +
			"_" + std::to_string(time(0)) + ".dump";
		FILE* fp = fopen(fullfilename.c_str(), "w");
		if(!fp)
		{
			__SS__ << "Configuration dump failed to file: " << fullfilename << __E__;
			__SS_THROW__;
		}

		//(a la ConfigurationManager::dumpActiveConfiguration)
		fullfilename = __ENV__("HOSTNAME") + std::string(":") + fullfilename;
		fprintf(fp,"Original location of dump:               %s\n", fullfilename.c_str());

		if(activeStateMachineConfigurationDumpOnRun_.size())
			fwrite(&activeStateMachineConfigurationDumpOnRun_[0], 1, 
				activeStateMachineConfigurationDumpOnRun_.size(), fp); 
		__COUT__ << "Wrote configuration dump of char count " << 
			activeStateMachineConfigurationDumpOnRun_.size() << 
			" to file: " << fullfilename << __E__;

		if(remoteSubsystemDump.size())
		{
			fwrite(&remoteSubsystemDump[0], 1, 
				remoteSubsystemDump.size(), fp); 

			__COUT__ << "Wrote remote subsystem configuration dump of char count " << 
				remoteSubsystemDump.size() << 
				" to file: " << fullfilename << __E__;
		}
		fclose(fp);

		__COUT_INFO__ << "Run transition Configuration Dump saved to file: " << fullfilename << __E__;
	} //done with local config dump
	RunControlStateMachine::theProgressBar_.step();


	// save last started group name/key
	ConfigurationManager::saveGroupNameAndKey(theConfigurationTableGroup_, FSM_LAST_STARTED_GROUP_ALIAS_FILE);

	makeSystemLogEntry("Run started.");
	__COUT__ << "Done starting run." << __E__;
	RunControlStateMachine::theProgressBar_.complete();

}  // end transitionStarting()
catch(const xdaq::exception::Exception& e)  // due to xoap send failure
{
	__SS__ << "\nTransition to Starting Run interrupted! There was a system communication error "
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
	try	{ throw; } //one more try to printout extra info
	catch(const std::exception &e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...){}
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}  // end transitionStarting() catch

//==============================================================================
void GatewaySupervisor::transitionStopping(toolbox::Event::Reference /*e*/)
try
{
	checkForAsyncError();

	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName() << __E__;

	activeStateMachineRunDuration_ms +=
	    std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - activeStateMachineRunStartTime).count();

	RunControlStateMachine::theProgressBar_.step();

	// calculate run duration and make system log entry
	{
		int dur   = activeStateMachineRunDuration_ms;
		int dur_s = dur / 1000;
		dur       = dur % 1000;
		int dur_m = dur_s / 60;
		dur_s     = dur_s % 60;
		int dur_h = dur_m / 60;
		dur_m     = dur_m % 60;
		std::ostringstream dur_ss;
		dur_ss << "Run stopping. Duration of " << std::setw(2) << std::setfill('0') << dur_h << ":" << std::setw(2) << std::setfill('0') << dur_m << ":"
		       << std::setw(2) << std::setfill('0') << dur_s << "." << dur << " seconds.";

		makeSystemLogEntry(dur_ss.str());
	}

	// the current message is not for Stop if its due to async exception, so rename
	if(RunControlStateMachine::asyncStopExceptionReceived_)
	{
		__COUT_ERR__ << "Broadcasting stop for async STOP exception!" << __E__;
		broadcastMessage(SOAPUtilities::makeSOAPMessageReference("Stop"));
	}
	else
		broadcastMessage(theStateMachine_.getCurrentMessage());

	makeSystemLogEntry("Run stopped.");
	__COUT__ << "Done stopping run." << __E__;
	RunControlStateMachine::theProgressBar_.complete();
}  // end transitionStopping()
catch(const xdaq::exception::Exception& e)  // due to xoap send failure
{
	__SS__ << "\nTransition to Stopping Run interrupted! There was a system communication error "
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
	try	{ throw; } //one more try to printout extra info
	catch(const std::exception &e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...){}
	__COUT_ERR__ << "\n" << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}  // end transitionStopping() catch

////////////////////////////////////////////////////////////////////////////////////////////
//////////////      MESSAGES ///////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////

//==============================================================================
// handleBroadcastMessageTarget
//	Sends message and gets reply
//	Handles sub-iterations at same target
//		if failure, THROW state machine exception
//	returns true if iterations are done, else false
bool GatewaySupervisor::handleBroadcastMessageTarget(const SupervisorInfo&  appInfo,
                                                     xoap::MessageReference message,
                                                     const std::string&     command,
                                                     const unsigned int&    iteration,
                                                     std::string&           reply,
                                                     unsigned int           threadIndex)
try
{
	unsigned int subIteration      = 0;  // reset for next subIteration loop
	bool         subIterationsDone = false;
	bool         iterationsDone    = true;

	while(!subIterationsDone)  // start subIteration handling loop
	{
		__COUT__ << "Broadcast thread " << threadIndex << "\t"
		         << "Supervisor instance = '" << appInfo.getName() << "' [LID=" << appInfo.getId() << "] in Context '" << appInfo.getContextName()
		         << "' [URL=" << appInfo.getURL() << "] Command = " << command << __E__;

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
			         << "Adding iteration parameters " << iteration << "." << subIteration << __E__;

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

		if(iteration == 0 && subIteration == 0) //first time through the supervisors
		{
			__COUT__ << "Broadcast thread " << threadIndex << "\t"
						<< "Sending message to Supervisor " << appInfo.getName() << " [LID=" << appInfo.getId() << "]: " << command << __E__;

			givenAppDetail = "";
		}
		else  // else this not the first time through the supervisors
		{
			if(givenAppDetail == "")
				givenAppDetail = std::to_string(iteration) + ":" + std::to_string(subIteration);
			if(subIteration == 0)
			{
				for(unsigned int j = 0; j < 4; ++j)
					__COUT__ << "Broadcast thread " << threadIndex << "\t"
					         << "Sending message to Supervisor " << appInfo.getName() << " [LID=" << appInfo.getId() << "]: " << command
					         << " (iteration: " << iteration << ")" << __E__;
			}
			else
			{
				for(unsigned int j = 0; j < 4; ++j)
					__COUT__ << "Broadcast thread " << threadIndex << "\t"
					         << "Sending message to Supervisor " << appInfo.getName() << " [LID=" << appInfo.getId() << "]: " << command
					         << " (iteration: " << iteration << ", sub-iteration: " << subIteration << ")" << __E__;
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
			__COUT__ << "Broadcast thread " << threadIndex << "\t givenAppStatus=" << givenAppStatus << __E__;
			__COUT__ << "Broadcast thread " << threadIndex << "\t appInfo.getStatus()=" << appInfo.getStatus() << __E__;

			// wait for app to exist in status before sending commands
			int waitAttempts = 0;
			while(appInfo.getStatus() == SupervisorInfo::APP_STATUS_UNKNOWN)
			{
				__COUT__ << "Broadcast thread " << threadIndex << "\t"
				         << "Waiting for Supervisor " << appInfo.getName() << " [LID=" << appInfo.getId() << "] in unknown state. waitAttempts of 10 = " << waitAttempts << __E__;
				++waitAttempts;
				if(waitAttempts == 10)
				{
					__SS__ << "Error! Gateway Supervisor failed to send message to app in unknown state "
					          "Supervisor instance = '"
					       << appInfo.getName() << "' [LID=" << appInfo.getId() << "] in Context '" << appInfo.getContextName() << "' [URL=" << appInfo.getURL()
					       << "].\n\n";
					__COUT_ERR__ << ss.str();
					XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
				}
				sleep(2);
			}

			// start recursive mutex scope (same thread can lock multiple times, but needs to unlock the same)
			std::lock_guard<std::recursive_mutex> lock(allSupervisorInfo_.getSupervisorInfoMutex(appInfo.getId()));
			// set app status, but leave progress and detail alone
			allSupervisorInfo_.setSupervisorStatus(appInfo, givenAppStatus, givenAppProgress, givenAppDetail);

			// for transition attempt, set status for app, in case the request occupies the target app			
			std::string tmpReply = send(appInfo.getDescriptor(), message);
			__COUTV__(tmpReply);
			//using the intermediate temporary string seems to possibly help when there are multiple crashes of FSM entities			
			reply = tmpReply;
			
			// then release mutex here using scope change, to allow the app to start giving its own updates			
		}
		catch(const xdaq::exception::Exception& e)  // due to xoap send failure
		{
			// do not kill whole system if xdaq xoap failure
			__SS__ << "Error! Gateway Supervisor can NOT " << command << " Supervisor instance = '" << appInfo.getName() << "' [LID=" << appInfo.getId()
			       << "] in Context '" << appInfo.getContextName() << "' [URL=" << appInfo.getURL() << "].\n\n"
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
						std::lock_guard<std::mutex> lock(broadcastCommandMessageIndexMutex_);
						parameters.addParameter("commandId", broadcastCommandMessageIndex_++);
					}  // end mutex scope
					SOAPUtilities::addParameters(message, parameters);
				}

				__COUT__ << "Broadcast thread " << threadIndex << "\t"
				         << "Re-Sending... " << SOAPUtilities::translate(message) << std::endl;

				reply = send(appInfo.getDescriptor(), message);
			}
			catch(const xdaq::exception::Exception& e)  // due to xoap send failure
			{
				__COUT_ERR__ << "Broadcast thread " << threadIndex << "\t"
				             << "Second try failed.." << __E__;
				XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
			}
			__COUT__ << "Broadcast thread " << threadIndex << "\t"
			         << "2nd try passed.." << __E__;
		}  // end send catch

		__COUT__ << "Broadcast thread " << threadIndex << "\t"
		         << "Reply received from " << appInfo.getName() << " [LID=" << appInfo.getId() << "]: " << reply << __E__;

		if((reply != command + "Done") && (reply != command + "Response") && (reply != command + "Iterate") && (reply != command + "SubIterate"))
		{
			__SS__ << "Error! Gateway Supervisor can NOT " << command << " Supervisor instance = '" << appInfo.getName() << "' [LID=" << appInfo.getId()
			       << "] in Context '" << appInfo.getContextName() << "' [URL=" << appInfo.getURL() << "].\n\n"
			       << reply;
			__COUT_ERR__ << ss.str() << __E__;

			__COUT__ << "Broadcast thread " << threadIndex << "\t"
			         << "Getting error message..." << __E__;
			try
			{
				xoap::MessageReference errorMessage =
				    sendWithSOAPReply(appInfo.getDescriptor(), SOAPUtilities::makeSOAPMessageReference("StateMachineErrorMessageRequest"));
				SOAPParameters parameters;
				parameters.addParameter("ErrorMessage");
				SOAPUtilities::receive(errorMessage, parameters);

				std::string error = parameters.getValue("ErrorMessage");
				if(error == "")
				{
					std::stringstream err;
					err << "Unknown error from Supervisor instance = '" << appInfo.getName() << "' [LID=" << appInfo.getId() << "] in Context '"
					    << appInfo.getContextName() << "' [URL=" << appInfo.getURL()
					    << "]. If the problem persists or is repeatable, please notify "
					       "admins.\n\n";
					error = err.str();
				}

				__SS__ << "Received error message from Supervisor instance = '" << appInfo.getName() << "' [LID=" << appInfo.getId() << "] in Context '"
				       << appInfo.getContextName() << "' [URL=" << appInfo.getURL() << "].\n\n Error Message = " << error << __E__;

				__COUT_ERR__ << ss.str() << __E__;

				if(command == RunControlStateMachine::ERROR_TRANSITION_NAME)
					return true;  // do not throw exception and exit loop if informing all
					              // apps about error
				// else throw exception and go into Error
				XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
			}
			catch(const xdaq::exception::Exception& e)  // due to xoap send failure
			{
				// do not kill whole system if xdaq xoap failure
				__SS__ << "Error! Gateway Supervisor failed to read error message from "
				          "Supervisor instance = '"
				       << appInfo.getName() << "' [LID=" << appInfo.getId() << "] in Context '" << appInfo.getContextName() << "' [URL=" << appInfo.getURL()
				       << "].\n\n"
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
			         << "Supervisor instance = '" << appInfo.getName() << "' [LID=" << appInfo.getId() << "] in Context '" << appInfo.getContextName()
			         << "' [URL=" << appInfo.getURL() << "] flagged for another iteration to " << command << "... (iteration: " << iteration << ")" << __E__;

		}  // end still working response handling
		else if(reply == command + "SubIterate")
		{
			// when 'Working' this front-end is expecting
			//	to get the same command again with an incremented sub-iteration index
			//	without any other front-ends taking actions or seeing the sub-iteration
			// index.

			subIterationsDone = false;
			__COUT__ << "Broadcast thread " << threadIndex << "\t"
			         << "Supervisor instance = '" << appInfo.getName() << "' [LID=" << appInfo.getId() << "] in Context '" << appInfo.getContextName()
			         << "' [URL=" << appInfo.getURL() << "] flagged for another sub-iteration to " << command << "... (iteration: " << iteration
			         << ", sub-iteration: " << subIteration << ")" << __E__;
		}
		else  // else success response
		{
			__COUT__ << "Broadcast thread " << threadIndex << "\t"
			         << "Supervisor instance = '" << appInfo.getName() << "' [LID=" << appInfo.getId() << "] in Context '" << appInfo.getContextName()
			         << "' [URL=" << appInfo.getURL() << "] was " << command << "'d correctly!" << __E__;
		}

		if(subIteration)
			__COUT__ << "Broadcast thread " << threadIndex << "\t"
			         << "Completed sub-iteration: " << subIteration << __E__;
		++subIteration;

	}  // end subIteration handling loop

	return iterationsDone;

}  // end handleBroadcastMessageTarget()
catch(const toolbox::fsm::exception::Exception& e) {throw;} //keep existing FSM execptions intact
catch(...)
{
	// do not kill whole system if unexpected exception
	__SS__ << "Error! Gateway Supervisor failed to broadcast message '" << command << "' to "
				"Supervisor instance = '"
			<< appInfo.getName() << "' [LID=" << appInfo.getId() << "] in Context '" << appInfo.getContextName() << "' [URL=" << appInfo.getURL()
			<< "]. Try re-initializing or restarting otsdaq."
			<< __E__;
	__COUT_ERR__ << ss.str();
	XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
}

//==============================================================================
// broadcastMessageThread
//	Sends transition command message and gets reply
//		if failure, THROW
void GatewaySupervisor::broadcastMessageThread(GatewaySupervisor* supervisorPtr, std::shared_ptr<GatewaySupervisor::BroadcastThreadStruct> threadStruct)
{
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
			         << "starting work... command = " << threadStruct->getCommand() << __E__;

			try
			{
				if(supervisorPtr->handleBroadcastMessageTarget(threadStruct->getAppInfo(),
				                                               threadStruct->getMessage(),
				                                               threadStruct->getCommand(),
				                                               threadStruct->getIteration(),
				                                               threadStruct->getReply(),
				                                               threadStruct->threadIndex_))
					threadStruct->getIterationsDone() = true;
			}
			catch(const toolbox::fsm::exception::Exception& e)
			{
				__COUT__ << "Broadcast thread " << threadStruct->threadIndex_ << "\t"
				         << "going into error: " << e.what() << __E__;

				threadStruct->getReply() = e.what();
				threadStruct->error_     = true;
				threadStruct->workToDo_  = false;
				threadStruct->working_   = false;  // indicate exiting
				return;
			}

			if(!threadStruct->getIterationsDone())
			{
				__COUT__ << "Broadcast thread " << threadStruct->threadIndex_ << "\t"
				         << "flagged for another iteration." << __E__;

				// set global iterationsDone
				std::lock_guard<std::mutex> lock(supervisorPtr->broadcastIterationsDoneMutex_);
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
// broadcastMessage
//	Broadcast state transition to all xdaq Supervisors and remote Gateway Supervisors.
//		- Transition in order of, remote Gateways first, then priority as given by AllSupervisorInfo
//	Update Supervisor Info based on result of transition.
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
		    command == RunControlStateMachine::SHUTDOWN_TRANSITION_NAME || command == RunControlStateMachine::STARTUP_TRANSITION_NAME);
	}
	catch(const std::runtime_error& e)
	{
		__SS__ << "Error getting supervisor priority. Was there a change in the context?"
		       << " Remember, if the context was changed, it is recommended to relaunch the "
		          "ots script. "
		       << e.what() << __E__;
		XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
	}

	RunControlStateMachine::theProgressBar_.step();

	// std::vector<std::vector<uint8_t/*bool*/>> supervisorIterationsDone; //Note: can not
	// use bool because std::vector does not allow access by reference of type bool
	GatewaySupervisor::BroadcastMessageIterationsDoneStruct supervisorIterationsDone;

	// initialize to false (not done)
	for(const auto& vectorAtPriority : orderedSupervisors)
		supervisorIterationsDone.push(vectorAtPriority.size());  // push_back(
		                                                         // std::vector<uint8_t>(vectorAtPriority.size(),
		                                                         // false /*initial value*/));

	unsigned int iteration = 0;
	// unsigned int subIteration;
	unsigned int iterationBreakpoint;

	// send command to all supervisors (for multiple iterations) until all are done

	// make a copy of the message to use as starting point for iterations
	xoap::MessageReference originalMessage = SOAPUtilities::makeSOAPMessageReference(SOAPUtilities::translate(message));

	__COUT__ << "=========> Broadcasting state machine command = " << command << __E__;

	unsigned int numberOfThreads = 1;

	try
	{
		numberOfThreads = CorePropertySupervisorBase::getSupervisorTableNode().getNode("NumberOfStateMachineBroadcastThreads").getValue<unsigned int>();
	}
	catch(...)
	{
		// ignore error for backwards compatibility
		__COUT__ << "Number of threads not in configuration, so defaulting to " << numberOfThreads << __E__;
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
		broadcastThreadStructs_.push_back(std::make_shared<GatewaySupervisor::BroadcastThreadStruct>());
		broadcastThreadStructs_[i]->threadIndex_ = i;

		std::thread([](GatewaySupervisor*                        supervisorPtr,
		               std::shared_ptr<GatewaySupervisor::BroadcastThreadStruct> threadStruct) { 
						GatewaySupervisor::broadcastMessageThread(supervisorPtr, threadStruct); },
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
			broadcastIterationsDone_ = true;

			{  // start mutex scope
				std::lock_guard<std::mutex> lock(broadcastIterationBreakpointMutex_);
				iterationBreakpoint = broadcastIterationBreakpoint_;  // get breakpoint
			}                                                         // end mutex scope

			if(iterationBreakpoint < (unsigned int)-1)
				__COUT__ << "Iteration breakpoint currently is " << iterationBreakpoint << __E__;
			if(iteration >= iterationBreakpoint)
			{
				broadcastIterationsDone_ = false;
				__COUT__ << "Waiting at transition breakpoint - iteration = " << iteration << __E__;
				usleep(5 * 1000 * 1000 /*5 s*/);
				continue;  // wait until breakpoint moved
			}

			if(iteration)
				__COUT__ << "Starting iteration: " << iteration << __E__;

			for(unsigned int i = 0; i < supervisorIterationsDone.size(); ++i)
			{
				for(unsigned int j = 0; j < supervisorIterationsDone.size(i); ++j)
				{
					checkForAsyncError();

					if(supervisorIterationsDone[i][j])
						continue;  // skip if supervisor is already done

					const SupervisorInfo& appInfo = *(orderedSupervisors[i][j]);

					// re-acquire original message
					message = SOAPUtilities::makeSOAPMessageReference(SOAPUtilities::translate(originalMessage));

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
									__COUT__ << "Giving work to thread " << k << ", command = " << command << __E__;

									std::lock_guard<std::mutex> lock(broadcastThreadStructs_[k]->threadMutex_);
									broadcastThreadStructs_[k]->setMessage(appInfo, message, command, iteration, supervisorIterationsDone[i][j]);

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
						if(handleBroadcastMessageTarget(appInfo, message, command, iteration, reply))
							supervisorIterationsDone[i][j] = true;
						else
							broadcastIterationsDone_ = false;
					}

				}  // end supervisors at same priority broadcast loop

				// before proceeding to next priority,
				//	make sure all threads have completed
				if(numberOfThreads)
				{
					__COUT__ << "Done with priority level. Waiting for threads to finish..." << __E__;
					bool done;
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
								         << broadcastThreadStructs_[i]->getReply() << __E__;
								XCEPT_RAISE(toolbox::fsm::exception::Exception, broadcastThreadStructs_[i]->getReply());
							}

						if(!done)  // update status and sleep
						{
							std::stringstream waitSs;
							waitSs << "Waiting on " << numOfThreadsWithWork << " of " << numberOfThreads << " threads to finish. Command = " << command;
							if(command == RunControlStateMachine::CONFIGURE_TRANSITION_NAME)
								waitSs << " w/" + RunControlStateMachine::getLastAttemptedConfigureGroup();
							if(numOfThreadsWithWork == 1)
							{
								waitSs << ".. " << broadcastThreadStructs_[lastUnfinishedThread]->getAppInfo().getName() << ":"
								       << broadcastThreadStructs_[lastUnfinishedThread]->getAppInfo().getId();
							}
							waitSs << __E__;
							__COUT__ << waitSs.str();

							{  // create lock scope that does not include sleep
								std::lock_guard<std::mutex> lock(broadcastCommandStatusUpdateMutex_);
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

			if(iteration || !broadcastIterationsDone_)
				__COUT__ << "Completed iteration: " << iteration << __E__;
			++iteration;

		} while(!broadcastIterationsDone_);

		RunControlStateMachine::theProgressBar_.step();
	}  // end main transition broadcast try
	catch(...)
	{
		__COUT__ << "Exception caught, exiting broadcast threads..." << __E__;

		// attempt to exit threads
		//	The threads should already be done with all work.
		//	If broadcastMessage scope ends, then the
		//	thread struct will be destructed, and the thread will
		//	crash on next access attempt (though we probably do not care).
		for(unsigned int i = 0; i < numberOfThreads; ++i)
			broadcastThreadStructs_[i]->exitThread_ = true;
		usleep(100 * 1000 /*100ms*/);  // sleep for exit time

		throw;  // re-throw
	}

	if(numberOfThreads)
	{
		__COUT__ << "All transitions completed. Wrapping up, exiting broadcast threads..." << __E__;

		// attempt to exit threads
		//	The threads should already be done with all work.
		//	If broadcastMessage scope ends, then the
		//	thread struct will be destructed, and the thread will
		//	crash on next access attempt (when the thread crashes, the whole context
		// crashes).
		for(unsigned int i = 0; i < numberOfThreads; ++i)
			broadcastThreadStructs_[i]->exitThread_ = true;
		usleep(100 * 1000 /*100ms*/);  // sleep for exit time
	}


	RunControlStateMachine::theProgressBar_.step();

	if(broadcastMessageToRemoteGatewaysComplete(originalMessage))
	{
		RunControlStateMachine::theProgressBar_.step();
		__COUT__ << "Broadcast complete." << __E__;
	}
}  // end broadcastMessage()

//==============================================================================
void GatewaySupervisor::broadcastMessageToRemoteGateways(const xoap::MessageReference message)
{
	SOAPCommand commandObj = SOAPUtilities::translate(message);
	std::string command = commandObj.getCommand();
	__COUTV__(command);
	

	std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
	for(auto& remoteGatewayApp : remoteGatewayApps_)
	{
		//construct command params based on remote gateway settings
		std::string commandAndParams = command;
		if(commandObj.hasParameters())
		{
			//parameters over UDP are much simpler than xoap message, so filter
			for(const auto& param : commandObj.getParameters())
			{
				__COUTTV__(param.first);
				__COUTTV__(param.second);
				if(param.first == "ConfigurationAlias")
				{						
					if(remoteGatewayApp.selected_config_alias != "") //replace 
						commandAndParams += "," + remoteGatewayApp.selected_config_alias;						 
					else
						commandAndParams += "," + param.second;
				}
				else if(param.first == "RunNumber")
				{	
					commandAndParams += "," + //param.first + ":" + 
							param.second;
				}

				// else
				// 	commandAndParams += "," + param.first + ":" + param.second;
			}
			__COUTV__(commandAndParams);
		}

		if(!remoteGatewayApp.fsm_included)
		{
			__COUT__ << "Skipping excluded Remote gateway '" << 
				remoteGatewayApp.appInfo.name << "' for FSM command = " << commandAndParams << __E__;
			continue; //skip if not included
		}

		if(remoteGatewayApp.fsm_mode == RemoteGatewayInfo::FSM_ModeTypes::DoNotHalt && 
			//do not allow halt/err transitions:
			(command == RunControlStateMachine::ERROR_TRANSITION_NAME || 
			command == RunControlStateMachine::FAIL_TRANSITION_NAME || 
			command == RunControlStateMachine::HALT_TRANSITION_NAME ||
			command == RunControlStateMachine::ABORT_TRANSITION_NAME ))
		{
			__COUT__ << "Skipping '" << remoteGatewayApp.getFsmMode() << "' Remote gateway '" << 
				remoteGatewayApp.appInfo.name << "' for FSM command = " << commandAndParams << __E__;
			continue; //skip if not included
		}

		if(remoteGatewayApp.fsm_mode == RemoteGatewayInfo::FSM_ModeTypes::OnlyConfigure && 
			!  //invert of allowed situations:
			(remoteGatewayApp.appInfo.status == RunControlStateMachine::INITIAL_STATE_NAME ||
			remoteGatewayApp.appInfo.status == RunControlStateMachine::HALTED_STATE_NAME ||			
			remoteGatewayApp.appInfo.status.find(RunControlStateMachine::FAILED_STATE_NAME) == 0 ||
			remoteGatewayApp.appInfo.status.find("Error") != std::string::npos || //	case "Error", "Soft-Error"									
			(remoteGatewayApp.appInfo.status == RunControlStateMachine::HALTED_STATE_NAME && 
				command == RunControlStateMachine::CONFIGURE_TRANSITION_NAME)))
		{
			__COUT__ << "Skipping '" << remoteGatewayApp.getFsmMode() << "' Remote gateway '" << 
				remoteGatewayApp.appInfo.name << "' w/status = " << remoteGatewayApp.appInfo.status <<
				"... for FSM command = " << commandAndParams << __E__;
			continue; //skip if not included
		}

		__COUT__ << "Launching FSM command '" << commandAndParams << "' on Remote gateway '" << 
			remoteGatewayApp.appInfo.name << "'..." << __E__;

		if(remoteGatewayApp.command != "")
		{
			__SUP_SS__ << "Can not target the remote subsystem '" << remoteGatewayApp.appInfo.name << 
				"' with command '" << command << "' which already has a pending command '"
				<< remoteGatewayApp.command << ".' Please try again after the pending command is sent." << __E__;
			__SUP_SS_THROW__;					
		}

		remoteGatewayApp.config_dump = ""; //clear, must come from new command completion
		remoteGatewayApp.command = commandAndParams;
		
		std::string logEntry = getLastLogEntry(command);
		if(logEntry.size())
			remoteGatewayApp.command += ",LogEntry:" + StringMacros::encodeURIComponent(logEntry);
		remoteGatewayApp.fsmName = activeStateMachineName_; //fsmName will be prepended during command send
		//force status for immediate user feedback
		remoteGatewayApp.appInfo.status = "Launching " + commandAndParams;
		remoteGatewayApp.appInfo.progress = 0;
	}
}  // end broadcastMessageToRemoteGateways()

//==============================================================================
bool GatewaySupervisor::broadcastMessageToRemoteGatewaysComplete(const xoap::MessageReference message)
{
	std::string command = SOAPUtilities::translate(message).getCommand();
	__COUTV__(command);

	bool done = command == "Error"; //dont check for done if Error'ing
	while(!done)
	{		
		__COUT__ << "Checking " << remoteGatewayApps_.size() << " remote gateway(s) completion for command = " <<
			command << __E__;

		done = true;
		std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
		for(auto& remoteGatewayApp : remoteGatewayApps_)
		{
			//skip remote gateways that were not commanded
			if(!remoteGatewayApp.fsm_included) continue;
			if(remoteGatewayApp.fsm_mode == RemoteGatewayInfo::FSM_ModeTypes::DoNotHalt && 
				//do not allow halt/err transitions:
				(command == RunControlStateMachine::ERROR_TRANSITION_NAME || 
				command == RunControlStateMachine::FAIL_TRANSITION_NAME || 
				command == RunControlStateMachine::HALT_TRANSITION_NAME ||
				command == RunControlStateMachine::ABORT_TRANSITION_NAME )) continue;
			if(remoteGatewayApp.fsm_mode == RemoteGatewayInfo::FSM_ModeTypes::OnlyConfigure && 
				!  //invert of allowed situations:
				(remoteGatewayApp.appInfo.status == RunControlStateMachine::INITIAL_STATE_NAME ||
				remoteGatewayApp.appInfo.status == RunControlStateMachine::HALTED_STATE_NAME ||			
				remoteGatewayApp.appInfo.status.find(RunControlStateMachine::FAILED_STATE_NAME) == 0 ||
				remoteGatewayApp.appInfo.status.find("Error") != std::string::npos || //	case "Error", "Soft-Error"									
				(remoteGatewayApp.appInfo.status == RunControlStateMachine::HALTED_STATE_NAME && 
					command == RunControlStateMachine::CONFIGURE_TRANSITION_NAME))) continue;	
			//if here, was commanded, so check status

			if(!(				
				remoteGatewayApp.appInfo.progress == 100 || 
				remoteGatewayApp.appInfo.status.find("Error") != std::string::npos ||
				remoteGatewayApp.appInfo.status.find("Fail") != std::string::npos				
				))
			{
				//not done
				if(remoteGatewayApp.appInfo.status == SupervisorInfo::APP_STATUS_UNKNOWN)
				{
					__SS__ <<  "Can not complete FSM command '" << command << "' with unknown status from Remote gateway '" << 
						remoteGatewayApp.appInfo.name << "' - it seems communication was lost. Please check the connection or notify admins." << __E__;						  
					__SS_THROW__;
				}
				__COUT__ << "Remote gateway '" << remoteGatewayApp.appInfo.name << "' not done w/command '" <<
					command << "' status = " << remoteGatewayApp.appInfo.status <<
					",... progress = " << remoteGatewayApp.appInfo.progress << __E__;

				done = false;
			}
			else 
			{
				//done
				__COUTT__ << "Done Remote gateway '" << remoteGatewayApp.appInfo.name << "' w/command '" <<
					command << "' status = " << remoteGatewayApp.appInfo.status <<
					",... progress = " << remoteGatewayApp.appInfo.progress << __E__;
			}

		}
		if(!done) sleep(2);

		checkForAsyncError();
	}

	__COUT__ << "Done with " << remoteGatewayApps_.size() << " remote gateway(s) command = " <<
		command << __E__;

	return true;
}  // end broadcastMessageToRemoteGatewaysComplete()

//==============================================================================
// LoginRequest
//  handles all users login/logout actions from web GUI.
//  NOTE: there are two ways for a user to be logged out: timeout or manual logout
//      System logbook messages are generated for login and logout
void GatewaySupervisor::loginRequest(xgi::Input* in, xgi::Output* out)
{
	std::chrono::steady_clock::time_point startClock = std::chrono::steady_clock::now();
	cgicc::Cgicc cgi(in);
	std::string  Command = CgiDataUtilities::getData(cgi, "RequestType");
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
		for(unsigned int i = 0; i < loggedOutUsernames.size(); ++i)  // Log logout for logged out users
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

			std::string sid = theWebUsers_.createNewLoginSession(uuid, cgi.getEnvironment().getRemoteAddr() /* ip */);

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
			uid = theWebUsers_.isCookieCodeActiveForLogin(uuid, cookieCode,
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

			uint64_t uid = theWebUsers_.attemptActiveSession(uuid,
			                                                 jumbledUser,
			                                                 jumbledPw,
			                                                 newAccountCode,
			                                                 cgi.getEnvironment().getRemoteAddr());  // after call jumbledUser holds displayName on success

			if(uid >= theWebUsers_.ACCOUNT_ERROR_THRESHOLD)
			{
				__COUT__ << "Login invalid." << __E__;
				jumbledUser = "";          // clear display name if failure
				if(newAccountCode != "1")  // indicates uuid not found
					newAccountCode = "0";  // clear cookie code if failure
			}
			else  // Log login in logbook for active experiment
				makeSystemLogEntry(theWebUsers_.getUsersUsername(uid) + " logged in.");

			//__COUT__ << "new cookieCode = " << newAccountCode.substr(0, 10) << __E__;

			HttpXmlDocument xmldoc(newAccountCode, jumbledUser);

			// include extra error detail
			if(uid == theWebUsers_.ACCOUNT_INACTIVE)
				xmldoc.addTextElementToData("Error", "Account is inactive. Notify admins.");
			else if(uid == theWebUsers_.ACCOUNT_BLACKLISTED)
				xmldoc.addTextElementToData("Error", "Account is blacklisted. Notify admins.");

			theWebUsers_.insertSettingsForUser(uid, &xmldoc);  // insert settings

			// insert active session count for user

			if(uid != theWebUsers_.NOT_FOUND_IN_DATABASE)
			{
				uint64_t asCnt = theWebUsers_.getActiveSessionCountForUser(uid) - 1;  // subtract 1 to remove just started session from count
				char     asStr[20];
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

			std::string uuid         = CgiDataUtilities::postData(cgi, "uuid");
			std::string jumbledEmail = cgicc::form_urldecode(CgiDataUtilities::getData(cgi, "httpsUser"));
			std::string username     = "";
			std::string cookieCode   = "";

			//		__COUT__ << "CERTIFICATE LOGIN REUEST RECEVIED!!!" << __E__;
			//		__COUT__ << "jumbledEmail = " << jumbledEmail << __E__;
			//		__COUT__ << "uuid = " << uuid << __E__;

			uint64_t uid =
			    theWebUsers_.attemptActiveSessionWithCert(uuid,
			                                              jumbledEmail,
			                                              cookieCode,
			                                              username,
			                                              cgi.getEnvironment().getRemoteAddr());  // after call jumbledUser holds displayName on success

			if(uid == theWebUsers_.NOT_FOUND_IN_DATABASE)
			{
				__COUT__ << "cookieCode invalid" << __E__;
				jumbledEmail = "";     // clear display name if failure
				if(cookieCode != "1")  // indicates uuid not found
					cookieCode = "0";  // clear cookie code if failure
			}
			else  // Log login in logbook for active experiment
				makeSystemLogEntry(theWebUsers_.getUsersUsername(uid) + " logged in.");

			//__COUT__ << "new cookieCode = " << cookieCode.substr(0, 10) << __E__;

			HttpXmlDocument xmldoc(cookieCode, jumbledEmail);

			theWebUsers_.insertSettingsForUser(uid, &xmldoc);  // insert settings

			// insert active session count for user

			if(uid != theWebUsers_.NOT_FOUND_IN_DATABASE)
			{
				uint64_t asCnt = theWebUsers_.getActiveSessionCountForUser(uid) - 1;  // subtract 1 to remove just started session from count
				char     asStr[20];
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
			                                 cgi.getEnvironment().getRemoteAddr()) != theWebUsers_.NOT_FOUND_IN_DATABASE)  // user logout
			{
				// if did some logging out, check if completely logged out
				// if so, system logbook message should be made.
				if(!theWebUsers_.isUserIdActive(uid))
					makeSystemLogEntry(theWebUsers_.getUsersUsername(uid) + " logged out.");
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
		__SS__ << "An error was encountered handling Command '" << Command << "':" << e.what() << __E__;
		__COUT__ << "\n" << ss.str();
		HttpXmlDocument xmldoc;
		xmldoc.addTextElementToData("Error", ss.str());
		xmldoc.outputXmlDocument((std::ostringstream*)out, false /*dispStdOut*/, true /*allowWhiteSpace*/);
	}
	catch(...)
	{
		__SS__ << "An unknown error was encountered handling Command '" << Command << ".' "
		       << "Please check the printouts to debug." << __E__;
		try	{ throw; } //one more try to printout extra info
		catch(const std::exception &e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...){}
		__COUT__ << "\n" << ss.str();
		HttpXmlDocument xmldoc;
		xmldoc.addTextElementToData("Error", ss.str());
		xmldoc.outputXmlDocument((std::ostringstream*)out, false /*dispStdOut*/, true /*allowWhiteSpace*/);
	}

	__COUTT__ << "Login end clock=" << artdaq::TimeUtils::GetElapsedTime(startClock) << __E__;
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
		// returned in cookieCode  Notes: cookie code not refreshed if RequestType =
		// getSystemMessages
		std::string cookieCode = CgiDataUtilities::postData(cgi, "CookieCode");
		uint64_t    uid;

		if(!theWebUsers_.cookieCodeIsActiveForRequest(cookieCode, 0 /*userPermissions*/, 
			&uid, "0" /*dummy ip*/, false /*refresh*/))
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
			WebUsers::tooltipSetNeverShowForUsername(theWebUsers_.getUsersUsername(uid),
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
		__SS__ << "An error was encountered handling Tooltip Command '" << Command << "':" << e.what() << __E__;
		__COUT__ << "\n" << ss.str();
		HttpXmlDocument xmldoc;
		xmldoc.addTextElementToData("Error", ss.str());
		xmldoc.outputXmlDocument((std::ostringstream*)out, false /*dispStdOut*/, true /*allowWhiteSpace*/);
	}
	catch(...)
	{
		__SS__ << "An unknown error was encountered handling Tooltip Command '" << Command << ".' "
		       << "Please check the printouts to debug." << __E__;
		try	{ throw; } //one more try to printout extra info
		catch(const std::exception &e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...){}
		__COUT__ << "\n" << ss.str();
		HttpXmlDocument xmldoc;
		xmldoc.addTextElementToData("Error", ss.str());
		xmldoc.outputXmlDocument((std::ostringstream*)out, false /*dispStdOut*/, true /*allowWhiteSpace*/);
	}

	//__COUT__ << "Done" << __E__;
}  // end tooltipRequest()

//==============================================================================
// setSupervisorPropertyDefaults
//		override to set defaults for supervisor property values (before user settings
// override)
void GatewaySupervisor::setSupervisorPropertyDefaults()
{
	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.UserPermissionsThreshold,
	                                                  std::string() + "*=1 | gatewayLaunchOTS=-1 | gatewayLaunchWiz=-1");

	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.AllowNoLoginRequestTypes,
	                                                  "getCurrentState "
	                                                  " | getAppStatus | getRemoteSubsystems | getRemoteSubsystemStatus");
}  // end setSupervisorPropertyDefaults()

//==============================================================================
// forceSupervisorPropertyValues
//		override to force supervisor property values (and ignore user settings)
void GatewaySupervisor::forceSupervisorPropertyValues()
{
	// note used by these handlers:
	//	request()
	//	stateMachineXgiHandler() -- prepend StateMachine to request type

	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.AutomatedRequestTypes,
	                                                  "getSystemMessages | getCurrentState | getIterationPlanStatus"
	                                                  " | getAppStatus | getRemoteSubsystems | getRemoteSubsystemStatus");
	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.RequireUserLockRequestTypes,
	                                                  "gatewayLaunchOTS | gatewayLaunchWiz | commandRemoteSubsystem");
	//	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.NeedUsernameRequestTypes,
	//			"StateMachine*"); //for all stateMachineXgiHandler requests

	if(readOnly_)
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
	out->getHTTPResponseHeader().addHeader("Access-Control-Allow-Origin", "*");  // to avoid block by blocked by CORS policy of browser

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

	HttpXmlDocument           xmlOut;
	WebUsers::RequestUserInfo userInfo(requestType, CgiDataUtilities::postData(cgiIn, "CookieCode"));

	CorePropertySupervisorBase::getRequestUserInfo(userInfo);

	if(!theWebUsers_.xmlRequestOnGateway(cgiIn, out, &xmlOut, userInfo))
		return;  // access failed

	// RequestType Commands:
	// getSettings
	// setSettings
	// accountSettings
	// getAliasList
	// getAppStatus
	// getAppId
	// getContextMemberNames
	// getSystemMessages
	// setUserWithLock
	// getStateMachine
	// getStateMachineLastLogEntry
	// stateMatchinePreferences
	// getStateMachineNames
	// getCurrentState
	// cancelStateMachineTransition
	// getIterationPlanStatus
	// getErrorInStateMatchine

	// getDesktopIcons
	// addDesktopIcon

	// resetConsoleCounts
	
	// getRemoteSubsystems
	// getRemoteSubsystemStatus
	// commandRemoteSubsystem
	// setRemoteSubsystemFsmControl
	// getSubsystemConfigAliasSelectInfo

	// resetUserTooltips
	// silenceAllUserTooltips

	// gatewayLaunchOTS
	// gatewayLaunchWiz

	if(0)  // leave for debugging
	{
		ConfigurationTree configLinkNode =
		    CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(supervisorContextUID_, supervisorApplicationUID_);

		ConfigurationTree fsmLinkNode = configLinkNode.getNode("LinkToStateMachineTable");

		__COUT__ << "requestType " << requestType << " v" << (fsmLinkNode.getTableVersion()) << __E__;
	}
	else
		__COUTVS__(40,requestType);

	try
	{
		if(requestType == "getSettings")
		{
			std::string accounts = CgiDataUtilities::getData(cgiIn, "accounts");

			__COUT__ << "Get Settings Request" << __E__;
			__COUT__ << "accounts = " << accounts << __E__;
			theWebUsers_.insertSettingsForUser(userInfo.uid_, &xmlOut, accounts == "1");
		}
		else if(requestType == "setSettings")
		{
			std::string bgcolor   = CgiDataUtilities::postData(cgiIn, "bgcolor");
			std::string dbcolor   = CgiDataUtilities::postData(cgiIn, "dbcolor");
			std::string wincolor  = CgiDataUtilities::postData(cgiIn, "wincolor");
			std::string layout    = CgiDataUtilities::postData(cgiIn, "layout");
			std::string syslayout = CgiDataUtilities::postData(cgiIn, "syslayout");

			__COUT__ << "Set Settings Request" << __E__;
			__COUT__ << "bgcolor = " << bgcolor << __E__;
			__COUT__ << "dbcolor = " << dbcolor << __E__;
			__COUT__ << "wincolor = " << wincolor << __E__;
			__COUT__ << "layout = " << layout << __E__;
			__COUT__ << "syslayout = " << syslayout << __E__;

			theWebUsers_.changeSettingsForUser(userInfo.uid_, bgcolor, dbcolor, wincolor, layout, syslayout);
			theWebUsers_.insertSettingsForUser(userInfo.uid_, &xmlOut, true);  // include user accounts
		}
		else if(requestType == "accountSettings")
		{
			std::string type     = CgiDataUtilities::postData(cgiIn, "type");  // updateAccount, createAccount, deleteAccount
			int         type_int = -1;

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

			theWebUsers_.modifyAccountSettings(userInfo.uid_, type_int, username, displayname, email, permissions);

			__COUT__ << "accounts = " << accounts << __E__;

			theWebUsers_.insertSettingsForUser(userInfo.uid_, &xmlOut, accounts == "1");
		}
		else if(requestType == "stateMatchinePreferences")
		{
			std::string       set              = CgiDataUtilities::getData(cgiIn, "set");
			const std::string DEFAULT_FSM_VIEW = "Default_FSM_View";
			if(set == "1")
				theWebUsers_.setGenericPreference(userInfo.uid_, DEFAULT_FSM_VIEW, CgiDataUtilities::getData(cgiIn, DEFAULT_FSM_VIEW));
			else
				theWebUsers_.getGenericPreference(userInfo.uid_, DEFAULT_FSM_VIEW, &xmlOut);
		}
		else if(requestType == "getAliasList")
		{
			// std::string username = userInfo.username_; 
			std::string fsmName  = CgiDataUtilities::getData(cgiIn, "fsmName");
			__SUP_COUTV__(fsmName);

			addFilteredConfigAliasesToXML(xmlOut, fsmName);
			if(0)
			{

				std::string stateMachineAliasFilter = "*";  // default to all

				// IMPORTANT -- use temporary ConfigurationManager to get the Active Group Aliases,
				//	 to avoid changing the Context Configuration tree for the Gateway Supervisor
				ConfigurationManager                                                                  temporaryConfigMgr;
				std::map<std::string /*alias*/, std::pair<std::string /*group name*/, TableGroupKey>> aliasMap;
				aliasMap = temporaryConfigMgr.getActiveGroupAliases();
				
				// AND IMPORTANT -- to use ConfigurationManager to get the Context settings for the Gateway Supervisor
				// get stateMachineAliasFilter if possible
				ConfigurationTree configLinkNode =
					CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(supervisorContextUID_, supervisorApplicationUID_);

				if(!configLinkNode.isDisconnected())
				{
					try  // for backwards compatibility
					{
						ConfigurationTree fsmLinkNode = configLinkNode.getNode("LinkToStateMachineTable");
						if(!fsmLinkNode.isDisconnected() && !fsmLinkNode.getNode(fsmName + "/SystemAliasFilter").isDefaultValue())
							stateMachineAliasFilter = fsmLinkNode.getNode(fsmName + "/SystemAliasFilter").getValue<std::string>();
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

				__COUT__ << "For FSM '" << fsmName << ",' stateMachineAliasFilter  = " << stateMachineAliasFilter << __E__;

				// filter list of aliases based on stateMachineAliasFilter
				//  ! as first character means choose those that do NOT match filter
				//	* can be used as wild card.
				{
					bool                     invertFilter = stateMachineAliasFilter.size() && stateMachineAliasFilter[0] == '!';
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
							if(filterArr[0] != "" && filterArr[0] != "*" && aliasMapPair.first != filterArr[0])
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
									if(aliasMapPair.first.rfind(filterArr[f]) != aliasMapPair.first.size() - filterArr[f].size())
									{
										filterMatch = false;
										break;
									}
								}
								else if((i = aliasMapPair.first.find(filterArr[f])) == std::string::npos)
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
						xmlOut.addTextElementToData("config_key", TableGroupKey::getFullGroupString(aliasMapPair.second.first, aliasMapPair.second.second, /*decorate as (<key>)*/ "(",")"));

						// __COUT__ << "config_alias_comment" << " " <<  temporaryConfigMgr.getNode(
						// 	ConfigurationManager::GROUP_ALIASES_TABLE_NAME).getNode(aliasMapPair.first).getNode(
						// 		TableViewColumnInfo::COL_NAME_COMMENT).getValue<std::string>() << __E__;
						xmlOut.addTextElementToData("config_alias_comment",
													temporaryConfigMgr.getNode(ConfigurationManager::GROUP_ALIASES_TABLE_NAME)
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
							xmlOut.addTextElementToData("config_create_time", groupCreationTime);
						}
						catch(...)
						{
							__COUT_WARN__ << "Failed to load group metadata." << __E__;
						}
					}
				}

				// return last group alias by user
				std::string fn =
					ConfigurationManager::LAST_TABLE_GROUP_SAVE_PATH + "/" + FSM_LAST_GROUP_ALIAS_FILE_START + fsmName + "." + FSM_USERS_PREFERENCES_FILETYPE;
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
				else if(aliasMap.size()) //if not set, return first
					xmlOut.addTextElementToData("UserLastConfigAlias", aliasMap.begin()->first);
			} //end if 0
		}
		else if(requestType == "getAppStatus")
		{
			for(const auto& it : allSupervisorInfo_.getAllSupervisorInfo())
			{
				const auto& appInfo = it.second;

				xmlOut.addTextElementToData("name",
				                            appInfo.getName());                      // get application name
				xmlOut.addTextElementToData("id", std::to_string(appInfo.getId()));  // get application id
				xmlOut.addTextElementToData("status", appInfo.getStatus());          // get status
				xmlOut.addTextElementToData(
				    "time", appInfo.getLastStatusTime() ? StringMacros::getTimestampString(appInfo.getLastStatusTime()) : "0");  // get time stamp
				xmlOut.addTextElementToData("stale",
				                            std::to_string(time(0) - appInfo.getLastStatusTime()));  // time since update
				xmlOut.addTextElementToData("progress", std::to_string(appInfo.getProgress()));      // get progress
				xmlOut.addTextElementToData("detail", appInfo.getDetail());                          // get detail
				xmlOut.addTextElementToData("class",
				                            appInfo.getClass());  // get application class
				xmlOut.addTextElementToData("url",
				                            appInfo.getURL());  // get application url
				xmlOut.addTextElementToData("context",
				                            appInfo.getContextName());  // get context
				auto subappElement = xmlOut.addTextElementToData("subapps", "");
				for(auto& subappInfoPair : appInfo.getSubappInfo())
				{
					xmlOut.addTextElementToParent("subapp_name", subappInfoPair.first, subappElement);
					xmlOut.addTextElementToParent("subapp_status", subappInfoPair.second.status, subappElement);  // get status
					xmlOut.addTextElementToParent("subapp_time",
					    subappInfoPair.second.lastStatusTime ? StringMacros::getTimestampString(subappInfoPair.second.lastStatusTime) : "0",
					                              subappElement);  // get timestamp
					xmlOut.addTextElementToParent("subapp_stale", std::to_string(time(0) - subappInfoPair.second.lastStatusTime), subappElement);  // time since update
					xmlOut.addTextElementToParent("subapp_progress", std::to_string(subappInfoPair.second.progress), subappElement);               // get progress
					xmlOut.addTextElementToParent("subapp_detail", subappInfoPair.second.detail, subappElement);                                   // get detail
					xmlOut.addTextElementToParent("subapp_url", subappInfoPair.second.url, subappElement);                                   // get url
					xmlOut.addTextElementToParent("subapp_class", subappInfoPair.second.class_name, subappElement);                                // get class

				}
			} //end app info loop

			//also return remote gateways as apps			
			std::vector<GatewaySupervisor::RemoteGatewayInfo> remoteApps; //local copy
			{ //lock for remainder of scope
				std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
				remoteApps = remoteGatewayApps_;
			}

			for(const auto& remoteApp : remoteApps)
			{
				const auto& appInfo = remoteApp.appInfo;

				//skip if no status (will show up as subapp of Gateway)
				if(appInfo.status == SupervisorInfo::APP_STATUS_UNKNOWN) continue;

				xmlOut.addTextElementToData("name",
				                            appInfo.name);                      // get application name
				xmlOut.addTextElementToData("id", std::to_string(-1));  // get application id
				xmlOut.addTextElementToData("status", appInfo.status);          // get status
				xmlOut.addTextElementToData(
				    "time", appInfo.lastStatusTime ? StringMacros::getTimestampString(appInfo.lastStatusTime) : "0");  // get timestamp
				xmlOut.addTextElementToData("stale",
				                            std::to_string(time(0) - appInfo.lastStatusTime));  // time since update
				xmlOut.addTextElementToData("progress", std::to_string(appInfo.progress));      // get progress
				xmlOut.addTextElementToData("detail", appInfo.detail);                          // get detail
				xmlOut.addTextElementToData("class",
				                            appInfo.class_name);  // get application class
				xmlOut.addTextElementToData("url",
				                            appInfo.url);  // get application url
				xmlOut.addTextElementToData("context", "Remote-" +
				                            appInfo.url);  // get context
				auto subappElement = xmlOut.addTextElementToData("subapps", "");
				for(auto& subappInfoPair : remoteApp.subapps)
				{
					xmlOut.addTextElementToParent("subapp_name", subappInfoPair.first, subappElement);
					xmlOut.addTextElementToParent("subapp_status", subappInfoPair.second.status, subappElement);  // get status
					xmlOut.addTextElementToParent("subapp_time",
					    subappInfoPair.second.lastStatusTime ? StringMacros::getTimestampString(subappInfoPair.second.lastStatusTime) : "0",
					                              subappElement);  // get time stamp
					xmlOut.addTextElementToParent("subapp_stale", std::to_string(time(0) - subappInfoPair.second.lastStatusTime), subappElement);  // time since update
					xmlOut.addTextElementToParent("subapp_progress", std::to_string(subappInfoPair.second.progress), subappElement);               // get progress
					xmlOut.addTextElementToParent("subapp_detail", subappInfoPair.second.detail, subappElement);                                   // get detail
					xmlOut.addTextElementToParent("subapp_url", subappInfoPair.second.url, subappElement);                                   // get url
					xmlOut.addTextElementToParent("subapp_class", subappInfoPair.second.class_name, subappElement);                                // get class

				}
			} //end remote app info loop

		}
		else if(requestType == "getAppId")
		{
			GatewaySupervisor::handleGetApplicationIdRequest(&allSupervisorInfo_, cgiIn, xmlOut);
		}
		else if(requestType == "getContextMemberNames")
		{
			const XDAQContextTable* contextTable = CorePropertySupervisorBase::theConfigurationManager_->__GET_CONFIG__(XDAQContextTable);

			auto contexts = contextTable->getContexts();
			for(const auto& context : contexts)
			{
				xmlOut.addTextElementToData("ContextMember", context.contextUID_);  // get context member name
			}
		}
		else if(requestType == "getSystemMessages")
		{
			xmlOut.addTextElementToData("systemMessages", theWebUsers_.getSystemMessage(userInfo.displayName_));

			xmlOut.addTextElementToData("username_with_lock",
			                            theWebUsers_.getUserWithLock());  // always give system lock update

			//__COUT__ << "userWithLock " << theWebUsers_.getUserWithLock() << __E__;
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
				xmlOut.addTextElementToData("server_alert",
				                            std::string("Set user lock action failed. You must have valid "
				                                        "permissions and ") +
				                                "locking user must be currently logged in.");

			theWebUsers_.insertSettingsForUser(userInfo.uid_, &xmlOut, accounts == "1");

			if(tmpUserWithLock != theWebUsers_.getUserWithLock())  // if there was a change, broadcast system message
				theWebUsers_.addSystemMessage(
				    "*", theWebUsers_.getUserWithLock() == "" ? tmpUserWithLock + " has unlocked ots." : theWebUsers_.getUserWithLock() + " has locked ots.");
		}
		else if(requestType == "getStateMachineLastLogEntry")
		{
			std::string fsmName  = CgiDataUtilities::getData(cgiIn, "fsmName");
			std::string transition  = CgiDataUtilities::getData(cgiIn, "transition");
			__SUP_COUTV__(fsmName);
			__SUP_COUTV__(transition);
			xmlOut.addTextElementToData("lastLogEntry",
				getLastLogEntry(transition,fsmName));
		}
		else if(requestType == "getStateMachine")
		{
			// __COUT__ << "Getting state machine" << __E__;

			std::string fsmName  = CgiDataUtilities::getData(cgiIn, "fsmName");
			__SUP_COUTVS__(20,fsmName);

			addRequiredFsmLogInputToXML(xmlOut,fsmName);

			std::vector<toolbox::fsm::State> states;
			states = theStateMachine_.getStates();
			char stateStr[2];
			stateStr[1] = '\0';
			std::string transName;
			std::string transParameter;

			// bool addRun, addCfg;
			for(unsigned int i = 0; i < states.size(); ++i)  // get all states
			{
				stateStr[0]             = states[i];
				DOMElement* stateParent = xmlOut.addTextElementToData("state", stateStr);

				xmlOut.addTextElementToParent("state_name", theStateMachine_.getStateName(states[i]), stateParent);

				//__COUT__ << "state: " << states[i] << " - " <<
				// theStateMachine_.getStateName(states[i]) << __E__;

				// get all transition final states, transitionNames and actionNames from
				// state
				std::map<std::string, toolbox::fsm::State, std::less<std::string>> trans       = theStateMachine_.getTransitions(states[i]);
				std::set<std::string>                                              actionNames = theStateMachine_.getInputs(states[i]);

				std::map<std::string, toolbox::fsm::State, std::less<std::string>>::iterator it  = trans.begin();
				std::set<std::string>::iterator                                              ait = actionNames.begin();

				//			addRun = false;
				//			addCfg = false;

				// handle hacky way to keep "forward" moving states on right of FSM
				// display  must be first!

				for(; it != trans.end() && ait != actionNames.end(); ++it, ++ait)
				{
					stateStr[0] = it->second;

					if(stateStr[0] == 'R')
					{
						// addRun = true;
						xmlOut.addTextElementToParent("state_transition", stateStr, stateParent);

						//__COUT__ << states[i] << " => " << *ait << __E__;

						xmlOut.addTextElementToParent("state_transition_action", *ait, stateParent);

						transName = theStateMachine_.getTransitionName(states[i], *ait);
						//__COUT__ << states[i] << " => " << transName << __E__;

						xmlOut.addTextElementToParent("state_transition_name", transName, stateParent);
						transParameter = theStateMachine_.getTransitionParameter(states[i], *ait);
						//__COUT__ << states[i] << " => " << transParameter<< __E__;

						xmlOut.addTextElementToParent("state_transition_parameter", transParameter, stateParent);
						break;
					}
					else if(stateStr[0] == 'C')
					{
						// addCfg = true;
						xmlOut.addTextElementToParent("state_transition", stateStr, stateParent);

						//__COUT__ << states[i] << " => " << *ait << __E__;

						xmlOut.addTextElementToParent("state_transition_action", *ait, stateParent);

						transName = theStateMachine_.getTransitionName(states[i], *ait);
						//__COUT__ << states[i] << " => " << transName << __E__;

						xmlOut.addTextElementToParent("state_transition_name", transName, stateParent);
						transParameter = theStateMachine_.getTransitionParameter(states[i], *ait);
						//__COUT__ << states[i] << " => " << transParameter<< __E__;

						xmlOut.addTextElementToParent("state_transition_parameter", transParameter, stateParent);
						break;
					}
				}

				// reset for 2nd pass
				it  = trans.begin();
				ait = actionNames.begin();

				// other states
				for(; it != trans.end() && ait != actionNames.end(); ++it, ++ait)
				{
					//__COUT__ << states[i] << " => " << it->second << __E__;

					stateStr[0] = it->second;

					if(stateStr[0] == 'R')
						continue;
					else if(stateStr[0] == 'C')
						continue;

					xmlOut.addTextElementToParent("state_transition", stateStr, stateParent);

					//__COUT__ << states[i] << " => " << *ait << __E__;

					xmlOut.addTextElementToParent("state_transition_action", *ait, stateParent);

					transName = theStateMachine_.getTransitionName(states[i], *ait);
					//__COUT__ << states[i] << " => " << transName << __E__;

					xmlOut.addTextElementToParent("state_transition_name", transName, stateParent);
					transParameter = theStateMachine_.getTransitionParameter(states[i], *ait);
					//__COUT__ << states[i] << " => " << transParameter<< __E__;

					xmlOut.addTextElementToParent("state_transition_parameter", transParameter, stateParent);
				}
			} //end state traversal loop
		}
		else if(requestType == "getStateMachineNames")
		{
			// get stateMachineAliasFilter if possible
			ConfigurationTree configLinkNode =
			    CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(supervisorContextUID_, supervisorApplicationUID_);

			try
			{
				auto fsmNodes = configLinkNode.getNode("LinkToStateMachineTable").getChildren();
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
			__SS__ << "State transition was cancelled by user!" << __E__;
			__MCOUT__(ss.str());
			RunControlStateMachine::theStateMachine_.setErrorMessage(ss.str());
			RunControlStateMachine::asyncFailureReceived_ = true;
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
			ConfigurationManager                              tmpCfgMgr; // Creating new temporary instance so that constructor will activate latest context, note: not using member CorePropertySupervisorBase::theConfigurationManager_
			const DesktopIconTable*                           iconTable = tmpCfgMgr.__GET_CONFIG__(DesktopIconTable);
			const std::vector<DesktopIconTable::DesktopIcon>& icons     = iconTable->getAllDesktopIcons();


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


			//__COUTV__((unsigned int)userInfo.permissionLevel_);

			std::map<std::string, WebUsers::permissionLevel_t> userPermissionLevelsMap = theWebUsers_.getPermissionsForUser(userInfo.uid_);
			std::map<std::string, WebUsers::permissionLevel_t> iconPermissionThresholdsMap;

			bool getRemoteIcons = true;
			std::string ipAddressForRemoteIconsOverUDP = "";

			bool firstIcon = true;
			for(const auto& icon : icons)
			{
				//__COUTV__(icon.caption_);
				//__COUTV__(icon.permissionThresholdString_);

				CorePropertySupervisorBase::extractPermissionsMapFromString(icon.permissionThresholdString_, iconPermissionThresholdsMap);

				if(!CorePropertySupervisorBase::doPermissionsGrantAccess(userPermissionLevelsMap, iconPermissionThresholdsMap))
					continue;  // skip icon if no access

				//__COUTV__(icon.caption_);

				if(getRemoteIcons)
				{
					__COUTV__(icon.windowContentURL_);
					if(icon.windowContentURL_.size() > 4 && 
						icon.windowContentURL_[0] == 'o' &&
						icon.windowContentURL_[1] == 't' &&
						icon.windowContentURL_[2] == 's' &&
						icon.windowContentURL_[3] == ':')
					{
						continue; //skip retrieval and use cache!

						__COUT__ << "Retrieving remote icons at " << icon.windowContentURL_ << __E__;

						std::vector<std::string> parsedFields = StringMacros::getVectorFromString(icon.windowContentURL_,{':'});
						__COUTV__(StringMacros::vectorToString(parsedFields));

						if(parsedFields.size() == 3)
						{
							__COUT__ << "Opening socket to " << parsedFields[1] << ":" << parsedFields[2] << __E__;

							Socket iconRemoteSocket(parsedFields[1],atoi(parsedFields[2].c_str()));

							if(ipAddressForRemoteIconsOverUDP == "")
							{
								ConfigurationTree configLinkNode = this->CorePropertySupervisorBase::getSupervisorTableNode();
								ipAddressForRemoteIconsOverUDP = configLinkNode.getNode("IPAddressForStateChangesOverUDP").getValue<std::string>();
								__COUTV__(ipAddressForRemoteIconsOverUDP);
							}
							try
							{
								TransceiverSocket iconSocket(ipAddressForRemoteIconsOverUDP, 0);
								iconSocket.initialize();
								std::string remoteIconString = iconSocket.sendAndReceive(iconRemoteSocket,"GetRemoteDesktopIcons", 10 /*timeoutSeconds*/);
								__COUTV__(remoteIconString);

								//now have remote icon string, append icons to list
								std::vector<std::string> remoteIconsCSV = StringMacros::getVectorFromString(remoteIconString, {','});
								const size_t numOfIconFields = 7;
								for(size_t i = 0; i+numOfIconFields < remoteIconsCSV.size(); i += numOfIconFields)
								{

									if(firstIcon)
										firstIcon = false;
									else
										iconString += ",";					

									__COUTV__(remoteIconsCSV[i+0]);
									if(icon.folderPath_ == "") //if not in folder, distinguish remote icon somehow
										iconString += icon.alternateText_ + " " + remoteIconsCSV[i+0]; //icon.caption_;
									else
										iconString += remoteIconsCSV[i+0]; //icon.caption_;
									iconString += "," + remoteIconsCSV[i+1]; //icon.alternateText_;
									iconString += "," + remoteIconsCSV[i+2]; //std::string(icon.enforceOneWindowInstance_ ? "1" : "0");
									iconString += "," + std::string("1");  // set permission to 1 so the
																		// desktop shows every icon that the
																		// server allows (i.e., trust server
																		// security, ignore client security)
									iconString += "," + remoteIconsCSV[i+4]; //icon.imageURL_;
									iconString += "," + remoteIconsCSV[i+5]; //icon.windowContentURL_;
									
									iconString += "," + icon.folderPath_ + "/" + remoteIconsCSV[i+6]; //icon.folderPath_;
									
								} //end append remote icons

							}
							catch(const std::runtime_error& e)
							{
								__SS__ << "Failure getting Remote Desktop Icons for record '" << icon.recordUID_ << "' from ots:host:port url=" << icon.windowContentURL_ << 
									". Please check the Remote Gateway App status, perhaps the Remote Gateway App is down. \n\nHere was the error:\n" << e.what();
								
								//add error for notification, but try to ignore so rest of system works
								__COUT_ERR__ << "\n" << ss.str();
								xmlOut.addTextElementToData("Error", ss.str());
								// __SS_THROW__;  
							}

							continue;
						}
					}
				} //end remote icon handling

				// have icon access, so add to CSV string
				if(firstIcon)
					firstIcon = false;
				else
					iconString += ",";
					

				iconString += icon.caption_;
				iconString += "," + icon.alternateText_;
				iconString += "," + std::string(icon.enforceOneWindowInstance_ ? "1" : "0");
				iconString += "," + std::string("1");  // set permission to 1 so the
				                                       // desktop shows every icon that the
				                                       // server allows (i.e., trust server
				                                       // security, ignore client security)
				iconString += "," + icon.imageURL_;
				iconString += "," + icon.windowContentURL_;
				iconString += "," + icon.folderPath_;
			}
			// //__COUTV__(iconString);

			//also return remote gateway icons
			std::vector<GatewaySupervisor::RemoteGatewayInfo> remoteGatewayApps; //local copy
			{ //lock for remainder of scope
				std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
				remoteGatewayApps = remoteGatewayApps_;
			}

			for(const auto& remoteGatewayApp : remoteGatewayApps)
			{				
				if(remoteGatewayApp.iconString == "") 
				{
					//add error if it has to do with icons
					if(remoteGatewayApp.error.find("desktop icons") != std::string::npos)
						xmlOut.addTextElementToData("Error", remoteGatewayApp.error);

					//add placeholder "Loading icon"
					if(firstIcon)
						firstIcon = false;
					else
						iconString += ",";
					
					if(remoteGatewayApp.parentIconFolderPath != "")
						iconString += remoteGatewayApp.parentIconFolderPath + " icons loading..."; //icon.caption_;
					else if(remoteGatewayApp.user_data_path_record != "")
						iconString += remoteGatewayApp.user_data_path_record + " icons loading..."; //icon.caption_;
					else
						iconString += remoteGatewayApp.appInfo.name + " icons loading..."; //icon.caption_;
						
					iconString += ",X"; //icon.alternateText_;
					iconString += ",1";//std::string(icon.enforceOneWindowInstance_ ? "1" : "0");
					iconString += ",0";//std::string("1");  // set permission to 1 so the
														// desktop shows every icon that the
														// server allows (i.e., trust server
														// security, ignore client security)
					iconString += ",";//icon.imageURL_;
					iconString += ",";//icon.windowContentURL_;
					iconString += ",";//icon.folderPath_;
				
					continue;
				}

				if(firstIcon)
					firstIcon = false;
				else
					iconString += ",";

				iconString += remoteGatewayApp.iconString;
			}

			xmlOut.addTextElementToData("iconList", iconString);			
		}
		else if(requestType == "addDesktopIcon")
		{
			std::vector<DesktopIconTable::DesktopIcon> newIcons;

			bool success = GatewaySupervisor::handleAddDesktopIconRequest(
				userInfo.username_,	cgiIn, xmlOut, &newIcons);

			if(success)
			{
				__COUT__ << "Attempting dynamic icon change..." << __E__;

				DesktopIconTable* iconTable = (DesktopIconTable*)CorePropertySupervisorBase::theConfigurationManager_->getDesktopIconTable();
				iconTable->setAllDesktopIcons(newIcons);
			}
			else
				__COUT__ << "Failed dynamic icon add." << __E__;
		} //end addDesktopIcon
		else if(requestType == "resetConsoleCounts")
		{
			//zero out console count and retake first messages

			for(const auto& it : allSupervisorInfo_.getAllSupervisorInfo())
			{
				const auto& appInfo = it.second;
				if(appInfo.isTypeConsoleSupervisor())
				{
					xoap::MessageReference tempMessage = SOAPUtilities::makeSOAPMessageReference("ResetConsoleCounts");
					std::string reply = send(appInfo.getDescriptor(), tempMessage);

					if(reply != "Done")
					{
						__SUP_SS__ << "Error while resetting console counts of Supervisor instance = '" << appInfo.getName() << "' [LID=" << appInfo.getId()
			       			<< "] in Context '" << appInfo.getContextName() << "' [URL=" << appInfo.getURL() << "].\n\n"
							<< reply << __E__;
						__SUP_SS_THROW__;
					}
					__SUP_COUT__ << "Reset console counts of Supervisor instance = '" << appInfo.getName() << "' [LID=" << appInfo.getId()
			       			<< "] in Context '" << appInfo.getContextName() << "' [URL=" << appInfo.getURL() << "]." << __E__;
				}
			} //end loop for Console Supervisors

			//for user display feedback, clear local cached values also
			std::lock_guard<std::mutex> lock(systemStatusMutex_); //lock for rest of scope
			lastConsoleErrTime_ = "0"; lastConsoleErr_ = "";
			lastConsoleWarnTime_ = "0"; lastConsoleWarn_ = "";
			lastConsoleInfoTime_ = "0"; lastConsoleInfo_ = "";
			firstConsoleErrTime_ = "0"; firstConsoleErr_ = "";
			firstConsoleWarnTime_ = "0"; firstConsoleWarn_ = "";
			firstConsoleInfoTime_ = "0"; firstConsoleInfo_ = "";

		} //end resetConsoleCounts
		else if(requestType == "getRemoteSubsystems" || requestType == "getRemoteSubsystemStatus")
		{
			std::string fsmName  = CgiDataUtilities::getData(cgiIn, "fsmName");			
			bool getRunTypeNote  = CgiDataUtilities::getDataAsInt(cgiIn, "getRunTypeNote");
			bool getFullInfo 	 = (requestType == "getRemoteSubsystems");
			bool getRunNumber    = CgiDataUtilities::getDataAsInt(cgiIn, "getRunNumber");

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

				addRequiredFsmLogInputToXML(xmlOut,fsmName); //(a la getStateMachine request)
												
			} //end getFullInfo prepend

			{ //get system status 
								
				xmlOut.addTextElementToData("last_run_log_entry", 
						getLastLogEntry(RunControlStateMachine::START_TRANSITION_NAME,fsmName));


				//getIterationPlanStatus returns iterator status and does not request next run number (which is expensive)
				//	.. so only get run number 1:10
				//getIterationPlanStatus will repeat a few fields, but js will just take first field, so doesnt matter
				addStateMachineStatusToXML(xmlOut, fsmName, getRunNumber);	 //(a la getCurrentState request)
				theIterator_.handleCommandRequest(xmlOut, "getIterationPlanStatus", "");

				std::lock_guard<std::mutex> lock(systemStatusMutex_); //lock for rest of scope

				xmlOut.addTextElementToData("last_logbook_entry", lastLogbookEntry_);
				xmlOut.addTextElementToData("last_logbook_entry_time", lastLogbookEntryTime_?StringMacros::getTimestampString(lastLogbookEntryTime_):"0");
				auto msgPair = theWebUsers_.getLastSystemMessage();
				xmlOut.addTextElementToData("last_system_message", msgPair.first);
				xmlOut.addTextElementToData("last_system_message_time", msgPair.second?StringMacros::getTimestampString(msgPair.second):"0");
				xmlOut.addNumberElementToData("active_user_count", theWebUsers_.getActiveUserCount());
				xmlOut.addNumberElementToData("console_err_count", systemConsoleErrCount_);
				xmlOut.addNumberElementToData("console_warn_count", systemConsoleWarnCount_);
				xmlOut.addNumberElementToData("console_info_count", systemConsoleInfoCount_);
				xmlOut.addTextElementToData("last_console_err_msg", lastConsoleErr_);
				xmlOut.addTextElementToData("last_console_warn_msg", lastConsoleWarn_);
				xmlOut.addTextElementToData("last_console_info_msg", lastConsoleInfo_);
				xmlOut.addTextElementToData("last_console_err_msg_time", lastConsoleErrTime_);
				xmlOut.addTextElementToData("last_console_warn_msg_time", lastConsoleWarnTime_);
				xmlOut.addTextElementToData("last_console_info_msg_time", lastConsoleInfoTime_);
				xmlOut.addTextElementToData("first_console_err_msg", firstConsoleErr_);
				xmlOut.addTextElementToData("first_console_warn_msg", firstConsoleWarn_);
				xmlOut.addTextElementToData("first_console_info_msg", firstConsoleInfo_);
				xmlOut.addTextElementToData("first_console_err_msg_time", firstConsoleErrTime_);
				xmlOut.addTextElementToData("first_console_warn_msg_time", firstConsoleWarnTime_);
				xmlOut.addTextElementToData("first_console_info_msg_time", firstConsoleInfoTime_);
		
			} //end get system status

			std::vector<GatewaySupervisor::RemoteGatewayInfo> remoteGatewayApps; //local copy
			{ //lock for remainder of scope
				std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
				__SUP_COUTVS__(22,remoteGatewayApps_.size());
				remoteGatewayApps = remoteGatewayApps_;
				if(remoteGatewayApps_.size())
					__SUP_COUT_TYPE__(TLVL_DEBUG+22) << __COUT_HDR__ << remoteGatewayApps_[0].command << " " << (remoteGatewayApps_[0].appInfo.status) << __E__;
			}

			std::string accumulateErrors = "";
			for(const auto& remoteSubsystem : remoteGatewayApps)
			{
				xmlOut.addTextElementToData("subsystem_name", remoteSubsystem.appInfo.name);
				xmlOut.addTextElementToData("subsystem_url", remoteSubsystem.appInfo.url);
				xmlOut.addTextElementToData("subsystem_landingPage", remoteSubsystem.landingPage);
				xmlOut.addTextElementToData("subsystem_status", remoteSubsystem.appInfo.status);
				xmlOut.addTextElementToData("subsystem_progress", std::to_string(remoteSubsystem.appInfo.progress));
				xmlOut.addTextElementToData("subsystem_detail", remoteSubsystem.appInfo.detail);
				xmlOut.addTextElementToData("subsystem_lastStatusTime", StringMacros::getTimestampString(remoteSubsystem.appInfo.lastStatusTime));
				xmlOut.addTextElementToData("subsystem_consoleErrCount", std::to_string(remoteSubsystem.consoleErrCount));
				xmlOut.addTextElementToData("subsystem_consoleWarnCount", std::to_string(remoteSubsystem.consoleWarnCount));
				
				if(remoteSubsystem.error != "")
				{
					__COUTT__ << "Error from Subsystem '" << remoteSubsystem.appInfo.name << 
						"' = " << remoteSubsystem.error << __E__;	

					if(remoteSubsystem.error.find("Failure gathering Remote Gateway desktop icons") == 
						std::string::npos) //only add if not Icon error
					{
						if(accumulateErrors.size()) accumulateErrors += "\n";
						accumulateErrors += remoteSubsystem.error;
					}
				}
				
				//special values for managing remote subsystems
				xmlOut.addTextElementToData("subsystem_configAlias", remoteSubsystem.selected_config_alias);

				if(getFullInfo)
				{
					if(remoteSubsystem.user_data_path_record == "")
					{
						// __SUP_SS__;
						__SUP_COUT_WARN__ << "Remote Subsystem '" <<
							remoteSubsystem.appInfo.name << "' user data path is empty. Perhaps the system is still booting up. If the problem persists, note that Remote Subsystems are specified through their Desktop Icon record. "
							"Please specify a valid User Data Path record as the Desktop Icon AlternateText field, targeting a UID in the SubsystemUserDataPathsTable (or contact system admins for assitance)." << __E__;
						// __SUP_SS_THROW__;
					}					
				}
				xmlOut.addTextElementToData("subsystem_configAliasChoices", StringMacros::setToString(remoteSubsystem.config_aliases,{','}) );	//CSV list of aliases
				xmlOut.addTextElementToData("subsystem_fsmMode", remoteSubsystem.getFsmMode());
				xmlOut.addTextElementToData("subsystem_fsmIncluded", remoteSubsystem.fsm_included?"1":"0");
			} //end remote app loop

			if(accumulateErrors != "")
				xmlOut.addTextElementToData("system_error", accumulateErrors);

		} //end getRemoteSubsystems
		else if(requestType == "setRemoteSubsystemFsmControl")
		{
			std::string targetSubsystem  = CgiDataUtilities::getData(cgiIn, "targetSubsystem");		 // * for all
			std::string setValue  = CgiDataUtilities::getData(cgiIn, "setValue"); 
			std::string controlType  = CgiDataUtilities::getData(cgiIn, "controlType");		 // include, mode

			setValue = StringMacros::decodeURIComponent(setValue);
			__SUP_COUTV__(targetSubsystem);
			__SUP_COUTV__(setValue);
			__SUP_COUTV__(controlType);

			bool changedSomething = false;
			std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
			for(auto& remoteGatewayApp : remoteGatewayApps_)
				if(targetSubsystem == "*" || targetSubsystem == remoteGatewayApp.appInfo.name)
				{
					changedSomething = true;
					if(controlType == "include")
						remoteGatewayApp.fsm_included = setValue == "1"?true:false;
					if(controlType == "configAlias")
					{
						if(remoteGatewayApp.config_aliases.find(setValue) == 
							remoteGatewayApp.config_aliases.end())
						{
							__SUP_SS__ << "Configuration Alias value '" << setValue << "' for target Subsystem '" << 
								remoteGatewayApp.appInfo.name << "' is not found in list of Configuration Aliases: " << 
								StringMacros::setToString(remoteGatewayApp.config_aliases) << __E__;
							__SUP_SS_THROW__;
						}
						remoteGatewayApp.selected_config_alias = setValue;
					}
					else if(controlType == "mode")
						remoteGatewayApp.fsm_mode = 
							setValue == "Do Not Halt"?RemoteGatewayInfo::FSM_ModeTypes::DoNotHalt:
								(setValue == "Only Configure"?RemoteGatewayInfo::FSM_ModeTypes::OnlyConfigure:
									RemoteGatewayInfo::FSM_ModeTypes::Follow_FSM);
				}
			
			if(!changedSomething)
			{
				__SUP_SS__ << "Did not find any matching subsystems for target '" << targetSubsystem << 
							"' attempted!" << __E__;
				__SUP_SS_THROW__;
			}

			saveRemoteGatewaySettings();

		} //end setRemoteSubsystemFsmControl
		else if(requestType == "getSubsystemConfigAliasSelectInfo")
		{
			std::string targetSubsystem  = CgiDataUtilities::getData(cgiIn, "targetSubsystem");	
			//return info on selected_config_alias
			
			bool found = false;
			std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
			for(auto& remoteGatewayApp : remoteGatewayApps_)
				if(targetSubsystem == remoteGatewayApp.appInfo.name)
				{
					found = true;

					if(remoteGatewayApp.selected_config_alias == "")
					{
						__SUP_SS__ << "No selected Configuration Alias found for target Subsystem '" << 
							remoteGatewayApp.appInfo.name << "' - please select one before requesting info." << __E__;
						__SUP_SS_THROW__;
					}

					std::pair<std::string, TableGroupKey> groupTranslation;
					std::string groupComment, groupAuthor, groupCreationTime;

					ConfigurationManager tmpCfgMgr; // Creating new temporary instance to not mess up member CorePropertySupervisorBase::theConfigurationManager_
					tmpCfgMgr.getOtherSubsystemConfigAliasInfo(
						remoteGatewayApp.user_data_path_record,
						remoteGatewayApp.selected_config_alias,
						groupTranslation, groupComment, groupAuthor, groupCreationTime
					);


					std::stringstream returnInfo;
					returnInfo << "At remote Subsystem <b>'" << remoteGatewayApp.appInfo.name << 
						",'</b> the Configure Alias <b>'" << remoteGatewayApp.selected_config_alias << 
						"'</b>  translates to <b>" << groupTranslation.first << "(" << groupTranslation.second <<
						")</b>  w/comment: <br><br><i>" << 						
						StringMacros::decodeURIComponent(groupComment);
					if(groupCreationTime != "" && groupCreationTime != "0")
						returnInfo << "<br><br>";
					returnInfo << "<b>" << groupTranslation.first << "(" << groupTranslation.second <<
						")</b> was created by " << groupAuthor << " (" <<
						StringMacros::getTimestampString(groupCreationTime) << ")";
					returnInfo << "</i>";

					xmlOut.addTextElementToData("alias_info", returnInfo.str());
					break;
				}

			if(!found)
			{
				__SUP_SS__ << "Did not find any matching subsystems for target '" << targetSubsystem << 
							"' attempted!" << __E__;
				__SUP_SS_THROW__;
			}

		} //end getSubsystemConfigAliasSelectInfo
		else if(requestType == "commandRemoteSubsystem")
		{
			std::string targetSubsystem  = CgiDataUtilities::getData(cgiIn, "targetSubsystem");		
			std::string command  = CgiDataUtilities::getData(cgiIn, "command");		
			std::string parameter  = CgiDataUtilities::getData(cgiIn, "parameter");	
			std::string fsmName  = CgiDataUtilities::getData(cgiIn, "fsmName");		

			__SUP_COUTV__(targetSubsystem);
			__SUP_COUTV__(command);
			__SUP_COUTV__(parameter);
			__SUP_COUTV__(fsmName);

			if(command == "")
			{
				__SUP_SS__ << "Illegal empty command received to target remote subsystem '" << targetSubsystem << 
							"' attempted!" << __E__;
				__SUP_SS_THROW__;
			}
			if(targetSubsystem == "")
			{
				__SUP_SS__ << "Illegal empty targetSubsystem received for remote subsystem command '" << command << 
							"' attempted!" << __E__;
				__SUP_SS_THROW__;
			}

			bool found = false;
			std::lock_guard<std::mutex> lock(remoteGatewayAppsMutex_);
			for(auto& remoteGatewayApp : remoteGatewayApps_)
			{
				if(targetSubsystem == remoteGatewayApp.appInfo.name)
				{
					if(remoteGatewayApp.command != "")
					{
						__SUP_SS__ << "Can not target the remote subsystem '" << targetSubsystem << 
							"' with command '" << command << "' which already has a pending command '"
							<< remoteGatewayApp.command << ".' Please try again after the pending command is sent." << __E__;
						__SUP_SS_THROW__;					
					}
					remoteGatewayApp.error = ""; //clear to see result of this command
					remoteGatewayApp.command = command + (parameter != ""?("," + parameter):"");
					
					//for non-FSM commands, do not modify fsmName
					if(command != "ResetConsoleCounts")
						remoteGatewayApp.fsmName = fsmName;

					//force status for immediate user feedback
					remoteGatewayApp.appInfo.status = "Launching " + command;
					remoteGatewayApp.appInfo.progress = 0;
					found = true;
				}
			} //end search for targetSubsystem

			if(!found)
			{
				__SUP_SS__ << "Target remote subsystem '" << targetSubsystem << 
							"' was not found for attempted command '" << command << "!'" << __E__;
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
				GatewaySupervisor::launchStartOTSCommand("LAUNCH_OTS", CorePropertySupervisorBase::theConfigurationManager_);
			else if(requestType == "gatewayLaunchWiz")
				GatewaySupervisor::launchStartOTSCommand("LAUNCH_WIZ", CorePropertySupervisorBase::theConfigurationManager_);
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
			__COUT__ << "launch ots script Command = " << "OTS_APP_SHUTDOWN" << __E__;
			GatewaySupervisor::launchStartOneServerCommand("OTS_APP_SHUTDOWN", CorePropertySupervisorBase::theConfigurationManager_, contextName);
			sleep(5);
			GatewaySupervisor::launchStartOneServerCommand("OTS_APP_STARTUP", CorePropertySupervisorBase::theConfigurationManager_, contextName);

			xmlOut.addTextElementToData("status", "restarted");
		}
		else
		{
			__SS__ << "requestType Request, " << requestType << ", not recognized." << __E__;
			__SS_THROW__;
		}
	}
	catch(const std::runtime_error& e)
	{
		__SS__ << "An error was encountered handling requestType '" << requestType << "':" << e.what() << __E__;
		__COUT__ << "\n" << ss.str();
		xmlOut.addTextElementToData("Error", ss.str());
	}
	catch(...)
	{
		__SS__ << "An unknown error was encountered handling requestType '" << requestType << ".' "
		       << "Please check the printouts to debug." << __E__;
		try	{ throw; } //one more try to printout extra info
		catch(const std::exception &e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...){}
		__COUT__ << "\n" << ss.str();
		xmlOut.addTextElementToData("Error", ss.str());
	}

	// return xml doc holding server response
	xmlOut.outputXmlDocument((std::ostringstream*)out, false /*dispStdOut*/, true /*allowWhiteSpace*/);  // Note: allow white space need for error response

	//__COUT__ << "Done" << __E__;
}  // end request()
catch(const std::runtime_error& e)
{
	__COUT_ERR__ << "Caught error at request(): " << e.what() << __E__;
	throw;
}  // end request() exception handling

//==============================================================================
void GatewaySupervisor::addStateMachineStatusToXML(
	HttpXmlDocument& xmlOut,
	const std::string& fsmName,
	bool getRunNumber /* = true */)
{
	xmlOut.addTextElementToData("current_state", theStateMachine_.getCurrentStateName());
	const std::string& gatewayStatus = allSupervisorInfo_.getGatewayInfo().getStatus();
	if(gatewayStatus.size() > std::string(RunControlStateMachine::FAILED_STATE_NAME).length() && 
		(gatewayStatus[0] == 'F' || gatewayStatus[0] == 'E')) //assume it is Failed or Error and send to state machine
		xmlOut.addTextElementToData("current_error", gatewayStatus);

	xmlOut.addTextElementToData("in_transition", theStateMachine_.isInTransition() ? "1" : "0");
	if(theStateMachine_.isInTransition())
	{
		xmlOut.addTextElementToData("transition_progress", RunControlStateMachine::theProgressBar_.readPercentageString());
		xmlOut.addTextElementToData("current_transition", theStateMachine_.getCurrentTransitionName());
	}
	else
	{	
		xmlOut.addTextElementToData("transition_progress", "100");
		xmlOut.addTextElementToData("current_transition", "");
	}
	xmlOut.addTextElementToData("time_in_state", std::to_string(theStateMachine_.getTimeInState()));


	// char tmp[20]; old size before adding db run number
	char tmp[50]; // for a 6 digits run number from the DB, this needs to be at least 34 chars

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
			xmlOut.addTextElementToData("soft_error", RunControlStateMachine::getErrorMessage());
		}
		
		std::string stateMachineRunAlias = "Run";  // default to "Run"

		// get stateMachineAliasFilter if possible
		ConfigurationTree configLinkNode =
			CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(supervisorContextUID_, supervisorApplicationUID_);

		if(!configLinkNode.isDisconnected())
		{
			try  // for backwards compatibility
			{
				ConfigurationTree fsmLinkNode = configLinkNode.getNode("LinkToStateMachineTable");
				if(!fsmLinkNode.isDisconnected())
				{
					if(!fsmLinkNode.getNode(fsmName + "/RunDisplayAlias").isDefaultValue())
						stateMachineRunAlias = fsmLinkNode.getNode(fsmName + "/RunDisplayAlias").getValue<std::string>();
					std::string runInfoPluginType = fsmLinkNode.getNode(fsmName + "/RunInfoPluginType").getValue<std::string>();
					if(runInfoPluginType != TableViewColumnInfo::DATATYPE_STRING_DEFAULT && runInfoPluginType != "No Run Info Plugin")
						useRunInfoDb = true;
				}
			}
			catch(std::runtime_error& e)
			{
				; //ignoring error
			}
			catch(...)
			{
				__COUT_ERR__ << "Unknown error looking for Run alias. Should never happen." << __E__;
			}
		}

		xmlOut.addTextElementToData("stateMachineRunAlias", stateMachineRunAlias);

		//// ======================== get run number based on fsm name ====

		if(theStateMachine_.getCurrentStateName() == RunControlStateMachine::RUNNING_STATE_NAME || 
			theStateMachine_.getCurrentStateName() ==  RunControlStateMachine::PAUSED_STATE_NAME)
		{
			if(useRunInfoDb)
				sprintf(tmp, "Current %s Number from DB: %s", stateMachineRunAlias.c_str(),
					activeStateMachineRunNumber_.c_str());
					//%u // getNextRunNumber(activeStateMachineName_) - 1);
			else
				sprintf(tmp, "Current %s Number: %s", stateMachineRunAlias.c_str(), 
					activeStateMachineRunNumber_.c_str()); //%u //getNextRunNumber(activeStateMachineName_) - 1);

			if(RunControlStateMachine::asyncPauseExceptionReceived_)
			{
				//__COUTV__(RunControlStateMachine::asyncPauseExceptionReceived_);
				//__COUTV__(RunControlStateMachine::getErrorMessage());
				xmlOut.addTextElementToData("soft_error", RunControlStateMachine::getErrorMessage());
			}
		}
		else if(getRunNumber) //only periodically get next run number (expensive from file, and shouldnt change much)
		{
			if(useRunInfoDb)
				sprintf(tmp, "Next %s Number from DB.", stateMachineRunAlias.c_str());
			else
				sprintf(tmp, "Next %s Number: %u", stateMachineRunAlias.c_str(), getNextRunNumber(fsmName));

			xmlOut.addTextElementToData("run_number", tmp);
		}

	} //end not-in-transition handling
}  // end request()

//==============================================================================
void GatewaySupervisor::addRequiredFsmLogInputToXML(
	HttpXmlDocument& xmlOut,
	const std::string& fsmName)
{
	bool        requireUserLogInputOnConfigure = false, requireUserLogInputOnRun = false;
	//if fsmName specified, return log entry requirements from config tree
	if(fsmName != "")
	{
		//------------------
		ConfigurationTree configLinkNode =
			CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(supervisorContextUID_, supervisorApplicationUID_);
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
	} //end log entry requirements gathering

	xmlOut.addTextElementToData("RequireUserLogInputOnConfigureTransition", requireUserLogInputOnConfigure?"1":"0");
	xmlOut.addTextElementToData("RequireUserLogInputOnRunTransition", 		requireUserLogInputOnRun?"1":"0");
}  // end request()

//==============================================================================
void GatewaySupervisor::addFilteredConfigAliasesToXML(
	HttpXmlDocument& xmlOut,
	const std::string& fsmName)
{
	__SUP_COUTV__(fsmName);

	// IMPORTANT -- use temporary ConfigurationManager to get the Active Group Aliases,
	//	 to avoid changing the Context Configuration tree for the Gateway Supervisor
	ConfigurationManager                                                                  temporaryConfigMgr;
	std::map<std::string /*alias*/, std::pair<std::string /*group name*/, TableGroupKey>> aliasMap;
	aliasMap = temporaryConfigMgr.getActiveGroupAliases();

	// also IMPORTANT -- to use theConfigurationManager_ to get the Context settings for the Gateway Supervisor
	// get stateMachineAliasFilter if possible
	ConfigurationTree configLinkNode =
		CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(supervisorContextUID_, supervisorApplicationUID_);

	std::string stateMachineAliasFilter = "*";  // default to all
	if(fsmName != "" && !configLinkNode.isDisconnected())
	{
		try  // for backwards compatibility
		{
			ConfigurationTree fsmLinkNode = configLinkNode.getNode("LinkToStateMachineTable");
			if(!fsmLinkNode.isDisconnected() && !fsmLinkNode.getNode(fsmName + "/SystemAliasFilter").isDefaultValue())
				stateMachineAliasFilter = fsmLinkNode.getNode(fsmName + "/SystemAliasFilter").getValue<std::string>();
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

	__COUT__ << "For FSM '" << fsmName << ",' stateMachineAliasFilter  = " << stateMachineAliasFilter << __E__;


	// filter list of aliases based on stateMachineAliasFilter
	//  ! as first character means choose those that do NOT match filter
	//	* can be used as wild card.
	{
		bool                     invertFilter = stateMachineAliasFilter.size() && stateMachineAliasFilter[0] == '!';
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
				if(filterArr[0] != "" && filterArr[0] != "*" && aliasMapPair.first != filterArr[0])
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
						if(aliasMapPair.first.rfind(filterArr[f]) != aliasMapPair.first.size() - filterArr[f].size())
						{
							filterMatch = false;
							break;
						}
					}
					else if((i = aliasMapPair.first.find(filterArr[f])) == std::string::npos)
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
			xmlOut.addTextElementToData("config_key", TableGroupKey::getFullGroupString(aliasMapPair.second.first, aliasMapPair.second.second, /*decorate as (<key>)*/ "(",")"));

			// __COUT__ << "config_alias_comment" << " " <<  temporaryConfigMgr.getNode(
			// 	ConfigurationManager::GROUP_ALIASES_TABLE_NAME).getNode(aliasMapPair.first).getNode(
			// 		TableViewColumnInfo::COL_NAME_COMMENT).getValue<std::string>() << __E__;
			xmlOut.addTextElementToData("config_alias_comment",
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
	std::string fn =
		ConfigurationManager::LAST_TABLE_GROUP_SAVE_PATH + "/" + FSM_LAST_GROUP_ALIAS_FILE_START + fsmName + "." + FSM_USERS_PREFERENCES_FILETYPE;
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
	else if(aliasMap.size()) //if not set, return first
		xmlOut.addTextElementToData("UserLastConfigAlias", aliasMap.begin()->first);
} //end addFilteredConfigAliasesToXML()

//==============================================================================
// launchStartOneServerCommand
//	static function (so WizardSupervisor can use it)
//	throws exception if command fails to start a server
void GatewaySupervisor::launchStartOneServerCommand(const std::string& command, ConfigurationManager* cfgMgr, std::string& contextName)
{
	__COUT__ << "launch ots script Command = " << command << __E__;
	__COUT__ << "Extracting target context hostname... " << __E__;

	std::string hostname;
	try
	{
		cfgMgr->init();  // completely reset to re-align with any changes
		const 	XDAQContextTable* contextTable = cfgMgr->__GET_CONFIG__(XDAQContextTable);
		auto	contexts = contextTable->getContexts();

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
			__COUT__ << "ots script command '" << command << "' launching on hostname = " << hostname << " in context name " << context.contextUID_ << __E__;
		}
	}
	catch(...)
	{
		__SS__ << "\nRelaunch of otsdaq interrupted! "
		       << "The Configuration Manager could not be initialized." << __E__;

		__SS_THROW__;
	}

	std::string fn = (std::string(__ENV__("SERVICE_DATA_PATH")) + "/StartOTS_action_" + hostname + ".cmd");
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

	fn = (std::string(__ENV__("SERVICE_DATA_PATH")) + "/StartOTS_action_" + hostname + ".cmd");
	fp = fopen(fn.c_str(), "r");
	if(fp)
	{
		char line[100];
		fgets(line, 100, fp);
		fclose(fp);

		if(strcmp(line, command.c_str()) == 0)
		{
			__SS__ << "The command looks to have been ignored by " << hostname << ". Is the ots launch script still running on that node?" << __E__;
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
// launchStartOTSCommand
//	static function (so WizardSupervisor can use it)
//	throws exception if command fails to start
void GatewaySupervisor::launchStartOTSCommand(const std::string& command, ConfigurationManager* cfgMgr)
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
			__COUT__ << "ots script command '" << command << "' launching on hostname = " << hostnames.back() << __E__;
		}
	}
	catch(...)
	{
		__SS__ << "\nRelaunch of otsdaq interrupted! "
		       << "The Configuration Manager could not be initialized." << __E__;

		__SS_THROW__;
	}

	for(const auto& hostname : hostnames)
	{
		std::string fn = (std::string(__ENV__("SERVICE_DATA_PATH")) + "/StartOTS_action_" + hostname + ".cmd");
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
	// note: StartOTS.sh has a sleep of 1 second

	for(const auto& hostname : hostnames)
	{
		std::string fn = (std::string(__ENV__("SERVICE_DATA_PATH")) + "/StartOTS_action_" + hostname + ".cmd");
		FILE*       fp = fopen(fn.c_str(), "r");
		if(fp)
		{
			char line[100];
			fgets(line, 100, fp);
			fclose(fp);

			if(strcmp(line, command.c_str()) == 0)
			{
				__SS__ << "The command looks to have been ignored by " << hostname << ". Is the ots launch script still running on that node?" << __E__;
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
// xoap::supervisorCookieCheck
//	verify cookie
xoap::MessageReference GatewaySupervisor::supervisorCookieCheck(xoap::MessageReference message)

{
	__COUTT__ << __E__;

	// SOAPUtilities::receive request parameters
	SOAPParameters parameters;
	parameters.addParameter("CookieCode");
	parameters.addParameter("RefreshOption");
	parameters.addParameter("IPAddress");
	SOAPUtilities::receive(message, parameters);
	std::string cookieCode    = parameters.getValue("CookieCode");
	std::string refreshOption = parameters.getValue("RefreshOption");  // give external supervisors option to
	                                                                   // refresh cookie or not, "1" to refresh
	std::string ipAddress = parameters.getValue("IPAddress");          // give external supervisors option to refresh
	                                                                   // cookie or not, "1" to refresh

	// If TRUE, cookie code is good, and refreshed code is in cookieCode, also pointers
	// optionally for uint8_t userPermissions, uint64_t uid  Else, error message is
	// returned in cookieCode
	std::map<std::string /*groupName*/, WebUsers::permissionLevel_t> userGroupPermissionsMap;
	std::string                                                      userWithLock = "";
	uint64_t                                                         uid, userSessionIndex;
	theWebUsers_.cookieCodeIsActiveForRequest(
	    cookieCode, &userGroupPermissionsMap, &uid /*uid is not given to remote users*/,
		ipAddress, refreshOption == "1", &userWithLock, &userSessionIndex);

	__COUTTV__(userWithLock);

	// fill return parameters
	SOAPParameters retParameters;
	retParameters.addParameter("CookieCode", cookieCode);
	retParameters.addParameter("Permissions", StringMacros::mapToString(userGroupPermissionsMap).c_str());
	retParameters.addParameter("UserWithLock", userWithLock);
	retParameters.addParameter("Username", theWebUsers_.getUsersUsername(uid));
	retParameters.addParameter("DisplayName", theWebUsers_.getUsersDisplayName(uid));
	retParameters.addParameter("UserSessionIndex", std::to_string(userSessionIndex));

	__COUTT__ << "Login response: " << retParameters.getValue("Username") << __E__;

	return SOAPUtilities::makeSOAPMessageReference("CookieResponse", retParameters);
}  // end supervisorCookieCheck()

//==============================================================================
// xoap::supervisorGetActiveUsers
//	get display names for all active users
xoap::MessageReference GatewaySupervisor::supervisorGetActiveUsers(xoap::MessageReference /*message*/)
{
	__COUT__ << __E__;

	SOAPParameters parameters("UserList", theWebUsers_.getActiveUsersString());
	return SOAPUtilities::makeSOAPMessageReference("ActiveUserResponse", parameters);
}  // end supervisorGetActiveUsers()

//==============================================================================
// xoap::supervisorSystemMessage
//	SOAPUtilities::receive a new system Message from a supervisor
//	ToUser wild card * is to all users
//	or comma-separated variable  (CSV) to multiple users
xoap::MessageReference GatewaySupervisor::supervisorSystemMessage(xoap::MessageReference message)
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
//static add system message (e.g. from remote monitoring)
void GatewaySupervisor::addSystemMessage(std::string toUserCSV, std::string message)
{
	__COUTTV__(toUserCSV);
	__COUTVS__(45,message);
	GatewaySupervisor::theWebUsers_.addSystemMessage(toUserCSV, message);
} //end addSystemMessage

//===================================================================================================================
// xoap::supervisorSystemLogbookEntry
//	SOAPUtilities::receive a new system Message from a supervisor
//	ToUser wild card * is to all users
xoap::MessageReference GatewaySupervisor::supervisorSystemLogbookEntry(xoap::MessageReference message)
{
	SOAPParameters parameters;
	parameters.addParameter("EntryText");
	SOAPUtilities::receive(message, parameters);

	__COUT__ << "EntryText: " << parameters.getValue("EntryText").substr(0, 10) << __E__;

	makeSystemLogEntry(parameters.getValue("EntryText"));

	return SOAPUtilities::makeSOAPMessageReference("SystemLogbookResponse");
} //end supervisorSystemLogbookEntry()

//===================================================================================================================
// supervisorLastTableGroupRequest
//	return the group name and key for the last state machine activity
//
//	Note: same as OtsConfigurationWizardSupervisor::supervisorLastTableGroupRequest
xoap::MessageReference GatewaySupervisor::supervisorLastTableGroupRequest(xoap::MessageReference message)
{
	SOAPParameters parameters;
	parameters.addParameter("ActionOfLastGroup");
	SOAPUtilities::receive(message, parameters);

	return GatewaySupervisor::lastTableGroupRequestHandler(parameters);
} //end supervisorLastTableGroupRequest()

//===================================================================================================================
// xoap::lastTableGroupRequestHandler
//	handles last config group request.
//	called by both:
//		GatewaySupervisor::supervisorLastTableGroupRequest
//		OtsConfigurationWizardSupervisor::supervisorLastTableGroupRequest
xoap::MessageReference GatewaySupervisor::lastTableGroupRequestHandler(const SOAPParameters& parameters)
{
	std::string action = parameters.getValue("ActionOfLastGroup");
	__COUT__ << "ActionOfLastGroup: " << action.substr(0, 30) << __E__;

	std::vector<std::string> actions;
	std::vector<std::string> fileNames;
	if(action == "ALL")
	{
		actions = std::vector<std::string>({"Configured",
			"Started","ActivatedConfig","ActivatedContext","ActivatedBackbone",
			"ActivatedIterator"});
		fileNames = std::vector<std::string>({FSM_LAST_CONFIGURED_GROUP_ALIAS_FILE,
			FSM_LAST_STARTED_GROUP_ALIAS_FILE,ConfigurationManager::LAST_ACTIVATED_CONFIG_GROUP_FILE,
			ConfigurationManager::LAST_ACTIVATED_CONTEXT_GROUP_FILE, ConfigurationManager::LAST_ACTIVATED_BACKBONE_GROUP_FILE,
			ConfigurationManager::LAST_ACTIVATED_ITERATOR_GROUP_FILE});	
	}
	else 
	{
		actions.push_back(action);

		if(action == "Configured")
			fileNames.push_back(FSM_LAST_CONFIGURED_GROUP_ALIAS_FILE); 
		else if(action == "Started")
			fileNames.push_back(FSM_LAST_STARTED_GROUP_ALIAS_FILE);
		else if(action == "ActivatedConfig")
			fileNames.push_back(ConfigurationManager::LAST_ACTIVATED_CONFIG_GROUP_FILE);
		else if(action == "ActivatedContext")
			fileNames.push_back(ConfigurationManager::LAST_ACTIVATED_CONTEXT_GROUP_FILE);
		else if(action == "ActivatedBackbone")
			fileNames.push_back(ConfigurationManager::LAST_ACTIVATED_BACKBONE_GROUP_FILE);
		else if(action == "ActivatedIterator")
			fileNames.push_back(ConfigurationManager::LAST_ACTIVATED_ITERATOR_GROUP_FILE);
		else
		{
			__COUT_ERR__ << "Invalid last group action requested." << __E__;
			return SOAPUtilities::makeSOAPMessageReference("LastConfigGroupResponseFailure");
		}
	}

	std::string groupNames = "";
	std::string groupKeys = "";
	std::string groupActions = "";
	std::string groupTimes = "";
	for(size_t i=0; i < fileNames.size(); ++i)
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

		groupNames 		+= theGroup.first;
		groupKeys	 	+= theGroup.second.toString();
		groupActions 	+= actions[i];
		groupTimes 		+= timeString;

	}
	// fill return parameters
	SOAPParameters retParameters;
	retParameters.addParameter("GroupName", groupNames);//theGroup.first);
	retParameters.addParameter("GroupKey", groupKeys);//theGroup.second.toString());
	retParameters.addParameter("GroupAction", groupActions);//action);
	retParameters.addParameter("GroupActionTime", groupTimes);//timeString);

	return SOAPUtilities::makeSOAPMessageReference("LastConfigGroupResponse", retParameters);
} //end lastTableGroupRequestHandler()

//==============================================================================
// getNextRunNumber
//
//	If fsmName is passed, then get next run number for that FSM name
//	Else get next run number for the active FSM name, activeStateMachineName_
//
// 	Note: the FSM name is sanitized of special characters and used in the filename.
unsigned int GatewaySupervisor::getNextRunNumber(const std::string& fsmNameIn)
{
	std::string runNumberFileName = RUN_NUMBER_PATH + "/";
	std::string fsmName           = fsmNameIn == "" ? activeStateMachineName_ : fsmNameIn;
	// prepend sanitized FSM name
	for(unsigned int i = 0; i < fsmName.size(); ++i)
		if((fsmName[i] >= 'a' && fsmName[i] <= 'z') || (fsmName[i] >= 'A' && fsmName[i] <= 'Z') || (fsmName[i] >= '0' && fsmName[i] <= '9'))
			runNumberFileName += fsmName[i];
	runNumberFileName += RUN_NUMBER_FILE_NAME;
	//__COUT__ << "runNumberFileName: " << runNumberFileName << __E__;

	std::ifstream runNumberFile(runNumberFileName.c_str());
	if(!runNumberFile.is_open())
	{
		__COUT__ << "Cannot open file: " << runNumberFileName << __E__;

		__COUT__ << "Creating file and setting Run Number to 1: " << runNumberFileName << __E__;
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
} // end getNextRunNumber()

//==============================================================================
void GatewaySupervisor::setNextRunNumber(unsigned int runNumber, const std::string& fsmNameIn)
{
	std::string runNumberFileName = RUN_NUMBER_PATH + "/";
	std::string fsmName           = fsmNameIn == "" ? activeStateMachineName_ : fsmNameIn;
	// prepend sanitized FSM name
	for(unsigned int i = 0; i < fsmName.size(); ++i)
		if((fsmName[i] >= 'a' && fsmName[i] <= 'z') || (fsmName[i] >= 'A' && fsmName[i] <= 'Z') || (fsmName[i] >= '0' && fsmName[i] <= '9'))
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
// getLastLogEntry
//
//	If fsmName is passed, then get last log entry for that FSM name and transition type
//	Else for the active FSM name, activeStateMachineName_
//
// 	Note: the FSM name is sanitized of special characters and used in the filename.
std::string GatewaySupervisor::getLastLogEntry(const std::string& logType, const std::string& fsmNameIn /* = "" */)
{
	std::string logEntryFileName = LOG_ENTRY_PATH + "/";
	std::string fsmName           = fsmNameIn == "" ? activeStateMachineName_ : fsmNameIn;

	if(logType == RunControlStateMachine::START_TRANSITION_NAME &&
		 stateMachineStartLogEntry_.find(fsmName) != stateMachineStartLogEntry_.end())
		return stateMachineStartLogEntry_.at(fsmName);
	else if(logType == RunControlStateMachine::CONFIGURE_TRANSITION_NAME &&
		 stateMachineConfigureLogEntry_.find(fsmName) != stateMachineConfigureLogEntry_.end())
		return stateMachineConfigureLogEntry_.at(fsmName);

	// prepend sanitized FSM name
	for(unsigned int i = 0; i < fsmName.size(); ++i)
		if((fsmName[i] >= 'a' && fsmName[i] <= 'z') || (fsmName[i] >= 'A' && fsmName[i] <= 'Z') || (fsmName[i] >= '0' && fsmName[i] <= '9'))
			logEntryFileName += fsmName[i];
	logEntryFileName += "_" + logType + "_" + LOG_ENTRY_FILE_NAME;
	__SUP_COUTTV__(logEntryFileName);

	std::string contents;
	std::FILE* fp = std::fopen(logEntryFileName.c_str(), "rb");
	if(!fp)
	{
		__SUP_COUTT__ << "Could not open file at " << logEntryFileName << 
			". Error: " << errno << " - " << strerror(errno) << __E__;
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

	return contents;
} // end getLastLogEntry()

//==============================================================================
// setLastLogEntry
//
//	If fsmName is passed, then get last log entry for that FSM name and transition type
//	Else for the active FSM name, activeStateMachineName_
//
// 	Note: the FSM name is sanitized of special characters and used in the filename.
void GatewaySupervisor::setLastLogEntry(const std::string& logType, const std::string& logEntry, const std::string& fsmNameIn /* = "" */)
{
	std::string logEntryFileName = LOG_ENTRY_PATH + "/";
	std::string fsmName           = fsmNameIn == "" ? activeStateMachineName_ : fsmNameIn;

	if(logType == RunControlStateMachine::START_TRANSITION_NAME)
		stateMachineStartLogEntry_[fsmName] = logEntry;
	else if(logType == RunControlStateMachine::CONFIGURE_TRANSITION_NAME)
		stateMachineConfigureLogEntry_[fsmName] = logEntry;
	else return; //for now,  do not save other types of transitions
	
	// prepend sanitized FSM name
	for(unsigned int i = 0; i < fsmName.size(); ++i)
		if((fsmName[i] >= 'a' && fsmName[i] <= 'z') || (fsmName[i] >= 'A' && fsmName[i] <= 'Z') || (fsmName[i] >= '0' && fsmName[i] <= '9'))
			logEntryFileName += fsmName[i];
	logEntryFileName += "_" + logType + "_" + LOG_ENTRY_FILE_NAME;
	__COUTTV__(logEntryFileName);
	__COUTTV__(logType);
	__COUTTV__(logEntry);

	std::FILE* fp = std::fopen(logEntryFileName.c_str(), "w");
	if(!fp)
	{
		__SUP_SS__ << "Could not open file at " << logEntryFileName << 
			". Error: " << errno << " - " << strerror(errno) << __E__;
		__SUP_SS_THROW__;
	}
	if(logEntry.size())
		std::fwrite(&logEntry[0], 1, logEntry.size(), fp);
	fclose(fp);
}  // end setLastLogEntry()

//==============================================================================
// loadRemoteGatewaySettings
//
//	 If editing remoteGatewayApps_, assume already locked remoteGatewayAppsMutex_
//
//	Load from file into vector of Remote Gateways passed by reference.
//	onlyNotFound := load only settings for remoteGateways not currently in vector (keep existing settings, e.g. right before a save)
void GatewaySupervisor::loadRemoteGatewaySettings(std::vector<GatewaySupervisor::RemoteGatewayInfo>& remoteGateways,
	bool onlyNotFound /* = false */) const
{	
	std::string filepath = std::string(__ENV__("SERVICE_DATA_PATH")) + "/" + REMOTE_SUBSYSTEM_SETTINGS_FILE_NAME;
	__SUP_COUTV__(filepath);

	std::ifstream settingsFile(filepath.c_str());
	if(!settingsFile.is_open())
	{
		__SUP_COUT__ << "Cannot open Remote Gateway settings file (assuming no settings yet!): " << filepath << __E__;

		__SUP_COUT__ << "Creating empty Remote Gateway settings file: " << filepath << __E__;
		FILE* fp = fopen(filepath.c_str(), "w");
		fprintf(fp, "\n");
		fclose(fp);

		settingsFile.open(filepath.c_str());
		if(!settingsFile.is_open())
		{
			__SUP_SS__ << "Error. Cannot create or load Remote Gateway settings file: " << filepath << __E__;
			__SUP_SS_THROW__;
		}
	}

	size_t NUM_FIELDS = 3; //name, fsmMode, included
	std::vector<std::string> values;
	float formatVersion = 0.0;
	bool done = false;
    do  // Read each line from the file
	{  
		size_t i=0;
		for(i=0; i<NUM_FIELDS; ++i)
		{
			if(i >= values.size()) values.push_back(""); //init values vector

			if(!std::getline(settingsFile, values[i]))
			{
				//no more lines left
				if(i) //at illegal moment mid-record?
				{
					settingsFile.close();
					__SUP_SS__ << "Error. Illegal file format in Remote Gateway settings file: " << filepath << __E__;
					__SUP_SS_THROW__;
				}
				//else end is correctly at record boundary
				done = true;
				break;
			}
			__SUP_COUTVS__(20,values[i]);

			if(values[i] == "")  //do not allow blank lines				
			{
				//rewind
				--i;
				continue;
			}
			else if(values[i].find("Remote Gateway Settings, file format v") != //grab format version if present
				std::string::npos)
			{				
				sscanf(values[i].c_str(),"Remote Gateway Settings, file format v%f",&formatVersion);
				__SUP_COUTV__(formatVersion);

				if(formatVersion > 0.5)
					NUM_FIELDS = 4; //name, fsmMode, included, selected_config_alias

				__SUP_COUTV__(NUM_FIELDS);
				//rewind
				--i;
				continue;
			}

		} //end record value load
		if(done) break;

		//at this point values vector complete for Remote Gateway

		bool found = false;
		for(i=0; i<remoteGateways.size(); ++i)
			if(values[0] == remoteGateways[i].appInfo.name)
			{
				found = true; 
				break;
			}

		if(!found) //create Remote Gateway (and i will be correctly pointing to back())		
		{
			remoteGateways.push_back(GatewaySupervisor::RemoteGatewayInfo());
			remoteGateways[i].appInfo.name = values[0];
		}
		else if(onlyNotFound)
			continue; //skip modifying current settings

		
		remoteGateways[i].fsm_mode = values[1] == "Do Not Halt"?RemoteGatewayInfo::FSM_ModeTypes::DoNotHalt:
							(values[1] == "Only Configure"?RemoteGatewayInfo::FSM_ModeTypes::OnlyConfigure:
								RemoteGatewayInfo::FSM_ModeTypes::Follow_FSM);
		remoteGateways[i].fsm_included = values[2] == "1"?true:false;
		if(values.size() > 3)
			remoteGateways[i].selected_config_alias = values[3];

		__SUP_COUT__ << "Loaded Remote Gateway '" << remoteGateways[i].appInfo.name << "' ==> " <<
			remoteGateways[i].getFsmMode() << " :" << remoteGateways[i].fsm_included << 
			"configAlias=" << remoteGateways[i].selected_config_alias << __E__;

	} while (1); //end file read loop

	settingsFile.close();
} //end loadRemoteGatewaySettings()

//==============================================================================
void GatewaySupervisor::saveRemoteGatewaySettings() const
{
	std::string filepath = std::string(__ENV__("SERVICE_DATA_PATH")) + "/" + REMOTE_SUBSYSTEM_SETTINGS_FILE_NAME;
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
	settingsFile << "Remote Gateway Settings, file format v1.0" << __E__; //save file format version first
	for(size_t i=0; i<remoteGateways.size(); ++i)
	{
		settingsFile << remoteGateways[i].appInfo.name << __E__;
		settingsFile << remoteGateways[i].getFsmMode() << __E__;
		settingsFile << std::string(remoteGateways[i].fsm_included?"1":"0") << __E__;
		settingsFile << remoteGateways[i].selected_config_alias << __E__;
	}

	settingsFile.close();
}  // end saveRemoteGatewaySettings()

//==============================================================================
void GatewaySupervisor::handleGetApplicationIdRequest(AllSupervisorInfo* allSupervisorInfo, cgicc::Cgicc& cgiIn, HttpXmlDocument& xmlOut)
{
	std::string classNeedle = StringMacros::decodeURIComponent(CgiDataUtilities::getData(cgiIn, "classNeedle"));
	__COUTV__(classNeedle);

	for(auto it : allSupervisorInfo->getAllSupervisorInfo())
	{
		// bool pass = true;

		auto appInfo = it.second;

		if(classNeedle != appInfo.getClass())
			continue;  // skip non-matches

		xmlOut.addTextElementToData("name",
		                            appInfo.getName());                      // get application name
		xmlOut.addTextElementToData("id", std::to_string(appInfo.getId()));  // get application id
		xmlOut.addTextElementToData("class",
		                            appInfo.getClass());  // get application class
		xmlOut.addTextElementToData("url",
		                            appInfo.getURL());  // get application url
		xmlOut.addTextElementToData("context",
		                            appInfo.getContextName());  // get context
	}

}  // end handleGetApplicationIdRequest()

//==============================================================================
bool GatewaySupervisor::handleAddDesktopIconRequest(const std::string&                          author,
                                                    cgicc::Cgicc&                               cgiIn,
                                                    HttpXmlDocument&                            xmlOut,
                                                    std::vector<DesktopIconTable::DesktopIcon>* newIcons /* = nullptr*/)
{
	std::string iconCaption     = CgiDataUtilities::getData(cgiIn, "iconCaption");      // from GET
	std::string iconAltText     = CgiDataUtilities::getData(cgiIn, "iconAltText");      // from GET
	std::string iconFolderPath  = CgiDataUtilities::getData(cgiIn, "iconFolderPath");   // from GET
	std::string iconImageURL    = CgiDataUtilities::getData(cgiIn, "iconImageURL");     // from GET
	std::string iconWindowURL   = CgiDataUtilities::getData(cgiIn, "iconWindowURL");    // from GET
	std::string iconPermissions = CgiDataUtilities::getData(cgiIn, "iconPermissions");  // from GET
	// windowLinkedApp is one of the only fields that needs to be decoded before write into table cells, because the app class name might be here
	std::string  windowLinkedApp          = CgiDataUtilities::getData(cgiIn, "iconLinkedApp");                                       // from GET
	unsigned int windowLinkedAppLID       = CgiDataUtilities::getDataAsInt(cgiIn, "iconLinkedAppLID");                               // from GET
	bool         enforceOneWindowInstance = CgiDataUtilities::getData(cgiIn, "iconEnforceOneWindowInstance") == "1" ? true : false;  // from GET

	std::string windowParameters = StringMacros::decodeURIComponent(CgiDataUtilities::postData(cgiIn, "iconParameters"));  // from POST

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

	bool success = ConfigurationSupervisorBase::handleAddDesktopIconXML(xmlOut,
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

		const std::vector<DesktopIconTable::DesktopIcon>& tmpNewIcons = tmpCfgMgr.__GET_CONFIG__(DesktopIconTable)->getAllDesktopIcons();

		newIcons->clear();
		for(const auto& tmpNewIcon : tmpNewIcons)
			newIcons->push_back(tmpNewIcon);
	}

	return success;
}  // end handleAddDesktopIconRequest()

//==============================================================================
xoap::MessageReference GatewaySupervisor::TRACESupervisorRequest(xoap::MessageReference message)
{
	return CorePropertySupervisorBase::TRACESupervisorRequest(message);
}  // end TRACESupervisorRequest()
