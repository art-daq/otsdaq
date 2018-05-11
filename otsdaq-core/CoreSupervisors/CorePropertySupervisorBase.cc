#include "otsdaq-core/CoreSupervisors/CorePropertySupervisorBase.h"


//#include <iostream>

using namespace ots;

const CorePropertySupervisorBase::SupervisorProperties 	CorePropertySupervisorBase::SUPERVISOR_PROPERTIES = CorePropertySupervisorBase::SupervisorProperties();

////========================================================================================================================
//void CorePropertySupervisorBase::init(
//		const std::string& supervisorContextUID,
//		const std::string& supervisorApplicationUID,
//		ConfigurationManager *theConfigurationManager)
//{
//	__SUP_COUT__ << "Begin!" << std::endl;
//
//
//	__SUP_COUT__ << "Looking for " <<
//				supervisorContextUID << "/" << supervisorApplicationUID <<
//				" supervisor node..." << __E__;
//
//	//test the supervisor UIDs at init
//	auto supervisorNode = CorePropertySupervisorBase::getSupervisorTreeNode();
//
//	supervisorConfigurationPath_  = "/" + supervisorContextUID + "/LinkToApplicationConfiguration/" +
//			supervisorApplicationUID + "/LinkToSupervisorConfiguration";
//
//}

//========================================================================================================================
CorePropertySupervisorBase::CorePropertySupervisorBase(xdaq::Application* application)
: theConfigurationManager_      (new ConfigurationManager)
, theContextTreeNode_ 			(theConfigurationManager_->getNode(theConfigurationManager_->__GET_CONFIG__(XDAQContextConfiguration)->getConfigurationName()))
, supervisorClass_              (application->getApplicationDescriptor()->getClassName())
, supervisorClassNoNamespace_   (supervisorClass_.substr(supervisorClass_.find_last_of(":")+1, supervisorClass_.length()-supervisorClass_.find_last_of(":")))
, supervisorContextUID_         ("MUST BE INITIALIZED INSIDE THE CONTRUCTOR TO THROW EXCEPTIONS")
, supervisorApplicationUID_     ("MUST BE INITIALIZED INSIDE THE CONTRUCTOR TO THROW EXCEPTIONS")
, supervisorConfigurationPath_  ("MUST BE INITIALIZED INSIDE THE CONTRUCTOR TO THROW EXCEPTIONS")
, propertiesAreSetup_			(false)
{
	INIT_MF("CorePropertySupervisorBase");

	__SUP_COUTV__(application->getApplicationContext()->getContextDescriptor()->getURL());
	__SUP_COUTV__(application->getApplicationDescriptor()->getLocalId());
	__SUP_COUTV__(supervisorClass_);
	__SUP_COUTV__(supervisorClassNoNamespace_);

	//get all supervisor info, and wiz mode or not
	allSupervisorInfo_.init(application->getApplicationContext());

	if(allSupervisorInfo_.isWizardMode())
	{
		__SUP_COUT__ << "Wiz mode detected. So skipping configuration location work for supervisor of class '" <<
				supervisorClass_ << "'" << __E__;

		return;
	}

	__SUP_COUT__ << "Getting configuration specific info for supervisor '" <<
			(allSupervisorInfo_.getSupervisorInfo(application).getName()) <<
			"' of class " << supervisorClass_ << "." << __E__;

	//get configuration specific info for the application supervisor

	try
	{
		CorePropertySupervisorBase::supervisorContextUID_ =
				theConfigurationManager_->__GET_CONFIG__(XDAQContextConfiguration)->getContextUID(
						application->getApplicationContext()->getContextDescriptor()->getURL());
	}
	catch(...)
	{
		__SUP_COUT_ERR__ << "XDAQ Supervisor could not access it's configuration through the theConfigurationManager_." <<
				//" The XDAQContextConfigurationName = " << XDAQContextConfigurationName_ <<
				". The getApplicationContext()->getContextDescriptor()->getURL() = " <<
				application->getApplicationContext()->getContextDescriptor()->getURL() << std::endl;
		throw;
	}

	try
	{
		CorePropertySupervisorBase::supervisorApplicationUID_ =
				theConfigurationManager_->__GET_CONFIG__(XDAQContextConfiguration)->getApplicationUID
				(
						application->getApplicationContext()->getContextDescriptor()->getURL(),
						application->getApplicationDescriptor()->getLocalId()
				);
	}
	catch(...)
	{
		__SUP_COUT_ERR__ << "XDAQ Supervisor could not access it's configuration through the theConfigurationManager_." <<
				" The supervisorContextUID_ = " << supervisorContextUID_ <<
				". The supervisorApplicationUID = " << supervisorApplicationUID_ << std::endl;
		throw;
	}

	CorePropertySupervisorBase::supervisorConfigurationPath_  = "/" +
			CorePropertySupervisorBase::supervisorContextUID_ + "/LinkToApplicationConfiguration/" +
			CorePropertySupervisorBase::supervisorApplicationUID_ + "/LinkToSupervisorConfiguration";

	__SUP_COUTV__(CorePropertySupervisorBase::supervisorContextUID_);
	__SUP_COUTV__(CorePropertySupervisorBase::supervisorApplicationUID_);
	__SUP_COUTV__(CorePropertySupervisorBase::supervisorConfigurationPath_);
}


//========================================================================================================================
CorePropertySupervisorBase::~CorePropertySupervisorBase(void)
{
}


//========================================================================================================================
//When overriding, setup default property values here
// called by CorePropertySupervisorBase constructor before loading user defined property values
void CorePropertySupervisorBase::setSupervisorPropertyDefaults(void)
{
	//This can be done in the constructor because when you start xdaq it loads the configuration that can't be changed while running!

	__SUP_COUT__ << "Setting up Core Supervisor Base property defaults for supervisor" <<
			"..." << __E__;

	//set core Supervisor base class defaults
	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.UserPermissionsThreshold,		"*=1");
	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.UserGroupsAllowed,				"");
	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.UserGroupsDisallowed,			"");

	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.CheckUserLockRequestTypes,		"");
	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.RequireUserLockRequestTypes,	"");
	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.AutomatedRequestTypes,			"");
	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.AllowNoLoginRequestTypes,		"");

//	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.NeedUsernameRequestTypes,		"");
//	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.NeedDisplayNameRequestTypes,	"");
//	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.NeedGroupMembershipRequestTypes,"");
//	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.NeedSessionIndexRequestTypes,	"");

	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.NoXmlWhiteSpaceRequestTypes,	"");
	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.NonXMLRequestTypes,				"");


	__SUP_COUT__ << "Done setting up Core Supervisor Base property defaults for supervisor" <<
			"..." << __E__;
}

//========================================================================================================================
void CorePropertySupervisorBase::checkSupervisorPropertySetup()
{
	if(propertiesAreSetup_) return;

	//Immediately mark properties as setup, (prevent infinite loops due to
	//	other launches from within this function, e.g. from getSupervisorProperty)
	//	only redo if Context configuration group changes
	propertiesAreSetup_ = true;


	CorePropertySupervisorBase::setSupervisorPropertyDefaults(); 	//calls base class version defaults

	__SUP_COUT__ << "Setting up supervisor specific property DEFAULTS for supervisor" <<
			"..." << __E__;
	setSupervisorPropertyDefaults();						//calls override version defaults
	__SUP_COUT__ << "Done setting up supervisor specific property DEFAULTS for supervisor" <<
			"." << __E__;

	if(allSupervisorInfo_.isWizardMode())
		__SUP_COUT__ << "Wiz mode detected. Skipping setup of supervisor properties for supervisor of class '" <<
						supervisorClass_ <<
					"'" << __E__;
	else
		CorePropertySupervisorBase::loadUserSupervisorProperties();		//loads user settings from configuration


	__SUP_COUT__ << "Setting up supervisor specific FORCED properties for supervisor" <<
			"..." << __E__;
	forceSupervisorPropertyValues();						//calls override forced values
	__SUP_COUT__ << "Done setting up supervisor specific FORCED properties for supervisor" <<
			"." << __E__;


	propertyStruct_.UserPermissionsThreshold.clear();
	StringMacros::getMapFromString(
			getSupervisorProperty(
					CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.UserPermissionsThreshold),
					propertyStruct_.UserPermissionsThreshold);

	propertyStruct_.UserGroupsAllowed.clear();
	StringMacros::getMapFromString(
			getSupervisorProperty(
					CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.UserGroupsAllowed),
					propertyStruct_.UserGroupsAllowed);

	propertyStruct_.UserGroupsDisallowed.clear();
	StringMacros::getMapFromString(
			getSupervisorProperty(
					CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.UserGroupsDisallowed),
					propertyStruct_.UserGroupsDisallowed);

	auto nameIt = SUPERVISOR_PROPERTIES.allSetNames_.begin();
	auto setIt = propertyStruct_.allSets_.begin();
	while(nameIt != SUPERVISOR_PROPERTIES.allSetNames_.end() &&
			setIt != propertyStruct_.allSets_.end())
	{
		(*setIt)->clear();
		StringMacros::getSetFromString(
				getSupervisorProperty(
						*(*nameIt)),
						*(*setIt));

		++nameIt; ++setIt;
	}


//	__SUP_COUT__ << "Final property settings:" << std::endl;
//	for(auto& property: propertyMap_)
//		__SUP_COUT__ << property.first << " = " << property.second << __E__;
}

//========================================================================================================================
//getSupervisorTreeNode ~
//	try to get this Supervisors configuration tree node
ConfigurationTree CorePropertySupervisorBase::getSupervisorTreeNode(void)
try
{
	if(supervisorContextUID_ == "" || supervisorApplicationUID_ == "")
	{
		__SUP_SS__ << "Empty supervisorContextUID_ or supervisorApplicationUID_." << __E__;
		__SUP_SS_THROW__;
	}
	return theConfigurationManager_->getSupervisorNode(
			supervisorContextUID_, supervisorApplicationUID_);
}
catch(...)
{
	__SUP_COUT_ERR__ << "XDAQ Supervisor could not access it's configuration node through theConfigurationManager_ " <<
			"(Did you remember to initialize using CorePropertySupervisorBase::init()?)." <<
			" The supervisorContextUID_ = " << supervisorContextUID_ <<
			". The supervisorApplicationUID = " << supervisorApplicationUID_ << std::endl;
	throw;
}

//========================================================================================================================
//loadUserSupervisorProperties ~
//	try to get user supervisor properties
void CorePropertySupervisorBase::loadUserSupervisorProperties(void)
{
	__SUP_COUT__ << "Loading user properties for supervisor '" <<
			supervisorContextUID_ << "/" << supervisorApplicationUID_ <<
			"'..." << __E__;

	//re-acquire the configuration supervisor node, in case the config has changed
	auto supervisorNode = CorePropertySupervisorBase::getSupervisorTreeNode();

	try
	{
		auto /*map<name,node>*/ children = supervisorNode.getNode("LinkToPropertyConfiguration").getChildren();

		for(auto& child:children)
		{
			if(child.second.getNode("Status").getValue<bool>() == false) continue; //skip OFF properties

			auto propertyName = child.second.getNode("PropertyName").getValue();
			setSupervisorProperty(propertyName, child.second.getNode("PropertyValue").getValue<std::string>());
		}
	}
	catch(...)
	{
		__SUP_COUT__ << "No supervisor security settings found, going with defaults." << __E__;
	}

	__SUP_COUT__ << "Done loading user properties for supervisor '" <<
			supervisorContextUID_ << "/" << supervisorApplicationUID_ <<
			"'" << __E__;
}

//========================================================================================================================
void CorePropertySupervisorBase::setSupervisorProperty(const std::string& propertyName, const std::string& propertyValue)
{
	propertyMap_[propertyName] = propertyValue;
//	__SUP_COUT__ << "Set propertyMap_[" << propertyName <<
//			"] = " << propertyMap_[propertyName] << __E__;
}

//========================================================================================================================
void CorePropertySupervisorBase::addSupervisorProperty(const std::string& propertyName, const std::string& propertyValue)
{
	propertyMap_[propertyName] = propertyValue + " | " + getSupervisorProperty(propertyName);
//	__SUP_COUT__ << "Set propertyMap_[" << propertyName <<
//			"] = " << propertyMap_[propertyName] << __E__;
}


//========================================================================================================================
//getSupervisorProperty
//		string version of template function
std::string CorePropertySupervisorBase::getSupervisorProperty(const std::string& propertyName)
{
	//check if need to setup properties
	checkSupervisorPropertySetup ();

	auto it = propertyMap_.find(propertyName);
	if(it == propertyMap_.end())
	{
		__SUP_SS__ << "Could not find property named " << propertyName << __E__;
		throw std::runtime_error(ss.str());//__SUP_SS_THROW__;
	}
	return StringMacros::validateValueForDefaultStringDataType(it->second);
}

//========================================================================================================================
uint8_t CorePropertySupervisorBase::getSupervisorPropertyUserPermissionsThreshold(const std::string& requestType)
{
	//check if need to setup properties
	checkSupervisorPropertySetup();

	auto it = propertyStruct_.UserPermissionsThreshold.find(requestType);
	if(it == propertyStruct_.UserPermissionsThreshold.end())
	{
		__SUP_SS__ << "Could not find requestType named " << requestType << " in UserPermissionsThreshold map." << __E__;
		throw std::runtime_error(ss.str()); //__SUP_SS_THROW__;
	}
	return it->second;
}

//========================================================================================================================
//getRequestUserInfo ~
//	extract user info for request based on property configuration
void CorePropertySupervisorBase::getRequestUserInfo(WebUsers::RequestUserInfo& userInfo)
{
	checkSupervisorPropertySetup();

	//__SUP_COUT__ << "userInfo.requestType_ " << userInfo.requestType_ << " files: " << cgiIn.getFiles().size() << std::endl;

	userInfo.automatedCommand_ 			= StringMacros::inWildCardSet(userInfo.requestType_, propertyStruct_.AutomatedRequestTypes); //automatic commands should not refresh cookie code.. only user initiated commands should!
	userInfo.NonXMLRequestType_			= StringMacros::inWildCardSet(userInfo.requestType_, propertyStruct_.NonXMLRequestTypes); //non-xml request types just return the request return string to client
	userInfo.NoXmlWhiteSpace_ 			= StringMacros::inWildCardSet(userInfo.requestType_, propertyStruct_.NoXmlWhiteSpaceRequestTypes);

	//**** start LOGIN GATEWAY CODE ***//
	//check cookieCode, sequence, userWithLock, and permissions access all in one shot!
	{
		userInfo.checkLock_				= StringMacros::inWildCardSet(userInfo.requestType_, propertyStruct_.CheckUserLockRequestTypes);
		userInfo.requireLock_ 			= StringMacros::inWildCardSet(userInfo.requestType_, propertyStruct_.RequireUserLockRequestTypes);
		userInfo.allowNoUser_ 			= StringMacros::inWildCardSet(userInfo.requestType_, propertyStruct_.AllowNoLoginRequestTypes);


//		__COUTV__(userInfo.requestType_);
//		__COUTV__(userInfo.checkLock_);
//		__COUTV__(userInfo.requireLock_);
//		__COUTV__(userInfo.allowNoUser_);

		userInfo.permissionsThreshold_ 	= -1; //default to max
		try
		{
			userInfo.permissionsThreshold_ = CorePropertySupervisorBase::getSupervisorPropertyUserPermissionsThreshold(userInfo.requestType_);
		}
		catch(std::runtime_error& e)
		{
//			if(!userInfo.automatedCommand_)
//				 __SUP_COUT__ << "No explicit permissions threshold for request '" <<
//						 userInfo.requestType_ << "'... Defaulting to max threshold = " <<
//						 (unsigned int)userInfo.permissionsThreshold_ << __E__;
		}

		try
		{
			StringMacros::getSetFromString(
					StringMacros::getWildCardMatchFromMap(userInfo.requestType_,
							propertyStruct_.UserGroupsAllowed),
							userInfo.groupsAllowed_);
		}
		catch(std::runtime_error& e)
		{
			userInfo.groupsAllowed_.clear();

//			if(!userInfo.automatedCommand_)
//				__SUP_COUT__ << "No explicit groups allowed for request '" <<
//					 userInfo.requestType_ << "'... Defaulting to empty groups allowed. " << __E__;
		}
		try
		{
			StringMacros::getSetFromString(
					StringMacros::getWildCardMatchFromMap(userInfo.requestType_,
									propertyStruct_.UserGroupsDisallowed),
									userInfo.groupsDisallowed_);
		}
		catch(std::runtime_error& e)
		{
			userInfo.groupsDisallowed_.clear();

//			if(!userInfo.automatedCommand_)
//				__SUP_COUT__ << "No explicit groups disallowed for request '" <<
//					 userInfo.requestType_ << "'... Defaulting to empty groups disallowed. " << __E__;
		}
	} //**** end LOGIN GATEWAY CODE ***//

	//completed user info, for the request type, is returned to caller
}

