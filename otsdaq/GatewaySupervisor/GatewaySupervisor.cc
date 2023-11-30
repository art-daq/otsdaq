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

#include "otsdaq/NetworkUtilities/TransceiverSocket.h"  // for UDP state changer

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
#define FSM_LAST_GROUP_ALIAS_FILE_START std::string("FSMLastGroupAlias-")
#define FSM_USERS_PREFERENCES_FILETYPE "pref"

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
void GatewaySupervisor::indicateOtsAlive(const CorePropertySupervisorBase* properties) { CorePropertySupervisorBase::indicateOtsAlive(properties); }

//==============================================================================
void GatewaySupervisor::init(void)
{
	supervisorGuiHasBeenLoaded_ = false;

	// setting up thread for UDP thread to drive state machine
	{
		bool enableStateChanges = false;
		try
		{
			enableStateChanges = CorePropertySupervisorBase::getSupervisorTableNode().getNode("EnableStateChangesOverUDP").getValue<bool>();
		}
		catch(...)
		{
			;
		}  // ignore errors

		if(enableStateChanges)
		{
			__COUT__ << "Enabling state changes over UDP..." << __E__;
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

	std::map<std::string /* appName */, bool /* lastStatusGood */> appLastStatusGood;

	while(1)
	{
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
				// send back status and progress parameters
				const std::string& err = theSupervisor->theStateMachine_.getErrorMessage();

				// status = err == "" ? (theSupervisor->theStateMachine_.isInTransition() ? theSupervisor->theStateMachine_.getProvenanceStateName()
				//                                                                        : theSupervisor->theStateMachine_.getCurrentStateName())
				//                    : (theSupervisor->theStateMachine_.getCurrentStateName() == "Paused" ? "Soft-Error:::" : "Failed:::") + err;

				if(err == "")
				{
					if(theSupervisor->theStateMachine_.isInTransition() || theSupervisor->theProgressBar_.read() < 100)
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
					}
					else
						status = theSupervisor->theStateMachine_.getCurrentStateName();
				}
				else
					status = (theSupervisor->theStateMachine_.getCurrentStateName() == "Paused" ? "Soft-Error:::" : "Failed:::") + err;

				progress = theSupervisor->theProgressBar_.readPercentageString();

				try
				{
					detail = (theSupervisor->theStateMachine_.isInTransition()
					              ? theSupervisor->theStateMachine_.getCurrentTransitionName(theSupervisor->stateMachineLastCommandInput_)
					              : "");
					// make sure broadcast message status is not being updated
					std::lock_guard<std::mutex> lock(theSupervisor->broadcastCommandStatusUpdateMutex_);
					if(detail != "" && theSupervisor->broadcastCommandStatus_ != "")
						detail += " - " + theSupervisor->broadcastCommandStatus_;
				}
				catch(...)
				{
					detail = "";
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

					subapps = SupervisorInfo::deserializeSubappInfos(parameters.getValue("Subapps"));

					if(!appLastStatusGood[appName])
					{
						__COUT__ << "First good status from "
						         << " Supervisor instance = '" << appName << "' [LID=" << appInfo.getId() << "] in Context '" << appInfo.getContextName()
						         << "' [URL=" << appInfo.getURL() << "].\n\n";
						__COUTV__(SOAPUtilities::translate(tempMessage));
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
						__COUTV__(SOAPUtilities::translate(tempMessage));
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
			sleep(5);  // sleep to not overwhelm server with errors
	}                  // end of infinite status checking loop
}  // end AppStatusWorkLoop

//==============================================================================
// StateChangerWorkLoop
//	child thread
void GatewaySupervisor::StateChangerWorkLoop(GatewaySupervisor* theSupervisor)
{
	ConfigurationTree configLinkNode = theSupervisor->CorePropertySupervisorBase::getSupervisorTableNode();

	std::string ipAddressForStateChangesOverUDP = configLinkNode.getNode("IPAddressForStateChangesOverUDP").getValue<std::string>();
	int         portForStateChangesOverUDP      = configLinkNode.getNode("PortForStateChangesOverUDP").getValue<int>();
	bool        acknowledgementEnabled          = configLinkNode.getNode("EnableAckForStateChangesOverUDP").getValue<bool>();

	//__COUT__ << "IPAddressForStateChangesOverUDP = " << ipAddressForStateChangesOverUDP
	//<< __E__;
	//__COUT__ << "PortForStateChangesOverUDP      = " << portForStateChangesOverUDP <<
	//__E__;
	//__COUT__ << "acknowledgmentEnabled           = " << acknowledgmentEnabled << __E__;

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
		__COUT__ << ss.str();
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
			__COUT__ << "UDP State Changer packet received of size = " << buffer.size() << __E__;

			size_t nCommas = std::count(buffer.begin(), buffer.end(), ',');
			if(nCommas == 0)
			{
				__SS__ << "Unrecognized State Machine command :-" << buffer
				       << "-. Format is FiniteStateMachineName,Command,Parameter(s). "
				          "Where Parameter(s) is/are optional."
				       << __E__;
				__COUT_INFO__ << ss.str();
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
				__COUT__ << "Word: " << buffer.substr(begin, commaPosition - begin) << __E__;

				begin = commaPosition + 1;
				++commaCounter;
			}

			// set scope of mutex
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
				    0, 0, command, fsmName, WebUsers::DEFAULT_STATECHANGER_USERNAME /*fsmWindowName*/, WebUsers::DEFAULT_STATECHANGER_USERNAME, parameters);
			}

			if(errorStr != "")
			{
				__SS__ << "UDP State Changer failed to execute command because of the "
				          "following error: "
				       << errorStr;
				__COUT_ERR__ << ss.str();
				if(acknowledgementEnabled)
					sock.acknowledge(errorStr, true /*verbose*/);
			}
			else
			{
				__SS__ << "Successfully executed state change command '" << command << ".'" << __E__;
				__COUT_INFO__ << ss.str();
				if(acknowledgementEnabled)
					sock.acknowledge("Done", true /*verbose*/);
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

	/////////////////
	// Validate FSM name
	//	if fsm name != active fsm name
	//		only allow, if current state is halted or init
	//		take active fsm name when configured
	//	else, allow
	if(activeStateMachineName_ != "" && activeStateMachineName_ != fsmName)
	{
		__COUT__ << "currentState = " << currentState << __E__;
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
			__COUT_ERR__ << "\n" << ss.str();

			xmlOut.addTextElementToData("state_tranisition_attempted",
			                            "0");  // indicate to GUI transition NOT attempted
			xmlOut.addTextElementToData("state_tranisition_attempted_err",
			                            ss.str());  // indicate to GUI transition NOT attempted
			xmlOut.outputXmlDocument((std::ostringstream*)out, false, true);
			return;
		}
		else  // clear active state machine
		{
			activeStateMachineName_       = "";
			activeStateMachineWindowName_ = "";
		}
	}

	// At this point, attempting transition!

	std::vector<std::string> parameters;

	if(command == "Configure")
		parameters.push_back(CgiDataUtilities::postData(cgiIn, "ConfigurationAlias"));

	std::string logEntry = CgiDataUtilities::postData(cgiIn, "logEntry");

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
                                                             const std::string&              logEntry)
{
	std::string errorStr = "";

	std::string currentState = theStateMachine_.getCurrentStateName();
	__COUT__ << "State Machine command = " << command << __E__;
	__COUT__ << "fsmName = " << fsmName << __E__;
	__COUT__ << "fsmWindowName = " << fsmWindowName << __E__;
	__COUT__ << "activeStateMachineName_ = " << activeStateMachineName_ << __E__;
	__COUTV__(logEntry);
	__COUT__ << "command = " << command << __E__;
	__COUT__ << "commandParameters.size = " << commandParameters.size() << __E__;

	activeStateMachineLogEntry_ = "";  // clear

	SOAPParameters parameters;
	if(command == "Configure")
	{
		if(currentState != "Halted")  // check if out of sync command
		{
			__SS__ << "Error - Can only transition to Configured if the current "
			       << "state is Halted. Perhaps your state machine is out of sync." << __E__;
			__COUT_ERR__ << "\n" << ss.str();
			errorStr = ss.str();

			if(xmldoc)
				xmldoc->addTextElementToData("state_tranisition_attempted",
				                             "0");  // indicate to GUI transition NOT attempted
			if(xmldoc)
				xmldoc->addTextElementToData("state_tranisition_attempted_err",
				                             ss.str());  // indicate to GUI transition NOT attempted
			if(out)
				xmldoc->outputXmlDocument((std::ostringstream*)out, false /*dispStdOut*/, true /*allowWhiteSpace*/);

			return errorStr;
		}

		// NOTE Original name of the configuration key
		// parameters.addParameter("RUN_KEY",CgiDataUtilities::postData(cgi,"ConfigurationAlias"));
		if(commandParameters.size() == 0)
		{
			__SS__ << "Error - Can only transition to Configured if a Configuration "
			          "Alias parameter is provided."
			       << __E__;
			__COUT_ERR__ << "\n" << ss.str();
			errorStr = ss.str();

			if(xmldoc)
				xmldoc->addTextElementToData("state_tranisition_attempted",
				                             "0");  // indicate to GUI transition NOT attempted
			if(xmldoc)
				xmldoc->addTextElementToData("state_tranisition_attempted_err",
				                             ss.str());  // indicate to GUI transition NOT attempted
			if(out)
				xmldoc->outputXmlDocument((std::ostringstream*)out, false /*dispStdOut*/, true /*allowWhiteSpace*/);

			return errorStr;
		}

		// test Require user log info
		try
		{
			ConfigurationTree configLinkNode =
			    CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(supervisorContextUID_, supervisorApplicationUID_);
			if(!configLinkNode.isDisconnected())
			{
				ConfigurationTree fsmLinkNode         = configLinkNode.getNode("LinkToStateMachineTable").getNode(fsmName);
				bool              requireUserLogInput = fsmLinkNode.getNode("RequireUserLogInputOnConfigureTransition").getValue<bool>();
				__COUTV__(requireUserLogInput);
				if(requireUserLogInput && logEntry.size() < 3)
				{
					__SS__ << "Error - the state machine property 'RequireUserLogInputOnConfigureTransition' has been enabled which requires the user to enter "
					          "at least 3 characters of log info to proceed with the Configure transition."
					       << __E__;
					__COUT_ERR__ << "\n" << ss.str();
					errorStr = ss.str();

					if(xmldoc)
						xmldoc->addTextElementToData("state_tranisition_attempted",
						                             "0");  // indicate to GUI transition NOT attempted
					if(xmldoc)
						xmldoc->addTextElementToData("state_tranisition_attempted_err",
						                             ss.str());  // indicate to GUI transition NOT attempted
					if(out)
						xmldoc->outputXmlDocument((std::ostringstream*)out, false /*dispStdOut*/, true /*allowWhiteSpace*/);

					return errorStr;
				}
				else if(requireUserLogInput)
					activeStateMachineLogEntry_ = logEntry;
			}
		}
		catch(...)
		{ /* ignore exceptions during test Require user log info*/
		}

		parameters.addParameter("ConfigurationAlias", commandParameters[0]);

		std::string configurationAlias = parameters.getValue("ConfigurationAlias");
		__COUT__ << "Configure --> Name: ConfigurationAlias Value: " << configurationAlias << __E__;
		lastConfigurationAlias_ = configurationAlias;
		// save last used config alias by user
		std::string fn =
		    ConfigurationManager::LAST_TABLE_GROUP_SAVE_PATH + "/" + FSM_LAST_GROUP_ALIAS_FILE_START + username + "." + FSM_USERS_PREFERENCES_FILETYPE;

		__COUT__ << "Save FSM preferences: " << fn << __E__;
		FILE* fp = fopen(fn.c_str(), "w");
		if(!fp)
		{
			__SS__ << ("Could not open file: " + fn) << __E__;
			__COUT_ERR__ << ss.str();
			__SS_THROW__;
		}
		fprintf(fp, "FSM_last_configuration_alias %s", configurationAlias.c_str());
		fclose(fp);

		activeStateMachineName_       = fsmName;
		activeStateMachineWindowName_ = fsmWindowName;
	}
	else if(command == "Start")
	{
		if(currentState != "Configured")  // check if out of sync command
		{
			__SS__ << "Error - Can only transition to Configured if the current "
			       << "state is Halted. Perhaps your state machine is out of sync. "
			       << "(Likely the server was restarted or another user changed the state)" << __E__;
			__COUT_ERR__ << "\n" << ss.str();
			errorStr = ss.str();

			if(xmldoc)
				xmldoc->addTextElementToData("state_tranisition_attempted",
				                             "0");  // indicate to GUI transition NOT attempted
			if(xmldoc)
				xmldoc->addTextElementToData("state_tranisition_attempted_err",
				                             ss.str());  // indicate to GUI transition NOT attempted
			if(out)
				xmldoc->outputXmlDocument((std::ostringstream*)out, false /*dispStdOut*/, true /*allowWhiteSpace*/);

			return errorStr;
		}
		unsigned int runNumber;
		if(commandParameters.size() == 0)
		{
			runNumber = getNextRunNumber();
			std::stringstream dumpSs;
			// Check if run number should come from db, if so create run info record into database
			try
			{
				ConfigurationTree configLinkNode =
				    CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(supervisorContextUID_, supervisorApplicationUID_);
				if(!configLinkNode.isDisconnected())
				{
					ConfigurationTree fsmLinkNode = configLinkNode.getNode("LinkToStateMachineTable").getNode(activeStateMachineName_);

					std::string runInfoPluginType = fsmLinkNode.getNode("RunInfoPluginType").getValue<std::string>();
					__COUTV__(runInfoPluginType);
					if(runInfoPluginType != TableViewColumnInfo::DATATYPE_STRING_DEFAULT && runInfoPluginType != "No Run Info Plugin")
					{
						RunInfoVInterface* runInfoInterface = nullptr;
						try
						{
							std::string dumpFormat = fsmLinkNode.getNode("ConfigurationDumpOnRunFormat").getValue<std::string>();

							// dump configuration
							CorePropertySupervisorBase::theConfigurationManager_->dumpActiveConfiguration(
							    "",
							    dumpFormat,
							    "Configuration Alias: " + lastConfigurationAlias_ + "\n\n" + "Run note: " + StringMacros::decodeURIComponent(logEntry),
							    theWebUsers_.getActiveUsersString(),
							    dumpSs);

							runInfoInterface = makeRunInfo(runInfoPluginType, activeStateMachineName_);
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

						runNumber = runInfoInterface->claimNextRunNumber(dumpSs.str());
					}  // end Run Info Plugin handling

					// test Require user log info
					try
					{
						bool requireUserLogInput = fsmLinkNode.getNode("RequireUserLogInputOnRunTransition").getValue<bool>();
						__COUTV__(requireUserLogInput);
						if(requireUserLogInput && logEntry.size() < 3)
						{
							__SS__ << "Error - the state machine property 'RequireUserLogInputOnRunTransition' has been enabled which requires the user to "
							          "enter at least 3 characters of log info to proceed with the Start transition."
							       << __E__;
							__COUT_ERR__ << "\n" << ss.str();
							errorStr = ss.str();

							if(xmldoc)
								xmldoc->addTextElementToData("state_tranisition_attempted",
								                             "0");  // indicate to GUI transition NOT attempted
							if(xmldoc)
								xmldoc->addTextElementToData("state_tranisition_attempted_err",
								                             ss.str());  // indicate to GUI transition NOT attempted
							if(out)
								xmldoc->outputXmlDocument((std::ostringstream*)out, false /*dispStdOut*/, true /*allowWhiteSpace*/);

							return errorStr;
						}
						else if(requireUserLogInput)
							activeStateMachineLogEntry_ = logEntry;
					}
					catch(...)
					{ /* ignore exceptions during test Require user log info*/
					}

				}  // end handle state machine config link
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

			setNextRunNumber(runNumber + 1);
		}
		else
		{
			runNumber = std::atoi(commandParameters[0].c_str());
			setNextRunNumber(runNumber + 1);
		}
		parameters.addParameter("RunNumber", runNumber);
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

//==============================================================================
xoap::MessageReference GatewaySupervisor::stateMachineXoapHandler(xoap::MessageReference message)

{
	__COUT__ << "Soap Handler!" << __E__;
	stateMachineWorkLoopManager_.removeProcessedRequests();
	stateMachineWorkLoopManager_.processRequest(message);
	__COUT__ << "Done - Soap Handler!" << __E__;
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

	__COUT__ << "Propagating command '" << command << "'..." << __E__;

	std::string reply = send(allSupervisorInfo_.getGatewayDescriptor(), stateMachineWorkLoopManager_.getMessage(workLoop));
	stateMachineWorkLoopManager_.report(workLoop, reply, 100, true);

	__COUT__ << "Done with command '" << command << ".' Reply = " << reply << __E__;
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
					RunInfoVInterface* runInfoInterface = nullptr;
					try
					{
						runInfoInterface = makeRunInfo(runInfoPluginType, activeStateMachineName_);
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
					RunInfoVInterface* runInfoInterface = nullptr;
					try
					{
						runInfoInterface = makeRunInfo(runInfoPluginType, activeStateMachineName_);
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
					RunInfoVInterface* runInfoInterface = nullptr;
					try
					{
						runInfoInterface = makeRunInfo(runInfoPluginType, activeStateMachineName_);
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
					RunInfoVInterface* runInfoInterface = nullptr;
					try
					{
						runInfoInterface = makeRunInfo(runInfoPluginType, activeStateMachineName_);
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
					RunInfoVInterface* runInfoInterface = nullptr;
					try
					{
						runInfoInterface = makeRunInfo(runInfoPluginType, activeStateMachineName_);
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

	std::string systemAlias = SOAPUtilities::translate(theStateMachine_.getCurrentMessage()).getParameters().getValue("ConfigurationAlias");

	__COUT__ << "Transition parameter: " << systemAlias << __E__;

	RunControlStateMachine::theProgressBar_.step();

	try
	{
		CorePropertySupervisorBase::theConfigurationManager_->init();  // completely reset to re-align with any changes
	}
	catch(...)
	{
		__SS__ << "\nTransition to Configuring interrupted! "
		       << "The Configuration Manager could not be initialized." << __E__;

		__COUT_ERR__ << "\n" << ss.str();
		XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
		return;
	}

	RunControlStateMachine::theProgressBar_.step();

	// Translate the system alias to a group name/key
	try
	{
		theConfigurationTableGroup_ = CorePropertySupervisorBase::theConfigurationManager_->getTableGroupFromAlias(systemAlias);
	}
	catch(...)
	{
		__COUT_INFO__ << "Exception occurred" << __E__;
	}

	RunControlStateMachine::theProgressBar_.step();

	if(theConfigurationTableGroup_.second.isInvalid())
	{
		__SS__ << "\nTransition to Configuring interrupted! System Alias " << systemAlias << " could not be translated to a group name and key." << __E__;

		__COUT_ERR__ << "\n" << ss.str();
		XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
		return;
	}

	RunControlStateMachine::theProgressBar_.step();

	__COUT__ << "Configuration table group name: " << theConfigurationTableGroup_.first << " key: " << theConfigurationTableGroup_.second << __E__;

	// make logbook entry
	{
		std::stringstream ss;
		ss << "Configuring '" << systemAlias << "' which translates to " << theConfigurationTableGroup_.first << " (" << theConfigurationTableGroup_.second
		   << ").";

		if(activeStateMachineLogEntry_ != "")
			ss << " User log entry:\n " << StringMacros::decodeURIComponent(activeStateMachineLogEntry_);

		makeSystemLogEntry(ss.str());
	}  // end make logbook entry

	RunControlStateMachine::theProgressBar_.step();

	// load and activate
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
		__SS__ << "\nTransition to Configuring interrupted! System Alias " << systemAlias << " was translated to " << theConfigurationTableGroup_.first << " ("
		       << theConfigurationTableGroup_.second << ") but could not be loaded and initialized." << __E__;
		ss << "\n\nHere was the error: " << e.what()
		   << "\n\nTo help debug this problem, try activating this group in the Configuration "
		      "GUI "
		   << " and detailed errors will be shown." << __E__;
		__COUT_ERR__ << "\n" << ss.str();
		XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
		return;
	}
	catch(...)
	{
		__SS__ << "\nTransition to Configuring interrupted! System Alias " << systemAlias << " was translated to " << theConfigurationTableGroup_.first << " ("
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
		__COUT_ERR__ << "\n" << ss.str();
		XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
		return;
	}

	// check if configuration dump is enabled on configure transition
	{
		try
		{
			CorePropertySupervisorBase::theConfigurationManager_->dumpMacroMakerModeFhicl();
		}
		catch(...)  // ignore error for now
		{
			__COUT_ERR__ << "Failed to dump MacroMaker mode fhicl." << __E__;
		}

		ConfigurationTree configLinkNode =
		    CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(supervisorContextUID_, supervisorApplicationUID_);
		if(!configLinkNode.isDisconnected())
		{
			try  // errors in dump are not tolerated
			{
				bool        dumpConfiguration   = true;
				bool        requireUserLogInput = false;
				std::string dumpFilePath, dumpFileRadix, dumpFormat;

				bool doThrow = false;
				try  // for backwards compatibility
				{
					ConfigurationTree fsmLinkNode = configLinkNode.getNode("LinkToStateMachineTable").getNode(activeStateMachineName_);
					dumpConfiguration             = fsmLinkNode.getNode("EnableConfigurationDumpOnConfigureTransition").getValue<bool>();
					doThrow                       = true;  // at this point throw the exception!
					dumpFilePath = fsmLinkNode.getNode("ConfigurationDumpOnConfigureFilePath").getValueWithDefault<std::string>(__ENV__("OTSDAQ_LOG_DIR"));
					dumpFileRadix =
					    fsmLinkNode.getNode("ConfigurationDumpOnConfigureFileRadix").getValueWithDefault<std::string>("ConfigTransitionConfigurationDump");
					dumpFormat          = fsmLinkNode.getNode("ConfigurationDumpOnConfigureFormat").getValue<std::string>();
					requireUserLogInput = fsmLinkNode.getNode("RequireUserLogInputOnConfigureTransition").getValue<bool>();
				}
				catch(std::runtime_error& e)  // throw exception on missing fields if dumpConfiguration set
				{
					__COUT_INFO__ << "FSM configuration dump Link disconnected at '" << ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME << "/"
					              << supervisorContextUID_ << "/" << supervisorApplicationUID_ << "/"
					              << "LinkToStateMachineTable/" << activeStateMachineName_ << "/"
					              << "EnableConfigurationDumpOnConfigureTransition" << __E__;
					if(doThrow)
					{
						__SS__ << "Configuration Dump on Configure transition was enabled, but there are missing fields! " << e.what() << __E__;
						__SS_THROW__;
					}
					dumpConfiguration = false;
				}

				if(dumpConfiguration)
				{
					__COUT_INFO__ << "Dumping the Configuration on the Configuration transition..." << __E__;
					// dump configuration
					CorePropertySupervisorBase::theConfigurationManager_->dumpActiveConfiguration(
					    dumpFilePath + "/" + dumpFileRadix + "_" + std::to_string(time(0)) + ".dump",
					    dumpFormat,
					    requireUserLogInput ? activeStateMachineLogEntry_ : "",
					    theWebUsers_.getActiveUsersString());
				}
				else
					__COUT_INFO__ << "Not dumping the Configuration on the Configuration transition." << __E__;
			}
			catch(std::runtime_error& e)
			{
				__SS__ << "\nTransition to Configuring interrupted! There was an error "
				          "identified "
				       << "during the configuration dump attempt:\n\n " << e.what() << __E__;
				__COUT_ERR__ << "\n" << ss.str();
				XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
				return;
			}
			catch(...)
			{
				__SS__ << "\nTransition to Configuring interrupted! There was an error "
				          "identified "
				       << "during the configuration dump attempt.\n\n " << __E__;
				try	{ throw; } //one more try to printout extra info
				catch(const std::exception &e)
				{
					ss << "Exception message: " << e.what();
				}
				catch(...){}
				__COUT_ERR__ << "\n" << ss.str();
				XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
				return;
			}
		}
		else
			__COUT_INFO__ << "No Gateway Supervisor configuration record found at '" << ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME << "/"
			              << supervisorContextUID_ << "/" << supervisorApplicationUID_
			              << "' - consider adding one to control configuration dumps and state machine properties." << __E__;
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

	// xoap::MessageReference message =
	// SOAPUtilities::makeSOAPMessageReference(SOAPUtilities::translate(theStateMachine_.getCurrentMessage()).getCommand(),
	// parameters);
	xoap::MessageReference message = theStateMachine_.getCurrentMessage();
	SOAPUtilities::addParameters(message, parameters);
	broadcastMessage(message);
	RunControlStateMachine::theProgressBar_.step();
	// Advertise the exiting of this method
	// diagService_->reportError("GatewaySupervisor::stateConfiguring: Exiting",DIAGINFO);

	// save last configured group name/key
	ConfigurationManager::saveGroupNameAndKey(theConfigurationTableGroup_, FSM_LAST_CONFIGURED_GROUP_ALIAS_FILE);

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

	__COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName() << __E__;

	makeSystemLogEntry("System halting.");

	broadcastMessage(theStateMachine_.getCurrentMessage());
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

	SOAPParameters parameters("RunNumber");
	SOAPUtilities::receive(theStateMachine_.getCurrentMessage(), parameters);

	activeStateMachineRunNumber_ = parameters.getValue("RunNumber");
	__COUTV__(activeStateMachineRunNumber_);

	// check if configuration dump is enabled on configure transition
	{
		ConfigurationTree configLinkNode =
		    CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(supervisorContextUID_, supervisorApplicationUID_);
		if(!configLinkNode.isDisconnected())
		{
			try  // errors in dump are not tolerated
			{
				bool        dumpConfiguration   = true;
				bool        requireUserLogInput = false;
				std::string dumpFilePath, dumpFileRadix, dumpFormat;

				bool doThrow = false;
				try  // for backwards compatibility
				{
					ConfigurationTree fsmLinkNode = configLinkNode.getNode("LinkToStateMachineTable").getNode(activeStateMachineName_);
					dumpConfiguration             = fsmLinkNode.getNode("EnableConfigurationDumpOnRunTransition").getValue<bool>();
					doThrow                       = true;  // at this point throw the exception!
					dumpFilePath  = fsmLinkNode.getNode("ConfigurationDumpOnRunFilePath").getValueWithDefault<std::string>(__ENV__("OTSDAQ_LOG_DIR"));
					dumpFileRadix = fsmLinkNode.getNode("ConfigurationDumpOnRunFileRadix").getValueWithDefault<std::string>("RunTransitionConfigurationDump");
					dumpFormat    = fsmLinkNode.getNode("ConfigurationDumpOnRunFormat").getValue<std::string>();
					requireUserLogInput = fsmLinkNode.getNode("RequireUserLogInputOnRunTransition").getValue<bool>();
				}
				catch(std::runtime_error& e)  // throw exception on missing fields if dumpConfiguration set
				{
					__COUT_INFO__ << "FSM configuration dump Link disconnected at '" << ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME << "/"
					              << supervisorContextUID_ << "/" << supervisorApplicationUID_ << "/"
					              << "LinkToStateMachineTable/" << activeStateMachineName_ << "/"
					              << "EnableConfigurationDumpOnRunTransition" << __E__;
					if(doThrow)
					{
						__SS__ << "Configuration Dump on the Run Start transition was enabled, but there are missing fields! " << e.what() << __E__;
						__SS_THROW__;
					}
					dumpConfiguration = false;
				}

				if(dumpConfiguration)
				{
					__COUT_INFO__ << "Dumping the Configuration on the Run Start transition..." << __E__;
					// dump configuration
					CorePropertySupervisorBase::theConfigurationManager_->dumpActiveConfiguration(
					    dumpFilePath + "/" + dumpFileRadix + "_Run" + activeStateMachineRunNumber_ + "_" + std::to_string(time(0)) + ".dump",
					    dumpFormat,
					    requireUserLogInput ? activeStateMachineLogEntry_ : "",
					    theWebUsers_.getActiveUsersString());
				}
				else
					__COUT_INFO__ << "Not dumping the Configuration on the Run Start transition." << __E__;
			}
			catch(std::runtime_error& e)
			{
				__SS__ << "\nTransition to Running interrupted! There was an error "
				          "identified "
				       << "during the configuration dump attempt:\n\n " << e.what() << __E__;
				__COUT_ERR__ << "\n" << ss.str();
				XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
				return;
			}
			catch(...)
			{
				__SS__ << "\nTransition to Running interrupted! There was an error "
				          "identified "
				       << "during the configuration dump attempt.\n\n " << __E__;
				try	{ throw; } //one more try to printout extra info
				catch(const std::exception &e)
				{
					ss << "Exception message: " << e.what();
				}
				catch(...){}
				__COUT_ERR__ << "\n" << ss.str();
				XCEPT_RAISE(toolbox::fsm::exception::Exception, ss.str());
				return;
			}
		}
		else
			__COUT_INFO__ << "No Gateway Supervisor configuration record found at '" << ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME << "/"
			              << supervisorContextUID_ << "/" << supervisorApplicationUID_
			              << "' - consider adding one to control configuration dumps and state machine properties." << __E__;
	}

	// make logbook entry
	{
		std::stringstream ss;
		ss << "Run '" << activeStateMachineRunNumber_ << "' starting.";

		if(activeStateMachineLogEntry_ != "")
			ss << " User log entry:\n " << StringMacros::decodeURIComponent(activeStateMachineLogEntry_);

		makeSystemLogEntry(ss.str());
	}  // end make logbook entry

	activeStateMachineRunStartTime   = std::chrono::steady_clock::now();
	activeStateMachineRunDuration_ms = 0;
	broadcastMessage(theStateMachine_.getCurrentMessage());

	// save last started group name/key
	ConfigurationManager::saveGroupNameAndKey(theConfigurationTableGroup_, FSM_LAST_STARTED_GROUP_ALIAS_FILE);
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
//	Broadcast state transition to all xdaq Supervisors.
//		- Transition in order of priority as given by AllSupervisorInfo
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

	__COUT__ << "Broadcast complete." << __E__;
}  // end broadcastMessage()

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
			__COUT__ << "Invalid Command" << __E__;
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

	__COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__ << "Login end clock=" << artdaq::TimeUtils::GetElapsedTime(startClock) << __E__;
}  // end loginRequest()

//==============================================================================
void GatewaySupervisor::tooltipRequest(xgi::Input* in, xgi::Output* out)
{
	cgicc::Cgicc cgi(in);

	std::string Command = CgiDataUtilities::getData(cgi, "RequestType");
	//__COUT__ << "Tooltip RequestType = " << Command << __E__;

	//**** start LOGIN GATEWAY CODE ***//
	// If TRUE, cookie code is good, and refreshed code is in cookieCode, also pointers
	// optionally for uint8_t userPermissions, uint64_t uid  Else, error message is
	// returned in cookieCode  Notes: cookie code not refreshed if RequestType =
	// getSystemMessages
	std::string cookieCode = CgiDataUtilities::postData(cgi, "CookieCode");
	uint64_t    uid;

	if(!theWebUsers_.cookieCodeIsActiveForRequest(cookieCode, 0 /*userPermissions*/, &uid, "0" /*dummy ip*/, false /*refresh*/))
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
	                                                  " | getAppStatus");
	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.RequireUserLockRequestTypes,
	                                                  "gatewayLaunchOTS | gatewayLaunchWiz");
	//	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.NeedUsernameRequestTypes,
	//			"StateMachine*"); //for all stateMachineXgiHandler requests

	if(readOnly_)
	{
        CorePropertySupervisorBase::setSupervisorProperty(
            CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.UserPermissionsThreshold,
            "*=0 | getSystemMessages=1 | getDesktopIcons=1");  // block users from writing if no write access
		__COUT__ << "readOnly true in setSupervisorProperty" << __E__;

	}
}  // end forceSupervisorPropertyValues()

//==============================================================================
void GatewaySupervisor::request(xgi::Input* in, xgi::Output* out)
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
	// stateMatchinePreferences
	// getStateMachineNames
	// getCurrentState
	// cancelStateMachineTransition
	// getIterationPlanStatus
	// getErrorInStateMatchine

	// getDesktopIcons
	// addDesktopIcon

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
			std::string username = theWebUsers_.getUsersUsername(userInfo.uid_);
			std::string fsmName  = CgiDataUtilities::getData(cgiIn, "fsmName");
			__SUP_COUTV__(fsmName);

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
					xmlOut.addTextElementToData("config_key", TableGroupKey::getFullGroupString(aliasMapPair.second.first, aliasMapPair.second.second).c_str());

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
			    ConfigurationManager::LAST_TABLE_GROUP_SAVE_PATH + "/" + FSM_LAST_GROUP_ALIAS_FILE_START + username + "." + FSM_USERS_PREFERENCES_FILETYPE;
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
					                              subappElement);  // get time stamp
					xmlOut.addTextElementToParent("subapp_stale", std::to_string(time(0) - subappInfoPair.second.lastStatusTime), subappElement);  // time since update
					xmlOut.addTextElementToParent("subapp_progress", std::to_string(subappInfoPair.second.progress), subappElement);               // get progress
					xmlOut.addTextElementToParent("subapp_detail", subappInfoPair.second.detail, subappElement);                                   // get detail
					xmlOut.addTextElementToParent("subapp_url", subappInfoPair.second.url, subappElement);                                   // get url
					xmlOut.addTextElementToParent("subapp_class", subappInfoPair.second.class_name, subappElement);                                // get class

				}
			}
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
		else if(requestType == "getStateMachine")
		{
			// __COUT__ << "Getting state machine" << __E__;
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
			}
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
			xmlOut.addTextElementToData("current_state", theStateMachine_.getCurrentStateName());
			const std::string& gatewayStatus = allSupervisorInfo_.getGatewayInfo().getStatus();
			if(gatewayStatus.size() > std::string(RunControlStateMachine::FAILED_STATE_NAME).length() && 
				(gatewayStatus[0] == 'F' || gatewayStatus[0] == 'E')) //assume it is Failed or Error and send to state machine
				xmlOut.addTextElementToData("current_error", gatewayStatus);

			xmlOut.addTextElementToData("in_transition", theStateMachine_.isInTransition() ? "1" : "0");
			if(theStateMachine_.isInTransition())
				xmlOut.addTextElementToData("transition_progress", RunControlStateMachine::theProgressBar_.readPercentageString());
			else
				xmlOut.addTextElementToData("transition_progress", "100");

			// char tmp[20]; old size before adding db run number
			char tmp[30];
			sprintf(tmp, "%lu", theStateMachine_.getTimeInState());
			xmlOut.addTextElementToData("time_in_state", tmp);

			//__COUT__ << "current state: " << theStateMachine_.getCurrentStateName() <<
			//__E__;

			//// ======================== get run alias based on fsm name ====

			std::string fsmName = CgiDataUtilities::getData(cgiIn, "fsmName");
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
				std::string stateMachineRunAlias = "Run";  // default to "Run"

				// get stateMachineAliasFilter if possible
				ConfigurationTree configLinkNode =
				    CorePropertySupervisorBase::theConfigurationManager_->getSupervisorTableNode(supervisorContextUID_, supervisorApplicationUID_);

				if(!configLinkNode.isDisconnected())
				{
					try  // for backwards compatibility
					{
						ConfigurationTree fsmLinkNode = configLinkNode.getNode("LinkToStateMachineTable");

						//__COUTV__(fsmLinkNode.getTableVersion());

						if(!fsmLinkNode.isDisconnected())
						{
							if(!fsmLinkNode.getNode(fsmName + "/RunDisplayAlias").isDefaultValue())
								stateMachineRunAlias = fsmLinkNode.getNode(fsmName + "/RunDisplayAlias").getValue<std::string>();
							std::string runInfoPluginType = fsmLinkNode.getNode(fsmName + "/RunInfoPluginType").getValue<std::string>();
							if(runInfoPluginType != TableViewColumnInfo::DATATYPE_STRING_DEFAULT && runInfoPluginType != "No Run Info Plugin")
								useRunInfoDb = true;
						}
						// else
						//	__COUT_INFO__ << "FSM Link disconnected." << __E__;
					}
					catch(std::runtime_error& e)
					{
						//__COUT_INFO__ << e.what() << __E__;
						//__COUT_INFO__ << "No state machine Run alias. Ignoring and
						// assuming alias of '" << 		stateMachineRunAlias << ".'" <<
						//__E__;
					}
					catch(...)
					{
						__COUT_ERR__ << "Unknown error. Should never happen." << __E__;

						__COUT_INFO__ << "No state machine Run alias. Ignoring and "
						                 "assuming alias of '"
						              << stateMachineRunAlias << ".'" << __E__;
					}
				}
				// else
				//	__COUT_INFO__ << "FSM Link disconnected." << __E__;

				//__COUT__ << "stateMachineRunAlias  = " << stateMachineRunAlias	<<
				//__E__;

				xmlOut.addTextElementToData("stateMachineRunAlias", stateMachineRunAlias);

				//// ======================== get run number based on fsm name ====

				if(theStateMachine_.getCurrentStateName() == "Running" || theStateMachine_.getCurrentStateName() == "Paused")
				{
					if(useRunInfoDb)
						sprintf(tmp, "Current %s Number from DB: %u", stateMachineRunAlias.c_str(), getNextRunNumber(activeStateMachineName_) - 1);
					else
						sprintf(tmp, "Current %s Number: %u", stateMachineRunAlias.c_str(), getNextRunNumber(activeStateMachineName_) - 1);

					if(RunControlStateMachine::asyncPauseExceptionReceived_)
					{
						//__COUTV__(RunControlStateMachine::asyncPauseExceptionReceived_);
						//__COUTV__(RunControlStateMachine::getErrorMessage());
						xmlOut.addTextElementToData("soft_error", RunControlStateMachine::getErrorMessage());
					}
				}
				else
				{
					if(useRunInfoDb)
						sprintf(tmp, "Next %s Number from DB.", stateMachineRunAlias.c_str());
					else
						sprintf(tmp, "Next %s Number: %u", stateMachineRunAlias.c_str(), getNextRunNumber(fsmName));
				}

				if(RunControlStateMachine::asyncStopExceptionReceived_)
				{
					//__COUTV__(RunControlStateMachine::asyncPauseExceptionReceived_);
					//__COUTV__(RunControlStateMachine::getErrorMessage());
					xmlOut.addTextElementToData("soft_error", RunControlStateMachine::getErrorMessage());
				}

				xmlOut.addTextElementToData("run_number", tmp);
			}
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

			std::string iconString = "";  // comma-separated icon string, 7 fields:
			//				0 - caption 		= text below icon
			//				1 - altText 		= text icon if no image given
			//				2 - uniqueWin 		= if true, only one window is allowed,
			// else  multiple instances of window 				3 - permissions 	=
			// security level needed to see icon 				4 - picfn 			=
			// icon image filename 				5 - linkurl 		= url of the window to
			// open 				6 - folderPath 		= folder and subfolder location
			// '/'
			// separated  for example:  State
			// Machine,FSM,1,200,icon-Physics.gif,/WebPath/html/StateMachine.html?fsm_name=OtherRuns0,,Chat,CHAT,1,1,icon-Chat.png,/urn:xdaq-application:lid=250,,Visualizer,VIS,0,10,icon-Visualizer.png,/WebPath/html/Visualization.html?urn=270,,Configure,CFG,0,10,icon-Configure.png,/urn:xdaq-application:lid=281,,Front-ends,CFG,0,15,icon-Configure.png,/WebPath/html/ConfigurationGUI_subset.html?urn=281&subsetBasePath=FEInterfaceTable&groupingFieldList=Status%2CFEInterfacePluginName&recordAlias=Front%2Dends&editableFieldList=%21%2ACommentDescription%2C%21SlowControls%2A,Config
			// Subsets

			//__COUTV__((unsigned int)userInfo.permissionLevel_);

			std::map<std::string, WebUsers::permissionLevel_t> userPermissionLevelsMap = theWebUsers_.getPermissionsForUser(userInfo.uid_);
			std::map<std::string, WebUsers::permissionLevel_t> iconPermissionThresholdsMap;

			bool firstIcon = true;
			for(const auto& icon : icons)
			{
				//__COUTV__(icon.caption_);
				//__COUTV__(icon.permissionThresholdString_);

				CorePropertySupervisorBase::extractPermissionsMapFromString(icon.permissionThresholdString_, iconPermissionThresholdsMap);

				if(!CorePropertySupervisorBase::doPermissionsGrantAccess(userPermissionLevelsMap, iconPermissionThresholdsMap))
					continue;  // skip icon if no access

				//__COUTV__(icon.caption_);

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
			//__COUTV__(iconString);

			xmlOut.addTextElementToData("iconList", iconString);
		}
		else if(requestType == "addDesktopIcon")
		{
			std::vector<DesktopIconTable::DesktopIcon> newIcons;

			bool success = GatewaySupervisor::handleAddDesktopIconRequest(theWebUsers_.getUsersUsername(userInfo.uid_), cgiIn, xmlOut, &newIcons);

			if(success)
			{
				__COUT__ << "Attempting dynamic icon change..." << __E__;

				DesktopIconTable* iconTable = (DesktopIconTable*)CorePropertySupervisorBase::theConfigurationManager_->getDesktopIconTable();
				iconTable->setAllDesktopIcons(newIcons);
			}
			else
				__COUT__ << "Failed dynamic icon add." << __E__;
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
			WebUsers::resetAllUserTooltips(theWebUsers_.getUsersUsername(userInfo.uid_));
		}
		else if(requestType == "silenceUserTooltips")
		{
			WebUsers::silenceAllUserTooltips(theWebUsers_.getUsersUsername(userInfo.uid_));
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
	//__COUT__ << __E__;

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
	uint64_t                                                         activeSessionIndex, uid;
	theWebUsers_.cookieCodeIsActiveForRequest(
	    cookieCode, &userGroupPermissionsMap, &uid /*uid is not given to remote users*/, ipAddress, refreshOption == "1", &userWithLock, &activeSessionIndex);

	//__COUT__ << "userWithLock " << userWithLock << __E__;

	// fill return parameters
	SOAPParameters retParameters;
	retParameters.addParameter("CookieCode", cookieCode);
	retParameters.addParameter("Permissions", StringMacros::mapToString(userGroupPermissionsMap).c_str());
	retParameters.addParameter("UserWithLock", userWithLock);
	retParameters.addParameter("Username", theWebUsers_.getUsersUsername(uid));
	retParameters.addParameter("DisplayName", theWebUsers_.getUsersDisplayName(uid));
	sprintf(tmpStringForConversions_, "%lu", activeSessionIndex);
	retParameters.addParameter("ActiveSessionIndex", tmpStringForConversions_);

	//__COUT__ << __E__;

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

	__COUTV__(toUserCSV);
	__COUTV__(subject);
	__COUTV__(systemMessage);
	__COUTV__(doEmail);

	theWebUsers_.addSystemMessage(toUserCSV, subject, systemMessage, doEmail == "1");

	return SOAPUtilities::makeSOAPMessageReference("SystemMessageResponse");
}  // end supervisorSystemMessage()

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
}

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
}

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

	std::string fileName = "";
	if(action == "Configured")
		fileName = FSM_LAST_CONFIGURED_GROUP_ALIAS_FILE;
	else if(action == "Started")
		fileName = FSM_LAST_STARTED_GROUP_ALIAS_FILE;
	else if(action == "ActivatedConfig")
		fileName = ConfigurationManager::LAST_ACTIVATED_CONFIG_GROUP_FILE;
	else if(action == "ActivatedContext")
		fileName = ConfigurationManager::LAST_ACTIVATED_CONTEXT_GROUP_FILE;
	else if(action == "ActivatedBackbone")
		fileName = ConfigurationManager::LAST_ACTIVATED_BACKBONE_GROUP_FILE;
	else if(action == "ActivatedIterator")
		fileName = ConfigurationManager::LAST_ACTIVATED_ITERATOR_GROUP_FILE;
	else
	{
		__COUT_ERR__ << "Invalid last group action requested." << __E__;
		return SOAPUtilities::makeSOAPMessageReference("LastConfigGroupResponseFailure");
	}
	std::string                                          timeString;
	std::pair<std::string /*group name*/, TableGroupKey> theGroup = ConfigurationManager::loadGroupNameAndKey(fileName, timeString);

	// fill return parameters
	SOAPParameters retParameters;
	retParameters.addParameter("GroupName", theGroup.first);
	retParameters.addParameter("GroupKey", theGroup.second.toString());
	retParameters.addParameter("GroupAction", action);
	retParameters.addParameter("GroupActionTime", timeString);

	return SOAPUtilities::makeSOAPMessageReference("LastConfigGroupResponse", retParameters);
}

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
		__COUT__ << "Can't open file: " << runNumberFileName << __E__;

		__COUT__ << "Creating file and setting Run Number to 1: " << runNumberFileName << __E__;
		FILE* fp = fopen(runNumberFileName.c_str(), "w");
		fprintf(fp, "1");
		fclose(fp);

		runNumberFile.open(runNumberFileName.c_str());
		if(!runNumberFile.is_open())
		{
			__SS__ << "Error. Can't create file: " << runNumberFileName << __E__;
			__COUT_ERR__ << ss.str();
			__SS_THROW__;
		}
	}
	std::string runNumberString;
	runNumberFile >> runNumberString;
	runNumberFile.close();
	return atoi(runNumberString.c_str());
}

//==============================================================================
bool GatewaySupervisor::setNextRunNumber(unsigned int runNumber, const std::string& fsmNameIn)
{
	std::string runNumberFileName = RUN_NUMBER_PATH + "/";
	std::string fsmName           = fsmNameIn == "" ? activeStateMachineName_ : fsmNameIn;
	// prepend sanitized FSM name
	for(unsigned int i = 0; i < fsmName.size(); ++i)
		if((fsmName[i] >= 'a' && fsmName[i] <= 'z') || (fsmName[i] >= 'A' && fsmName[i] <= 'Z') || (fsmName[i] >= '0' && fsmName[i] <= '9'))
			runNumberFileName += fsmName[i];
	runNumberFileName += RUN_NUMBER_FILE_NAME;
	__COUT__ << "runNumberFileName: " << runNumberFileName << __E__;

	std::ofstream runNumberFile(runNumberFileName.c_str());
	if(!runNumberFile.is_open())
	{
		__SS__ << "Can't open file: " << runNumberFileName << __E__;
		__COUT__ << ss.str();
		__SS_THROW__;
	}
	std::stringstream runNumberStream;
	runNumberStream << runNumber;
	runNumberFile << runNumberStream.str().c_str();
	runNumberFile.close();
	return true;
}  // end setNextRunNumber()

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
