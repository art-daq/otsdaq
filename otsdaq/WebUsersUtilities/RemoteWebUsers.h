#ifndef _ots_Utilities_RemoteWebUsers_h
#define _ots_Utilities_RemoteWebUsers_h

#include "otsdaq/SOAPUtilities/SOAPMessenger.h"  //for xdaq::ApplicationDescriptor
#include "otsdaq/WebUsersUtilities/WebUsers.h"

#include <iostream>
#include <string>

#include "otsdaq/TableCore/TableGroupKey.h"  //for TableGroupKey

// clang-format off
namespace ots
{
class AllSupervisorInfo;
class HttpXmlDocument;

// RemoteWebUsers
//	This class provides the functionality for client supervisors to check with the Gateway
// Supervisor 	to verify user access. It also provides the functionality for client
// supervisors to retreive user info.
class RemoteWebUsers : public SOAPMessenger
{
  public:
	RemoteWebUsers(xdaq::Application* application, XDAQ_CONST_CALL xdaq::ApplicationDescriptor* gatewaySupervisorDescriptor);



	// const_cast away the const
	//	so that this line is compatible with slf6 and slf7 versions of xdaq
	//	where they changed to XDAQ_CONST_CALL xdaq::ApplicationDescriptor* in slf7
	//
	// XDAQ_CONST_CALL is defined in "otsdaq/Macros/CoutMacros.h"
	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* gatewaySupervisorDescriptor_;

	// for external supervisors to check with Supervisor for login
	// if false, user request handling code should just return.. out is handled on false;
	// on true, out is untouched
	bool 		xmlRequestToGateway(cgicc::Cgicc& cgi, std::ostringstream* out, HttpXmlDocument* xmldoc, const AllSupervisorInfo& allSupervisorInfo, WebUsers::RequestUserInfo& userInfo);

	//			uint8_t* 						userPermissions = 0,
	//			const uint8_t					permissionsThreshold = 1,
	//			const bool						allowNoUser = false,
	//			const std::set<std::string>&	groupsAllowed = {},
	//			const std::set<std::string>&	groupsDisallowed = {},
	//			const bool						refreshCookie = true,
	//			const bool						checkLock = false,
	//			const bool						lockRequired = false,
	//			std::string* 					userWithLock = 0,
	//			std::string* 					username = 0,
	//			std::string* 					displayName = 0,
	//			std::string* 					userGroups = 0,
	//			uint64_t* 						activeSessionIndex = 0);

	std::string getActiveUserList		(void);
	void        sendSystemMessage		(const std::string& toUser, const std::string& message, bool doEmail = false);
	void        sendSystemMessage		(const std::string& toUser, const std::string& subject, const std::string& message, bool doEmail = false);
	void        makeSystemLogEntry	(const std::string& entryText);
	std::pair<std::string /*group name*/, TableGroupKey>
				getLastTableGroup		(const std::string& actionOfLastGroup, std::string& returnedActionTimeString);  // actionOfLastGroup = "Configured" or "Started", for example

  private:

	//"Active User List" associations ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	std::string ActiveUserList_;
	time_t      ActiveUserLastUpdateTime_;
	enum
	{
		ACTIVE_USERS_UPDATE_THRESHOLD = 2,  // seconds, min amount of time between Supervisor requests
	};

	//std::string tmpUserWithLock_, tmpUserGroups_, tmpUsername_;

};
// clang-format on
}  // namespace ots

#endif
