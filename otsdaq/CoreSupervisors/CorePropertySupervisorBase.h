#ifndef _ots_CorePropertySupervisorBase_h_
#define _ots_CorePropertySupervisorBase_h_

#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/ConfigurationInterface/ConfigurationTree.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"
#include "otsdaq/SupervisorInfo/AllSupervisorInfo.h"
#include "otsdaq/TableCore/TableGroupKey.h"
#include "otsdaq/TablePlugins/XDAQContextTable/XDAQContextTable.h"

#include "otsdaq/WebUsersUtilities/WebUsers.h"  //for WebUsers::RequestUserInfo

// #pragma GCC diagnostic push
// #pragma GCC diagnostic ignored "-Wdeprecated-declarations"
// #include <xdaq/Application.h>
// #pragma GCC diagnostic pop

#include "otsdaq/SOAPUtilities/SOAPMessenger.h"
#include "otsdaq/WorkLoopManager/WorkLoopManager.h"

#include "otsdaq/CoreSupervisors/CorePropertySupervisorBase.h"

#include "otsdaq/CgiDataUtilities/CgiDataUtilities.h"
#include "otsdaq/SOAPUtilities/SOAPUtilities.h"
#include "otsdaq/XmlUtilities/HttpXmlDocument.h"

#include "otsdaq/FiniteStateMachine/VStateMachine.h"

#include "otsdaq/MessageFacility/ITRACEController.h"
#include "otsdaq/WebUsersUtilities/RemoteWebUsers.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <xdaq/Application.h>
#pragma GCC diagnostic pop
#include "otsdaq/Macros/XDAQApplicationMacros.h"
#include "xgi/Method.h"

#include <toolbox/fsm/FailedEvent.h>

#include <xdaq/NamespaceURI.h>
#include <xoap/Method.h>

namespace ots
{
// clang-format off

class ITRACEController;

// CorePropertySupervisorBase
//	This class provides supervisor property get and set functionality. It has member
// variables 		generally useful to the configuration of client supervisors.
class CorePropertySupervisorBase
{
	friend class GatewaySupervisor;  // for access to indicateOtsAlive()

  public:
	CorePropertySupervisorBase(xdaq::Application* application);
	virtual ~CorePropertySupervisorBase(void);

	AllSupervisorInfo     allSupervisorInfo_;
	ConfigurationManager* theConfigurationManager_;

	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* getGatewaySupervisorDescriptor		(void); //will be wizard supervisor in wiz mode

	virtual void 					setSupervisorPropertyDefaults					(void);  // override to control supervisor specific defaults
	virtual void 					forceSupervisorPropertyValues					(void){;}  // override to force supervisor property values (and ignore user settings)

	void 							getRequestUserInfo								(WebUsers::RequestUserInfo& requestUserInfo);

	// supervisors should use these two static functions to standardize permissions
	// access:
	static void 					extractPermissionsMapFromString					(const std::string& permissionsString, std::map<std::string, WebUsers::permissionLevel_t>& permissionsMap);
	static bool 					doPermissionsGrantAccess						(std::map<std::string, WebUsers::permissionLevel_t>& permissionLevelsMap, std::map<std::string, WebUsers::permissionLevel_t>& permissionThresholdsMap);

	ConfigurationTree 				getContextTreeNode								(void) const
	{
		return theConfigurationManager_->getNode(
		    theConfigurationManager_->__GET_CONFIG__(XDAQContextTable)->getTableName());
	}
	ConfigurationTree 				getSupervisorTableNode							(void) const
	{
		return getContextTreeNode().getNode(supervisorConfigurationPath_);
	}

	const std::string& 				getContextUID									(void) const { return supervisorContextUID_; }
	const std::string& 				getSupervisorUID								(void) const { return supervisorApplicationUID_; }
	const std::string& 				getSupervisorConfigurationPath					(void) const
	{
		return supervisorConfigurationPath_;
	}

  protected:
	const std::string supervisorClass_;
	const std::string supervisorClassNoNamespace_;

  private:
	static void 					indicateOtsAlive								(const CorePropertySupervisorBase* properties = 0);


	std::string supervisorContextUID_;
	std::string supervisorApplicationUID_;
	std::string supervisorConfigurationPath_;


  protected:
	xoap::MessageReference 			TRACESupervisorRequest							(xoap::MessageReference message);
	const std::string&				getTraceLevels									(void);
	const std::string&				setTraceLevels									(std::string const& host, std::string const& mode, std::string const& labelsStr, uint32_t setValueMSB, uint32_t setValueLSB);
	const std::string&				setIndividualTraceLevels						(std::string const& host, std::string const& mode, std::string const& labelValuesStr);
	const std::string&				getTraceTriggerStatus							(void);
	const std::string&				setTraceTriggerEnable							(std::string const& host, size_t entriesAfterTrigger);
	const std::string&				resetTRACE										(std::string const& host);
	const std::string&				enableTRACE										(std::string const& host, bool enable);
	const std::string&				getTraceSnapshot								(std::string const& host, std::string const& filterFor, std::string const& filterOut);

	// Supervisor Property names
	//	to access, use CorePropertySupervisorBase::getSupervisorProperty and
	// CorePropertySupervisorBase::setSupervisorProperty
	static const struct SupervisorProperties
	{
		SupervisorProperties()
		    : allSetNames_({&CheckUserLockRequestTypes,
		                    &RequireUserLockRequestTypes,
		                    &AutomatedRequestTypes,
		                    &AllowNoLoginRequestTypes,
							&RequireSecurityRequestTypes,
		                    &NoXmlWhiteSpaceRequestTypes,
		                    &NonXMLRequestTypes})
		{
		}

		const std::string UserPermissionsThreshold = "UserPermissionsThreshold";
		const std::string UserGroupsAllowed        = "UserGroupsAllowed";
		const std::string UserGroupsDisallowed     = "UserGroupsDisallowed";

		const std::string CheckUserLockRequestTypes   = "CheckUserLockRequestTypes";
		const std::string RequireUserLockRequestTypes = "RequireUserLockRequestTypes";
		const std::string AutomatedRequestTypes       = "AutomatedRequestTypes";
		const std::string AllowNoLoginRequestTypes    = "AllowNoLoginRequestTypes";
		const std::string RequireSecurityRequestTypes = "RequireSecurityRequestTypes";

		const std::string NoXmlWhiteSpaceRequestTypes = "NoXmlWhiteSpaceRequestTypes";
		const std::string NonXMLRequestTypes          = "NonXMLRequestTypes";

		const std::set<const std::string*> allSetNames_;
	} SUPERVISOR_PROPERTIES;

  private:
	// property private members
	void          					checkSupervisorPropertySetup						(void);
	volatile bool propertiesAreSetup_;

	// for public access to property map,..
	//	use CorePropertySupervisorBase::getSupervisorProperty and
	// CorePropertySupervisorBase::setSupervisorProperty
	std::map<std::string, std::string> propertyMap_;
	struct CoreSupervisorPropertyStruct
	{
		CoreSupervisorPropertyStruct()
		    : allSets_({&CheckUserLockRequestTypes,
		                &RequireUserLockRequestTypes,
		                &AutomatedRequestTypes,
		                &AllowNoLoginRequestTypes,
						&RequireSecurityRequestTypes,
		                &NoXmlWhiteSpaceRequestTypes,
		                &NonXMLRequestTypes})
		{
		}

		std::map<std::string, WebUsers::permissionLevel_t> UserPermissionsThreshold;
		std::map<std::string, std::string>                 UserGroupsAllowed;
		std::map<std::string, std::string>                 UserGroupsDisallowed;

		std::set<std::string> CheckUserLockRequestTypes;
		std::set<std::string> RequireUserLockRequestTypes;
		std::set<std::string> AutomatedRequestTypes;
		std::set<std::string> AllowNoLoginRequestTypes;
		std::set<std::string> RequireSecurityRequestTypes;

		std::set<std::string> NoXmlWhiteSpaceRequestTypes;
		std::set<std::string> NonXMLRequestTypes;

		std::set<std::set<std::string>*> allSets_;
	} propertyStruct_;

  public:
	void 							resetPropertiesAreSetup								(void)
	{
		propertiesAreSetup_ = false;
	}  // forces reload of properties from configuration
	ConfigurationTree getSupervisorTreeNode(void);

	void 							loadUserSupervisorProperties						(void);
	template<class T>
	void 							setSupervisorProperty								(
			const std::string& propertyName, const T& propertyValue)
	{
		std::stringstream ss;
		ss << propertyValue;
		setSupervisorProperty(propertyName, ss.str());
	} //end setSupervisorProperty()
	void 							setSupervisorProperty								(const std::string& propertyName,
	                           const std::string& propertyValue);
	template<class T>
	void 							addSupervisorProperty								(
			const std::string& propertyName, const T& propertyValue)
	{
		// prepend new values.. since map/set extraction takes the first value encountered
		std::stringstream ss;
		ss << propertyValue << " | " << getSupervisorProperty(propertyName);
		setSupervisorProperty(propertyName, ss.str());
	} //end addSupervisorProperty()
	void 							addSupervisorProperty								(const std::string& propertyName, const std::string& propertyValue);
	template<class T>
	T 								getSupervisorProperty								(
			const std::string& propertyName)
	{
		// check if need to setup properties
		checkSupervisorPropertySetup();

		auto it = propertyMap_.find(propertyName);
		if(it == propertyMap_.end())
		{
			__SS__ << "Could not find property named " << propertyName << __E__;
			__SS_THROW__;
		}
		return StringMacros::validateValueForDefaultStringDataType<T>(it->second);
	} //end getSupervisorProperty()
	template<class T>
	T 									getSupervisorProperty							(
			const std::string& propertyName, const T& defaultValue)
	{
		// check if need to setup properties
		checkSupervisorPropertySetup();

		auto it = propertyMap_.find(propertyName);
		if(it == propertyMap_.end())
		{
			//not found, so returning default value
			return defaultValue;
		}
		return StringMacros::validateValueForDefaultStringDataType<T>(it->second);
	} //end getSupervisorProperty()
	std::string                 		getSupervisorProperty							(const std::string& propertyName);
	std::string                 		getSupervisorProperty							(const std::string& propertyName, const std::string& defaultValue);
	WebUsers::permissionLevel_t 		getSupervisorPropertyUserPermissionsThreshold	(const std::string& requestType);

  protected:
	ITRACEController* 					theTRACEController_; //only define for an app that receives a command
  private:
	std::string 						traceReturnString_, traceReturnHostString_;
	bool								readOnly_;
};

// clang-format on

}  // namespace ots

#endif
