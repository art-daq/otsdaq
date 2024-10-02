#include "otsdaq/WebUsersUtilities/RemoteWebUsers.h"

#include "otsdaq/CgiDataUtilities/CgiDataUtilities.h"
#include "otsdaq/SOAPUtilities/SOAPCommand.h"
#include "otsdaq/SOAPUtilities/SOAPParameters.h"  //must include in .h for static function
#include "otsdaq/SOAPUtilities/SOAPUtilities.h"
#include "otsdaq/XmlUtilities/HttpXmlDocument.h"

#include <cstdio>
#include <cstdlib>
#include <vector>
#include <tuple>

#include "otsdaq/SupervisorInfo/AllSupervisorInfo.h"

using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "RemoteWebUsers"

// clang-format off
//==============================================================================
// User Notes:
//	- use xmlRequestGateway to check security from outside the Supervisor and Wizard
//
//	Example usage:
//
//
//
//			void exampleClass::exampleRequestHandler(xgi::Input * in, xgi::Output * out)
//
//			{
//				cgicc::Cgicc cgi(in);
//
//				//...
//
//				HttpXmlDocument xmldoc;
//				std::string userWithLock, userName, displayName;
//				uint64_t activeSessionIndex;
//				uint8_t userPermissions;
//
//				//**** start LOGIN GATEWAY CODE ***//
//				//check cookieCode, sequence, userWithLock, and permissions access all in
// one  shot!
//				{
//					bool automaticCommand = 0; //automatic commands should not refresh
// cookie  code.. only user initiated commands should! 					bool checkLock =
// true; 					bool lockRequired = true;
//
//					if(!theRemoteWebUsers_.xmlRequestToGateway(
//							cgi,out,&xmldoc,theSupervisorsConfiguration_
//							,&userPermissions  			//acquire user's access level
//(optionally  null  pointer)//
//							,!automaticCommand			//true/false refresh cookie code
//							,USER_PERMISSIONS_THRESHOLD //set access level requirement to
// pass  gateway
//							,checkLock					//true/false enable check that
// system  is  unlocked  or  this user has the lock ,lockRequired
////true/false requires this user has the lock to  proceed
//							,&userWithLock				//acquire username with lock
//(optionally  null  pointer)
//							,&userName					//acquire username of this user
//(optionally
// null  pointer) 							,0//,&displayName			//acquire user's
// Display  Name
//							,0//,&activeSessionIndex	//acquire user's session index
// associated  with  the cookieCode
//							))
//					{	//failure
//						//std::cout << out->str() << std::endl; //could print out return
// string  on  failure 						return;
//					}
//				}
//				//done checking cookieCode, sequence, userWithLock, and permissions access
// all  in one shot!
//				//**** end LOGIN GATEWAY CODE ***//
//
//				//Success! if here.
//				//
//				//... use acquired values below
//				//...
//
//				//add to xml document, for example:
//				//DOMElement* parentEl;
//				//parentEl = xmldoc.addTextElementToData("ExampleTag", "parent-data");
//				//xmldoc.addTextElementToParent("ExampleChild", "child-data", parentEl);
//
//				//return xml doc holding server response
//				//xmldoc.outputXmlDocument((std::ostringstream*) out, true); //true to
// also  print to std::cout
//			}
//
//
// clang-format on

//==============================================================================
RemoteWebUsers::RemoteWebUsers(xdaq::Application* application, XDAQ_CONST_CALL xdaq::ApplicationDescriptor* gatewaySupervisorDescriptor)
    : SOAPMessenger(application), gatewaySupervisorDescriptor_(gatewaySupervisorDescriptor)
{
	ActiveUserLastUpdateTime_ = 0;   // init to never
	ActiveUserList_           = "";  // init to empty
}  // end constructor()

//==============================================================================
// xmlRequestGateway
//	if false, user code should just return.. out is handled on false; on true, out is
// untouched
bool RemoteWebUsers::xmlRequestToGateway(
    cgicc::Cgicc& cgi, std::ostringstream* out, HttpXmlDocument* xmldoc, const AllSupervisorInfo& allSupervisorInfo, WebUsers::RequestUserInfo& userInfo)
{
	//__COUT__ << std::endl;
	// initialize user info parameters to failed results
	WebUsers::initializeRequestUserInfo(cgi, userInfo);

	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* gatewaySupervisor;

	SOAPParameters         parameters;
	xoap::MessageReference retMsg;

	//**** start LOGIN GATEWAY CODE ***//
	// If TRUE, cookie code is good, and refreshed code is in cookieCode
	// Else, error message is returned in cookieCode
	// tmpCookieCode_ = CgiDataUtilities::getOrPostData(cgi,"CookieCode"); //from GET or
	// POST

	//	__COUT__ << cookieCode.length() << std::endl;
	//	__COUT__ << "cookieCode=" << cookieCode << std::endl;
	//__COUT__ << std::endl;

	/////////////////////////////////////////////////////
	// have CookieCode, try it out
	if(allSupervisorInfo.isWizardMode() || allSupervisorInfo.isMacroMakerMode())
	{
		// if missing CookieCode... check if in Wizard mode and using sequence
		std::string sequence = CgiDataUtilities::getOrPostData(cgi, "sequence");  // from GET or POST
		//__COUT__ << "sequence=" << sequence << std::endl;
		if(!sequence.length())
		{
			__COUT_ERR__ << "Invalid access attempt (@" << userInfo.ip_ << ")." << std::endl;
			*out << WebUsers::REQ_NO_LOGIN_RESPONSE;
			// invalid cookie and also invalid sequence
			goto HANDLE_ACCESS_FAILURE;  // return false, access failed
		}

		// have sequence, try it out

		if(allSupervisorInfo.isWizardMode())
			gatewaySupervisor = allSupervisorInfo.getWizardInfo().getDescriptor();
		else //is MacroMaker mode
			gatewaySupervisor = allSupervisorInfo.getAllMacroMakerTypeSupervisorInfo().begin()->second.getDescriptor();

		if(!gatewaySupervisor)
		{
			__COUT_ERR__ << "Missing gateway supervisor." << std::endl;
			*out << WebUsers::REQ_NO_LOGIN_RESPONSE;
			// sequence code present, but no wizard supervisor
			goto HANDLE_ACCESS_FAILURE;  // return false, access failed
		}

		parameters.addParameter("sequence", sequence);
		parameters.addParameter("IPAddress", userInfo.ip_);
		retMsg = SOAPMessenger::sendWithSOAPReply(gatewaySupervisor, "SupervisorSequenceCheck", parameters);
		parameters.clear();
		parameters.addParameter("Permissions");
		SOAPUtilities::receive(retMsg, parameters);

		userInfo.setGroupPermissionLevels(parameters.getValue("Permissions"));

		if(WebUsers::checkRequestAccess(cgi, out, xmldoc, userInfo, true /*isWizardMode*/, sequence))
			return true;
		else
			goto HANDLE_ACCESS_FAILURE;  // return false, access failed

		//		if(userInfo.permissionLevel_ < userInfo.permissionsThreshold_)
		//		{
		//			*out << WebUsers::REQ_NO_LOGIN_RESPONSE;
		//			__COUT__ << "User (@" << userInfo.ip_ << ") has insufficient
		// permissions: " << userInfo.permissionLevel_ << "<" <<
		//					userInfo.permissionsThreshold_ << std::endl;
		//			return false;	//invalid cookie and present sequence, but not correct
		// sequence
		//		}
		//
		//		userInfo.setUsername("admin");
		//		userInfo.setDisplayName("Admin");
		//		userInfo.setUsernameWithLock("admin");
		//		userInfo.setActiveUserSessionIndex(0);
		//		userInfo.setGroupMemebership("admin");
		//
		//		return true; //successful sequence login!
	}

	// else proceed with inquiry to Gateway Supervisor

	gatewaySupervisor = allSupervisorInfo.getGatewayInfo().getDescriptor();

	if(!gatewaySupervisor)
	{
		__COUT_ERR__ << "Missing gateway supervisor." << std::endl;
		*out << WebUsers::REQ_NO_LOGIN_RESPONSE;
		goto HANDLE_ACCESS_FAILURE;  // return false, access failed
	}

	//__COUT__ << std::endl;

	parameters.clear();
	parameters.addParameter("CookieCode", userInfo.cookieCode_);
	parameters.addParameter("RefreshOption", userInfo.automatedCommand_ ? "0" : "1");
	parameters.addParameter("IPAddress", userInfo.ip_);

	retMsg = SOAPMessenger::sendWithSOAPReply(gatewaySupervisor, "SupervisorCookieCheck", parameters);

	parameters.clear();
	parameters.addParameter("CookieCode");
	parameters.addParameter("Permissions");
	parameters.addParameter("UserGroups");
	parameters.addParameter("UserWithLock");
	parameters.addParameter("Username");
	parameters.addParameter("DisplayName");
	// parameters.addParameter("ActiveSessionIndex");
	SOAPUtilities::receive(retMsg, parameters);

	//__COUT__ << std::endl;

	// first extract a few things always from parameters
	//	like permissionLevel for this request... must consider allowed groups!!
	userInfo.setGroupPermissionLevels(parameters.getValue("Permissions"));
	userInfo.cookieCode_             = parameters.getValue("CookieCode");
	userInfo.username_               = parameters.getValue("Username");
	userInfo.displayName_            = parameters.getValue("DisplayName");
	userInfo.usernameWithLock_       = parameters.getValue("UserWithLock");
	// userInfo.activeUserSessionIndex_ = strtoul(parameters.getValue("ActiveSessionIndex").c_str(), 0, 0);

	if(!WebUsers::checkRequestAccess(cgi, out, xmldoc, userInfo))
		goto HANDLE_ACCESS_FAILURE;  // return false, access failed
	// else successful access request!

	return true;  // request granted

	/////////////////////////////////////////////////////

HANDLE_ACCESS_FAILURE:

	// print out return string on failure
	if(!userInfo.automatedCommand_)
		__COUT_ERR__ << "Failed request (requestType = " << userInfo.requestType_ << "): " << out->str() << __E__;
	return false;  // access failed
}  // end xmlRequestToGateway()

//==============================================================================
// getActiveUserList
//	if lastUpdateTime is not too recent as spec'd by ACTIVE_USERS_UPDATE_THRESHOLD
//	if server responds with
std::string RemoteWebUsers::getActiveUserList()
{
	if(time(0) - ActiveUserLastUpdateTime_ > ACTIVE_USERS_UPDATE_THRESHOLD)  // need to update
	{
		__COUT__ << "Need to update " << std::endl;

		xoap::MessageReference retMsg = ots::SOAPMessenger::sendWithSOAPReply(gatewaySupervisorDescriptor_, "SupervisorGetActiveUsers");

		SOAPParameters retParameters("UserList");
		SOAPUtilities::receive(retMsg, retParameters);

		ActiveUserLastUpdateTime_ = time(0);
		return (ActiveUserList_ = retParameters.getValue("UserList"));
	}
	else
		return ActiveUserList_;
}  // end getActiveUserList()


//==============================================================================
// getLastTableGroups
//	request last "Configured" or "Started" group, for example
//	returns empty "" for actionTimeString on failure
//	returns "Wed Dec 31 18:00:01 1969 CST" for actionTimeString (in CST) if action never
// has occurred
void RemoteWebUsers::getLastTableGroups(
	std::map< std::string /* group type */,
			std::tuple<std::string /*group name*/, TableGroupKey, 
				std::string /* time string*/>>& theGroups
	)
{
	xoap::MessageReference retMsg = ots::SOAPMessenger::sendWithSOAPReply(
	    gatewaySupervisorDescriptor_, "SupervisorLastTableGroupRequest", SOAPParameters("ActionOfLastGroup", "ALL"));


	SOAPParameters retParameters;
	retParameters.addParameter("GroupName");
	retParameters.addParameter("GroupKey");
	retParameters.addParameter("GroupAction");
	retParameters.addParameter("GroupActionTime");
	SOAPUtilities::receive(retMsg, retParameters);


	//parse as CSV
	std::vector<std::string> groupNames = StringMacros::getVectorFromString(
			retParameters.getValue("GroupName"), {','});
	std::vector<std::string> groupKeys = StringMacros::getVectorFromString(
			retParameters.getValue("GroupKey"), {','});
	std::vector<std::string> groupActions = StringMacros::getVectorFromString(
			retParameters.getValue("GroupAction"), {','});
	std::vector<std::string> groupTimes = StringMacros::getVectorFromString(
			retParameters.getValue("GroupActionTime"), {','});

	if(groupNames.size() < 2)
	{
		//expecting something like 7?
		__SS__ << "Failure in handling request for recent config group activity. Response received was this: \n" <<
				SOAPUtilities::translate(retMsg) << __E__;
		__SS_THROW__;
	}

	if(groupNames.size() != groupKeys.size() || groupNames.size() != groupActions.size() || 
		groupNames.size() != groupTimes.size())
	{
		__SS__ << "Illegal list size mismatch while retrieving recent config group info. Should not be possible! Notify admins." << __E__;
		__SS_THROW__;
	}

	for(size_t i=0; i < groupNames.size(); ++i)
	{
		theGroups[groupActions[i]] = std::make_tuple(
			groupNames[i], strtol(groupKeys[i].c_str(), 0, 0), groupTimes[i]
		);
	}

	__COUTT__ << "Done with getLastTableGroups()" << __E__;
}  // end getLastTableGroup()

//==============================================================================
// getLastTableGroup
//	request last "Configured" or "Started" group, for example
//	returns empty "" for actionTimeString on failure
//	returns "Wed Dec 31 18:00:01 1969 CST" for actionTimeString (in CST) if action never
// has occurred
std::pair<std::string /*group name*/, TableGroupKey> RemoteWebUsers::getLastTableGroup(const std::string& actionOfLastGroup, std::string& actionTimeString)
{
	actionTimeString              = "";
	xoap::MessageReference retMsg = ots::SOAPMessenger::sendWithSOAPReply(
	    gatewaySupervisorDescriptor_, "SupervisorLastTableGroupRequest", SOAPParameters("ActionOfLastGroup", actionOfLastGroup));

	SOAPParameters retParameters;
	retParameters.addParameter("GroupName");
	retParameters.addParameter("GroupKey");
	retParameters.addParameter("GroupAction");
	retParameters.addParameter("GroupActionTime");
	SOAPUtilities::receive(retMsg, retParameters);

	std::pair<std::string /*group name*/, TableGroupKey> theGroup;
	if(retParameters.getValue("GroupAction") != actionOfLastGroup)  // if action doesn't match.. weird
	{
		__SS__ << "Returned group action '" << retParameters.getValue("GroupAction") << "' does not match requested group action '" << actionOfLastGroup
		              << ".'" << std::endl;
		__SS_THROW__;
	}
	// else we have an action match

	theGroup.first   = retParameters.getValue("GroupName");
	theGroup.second  = strtol(retParameters.getValue("GroupKey").c_str(), 0, 0);
	actionTimeString = retParameters.getValue("GroupActionTime");
	return theGroup;
}  // end getLastTableGroup()

//==============================================================================
// sendSystemMessage
//	send system message to toUser through Supervisor
//	toUser wild card * is to all users
void RemoteWebUsers::sendSystemMessage(const std::string& toUser, const std::string& message, bool doEmail /*=false*/)
{
	sendSystemMessage(toUser, "" /*subject*/, message, doEmail);
}  // end sendSystemMessage)

//==============================================================================
// sendSystemMessage
//	send system message to toUser comma separate variable (CSV) list through Supervisor
//	toUser wild card * is to all users
void RemoteWebUsers::sendSystemMessage(const std::string& toUser, const std::string& subject, const std::string& message, bool doEmail /*=false*/)
{
	SOAPParameters parameters;
	parameters.addParameter("ToUser", toUser);  // CSV list or *
	parameters.addParameter("Subject", subject);
	parameters.addParameter("Message", message);
	parameters.addParameter("DoEmail", doEmail ? "1" : "0");

	xoap::MessageReference retMsg = SOAPMessenger::sendWithSOAPReply(gatewaySupervisorDescriptor_, "SupervisorSystemMessage", parameters);

	//__COUT__ << SOAPUtilities::translate(retMsg) << __E__;
}  // end sendSystemMessage)

//==============================================================================
// makeSystemLogEntry
//	make system logbook through Supervisor
void RemoteWebUsers::makeSystemLogEntry(const std::string& entryText)
{
	SOAPParameters parameters;
	parameters.addParameter("EntryText", entryText);

	xoap::MessageReference retMsg = SOAPMessenger::sendWithSOAPReply(gatewaySupervisorDescriptor_, "SupervisorSystemLogbookEntry", parameters);

	//__COUT__ << SOAPUtilities::translate(retMsg) << __E__;
}  // end makeSystemLogEntry()
