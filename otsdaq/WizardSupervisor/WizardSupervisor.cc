#include "otsdaq/WizardSupervisor/WizardSupervisor.h"

#include "otsdaq/CoreSupervisors/CorePropertySupervisorBase.h"
#include "otsdaq/GatewaySupervisor/GatewaySupervisor.h"

#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/ITRACEController.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <xdaq/NamespaceURI.h>
#include "otsdaq/CgiDataUtilities/CgiDataUtilities.h"
#include "otsdaq/SOAPUtilities/SOAPCommand.h"
#include "otsdaq/SOAPUtilities/SOAPUtilities.h"
#include "otsdaq/WebUsersUtilities/WebUsers.h"
#include "otsdaq/XmlUtilities/HttpXmlDocument.h"

#include <dirent.h>    //for DIR
#include <sys/stat.h>  // mkdir
#include <boost/filesystem.hpp>
#include <chrono>  // std::chrono::seconds
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>  // std::this_thread::sleep_for
namespace filesystem = boost::filesystem;
using namespace ots;

// clang-format off

const std::string WizardSupervisor::WIZ_SUPERVISOR		= __ENV__("OTS_CONFIGURATION_WIZARD_SUPERVISOR_SERVER");
const std::string WizardSupervisor::WIZ_PORT			= __ENV__("PORT");
const std::string WizardSupervisor::SERVICE_DATA_PATH 	= __ENV__("SERVICE_DATA_PATH");

#define SECURITY_FILE_NAME 				WizardSupervisor::SERVICE_DATA_PATH + "/OtsWizardData/security.dat"
#define SEQUENCE_FILE_NAME 				WizardSupervisor::SERVICE_DATA_PATH + "/OtsWizardData/sequence.dat"
#define SEQUENCE_OUT_FILE_NAME 			WizardSupervisor::SERVICE_DATA_PATH + "/OtsWizardData/sequence.out"
#define USER_IMPORT_EXPORT_PATH 		WizardSupervisor::SERVICE_DATA_PATH + "/"

#define XML_STATUS 						"editUserData_status"

#define XML_ADMIN_STATUS 				"logbook_admin_status"
#define XML_MOST_RECENT_DAY 			"most_recent_day"
#define XML_EXPERIMENTS_ROOT 			"experiments"
#define XML_EXPERIMENT 					"experiment"
#define XML_ACTIVE_EXPERIMENT 			"active_experiment"
#define XML_EXPERIMENT_CREATE 			"create_time"
#define XML_EXPERIMENT_CREATOR 			"creator"

#define XML_LOGBOOK_ENTRY 				"logbook_entry"
#define XML_LOGBOOK_ENTRY_SUBJECT 		"logbook_entry_subject"
#define XML_LOGBOOK_ENTRY_TEXT 			"logbook_entry_text"
#define XML_LOGBOOK_ENTRY_FILE 			"logbook_entry_file"
#define XML_LOGBOOK_ENTRY_TIME 			"logbook_entry_time"
#define XML_LOGBOOK_ENTRY_CREATOR 		"logbook_entry_creator"
#define XML_LOGBOOK_ENTRY_HIDDEN 		"logbook_entry_hidden"
#define XML_LOGBOOK_ENTRY_HIDER 		"logbook_entry_hider"
#define XML_LOGBOOK_ENTRY_HIDDEN_TIME 	"logbook_entry_hidden_time"

#define XML_PREVIEW_INDEX 				"preview_index"
#define LOGBOOK_PREVIEW_FILE 			"preview.xml"

XDAQ_INSTANTIATOR_IMPL(WizardSupervisor)

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "Wizard"

// init allowed file upload types
const std::vector<std::string>	WizardSupervisor::allowedFileUploadTypes_ = {
		"image/png",
		"image/jpeg",
		"image/gif",
		"image/bmp",
		"application/pdf",
		"application/zip",
		"text/plain"
};
const std::vector<std::string>	WizardSupervisor::matchingFileUploadTypes_ = {
		"png",
		"jpeg",
		"gif",
		"ima/bmp",
		"pdf",
		"zip",
		"txt"
};

// clang-format on

//==============================================================================
WizardSupervisor::WizardSupervisor(xdaq::ApplicationStub* s)
    : xdaq::Application(s)
    , SOAPMessenger(this)
    , supervisorClass_(getApplicationDescriptor()->getClassName())
    , supervisorClassNoNamespace_(
          supervisorClass_.substr(supervisorClass_.find_last_of(":") + 1, supervisorClass_.length() - supervisorClass_.find_last_of(":")))
{
	__COUT__ << "Constructor." << __E__;

	INIT_MF("." /*directory used is USER_DATA/LOG/.*/);

	// get all supervisor info, and wiz mode, macroMaker mode, or not
	allSupervisorInfo_.init(getApplicationContext());

	xgi::bind(this, &WizardSupervisor::Default, "Default");
	xgi::bind(this, &WizardSupervisor::verification, "Verify");
	xgi::bind(this, &WizardSupervisor::request, "Request");
	xgi::bind(this, &WizardSupervisor::requestIcons, "requestIcons");
	xgi::bind(this, &WizardSupervisor::editSecurity, "editSecurity");
	xgi::bind(this, &WizardSupervisor::UserSettings, "UserSettings");
	xgi::bind(this, &WizardSupervisor::tooltipRequest, "TooltipRequest");
	xgi::bind(this, &WizardSupervisor::toggleSecurityCodeGeneration, "ToggleSecurityCodeGeneration");
	xoap::bind(this, &WizardSupervisor::supervisorSequenceCheck, "SupervisorSequenceCheck", XDAQ_NS_URI);
	xoap::bind(this, &WizardSupervisor::supervisorLastTableGroupRequest, "SupervisorLastTableGroupRequest", XDAQ_NS_URI);

	init();
	generateURL();
	GatewaySupervisor::indicateOtsAlive();

	__COUT__ << "Constructed." << __E__;
}  // end constructor()
//==============================================================================
WizardSupervisor::~WizardSupervisor(void)
{
	__COUT__ << "Destructor." << __E__;
	destroy();
	__COUT__ << "Destructed." << __E__;
}

//==============================================================================
void WizardSupervisor::destroy(void)
{
	// called by destructor
}  // end destroy()

//==============================================================================
void WizardSupervisor::init(void)
{
	// attempt to make directory structure (just in case)
	mkdir((WizardSupervisor::SERVICE_DATA_PATH).c_str(), 0755);
	mkdir((WizardSupervisor::SERVICE_DATA_PATH + "/OtsWizardData").c_str(), 0755);

}  // end init()

//==============================================================================
void WizardSupervisor::requestIcons(xgi::Input* in, xgi::Output* out)
{
	cgicc::Cgicc cgi(in);

	std::string submittedSequence = CgiDataUtilities::postData(cgi, "sequence");

	// SECURITY CHECK START ****
	if(securityCode_.compare(submittedSequence) != 0)
	{
		__COUT__ << "Unauthorized Request made, security sequence doesn't match! " << time(0) << std::endl;
		return;
	}
	else
	{
		__COUT__ << "*** Successfully authenticated security sequence @ " << time(0) << std::endl;
	}
	// SECURITY CHECK END ****

	// an icon is 7 fields.. give comma-separated
	// 0 - subtext = text below icon
	// 1 - altText = text for icon if image set to 0
	// 2 - uniqueWin = if true, only one window is allowed, else multiple instances of
	// window  3 - permissions = security level needed to see icon  4 - picfn = icon image
	// filename, 0 for no image  5 - linkurl = url of the window to open  6 - folderPath =
	// folder and subfolder location

	// clang-format off
	*out << "Configure,CFG,0,1,icon-Configure.png,/urn:xdaq-application:lid=280/,/"

	     << ",Table Editor,TBL,0,1,icon-ControlsDashboard.png,"
		 	"/urn:xdaq-application:lid=280/?configWindowName=tableEditor,/"

	     << ",Desktop Icon Editor,ICON,0,1,icon-IconEditor.png,"
		 	"/WebPath/html/ConfigurationGUI_subset.html?urn=280&subsetBasePath=DesktopIconTable&"
	        "recordAlias=Icons&groupingFieldList=Status%2CForceOnlyOneInstance%2CRequiredPermissionLevel,/"

		 //User Settings ------------------
		 << ",Edit User Accounts,USER,1,1,"
		 				 "/WebPath/images/dashboardImages/icon-Settings.png,/WebPath/html/UserSettings.html,/User Settings"

		 << ",Security Settings,SEC,1,1,icon-SecuritySettings.png,"
		 	"/WebPath/html/SecuritySettings.html,/User Settings"

	     << ",Edit User Data,USER,1,1,icon-EditUserData.png,/WebPath/html/EditUserData.html,/User Settings"

		 //end User Settings ------------------

	     << ",Console,C,1,1,icon-Console.png,/urn:xdaq-application:lid=260/,/" 
		 
		 //Configuration Wizards ------------------
		 << ",Front-end Wizard,CFG,0,1,icon-Configure.png,"
		 	"/WebPath/html/RecordWiz_ConfigurationGUI.html?urn=280&recordAlias=Front%2Dend,Config Wizards"

	     << ",Processor Wizard,CFG,0,1,icon-Configure.png,"
		 	"/WebPath/html/RecordWiz_ConfigurationGUI.html?urn=280&recordAlias=Processor,Config Wizards"

	     << ",artdaq Config Editor,CFG,0,1,icon-Configure.png,"
		 	"/WebPath/html/ConfigurationGUI_artdaq.html?urn=280,Config Wizards"

	     << ",Block Diagram,CFG,0,1,icon-Configure.png,"
		 	"/WebPath/html/ConfigurationSubsetBlockDiagram.html?urn=280,Config Wizards"
		 //end Configuration Wizards ------------------

	     << ",Code Editor,CODE,0,1,icon-CodeEditor.png,/urn:xdaq-application:lid=240/,/"

		 //Documentation ------------------
	     << ",State Machine Screenshot,FSM-SS,1,1,icon-Physics.gif,"
		 	"/WebPath/images/windowContentImages/state_machine_screenshot.png,/Documentation"

		 	 //uniqueWin mode == 2 for new tab
		 << ",Redmine Project for otsdaq,RED,2,1,../otsdaqIcons/android-icon-36x36.png,"
		 	"https://cdcvs.fnal.gov/redmine/projects/otsdaq,/Documentation"
		 	 //uniqueWin mode == 2 for new tab
		 << ",Homepage for otsdaq,OTS,2,1,../otsdaqIcons/android-icon-36x36.png,"
		 		 	"https://otsdaq.fnal.gov,/Documentation"
		 //end Documentation ------------------

	    //",Iterate,IT,0,1,icon-Iterate.png,/urn:xdaq-application:lid=280/?configWindowName=iterate,/"
	    //<<
	    //",Configure,CFG,0,1,icon-Configure.png,/urn:xdaq-application:lid=280/,myFolder"
	    //<<
	    //",Configure,CFG,0,1,icon-Configure.png,/urn:xdaq-application:lid=280/,/myFolder/mySub.folder"
	    //<<
	    //",Configure,CFG,0,1,icon-Configure.png,/urn:xdaq-application:lid=280/,myFolder/"
	    //<<

	    
	    //",Consumer
	    // Wizard,CFG,0,1,icon-Configure.png,/WebPath/html/RecordWiz_ConfigurationGUI.html?urn=280&subsetBasePath=FEInterfaceConfiguration&recordAlias=Consumer,Config
	    // Wizards" <<

	    //",DB Utilities,DB,1,1,0,http://127.0.0.1:8080/db/client.html" <<
        

	     << "";
	// clang-format on
	return;
}  // end requestIcons()

//==============================================================================
void WizardSupervisor::verification(xgi::Input* in, xgi::Output* out)
{
	cgicc::Cgicc cgi(in);
	std::string  submittedSequence = CgiDataUtilities::getData(cgi, "code");
	__COUT__ << "submittedSequence=" << submittedSequence << " " << time(0) << std::endl;

	std::string securityWarning = "";

	if(securityCode_.compare(submittedSequence) != 0)
	{
		__COUT__ << "Unauthorized Request made, security sequence doesn't match!" << std::endl;
		*out << "Invalid code.";
		return;
	}
	else
	{
		// defaultSequence_ = false;
		__COUT__ << "*** Successfully authenticated security sequence "
		         << "@ " << time(0) << std::endl;

		if(defaultSequence_)
		{
			//__COUT__ << " UNSECURE!!!" << std::endl;
			securityWarning = "&secure=False";
		}
	}

	*out << "<!DOCTYPE HTML><html lang='en'><head><title>ots wiz</title>" << GatewaySupervisor::getIconHeaderString() <<
	    // end show ots icon
	    "</head>"
	     << "<frameset col='100%' row='100%'><frame src='/WebPath/html/Wizard.html?urn=" << this->getApplicationDescriptor()->getLocalId() << securityWarning
	     << "'></frameset></html>";
}  // end verification()

//==============================================================================
void WizardSupervisor::generateURL()
{
	defaultSequence_ = true;

	int   length = 4;
	FILE* fp     = fopen((SEQUENCE_FILE_NAME).c_str(), "r");
	if(fp)
	{
		__COUT_INFO__ << "Sequence length file found: " << SEQUENCE_FILE_NAME << std::endl;
		char line[100];
		fgets(line, 100, fp);
		sscanf(line, "%d", &length);
		fclose(fp);
		if(length < 4)
			length = 4;  // don't allow shorter than 4
		else
			defaultSequence_ = false;
		srand(time(0));  // randomize differently each "time"
	}
	else
	{
		__COUT_INFO__ << "(Reverting to default wiz security) Sequence length file NOT found: " << SEQUENCE_FILE_NAME << std::endl;
		srand(0);  // use same seed for convenience if file not found
	}

	__COUT__ << "Sequence length = " << length << std::endl;

	securityCode_ = "";

	const char alphanum[] =
	    "0123456789"
	    "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
	    "abcdefghijklmnopqrstuvwxyz";

	for(int i = 0; i < length; ++i)
	{
		securityCode_ += alphanum[rand() % (sizeof(alphanum) - 1)];
	}

	__COUT__ << WizardSupervisor::WIZ_SUPERVISOR << ":" << WizardSupervisor::WIZ_PORT
	         << "/urn:xdaq-application:lid=" << this->getApplicationDescriptor()->getLocalId() << "/Verify?code=" << securityCode_ << std::endl;

	// Note: print out handled by StartOTS.sh now
	// std::thread([&](WizardSupervisor *ptr, std::string securityCode)
	//		{printURL(ptr,securityCode);},this,securityCode_).detach();

	fp = fopen((SEQUENCE_OUT_FILE_NAME).c_str(), "w");
	if(fp)
	{
		fprintf(fp, "%s", securityCode_.c_str());
		fclose(fp);
	}
	else
		__COUT_ERR__ << "Sequence output file NOT found: " << SEQUENCE_OUT_FILE_NAME << std::endl;

	return;
}  // end generateURL()

void WizardSupervisor::printURL(WizardSupervisor* ptr, std::string securityCode)
{
	// child process
	int i = 0;
	for(; i < 5; ++i)
	{
		std::this_thread::sleep_for(std::chrono::seconds(2));
		__COUT__ << WizardSupervisor::WIZ_SUPERVISOR << ":" << WizardSupervisor::WIZ_PORT
		         << "/urn:xdaq-application:lid=" << ptr->getApplicationDescriptor()->getLocalId() << "/Verify?code=" << securityCode << std::endl;
	}
}  // end printURL()

//==============================================================================
void WizardSupervisor::tooltipRequest(xgi::Input* in, xgi::Output* out)
{
	cgicc::Cgicc cgi(in);

	std::string Command = CgiDataUtilities::getData(cgi, "RequestType");
	//__COUT__ << "Command = " << Command << std::endl;

	std::string submittedSequence = CgiDataUtilities::postData(cgi, "sequence");

	// SECURITY CHECK START ****
	if(securityCode_.compare(submittedSequence) != 0)
	{
		__COUT__ << "Unauthorized Request made, security sequence doesn't match!" << std::endl;
		return;
	}
	//	else
	//	{
	//		__COUT__ << "***Successfully authenticated security sequence." << std::endl;
	//	}
	// SECURITY CHECK END ****

	HttpXmlDocument xmldoc;

	if(Command == "check")
	{
		WebUsers::tooltipCheckForUsername(WebUsers::DEFAULT_ADMIN_USERNAME,
		                                  &xmldoc,
		                                  CgiDataUtilities::getData(cgi, "srcFile"),
		                                  CgiDataUtilities::getData(cgi, "srcFunc"),
		                                  CgiDataUtilities::getData(cgi, "srcId"));
	}
	else if(Command == "setNeverShow")
	{
		WebUsers::tooltipSetNeverShowForUsername(WebUsers::DEFAULT_ADMIN_USERNAME,
		                                         &xmldoc,
		                                         CgiDataUtilities::getData(cgi, "srcFile"),
		                                         CgiDataUtilities::getData(cgi, "srcFunc"),
		                                         CgiDataUtilities::getData(cgi, "srcId"),
		                                         CgiDataUtilities::getData(cgi, "doNeverShow") == "1" ? true : false,
		                                         CgiDataUtilities::getData(cgi, "temporarySilence") == "1" ? true : false);
	}
	else
		__COUT__ << "Command Request, " << Command << ", not recognized." << std::endl;

	xmldoc.outputXmlDocument((std::ostringstream*)out, false, true);
}  // end tooltipRequest()

//==============================================================================
void WizardSupervisor::toggleSecurityCodeGeneration(xgi::Input* in, xgi::Output* out)
{
	cgicc::Cgicc cgi(in);

	std::string Command = CgiDataUtilities::getData(cgi, "RequestType");
	__COUT__ << "Got to Command = " << Command << std::endl;

	std::string submittedSequence = CgiDataUtilities::postData(cgi, "sequence");

	// SECURITY CHECK START ****
	if(securityCode_.compare(submittedSequence) != 0)
	{
		__COUT__ << "Unauthorized Request made, security sequence doesn't match!" << std::endl;
		return;
	}
	else
	{
		__COUT__ << "***Successfully authenticated security sequence." << std::endl;
	}
	// SECURITY CHECK END ****

	HttpXmlDocument xmldoc;

	if(Command == "TurnGenerationOn")
	{
		__COUT__ << "Turning automatic URL Generation on with a sequence depth of 16!" << std::endl;
		std::ofstream outfile((SEQUENCE_FILE_NAME).c_str());
		outfile << "16" << std::endl;
		outfile.close();
		generateURL();

		std::thread([&](WizardSupervisor* ptr, std::string securityCode) { printURL(ptr, securityCode); }, this, securityCode_).detach();

		xmldoc.addTextElementToData("Status", "Generation_Success");
	}
	else
		__COUT__ << "Command Request, " << Command << ", not recognized." << std::endl;

	xmldoc.outputXmlDocument((std::ostringstream*)out, false, true);
} //end toggleSecurityCodeGeneration()

//==============================================================================
// xoap::supervisorSequenceCheck
//	verify cookie
xoap::MessageReference WizardSupervisor::supervisorSequenceCheck(xoap::MessageReference message)
{
	// SOAPUtilities::receive request parameters
	SOAPParameters parameters;
	parameters.addParameter("sequence");
	SOAPUtilities::receive(message, parameters);

	std::string submittedSequence = parameters.getValue("sequence");

	// If submittedSequence matches securityCode_ then return full permissions (255)
	//	else, return permissions 0
	std::map<std::string /*groupName*/, WebUsers::permissionLevel_t> permissionMap;

	if(securityCode_ == submittedSequence)
		permissionMap.emplace(
		    std::pair<std::string /*groupName*/, WebUsers::permissionLevel_t>(WebUsers::DEFAULT_USER_GROUP, WebUsers::PERMISSION_LEVEL_ADMIN));
	else
	{
		__COUT__ << "Unauthorized Request made, security sequence doesn't match!" << std::endl;

		permissionMap.emplace(
		    std::pair<std::string /*groupName*/, WebUsers::permissionLevel_t>(WebUsers::DEFAULT_USER_GROUP, WebUsers::PERMISSION_LEVEL_INACTIVE));
	}

	// fill return parameters
	SOAPParameters retParameters;
	retParameters.addParameter("Permissions", StringMacros::mapToString(permissionMap));

	return SOAPUtilities::makeSOAPMessageReference("SequenceResponse", retParameters);
} //end supervisorSequenceCheck()

//===================================================================================================================
// xoap::supervisorLastTableGroupRequest
//	return the group name and key for the last state machine activity
//
//	Note: same as Supervisor::supervisorLastTableGroupRequest
xoap::MessageReference WizardSupervisor::supervisorLastTableGroupRequest(xoap::MessageReference message)
{
	SOAPParameters parameters;
	parameters.addParameter("ActionOfLastGroup");
	SOAPUtilities::receive(message, parameters);

	return GatewaySupervisor::lastTableGroupRequestHandler(parameters);
} //end supervisorLastTableGroupRequest()

//==============================================================================
void WizardSupervisor::Default(xgi::Input* /*in*/, xgi::Output* out)
{
	__COUT__ << "Unauthorized Request made, security sequence doesn't match!" << std::endl;
	*out << "Unauthorized Request.";
} //end Default()

//==============================================================================
void WizardSupervisor::request(xgi::Input* in, xgi::Output* out)
{
	cgicc::Cgicc cgiIn(in);

	std::string submittedSequence = CgiDataUtilities::postData(cgiIn, "sequence");

	// SECURITY CHECK START ****
	if(securityCode_.compare(submittedSequence) != 0)
	{
		__COUT__ << "Unauthorized Request made, security sequence doesn't match! " << time(0) << std::endl;
		*out << WebUsers::REQ_NO_PERMISSION_RESPONSE.c_str();
		return;
	}
	else
	{
		__COUT__ << "***Successfully authenticated security sequence. " << time(0) << std::endl;
	}
	// SECURITY CHECK END ****

	std::string requestType = CgiDataUtilities::getData(cgiIn, "RequestType");
	__COUTV__(requestType);

	HttpXmlDocument xmlOut;

	try
	{
		// RequestType Commands:
		// 	gatewayLaunchOTS
		// 	gatewayLaunchWiz
		// 	addDesktopIcon
		//	getSettings
		//	accountSettings

		if(requestType == "gatewayLaunchOTS" || requestType == "gatewayLaunchWiz")
		{
			// NOTE: similar to ConfigurationGUI version but DOES keep active login
			// sessions

			__COUT_WARN__ << requestType << " requestType received! " << __E__;
			__MOUT_WARN__ << requestType << " requestType received! " << __E__;

			// now launch
			ConfigurationManager cfgMgr;
			if(requestType == "gatewayLaunchOTS")
				GatewaySupervisor::launchStartOTSCommand("LAUNCH_OTS", &cfgMgr);
			else if(requestType == "gatewayLaunchWiz")
				GatewaySupervisor::launchStartOTSCommand("LAUNCH_WIZ", &cfgMgr);
		}
		else if(requestType == "addDesktopIcon")
		{
			GatewaySupervisor::handleAddDesktopIconRequest("admin", cgiIn, xmlOut);
		}
		else if(requestType == "getAppId")
		{
			GatewaySupervisor::handleGetApplicationIdRequest(&allSupervisorInfo_, cgiIn, xmlOut);
		}
		else if(requestType == "getSettings")
		{
			std::string accounts = CgiDataUtilities::getData(cgiIn, "accounts");

			__COUT__ << "Get Settings Request" << __E__;
			__COUT__ << "accounts = " << accounts << __E__;

			GatewaySupervisor::theWebUsers_.insertSettingsForUser(0 /*admin UID*/, &xmlOut, accounts == "1");
		}
		else if(requestType == "accountSettings")
		{
			std::string type     = CgiDataUtilities::postData(cgiIn, "type");  // updateAccount, createAccount, deleteAccount
			int         type_int = -1;

			if(type == "updateAccount")
				type_int = GatewaySupervisor::theWebUsers_.MOD_TYPE_UPDATE;
			else if(type == "createAccount")
				type_int = GatewaySupervisor::theWebUsers_.MOD_TYPE_ADD;
			else if(type == "deleteAccount")
				type_int = GatewaySupervisor::theWebUsers_.MOD_TYPE_DELETE;

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

			GatewaySupervisor::theWebUsers_.modifyAccountSettings(0 /*admin UID*/, type_int, username, displayname, email, permissions);

			__COUT__ << "accounts = " << accounts << __E__;

			GatewaySupervisor::theWebUsers_.insertSettingsForUser(0 /*admin UID*/, &xmlOut, accounts == "1");
		}
		else
		{
			__SS__ << "requestType Request '" << requestType << "' not recognized." << __E__;
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

	// report any errors encountered
	{
		unsigned int occurance = 0;
		std::string  err       = xmlOut.getMatchingValue("Error", occurance++);
		while(err != "")
		{
			__COUT_ERR__ << "'" << requestType << "' ERROR encountered: " << err << __E__;
			__MOUT_ERR__ << "'" << requestType << "' ERROR encountered: " << err << __E__;
			err = xmlOut.getMatchingValue("Error", occurance++);
		}
	}

	// return xml doc holding server response
	xmlOut.outputXmlDocument((std::ostringstream*)out, false /*dispStdOut*/, true /*allowWhiteSpace*/);  // Note: allow white space need for error response

}  // end request()

//==============================================================================
void WizardSupervisor::editSecurity(xgi::Input* in, xgi::Output* out)
{
	// if sequence doesn't match up -> return
	cgicc::Cgicc cgi(in);
	std::string  submittedSequence = CgiDataUtilities::postData(cgi, "sequence");
	std::string  submittedSecurity = CgiDataUtilities::postData(cgi, "selection");

	// SECURITY CHECK START ****
	if(securityCode_.compare(submittedSequence) != 0)
	{
		__COUT__ << "Unauthorized Request made, security sequence doesn't match!" << std::endl;
		return;
	}
	else
	{
		__COUT__ << "***Successfully authenticated security sequence." << std::endl;
	}
	// SECURITY CHECK END ****

	if(submittedSecurity != "")
	{
		__COUT__ << "Selection exists!" << std::endl;
		__COUT__ << submittedSecurity << std::endl;

		if(submittedSecurity == "ResetAllUserData")
		{
			WebUsers::deleteUserData();
			__COUT__ << "Turning URL Generation back to default!" << std::endl;
			// std::remove((SEQUENCE_FILE_NAME).c_str());
			// std::remove((SEQUENCE_OUT_FILE_NAME).c_str());
			std::ofstream newFile((SEQUENCE_FILE_NAME).c_str());
			newFile << "4" << std::endl;
			newFile.close();

			generateURL();
			std::thread([&](WizardSupervisor* ptr, std::string securityCode) { printURL(ptr, securityCode); }, this, securityCode_).detach();
			*out << "Default_URL_Generation";
		}
		else if(submittedSecurity == "ResetAllUserTooltips")
		{
			WebUsers::resetAllUserTooltips();
			*out << submittedSecurity;
			return;
		}
		else if(submittedSecurity == WebUsers::SECURITY_TYPE_DIGEST_ACCESS || submittedSecurity == WebUsers::SECURITY_TYPE_NONE)
		{
			// attempt to make directory structure (just in case)
			mkdir((WizardSupervisor::SERVICE_DATA_PATH).c_str(), 0755);
			mkdir((WizardSupervisor::SERVICE_DATA_PATH + "/OtsWizardData").c_str(), 0755);

			std::ofstream writeSecurityFile;

			writeSecurityFile.open((SECURITY_FILE_NAME).c_str());
			if(writeSecurityFile.is_open())
				writeSecurityFile << submittedSecurity;
			else
			{
				__COUT_ERR__ << "Error writing file!" << std::endl;
				*out << "Error";
				return;
			}

			writeSecurityFile.close();
		}
		else
		{
			__COUT_ERR__ << "Invalid submittedSecurity string: " << submittedSecurity << std::endl;
			*out << "Error";
			return;
		}
	}

	{
		// Always return the file
		std::ifstream securityFile;
		std::string   security;

		securityFile.open((SECURITY_FILE_NAME).c_str());
		if(securityFile.is_open())
		{
			std::getline(securityFile, security);
			securityFile.close();
		}
		else
			security = WebUsers::SECURITY_TYPE_DEFAULT;  // default security when no file exists

		*out << security;
	}

}  // end editSecurity()

//==============================================================================
void WizardSupervisor::UserSettings(xgi::Input* in, xgi::Output* out)
{
	// if sequence doesn't match up -> return
	cgicc::Cgicc cgi(in);
	std::string  submittedSequence = CgiDataUtilities::postData(cgi, "sequence");
	std::string  securityFileName  = SECURITY_FILE_NAME;
	std::string  Command;
	if((Command = CgiDataUtilities::postData(cgi, "RequestType")) == "")
		Command = cgi("RequestType");  // get command from form, if PreviewEntry

	__COUT__ << Command << std::endl;
	__COUT__ << "We are viewing Users' Settings!" << std::endl;

	// SECURITY CHECK START ****
	if(securityCode_.compare(submittedSequence) != 0)
	{
		__COUT__ << "Unauthorized Request made, security sequence doesn't match!" << std::endl;
		__COUT__ << submittedSequence << std::endl;
		// return;
	}
	else
	{
		__COUT__ << "***Successfully authenticated security sequence." << std::endl;
	}
	// SECURITY CHECK END ****

	HttpXmlDocument xmldoc;
	// uint64_t        activeSessionIndex;
	std::string user;
	// uint8_t         userPermissions;

	if(Command != "")
	{
		__COUT__ << "Action exists!" << std::endl;
		__COUT__ << Command << std::endl;

		if(Command == "Import")
		{
			// cleanup temporary folder
			// NOTE: all input parameters for User Data will be attached to form
			//	so use cgi(xxx) to get values.
			// increment number for each temporary preview, previewPostTempIndex_
			// save entry and uploads to previewPath / previewPostTempIndex_ /.

			// cleanUpPreviews();
			// 			std::string EntryText = cgi("EntryText");
			// 			__COUT__ << "EntryText " << EntryText <<  std::endl << std::endl;
			// 			std::string EntrySubject = cgi("EntrySubject");
			// 			__COUT__ << "EntrySubject " << EntrySubject <<  std::endl <<
			// std::endl;

			// get creator name
			// std::string creator = user;
			// CgiDataUtilities::postData(cgi, "file");//
			// __COUT__ << cgi("Entry") << std::endl;
			// __COUT__ << cgi("Filename") << std::endl;
			// __COUT__ << cgi("Imported_File") << std::endl;

			std::string      pathToTemporalFolder = USER_IMPORT_EXPORT_PATH + "tmp/";
			filesystem::path temporaryPath        = pathToTemporalFolder;

			if(filesystem::exists(temporaryPath))
				__COUT__ << temporaryPath << " exists!" << std::endl;
			else
			{
				__COUT__ << temporaryPath << " does not exist! Creating it now. " << std::endl;
				filesystem::create_directory(temporaryPath);
			}

			// read the uploaded values
			const std::vector<cgicc::FormFile> files = cgi.getFiles();
			// __COUT__ << "FormFiles: " << sizeof(files) << std::endl;
			// __COUT__ << "Number of files: " << files.size() << std::endl;
			// sets uploaded values in a file with the same name within the structure
			for(unsigned int i = 0; i < files.size(); ++i)
			{
				std::string filename = pathToTemporalFolder + files[i].getFilename();
				//  __SS__ << filename << __E__;
				//  __COUT_ERR__ << "\n" << ss.str();
				std::size_t tarFoundFlag   = filename.find(".tar");
				std::size_t tarGzFoundFlag = filename.find("gz");
				std::size_t tarBzFoundFlag = filename.find("bz2");

				if(tarFoundFlag != std::string::npos)
				{
					if(tarGzFoundFlag != std::string::npos || tarBzFoundFlag != std::string::npos)
					{
						__SS__ << "This is not a valid tar file, due to bad extension naming" << __E__;
						__COUT_ERR__ << "\n" << ss.str();
					}
					else
					{
						std::ofstream myFile;
						myFile.open(filename.c_str());
						files[0].writeToStream(myFile);
						std::string commandToDecompressUserData = std::string("tar -xvf ") + filename;
						filesystem::current_path(USER_IMPORT_EXPORT_PATH);
						system(commandToDecompressUserData.c_str());
						// std::string resultString = StringMacros::exec(commandToDecompressUserData.c_str());
						// __COUTV__(resultString);
					}
				}
				else
				{
					__SS__ << "This is not a valid tar file for user preferences" << __E__;
					__COUT_ERR__ << "\n" << ss.str();
				}
			}

			// __COUT__ << files[0].getFilename() << std::endl;
			// __COUT__ << "********************Files Begin********************" << std::endl;
			// for(unsigned int i = 0; i < files.size(); ++i)
			// {
			// 	__COUT__ << files[i].getDataType() << std::endl;
			// }
			// __COUT__ << "*********************Files End*********************" << std::endl;

			//			savePostPreview(EntrySubject, EntryText, cgi.getFiles(), creator,
			//&xmldoc);  else xmldoc.addTextElementToData(XML_STATUS,"Failed - could not
			// get username info.");
		}
		else if(Command == "Export")
		{
			// __SS__ << "This has been commented out due to problems compiling. Contact "
			//           "system admins."
			//        << __E__;
			//__COUT_ERR__ << "\n" << ss.str();

			// // Check for a TMP directory; if it doesn't exist, make it
			std::string      pathToTemporalFolder = USER_IMPORT_EXPORT_PATH + "tmp/";
			filesystem::path temporaryPath        = pathToTemporalFolder;

			if(filesystem::exists(temporaryPath))
				__COUT__ << temporaryPath << " exists!" << std::endl;
			else
			{
				__COUT__ << temporaryPath << " does not exist! Creating it now. " << std::endl;
				filesystem::create_directory(temporaryPath);
			}
			std::string commandToCompressUserData = std::string("tar -cvf user_settings.tar ") + std::string("ActiveTableGroups.cfg ") +
			                                        std::string("ConsolePreferences ") + std::string("CoreTableInfoNames.dat ") + std::string("LoginData ") +
			                                        std::string("OtsWizardData ") + std::string("ProgressBarData ");

			filesystem::current_path(USER_IMPORT_EXPORT_PATH);
			system(commandToCompressUserData.c_str());
			filesystem::rename("user_settings.tar", "tmp/user_settings.tar");
			filesystem::current_path(temporaryPath);
			__COUT__ << system("echo You can find the output on the following path ") << __E__;
			__COUT__ << system("pwd") << std::endl;
		}
		else
		{
			__COUT__ << "Command request not recognized: " << Command << std::endl;
			*out << "Error";
			return;
		}
	}

	*out << "test";
	return;
} //end UserSettings()

//==============================================================================
//	validateUploadFileType
//      returns "" if file type is invalid, else returns file extension to use
std::string WizardSupervisor::validateUploadFileType(const std::string fileType)
{
	for(unsigned int i = 0; i < allowedFileUploadTypes_.size(); ++i)
		if(allowedFileUploadTypes_[i] == fileType)
			return matchingFileUploadTypes_[i];  // found and done

	return "";  // not valid, return ""
} //end validateUploadFileType()

//==============================================================================
//	cleanUpPreviews
//      cleanup logbook preview directory
//      all names have time_t creation time + "_" + incremented index
void WizardSupervisor::cleanUpPreviews()
{
	std::string userData = (std::string)USER_IMPORT_EXPORT_PATH;

	DIR* dir = opendir(userData.c_str());
	if(!dir)
	{
		__COUT__ << "Error - User Data directory missing: " << userData << std::endl;
		return;
	}

	struct dirent* entry;
	time_t         dirCreateTime;
	unsigned int   i;

	while((entry = readdir(dir)))  // loop through all entries in directory and remove anything expired
	{
		if(strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0 && strcmp(entry->d_name, ".svn") != 0)
		{
			// replace _ with space so sscanf works
			for(i = 0; i < strlen(entry->d_name); ++i)
				if(entry->d_name[i] == '_')
				{
					entry->d_name[i] = ' ';
					break;
				}
			sscanf(entry->d_name, "%li", &dirCreateTime);

			if((time(0) - dirCreateTime) > USER_DATA_EXPIRATION_TIME)
			{
				__COUT__ << "Expired" << std::endl;

				entry->d_name[i] = '_';  // put _ back

				__COUT__ << "rm -rf " << USER_IMPORT_EXPORT_PATH + (std::string)entry->d_name << std::endl << std::endl;
				system(((std::string)("rm -rf " + userData + (std::string)entry->d_name)).c_str());
			}
		}
	}

	closedir(dir);
} //end cleanUpPreviews()

//==============================================================================
//	savePostPreview
//      save post to preview directory named with time and incremented index
void WizardSupervisor::savePostPreview(
    std::string& /*subject*/, std::string& /*text*/, const std::vector<cgicc::FormFile>& /*files*/, std::string /*creator*/, HttpXmlDocument* xmldoc)
{
	/*if(activeExperiment_ == "") //no active experiment!
	{
	    if(xmldoc) xmldoc->addTextElementToData(XML_STATUS,"Failed - no active experiment
	currently!"); return;
	}
*/
	char fileIndex[40];
	sprintf(fileIndex, "%lu_%lu", time(0),
	        clock());  // create unique time label for entry time(0)_clock()
	std::string userDataPath = (std::string)USER_IMPORT_EXPORT_PATH + (std::string)fileIndex;

	__COUT__ << "userDataPath " << userDataPath << std::endl;
	if(-1 == mkdir(userDataPath.c_str(), 0755))
	{
		if(xmldoc)
			xmldoc->addTextElementToData(XML_STATUS, "Failed - directory could not be generated.");
		return;
	}
	/*
	//new directory created successfully, save text and files
	//entry structure:
	//  <XML_LOGBOOK_ENTRY>
	//		<XML_LOGBOOK_ENTRY_TIME>
	//		<XML_LOGBOOK_ENTRY_CREATOR>
	//      <XML_LOGBOOK_ENTRY_SUBJECT>
	//      <XML_LOGBOOK_ENTRY_TEXT>
	//      <XML_LOGBOOK_ENTRY_FILE value=fileType0>
	//      <XML_LOGBOOK_ENTRY_FILE value=fileType1> ...
	//  </XML_LOGBOOK_ENTRY>

	escapeLogbookEntry(text);
	escapeLogbookEntry(subject);
	__COUT__ << "~~subject " << subject << std::endl << "~~text " << text << std::endl <<
	std::endl;

	HttpXmlDocument previewXml;

	previewXml.addTextElementToData(XML_LOGBOOK_ENTRY);
	previewXml.addTextElementToParent(XML_LOGBOOK_ENTRY_TIME, fileIndex,
	XML_LOGBOOK_ENTRY); if(xmldoc)
	xmldoc->addTextElementToData(XML_LOGBOOK_ENTRY_TIME,fileIndex); //return time
	previewXml.addTextElementToParent(XML_LOGBOOK_ENTRY_CREATOR, creator,
	XML_LOGBOOK_ENTRY); if(xmldoc)
	xmldoc->addTextElementToData(XML_LOGBOOK_ENTRY_CREATOR,creator); //return creator
	previewXml.addTextElementToParent(XML_LOGBOOK_ENTRY_TEXT, text, XML_LOGBOOK_ENTRY);
	if(xmldoc) xmldoc->addTextElementToData(XML_LOGBOOK_ENTRY_TEXT,text); //return text
	previewXml.addTextElementToParent(XML_LOGBOOK_ENTRY_SUBJECT, subject,
	XML_LOGBOOK_ENTRY); if(xmldoc)
	xmldoc->addTextElementToData(XML_LOGBOOK_ENTRY_SUBJECT,subject); //return subject

	__COUT__ << "file size " << files.size() << std::endl;

	std::string filename;
	std::ofstream myfile;
	for (unsigned int i=0; i<files.size(); ++i)
	{

	    previewXml.addTextElementToParent(XML_LOGBOOK_ENTRY_FILE, files[i].getDataType(),
	XML_LOGBOOK_ENTRY); if(xmldoc)
	xmldoc->addTextElementToData(XML_LOGBOOK_ENTRY_FILE,files[i].getDataType()); //return
	file type

	    if((filename = validateUploadFileType(files[i].getDataType())) == "") //invalid
	file type
	    {
	        if(xmldoc) xmldoc->addTextElementToData(XML_STATUS,"Failed - invalid file
	type, " + files[i].getDataType() + "."); return;
	    }*/

	/*//file validated, so save upload to temp directory
	    sprintf(fileIndex,"%d",i);
	    filename = previewPath + "/" + (std::string)LOGBOOK_PREVIEW_UPLOAD_PREFACE +
	            (std::string)fileIndex + "." + filename;

	    __COUT__ << "file " << i << " - " << filename << std::endl;
	    myfile.open(filename.c_str());
	    if (myfile.is_open())
	    {
	        files[i].writeToStream(myfile);
	        myfile.close();
	    }
	}*/
	/*
	//save xml doc for preview entry
	previewXml.saveXmlDocument(USER_IMPORT_EXPORT_PATH + "/" + (std::string)LOGBOOK_PREVIEW_FILE);

	if(xmldoc) xmldoc->addTextElementToData(XML_STATUS,"1"); //1 indicates success!
	if(xmldoc) xmldoc->addTextElementToData(XML_PREVIEW_INDEX,"1"); //1 indicates is a
	preview post*/
} //end savePostPreview()
