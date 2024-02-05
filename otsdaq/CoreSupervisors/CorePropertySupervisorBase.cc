#include "otsdaq/CoreSupervisors/CorePropertySupervisorBase.h"
#include "otsdaq/MessageFacility/ITRACEController.h"
#include "otsdaq/MessageFacility/TRACEController.h"

using namespace ots;

const CorePropertySupervisorBase::SupervisorProperties CorePropertySupervisorBase::SUPERVISOR_PROPERTIES = CorePropertySupervisorBase::SupervisorProperties();

// clang-format off
//==============================================================================
CorePropertySupervisorBase::CorePropertySupervisorBase(xdaq::Application* application)
    : theConfigurationManager_(0)  // new ConfigurationManager)
    , supervisorClass_(application->getApplicationDescriptor()->getClassName())
    , supervisorClassNoNamespace_(
          supervisorClass_.substr(supervisorClass_.find_last_of(":") + 1, supervisorClass_.length() - supervisorClass_.find_last_of(":")))
    , supervisorContextUID_("UNINITIALIZED_supervisorContextUID")                // MUST BE INITIALIZED INSIDE THE CONTRUCTOR TO THROW EXCEPTIONS on bad conditions
    , supervisorApplicationUID_("UNINITIALIZED_supervisorApplicationUID")        // MUST BE INITIALIZED INSIDE THE CONTRUCTOR TO THROW EXCEPTIONS on bad conditions
    , supervisorConfigurationPath_("UNINITIALIZED_supervisorConfigurationPath")  // MUST BE INITIALIZED INSIDE THE CONTRUCTOR TO THROW EXCEPTIONS on bad conditions
    , propertiesAreSetup_(false)
	, theTRACEController_(nullptr)
{
	INIT_MF("." /*directory used is USER_DATA/LOG/.*/);



	__SUP_COUTV__(application->getApplicationContext()->getContextDescriptor()->getURL());
	__SUP_COUTV__(application->getApplicationDescriptor()->getLocalId());
	__SUP_COUTV__(supervisorClass_);
	__SUP_COUTV__(supervisorClassNoNamespace_);

	// get all supervisor info, and wiz mode, macroMaker mode, or not
	allSupervisorInfo_.init(application->getApplicationContext());

	if(allSupervisorInfo_.isMacroMakerMode())
	{
		theConfigurationManager_ = new ConfigurationManager(false /*initForWriteAccess*/, true /*initializeFromFhicl*/);
		__SUP_COUT__ << "Macro Maker mode detected. So skipping configuration location work for "
		                "supervisor of class '"
		             << supervisorClass_ << "'" << __E__;

		supervisorContextUID_        = "MacroMakerFEContext";
		supervisorApplicationUID_    = "MacroMakerFESupervisor";
		supervisorConfigurationPath_ = CorePropertySupervisorBase::supervisorContextUID_ + "/LinkToApplicationTable/" +
		                               CorePropertySupervisorBase::supervisorApplicationUID_ + "/LinkToSupervisorTable";

		__SUP_COUTV__(CorePropertySupervisorBase::supervisorContextUID_);
		__SUP_COUTV__(CorePropertySupervisorBase::supervisorApplicationUID_);
		__SUP_COUTV__(CorePropertySupervisorBase::supervisorConfigurationPath_);

		CorePropertySupervisorBase::indicateOtsAlive(0);

		return;
	}
	else if(allSupervisorInfo_.isWizardMode())
	{
		__SUP_COUT__ << "Wiz mode detected. So skipping configuration location work for "
		                "supervisor of class '"
		             << supervisorClass_ << "'" << __E__;
		supervisorContextUID_        = "NO CONTEXT ID IN WIZ MODE";
		supervisorApplicationUID_    = std::to_string(application->getApplicationDescriptor()->getLocalId());
		supervisorConfigurationPath_ = "NO APP PATH IN WIZ MODE";

		__SUP_COUTV__(CorePropertySupervisorBase::supervisorContextUID_);
		__SUP_COUTV__(CorePropertySupervisorBase::supervisorApplicationUID_);
		__SUP_COUTV__(CorePropertySupervisorBase::supervisorConfigurationPath_);

		return;
	}

	__SUP_COUT__ << "Getting configuration specific info for supervisor '" << (allSupervisorInfo_.getSupervisorInfo(application).getName()) << "' of class "
	             << supervisorClass_ << "." << __E__;

	// get configuration specific info for the application supervisor

	try
	{
		theConfigurationManager_ = new ConfigurationManager();
		CorePropertySupervisorBase::supervisorContextUID_ =
		    theConfigurationManager_->__GET_CONFIG__(XDAQContextTable)->getContextUID(application->getApplicationContext()->getContextDescriptor()->getURL());
	}
	catch(...)
	{
		__SUP_COUT_ERR__ << "XDAQ Supervisor could not access it's configuration through "
		                    "the Configuration Manager."
		                 << ". The getApplicationContext()->getContextDescriptor()->getURL() = "
		                 << application->getApplicationContext()->getContextDescriptor()->getURL() << __E__;
		throw;
	}

	try
	{
		CorePropertySupervisorBase::supervisorApplicationUID_ = theConfigurationManager_->__GET_CONFIG__(XDAQContextTable)
		                                                            ->getApplicationUID(application->getApplicationContext()->getContextDescriptor()->getURL(),
		                                                                                application->getApplicationDescriptor()->getLocalId());
	}
	catch(...)
	{
		__SUP_COUT_ERR__ << "XDAQ Supervisor could not access it's configuration through "
		                    "the Configuration Manager."
		                 << " The supervisorContextUID_ = " << supervisorContextUID_ << ". The supervisorApplicationUID = " << supervisorApplicationUID_
		                 << __E__;
		throw;
	}

	CorePropertySupervisorBase::supervisorConfigurationPath_ = "/" + CorePropertySupervisorBase::supervisorContextUID_ + "/LinkToApplicationTable/" +
	                                                           CorePropertySupervisorBase::supervisorApplicationUID_ + "/LinkToSupervisorTable";

	__SUP_COUTV__(CorePropertySupervisorBase::supervisorContextUID_);
	__SUP_COUTV__(CorePropertySupervisorBase::supervisorApplicationUID_);
	__SUP_COUTV__(CorePropertySupervisorBase::supervisorConfigurationPath_);

	//try to verify binding port for context was established
	//All this code failed to do the trick 
	// {
	// 			application->ptr_;
			
	// 			PeerTransportHTTP(this)
	// 			const xdaq::NetGroup* netGroupPtr = application->getApplicationContext()->getNetGroup();
	// 			auto netVector = netGroupPtr->getNetworks();
	// 			__SUP_COUTV__(netVector.size());
	// }

	CorePropertySupervisorBase::indicateOtsAlive(this);

	theConfigurationManager_->setOwnerContext(CorePropertySupervisorBase::supervisorContextUID_);
	theConfigurationManager_->setOwnerApp(CorePropertySupervisorBase::supervisorApplicationUID_);

}  // end constructor
// clang-format on

//==============================================================================
CorePropertySupervisorBase::~CorePropertySupervisorBase(void)
{
	__SUP_COUT__ << "Destructor." << __E__;
	if(theConfigurationManager_)
		delete theConfigurationManager_;

	if(theTRACEController_)
	{
		__SUP_COUT__ << "Destroying TRACE Controller..." << __E__;
		delete theTRACEController_;
		theTRACEController_ = nullptr;
	}
	__SUP_COUT__ << "Destructed." << __E__;
}  // end destructor

//==============================================================================
void CorePropertySupervisorBase::indicateOtsAlive(const CorePropertySupervisorBase* properties)
{
	char        portStr[100] = "0";
	std::string hostname     = "wiz";

	/* Note: the environment variable __ENV__("HOSTNAME")
	//	fails in multinode ots systems started through ssh
	//	because it will change meaning from host to host
	*/

	if(properties)
	{
		unsigned int port = properties->getContextTreeNode().getNode(properties->supervisorContextUID_).getNode("Port").getValue<unsigned int>();

		// help the user out if the config has old defaults for port/address
		// Same as XDAQContextTable_table.cc:extractContexts:L164
		if(port == 0)  // convert 0 to ${OTS_MAIN_PORT}
			port = atoi(__ENV__("OTS_MAIN_PORT"));

		sprintf(portStr, "%u", port);

		hostname = properties->getContextTreeNode().getNode(properties->supervisorContextUID_).getNode("Address").getValue<std::string>();
		if(hostname == "DEFAULT")  // convert DEFAULT to http://${HOSTNAME}
			hostname = "http://" + std::string(__ENV__("HOSTNAME"));

		size_t i = hostname.find("//");
		if(i != std::string::npos)
			hostname = hostname.substr(i + 2);

		__COUTV__(hostname);
	}

	// indicate ots is alive (for StartOTS.sh to verify launch was successful)
	std::string filename = std::string(__ENV__("OTSDAQ_LOG_DIR")) + "/otsdaq_is_alive-" + hostname + "-" + portStr + ".dat";
	FILE*       fp       = fopen(filename.c_str(), "w");
	if(!fp)
	{
		__SS__ << "Failed to open the ots-is-alive file: " << filename << __E__;
		__SS_THROW__;
	}
	fprintf(fp, "%s %s %ld\n", hostname.c_str(), portStr, time(0));
	fclose(fp);

	__COUT__ << "Marked alive: " << filename << __E__;
}  // end indicateOtsAlive()

//==============================================================================
// will be wizard supervisor in wiz mode, otherwise Gateway Supervisor descriptor
XDAQ_CONST_CALL xdaq::ApplicationDescriptor* CorePropertySupervisorBase::getGatewaySupervisorDescriptor(void)
{
	if(allSupervisorInfo_.isMacroMakerMode())
		return 0;

	return allSupervisorInfo_.isWizardMode() ? allSupervisorInfo_.getWizardDescriptor() : allSupervisorInfo_.getGatewayDescriptor();
}  // end getGatewaySupervisorDescriptor()

//==============================================================================
// When overriding, setup default property values here
// called by CorePropertySupervisorBase constructor before loading user defined property
// values
void CorePropertySupervisorBase::setSupervisorPropertyDefaults(void)
{
	// This can be done in the constructor because when you start xdaq it loads the
	// configuration that can't be changed while running!

	//__SUP_COUT__ << "Setting up Core Supervisor Base property defaults for supervisor"
	//<<
	//		"..." << __E__;

	// set core Supervisor base class defaults
	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.UserPermissionsThreshold, "*=1");
	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.UserGroupsAllowed, "");
	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.UserGroupsDisallowed, "");

	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.CheckUserLockRequestTypes, "");
	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.RequireUserLockRequestTypes, "");
	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.AutomatedRequestTypes, "");
	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.AllowNoLoginRequestTypes, "");
	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.RequireSecurityRequestTypes, "");

	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.NoXmlWhiteSpaceRequestTypes, "");
	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.NonXMLRequestTypes, "");

	//	__SUP_COUT__ << "Done setting up Core Supervisor Base property defaults for
	// supervisor" <<
	//			"..." << __E__;
}  // end setSupervisorPropertyDefaults()

//==============================================================================
// extractPermissionsMapFromString
//	Static function that extract map function to standardize approach
//		in case needed by supervisors for special permissions handling.
//	For example, used to serve Desktop Icons.
//
//	permissionsString format is as follows:
//		<groupName>:<permissionsThreshold> pairs separated by ',' '&' or '|'
//		for example, to give access admins and pixel team but not calorimeter team:
//			allUsers:255 | pixelTeam:1 | calorimeterTeam:0
//
//	Note: WebUsers::DEFAULT_USER_GROUP = allUsers
//
//	Use with CorePropertySupervisorBase::doPermissionsGrantAccess to determine
//		if access is allowed.
void CorePropertySupervisorBase::extractPermissionsMapFromString(const std::string&                                  permissionsString,
                                                                 std::map<std::string, WebUsers::permissionLevel_t>& permissionsMap)
{
	permissionsMap.clear();
	StringMacros::getMapFromString(permissionsString, permissionsMap);
	if(permissionsMap.size() == 0)  // do not allow empty permissions map
		permissionsMap.emplace(std::pair<std::string, WebUsers::permissionLevel_t>(WebUsers::DEFAULT_USER_GROUP,
		                                                                           atoi(permissionsString.c_str()))  // convert to integer
		);
}  // end extractPermissionsMapFromString()

//==============================================================================
// doPermissionsGrantAccess
//	Static function that checks permissionLevelsMap against permissionThresholdsMap and
// returns true if 	access requirements are met.
//
//	This is useful in standardizing approach for supervisors in case of
//		of special permissions handling.
//	For example, used to serve Desktop Icons.
//
//	permissionLevelsString format is as follows:
//		<groupName>:<permissionsLevel> pairs separated by ',' '&' or '|'
//		for example, to be a standard user and an admin on the pixel team and no access to
// calorimeter team: 			allUsers:1 | pixelTeam:255 | calorimeterTeam:0
//
//	permissionThresoldsString format is as follows:
//		<groupName>:<permissionsThreshold> pairs separated by ',' '&' or '|'
//		for example, to give access admins and pixel team but not calorimeter team:
//			allUsers:255 | pixelTeam:1 | calorimeterTeam:0
bool CorePropertySupervisorBase::doPermissionsGrantAccess(std::map<std::string, WebUsers::permissionLevel_t>& permissionLevelsMap,
                                                          std::map<std::string, WebUsers::permissionLevel_t>& permissionThresholdsMap)
{
	// return true if a permission level group name is found with a permission level
	//	greater than or equal to the permission level at a matching group name entry in
	// the thresholds map.

	//__COUTV__(StringMacros::mapToString(permissionLevelsMap));
	//__COUTV__(StringMacros::mapToString(permissionThresholdsMap));

	for(const auto& permissionLevelGroupPair : permissionLevelsMap)
	{
		//__COUTV__(permissionLevelGroupPair.first);
		//__COUTV__(permissionLevelGroupPair.second);

		for(const auto& permissionThresholdGroupPair : permissionThresholdsMap)
		{
			//__COUTV__(permissionThresholdGroupPair.first);
			//__COUTV__(permissionThresholdGroupPair.second);
			if(permissionLevelGroupPair.first == permissionThresholdGroupPair.first && permissionThresholdGroupPair.second &&  // not explicitly disallowed
			   permissionLevelGroupPair.second >= permissionThresholdGroupPair.second)
				return true;  // access granted!
		}
	}
	//__COUT__ << "Denied." << __E__;

	// if here, no access group match found
	// so denied
	return false;
}  // end doPermissionsGrantAccess()

//==============================================================================
void CorePropertySupervisorBase::checkSupervisorPropertySetup()
{
	if(propertiesAreSetup_)
		return;

	// Immediately mark properties as setup, (prevent infinite loops due to
	//	other launches from within this function, e.g. from getSupervisorProperty)
	//	only redo if Context configuration group changes
	propertiesAreSetup_ = true;


	CorePropertySupervisorBase::setSupervisorPropertyDefaults();  // calls base class
	                                                              // version defaults

	//__SUP_COUT__ << "Setting up supervisor specific property DEFAULTS for supervisor..."
	//<< __E__;
	setSupervisorPropertyDefaults();  // calls override version defaults
	                                  //	__SUP_COUT__ << "Done setting up supervisor
	                                  // specific property DEFAULTS for supervisor" <<
	                                  //			"." << __E__;

	if(allSupervisorInfo_.isWizardMode())
		__SUP_COUT__ << "Wiz mode detected. Skipping setup of supervisor properties for "
		                "supervisor of class '"
		             << supervisorClass_ << "'" << __E__;
	else if(allSupervisorInfo_.isMacroMakerMode())
		__SUP_COUT__ << "Maker Maker mode detected. Skipping setup of supervisor properties for "
		                "supervisor of class '"
		             << supervisorClass_ << "'" << __E__;
	else
		CorePropertySupervisorBase::loadUserSupervisorProperties();  // loads user
		                                                             // settings from
		                                                             // configuration


	readOnly_        		= getSupervisorProperty("ReadOnly","0") == "1"?true:false;
    __SUP_COUTV__(readOnly_);
	
	//__SUP_COUT__ << "Setting up supervisor specific FORCED properties for supervisor..."
	//<< __E__;
	forceSupervisorPropertyValues();  // calls override forced values
	                                  //	__SUP_COUT__ << "Done setting up supervisor
	                                  // specific FORCED properties for supervisor" <<
	                                  //			"." << __E__;

	CorePropertySupervisorBase::extractPermissionsMapFromString(
	    getSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.UserPermissionsThreshold), propertyStruct_.UserPermissionsThreshold);

	propertyStruct_.UserGroupsAllowed.clear();
	StringMacros::getMapFromString(getSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.UserGroupsAllowed),
	                               propertyStruct_.UserGroupsAllowed);

	propertyStruct_.UserGroupsDisallowed.clear();
	StringMacros::getMapFromString(getSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.UserGroupsDisallowed),
	                               propertyStruct_.UserGroupsDisallowed);

	auto nameIt = SUPERVISOR_PROPERTIES.allSetNames_.begin();
	auto setIt  = propertyStruct_.allSets_.begin();
	while(nameIt != SUPERVISOR_PROPERTIES.allSetNames_.end() && setIt != propertyStruct_.allSets_.end())
	{
		(*setIt)->clear();
		StringMacros::getSetFromString(getSupervisorProperty(*(*nameIt)), *(*setIt));

		++nameIt;
		++setIt;
	}

	__SUP_COUT__ << "Final supervisor property settings:" << __E__;
	for(auto& property : propertyMap_)
		__SUP_COUT__ << "\t" << property.first << " = " << property.second << __E__;
}  // end checkSupervisorPropertySetup()

//==============================================================================
// getSupervisorTreeNode ~
//	try to get this Supervisors configuration tree node
ConfigurationTree CorePropertySupervisorBase::getSupervisorTreeNode(void)
try
{
	if(supervisorContextUID_ == "" || supervisorApplicationUID_ == "")
	{
		__SUP_SS__ << "Empty supervisorContextUID_ or supervisorApplicationUID_." << __E__;
		__SUP_SS_THROW__;
	}
	return theConfigurationManager_->getSupervisorNode(supervisorContextUID_, supervisorApplicationUID_);
}  // end getSupervisorTreeNode()
catch(...)
{
	__SUP_COUT_ERR__ << "XDAQ Supervisor could not access it's configuration node through "
	                    "theConfigurationManager_ "
	                 << "(Did you remember to initialize using CorePropertySupervisorBase::init()?)."
	                 << " The supervisorContextUID_ = " << supervisorContextUID_ << ". The supervisorApplicationUID = " << supervisorApplicationUID_ << __E__;
	throw;
}  // end getSupervisorTreeNode() exception handling

//==============================================================================
// loadUserSupervisorProperties ~
//	try to get user supervisor properties
void CorePropertySupervisorBase::loadUserSupervisorProperties(void)
{
	//	__SUP_COUT__ << "Loading user properties for supervisor '" <<
	//			supervisorContextUID_ << "/" << supervisorApplicationUID_ <<
	//			"'..." << __E__;

	// re-acquire the configuration supervisor node, in case the config has changed
	auto supervisorNode = CorePropertySupervisorBase::getSupervisorTreeNode();

	try
	{
		auto /*map<name,node>*/ children = supervisorNode.getNode("LinkToPropertyTable").getChildren();

		for(auto& child : children)
		{
			if(child.second.getNode("Status").getValue<bool>() == false)
				continue;  // skip OFF properties

			auto propertyName = child.second.getNode("PropertyName").getValue();
			setSupervisorProperty(propertyName, child.second.getNode("PropertyValue").getValue<std::string>());
		}
	}
	catch(...)
	{
		__SUP_COUT__ << "No user supervisor property settings found in the configuration "
		                "tree, going with the defaults."
		             << __E__;
	}

	//	__SUP_COUT__ << "Done loading user properties for supervisor '" <<
	//			supervisorContextUID_ << "/" << supervisorApplicationUID_ <<
	//			"'" << __E__;
}  // end loadUserSupervisorProperties()

//==============================================================================
void CorePropertySupervisorBase::setSupervisorProperty(const std::string& propertyName, const std::string& propertyValue)
{
	propertyMap_[propertyName] = propertyValue;
	//	__SUP_COUT__ << "Set propertyMap_[" << propertyName <<
	//			"] = " << propertyMap_[propertyName] << __E__;
}  // end setSupervisorProperty()

//==============================================================================
void CorePropertySupervisorBase::addSupervisorProperty(const std::string& propertyName, const std::string& propertyValue)
{
	propertyMap_[propertyName] = propertyValue + " | " + getSupervisorProperty(propertyName);
	//	__SUP_COUT__ << "Set propertyMap_[" << propertyName <<
	//			"] = " << propertyMap_[propertyName] << __E__;
}  // end addSupervisorProperty()

//==============================================================================
// getSupervisorProperty
//		string version of template function
std::string CorePropertySupervisorBase::getSupervisorProperty(const std::string& propertyName)
{
	// check if need to setup properties
	checkSupervisorPropertySetup();

	auto it = propertyMap_.find(propertyName);
	if(it == propertyMap_.end())
	{
		__SUP_SS__ << "Could not find property named " << propertyName << __E__;
		__SS_THROW__;  //__SUP_SS_THROW__;
	}
	return StringMacros::validateValueForDefaultStringDataType(it->second);
}  // end getSupervisorProperty()

//==============================================================================
// getSupervisorProperty
std::string CorePropertySupervisorBase::getSupervisorProperty(const std::string& propertyName, const std::string& defaultValue)
{
	// check if need to setup properties
	checkSupervisorPropertySetup();

	auto it = propertyMap_.find(propertyName);
	if(it == propertyMap_.end())
	{
		// not found, so returning default value
		return defaultValue;
	}
	return StringMacros::validateValueForDefaultStringDataType(it->second);
}  // end getSupervisorProperty()

//==============================================================================
// getSupervisorPropertyUserPermissionsThreshold
//	returns the threshold based on the requestType
WebUsers::permissionLevel_t CorePropertySupervisorBase::getSupervisorPropertyUserPermissionsThreshold(const std::string& requestType)
{
	// check if need to setup properties
	checkSupervisorPropertySetup();

	return StringMacros::getWildCardMatchFromMap(requestType, propertyStruct_.UserPermissionsThreshold);

	//	auto it = propertyStruct_.UserPermissionsThreshold.find(requestType);
	//	if(it == propertyStruct_.UserPermissionsThreshold.end())
	//	{
	//		__SUP_SS__ << "Could not find requestType named " << requestType << " in
	// UserPermissionsThreshold map." << __E__;
	//		__SS_THROW__; //__SUP_SS_THROW__;
	//	}
	//	return it->second;
}  // end getSupervisorPropertyUserPermissionsThreshold()

//==============================================================================
// getRequestUserInfo ~
//	extract user info for request based on property configuration
void CorePropertySupervisorBase::getRequestUserInfo(WebUsers::RequestUserInfo& userInfo)
{
	checkSupervisorPropertySetup();

	//__SUP_COUT__ << "userInfo.requestType_ " << userInfo.requestType_ << __E__;

	userInfo.automatedCommand_ = StringMacros::inWildCardSet(userInfo.requestType_,
	                                                         propertyStruct_.AutomatedRequestTypes);  // automatic commands should not refresh
	                                                                                                  // cookie code.. only user initiated
	                                                                                                  // commands should!
	userInfo.NonXMLRequestType_ = StringMacros::inWildCardSet(userInfo.requestType_, propertyStruct_.NonXMLRequestTypes);  // non-xml request
	                                                                                                                       // types just return
	                                                                                                                       // the request return
	                                                                                                                       // string to client
	userInfo.NoXmlWhiteSpace_ = StringMacros::inWildCardSet(userInfo.requestType_, propertyStruct_.NoXmlWhiteSpaceRequestTypes);

	//**** start LOGIN GATEWAY CODE ***//
	// check cookieCode, sequence, userWithLock, and permissions access all in one shot!
	{
		userInfo.checkLock_       = StringMacros::inWildCardSet(userInfo.requestType_, propertyStruct_.CheckUserLockRequestTypes);
		userInfo.requireLock_     = StringMacros::inWildCardSet(userInfo.requestType_, propertyStruct_.RequireUserLockRequestTypes);
		userInfo.allowNoUser_     = StringMacros::inWildCardSet(userInfo.requestType_, propertyStruct_.AllowNoLoginRequestTypes);
		userInfo.requireSecurity_ = StringMacros::inWildCardSet(userInfo.requestType_, propertyStruct_.RequireSecurityRequestTypes);

		userInfo.permissionsThreshold_ = -1;  // default to max
		try
		{
			userInfo.permissionsThreshold_ = CorePropertySupervisorBase::getSupervisorPropertyUserPermissionsThreshold(userInfo.requestType_);
		}
		catch(std::runtime_error& e)
		{
			if(!userInfo.automatedCommand_)
				__SUP_COUT__ << "No explicit permissions threshold for request '" << userInfo.requestType_
				             << "'... Defaulting to max threshold = " << (unsigned int)userInfo.permissionsThreshold_ << __E__;
		}

		// __COUTV__(userInfo.requestType_);
		// __COUTV__(userInfo.checkLock_);
		// __COUTV__(userInfo.requireLock_);
		// __COUTV__(userInfo.allowNoUser_);
		// __COUTV__((unsigned int)userInfo.permissionsThreshold_);

		try
		{
			StringMacros::getSetFromString(StringMacros::getWildCardMatchFromMap(userInfo.requestType_, propertyStruct_.UserGroupsAllowed),
			                               userInfo.groupsAllowed_);
		}
		catch(std::runtime_error& e)
		{
			userInfo.groupsAllowed_.clear();

			//			if(!userInfo.automatedCommand_)
			//				__SUP_COUT__ << "No explicit groups allowed for request '" <<
			//					 userInfo.requestType_ << "'... Defaulting to empty groups
			// allowed. " << __E__;
		}
		try
		{
			StringMacros::getSetFromString(StringMacros::getWildCardMatchFromMap(userInfo.requestType_, propertyStruct_.UserGroupsDisallowed),
			                               userInfo.groupsDisallowed_);
		}
		catch(std::runtime_error& e)
		{
			userInfo.groupsDisallowed_.clear();

			//			if(!userInfo.automatedCommand_)
			//				__SUP_COUT__ << "No explicit groups disallowed for request '"
			//<<
			//					 userInfo.requestType_ << "'... Defaulting to empty groups
			// disallowed. " << __E__;
		}
	}  //**** end LOGIN GATEWAY CODE ***//

	// completed user info, for the request type, is returned to caller
}  // end getRequestUserInfo()

//==============================================================================
xoap::MessageReference CorePropertySupervisorBase::TRACESupervisorRequest(xoap::MessageReference message)
{
	__SUP_COUT__ << "$$$$$$$$$$$$$$$$$" << __E__;

	// receive request parameters
	SOAPParameters parameters;
	parameters.addParameter("Request");

	__SUP_COUT__ << "Received TRACE message: " << SOAPUtilities::translate(message) << __E__;

	SOAPUtilities::receive(message, parameters);
	std::string request = parameters.getValue("Request");

	__SUP_COUT__ << "request: " << request << __E__;

	// request types:
	//	GetTraceLevels
	//	SetTraceLevels

	SOAPParameters retParameters;
	try
	{
		if(request == "GetTraceLevels")
		{
			retParameters.addParameter("TRACEList", getTraceLevels());
			retParameters.addParameter("TRACEHostnameList", traceReturnHostString_);
			return SOAPUtilities::makeSOAPMessageReference(supervisorClassNoNamespace_ + "Response", retParameters);
		}
		else if(request == "SetTraceLevels")
		{
			parameters.addParameter("IndividualValues");
			parameters.addParameter("Host");
			parameters.addParameter("SetMode");
			parameters.addParameter("Labels");
			parameters.addParameter("SetValueMSB");
			parameters.addParameter("SetValueLSB");
			SOAPUtilities::receive(message, parameters);

			int         individualValues = parameters.getValueAsInt("IndividualValues");
			std::string host             = parameters.getValue("Host");
			std::string setMode          = parameters.getValue("SetMode");
			std::string labelsStr        = parameters.getValue("Labels");
			int         setValueMSB      = parameters.getValueAsInt("SetValueMSB");
			int         setValueLSB      = parameters.getValueAsInt("SetValueLSB");
			__SUP_COUTV__(individualValues);
			__SUP_COUTV__(host);
			__SUP_COUTV__(setMode);
			__SUP_COUTV__(setValueMSB);
			__SUP_COUTV__(setValueLSB);
			__SUP_COUTV__(labelsStr);

			if(individualValues)
				retParameters.addParameter("TRACEList", setIndividualTraceLevels(host, setMode, labelsStr));
			else
				retParameters.addParameter("TRACEList", setTraceLevels(host, setMode, labelsStr, setValueMSB, setValueLSB));
			return SOAPUtilities::makeSOAPMessageReference(supervisorClassNoNamespace_ + "Response", retParameters);
		}
		else if(request == "GetTriggerStatus")
		{
			retParameters.addParameter("TRACETriggerStatus", getTraceTriggerStatus());
			return SOAPUtilities::makeSOAPMessageReference(supervisorClassNoNamespace_ + "Response", retParameters);
		}
		else if(request == "SetTriggerEnable")
		{
			parameters.addParameter("Host");
			parameters.addParameter("EntriesAfterTrigger");
			SOAPUtilities::receive(message, parameters);

			std::string host                = parameters.getValue("Host");
			int         entriesAfterTrigger = parameters.getValueAsInt("EntriesAfterTrigger");
			__SUP_COUTV__(host);
			__SUP_COUTV__(entriesAfterTrigger);
			retParameters.addParameter("TRACETriggerStatus", setTraceTriggerEnable(host, entriesAfterTrigger));
			return SOAPUtilities::makeSOAPMessageReference(supervisorClassNoNamespace_ + "Response", retParameters);
		}
		else if(request == "ResetTRACE")
		{
			parameters.addParameter("Host");
			SOAPUtilities::receive(message, parameters);

			std::string host = parameters.getValue("Host");
			__SUP_COUTV__(host);
			retParameters.addParameter("TRACETriggerStatus", resetTRACE(host));
			return SOAPUtilities::makeSOAPMessageReference(supervisorClassNoNamespace_ + "Response", retParameters);
		}
		else if(request == "EnableTRACE")
		{
			parameters.addParameter("Host");
			parameters.addParameter("SetEnable");
			SOAPUtilities::receive(message, parameters);

			std::string host   = parameters.getValue("Host");
			bool        enable = parameters.getValueAsInt("SetEnable") ? true : false;
			__SUP_COUTV__(host);
			__SUP_COUTV__(enable);

			retParameters.addParameter("TRACETriggerStatus", enableTRACE(host, enable));
			return SOAPUtilities::makeSOAPMessageReference(supervisorClassNoNamespace_ + "Response", retParameters);
		}
		else if(request == "GetSnapshot")
		{
			parameters.addParameter("Host");
			parameters.addParameter("FilterForCSV");
			parameters.addParameter("FilterOutCSV");
			SOAPUtilities::receive(message, parameters);

			std::string host      = parameters.getValue("Host");
			std::string filterFor = parameters.getValue("FilterForCSV");
			std::string filterOut = parameters.getValue("FilterOutCSV");
			__SUP_COUTV__(host);
			__SUP_COUTV__(filterFor);
			__SUP_COUTV__(filterOut);
			retParameters.addParameter("TRACESnapshot", getTraceSnapshot(host, filterFor, filterOut));
			retParameters.addParameter("TRACETriggerStatus", getTraceTriggerStatus());
			return SOAPUtilities::makeSOAPMessageReference(supervisorClassNoNamespace_ + "Response", retParameters);
		}
		else
		{
			__SUP_SS__ << "Unrecognized request received! '" << request << "'" << __E__;
			__SUP_SS_THROW__;
		}
	}
	catch(const std::runtime_error& e)
	{
		__SUP_SS__ << "Error occurred handling request: " << e.what() << __E__;
		__SUP_COUT_ERR__ << ss.str();
		retParameters.addParameter("Error", ss.str());
	}
	catch(...)
	{
		__SUP_SS__ << "Error occurred handling request." << __E__;
		try	{ throw; } //one more try to printout extra info
		catch(const std::exception &e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...){}
		__SUP_COUT_ERR__ << ss.str();
		retParameters.addParameter("Error", ss.str());
	}

	return SOAPUtilities::makeSOAPMessageReference("TRACEFault", retParameters);

}  // end TRACESupervisorRequest()

//==============================================================================
const std::string& CorePropertySupervisorBase::getTraceLevels()
{
	__SUP_COUT__ << "getTraceLevels()" << __E__;

	traceReturnString_     = "";  // reset;
	traceReturnHostString_ = "";  // reset;

	if(!theTRACEController_)
	{
		__SUP_COUT__ << "No TRACE Controller found, constructing!" << __E__;
		theTRACEController_ = new TRACEController();
	}

	// typedef std::unordered_map<std::string, TraceLevelMap> HostTraceLevelMap =
	ITRACEController::HostTraceLevelMap traceHostMap = theTRACEController_->getTraceLevels();
	for(const auto& traceMap : traceHostMap)
	{
		//__COUTV__(traceMap.first);

		// NOTE: TRACE hostname resolution is not necessarily the same as xdaq context name resolution
		//  so return TRACE hostname resolution so a map can be generated at the controller

		traceReturnHostString_ = ";" + traceMap.first;
		traceReturnString_ += ";" + traceMap.first;

		for(const auto& traceMask : traceMap.second)
		{
			//__COUTV__(traceMask.first);
			//__COUTV__(traceMask.second);
			// give in 32b chunks since javascript is 32-bit
			traceReturnString_ += "," + traceMask.first + ",M:" + std::to_string((unsigned int)(traceMask.second.M >> 32)) + ":" +
			                      std::to_string((unsigned int)traceMask.second.M) + ":S:" + std::to_string((unsigned int)(traceMask.second.S >> 32)) + ":" +
			                      std::to_string((unsigned int)traceMask.second.S) + ":T:" + std::to_string((unsigned int)(traceMask.second.T >> 32)) + ":" +
			                      std::to_string((unsigned int)traceMask.second.T);
		}  // end label loop
	}      // end host loop
	__SUP_COUT__ << "end getTraceLevels()" << __E__;
	return traceReturnString_;
}  // end getTraceLevels()

//==============================================================================
const std::string& CorePropertySupervisorBase::setTraceLevels(
    std::string const& host, std::string const& mode, std::string const& labelsStr, uint32_t setValueMSB, uint32_t setValueLSB)
{
	__SUP_COUT__ << "setTraceLevels()" << __E__;

	if(!theTRACEController_)
	{
		__SUP_COUT__ << "No TRACE Controller found, constructing!" << __E__;
		theTRACEController_ = new TRACEController();
	}

	ITRACEController::TraceMasks setMask;
	bool                         allMode = mode == "ALL";
	if(allMode || mode == "FAST")
		setMask.M = ((uint64_t(setValueMSB)) << 32) | (uint64_t(uint32_t(setValueLSB)));
	if(allMode || mode == "SLOW")
		setMask.S = ((uint64_t(setValueMSB)) << 32) | (uint64_t(uint32_t(setValueLSB)));
	if(allMode || mode == "TRIGGER")
		setMask.T = ((uint64_t(setValueMSB)) << 32) | (uint64_t(uint32_t(setValueLSB)));

	std::vector<std::string /*labels*/> labels;
	StringMacros::getVectorFromString(labelsStr, labels, {','});
	for(const auto& label : labels)
	{
		__SUP_COUTV__(label);
		theTRACEController_->setTraceLevelMask(label, setMask, host, mode);
	}

	__SUP_COUT__ << "end setTraceLevels()" << __E__;
	return getTraceLevels();
}  // end setTraceLevels()

//==============================================================================
const std::string& CorePropertySupervisorBase::setIndividualTraceLevels(std::string const& host, std::string const& mode, std::string const& labelValuesStr)
{
	__SUP_COUT__ << "setIndividualTraceLevels()" << __E__;

	if(!theTRACEController_)
	{
		__SUP_COUT__ << "No TRACE Controller found, constructing!" << __E__;
		theTRACEController_ = new TRACEController();
	}

	ITRACEController::TraceMasks setMask;
	bool                         allMode     = mode == "ALL";
	bool                         fastMode    = mode == "FAST";
	bool                         slowMode    = mode == "SLOW";
	bool                         triggerMode = mode == "TRIGGER";

	std::vector<std::string /*labels,msb,lsb*/> labelValues;
	StringMacros::getVectorFromString(labelValuesStr, labelValues, {','});
	for(unsigned int i = 0; i < labelValues.size(); i += 3 /*label,msb,lsb*/)
	{
		__SUP_COUT__ << "Label = " << labelValues[i] << " msb/lsb " << labelValues[i + 1] << "/" << labelValues[i + 2] << __E__;

		if(allMode || fastMode)
			setMask.M = ((uint64_t(atoi(labelValues[i + 1].c_str()))) << 32) | (uint64_t(uint32_t(atoi(labelValues[i + 2].c_str()))));
		if(allMode || slowMode)
			setMask.S = ((uint64_t(atoi(labelValues[i + 1].c_str()))) << 32) | (uint64_t(uint32_t(atoi(labelValues[i + 2].c_str()))));
		if(allMode || triggerMode)
			setMask.T = ((uint64_t(atoi(labelValues[i + 1].c_str()))) << 32) | (uint64_t(uint32_t(atoi(labelValues[i + 2].c_str()))));

		theTRACEController_->setTraceLevelMask(labelValues[i], setMask, host, mode);
	}

	__SUP_COUT__ << "end setIndividualTraceLevels()" << __E__;
	return getTraceLevels();
}  // end setTraceLevels()

//==============================================================================
const std::string& CorePropertySupervisorBase::getTraceTriggerStatus()
{
	__SUP_COUT__ << "getTraceTriggerStatus()" << __E__;

	traceReturnString_ = "";  // reset;

	if(!theTRACEController_)
	{
		__SUP_COUT__ << "No TRACE Controller found, constructing!" << __E__;
		theTRACEController_ = new TRACEController();
	}

	bool isTriggered = theTRACEController_->getIsTriggered();
	traceReturnString_ += ";" + theTRACEController_->getHostnameString() + "," + (isTriggered ? "1" : "0");

	__SUP_COUT__ << "end getTraceTriggerStatus() " << traceReturnString_ << __E__;
	return traceReturnString_;
}  // end getTraceTriggerStatus()

//==============================================================================
const std::string& CorePropertySupervisorBase::setTraceTriggerEnable(std::string const& host, size_t entriesAfterTrigger)
{
	__SUP_COUT__ << "setTraceTriggerEnable() " << host << __E__;

	if(!theTRACEController_)
	{
		__SUP_COUT__ << "No TRACE Controller found, constructing!" << __E__;
		theTRACEController_ = new TRACEController();
	}
	theTRACEController_->setTriggerEnable(entriesAfterTrigger);
	__SUP_COUT__ << "end setTraceTriggerEnable()" << __E__;
	return getTraceTriggerStatus();
}  // end setTraceTriggerEnable()

//==============================================================================
const std::string& CorePropertySupervisorBase::resetTRACE(std::string const& host)
{
	__SUP_COUT__ << "resetTRACE() " << host << __E__;

	if(!theTRACEController_)
	{
		__SUP_COUT__ << "No TRACE Controller found, constructing!" << __E__;
		theTRACEController_ = new TRACEController();
	}
	theTRACEController_->resetTraceBuffer();
	theTRACEController_->enableTrace();
	__SUP_COUT__ << "end resetTRACE()" << __E__;
	return getTraceTriggerStatus();
}  // end resetTRACE()

//==============================================================================
const std::string& CorePropertySupervisorBase::enableTRACE(std::string const& host, bool enable)
{
	__SUP_COUT__ << "enableTRACE() " << host << " " << enable << __E__;

	if(!theTRACEController_)
	{
		__SUP_COUT__ << "No TRACE Controller found, constructing!" << __E__;
		theTRACEController_ = new TRACEController();
	}
	theTRACEController_->enableTrace(enable);
	__SUP_COUT__ << "end enableTRACE()" << __E__;
	return getTraceTriggerStatus();
}  // end enableTRACE()

//==============================================================================
const std::string& CorePropertySupervisorBase::getTraceSnapshot(std::string const& host, std::string const& filterFor, std::string const& filterOut)
{
	__SUP_COUT__ << "getTraceSnapshot()" << host << __E__;

	traceReturnString_ = "";  // reset;

	if(!theTRACEController_)
	{
		__SUP_COUT__ << "No TRACE Controller found, constructing!" << __E__;
		theTRACEController_ = new TRACEController();
	}

	traceReturnString_ = theTRACEController_->getTraceBufferDump(filterFor, filterOut);
	// std::cout << traceReturnString_ << __E__;

	const size_t MAX_SZ = 200000;
	if(traceReturnString_.size() > MAX_SZ)
	{
		__SUP_COUT__ << "Truncating from " << traceReturnString_.size() << " to " << MAX_SZ << __E__;
		traceReturnString_.resize(MAX_SZ);
		traceReturnString_ += "\n...TRUNCATED";
	}
	else if(traceReturnString_.size() == 0)
	{
		__SUP_COUT__ << "Empty snapshot" << __E__;
		traceReturnString_ = "Empty TRACE snapshot.";
	}
	__SUP_COUT__ << "end getTraceSnapshot() Bytes = " << traceReturnString_.size() << __E__;
	return traceReturnString_;
}  // end getTraceSnapshot()
