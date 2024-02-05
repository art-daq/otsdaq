#include "otsdaq/TablePlugins/XDAQContextTable/XDAQContextTable.h"
#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"

#include <stdio.h>
#include <fstream>  // std::fstream
#include <iostream>

using namespace ots;

// clang-format off

#define XDAQ_RUN_FILE         std::string(__ENV__("XDAQ_CONFIGURATION_DATA_PATH")) + "/" + std::string(__ENV__("XDAQ_CONFIGURATION_XML")) + ".xml"
#define APP_PRIORITY_FILE     std::string(__ENV__("XDAQ_CONFIGURATION_DATA_PATH")) + "/" + "xdaqAppStateMachinePriority"

const std::string XDAQContextTable::DEPRECATED_SUPERVISOR_CLASS 				= "ots::Supervisor";  // still allowed for now, in StartOTS
const std::string XDAQContextTable::GATEWAY_SUPERVISOR_CLASS 					= "ots::GatewaySupervisor";
const std::string XDAQContextTable::WIZARD_SUPERVISOR_CLASS  					= "ots::WizardSupervisor";
const std::string XDAQContextTable::ARTDAQ_SUPERVISOR_CLASS  					= "ots::ARTDAQSupervisor";
const std::set<std::string> XDAQContextTable::FETypeClassNames_ 				= { "ots::FESupervisor", "ots::FEDataManagerSupervisor", "ots::ARTDAQFEDataManagerSupervisor"};
const std::set<std::string> XDAQContextTable::DMTypeClassNames_ 				= { "ots::DataManagerSupervisor", "ots::FEDataManagerSupervisor", "ots::ARTDAQFEDataManagerSupervisor"};
const std::set<std::string> XDAQContextTable::LogbookTypeClassNames_ 			= { "ots::LogbookSupervisor", "ots::ECLSupervisor"};
const std::set<std::string> XDAQContextTable::MacroMakerTypeClassNames_ 		= { "ots::MacroMakerSupervisor"};
const std::set<std::string> XDAQContextTable::ChatTypeClassNames_ 				= { "ots::ChatSupervisor"};
const std::set<std::string> XDAQContextTable::ConsoleTypeClassNames_ 			= { "ots::ConsoleSupervisor"};
const std::set<std::string> XDAQContextTable::ConfigurationGUITypeClassNames_ 	= { "ots::ConfigurationGUISupervisor"};
const std::set<std::string> XDAQContextTable::CodeEditorTypeClassNames_ 		= { "ots::CodeEditorSupervisor"};
const std::set<std::string> XDAQContextTable::VisualizerTypeClassNames_ 		= { "ots::VisualSupervisor"};
const std::set<std::string> XDAQContextTable::SlowControlsTypeClassNames_ 		= { "ots::SlowControlsDashboardSupervisor"};

//NOTE!!! std::next + offset reads std::set from right-to-left above (end to beginning)
const std::map<std::string /*class*/, std::string /*module*/> XDAQContextTable::AppClassModuleLookup_ = {
	std::make_pair(XDAQContextTable::GATEWAY_SUPERVISOR_CLASS,							"${OTSDAQ_LIB}/libGatewaySupervisor.so"),
	std::make_pair(XDAQContextTable::ARTDAQ_SUPERVISOR_CLASS,							"${OTSDAQ_LIB}/libARTDAQSupervisor.so"),
	std::make_pair(*(std::next(XDAQContextTable::FETypeClassNames_.begin(),2)),			"${OTSDAQ_LIB}/libCoreSupervisors.so"),
	std::make_pair(*(std::next(XDAQContextTable::FETypeClassNames_.begin(),1)),			"${OTSDAQ_LIB}/libCoreSupervisors.so"),
	std::make_pair(*(XDAQContextTable::FETypeClassNames_.begin()),						"${OTSDAQ_LIB}/libCoreSupervisors.so"),
	std::make_pair(*(std::next(XDAQContextTable::DMTypeClassNames_.begin(),2)),			"${OTSDAQ_LIB}/libCoreSupervisors.so"),
	std::make_pair(*(std::next(XDAQContextTable::DMTypeClassNames_.begin(),1)),			"${OTSDAQ_LIB}/libCoreSupervisors.so"),
	std::make_pair(*(XDAQContextTable::DMTypeClassNames_.begin()),						"${OTSDAQ_LIB}/libCoreSupervisors.so"),
	std::make_pair(*(std::next(XDAQContextTable::LogbookTypeClassNames_.begin(),1)),	"${OTSDAQ_UTILITIES_LIB}/libLogbook.so"),
	std::make_pair(*(XDAQContextTable::LogbookTypeClassNames_.begin()),					"${OTSDAQ_UTILITIES_LIB}/libECLWriter.so"),
	std::make_pair(*(XDAQContextTable::MacroMakerTypeClassNames_.begin()),				"${OTSDAQ_UTILITIES_LIB}/libMacroMaker.so"),
	std::make_pair(*(XDAQContextTable::ChatTypeClassNames_.begin()),					"${OTSDAQ_UTILITIES_LIB}/libChat.so"),
	std::make_pair(*(XDAQContextTable::ConsoleTypeClassNames_.begin()),					"${OTSDAQ_UTILITIES_LIB}/libConsole.so"),
	std::make_pair(*(XDAQContextTable::ConfigurationGUITypeClassNames_.begin()),		"${OTSDAQ_UTILITIES_LIB}/libConfigurationGUI.so"),
	std::make_pair(*(XDAQContextTable::CodeEditorTypeClassNames_.begin()),				"${OTSDAQ_LIB}/libCodeEditor.so"),
	std::make_pair(*(XDAQContextTable::VisualizerTypeClassNames_.begin()),				"${OTSDAQ_UTILITIES_LIB}/libVisualization.so"),
	std::make_pair(*(XDAQContextTable::SlowControlsTypeClassNames_.begin()),			"${OTSDAQ_UTILITIES_LIB}/libSlowControlsDashboard.so")
};
// Module Choices from fixed choice XDAQApp table:
//		${OTSDAQ_LIB}/libGatewaySupervisor.so
//		${OTSDAQ_LIB}/libCoreSupervisors.so
//		${OTSDAQ_LIB}/libEventBuilderApp.so
//		${OTSDAQ_UTILITIES_LIB}/libChat.so
//		${OTSDAQ_UTILITIES_LIB}/libConsole.so
//		${OTSDAQ_UTILITIES_LIB}/libLogbook.so
//		${OTSDAQ_UTILITIES_LIB}/libVisualization.so
//		${OTSDAQ_UTILITIES_LIB}/libConfigurationGUI.so
//		${OTSDAQ_UTILITIES_LIB}/libSlowControlsDashboard.so
//		${OTSDAQ_UTILITIES_LIB}/libMacroMaker.so
//		${OTSDAQ_LIB}/libARTDAQSupervisor.so
//		${OTSDAQ_LIB}/libARTDAQOnlineMonitor.so
//		${OTSDAQ_LIB}/libDispatcherApp.so
//		${OTSDAQ_LIB}/libDataLoggerApp.so
//		${OTSDAQ_LIB}/libCodeEditor.so

// Class Choices from fixed choice XDAQApp table:
//		ots::GatewaySupervisor
//		ots::FESupervisor
//		ots::DataManagerSupervisor
//		ots::FEDataManagerSupervisor
//		ots::ARTDAQDataManagerSupervisor
//		ots::ARTDAQFEDataManagerSupervisor
//		ots::ARTDAQSupervisor
//		ots::ARTDAQOnlineMonitorSupervisor
//		ots::EventBuilderApp
//		ots::DispatcherApp
//		ots::DataLoggerApp
//		ots::ConfigurationGUISupervisor
//		ots::MacroMakerSupervisor
//		ots::VisualSupervisor
//		ots::ChatSupervisor
//		ots::ConsoleSupervisor
//		ots::LogbookSupervisor
//		ots::SlowControlsDashboardSupervisor
//		ots::CodeEditorSupervisor


const uint8_t		 	XDAQContextTable::XDAQApplication::DEFAULT_PRIORITY 	= 100;
const unsigned int 		XDAQContextTable::XDAQApplication::GATEWAY_APP_ID 		= 200;

XDAQContextTable::ColContext 				XDAQContextTable::colContext_ 		= XDAQContextTable::ColContext();  // initialize static member
XDAQContextTable::ColApplication 			XDAQContextTable::colApplication_ 	= XDAQContextTable::ColApplication();  // initialize static member
XDAQContextTable::ColApplicationProperty 	XDAQContextTable::colAppProperty_ 	= XDAQContextTable::ColApplicationProperty();  // initialize static member

// clang-format on

//==============================================================================
XDAQContextTable::XDAQContextTable(void) : TableBase(ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME), artdaqSupervisorContext_((unsigned int)-1)
{
	//////////////////////////////////////////////////////////////////////
	// WARNING: the names used in C++ MUST match the Table INFO  //
	//////////////////////////////////////////////////////////////////////

	// KEEP FOR DEBUGGING next traversal order through set
	//  for(auto &lu:AppClassModuleLookup_)
	//  	__COUT__ << lu.first << " " << lu.second << __E__;

	// __COUTV__(*(XDAQContextTable::FETypeClassNames_.begin()));
	// __COUTV__(*(std::next(XDAQContextTable::FETypeClassNames_.begin(),1)));
	// __COUTV__(*(std::next(XDAQContextTable::FETypeClassNames_.begin(),2)));
}  // end constructor

//==============================================================================
XDAQContextTable::~XDAQContextTable(void) {}

//==============================================================================
void XDAQContextTable::init(ConfigurationManager* configManager)
{
	//__COUT__ << "init" << __E__;
	extractContexts(configManager);

	bool isFirstAppInContext = configManager->isOwnerFirstAppInContext();

	//__COUTV__(isFirstAppInContext);
	if(!isFirstAppInContext)
		return;

	{
		/////////////////////////
		// generate xdaq run parameter file
		std::fstream fs;
		fs.open(XDAQ_RUN_FILE, std::fstream::out | std::fstream::trunc);
		if(fs.fail())
		{
			__SS__ << "Failed to open XDAQ run file: " << XDAQ_RUN_FILE << __E__;
			__SS_THROW__;
		}
		outputXDAQXML((std::ostream&)fs);
		fs.close();
	}
}  // end init()

//==============================================================================
std::string XDAQContextTable::getContextAddress(const std::string& contextUID, bool wantHttp) const
{
	if(contextUID == "X")
		return "";
	for(auto& context : contexts_)
	{
		if(context.contextUID_ == contextUID)
		{
			if(wantHttp)
				return context.address_;
			size_t i = context.address_.find("://");
			if(i == std::string::npos)
				i = 0;
			else
				i += 3;
			return context.address_.substr(i);
		}
	}
	return "";
}  // end getContextAddress()

//==============================================================================
const XDAQContextTable::XDAQContext* XDAQContextTable::getTheARTDAQSupervisorContext() const
{
	if(artdaqSupervisorContext_ >= contexts_.size())
		return nullptr;
	return &contexts_[artdaqSupervisorContext_];
}  // end getTheARTDAQSupervisorContext()

//==============================================================================
ConfigurationTree XDAQContextTable::getContextNode(const ConfigurationManager* configManager, const std::string& contextUID)
{
	return configManager->getNode(ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME).getNode(contextUID);
}  // end getContextNode()

//==============================================================================
ConfigurationTree XDAQContextTable::getApplicationNode(const ConfigurationManager* configManager, const std::string& contextUID, const std::string& appUID)
{
	return configManager->getNode(ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME)
	    .getNode(contextUID + "/" + colContext_.colLinkToApplicationTable_ + "/" + appUID);
}  // end getApplicationNode()

//==============================================================================
ConfigurationTree XDAQContextTable::getSupervisorConfigNode(const ConfigurationManager* configManager, const std::string& contextUID, const std::string& appUID)
{
	return configManager->getNode(ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME)
	    .getNode(contextUID + "/" + XDAQContextTable::colContext_.colLinkToApplicationTable_ + "/" + appUID + "/" +
	             XDAQContextTable::colApplication_.colLinkToSupervisorTable_);
}  // end getSupervisorConfigNode()

//==============================================================================
// extractContexts
//	Could be called by other tables if they need to access the context.
//		This doesn't re-write config files, it just re-makes constructs in software.
void XDAQContextTable::extractContexts(ConfigurationManager* configManager)
{
	//__COUT__ << "*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*" << __E__;
	//__COUT__ << configManager->__SELF_NODE__ << __E__;

	//	__COUT__ << configManager->getNode(this->getTableName()).getValueAsString()
	//		  											  << __E__;

	auto children = configManager->__SELF_NODE__.getChildren();

	contexts_.clear();                            // reset
	artdaqSupervisorContext_ = (unsigned int)-1;  // reset

	// Enforce that app IDs do not repeat!
	//	Note: this is important because there are maps in MacroMaker and
	// SupervisorDescriptorInfoBase that rely on localId() as key
	std::set<unsigned int /*appId*/> appIdSet;

	for(auto& child : children)
	{
		contexts_.push_back(XDAQContext());
		//__COUT__ << child.first << __E__;
		//		__COUT__ << child.second.getNode(colContextUID_) << __E__;

		contexts_.back().contextUID_ = child.first;

		contexts_.back().sourceConfig_ =
		    child.second.getTableName() + "_v" + child.second.getTableVersion().toString() + " @ " + std::to_string(child.second.getTableCreationTime());
		child.second.getNode(colContext_.colContextUID_).getValue(contexts_.back().contextUID_);
		child.second.getNode(colContext_.colStatus_).getValue(contexts_.back().status_);
		child.second.getNode(colContext_.colId_).getValue(contexts_.back().id_);
		child.second.getNode(colContext_.colAddress_).getValue(contexts_.back().address_);
		child.second.getNode(colContext_.colPort_).getValue(contexts_.back().port_);

		// help the user out if the config has old defaults for port/address
		// Same as CorePropertySupervisorBase.cc:indicateOtsAlive:L156
		if(contexts_.back().port_ == 0)  // convert 0 to ${OTS_MAIN_PORT}
			contexts_.back().port_ = atoi(__ENV__("OTS_MAIN_PORT"));
		if(contexts_.back().address_ == "DEFAULT")  // convert DEFAULT to http://${HOSTNAME}
			contexts_.back().address_ = "http://" + std::string(__ENV__("HOSTNAME"));
		if(contexts_.back().port_ < 1024 || contexts_.back().port_ > 49151)
		{
			__SS__ << "Illegal xdaq Context port: " << contexts_.back().port_ << ". Port must be between 1024 and 49151." << __E__;
		}
		// child.second.getNode(colContext_.colARTDAQDataPort_).getValue(contexts_.back().artdaqDataPort_);

		//__COUT__ << contexts_.back().address_ << __E__;
		auto appLink = child.second.getNode(colContext_.colLinkToApplicationTable_);
		if(appLink.isDisconnected())
		{
			__SS__ << "Application link is disconnected!" << __E__;
			__SS_THROW__;
		}

		// add xdaq applications to this context
		auto appChildren = appLink.getChildren();
		for(auto appChild : appChildren)
		{
			//__COUT__ << "Loop: " << child.first << "/" << appChild.first << __E__;

			contexts_.back().applications_.push_back(XDAQApplication());

			contexts_.back().applications_.back().applicationGroupID_ = child.first;
			contexts_.back().applications_.back().sourceConfig_ = appChild.second.getTableName() + "_v" + appChild.second.getTableVersion().toString() + " @ " +
			                                                      std::to_string(appChild.second.getTableCreationTime());

			appChild.second.getNode(colApplication_.colApplicationUID_).getValue(contexts_.back().applications_.back().applicationUID_);
			appChild.second.getNode(colApplication_.colStatus_).getValue(contexts_.back().applications_.back().status_);
			appChild.second.getNode(colApplication_.colClass_).getValue(contexts_.back().applications_.back().class_);
			appChild.second.getNode(colApplication_.colId_).getValue(contexts_.back().applications_.back().id_);

			// infer Gateway is XDAQContextTable::XDAQApplication::GATEWAY_APP_ID from default
			if(appChild.second.getNode(colApplication_.colId_).isDefaultValue() &&
			   (contexts_.back().applications_.back().class_ == XDAQContextTable::GATEWAY_SUPERVISOR_CLASS ||
			    contexts_.back().applications_.back().class_ == XDAQContextTable::DEPRECATED_SUPERVISOR_CLASS))
			{
				contexts_.back().applications_.back().id_ = XDAQContextTable::XDAQApplication::GATEWAY_APP_ID;
			}

			// assert Gateway is XDAQContextTable::XDAQApplication::GATEWAY_APP_ID
			if((contexts_.back().applications_.back().id_ == XDAQContextTable::XDAQApplication::GATEWAY_APP_ID &&
			    contexts_.back().applications_.back().class_ != XDAQContextTable::GATEWAY_SUPERVISOR_CLASS &&
			    contexts_.back().applications_.back().class_ != XDAQContextTable::DEPRECATED_SUPERVISOR_CLASS) ||
			   (contexts_.back().applications_.back().id_ != XDAQContextTable::XDAQApplication::GATEWAY_APP_ID &&
			    (contexts_.back().applications_.back().class_ == XDAQContextTable::GATEWAY_SUPERVISOR_CLASS ||
			     contexts_.back().applications_.back().class_ == XDAQContextTable::DEPRECATED_SUPERVISOR_CLASS)))
			{
				__SS__ << "XDAQ Application ID of " << XDAQContextTable::XDAQApplication::GATEWAY_APP_ID << " is reserved for the Gateway Supervisor's class '"
				       << XDAQContextTable::GATEWAY_SUPERVISOR_CLASS << ".' There must be one and only one XDAQ supervisor application specified with ID '"
				       << XDAQContextTable::XDAQApplication::GATEWAY_APP_ID << "' and class '" << XDAQContextTable::GATEWAY_SUPERVISOR_CLASS
				       << ".' A conflict was found specifically at appName=" << contexts_.back().applications_.back().applicationUID_
				       << " with id=" << contexts_.back().applications_.back().id_ << " and class=" << contexts_.back().applications_.back().class_ << __E__;
				__SS_THROW__;
			}

			// assert NO app id repeats if context/app enabled
			if(contexts_.back().status_ && contexts_.back().applications_.back().status_)
			{
				// assert NO app id repeats
				if(appIdSet.find(contexts_.back().applications_.back().id_) != appIdSet.end())
				{
					__SS__
					    << "XDAQ Application IDs are not unique; this could be due to multiple instances of the same XDAQ application linked to from two "
					       "seperate XDAQ Contexts (check all enabled XDAQ Contexts for replicated application IDs). Specifically, there is a duplicate at id="
					    << contexts_.back().applications_.back().id_ << " appName=" << contexts_.back().applications_.back().applicationUID_ << __E__;
					__COUT_ERR__ << "\n" << ss.str();
					__SS_THROW__;
				}
				appIdSet.insert(contexts_.back().applications_.back().id_);
			}

			// convert defaults to values
			if(appChild.second.getNode(colApplication_.colInstance_).isDefaultValue())
				contexts_.back().applications_.back().instance_ = 1;
			else
				appChild.second.getNode(colApplication_.colInstance_).getValue(contexts_.back().applications_.back().instance_);

			if(appChild.second.getNode(colApplication_.colNetwork_).isDefaultValue())
				contexts_.back().applications_.back().network_ = "local";
			else
				appChild.second.getNode(colApplication_.colNetwork_).getValue(contexts_.back().applications_.back().network_);

			if(appChild.second.getNode(colApplication_.colGroup_).isDefaultValue())
				contexts_.back().applications_.back().group_ = "daq";
			else
				appChild.second.getNode(colApplication_.colGroup_).getValue(contexts_.back().applications_.back().group_);

			// force deprecated Supervisor to GatewaySupervisor class
			if(contexts_.back().applications_.back().class_ == XDAQContextTable::DEPRECATED_SUPERVISOR_CLASS)
			{
				contexts_.back().applications_.back().class_ = XDAQContextTable::GATEWAY_SUPERVISOR_CLASS;
				__COUT__ << "Fixing deprecated Supervisor class from " << XDAQContextTable::DEPRECATED_SUPERVISOR_CLASS << " to "
				         << (contexts_.back().applications_.back().class_);
			}

			if(contexts_.back().applications_.back().class_ == XDAQContextTable::GATEWAY_SUPERVISOR_CLASS &&
			   contexts_.back().applications_.back().module_.find("libSupervisor.so") != std::string::npos)
			{
				__COUT__ << "Fixing deprecated Supervisor class from " << contexts_.back().applications_.back().module_ << " to ";
				contexts_.back().applications_.back().module_ =
				    contexts_.back().applications_.back().module_.substr(
				        0, contexts_.back().applications_.back().module_.size() - std::string("Supervisor.so").size()) +
				    "GatewaySupervisor.so";
				std::cout << contexts_.back().applications_.back().module_ << __E__;
			}

			// __COUT__ << "CHECK Inferred module of '" << contexts_.back().applications_.back().applicationUID_ << "' as '" <<
			// 	contexts_.back().applications_.back().module_ << "' " << (appChild.second.getNode(colApplication_.colModule_).isDefaultValue()?"YES":"NO") <<
			// __E__;

			// if module is default, attempt to resolve from class
			if(contexts_.back().applications_.back().id_ == XDAQContextTable::XDAQApplication::GATEWAY_APP_ID)  // force correct Gateway Supervisor module
				contexts_.back().applications_.back().module_ = XDAQContextTable::AppClassModuleLookup_.at(XDAQContextTable::GATEWAY_SUPERVISOR_CLASS);
			else if(appChild.second.getNode(colApplication_.colModule_).isDefaultValue() &&
			        XDAQContextTable::AppClassModuleLookup_.find(contexts_.back().applications_.back().class_) != XDAQContextTable::AppClassModuleLookup_.end())
			{
				contexts_.back().applications_.back().module_ = XDAQContextTable::AppClassModuleLookup_.at(contexts_.back().applications_.back().class_);
				__COUT__ << "Inferred module of '" << contexts_.back().applications_.back().applicationUID_ << "' as '"
				         << contexts_.back().applications_.back().module_ << "' based on class '" << contexts_.back().applications_.back().class_ << "'"
				         << __E__;
			}
			else  // keep module env variable!! so do getValueAsString()
				contexts_.back().applications_.back().module_ = appChild.second.getNode(colApplication_.colModule_).getValueAsString();

			try
			{
				appChild.second.getNode(colApplication_.colConfigurePriority_)
				    .getValue(contexts_.back().applications_.back().stateMachineCommandPriority_["Configure"]);
				appChild.second.getNode(colApplication_.colStartPriority_)
				    .getValue(contexts_.back().applications_.back().stateMachineCommandPriority_["Start"]);
				appChild.second.getNode(colApplication_.colStopPriority_).getValue(contexts_.back().applications_.back().stateMachineCommandPriority_["Stop"]);
			}
			catch(...)
			{
				__COUT__ << "Ignoring missing state machine priorities..." << __E__;
			}

			auto appPropertyLink = appChild.second.getNode(colApplication_.colLinkToPropertyTable_);
			if(!appPropertyLink.isDisconnected())
			{
				// add xdaq application properties to this context

				auto appPropertyChildren = appPropertyLink.getChildren();

				//__COUT__ << "appPropertyChildren.size() " << appPropertyChildren.size()
				//<< __E__;

				for(auto appPropertyChild : appPropertyChildren)
				{
					contexts_.back().applications_.back().properties_.push_back(XDAQApplicationProperty());
					contexts_.back().applications_.back().properties_.back().status_ =
					    appPropertyChild.second.getNode(colAppProperty_.colStatus_).getValue<bool>();
					contexts_.back().applications_.back().properties_.back().name_ =
					    appPropertyChild.second.getNode(colAppProperty_.colPropertyName_).getValue<std::string>();
					contexts_.back().applications_.back().properties_.back().type_ =
					    appPropertyChild.second.getNode(colAppProperty_.colPropertyType_).getValue<std::string>();
					contexts_.back().applications_.back().properties_.back().value_ =
					    appPropertyChild.second.getNode(colAppProperty_.colPropertyValue_).getValue<std::string>();

					//__COUT__ <<
					// contexts_.back().applications_.back().properties_.back().name_ <<
					// __E__;
				}
			}
			// else
			//	__COUT__ << "Disconnected." << __E__;
		}

		// check for artdaq Supervisor in context
		{
			if(!contexts_.back().status_)
				continue;  // skip if disabled

			for(auto& app : contexts_.back().applications_)
			{
				if(app.class_ ==  // if artdaq interface supervisor
				       "ots::ARTDAQSupervisor" &&
				   app.status_)
				{
					__COUT__ << "Found " << app.class_ << " in context '" << contexts_.back().contextUID_ << "'" << __E__;

					if(artdaqSupervisorContext_ < contexts_.size())
					{
						__SS__ << "Error! Only one artdaq Supervisor is allowed to be active - "
						       << "two encountered in context '" << contexts_[artdaqSupervisorContext_].contextUID_ << "' and '" << contexts_.back().contextUID_
						       << "'..." << __E__;

						artdaqSupervisorContext_ = (unsigned int)-1;  // reset

						__SS_THROW__;
					}

					artdaqSupervisorContext_ = contexts_.size() - 1;
					// break; //continue to look for invalid configuration
				}
			}  // end artdaq app loop

		}  // end artdaq context handling

	}  // end primary context loop

}  // end extractContexts()

//==============================================================================
void XDAQContextTable::outputXDAQXML(std::ostream& out)
{
	// each generated context will look something like this:
	//<xc:Context id="0" url="http://${SUPERVISOR_SERVER}:${PORT}">
	////<xc:Application class="ots::FESupervisor" id="${FEW_SUPERVISOR_ID}" instance="1"
	/// network="local" group="daq"/>
	////<xc:Module>${OTSDAQ_LIB}/libCoreSupervisors.so</xc:Module>
	//</xc:Context>

	// print xml header information and declare xc partition
	out << "<?xml version='1.0'?>\n"
	    << "<xc:Partition \txmlns:xsi\t= \"http://www.w3.org/2001/XMLSchema-instance\"\n"
	    << "\t\txmlns:soapenc\t= \"http://schemas.xmlsoap.org/soap/encoding/\"\n"
	    << "\t\txmlns:xc\t= "
	       "\"http://xdaq.web.cern.ch/xdaq/xsd/2004/XMLConfiguration-30\">\n\n";

	// print partition open
	// for each context
	//	open context
	//	for each app in context
	//		print application
	//		print module
	//	close context
	// close partition

	char tmp[200];
	for(XDAQContext& context : contexts_)
	{
		//__COUT__ << context.contextUID_ << __E__;

		sprintf(tmp, "\t<!-- ContextUID='%s' sourceConfig='%s' -->", context.contextUID_.c_str(), context.sourceConfig_.c_str());
		out << tmp << "\n";

		if(!context.status_)  // comment out if disabled
			out << "\t<!--\n";

		sprintf(tmp, "\t<xc:Context id=\"%u\" url=\"%s:%u\">", context.id_, context.address_.c_str(), context.port_);
		out << tmp << "\n\n";

		for(XDAQApplication& app : context.applications_)
		{
			//__COUT__ << app.id_ << __E__;

			if(context.status_)
			{
				sprintf(tmp,
				        "\t\t<!-- Application GroupID = '%s' UID='%s' sourceConfig='%s' -->",
				        app.applicationGroupID_.c_str(),
				        app.applicationUID_.c_str(),
				        app.sourceConfig_.c_str());
				out << tmp << "\n";

				if(!app.status_)  // comment out if disabled
					out << "\t\t<!--\n";
			}

			if(app.class_ == "ots::GatewaySupervisor")  // add otsdaq icons
				sprintf(tmp,
				        "\t\t<xc:Application class=\"%s\" id=\"%u\" instance=\"%u\" "
				        "network=\"%s\" icon=\"/WebPath/images/otsdaqIcons/logo_square.png\" icon16x16=\"/WebPath/images/otsdaqIcons/favicon-16x16.png\" "
				        "group=\"%s\">\n",
				        app.class_.c_str(),
				        app.id_,
				        app.instance_,
				        app.network_.c_str(),
				        app.group_.c_str());
			else
				sprintf(tmp,
				        "\t\t<xc:Application class=\"%s\" id=\"%u\" instance=\"%u\" "
				        "network=\"%s\" group=\"%s\">\n",
				        app.class_.c_str(),
				        app.id_,
				        app.instance_,
				        app.network_.c_str(),
				        app.group_.c_str());
			out << tmp;

			////////////////////// properties
			int foundColon = app.class_.rfind(':');
			if(foundColon >= 0)
				++foundColon;
			else
			{
				__SS__ << "Illegal XDAQApplication class name value of '" << app.class_ << "' - please check the entry for app ID = " << app.id_ << __E__;
				__SS_THROW__;
			}
			out << "\t\t\t<properties xmlns=\"urn:xdaq-application:" << app.class_.substr(foundColon) << "\" xsi:type=\"soapenc:Struct\">\n";

			//__COUT__ << "app.properties_ " << app.properties_.size() << __E__;
			for(XDAQApplicationProperty& appProperty : app.properties_)
			{
				if(appProperty.type_ == "ots::SupervisorProperty")
					continue;  // skip ots Property values

				if(!appProperty.status_)
					out << "\t\t\t\t<!--\n";

				sprintf(tmp,
				        "\t\t\t\t<%s xsi:type=\"%s\">%s</%s>\n",
				        appProperty.name_.c_str(),
				        appProperty.type_.c_str(),
				        appProperty.value_.c_str(),
				        appProperty.name_.c_str());
				out << tmp;

				if(!appProperty.status_)
					out << "\t\t\t\t-->\n";
			}
			out << "\t\t\t</properties>\n";
			////////////////////// end properties

			out << "\t\t</xc:Application>\n";

			sprintf(tmp, "\t\t<xc:Module>%s</xc:Module>\n", app.module_.c_str());
			out << tmp;

			if(context.status_ && !app.status_)
				out << "\t\t-->\n";
			out << "\n";

			//	__COUT__ << "DONE" << __E__;
		}

		out << "\t</xc:Context>\n";
		if(!context.status_)
			out << "\t-->\n";
		out << "\n";
	}

	//__COUT__ << "DONE" << __E__;
	out << "</xc:Partition>\n\n\n";
}

//==============================================================================
std::string XDAQContextTable::getContextUID(const std::string& url) const
{
	for(auto context : contexts_)
	{
		if(!context.status_)
			continue;

		if(url == context.address_ + ":" + std::to_string(context.port_))
			return context.contextUID_;
	}
	return "";
}

//==============================================================================
std::string XDAQContextTable::getApplicationUID(const std::string& url, unsigned int id) const
{
	//__COUTV__(url); __COUTV__(id);
	for(auto context : contexts_)
	{
		//__COUT__ << "Checking " << (context.address_ + ":" +
		// std::to_string(context.port_)) << __E__;
		//__COUTV__(context.status_);

		if(!context.status_)
			continue;

		//__COUT__ << "Checking " << (context.address_ + ":" +
		// std::to_string(context.port_)) << __E__;
		if(url == context.address_ + ":" + std::to_string(context.port_))
			for(auto application : context.applications_)
			{
				//__COUTV__(application.status_);	__COUTV__(application.id_);
				if(!application.status_)
					continue;

				if(application.id_ == id)
				{
					return application.applicationUID_;
				}
			}
	}
	return "";
}  // end getApplicationUID()

//==============================================================================
// only considers ON contexts and applications
std::string XDAQContextTable::getContextOfApplication(ConfigurationManager* configManager, const std::string& appUID) const
{
	// look through all contexts until first appUID found

	auto childrenMap = configManager->__SELF_NODE__.getChildren();

	for(auto& context : childrenMap)
	{
		if(!context.second.getNode(XDAQContextTable::colContext_.colStatus_).getValue<bool>())
			continue;

		ConfigurationTree appLink = context.second.getNode(XDAQContextTable::colContext_.colLinkToApplicationTable_);
		if(appLink.isDisconnected())
			continue;

		auto appMap = appLink.getChildren();
		for(auto& app : appMap)
		{
			if(!app.second.getNode(XDAQContextTable::colApplication_.colStatus_).getValue<bool>())
				continue;

			if(app.first == appUID)
				return context.first;  // return context UID
		}                              // end app search loop
	}                                  // end context search loop

	__SS__ << "Application '" << appUID << "' search found no parent context!" << __E__;
	// ss << StringMacros::stackTrace() << __E__;
	__SS_THROW__;
}  // end getContextOfApplication()

//==============================================================================
// only considers ON contexts and applications
std::string XDAQContextTable::getContextOfGateway(ConfigurationManager* configManager) const
{
	// look through all contexts until first gateway found

	auto childrenMap = configManager->__SELF_NODE__.getChildren();

	for(auto& context : childrenMap)
	{
		if(!context.second.getNode(XDAQContextTable::colContext_.colStatus_).getValue<bool>())
			continue;

		ConfigurationTree appLink = context.second.getNode(XDAQContextTable::colContext_.colLinkToApplicationTable_);
		if(appLink.isDisconnected())
			continue;

		auto appMap = appLink.getChildren();
		for(auto& app : appMap)
		{
			if(!app.second.getNode(XDAQContextTable::colApplication_.colStatus_).getValue<bool>())
				continue;

			std::string className = app.second.getNode(XDAQContextTable::colApplication_.colClass_).getValue<std::string>();
			if(className == XDAQContextTable::GATEWAY_SUPERVISOR_CLASS || className == XDAQContextTable::DEPRECATED_SUPERVISOR_CLASS)
				return context.first;  // return context UID
		}                              // end app search loop
	}                                  // end context search loop

	__SS__ << "Gateway Application search found no parent context!" << __E__;
	__SS_THROW__;
}  // end getContextOfGateway()
