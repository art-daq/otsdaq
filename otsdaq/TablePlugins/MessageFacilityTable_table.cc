#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/Macros/TablePluginMacros.h"
#include "otsdaq/TablePlugins/MessageFacilityTable.h"

#include <stdio.h>
#include <fstream>  // std::fstream
#include <iostream>
using namespace ots;

// clang-format off
#define MF_CFG_FILE 						std::string(__ENV__("USER_DATA")) + "/MessageFacilityConfigurations/MessageFacilityGen.fcl"
#define MF_ARTDAQ_INTERFACE_CFG_FILE 		std::string(__ENV__("USER_DATA")) + "/MessageFacilityConfigurations/ARTDAQInterfaceMessageFacilityGen.fcl"
#define QT_CFG_FILE 						std::string(__ENV__("USER_DATA")) + "/MessageFacilityConfigurations/QTMessageViewerGen.fcl"
#define QUIET_CFG_FILE 						std::string(__ENV__("USER_DATA")) + "/MessageFacilityConfigurations/QuietForwarderGen.cfg"
#define USE_WEB_BOOL_FILE                 	std::string(__ENV__("USER_DATA")) + "/MessageFacilityConfigurations/UseWebConsole.bool"
#define USE_QT_BOOL_FILE                  	std::string(__ENV__("USER_DATA")) + "/MessageFacilityConfigurations/UseQTViewer.bool"

// MessageFacilityTable Column names
#define COL_NAME 							"UID"
#define COL_STATUS 							TableViewColumnInfo::COL_NAME_STATUS
#define COL_ENABLE_FWD 						"EnableUDPForwarding"

#define COL_USE_WEB 						"ForwardToWebConsoleGUI"
#define COL_WEB_IP 							"WebConsoleForwardingIPAddress"
#define COL_WEB_PORT0 						"WebConsoleForwardingPort0"
#define COL_WEB_PORT1 						"WebConsoleForwardingPort1"

#define COL_USE_QT 							"ForwardToQTViewerGUI"
#define COL_QT_IP 							"QTViewerForwardingIPAddress"
#define COL_QT_PORT 						"QTViewerForwardingPort"

#undef __COUT__
#define __COUT__ __COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__

// clang-format on

MessageFacilityTable::MessageFacilityTable(void) : TableBase("MessageFacilityTable")
{
	//////////////////////////////////////////////////////////////////////
	// WARNING: the names used in C++ MUST match the Table INFO  //
	//////////////////////////////////////////////////////////////////////
}

MessageFacilityTable::~MessageFacilityTable(void) {}

void MessageFacilityTable::init(ConfigurationManager* configManager)
{
	// use isFirstAppInContext to only run once per context, for example to avoid
	//	generating files on local disk multiple times.
	bool isFirstAppInContext = configManager->isOwnerFirstAppInContext();

	//__COUTV__(isFirstAppInContext);
	if(!isFirstAppInContext)
		return;

	//	__COUT__ << "*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*" << std::endl;
	//	__COUT__ << configManager->__SELF_NODE__ << std::endl;

	bool        enableFwd = true, useWeb = true, useQT = false;  // default to enabling web console
	int         fwdPort, destFwdPort;
	std::string fwdIP;

	// attempt to setup forward IP:Port:FwdPort defaults
	try
	{
		fwdPort = atoi(__ENV__("OTS_MAIN_PORT")) + 30000;
	}
	catch(...)
	{
	}  // ignore errors (assume user will setup in table)
	try
	{
		destFwdPort = atoi(__ENV__("OTS_MAIN_PORT")) + 30001;
	}
	catch(...)
	{
	}  // ignore errors (assume user will setup in table)
	try
	{
		fwdIP = __ENV__("HOSTNAME");
	}
	catch(...)
	{
	}  // ignore errors (assume user will setup in table)

	auto childrenMap = configManager->__SELF_NODE__.getChildren();

	std::stringstream fclSs;

	// loop through all children just to be same as other tables
	//	exit loop after first active one
	for(auto& child : childrenMap)
	{
		bool status;
		child.second.getNode(COL_STATUS).getValue(status);

		if(!status)
			continue;  // skip inactive rows

		child.second.getNode(COL_ENABLE_FWD).getValue(enableFwd);

		child.second.getNode(COL_USE_WEB).getValue(useWeb);
		child.second.getNode(COL_USE_QT).getValue(useQT);

		if(!child.second.getNode(COL_WEB_PORT0).isDefaultValue())
			child.second.getNode(COL_WEB_PORT0).getValue(fwdPort);

		if(!child.second.getNode(COL_WEB_IP).isDefaultValue())
			child.second.getNode(COL_WEB_IP).getValue(fwdIP);

		if(!child.second.getNode(COL_WEB_PORT1).isDefaultValue())
			child.second.getNode(COL_WEB_PORT1).getValue(destFwdPort);

		if(useQT)
		{
			if(!child.second.getNode(COL_QT_IP).isDefaultValue())
				child.second.getNode(COL_QT_IP).getValue(fwdIP);

			if(!child.second.getNode(COL_QT_PORT).isDefaultValue())
				child.second.getNode(COL_QT_PORT).getValue(fwdPort);
		}

		__COUT__ << "Found FWD/WEB/QT " << (COL_ENABLE_FWD ? "true" : "false") << "/" << (COL_USE_WEB ? "true" : "false") << "/"
		         << (COL_USE_QT ? "true" : "false") << " and IP:Port:FwdPort " << fwdIP << ":" << fwdPort << ":" << destFwdPort << " in MesageFacility table."
		         << __E__;
		break;  // take first enable row only!
	}           // end record loop

	if(useWeb && useQT)
	{
		__SS__ << "Illegal Message Facility table: "
		       << "Can only enable Web Console or QT Viewer, not both." << std::endl;
		__SS_THROW__;
	}

	std::fstream bfs;
	// output use web bool for StartOTS.sh
	bfs.open(USE_WEB_BOOL_FILE, std::fstream::out | std::fstream::trunc);
	if(bfs.fail())
	{
		__SS__ << "Failed to open boolean Use of Web Console table file: " << USE_WEB_BOOL_FILE << std::endl;
		__SS_THROW__;
	}
	bfs << (useWeb ? 1 : 0);
	bfs.close();

	// output use web bool for StartOTS.sh
	bfs.open(USE_QT_BOOL_FILE, std::fstream::out | std::fstream::trunc);
	if(bfs.fail())
	{
		__SS__ << "Failed to open boolean Use of QT Viewer table file: " << USE_QT_BOOL_FILE << std::endl;
		__SS_THROW__;
	}
	bfs << (useQT ? 1 : 0);
	bfs.close();

	if(enableFwd)  // write udp forward config
	{
		// handle using web gui
		if(useWeb)
		{
			__COUT__ << "Forwarding to Web GUI at IP:Port:FwdPort " << fwdIP << ":" << fwdPort << ":" << destFwdPort << " with UDP forward MesageFacility."
			         << __E__;

			fclSs << "otsConsole: {\n";
			fclSs << "\t"
			      << "type: UDP\n";
			fclSs << "\t"
			      << "threshold: DEBUG\n";
			fclSs << "\t"
			      << "filename_delimit: \"/src\"\n";
			fclSs << "\t"
			      << "port: " << fwdPort << "\n";
			fclSs << "\t"
			      << "host: \"" << fwdIP << "\"\n";
			fclSs << "}\n";

			fclSs << "console: {\n";
			fclSs << "\t"
			      << "type: \"OTS\"\n";
			fclSs << "\t"
			      << "threshold: \"DEBUG\"\n";
			fclSs << "\t"
			      << "filename_delimit: \"/src\"\n";
			fclSs << "\t"
			      << "format_string: \"|%L:%N: %f:%u |\t%m\"\n";

			fclSs << "\n}\n";

			// output quiet forwarder config file
			std::stringstream qtSs;
			qtSs << "RECEIVE_PORT \t " << fwdPort << "\n";
			qtSs << "DESTINATION_PORT \t " << destFwdPort << "\n";
			qtSs << "DESTINATION_IP \t " << fwdIP << "\n";

			std::fstream qtfs;
			qtfs.open(QUIET_CFG_FILE, std::fstream::out | std::fstream::trunc);
			if(qtfs.fail())
			{
				__SS__ << "Failed to open Web Console's 'Quiet Forwarder' "
				          "table file: "
				       << QUIET_CFG_FILE << std::endl;
				__SS_THROW__;
			}
			qtfs << qtSs.str();
			qtfs.close();
			__COUT__ << "Wrote " << QUIET_CFG_FILE << ":" << __E__ << qtSs.str() << __E__;
		}

		// handle using qt viewer
		if(useQT)
		{
			__COUT__ << "Forwarding to QT GUI  at IP:Port " << fwdIP << ":" << fwdPort << " with UDP forward MesageFacility." << __E__;

			fclSs << "otsViewer: {\n";
			fclSs << "\t"
			      << "type: UDP\n";
			fclSs << "\t"
			      << "threshold: DEBUG\n";
			fclSs << "\t"
			      << "filename_delimit: \"/src\"\n";
			fclSs << "\t"
			      << "port: " << fwdPort << "\n";
			fclSs << "\t"
			      << "host: \"" << fwdIP << "\"\n";
			fclSs << "}\n";

			// output QT Viewer config file
			std::fstream qtfs;
			qtfs.open(QT_CFG_FILE, std::fstream::out | std::fstream::trunc);
			if(qtfs.fail())
			{
				__SS__ << "Failed to open QT Message Viewer table file: " << QT_CFG_FILE << std::endl;
				__SS_THROW__;
			}
			qtfs << "receivers: \n{\n";
			qtfs << "\t"
			     << "syslog: \n{\n";
			qtfs << "\t\t"
			     << "receiverType: "
			     << "\"UDP\""
			     << "\n";
			qtfs << "\t\t"
			     << "port: " << fwdPort << "\n";
			qtfs << "\t}\n";  // close syslog
			qtfs << "}\n";    // close receivers
			qtfs << "\n";
			qtfs << "threshold: "
			     << "DEBUG"
			     << "\n";
			qtfs.close();
		}
	}
	else  // write cout config (not forwarding to external process)
	{
		__COUT__ << "Using cout-only MesageFacility table." << std::endl;
		fclSs << "console: {\n";
		fclSs << "\t"
		      << "type: \"OTS\"\n";
		fclSs << "\t"
		      << "threshold: \"DEBUG\"\n";
		fclSs << "\t"
		      << "filename_delimit: \"/src\"\n";
		fclSs << "\t"
		      << "format_string: \"|%L:%N:%f [%u]\t%m\"\n";

		fclSs << "\n}\n";
	}

	// generate MF_CFG_FILE file
	std::fstream fs;
	fs.open(MF_CFG_FILE, std::fstream::out | std::fstream::trunc);
	if(fs.fail())
	{
		__SS__ << "Failed to open Message Facility table file: " << MF_CFG_FILE << __E__;
		__SS_THROW__;
	}
	else
		__COUT__ << "Opened.. " << MF_CFG_FILE << __E__;

	// generate MF_CFG_MF_ARTDAQ_INTERFACE_CFG_FILE file
	std::fstream artdaqfs;
	artdaqfs.open(MF_ARTDAQ_INTERFACE_CFG_FILE, std::fstream::out | std::fstream::trunc);
	if(artdaqfs.fail())
	{
		__SS__ << "Failed to open artdaq interface Message Facility table file: " << MF_ARTDAQ_INTERFACE_CFG_FILE << __E__;
		__SS_THROW__;
	}
	else
		__COUT__ << "Opened for artdaq.. " << MF_ARTDAQ_INTERFACE_CFG_FILE << __E__;

	// close MF config files
	artdaqfs << fclSs.str();
	// fs << fclSs.str() << "\nfile: {type:\"file\" filename:\"/dev/null\"}\n";
	fs << fclSs.str() << "\nfile: \"\"\n";
	fs.close();
	artdaqfs.close();

	__COUT__ << "Wrote " << __E__ << fclSs.str() << __E__;

}  // end init()

DEFINE_OTS_TABLE(MessageFacilityTable)
