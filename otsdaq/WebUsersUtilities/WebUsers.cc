#include "otsdaq/WebUsersUtilities/WebUsers.h"
#include "otsdaq/XmlUtilities/HttpXmlDocument.h"

#include <openssl/sha.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <iostream>

#include <chrono>  // std::chrono::seconds
#include <thread>  // std::this_thread::sleep_for

using namespace ots;

// clang-format off
#define WEB_LOGIN_BKUP_DB_PATH 				"bkup/"

#define SECURITY_FILE_NAME 					std::string(__ENV__("SERVICE_DATA_PATH")) + "/OtsWizardData/security.dat"

#define USERS_ACTIVE_SESSIONS_FILE 			USERS_DB_PATH + "/activeSessions.sv"

#define HASHES_DB_FILE 						HASHES_DB_PATH + "/hashes.xml"
#define USERS_DB_FILE 						USERS_DB_PATH + "/users.xml"
#define USERS_GLOBAL_HISTORY_FILE 			"__global"
#define USERS_LOGIN_HISTORY_FILETYPE 		"hist"
#define USERS_PREFERENCES_FILETYPE 			"pref"
#define SYSTEM_PREFERENCES_PREFIX 			"system.preset"
#define USER_WITH_LOCK_FILE 				WEB_LOGIN_DB_PATH + "/user_with_lock.dat"
#define IP_BLACKLIST_FILE 					WEB_LOGIN_DB_PATH + "/ip_generated_blacklist.dat"
#define IP_REJECT_FILE 						WEB_LOGIN_DB_PATH + "/ip_reject.dat"
#define IP_ACCEPT_FILE 						WEB_LOGIN_DB_PATH + "/ip_accept.dat"

#define SILENCE_ALL_TOOLTIPS_FILENAME       "silenceTooltips"

#define HASHES_DB_GLOBAL_STRING 			"hashData"
#define HASHES_DB_ENTRY_STRING 				"hashEntry"
#define USERS_DB_GLOBAL_STRING 				"userData"
#define USERS_DB_ENTRY_STRING 				"userEntry"
#define USERS_DB_NEXT_UID_STRING 			"nextUserId"

// defines for user preferences
#define PREF_XML_BGCOLOR_FIELD 				"pref_bgcolor"    	// -background color
#define PREF_XML_DBCOLOR_FIELD 				"pref_dbcolor"    	// -dashboard color
#define PREF_XML_WINCOLOR_FIELD 			"pref_wincolor"  	// -window color
#define PREF_XML_LAYOUT_FIELD 				"pref_layout"  		// -3 defaults window layouts(and current)
#define PREF_XML_SYSLAYOUT_FIELD 			"pref_syslayout"  	// -2 defaults window layouts
#define PREF_XML_PERMISSIONS_FIELD 			"desktop_user_permissions"  // 0-255 permissions value (255 is admin super user)
#define PREF_XML_USERLOCK_FIELD 			"username_with_lock"        // user with lock (to lockout others)
#define PREF_XML_USERNAME_FIELD 			"pref_username"  	// user with lock (to lockout others)
#define PREF_XML_OTS_OWNER_FIELD 			"ots_owner"  		// e.g. the experiment name

#define PREF_XML_BGCOLOR_DEFAULT 			"rgb(0,76,151)"     // -background color
#define PREF_XML_DBCOLOR_DEFAULT 			"rgb(0,40,85)"      // -dashboard color
#define PREF_XML_WINCOLOR_DEFAULT 			"rgba(196,229,255,0.9)"  	// -window color
#define PREF_XML_LAYOUT_DEFAULT 			"0;0;0;0"  			// 3 default window layouts(and current)
#define PREF_XML_SYSLAYOUT_DEFAULT 			"0;0"   			// 2 system default window layouts

#define PREF_XML_ACCOUNTS_FIELD 			"users_accounts"  	// user accounts field for super users
#define PREF_XML_LOGIN_HISTORY_FIELD 		"login_entry"  		// login history field for user login history data

const std::string WebUsers::OTS_OWNER					  =	getenv("OTS_OWNER")?getenv("OTS_OWNER"):"";
const std::string WebUsers::DEFAULT_ADMIN_USERNAME        = "admin";
const std::string WebUsers::DEFAULT_ADMIN_DISPLAY_NAME    = "Administrator";
const std::string WebUsers::DEFAULT_ADMIN_EMAIL           = "root@otsdaq.fnal.gov";
const std::string WebUsers::DEFAULT_ITERATOR_USERNAME     = "iterator";
const std::string WebUsers::DEFAULT_STATECHANGER_USERNAME = "statechanger";
const std::string WebUsers::DEFAULT_USER_GROUP            = "allUsers";

const std::string WebUsers::REQ_NO_LOGIN_RESPONSE      = "NoLogin";
const std::string WebUsers::REQ_NO_PERMISSION_RESPONSE = "NoPermission";
const std::string WebUsers::REQ_USER_LOCKOUT_RESPONSE  = "UserLockout";
const std::string WebUsers::REQ_LOCK_REQUIRED_RESPONSE = "LockRequired";
const std::string WebUsers::REQ_ALLOW_NO_USER          = "AllowNoUser";

const std::string WebUsers::SECURITY_TYPE_NONE          = "NoSecurity";
const std::string WebUsers::SECURITY_TYPE_DIGEST_ACCESS = "DigestAccessAuthentication";
const std::string WebUsers::SECURITY_TYPE_DEFAULT 		= WebUsers::SECURITY_TYPE_NONE; // default to NO SECURITY

const std::vector<std::string> WebUsers::HashesDatabaseEntryFields_ = {"hash","lastAccessTime"};
const std::vector<std::string> WebUsers::UsersDatabaseEntryFields_ = {"username","displayName","salt",
		"uid","permissions","lastLoginAttemptTime","accountCreatedTime",
		"loginFailureCount","lastModifiedTime","lastModifierUsername","useremail"};

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "WebUsers"

// clang-format on

WebUsers::WebUsers()
{
	INIT_MF("." /*directory used is USER_DATA/LOG/.*/);

	// deleteUserData(); //leave for debugging to reset user data

	usersNextUserId_       = 0;   // first UID, default to 0 but get from database
	usersUsernameWithLock_ = "";  // init to no user with lock

	// define field labels
	//	HashesDatabaseEntryFields.push_back("hash");
	//	HashesDatabaseEntryFields.push_back("lastAccessTime");  // last login month resolution, blurred by 1/2 month
	//
	//	WebUsers::UsersDatabaseEntryFields_.push_back("username");
	//	WebUsers::UsersDatabaseEntryFields_.push_back("displayName");
	//	WebUsers::UsersDatabaseEntryFields_.push_back("salt");
	//	WebUsers::UsersDatabaseEntryFields_.push_back("uid");
	//	WebUsers::UsersDatabaseEntryFields_.push_back("permissions");
	//	WebUsers::UsersDatabaseEntryFields_.push_back("lastLoginAttemptTime");
	//	WebUsers::UsersDatabaseEntryFields_.push_back("accountCreatedTime");
	//	WebUsers::UsersDatabaseEntryFields_.push_back("loginFailureCount");
	//	WebUsers::UsersDatabaseEntryFields_.push_back("lastModifiedTime");
	//	WebUsers::UsersDatabaseEntryFields_.push_back("lastModifierUsername");
	//	WebUsers::UsersDatabaseEntryFields_.push_back("useremail");

	// attempt to make directory structure (just in case)
	mkdir(((std::string)WEB_LOGIN_DB_PATH).c_str(), 0755);
	mkdir(((std::string)WEB_LOGIN_DB_PATH + "bkup/" + USERS_DB_PATH).c_str(), 0755);
	mkdir(((std::string)WEB_LOGIN_DB_PATH + HASHES_DB_PATH).c_str(), 0755);
	mkdir(((std::string)WEB_LOGIN_DB_PATH + USERS_DB_PATH).c_str(), 0755);
	mkdir(((std::string)WEB_LOGIN_DB_PATH + USERS_LOGIN_HISTORY_PATH).c_str(), 0755);
	mkdir(((std::string)WEB_LOGIN_DB_PATH + USERS_PREFERENCES_PATH).c_str(), 0755);

	if(!loadDatabases())
		__COUT__ << "FATAL USER DATABASE ERROR - failed to load!!!" << __E__;

	loadSecuritySelection();

	// print out admin new user code for ease of use
	uint64_t    i;
	std::string user = DEFAULT_ADMIN_USERNAME;
	if((i = searchUsersDatabaseForUsername(user)) == NOT_FOUND_IN_DATABASE)
	{
		__SS__ << "user: " << user << " is not found. This should be impossible!" << __E__;
		__COUT_ERR__ << ss.str();
		__SS_THROW__;  // THIS CAN NOT HAPPEN?! There must be an admin user
	}
	else if(Users_[i].salt_ == "" &&  // admin password not setup, so print out NAC to help out
	        securityType_ == SECURITY_TYPE_DIGEST_ACCESS)
	{
		//////////////////////////////////////////////////////////////////////
		// start thread for notifying the user about the admin new account code
		// notify for 10 seconds (e.g.)
		std::thread([](const std::string& nac, const std::string& user) { WebUsers::NACDisplayThread(nac, user); }, Users_[i].getNewAccountCode(), user)
		    .detach();
	}

	// attempt to load persistent user sessions
	loadActiveSessions();

	// default user with lock to admin and/or try to load last user with lock
	// Note: this must happen after getting persistent active sessions
	loadUserWithLock();

	srand(time(0));  // seed random for hash salt generation

	__COUT__ << "Done with Web Users initialization!" << __E__;
}  // end constructor

//==============================================================================
// xmlRequestOnGateway
//	check the validity of an xml request at the server side, i.e. at the Gateway
// supervisor, which is the owner 		of the web users instance. 	if false, gateway
// request code should just return.. out is handled on false; on true, out is untouched
bool WebUsers::xmlRequestOnGateway(cgicc::Cgicc& cgi, std::ostringstream* out, HttpXmlDocument* xmldoc, WebUsers::RequestUserInfo& userInfo)
{
	std::lock_guard<std::mutex> lock(webUserMutex_);

	// initialize user info parameters to failed results
	WebUsers::initializeRequestUserInfo(cgi, userInfo);

	uint64_t i;

	if(!cookieCodeIsActiveForRequest(userInfo.cookieCode_,
	                                 &userInfo.groupPermissionLevelMap_,
	                                 &userInfo.uid_,
	                                 userInfo.ip_,
	                                 !userInfo.automatedCommand_ /*refresh cookie*/,
	                                 &userInfo.usernameWithLock_,
	                                 &userInfo.userSessionIndex_))
	{
		*out << userInfo.cookieCode_;
		goto HANDLE_ACCESS_FAILURE;  // return false, access failed
	}

	// setup userInfo.permissionLevel_ based on userInfo.groupPermissionLevelMap_
	userInfo.getGroupPermissionLevel();

	i = searchUsersDatabaseForUserId(userInfo.uid_);
	if(i >= Users_.size())
	{
		__SS__ << "Illegal uid encountered in cookie codes!? " << i << __E__;
		ss << "User size = " << Users_.size() << __E__;
		__SS_THROW__;
	}

	userInfo.username_    = Users_[i].username_;
	userInfo.displayName_ = Users_[i].displayName_;
	
	if(!WebUsers::checkRequestAccess(cgi, out, xmldoc, userInfo))
		goto HANDLE_ACCESS_FAILURE;  // return false, access failed

	return true;  // access success!

HANDLE_ACCESS_FAILURE:
	// print out return string on failure
	if(!userInfo.automatedCommand_)
		__COUT_ERR__ << "Failed request (requestType = " << userInfo.requestType_ << "): " << out->str() << __E__;
	return false;  // access failed

}  // end xmlRequestOnGateway()

//==============================================================================
// initializeRequestUserInfo
//	initialize user info parameters to failed results
void WebUsers::initializeRequestUserInfo(cgicc::Cgicc& cgi, WebUsers::RequestUserInfo& userInfo)
{
	userInfo.ip_ = cgi.getEnvironment().getRemoteAddr();

	// note if related bools are false, members below may not be set
	userInfo.username_               = "";
	userInfo.displayName_            = "";
	userInfo.usernameWithLock_       = "";
	userInfo.userSessionIndex_ 		 = NOT_FOUND_IN_DATABASE;
	userInfo.setGroupPermissionLevels("");  // always init to inactive
}

//==============================================================================
// checkRequestAccess
//	check user permission parameters based on cookie code, user permission level
//(extracted previous from group membership) 	Note: assumes
// userInfo.groupPermissionLevelMap_ and userInfo.permissionLevel_ are properly setup
//		by either calling userInfo.setGroupPermissionLevels() or
// userInfo.getGroupPermissionLevel()
bool WebUsers::checkRequestAccess(cgicc::Cgicc& /*cgi*/,
                                  std::ostringstream*        out,
                                  HttpXmlDocument*           xmldoc,
                                  WebUsers::RequestUserInfo& userInfo,
                                  bool                       isWizardMode /* = false */,
                                  const std::string&         wizardModeSequence /* = "" */)
{
	// steps:
	// - check access based on cookieCode and permission level
	// - check user lock flags and status

	if(userInfo.requireSecurity_ && userInfo.permissionsThreshold_ > 1)
	{
		// In an attempt to force accountability,..
		// only allow higher permission threshold requests
		//	 if wiz mode with random code, or normal mode with security mode enabled

		if(isWizardMode && wizardModeSequence.size() < 8)
		{
			// force wiz mode sequence to be "random and large"
			*out << WebUsers::REQ_NO_PERMISSION_RESPONSE;
			__COUT__ << "User (@" << userInfo.ip_ << ") has attempted requestType '" << userInfo.requestType_
			         << "' which requires sufficient security enabled. Please enable the "
			            "random wizard mode"
			            " sequence of at least 8 characters."
			         << __E__;
			return false;  // invalid cookie and present sequence, but not correct
			               // sequence
		}
		else if(!isWizardMode && (userInfo.username_ == WebUsers::DEFAULT_ADMIN_USERNAME || userInfo.username_ == WebUsers::DEFAULT_ITERATOR_USERNAME ||
		                          userInfo.username_ == WebUsers::DEFAULT_STATECHANGER_USERNAME))
		{
			// force non-admin user, which implies sufficient security
			*out << WebUsers::REQ_NO_PERMISSION_RESPONSE;
			__COUT__ << "User (@" << userInfo.ip_ << ") has attempted requestType '" << userInfo.requestType_
			         << "' which requires sufficient security enabled. Please enable "
			            "individual user "
			            " logins (Note: the user admin is disallowed in an attempt to force personal accountability for edits)."
			         << __E__;
			return false;  // invalid cookie and present sequence, but not correct
			               // sequence
		}

	}  // end security required verification

	if(!userInfo.automatedCommand_)
	{
		__COUTT__ << "requestType ==========>>> " << userInfo.requestType_ << __E__;
		__COUTTV__((unsigned int)userInfo.permissionLevel_);
		__COUTTV__((unsigned int)userInfo.permissionsThreshold_);
	}

	// second, start check access -------
	if(!isWizardMode && !userInfo.allowNoUser_ && userInfo.cookieCode_.length() != WebUsers::COOKIE_CODE_LENGTH)
	{
		__COUT__ << "User (@" << userInfo.ip_ << ") has invalid cookie code: " << userInfo.cookieCode_ << std::endl;
		*out << WebUsers::REQ_NO_LOGIN_RESPONSE;
		return false;  // invalid cookie and present sequence, but not correct sequence
	}

	if(!userInfo.allowNoUser_ && (userInfo.permissionLevel_ == 0 ||  // reject inactive user permission level
								  userInfo.permissionsThreshold_ == 0 || // reject inactive requests
	                              userInfo.permissionLevel_ < userInfo.permissionsThreshold_))

	{
		*out << WebUsers::REQ_NO_PERMISSION_RESPONSE;
		__COUT__ << "User (@" << userInfo.ip_ << ") has insufficient permissions for requestType '" << userInfo.requestType_
		         << "' : user level is " << (unsigned int)userInfo.permissionLevel_ << ", " << 
				 (unsigned int)userInfo.permissionsThreshold_ << " required." << __E__;
		return false;  // invalid cookie and present sequence, but not correct sequence
	}
	// end check access -------

	if(isWizardMode)
	{
		userInfo.username_               = WebUsers::DEFAULT_ADMIN_USERNAME;
		userInfo.displayName_            = "Admin";
		userInfo.usernameWithLock_       = userInfo.username_;
		userInfo.userSessionIndex_ 		 = 0;
		return true;  // done, wizard mode access granted
	}
	// else, normal gateway verify mode

	if(xmldoc)  // fill with cookie code tag
	{
		if(userInfo.allowNoUser_)
			xmldoc->setHeader(WebUsers::REQ_ALLOW_NO_USER);
		else
			xmldoc->setHeader(userInfo.cookieCode_);
	}

	if(userInfo.allowNoUser_)
	{
		if(userInfo.automatedCommand_)
			__COUTT__ << "Allowing anonymous access." << __E__;

		return true;  // ignore lock for allow-no-user case
	}

	//	if(!userInfo.automatedCommand_)
	//	{
	//		__COUTV__(userInfo.username_);
	//		__COUTV__(userInfo.usernameWithLock_);
	//	}

	if((userInfo.checkLock_ || userInfo.requireLock_) && userInfo.usernameWithLock_ != "" && 
		userInfo.usernameWithLock_ != userInfo.username_)
	{
		*out << WebUsers::REQ_USER_LOCKOUT_RESPONSE;
		__COUT__ << "User '" << userInfo.username_ << "' is locked out. '" << userInfo.usernameWithLock_ << "' has lock." << std::endl;
		return false;  // failed due to another user having lock
	}

	if(userInfo.requireLock_ && userInfo.usernameWithLock_ != userInfo.username_)
	{
		*out << WebUsers::REQ_LOCK_REQUIRED_RESPONSE;
		__COUT__ << "User '" << userInfo.username_ << "' must have lock to proceed. ('" << userInfo.usernameWithLock_ << "' has lock.)" << std::endl;
		return false;  // failed due to lock being required, and this user does not have
		               // it
	}

	return true;  // access success!

}  // end checkRequestAccess()

//==============================================================================
// saveActiveSessions
//	save active sessions structure so that they can survive restart
void WebUsers::saveActiveSessions()
{
	std::string fn;

	fn = (std::string)WEB_LOGIN_DB_PATH + (std::string)USERS_ACTIVE_SESSIONS_FILE;
	__COUT__ << fn << __E__;

	FILE* fp = fopen(fn.c_str(), "w");
	if(!fp)
	{
		__COUT_ERR__ << "Error! Persistent active sessions could not be saved to file: " << fn << __E__;
		return;
	}

	int version = 0;
	fprintf(fp, "%d\n", version);
	for(unsigned int i = 0; i < ActiveSessions_.size(); ++i)
	{
		//		__COUT__ << "SAVE " << ActiveSessionCookieCodeVector[i] << __E__;
		//		__COUT__ << "SAVE " << ActiveSessionIpVector[i] << __E__;
		//		__COUT__ << "SAVE " << ActiveSessionUserIdVector[i] << __E__;
		//		__COUT__ << "SAVE " << ActiveSessionIndex[i] << __E__;
		//		__COUT__ << "SAVE " << ActiveSessionStartTimeVector[i] << __E__;

		fprintf(fp, "%s\n", ActiveSessions_[i].cookieCode_.c_str());
		fprintf(fp, "%s\n", ActiveSessions_[i].ip_.c_str());
		fprintf(fp, "%lu\n", ActiveSessions_[i].userId_);
		fprintf(fp, "%lu\n", ActiveSessions_[i].sessionIndex_);
		fprintf(fp, "%ld\n", ActiveSessions_[i].startTime_);
	}

	__COUT__ << "Active Sessions saved with size " << ActiveSessions_.size() << __E__;

	fclose(fp);
}  // end saveActiveSessions()

//====================================================================================================================
// loadActiveSessions
//	load active sessions structure so that they can survive restart
void WebUsers::loadActiveSessions()
{
	std::string fn;

	fn = (std::string)WEB_LOGIN_DB_PATH + (std::string)USERS_ACTIVE_SESSIONS_FILE;
	__COUT__ << fn << __E__;
	FILE* fp = fopen(fn.c_str(), "r");
	if(!fp)
	{
		__COUT_INFO__ << "Persistent active sessions were not found to be loaded at file: " << fn << __E__;
		return;
	}

	int version;

	const int LINELEN = 1000;
	char      line[LINELEN];
	fgets(line, LINELEN, fp);
	sscanf(line, "%d", &version);
	if(version == 0)
	{
		__COUT__ << "Extracting active sessions..." << __E__;
	}
	while(fgets(line, LINELEN, fp))
	{
		if(strlen(line))
			line[strlen(line) - 1] = '\0';  // remove new line
		if(strlen(line) != COOKIE_CODE_LENGTH)
		{
			__COUT__ << "Illegal cookie code found: " << line << __E__;

			fclose(fp);
			return;
		}
		ActiveSessions_.push_back(ActiveSession());
		ActiveSessions_.back().cookieCode_ = line;

		fgets(line, LINELEN, fp);
		if(strlen(line))
			line[strlen(line) - 1] = '\0';  // remove new line
		ActiveSessions_.back().ip_ = line;

		fgets(line, LINELEN, fp);
		sscanf(line, "%lu", &(ActiveSessions_.back().userId_));

		fgets(line, LINELEN, fp);
		sscanf(line, "%lu", &(ActiveSessions_.back().sessionIndex_));

		fgets(line, LINELEN, fp);
		sscanf(line, "%ld", &(ActiveSessions_.back().startTime_));
	}

	__COUT__ << "Active Sessions loaded with size " << ActiveSessions_.size() << __E__;

	fclose(fp);
	// clear file after loading
	fp = fopen(fn.c_str(), "w");
	if(fp)
		fclose(fp);
}  // end loadActiveSessions()

//==============================================================================
// loadDatabaseFromFile
//	load Hashes and Users from file
//	create database if non-existent
bool WebUsers::loadDatabases()
{
	std::string fn;

	FILE*              fp;
	const unsigned int LINE_LEN = 1000;
	char               line[LINE_LEN];
	unsigned int       i, si, c, len, f;
	// uint64_t           tmpInt64;

	// hashes
	//	File Organization:
	//		<hashData>
	//			<hashEntry><hash>hash0</hash><lastAccessTime>lastAccessTime0</lastAccessTime></hashEntry>
	//			<hashEntry><hash>hash1</hash><lastAccessTime>lastAccessTime1</lastAccessTime></hashEntry>
	//			..
	//		</hashData>

	fn = (std::string)WEB_LOGIN_DB_PATH + (std::string)HASHES_DB_FILE;
	__COUT__ << fn << __E__;
	fp = fopen(fn.c_str(), "r");
	if(!fp)  // need to create file
	{
		mkdir(((std::string)WEB_LOGIN_DB_PATH + (std::string)HASHES_DB_PATH).c_str(), 0755);
		__COUT__ << ((std::string)WEB_LOGIN_DB_PATH + (std::string)HASHES_DB_PATH).c_str() << __E__;
		fp = fopen(fn.c_str(), "w");
		if(!fp)
			return false;
		__COUT__ << "Hashes database created: " << fn << __E__;

		saveToDatabase(fp, HASHES_DB_GLOBAL_STRING, "", DB_SAVE_OPEN);
		saveToDatabase(fp, HASHES_DB_GLOBAL_STRING, "", DB_SAVE_CLOSE);
		fclose(fp);
	}
	else  // load structures if hashes exists
	{
		// for every HASHES_DB_ENTRY_STRING, extract to local vector
		// trusting file construction, assuming fields based >'s and <'s
		while(fgets(line, LINE_LEN, fp))
		{
			if(strlen(line) < SHA512_DIGEST_LENGTH)
				continue;

			c   = 0;
			len = strlen(line);  // save len, strlen will change because of \0 manipulations
			for(i = 0; i < len; ++i)
				if(line[i] == '>')
				{
					++c;  // count >'s
					if(c != 2 && c != 4)
						continue;  // only proceed for field data

					si = ++i;  // save start index
					while(i < len && line[i] != '<')
						++i;
					if(i == len)
						break;
					line[i] = '\0';  // close std::string

					//__COUT__ << "Found Hashes field " << c/2 << " " << &line[si] <<
					//__E__;

					f = c / 2 - 1;
					if(f == 0)  // hash
					{
						Hashes_.push_back(Hash());
						Hashes_.back().hash_ = &line[si];
					}
					else if(f == 1)  // lastAccessTime
						sscanf(&line[si], "%ld", &Hashes_.back().accessTime_);
				}
		}
		__COUT__ << Hashes_.size() << " Hashes found." << __E__;

		fclose(fp);
	}

	// users
	//	File Organization:
	//		<userData>
	//			<nextUserId>...</nextUserId>
	//			<userEntry>...</userEntry>
	//			<userEntry>...</userEntry>
	//			..
	//		</userData>

	fn = (std::string)WEB_LOGIN_DB_PATH + (std::string)USERS_DB_FILE;
	fp = fopen(fn.c_str(), "r");
	if(!fp)  // need to create file
	{
		mkdir(((std::string)WEB_LOGIN_DB_PATH + (std::string)USERS_DB_PATH).c_str(), 0755);
		__COUT__ << ((std::string)WEB_LOGIN_DB_PATH + (std::string)USERS_DB_PATH).c_str() << __E__;
		fp = fopen(fn.c_str(), "w");
		if(!fp)
			return false;
		__COUT__ << "Users database created: " << fn << __E__;

		saveToDatabase(fp, USERS_DB_GLOBAL_STRING, "", DB_SAVE_OPEN);
		char nidStr[100];
		sprintf(nidStr, "%lu", usersNextUserId_);
		saveToDatabase(fp, USERS_DB_NEXT_UID_STRING, nidStr, DB_SAVE_OPEN_AND_CLOSE);
		saveToDatabase(fp, USERS_DB_GLOBAL_STRING, "", DB_SAVE_CLOSE);
		fclose(fp);

		createNewAccount(DEFAULT_ADMIN_USERNAME, DEFAULT_ADMIN_DISPLAY_NAME,
		                 DEFAULT_ADMIN_EMAIL);  // account 0 is always admin
	}
	else  // extract next user id and user entries if users exists
	{
		__COUT__ << "Users database: " << fn << __E__;
		// for every USERS_DB_ENTRY_STRING, extract to local vector
		// trusting file construction, assuming fields based >'s and <'s

		char salt[] = "nextUserId";
		while(fgets(line, LINE_LEN, fp))
		{
			if(strlen(line) < strlen(salt) * 2)
				continue;  // line size	should indicate xml tags on same line

			for(i = 0; i < strlen(salt); ++i)  // check for opening tag
				if(line[i + 1] != salt[i])
					break;

			if(i == strlen(salt))  // all salt matched, so found correct line! increment
			                       // to get line index
			{
				i += 2;
				si = i;
				while(i < LINE_LEN && line[i] != '\0' && line[i] != '<')
					++i;         // find '<'
				line[i] = '\0';  // close std::string
				sscanf(&line[si], "%lu", &usersNextUserId_);
				break;  // done with next uid
			}
		}

		__COUT__ << "Found Users database next user Id: " << usersNextUserId_ << __E__;

		// trusting file construction, assuming fields based >'s and <'s and each entry on
		// one line
		while(fgets(line, LINE_LEN, fp))
		{
			if(strlen(line) < 30)
				continue;  // rule out header tags

			c   = 0;
			len = strlen(line);  // save len, strlen will change because of \0 manipulations
			if(len >= LINE_LEN)
			{
				__COUT__ << "Line buffer too small: " << len << __E__;
				break;
			}

			// get fields from line
			f = 0;
			for(i = 0; i < len; ++i)
				if(line[i] == '>')
				{
					++c;  // count >'s
					if(c == 0 || c % 2 == 1)
						continue;  // only proceed for field data (even

					si = ++i;  // save start index
					while(i < len && line[i] != '<')
						++i;
					if(i == len)
						break;
					line[i] = '\0';  // close std::string
					f       = c / 2 - 1;

					//__COUT__ << "Found Users[" <<
					//		Users_.size() << "] field " << f << " " << &line[si] << __E__;

					if(f == 0)  // username
					{
						Users_.push_back(User());
						Users_.back().username_ = &line[si];
					}
					else if(f == 1)  // displayName
						Users_.back().displayName_ = &line[si];
					else if(f == 2)  // salt
						Users_.back().salt_ = &line[si];
					else if(f == 3)  // uid
						sscanf(&line[si], "%lu", &Users_.back().userId_);
					else if(f == 4)  // permissions
					{
						std::map<std::string, permissionLevel_t>& lastPermissionsMap = Users_.back().permissions_;
						StringMacros::getMapFromString<permissionLevel_t>(&line[si], lastPermissionsMap);

						//__COUT__ << "User permission levels:" <<
						// StringMacros::mapToString(lastPermissionsMap) << __E__;

						// verify 'allUsers' is there
						//	if not, add it as a disabled user (i.e.
						// WebUsers::PERMISSION_LEVEL_INACTIVE)
						if(lastPermissionsMap.find(WebUsers::DEFAULT_USER_GROUP) == lastPermissionsMap.end())
						{
							__MCOUT_INFO__("User '" << Users_.back().username_ << "' is not a member of the default user group '"
							                        << WebUsers::DEFAULT_USER_GROUP
							                        << ".' Assuming user account is inactive (permission "
							                           "level := "
							                        << WebUsers::PERMISSION_LEVEL_INACTIVE << ")." << __E__);
							lastPermissionsMap[WebUsers::DEFAULT_USER_GROUP] = WebUsers::PERMISSION_LEVEL_INACTIVE;  // mark inactive
						}

						if(Users_.back().username_ == DEFAULT_ADMIN_USERNAME)
						{
							// overwrite admin with full permissions (irregardless of corrupt user db situation), never allow to be inactive for example

							std::map<std::string /*groupName*/, WebUsers::permissionLevel_t> initPermissions = {
							    {WebUsers::DEFAULT_USER_GROUP, WebUsers::PERMISSION_LEVEL_ADMIN}};

							Users_.back().permissions_ = initPermissions;
						}
					}
					else if(f == 5)  // lastLoginAttemptTime
						sscanf(&line[si], "%ld", &Users_.back().lastLoginAttempt_);
					else if(f == 6)  // accountCreatedTime
						sscanf(&line[si], "%ld", &Users_.back().accountCreationTime_);
					else if(f == 7)  // loginFailureCount
						sscanf(&line[si], "%hhu", &Users_.back().loginFailureCount_);
					else if(f == 8)  // lastModifierTime
						sscanf(&line[si], "%ld", &Users_.back().accessModifierTime());
					else if(f == 9)  // lastModifierUsername
						Users_.back().loadModifierUsername(&line[si]);
					else if(f == 10)  // user email
						Users_.back().email_ = &line[si];
				}

		}  // end get line loop
		fclose(fp);
	}

	__COUT__ << Users_.size() << " Users found." << __E__;
	for(size_t ii = 0; ii < Users_.size(); ++ii)
	{
		std::cout <<  // do not send to message facility
		    "User [" << Users_[ii].userId_ << "] \tName: " << std::left << std::setfill(' ') << std::setw(20) << Users_[ii].username_
		          << "\tDisplay Name: " << std::left << std::setfill(' ') << std::setw(30) << Users_[ii].displayName_ << "\tEmail: " << std::left
		          << std::setfill(' ') << std::setw(30) << Users_[ii].email_ << "\tNAC: " << std::left << std::setfill(' ') << std::setw(5)
		          << Users_[ii].getNewAccountCode() << "\tFailedCount: " << (int)Users_[ii].loginFailureCount_
		          << "\tPermissions: " << StringMacros::mapToString(Users_[ii].permissions_) <<
		    //"\tSalt: " << Users_[ii].salt_.size() << " " <<  Users_[ii].salt_ <<
		    __E__;
	}
	//	__COUT__ << Hashes_.size() << " Hashes found." << __E__;
	//	for(size_t ii = 0; ii < Hashes_.size(); ++ii)
	//	{
	//		std::cout << //do not send to message facility
	//				"Hash [" << ii <<
	//				"]: " << Hashes_[ii].hash_ <<
	//				__E__;
	//	}
	return true;
}  // end loadDatabases()

//==============================================================================
// saveToDatabase
void WebUsers::saveToDatabase(FILE* fp, const std::string& field, const std::string& value, uint8_t type, bool addNewLine)
{
	if(!fp)
		return;

	std::string newLine = addNewLine ? "\n" : "";

	if(type == DB_SAVE_OPEN_AND_CLOSE)
		fprintf(fp, "<%s>%s</%s>%s", field.c_str(), value.c_str(), field.c_str(), newLine.c_str());
	else if(type == DB_SAVE_OPEN)
		fprintf(fp, "<%s>%s%s", field.c_str(), value.c_str(), newLine.c_str());
	else if(type == DB_SAVE_CLOSE)
		fprintf(fp, "</%s>%s", field.c_str(), newLine.c_str());
}  // end saveToDatabase()

//==============================================================================
// saveDatabaseToFile
//	returns true if saved database successfully
//		db: DB_USERS or DB_HASHES
//	else false

bool WebUsers::saveDatabaseToFile(uint8_t db)
{
	//__COUT__ << "Save Database: " << (int)db << __E__;

	std::string fn = (std::string)WEB_LOGIN_DB_PATH + ((db == DB_USERS) ? (std::string)USERS_DB_FILE : (std::string)HASHES_DB_FILE);

	__COUT__ << "Save Database Filename: " << fn << __E__;

	// backup file organized by day
	if(0)
	{
		char dayAppend[20];
		sprintf(dayAppend, ".%lu.bkup", time(0) / (3600 * 24));
		std::string bkup_fn = (std::string)WEB_LOGIN_DB_PATH + (std::string)WEB_LOGIN_BKUP_DB_PATH +
		                      ((db == DB_USERS) ? (std::string)USERS_DB_FILE : (std::string)HASHES_DB_FILE) + (std::string)dayAppend;

		__COUT__ << "Backup file: " << bkup_fn << __E__;

		std::string shell_command = "mv " + fn + " " + bkup_fn;
		system(shell_command.c_str());
	}

	FILE* fp = fopen(fn.c_str(), "wb");  // write in binary mode
	if(!fp)
		return false;

	char fldStr[100];

	if(db == DB_USERS)  // USERS
	{
		saveToDatabase(fp, USERS_DB_GLOBAL_STRING, "", DB_SAVE_OPEN);

		sprintf(fldStr, "%lu", usersNextUserId_);
		saveToDatabase(fp, USERS_DB_NEXT_UID_STRING, fldStr, DB_SAVE_OPEN_AND_CLOSE);

		__COUT__ << "Saving " << Users_.size() << " Users." << __E__;

		for(uint64_t i = 0; i < Users_.size(); ++i)
		{
			//__COUT__ << "Saving User: " << UsersUsernameVector[i] << __E__;

			saveToDatabase(fp, USERS_DB_ENTRY_STRING, "", DB_SAVE_OPEN, false);

			for(unsigned int f = 0; f < WebUsers::UsersDatabaseEntryFields_.size(); ++f)
			{
				//__COUT__ << "Saving Field: " << f << __E__;
				if(f == 0)  // username
					saveToDatabase(fp, WebUsers::UsersDatabaseEntryFields_[f], Users_[i].username_, DB_SAVE_OPEN_AND_CLOSE, false);
				else if(f == 1)  // displayName
					saveToDatabase(fp, WebUsers::UsersDatabaseEntryFields_[f], Users_[i].displayName_, DB_SAVE_OPEN_AND_CLOSE, false);
				else if(f == 2)  // salt
					saveToDatabase(fp, WebUsers::UsersDatabaseEntryFields_[f], Users_[i].salt_, DB_SAVE_OPEN_AND_CLOSE, false);
				else if(f == 3)  // uid
				{
					sprintf(fldStr, "%lu", Users_[i].userId_);
					saveToDatabase(fp, WebUsers::UsersDatabaseEntryFields_[f], fldStr, DB_SAVE_OPEN_AND_CLOSE, false);
				}
				else if(f == 4)  // permissions
					saveToDatabase(fp,
					               WebUsers::UsersDatabaseEntryFields_[f],
					               StringMacros::mapToString(Users_[i].permissions_, "," /*primary delimeter*/, ":" /*secondary delimeter*/),
					               DB_SAVE_OPEN_AND_CLOSE,
					               false);
				else if(f == 5)  // lastLoginAttemptTime
				{
					sprintf(fldStr, "%lu", Users_[i].lastLoginAttempt_);
					saveToDatabase(fp, WebUsers::UsersDatabaseEntryFields_[f], fldStr, DB_SAVE_OPEN_AND_CLOSE, false);
				}
				else if(f == 6)  // accountCreatedTime
				{
					sprintf(fldStr, "%lu", Users_[i].accountCreationTime_);
					saveToDatabase(fp, WebUsers::UsersDatabaseEntryFields_[f], fldStr, DB_SAVE_OPEN_AND_CLOSE, false);
				}
				else if(f == 7)  // loginFailureCount
				{
					sprintf(fldStr, "%hhu", Users_[i].loginFailureCount_);
					saveToDatabase(fp, WebUsers::UsersDatabaseEntryFields_[f], fldStr, DB_SAVE_OPEN_AND_CLOSE, false);
				}
				else if(f == 8)  // lastModifierTime
				{
					sprintf(fldStr, "%lu", Users_[i].getModifierTime());
					saveToDatabase(fp, WebUsers::UsersDatabaseEntryFields_[f], fldStr, DB_SAVE_OPEN_AND_CLOSE, false);
				}
				else if(f == 9)  // lastModifierUsername
					saveToDatabase(fp, WebUsers::UsersDatabaseEntryFields_[f], Users_[i].getModifierUsername(), DB_SAVE_OPEN_AND_CLOSE, false);
				else if(f == 10)  // useremail
					saveToDatabase(fp, WebUsers::UsersDatabaseEntryFields_[f], Users_[i].email_, DB_SAVE_OPEN_AND_CLOSE, false);
			}

			saveToDatabase(fp, USERS_DB_ENTRY_STRING, "", DB_SAVE_CLOSE);
		}

		saveToDatabase(fp, USERS_DB_GLOBAL_STRING, "", DB_SAVE_CLOSE);
	}
	else  // HASHES
	{
		saveToDatabase(fp, HASHES_DB_GLOBAL_STRING, "", DB_SAVE_OPEN);

		__COUT__ << "Saving " << Hashes_.size() << " Hashes." << __E__;
		for(uint64_t i = 0; i < Hashes_.size(); ++i)
		{
			__COUT__ << "Saving " << Hashes_[i].hash_ << " Hash." << __E__;
			saveToDatabase(fp, HASHES_DB_ENTRY_STRING, "", DB_SAVE_OPEN, false);
			for(unsigned int f = 0; f < WebUsers::HashesDatabaseEntryFields_.size(); ++f)
			{
				if(f == 0)  // hash
					saveToDatabase(fp, WebUsers::HashesDatabaseEntryFields_[f], Hashes_[i].hash_, DB_SAVE_OPEN_AND_CLOSE, false);
				else if(f == 1)  // lastAccessTime
				{
					sprintf(fldStr, "%lu", Hashes_[i].accessTime_);
					saveToDatabase(fp, WebUsers::HashesDatabaseEntryFields_[f], fldStr, DB_SAVE_OPEN_AND_CLOSE, false);
				}
			}
			saveToDatabase(fp, HASHES_DB_ENTRY_STRING, "", DB_SAVE_CLOSE);
		}

		saveToDatabase(fp, HASHES_DB_GLOBAL_STRING, "", DB_SAVE_CLOSE);
	}

	fclose(fp);
	return true;
}  // end saveDatabaseToFile()

//==============================================================================
// createNewAccount
//	adds a new valid user to database
//		inputs: username and name to display
//		initializes database entry with minimal permissions
//			and salt starts as "" until password is set
//		Special case if first user name!! max permissions given (super user made)
// //Note: username, userId, AND displayName must be unique!
void WebUsers::createNewAccount(const std::string& username, const std::string& displayName, const std::string& email)
{
	__COUT__ << "Creating account: " << username << __E__;
	// check if username already exists
	uint64_t i;
	if((i = searchUsersDatabaseForUsername(username)) != NOT_FOUND_IN_DATABASE || username == WebUsers::DEFAULT_ITERATOR_USERNAME ||
	   username == WebUsers::DEFAULT_STATECHANGER_USERNAME)  // prevent reserved usernames
	                                                         // from being created!
	{
		__SS__ << "Username '" << username << "' already exists! Please choose a unique username." << __E__;
		__SS_THROW__;
	}

	// enforce unique Display Name
	if((i = searchUsersDatabaseForDisplayName(displayName)) != NOT_FOUND_IN_DATABASE)
	// from being created!
	{
		__SS__ << "Display Name '" << displayName << "' already exists! Please choose a unique display name." << __E__;
		__SS_THROW__;
	}

	// create Users database entry
	Users_.push_back(User());

	Users_.back().username_    = username;
	Users_.back().displayName_ = displayName;
	Users_.back().email_       = email;

	// first user is admin always!
	std::map<std::string /*groupName*/, WebUsers::permissionLevel_t> initPermissions = {
	    {WebUsers::DEFAULT_USER_GROUP, (Users_.size() ? WebUsers::PERMISSION_LEVEL_NOVICE : WebUsers::PERMISSION_LEVEL_ADMIN)}};

	Users_.back().permissions_ = initPermissions;
	Users_.back().userId_      = usersNextUserId_++;
	if(usersNextUserId_ >= ACCOUNT_ERROR_THRESHOLD)  // error wrap around case
	{
		__SS__ << "usersNextUserId_ wrap around!! Too many users??? Notify Admins." << __E__;
		__SS_THROW__;
		usersNextUserId_ = 1;  // for safety to avoid weird issues at -1 and 0 (if used
		                       // for error indication)
	}

	Users_.back().accountCreationTime_ = time(0);

	if(!saveDatabaseToFile(DB_USERS))
	{
		__SS__ << "Failed to save User DB!" << __E__;
		__SS_THROW__;
	}
}  // end createNewAccount()

//==============================================================================
// deleteAccount
//	private function, deletes user account
//		inputs: username and name to display
//		if username and display name match account found, then account is deleted and true
// returned 		else false
bool WebUsers::deleteAccount(const std::string& username, const std::string& displayName)
{
	uint64_t i = searchUsersDatabaseForUsername(username);
	if(i == NOT_FOUND_IN_DATABASE)
		return false;
	if(Users_[i].displayName_ != displayName)
		return false;  // display name does not match

	// delete entry from user database vector

	Users_.erase(Users_.begin() + i);

	// save database
	return saveDatabaseToFile(DB_USERS);
}  // end deleteAccount()

//==============================================================================
unsigned int WebUsers::hexByteStrToInt(const char* h)
{
	unsigned int rv;
	char         hs[3] = {h[0], h[1], '\0'};
	sscanf(hs, "%X", &rv);
	return rv;
}  // end hexByteStrToInt()

//==============================================================================
void WebUsers::intToHexStr(unsigned char i, char* h) { sprintf(h, "%2.2X", i); }

//==============================================================================
// WebUsers::attemptActiveSession ---
//	Attempts login.
//
//	If new login, then new account code must match account creation time and account is
// made with pw
//
//	if old login, password is checked
//	returns User Id, cookieCode in newAccountCode, and displayName in jumbledUser on
// success 	else returns -1 and cookieCode "0"
uint64_t WebUsers::attemptActiveSession(
    const std::string& uuid, std::string& jumbledUser, const std::string& jumbledPw, std::string& newAccountCode, const std::string& ip)
{
	//__COUTV__(ip);
	if(!checkIpAccess(ip))
	{
		__COUT_ERR__ << "rejected ip: " << ip << __E__;
		return ACCOUNT_BLACKLISTED;
	}

	cleanupExpiredEntries();  // remove expired active and login sessions

	if(!CareAboutCookieCodes_)  // NO SECURITY
	{
		uint64_t uid   = getAdminUserID();
		jumbledUser    = getUsersDisplayName(uid);
		newAccountCode = genCookieCode();  // return "dummy" cookie code by reference
		return uid;
	}

	uint64_t i;

	// search login sessions for uuid
	if((i = searchLoginSessionDatabaseForUUID(uuid)) == NOT_FOUND_IN_DATABASE)
	{
		__COUT_ERR__ << "uuid: " << uuid << " is not found" << __E__;
		newAccountCode = "1";  // to indicate uuid was not found

		incrementIpBlacklistCount(ip);  // increment ip blacklist counter

		return NOT_FOUND_IN_DATABASE;
	}
	++LoginSessions_[i].loginAttempts_;

	std::string user = dejumble(jumbledUser, LoginSessions_[i].id_);
	__COUTV__(user);
	std::string pw = dejumble(jumbledPw, LoginSessions_[i].id_);

	// search users for username
	if((i = searchUsersDatabaseForUsername(user)) == NOT_FOUND_IN_DATABASE)
	{
		__COUT_ERR__ << "user: " << user << " is not found" << __E__;

		incrementIpBlacklistCount(ip);  // increment ip blacklist counter

		return NOT_FOUND_IN_DATABASE;
	}
	else
		ipBlacklistCounts_[ip] = 0;  // clear blacklist count

	Users_[i].lastLoginAttempt_ = time(0);

	if(isInactiveForGroup(Users_[i].permissions_))
	{
		__MCOUT_ERR__("User '" << user << "' account INACTIVE (could be due to failed logins)" << __E__);
		return ACCOUNT_INACTIVE;
	}

	if(Users_[i].salt_ == "")  // first login
	{
		__MCOUT__("First login attempt for user: " << user << __E__);

		if(newAccountCode != Users_[i].getNewAccountCode())
		{
			__COUT__ << "New account code did not match: " << Users_[i].getNewAccountCode() << " != " << newAccountCode << __E__;
			saveDatabaseToFile(DB_USERS);  // users db modified, so save
			return NOT_FOUND_IN_DATABASE;
		}

		// initial user account setup

		// add until no collision (should 'never' be a collision)
		while(!addToHashesDatabase(sha512(user, pw, Users_[i].salt_)))  // sha256 modifies UsersSaltVector[i]
		{
			// this should never happen, it would mean the user+pw+saltcontext was the
			// same
			// but if it were to happen, try again...
			Users_[i].salt_ = "";
		}

		__COUT__ << "\tHash added: " << Hashes_.back().hash_ << __E__;
	}
	else
	{
		std::string salt = Users_[i].salt_;  // don't want to modify saved salt
		//__COUT__ << salt.size() << " " << salt << " " << i << __E__;
		if(searchHashesDatabaseForHash(sha512(user, pw, salt)) == NOT_FOUND_IN_DATABASE)
		{
			__COUT__ << "Failed login for " << user << " with permissions " << StringMacros::mapToString(Users_[i].permissions_) << __E__;

			// do not allow wrap around
			if(++Users_[i].loginFailureCount_ != (unsigned char)-1)
				++Users_[i].loginFailureCount_;

			if(Users_[i].loginFailureCount_ >= USERS_MAX_LOGIN_FAILURES)
				Users_[i].permissions_[WebUsers::DEFAULT_USER_GROUP] = WebUsers::PERMISSION_LEVEL_INACTIVE;  // Lock account

			__COUT_INFO__ << "User/pw for user '" << user << "' was not correct (Failed Attempt #" << (int)Users_[i].loginFailureCount_ << " of "
			              << (int)USERS_MAX_LOGIN_FAILURES << " allowed)." << __E__;

			__COUTV__(isInactiveForGroup(Users_[i].permissions_));
			if(isInactiveForGroup(Users_[i].permissions_))
				__MCOUT_INFO__("Account '" << user
				                           << "' has been marked inactive due to too many failed "
				                              "login attempts (Failed Attempt #"
				                           << (int)Users_[i].loginFailureCount_ << ")! Note only admins can reactivate accounts." << __E__);

			saveDatabaseToFile(DB_USERS);  // users db modified, so save
			return NOT_FOUND_IN_DATABASE;
		}
	}

	__MCOUT_INFO__("Login successful for: " << user << __E__);

	Users_[i].loginFailureCount_ = 0;

	// record to login history for user (h==0) and on global server level (h==1)
	for(int h = 0; h < 2; ++h)
	{
		std::string fn = (std::string)WEB_LOGIN_DB_PATH + (std::string)USERS_LOGIN_HISTORY_PATH + (h ? USERS_GLOBAL_HISTORY_FILE : Users_[i].username_) + "." +
		                 (std::string)USERS_LOGIN_HISTORY_FILETYPE;

		HttpXmlDocument histXml;

		if(histXml.loadXmlDocument(fn))  // not found
		{
			while(histXml.getChildrenCount() + 1 > (h ? USERS_GLOBAL_HISTORY_SIZE : USERS_LOGIN_HISTORY_SIZE))
				histXml.removeDataElement();
		}
		else
			__COUT__ << "No previous login history found." << __E__;

		// add new entry to history
		char entryStr[500];
		if(h)
			sprintf(entryStr,
			        "Time=%lu Username=%s Permissions=%s UID=%lu",
			        time(0),
			        Users_[i].username_.c_str(),
			        StringMacros::mapToString(Users_[i].permissions_).c_str(),
			        Users_[i].userId_);
		else
			sprintf(entryStr,
			        "Time=%lu displayName=%s Permissions=%s UID=%lu",
			        time(0),
			        Users_[i].displayName_.c_str(),
			        StringMacros::mapToString(Users_[i].permissions_).c_str(),
			        Users_[i].userId_);
		histXml.addTextElementToData(PREF_XML_LOGIN_HISTORY_FIELD, entryStr);

		// save file
		histXml.saveXmlDocument(fn);
	}

	// SUCCESS!!
	saveDatabaseToFile(DB_USERS);             // users db modified, so save
	jumbledUser    = Users_[i].displayName_;  // pass by reference displayName
	newAccountCode = createNewActiveSession(Users_[i].userId_,
	                                        ip);  // return cookie code by reference

	if(ActiveSessions_.size() == 1)  // if only one user, then attempt to take lock for user friendliness
	{
		__COUT__ << "Attempting to auto-lock for first login user '" << Users_[i].username_ << "'... " << __E__;
		setUserWithLock(Users_[i].userId_, true /*lock*/, Users_[i].username_);
	}

	return Users_[i].userId_;  // return user Id
}  // end attemptActiveSession()

//==============================================================================
// WebUsers::attemptActiveSessionWithCert ---
//	Attempts login using certificate.
//
//	returns User Id, cookieCode, and displayName in jumbledEmail on success
//	else returns -1 and cookieCode "0"
uint64_t WebUsers::attemptActiveSessionWithCert(const std::string& uuid, std::string& email, std::string& cookieCode, std::string& user, const std::string& ip)
{
	if(!checkIpAccess(ip))
	{
		__COUT_ERR__ << "rejected ip: " << ip << __E__;
		return NOT_FOUND_IN_DATABASE;
	}

	cleanupExpiredEntries();  // remove expired active and login sessions

	if(!CareAboutCookieCodes_)  // NO SECURITY
	{
		uint64_t uid = getAdminUserID();
		email        = getUsersDisplayName(uid);
		cookieCode   = genCookieCode();  // return "dummy" cookie code by reference
		return uid;
	}

	if(email == "")
	{
		__COUT__ << "Rejecting cert logon with blank fingerprint" << __E__;

		incrementIpBlacklistCount(ip);  // increment ip blacklist counter

		return NOT_FOUND_IN_DATABASE;
	}

	uint64_t i;

	// search login sessions for uuid
	if((i = searchLoginSessionDatabaseForUUID(uuid)) == NOT_FOUND_IN_DATABASE)
	{
		__COUT__ << "uuid: " << uuid << " is not found" << __E__;
		cookieCode = "1";  // to indicate uuid was not found

		incrementIpBlacklistCount(ip);  // increment ip blacklist counter

		return NOT_FOUND_IN_DATABASE;
	}
	++LoginSessions_[i].loginAttempts_;

	email = getUserEmailFromFingerprint(email);
	__COUT__ << "DejumbledEmail = " << email << __E__;
	if(email == "")
	{
		__COUT__ << "Rejecting logon with unknown fingerprint" << __E__;

		incrementIpBlacklistCount(ip);  // increment ip blacklist counter

		return NOT_FOUND_IN_DATABASE;
	}

	// search users for username
	if((i = searchUsersDatabaseForUserEmail(email)) == NOT_FOUND_IN_DATABASE)
	{
		__COUT__ << "email: " << email << " is not found" << __E__;

		incrementIpBlacklistCount(ip);  // increment ip blacklist counter

		return NOT_FOUND_IN_DATABASE;
	}
	else
		ipBlacklistCounts_[ip] = 0;  // clear blacklist count

	user = getUsersUsername(i);

	Users_[i].lastLoginAttempt_ = time(0);
	if(isInactiveForGroup(Users_[i].permissions_))
	{
		__MCOUT__("User '" << user << "' account INACTIVE (could be due to failed logins)." << __E__);
		return NOT_FOUND_IN_DATABASE;
	}

	if(Users_[i].salt_ == "")  // Can't be first login
	{
		return NOT_FOUND_IN_DATABASE;
	}

	__MCOUT__("Login successful for: " << user << __E__);

	Users_[i].loginFailureCount_ = 0;

	// record to login history for user (h==0) and on global server level (h==1)
	for(int h = 0; h < 2; ++h)
	{
		std::string fn = (std::string)WEB_LOGIN_DB_PATH + (std::string)USERS_LOGIN_HISTORY_PATH + (h ? USERS_GLOBAL_HISTORY_FILE : Users_[i].username_) + "." +
		                 (std::string)USERS_LOGIN_HISTORY_FILETYPE;

		HttpXmlDocument histXml;

		if(histXml.loadXmlDocument(fn))  // not found
		{
			while(histXml.getChildrenCount() + 1 > (h ? USERS_GLOBAL_HISTORY_SIZE : USERS_LOGIN_HISTORY_SIZE))
				histXml.removeDataElement();
		}
		else
			__COUT__ << "No previous login history found." << __E__;

		// add new entry to history
		char entryStr[500];
		if(h)
			sprintf(entryStr,
			        "Time=%lu Username=%s Permissions=%s UID=%lu",
			        time(0),
			        Users_[i].username_.c_str(),
			        StringMacros::mapToString(Users_[i].permissions_).c_str(),
			        Users_[i].userId_);
		else
			sprintf(entryStr,
			        "Time=%lu displayName=%s Permissions=%s UID=%lu",
			        time(0),
			        Users_[i].displayName_.c_str(),
			        StringMacros::mapToString(Users_[i].permissions_).c_str(),
			        Users_[i].userId_);
		histXml.addTextElementToData(PREF_XML_LOGIN_HISTORY_FIELD, entryStr);

		// save file
		histXml.saveXmlDocument(fn);
	}

	// SUCCESS!!
	saveDatabaseToFile(DB_USERS);         // users db modified, so save
	email      = Users_[i].displayName_;  // pass by reference displayName
	cookieCode = createNewActiveSession(Users_[i].userId_,
	                                    ip);  // return cookie code by reference
	return Users_[i].userId_;                 // return user Id
}  // end attemptActiveSessionWithCert()

//==============================================================================
// WebUsers::searchActiveSessionDatabaseForUID ---
//	returns index if found, else -1
uint64_t WebUsers::searchActiveSessionDatabaseForCookie(const std::string& cookieCode) const
{
	uint64_t i = 0;
	for(; i < ActiveSessions_.size(); ++i)
		if(ActiveSessions_[i].cookieCode_ == cookieCode)
			break;
	return (i == ActiveSessions_.size()) ? NOT_FOUND_IN_DATABASE : i;
} //end searchActiveSessionDatabaseForCookie()

// //==============================================================================
// // WebUsers::searchRemoteSessionDatabaseForUsername ---
// //	returns index if found, else -1
// uint64_t WebUsers::searchRemoteSessionDatabaseForUsername(const std::string& username) const
// {
// 	for(const auto& remoteSession : RemoteSessions_)
// 		if(remoteSession.second.second.username_ == username)
// 			return remoteSession.first;
// 	return NOT_FOUND_IN_DATABASE;
// } //end searchRemoteSessionDatabaseForUsername()

//==============================================================================
// WebUsers::checkRemoteLoginVerification ---
//	checks over remote socket
//	returns userId if login verified, else -1
uint64_t WebUsers::checkRemoteLoginVerification(const std::string& cookieCode, bool refresh, const std::string& ip)
{
	__COUTVS__(2,cookieCode);
	remoteLoginVerificationEnabledBlackoutTime_ = 0;
	if(!remoteLoginVerificationSocket_)  //instantiate socket first time needed
	{
		if(!remoteLoginVerificationPort_)
		{
			__SS__ << "Illegal remote login verification port found in remote destination " << remoteLoginVerificationIP_ << ":" <<
				remoteLoginVerificationPort_ << ". Please check remote settings." << __E__;
			__SS_THROW__;
		}
		__COUT_INFO__ << "Instantiating Remote Gateway login verifification socket! Validation requests will go to " << 
			remoteLoginVerificationIP_ << ":" << remoteLoginVerificationPort_ << __E__;						
		
		remoteLoginVerificationSocket_ = std::make_unique<TransceiverSocket>(remoteLoginVerificationIP_);
		remoteLoginVerificationSocket_->initialize();

		remoteLoginVerificationSocketTarget_ = std::make_unique<Socket>(remoteLoginVerificationIP_,remoteLoginVerificationPort_);		
	}

	//check if cookie code is cached locally
	cleanupExpiredRemoteEntries();  // remove expired cookies
	auto it = RemoteSessions_.find(cookieCode);
	if(it != RemoteSessions_.end()) //then found cached cookie code
	{
		__COUTT__ << "cookieCode still active locally!" << __E__;
		return it->second.userId_;
	}
	//else ask Remote server to verify login

	// Send these parameters:
	// command = loginVerify
	// parameters.addParameter("CookieCode");
	// parameters.addParameter("RefreshOption");
	// parameters.addParameter("IPAddress");

	std::string request = "loginVerify," + cookieCode + "," + 
		(refresh?"1":"0") + "," + ip;

	__COUTV__(request);
	
	std::string requestResponseString = remoteLoginVerificationSocket_->sendAndReceive(*remoteLoginVerificationSocketTarget_, request, 10 /*timeoutSeconds*/);
	__COUTV__(requestResponseString);

	//from response... extract refreshedCookieCode, permissions, userWithLock, username, and display name
	std::vector<std::string> rxParams = StringMacros::getVectorFromString(requestResponseString);
	__COUTV__(StringMacros::vectorToString(rxParams));

	if(rxParams.size() != 6)
	{
		__COUT__ << "Remote login response indicates rejected: " << rxParams.size() << __E__;
		return NOT_FOUND_IN_DATABASE;
	}
	//else valid remote login! so create active remote session object

	// Receive these parameters
	// 0: retParameters.addParameter("CookieCode", cookieCode);
	// 1: retParameters.addParameter("Permissions", StringMacros::mapToString(userGroupPermissionsMap).c_str());
	// 2: retParameters.addParameter("UserWithLock", userWithLock);
	// 3: retParameters.addParameter("Username", theWebUsers_.getUsersUsername(uid));
	// 4: retParameters.addParameter("DisplayName", theWebUsers_.getUsersDisplayName(uid));	
	// 5: retParameters.addParameter("UserSessionIndex", td::to_string(userSessionIndex)); 

	if(rxParams[2] != "" && 
		usersUsernameWithLock_ != rxParams[2])
	{
		__COUT_INFO__ << "Overriding local user-with-lock '" << usersUsernameWithLock_ << 
			"' with remote user-with-lock 'Remote:" << rxParams[2] << "'" << __E__;
		usersUsernameWithLock_ = rxParams[2];
	}

	//search for an existing matching username, otherwise create
	std::string username = rxParams[3];
	__COUTTV__(username);	
	uint64_t j = searchUsersDatabaseForUsername(username);
	if(j == NOT_FOUND_IN_DATABASE)
	{
		__COUT_INFO__ << "Creating User entry for remote user '" << username << "' in local user list to track user preferences." << __E__;		
		createNewAccount(username, rxParams[4] /* displayName */, "" /* email */);
		j = Users_.size()-1;		
	}

	Users_[j].lastLoginAttempt_ = time(0);	
	Users_[j].setModifier("REMOTE_GATEWAY");

	//take permissions from remote source always, it overrides existing local user settings (and will force changes to local user db)
	__COUTV__(StringMacros::decodeURIComponent(rxParams[1]));
	Users_[j].permissions_.clear(); //otherwise collissions could occur in getMapFromString()
	StringMacros::getMapFromString(StringMacros::decodeURIComponent(rxParams[1]), 
		Users_[j].permissions_);
	__COUTV__(StringMacros::mapToString(Users_[j].permissions_));

	//fill in Remote Session and User info to cache for next login attempt

	ActiveSession& newRemoteSession = RemoteSessions_[rxParams[0]]; //construct remote ActiveSession
	newRemoteSession.cookieCode_ = rxParams[0];
	newRemoteSession.ip_ = ip;
	newRemoteSession.userId_ = Users_[j].userId_;
	sscanf(rxParams[5].c_str(),"%lu", &newRemoteSession.sessionIndex_);
	newRemoteSession.startTime_ = time(0);	

	return Users_[j].userId_;
} //end checkRemoteLoginVerification()

//==============================================================================
// WebUsers::isUsernameActive ---
//	returns true if found, else false
bool WebUsers::isUsernameActive(const std::string& username) const
{
	uint64_t u;
	if((u = searchUsersDatabaseForUsername(username)) == NOT_FOUND_IN_DATABASE)
		return false;
	return isUserIdActive(Users_[u].userId_);
} //end isUsernameActive()

//==============================================================================
// WebUsers::isUserIdActive ---
//	returns true if found, else false
bool WebUsers::isUserIdActive(uint64_t uid) const
{
	__COUTT__ << "isUserIdActive? " << uid << __E__;
	if(remoteLoginVerificationEnabled_) //first check remote sessions
	{
		for(const auto& remoteSession : RemoteSessions_)
			if(remoteSession.second.userId_ == uid)
				return true;
	} //end remote session checkion	

	uint64_t i = 0;
	for(; i < ActiveSessions_.size(); ++i)
		if(ActiveSessions_[i].userId_ == uid)
			return true;
	return false;
}  // end isUserIdActive()

//==============================================================================
// WebUsers::searchUsersDatabaseForUsername ---
//	returns index if found, else -1
uint64_t WebUsers::searchUsersDatabaseForUsername(const std::string& username) const
{
	uint64_t i = 0;
	for(; i < Users_.size(); ++i)
		if(Users_[i].username_ == username)
			break;
	return (i == Users_.size()) ? NOT_FOUND_IN_DATABASE : i;
}  // end searchUsersDatabaseForUsername()

//==============================================================================
// WebUsers::searchUsersDatabaseForDisplayName ---
//	returns index if found, else -1
uint64_t WebUsers::searchUsersDatabaseForDisplayName(const std::string& displayName) const
{
	uint64_t i = 0;
	for(; i < Users_.size(); ++i)
		if(Users_[i].displayName_ == displayName)
			break;
	return (i == Users_.size()) ? NOT_FOUND_IN_DATABASE : i;
}  // end searchUsersDatabaseForUsername()

//==============================================================================
// WebUsers::searchUsersDatabaseForUserEmail ---
//	returns index if found, else -1
uint64_t WebUsers::searchUsersDatabaseForUserEmail(const std::string& useremail) const
{
	uint64_t i = 0;
	for(; i < Users_.size(); ++i)
		if(Users_[i].email_ == useremail)
			break;
	return (i == Users_.size()) ? NOT_FOUND_IN_DATABASE : i;
}  // end searchUsersDatabaseForUserEmail()

//==============================================================================
// WebUsers::searchUsersDatabaseForUserId ---
//	returns index if found, else -1
uint64_t WebUsers::searchUsersDatabaseForUserId(uint64_t uid) const
{
	uint64_t i = 0;
	for(; i < Users_.size(); ++i)
		if(Users_[i].userId_ == uid)
			break;
	return (i == Users_.size()) ? NOT_FOUND_IN_DATABASE : i;
}  // end searchUsersDatabaseForUserId();

//==============================================================================
// WebUsers::searchLoginSessionDatabaseForUUID ---
//	returns index if found, else -1
uint64_t WebUsers::searchLoginSessionDatabaseForUUID(const std::string& uuid) const
{
	uint64_t i = 0;
	for(; i < LoginSessions_.size(); ++i)
		if(LoginSessions_[i].uuid_ == uuid)
			break;
	return (i == LoginSessions_.size()) ? NOT_FOUND_IN_DATABASE : i;
}  // end searchLoginSessionDatabaseForUUID()

//==============================================================================
// WebUsers::searchHashesDatabaseForHash ---
//	returns index if found, else -1
uint64_t WebUsers::searchHashesDatabaseForHash(const std::string& hash)
{
	uint64_t i = 0;
	//__COUT__ << i << " " << Hashes_.size() << " " << hash << __E__;
	for(; i < Hashes_.size(); ++i)
		if(Hashes_[i].hash_ == hash)
			break;
	// else
	//	__COUT__ << HashesVector[i] << " ?????? " << __E__;
	//__COUT__ << i << __E__;
	if(i < Hashes_.size())  // if found, means login successful, so update access time
		Hashes_[i].accessTime_ = ((time(0) + (rand() % 2 ? 1 : -1) * (rand() % 30 * 24 * 60 * 60)) & 0x0FFFFFFFFFE000000);
	// else
	//	__COUT__ << "No matching hash..." << __E__;

	//__COUT__ << i << __E__;
	return (i == Hashes_.size()) ? NOT_FOUND_IN_DATABASE : i;
}  // end searchHashesDatabaseForHash()

//==============================================================================
// WebUsers::addToHashesDatabase ---
//	returns false if hash already exists
//	else true for success
bool WebUsers::addToHashesDatabase(const std::string& hash)
{
	if(searchHashesDatabaseForHash(hash) != NOT_FOUND_IN_DATABASE)
	{
		__COUT__ << "Hash collision: " << hash << __E__;
		return false;
	}
	Hashes_.push_back(Hash());
	Hashes_.back().hash_       = hash;
	Hashes_.back().accessTime_ = ((time(0) + (rand() % 2 ? 1 : -1) * (rand() % 30 * 24 * 60 * 60)) & 0x0FFFFFFFFFE000000);
	// in seconds, blur by month and mask out changes on year time frame: 0xFFFFFFFF
	// FE000000
	return saveDatabaseToFile(DB_HASHES);
}  // end addToHashesDatabase()

//==============================================================================
// WebUsers::genCookieCode ---
std::string WebUsers::genCookieCode()
{
	char        hexStr[3];
	std::string cc = "";
	for(uint32_t i = 0; i < COOKIE_CODE_LENGTH / 2; ++i)
	{
		intToHexStr(rand(), hexStr);
		cc.append(hexStr);
	}
	return cc;
}  // end genCookieCode()

//==============================================================================
// WebUsers::createNewActiveSession ---
//	if asIndex is not specified (0), new session receives max(ActiveSessionIndex) for user
//+1.. always skipping 0. 	In this ActiveSessionIndex should link a thread of cookieCodes
std::string WebUsers::createNewActiveSession(uint64_t uid, const std::string& ip, uint64_t asIndex)
{
	//__COUTV__(ip);
	ActiveSessions_.push_back(ActiveSession());
	ActiveSessions_.back().cookieCode_ = genCookieCode();
	ActiveSessions_.back().ip_         = ip;
	ActiveSessions_.back().userId_     = uid;
	ActiveSessions_.back().startTime_  = time(0);

	if(asIndex)  // this is a refresh of current active session
		ActiveSessions_.back().sessionIndex_ = asIndex;
	else
	{
		// find max(ActiveSessionIndex)
		uint64_t max = 0;
		for(uint64_t j = 0; j < ActiveSessions_.size(); ++j)
			if(ActiveSessions_[j].userId_ == uid && max < ActiveSessions_[j].sessionIndex_)  // new max
				max = ActiveSessions_[j].sessionIndex_;

		ActiveSessions_.back().sessionIndex_ = (max ? max + 1 : 1);  // 0 is illegal
	}

	return ActiveSessions_.back().cookieCode_;
}  // end createNewActiveSession()

//==============================================================================
// WebUsers::refreshCookieCode ---
// Basic idea is to return valid cookieCode to user for future commands
// There are two issues that arise due to "same user - multiple location":
//		1. Multiple Tabs Scenario (same browser cookie)
//		2. Multiple Browser Scenario (separate login chain)
// We want to allow both modes of operation.
//
// Solution to 1. : long expiration and overlap times
//	return most recent cookie for ActiveSessionIndex (should be deepest in vector always)
//		- If half of expiration time is up, a new cookie is generated as most recent
//		but previous is maintained and start time is changed to accommodate overlap time.
//		- Overlap time should be enough to allow other tabs to take an action and
//		receive the new cookie code.
//
// Solution to 2. : ActiveSessionIndex
//	return most recent cookie for ActiveSessionIndex (should be deepest in vector always)
//		- Independent browsers will have independent cookie chains for same user
//		based on ActiveSessionIndex.
//		- Can use ActiveSessionIndex to detect old logins and log them out.
//
//	enableRefresh added for automatic actions that take place, that should still get
//		the most recent code, but should not generate new codes (set enableRefresh =
// false).
std::string WebUsers::refreshCookieCode(unsigned int i, bool enableRefresh)
{
	// find most recent cookie for ActiveSessionIndex (should be deepest in vector always)
	for(uint64_t j = ActiveSessions_.size() - 1; j != (uint64_t)-1; --j)  // reverse iterate vector
		if(ActiveSessions_[j].userId_ == ActiveSessions_[i].userId_ &&
		   ActiveSessions_[j].sessionIndex_ == ActiveSessions_[i].sessionIndex_)  // if uid and asIndex match, found match
		{
			// found!

			// If half of expiration time is up, a new cookie is generated as most recent
			if(enableRefresh && (time(0) - ActiveSessions_[j].startTime_ > ACTIVE_SESSION_EXPIRATION_TIME / 2))
			{
				// but previous is maintained and start time is changed to accommodate
				// overlap time.
				ActiveSessions_[j].startTime_ = time(0) - ACTIVE_SESSION_EXPIRATION_TIME + ACTIVE_SESSION_COOKIE_OVERLAP_TIME;  // give time window for stale
				                                                                                                                // cookie commands before
				                                                                                                                // expiring

				// create new active cookieCode with same ActiveSessionIndex, will now be
				// found as most recent
				return createNewActiveSession(ActiveSessions_[i].userId_, ActiveSessions_[i].ip_, ActiveSessions_[i].sessionIndex_);
			}

			return ActiveSessions_[j].cookieCode_;  // cookieCode is unchanged
		}

	return "0";  // failure, should be impossible since i is already validated
}  // end refreshCookieCode()

//==============================================================================
// WebUsers::IsCookieActive ---
//	returns User Id on success, returns by reference refreshed cookieCode and displayName
// if cookieCode/user combo is still active 	displayName is returned in username
// std::string 	else returns -1
uint64_t WebUsers::isCookieCodeActiveForLogin(const std::string& uuid, std::string& cookieCode, std::string& username)
{
	if(!CareAboutCookieCodes_)
		return getAdminUserID();  // always successful

	// else
	//	__COUT__ << "I care about
	// cookies?!?!?!*************************************************" << __E__;

	if(!ActiveSessions_.size())
		return NOT_FOUND_IN_DATABASE;  // no active sessions, so do nothing

	uint64_t i, j;  // used to iterate and search

	// find uuid in login session database else return "0"
	if((i = searchLoginSessionDatabaseForUUID(uuid)) == NOT_FOUND_IN_DATABASE)
	{
		__COUT__ << "uuid not found: " << uuid << __E__;
		return NOT_FOUND_IN_DATABASE;
	}

	username = dejumble(username, LoginSessions_[i].id_);  // dejumble user for cookie check

	// search active users for cookie code
	if((i = searchActiveSessionDatabaseForCookie(cookieCode)) == NOT_FOUND_IN_DATABASE)
	{
		__COUT__ << "Cookie code not found" << __E__;
		return NOT_FOUND_IN_DATABASE;
	}

	// search users for user id
	if((j = searchUsersDatabaseForUserId(ActiveSessions_[i].userId_)) == NOT_FOUND_IN_DATABASE)
	{
		__COUT__ << "User ID not found" << __E__;
		return NOT_FOUND_IN_DATABASE;
	}

	// match username, with one found
	if(Users_[j].username_ != username)
	{
		__COUT__ << "cookieCode: " << cookieCode << " was.." << __E__;
		__COUT__ << "username: " << username << " is not found" << __E__;
		return NOT_FOUND_IN_DATABASE;
	}

	username   = Users_[j].displayName_;  // return display name by reference
	cookieCode = refreshCookieCode(i);    // refresh cookie by reference
	return Users_[j].userId_;             // return user ID
}

//==============================================================================
// WebUsers::getActiveSessionCountForUser ---
//	Returns count of unique ActiveSessionIndex entries for user's uid
uint64_t WebUsers::getActiveSessionCountForUser(uint64_t uid)
{
	bool                  unique;
	std::vector<uint64_t> uniqueAsi;  // maintain unique as indices for reference

	uint64_t i, j;
	for(i = 0; i < ActiveSessions_.size(); ++i)
		if(ActiveSessions_[i].userId_ == uid)  // found active session for user
		{
			// check if ActiveSessionIndex is unique
			unique = true;

			for(j = 0; j < uniqueAsi.size(); ++j)
				if(uniqueAsi[j] == ActiveSessions_[i].sessionIndex_)
				{
					unique = false;
					break;
				}

			if(unique)  // unique! so count and save
				uniqueAsi.push_back(ActiveSessions_[i].sessionIndex_);
		}

	__COUT__ << "Found " << uniqueAsi.size() << " active sessions for uid " << uid << __E__;

	return uniqueAsi.size();
}  // end getActiveSessionCountForUser()

//==============================================================================
// WebUsers::checkIpAccess ---
//	checks user defined accept,
//	then checks reject IP file
//	then checks blacklist file
//	return true if ip is accepted, and false if rejected
bool WebUsers::checkIpAccess(const std::string& ip)
{
	if(ip == "0")
		return true;  // always accept dummy IP

	FILE*  fp = fopen((IP_ACCEPT_FILE).c_str(), "r");
	char   line[300];
	size_t len;

	if(fp)
	{
		while(fgets(line, 300, fp))
		{
			len = strlen(line);
			// remove new line
			if(len > 2 && line[len - 1] == '\n')
				line[len - 1] = '\0';
			if(StringMacros::wildCardMatch(ip, line))
				return true;  // found in accept file, so accept
		}

		fclose(fp);
	}

	fp = fopen((IP_REJECT_FILE).c_str(), "r");
	if(fp)
	{
		while(fgets(line, 300, fp))
		{
			len = strlen(line);
			// remove new line
			if(len > 2 && line[len - 1] == '\n')
				line[len - 1] = '\0';
			if(StringMacros::wildCardMatch(ip, line))
				return false;  // found in reject file, so reject
		}

		fclose(fp);
	}

	fp = fopen((IP_BLACKLIST_FILE).c_str(), "r");
	if(fp)
	{
		while(fgets(line, 300, fp))
		{
			len = strlen(line);
			// remove new line
			if(len > 2 && line[len - 1] == '\n')
				line[len - 1] = '\0';
			if(StringMacros::wildCardMatch(ip, line))
				return false;  // found in blacklist file, so reject
		}

		fclose(fp);
	}

	// default to accept if nothing triggered above
	return true;
}  // end checkIpAccess()

//==============================================================================
// WebUsers::incrementIpBlacklistCount ---
void WebUsers::incrementIpBlacklistCount(const std::string& ip)
{
	// increment ip blacklist counter
	auto it = ipBlacklistCounts_.find(ip);
	if(it == ipBlacklistCounts_.end())
	{
		__COUT__ << "First error for ip '" << ip << "'" << __E__;
		ipBlacklistCounts_[ip] = 1;
	}
	else
	{
		++(it->second);

		if(it->second >= IP_BLACKLIST_COUNT_THRESHOLD)
		{
			__MCOUT__("Adding IP '" << ip << "' to blacklist!" << __E__);

			// append to blacklisted IP to generated IP reject file
			FILE* fp = fopen((IP_BLACKLIST_FILE).c_str(), "a");
			if(!fp)
			{
				__SS__ << "IP black list file '" << IP_BLACKLIST_FILE << "' could not be opened." << __E__;
				__MCOUT_ERR__(ss.str());
				return;
			}
			fprintf(fp, "%s\n", ip.c_str());
			fclose(fp);
		}
	}
}  // end incrementIpBlacklistCount()

//==============================================================================
// WebUsers::getUsersDisplayName ---
std::string WebUsers::getUsersDisplayName(uint64_t uid)
{
	uint64_t i;
	if((i = searchUsersDatabaseForUserId(uid)) == NOT_FOUND_IN_DATABASE)
		return "";
	return Users_[i].displayName_;
}  // end getUsersDisplayName()

//==============================================================================
// WebUsers::getUsersUsername ---
std::string WebUsers::getUsersUsername(uint64_t uid)
{
	uint64_t i;
	if((i = searchUsersDatabaseForUserId(uid)) == NOT_FOUND_IN_DATABASE)
		return "";
	return Users_[i].username_;
}  // end getUsersUsername()

// //==============================================================================
// // WebUsers::getUsersDisplayName --- public version which considers remote users
// std::string WebUsers::getUsersDisplayName(uint64_t uid, uint64_t remoteSessionID)
// {
// 	uint64_t i;
// 	if((i = searchUsersDatabaseForUserId(uid)) == NOT_FOUND_IN_DATABASE)
// 		return "";
// 	return Users_[i].displayName_;
// }  // end getUsersDisplayName()

// //==============================================================================
// // WebUsers::getUsersUsername --- public version which considers remote users
// std::string WebUsers::getUsersUsername(uint64_t uid, uint64_t remoteSessionID)
// {
// 	if(uid == ACCOUNT_REMOTE) return getRemoteUsersUsername(remoteSessionID);

// 	uint64_t i;
// 	if((i = searchUsersDatabaseForUserId(uid)) == NOT_FOUND_IN_DATABASE)
// 		return "";
// 	return Users_[i].username_;
// }  // end getUsersUsername()

// //==============================================================================
// // WebUsers::getRemoteUsersDisplayName ---
// std::string WebUsers::getRemoteUsersDisplayName(uint64_t remoteSessionID)
// {
// 	auto it = RemoteSessions_.find(remoteSessionID);
// 	if(it == RemoteSessions_.end())
// 		return "";
// 	return it->second.second.displayName_;
// }  // end getRemoteUsersDisplayName()

// //==============================================================================
// // WebUsers::getRemoteUsersUsername ---
// std::string WebUsers::getRemoteUsersUsername(uint64_t remoteSessionID)
// {	
// 	auto it = RemoteSessions_.find(remoteSessionID);
// 	if(it == RemoteSessions_.end())
// 		return "";
// 	return it->second.second.username_;
// }  // end getRemoteUsersUsername()

//==============================================================================
// WebUsers::cookieCodeLogout ---
//	Used to logout user based on cookieCode and ActiveSessionIndex
//		logoutOtherUserSessions true logs out all of user's other sessions by uid
//		Note: when true, user will remain logged in to current active session
//		logoutOtherUserSessions false logs out only this cookieCode/ActiveSessionIndex
//		Note: when false, user will remain logged in other locations based different
// ActiveSessionIndex
//
//  on failure, returns -1
// 	on success returns number of active sessions that were removed
uint64_t WebUsers::cookieCodeLogout(const std::string& cookieCode, bool logoutOtherUserSessions, uint64_t* userId, const std::string& ip)
{
	uint64_t i;

	// search active users for cookie code
	if((i = searchActiveSessionDatabaseForCookie(cookieCode)) == NOT_FOUND_IN_DATABASE)
	{
		__COUT__ << "Cookie code not found" << __E__;

		incrementIpBlacklistCount(ip);  // increment ip blacklist counter

		return NOT_FOUND_IN_DATABASE;
	}
	else
		ipBlacklistCounts_[ip] = 0;  // clear blacklist count

	// check ip
	if(ActiveSessions_[i].ip_ != ip)
	{
		__COUT__ << "IP does not match active session" << __E__;
		return NOT_FOUND_IN_DATABASE;
	}

	// found valid active session i
	// if logoutOtherUserSessions
	//	remove active sessions that match ActiveSessionUserIdVector[i] and
	// ActiveSessionIndex[i]  else 	remove active sessions that match
	// ActiveSessionUserIdVector[i] but not ActiveSessionIndex[i]

	uint64_t asi = ActiveSessions_[i].sessionIndex_;
	uint64_t uid = ActiveSessions_[i].userId_;
	if(userId)
		*userId = uid;  // return uid if requested
	uint64_t logoutCount = 0;

	i = 0;
	while(i < ActiveSessions_.size())
	{
		if((logoutOtherUserSessions && ActiveSessions_[i].userId_ == uid && ActiveSessions_[i].sessionIndex_ != asi) ||
		   (!logoutOtherUserSessions && ActiveSessions_[i].userId_ == uid && ActiveSessions_[i].sessionIndex_ == asi))
		{
			__COUT__ << "Logging out of active session " << ActiveSessions_[i].userId_ << "-" << ActiveSessions_[i].sessionIndex_ << __E__;
			ActiveSessions_.erase(ActiveSessions_.begin() + i);
			++logoutCount;
		}
		else  // only increment if no delete, for effectively erase rewind
			++i;
	}  // end cleanup active sessioins loop

	__COUT__ << "Found and removed active session count = " << logoutCount << __E__;

	return logoutCount;
}  // end cookieCodeLogout()

// //==============================================================================
// // WebUsers::getUserInfoForCookie ---
// bool WebUsers::getUserInfoForCookie(std::string& cookieCode, std::string* userName, std::string* displayName, uint64_t* activeSessionIndex)
// {
// 	if(userName)
// 		*userName = "";
// 	if(displayName)
// 		*displayName = "";

// 	if(!CareAboutCookieCodes_)  // NO SECURITY, return admin
// 	{
// 		uint64_t uid = getAdminUserID();
// 		if(userName)
// 			*userName = getUsersUsername(uid);
// 		if(displayName)
// 			*displayName = getUsersDisplayName(uid);
// 		if(activeSessionIndex)
// 			*activeSessionIndex = -1;
// 		return true;
// 	}

// 	uint64_t i, j;

// 	// search active users for cookie code
// 	if((i = searchActiveSessionDatabaseForCookie(cookieCode)) == NOT_FOUND_IN_DATABASE)
// 	{
// 		__COUT__ << "cookieCode NOT_FOUND_IN_DATABASE" << __E__;
// 		return false;
// 	}

// 	// get Users record
// 	if((j = searchUsersDatabaseForUserId(ActiveSessions_[i].userId_)) == NOT_FOUND_IN_DATABASE)
// 	{
// 		__COUT__ << "ActiveSession UserId NOT_FOUND_IN_DATABASE" << __E__;
// 		return false;
// 	}

// 	if(userName)
// 		*userName = Users_[j].username_;
// 	if(displayName)
// 		*displayName = Users_[j].displayName_;
// 	if(activeSessionIndex)
// 		*activeSessionIndex = ActiveSessions_[i].sessionIndex_;
// 	return true;
// }  // end getUserInfoForCookie()

//==============================================================================
// WebUsers::isCookieCodeActiveForRequest ---
//	Used to verify cookie code for all general user requests
//  cookieCode/ip must be active to pass
//
//  cookieCode is passed by reference. It is refreshed, if refresh=true on success and may
//  be modified.
//		on success, if userPermissions and/or uid are not null, the permissions and uid
// are  returned
//  on failure, cookieCode contains error message to return to client
//
//  If do NOT care about cookie code, then returns uid 0 (admin)
//		and grants full permissions
bool WebUsers::cookieCodeIsActiveForRequest(std::string&                                                      cookieCode,
                                            std::map<std::string /*groupName*/, WebUsers::permissionLevel_t>* userPermissions,
                                            uint64_t*                                                         uid,
                                            const std::string&                                                ip,
                                            bool                                                              refresh,
                                            std::string*                                                      userWithLock,
                                            uint64_t*                                                         userSessionIndex)
{
	//__COUTV__(ip);

	// check ip black list and increment counter if cookie code not found
	if(!checkIpAccess(ip))
	{
		__COUT_ERR__ << "User IP rejected." << __E__;
		cookieCode = REQ_NO_LOGIN_RESPONSE;
		return false;
	}

	cleanupExpiredEntries();  // remove expired cookies

	uint64_t i, j, userId = NOT_FOUND_IN_DATABASE, userSession = NOT_FOUND_IN_DATABASE;

	__COUTTV__(CareAboutCookieCodes_);
	__COUTT__ << "refresh cookie " << refresh << __E__;
	__COUTVS__(2,cookieCode);

	//always go remote if enabled
	try
	{
		if(remoteLoginVerificationEnabled_ && time(0) > remoteLoginVerificationEnabledBlackoutTime_ &&
			(userId = checkRemoteLoginVerification(cookieCode, refresh, ip)) != NOT_FOUND_IN_DATABASE)
		{		
			// remote verify success!
			userSession = RemoteSessions_.at(cookieCode).sessionIndex_;
			__COUTT__ << "Remote login session verified." << __E__;
		}
	}
	catch(...)
	{
		__COUT_WARN__ << "Ignoring exception during remote login verification." << __E__;

		//Disable remote login in the case that remote verifier is down
		if(!CareAboutCookieCodes_ && remoteLoginVerificationEnabled_ && 
			remoteLoginVerificationEnabledBlackoutTime_ == 0)
		{
			remoteLoginVerificationEnabled_ = false;
			remoteLoginVerificationEnabledBlackoutTime_ = time(0) + 5*60;
			__COUT_INFO__ << "Disabled remote login until " << StringMacros::getTimestampString(remoteLoginVerificationEnabledBlackoutTime_) << __E__;
		}
	}

	if(remoteLoginVerificationEnabled_ && userId == NOT_FOUND_IN_DATABASE)
		__COUTT__ << "Remote login verification failed." << __E__;

	if(!CareAboutCookieCodes_ && userId == NOT_FOUND_IN_DATABASE)  // No Security, so grant admin
	{		
		if(userPermissions)
			*userPermissions =
			    std::map<std::string /*groupName*/, WebUsers::permissionLevel_t>({{WebUsers::DEFAULT_USER_GROUP, WebUsers::PERMISSION_LEVEL_ADMIN}});
		if(uid)
			*uid = getAdminUserID();
		if(userWithLock)
			*userWithLock = usersUsernameWithLock_;
		if(userSessionIndex)
			*userSessionIndex = 0;

		if(cookieCode.size() != COOKIE_CODE_LENGTH)
			cookieCode = genCookieCode();  // return "dummy" cookie code

		
		if(remoteLoginVerificationEnabled_) //add admin session to Remote Sessions for quick verify
		{
			//fill in Remote Session and User info to cache for next login attempt

			while(RemoteSessions_.find(cookieCode) != RemoteSessions_.end()) 
				cookieCode = genCookieCode(); // regenerate on the off chance of collissions
			ActiveSession& newRemoteSession = RemoteSessions_[cookieCode]; //construct remote ActiveSession
			newRemoteSession.cookieCode_ = cookieCode;
			newRemoteSession.ip_ = ip;
			newRemoteSession.userId_ = getAdminUserID();
			newRemoteSession.sessionIndex_ = 0;
			newRemoteSession.startTime_ = time(0);	
		}

		return true;
	}
	// else using security!

	if(userId == NOT_FOUND_IN_DATABASE) //handle standard active session verify
	{
		// search active users for cookie code
		if((i = searchActiveSessionDatabaseForCookie(cookieCode)) == NOT_FOUND_IN_DATABASE)
		{
			__COUT_ERR__ << "Cookie code not found" << __E__;
			cookieCode = REQ_NO_LOGIN_RESPONSE;

			incrementIpBlacklistCount(ip);  // increment ip blacklist counter

			return false;
		}
		else
			ipBlacklistCounts_[ip] = 0;  // clear blacklist count

		// check ip
		if(ip != "0" && ActiveSessions_[i].ip_ != ip)
		{
			__COUTV__(ActiveSessions_[i].ip_);
			//__COUTV__(ip);
			__COUT_ERR__ << "IP does not match active session." << __E__;
			cookieCode = REQ_NO_LOGIN_RESPONSE;
			return false;
		}

		userId = ActiveSessions_[i].userId_;
		userSession = ActiveSessions_[i].sessionIndex_;
		cookieCode = refreshCookieCode(i, refresh);  // refresh cookie by reference
		__COUTT__ << "Login session verified." << __E__;
	}

	//at this point userId has been confirmed remotely or locally

	// get Users record
	if((j = searchUsersDatabaseForUserId(userId)) == NOT_FOUND_IN_DATABASE)
	{
		__COUT_ERR__ << "After login verification, User ID not found! Notify admins." << __E__;
		cookieCode = REQ_NO_LOGIN_RESPONSE;
		return false;
	}
	
	std::map<std::string /*groupName*/, WebUsers::permissionLevel_t> tmpPerm = getPermissionsForUser(userId);

	if(isInactiveForGroup(tmpPerm))  // Check for inactive for all requests!
	{
		__COUTT__ << "Inactive user identified." << __E__;
		cookieCode = REQ_NO_PERMISSION_RESPONSE;
		return false;
	}

	// success!
	if(userPermissions)
		*userPermissions = tmpPerm;
	if(uid)
		*uid = userId;
	if(userWithLock)
		*userWithLock = usersUsernameWithLock_;
	if(userSessionIndex)
		*userSessionIndex = userSession; 


	return true;
}  // end cookieCodeIsActiveForRequest()

//==============================================================================
// WebUsers::cleanupExpiredEntries ---
//	cleanup expired entries from Login Session and Active Session databases
//	check if usersUsernameWithLock_ is still active
//  return the vector of logged out user names if a parameter
//		if not a parameter, store logged out user names for next time called with
// parameter
void WebUsers::cleanupExpiredEntries(std::vector<std::string>* loggedOutUsernames)
{
	uint64_t i;  // used to iterate and search
	uint64_t tmpUid;

	if(loggedOutUsernames)  // return logged out users this time and clear storage vector
	{
		for(i = 0; i < UsersLoggedOutUsernames_.size(); ++i)
			loggedOutUsernames->push_back(UsersLoggedOutUsernames_[i]);
		UsersLoggedOutUsernames_.clear();
	}

	// remove expired entries from Login Session
	for(i = 0; i < LoginSessions_.size(); ++i)
		if(LoginSessions_[i].startTime_ + LOGIN_SESSION_EXPIRATION_TIME < time(0) ||  // expired
		   LoginSessions_[i].loginAttempts_ > LOGIN_SESSION_ATTEMPTS_MAX)
		{
			__COUT__ << "Found expired login sessions: #" << (i + 1) << " of " << LoginSessions_.size() << __E__;
			//" at time " << LoginSessionStartTimeVector[i] << " with attempts " <<
			// LoginSessionAttemptsVector[i] << __E__;

			LoginSessions_.erase(LoginSessions_.begin() + i);
			--i;  // rewind loop
		}

	// declare structures for ascii time
	//	struct tm * timeinfo;
	//	time_t tmpt;
	//	char tstr[200];
	//	timeinfo = localtime ( &(tmpt=time(0)) );
	//	sprintf(tstr,"\"%s\"",asctime (timeinfo)); tstr[strlen(tstr)-2] = '\"';
	//__COUT__ << "Current time is: " << time(0) << " " << tstr << __E__;

	// remove expired entries from Active Session
	for(i = 0; i < ActiveSessions_.size(); ++i)
		if(ActiveSessions_[i].startTime_ + ACTIVE_SESSION_EXPIRATION_TIME <= time(0))  // expired
		{
			// timeinfo = localtime (&(tmpt=ActiveSessionStartTimeVector[i]));
			// sprintf(tstr,"\"%s\"",asctime (timeinfo)); tstr[strlen(tstr)-2] = '\"';
			//__COUT__ << "Found expired user: " << ActiveSessionUserIdVector[i] <<
			//	" start time " << tstr << " i: " << i << " size: " <<
			// ActiveSessionStartTimeVector.size()
			//	<< __E__;

			__COUT__ << "Found expired active sessions: #" << (i + 1) << " of " << ActiveSessions_.size() << __E__;
			__COUTTV__(ActiveSessions_[i].cookieCode_);

			tmpUid = ActiveSessions_[i].userId_;
			ActiveSessions_.erase(ActiveSessions_.begin() + i);

			if(!isUserIdActive(tmpUid))  // if uid no longer active, then user was
			                             // completely logged out
			{
				if(loggedOutUsernames)  // return logged out users this time
					loggedOutUsernames->push_back(Users_[searchUsersDatabaseForUserId(tmpUid)].username_);
				else  // store for next time requested as parameter
					UsersLoggedOutUsernames_.push_back(Users_[searchUsersDatabaseForUserId(tmpUid)].username_);
			}

			--i;  // rewind loop
		}
	//		else
	//		{
	//			timeinfo = localtime (&(tmpt=ActiveSessionStartTimeVector[i] +
	// ACTIVE_SESSION_EXPIRATION_TIME)); 			sprintf(tstr,"\"%s\"",asctime
	//(timeinfo)); tstr[strlen(tstr)-2] = '\"';
	//
	//			//__COUT__ << "Found user: " << ActiveSessionUserIdVector[i] << "-" <<
	// ActiveSessionIndex[i] <<
	//			//	" expires " << tstr <<
	//    		//	" sec left " << ActiveSessionStartTimeVector[i] +
	//    ACTIVE_SESSION_EXPIRATION_TIME - time(0) << __E__;
	//
	//		}

	//__COUT__ << "Found usersUsernameWithLock_: " << usersUsernameWithLock_ << __E__;
	// size_t posRemoteFlag = std::string::npos; 
	if(CareAboutCookieCodes_ && usersUsernameWithLock_ != "" &&
			// ((remoteLoginVerificationEnabled_ && //if remote login enabled, check if userWithLock is remote
			// 	(posRemoteFlag = usersUsernameWithLock_.find(REMOTE_USERLOCK_PREFIX)) == 0 && 
			// 	searchRemoteSessionDatabaseForUsername(
			// 		usersUsernameWithLock_.substr(strlen(REMOTE_USERLOCK_PREFIX))) == NOT_FOUND_IN_DATABASE ) ||
			// (posRemoteFlag != 0 && 
			!isUsernameActive(usersUsernameWithLock_))
			//)))  // unlock if user no longer logged in
		usersUsernameWithLock_ = "";
}  // end cleanupExpiredEntries()

//==============================================================================
// WebUsers::cleanupExpiredRemoteEntries ---
//	cleanup expired entries from Remote Active Session databases
//		Give less time than ACTIVE_SESSION_EXPIRATION_TIME (e.g. /4, and assume safe to keep session open and cached locally to avoid hitting remote server with back-to-back requests)
void WebUsers::cleanupExpiredRemoteEntries()
{
	// remove expired entries from Remote Active Session
	std::vector<std::string> toErase;
	for(const auto& remoteSession : RemoteSessions_)
		if(remoteSession.second.startTime_ + ACTIVE_SESSION_EXPIRATION_TIME/4 <= time(0))  // expired
		{			
			__COUT__ << "Found expired remote active sessions: #" << remoteSession.first << " in " << RemoteSessions_.size() << __E__;
			toErase.push_back(remoteSession.first); //mark for erasing
		}
	for(const auto& eraseId : toErase)
		RemoteSessions_.erase(eraseId);	
} // end cleanupExpiredRemoteEntries()

//==============================================================================
// createNewLoginSession
//	adds a new login session id to database
//		inputs: UUID
//		checks that UUID is unique
//		initializes database entry and returns sessionId std::string
//		return "" on failure
std::string WebUsers::createNewLoginSession(const std::string& UUID, const std::string& ip)
{
	__COUTV__(UUID);
	//__COUTV__(ip);

	uint64_t i = 0;
	for(; i < LoginSessions_.size(); ++i)
		if(LoginSessions_[i].uuid_ == UUID)
			break;

	if(i != LoginSessions_.size())
	{
		__COUT_ERR__ << "UUID: " << UUID << " is not unique" << __E__;
		return "";
	}
	// else UUID is unique

	LoginSessions_.push_back(LoginSession());
	LoginSessions_.back().uuid_ = UUID;

	// generate sessionId
	char        hexStr[3];
	std::string sid = "";
	for(i = 0; i < SESSION_ID_LENGTH / 2; ++i)
	{
		intToHexStr(rand(), hexStr);
		sid.append(hexStr);
	}
	LoginSessions_.back().id_            = sid;
	LoginSessions_.back().ip_            = ip;
	LoginSessions_.back().startTime_     = time(0);
	LoginSessions_.back().loginAttempts_ = 0;

	return sid;
}  // end createNewLoginSession()

//==============================================================================
// WebUsers::sha512
//	performs SHA-512 encoding using openssl linux library crypto on context+user+password
//	if context is empty std::string "", context is generated and returned by reference
//	hashed result is returned
std::string WebUsers::sha512(const std::string& user, const std::string& password, std::string& salt)
{
	SHA512_CTX sha512_context;
	char       hexStr[3];

	if(salt == "")  // generate context
	{
		SHA512_Init(&sha512_context);

		for(unsigned int i = 0; i < 8; ++i)
			sha512_context.h[i] += rand();

		for(unsigned int i = 0; i < sizeof(SHA512_CTX); ++i)
		{
			intToHexStr((uint8_t)(((uint8_t*)(&sha512_context))[i]), hexStr);

			salt.append(hexStr);
		}
		//__COUT__ << salt << __E__;
	}
	else  // use existing context
	{
		//__COUT__ << salt << __E__;

		for(unsigned int i = 0; i < sizeof(SHA512_CTX); ++i)
			((uint8_t*)(&sha512_context))[i] = hexByteStrToInt(&(salt.c_str()[i * 2]));
	}

	std::string strToHash = salt + user + password;

	//__COUT__ << salt << __E__;
	unsigned char hash[SHA512_DIGEST_LENGTH];
	//__COUT__ << salt << __E__;
	char retHash[SHA512_DIGEST_LENGTH * 2 + 1];
	//__COUT__ << strToHash.length() << " " << strToHash << __E__;

	//__COUT__ << "If crashing occurs here, may be an illegal salt context." << __E__;
	SHA512_Update(&sha512_context, strToHash.c_str(), strToHash.length());

	SHA512_Final(hash, &sha512_context);

	//__COUT__ << salt << __E__;
	int i = 0;
	for(i = 0; i < SHA512_DIGEST_LENGTH; i++)
		sprintf(retHash + (i * 2), "%02x", hash[i]);

	//__COUT__ << salt << __E__;
	retHash[SHA512_DIGEST_LENGTH * 2] = '\0';

	//__COUT__ << salt << __E__;

	return retHash;
}  // end sha512()

//==============================================================================
// WebUsers::dejumble
//	the client sends username and pw jumbled for http transmission
//	this function dejumbles
std::string WebUsers::dejumble(const std::string& u, const std::string& s)
{
	if(s.length() != SESSION_ID_LENGTH)
		return "";  // session std::string must be even

	const int ss  = s.length() / 2;
	int       p   = hexByteStrToInt(&(s.c_str()[0])) % ss;
	int       n   = hexByteStrToInt(&(s.c_str()[p * 2])) % ss;
	int       len = (hexByteStrToInt(&(u.c_str()[p * 2])) - p - n + ss * 3) % ss;

	std::vector<bool> x(ss);
	for(int i = 0; i < ss; ++i)
		x[i] = 0;
	x[p] = 1;

	int c = hexByteStrToInt(&(u.c_str()[p * 2]));

	std::string user = "";

	for(int l = 0; l < len; ++l)
	{
		p = (p + hexByteStrToInt(&(s.c_str()[p * 2]))) % ss;
		while(x[p])
			p = (p + 1) % ss;
		x[p] = 1;
		n    = hexByteStrToInt(&(s.c_str()[p * 2]));
		user.append(1, (hexByteStrToInt(&(u.c_str()[p * 2])) - c - n + ss * 4) % ss);
		c = hexByteStrToInt(&(u.c_str()[p * 2]));
	}

	return user;
}  // end dejumble()

//==============================================================================
// WebUsers::getPermissionForUser
// return WebUsers::PERMISSION_LEVEL_INACTIVE if invalid index
std::map<std::string /*groupName*/, WebUsers::permissionLevel_t> WebUsers::getPermissionsForUser(uint64_t uid)
{
	uint64_t userIndex = searchUsersDatabaseForUserId(uid);
	if(userIndex < Users_.size())
		return Users_[userIndex].permissions_;

	// else return all user inactive map
	std::map<std::string /*groupName*/, WebUsers::permissionLevel_t> retErrorMap;
	retErrorMap[WebUsers::DEFAULT_USER_GROUP] = WebUsers::PERMISSION_LEVEL_INACTIVE;
	return retErrorMap;
}  // end getPermissionsForUser()

// //==============================================================================
// // WebUsers::getPermissionForUser --- public version which considers remote users
// // return WebUsers::PERMISSION_LEVEL_INACTIVE if invalid index
// std::map<std::string /*groupName*/, WebUsers::permissionLevel_t> WebUsers::getPermissionsForUser(uint64_t uid, uint64_t remoteSessionID)
// {	
// 	if(uid == ACCOUNT_REMOTE) 
// 	{
// 		auto it = RemoteSessions_.find(remoteSessionID);
// 		if(it == RemoteSessions_.end())
// 		{
// 			// else return all user inactive map
// 			std::map<std::string /*groupName*/, WebUsers::permissionLevel_t> retErrorMap;
// 			retErrorMap[WebUsers::DEFAULT_USER_GROUP] = WebUsers::PERMISSION_LEVEL_INACTIVE;
// 			return retErrorMap;
// 		}
// 		return it->second.second.permissions_;
// 	}

// 	//__COUTV__(uid);
// 	uint64_t userIndex = searchUsersDatabaseForUserId(uid);
// 	//__COUTV__(userIndex); __COUTV__(UsersPermissionsVector.size());
// 	if(userIndex < Users_.size())
// 		return Users_[userIndex].permissions_;

// 	// else return all user inactive map
// 	std::map<std::string /*groupName*/, WebUsers::permissionLevel_t> retErrorMap;
// 	retErrorMap[WebUsers::DEFAULT_USER_GROUP] = WebUsers::PERMISSION_LEVEL_INACTIVE;
// 	return retErrorMap;
// }  // end getPermissionsForUser()

//==============================================================================
// WebUsers::getPermissionLevelForGroup
// return WebUsers::PERMISSION_LEVEL_INACTIVE if group not found in permission map
WebUsers::permissionLevel_t WebUsers::getPermissionLevelForGroup(const std::map<std::string /*groupName*/, WebUsers::permissionLevel_t>& permissionMap,
                                                                 const std::string&                                                      groupName)
{
	auto it = permissionMap.find(groupName);
	if(it == permissionMap.end())
	{
		__COUT__ << "Group name '" << groupName << "' not found - assuming inactive user in this group." << __E__;
		return WebUsers::PERMISSION_LEVEL_INACTIVE;
	}
	return it->second;
}  // end getPermissionLevelForGroup()

//==============================================================================
bool WebUsers::isInactiveForGroup(const std::map<std::string /*groupName*/, WebUsers::permissionLevel_t>& permissionMap, const std::string& groupName)
{
	return getPermissionLevelForGroup(permissionMap, groupName) == WebUsers::PERMISSION_LEVEL_INACTIVE;
}

//==============================================================================
bool WebUsers::isAdminForGroup(const std::map<std::string /*groupName*/, WebUsers::permissionLevel_t>& permissionMap, const std::string& groupName)
{
	return getPermissionLevelForGroup(permissionMap, groupName) == WebUsers::PERMISSION_LEVEL_ADMIN;
}

//==============================================================================
// WebUsers::getPermissionForUser
// return 0 if invalid index
std::string WebUsers::getTooltipFilename(const std::string& username, const std::string& srcFile, const std::string& srcFunc, const std::string& srcId)
{
	std::string filename = (std::string)WEB_LOGIN_DB_PATH + TOOLTIP_DB_PATH + "/";

	// make tooltip directory if not there
	//	note: this is static so WebUsers constructor has not necessarily been called
	mkdir(((std::string)WEB_LOGIN_DB_PATH).c_str(), 0755);
	mkdir(((std::string)WEB_LOGIN_DB_PATH + USERS_DB_PATH).c_str(), 0755);
	mkdir(filename.c_str(), 0755);

	for(const char& c : username)
		if(  // only keep alpha numeric
		    (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
			filename += c;
	filename += "/";

	// make username tooltip directory if not there
	mkdir(filename.c_str(), 0755);

	for(const char& c : srcFile)
		if(  // only keep alpha numeric
		    (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
			filename += c;
	filename += "_";
	for(const char& c : srcFunc)
		if(  // only keep alpha numeric
		    (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
			filename += c;
	filename += "_";
	for(const char& c : srcId)
		if(  // only keep alpha numeric
		    (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
			filename += c;
	filename += ".tip";
	//__COUT__ << "filename " << filename << __E__;
	return filename;
}

std::string ots::WebUsers::getUserEmailFromFingerprint(const std::string& fingerprint)
{
	__COUT__ << "Checking if user fingerprint " << fingerprint << " is in memory database" << __E__;
	if(certFingerprints_.count(fingerprint))
	{
		return certFingerprints_[fingerprint];
	}

	__COUT__ << "Going to read credential database " << WEB_LOGIN_CERTDATA_PATH << __E__;
	std::ifstream f(WEB_LOGIN_CERTDATA_PATH);
	bool          open = false;
	while(f)
	{
		open = true;
		std::string email;
		std::string fp;
		f >> email >> fp;
		if(fp != "NOKEY" && fp != "")
		{
			__COUT__ << "Adding user " << email << " to list with fingerprint " << fp << __E__;
			certFingerprints_[fp] = email;
		}
	}
	if(open)
	{
		f.close();
		remove(WEB_LOGIN_CERTDATA_PATH.c_str());
	}

	__COUT__ << "Checking again if fingerprint is in memory database" << __E__;
	if(certFingerprints_.count(fingerprint))
	{
		return certFingerprints_[fingerprint];
	}

	__COUT__ << "Could not match fingerprint, returning null email" << __E__;
	return "";
}  // end getUserEmailFromFingerprint()

//==============================================================================
// WebUsers::tooltipSetNeverShowForUsername
//	temporarySilence has priority over the neverShow setting
void WebUsers::tooltipSetNeverShowForUsername(const std::string& username,
                                              HttpXmlDocument* /*xmldoc*/,
                                              const std::string& srcFile,
                                              const std::string& srcFunc,
                                              const std::string& srcId,
                                              bool               doNeverShow,
                                              bool               temporarySilence)
{
	std::string filename;
	bool        isForAll = (srcFile == "ALL" && srcFunc == "ALL" && srcId == "ALL");

	if(isForAll)
	{
		__COUT__ << "Disabling ALL tooltips for user '" << username << "' is now set to " << doNeverShow << __E__;
		filename = getTooltipFilename(username, SILENCE_ALL_TOOLTIPS_FILENAME, "", "");
	}
	else
	{
		filename = getTooltipFilename(username, srcFile, srcFunc, srcId);
		__COUT__ << "Setting tooltip never show for user '" << username << "' to " << doNeverShow << " (temporarySilence=" << temporarySilence << ")" << __E__;
	}

	FILE* fp = fopen(filename.c_str(), "w");
	if(fp)
	{  // file exists, so do NOT show tooltip
		if(temporarySilence)
			fprintf(fp, "%ld", time(0) + 7 /*days*/ * 24 /*hours*/ * 60 * 60);  // mute for a week
		else if(!isForAll && doNeverShow && username == WebUsers::DEFAULT_ADMIN_USERNAME)
		{
			// admin could be shared account, so max out at 30 days
			fprintf(fp, "%ld", time(0) + 30 /*days*/ * 24 /*hours*/ * 60 * 60);

			__COUT__ << "User '" << username
			         << "' may be a shared account, so max silence duration for tooltips "
			            "is 30 days. Silencing now."
			         << __E__;
		}
		else
			fputc(doNeverShow ? '1' : '0', fp);
		fclose(fp);
	}
	else  // default to show tool tip
		__COUT_ERR__ << "Big problem with tooltips! File not accessible: " << filename << __E__;
}  // end tooltipSetNeverShowForUsername()

//==============================================================================
// WebUsers::tooltipCheckForUsername
//	read file for tooltip
//		if not 1 then never show
//		if 0 then "always show"
//		if other then treat as temporary mute..
//			i.e. if time(0) > val show
void WebUsers::tooltipCheckForUsername(
    const std::string& username, HttpXmlDocument* xmldoc, const std::string& srcFile, const std::string& srcFunc, const std::string& srcId)
{
	if(srcId == "ALWAYS")
	{
		// ALWAYS shows tool tip
		xmldoc->addTextElementToData("ShowTooltip", "1");
		return;
	}

	//	__COUT__ << "username " << username << __E__;
	//	__COUT__ << "srcFile " << srcFile << __E__;
	//	__COUT__ << "srcFunc " << srcFunc << __E__;
	//	__COUT__ << "srcId " << srcId << __E__;
	//__COUT__ << "Checking tooltip for user: " << username << __E__;

	// if the silence file exists, silence all tooltips
	std::string silencefilename = getTooltipFilename(username, SILENCE_ALL_TOOLTIPS_FILENAME, "", "");
	//__COUTV__(silencefilename);
	FILE* silencefp = fopen(silencefilename.c_str(), "r");
	if(silencefp != NULL)
	{
		time_t val;
		char   line[100];
		fgets(line, 100, silencefp);
		sscanf(line, "%ld", &val);
		fclose(silencefp);
		if(val == 1)
		{
			xmldoc->addTextElementToData("ShowTooltip", "0");
			// tooltipSetNeverShowForUsername(username, xmldoc, srcFile, srcFunc, srcId, true, true);
			return;
		}
	}

	std::string filename = getTooltipFilename(username, srcFile, srcFunc, srcId);
	FILE*       fp       = fopen(filename.c_str(), "r");
	if(fp)
	{  // file exists, so do NOT show tooltip
		time_t val;
		char   line[100];
		fgets(line, 100, fp);
		sscanf(line, "%ld", &val);
		fclose(fp);

		__COUT__ << "tooltip value read = " << val << " vs time(0)=" << time(0) << __E__;

		// if first line in file is a 1 then do not show
		//	else show if current time is greater than value
		xmldoc->addTextElementToData("ShowTooltip", val == 1 ? "0" : (time(0) > val ? "1" : "0"));
	}
	else  // default to show tool tip
	{
		xmldoc->addTextElementToData("ShowTooltip", "1");
	}

}  // end tooltipCheckForUsername();

//==============================================================================
// WebUsers::resetAllUserTooltips
void WebUsers::resetAllUserTooltips(const std::string& userNeedle)
{
	std::system(("rm -rf " + (std::string)WEB_LOGIN_DB_PATH + TOOLTIP_DB_PATH + "/" + userNeedle).c_str());
	__COUT__ << "Successfully reset Tooltips for user " << userNeedle << __E__;
}  // end of resetAllUserTooltips()

//==============================================================================
// WebUsers::silenceAllUserTooltips
// creates a file
void WebUsers::silenceAllUserTooltips(const std::string& username)
{
	std::string silencefilename = getTooltipFilename(username, SILENCE_ALL_TOOLTIPS_FILENAME, "", "");  // srcFile, srcFunc, srcId);
	FILE*       silencefp       = fopen(silencefilename.c_str(), "w");
	if(silencefp != NULL)
	{
		fputs("1", silencefp);
		fclose(silencefp);
	}

}  // end of silenceAllUserTooltips()

//==============================================================================
// WebUsers::insertGetSettingsResponse
//  add settings to xml document
//  all active users have permissions of at least 1 so have web preferences:
//      -background color
//      -dashboard color
//      -window color
//      -3 user defaults for window layouts(and current), can set current as one of
//      defaults
//  super users have account controls:
//      -list of user accounts to edit permissions, display name, or delete account
//      -add new account
//	...and super users have system default window layout
//		-2 system defaults for window layouts
//
//	layout settings explanation
//		0 = no windows, never set, empty desktop
//		example 2 layouts set, 2 not,
//			[<win name>, <win subname>, <win url>, <x>, <y>, <w>, <h>]; [<win name>, <win
// subname>, <win url>, <x>, <y>, <w>, <h>]...];0;0
void WebUsers::insertSettingsForUser(uint64_t uid, HttpXmlDocument* xmldoc, bool includeAccounts)
{
	std::map<std::string /*groupName*/, WebUsers::permissionLevel_t> permissionMap = getPermissionsForUser(uid);

	//__COUTV__(StringMacros::mapToString(permissionMap));
	if(isInactiveForGroup(permissionMap))
		return;  // not an active user

	uint64_t userIndex = searchUsersDatabaseForUserId(uid);
	__COUT__ << "Gettings settings for user: " << Users_[userIndex].username_ << __E__;

	std::string fn =
	    (std::string)WEB_LOGIN_DB_PATH + (std::string)USERS_PREFERENCES_PATH + Users_[userIndex].username_ + "." + (std::string)USERS_PREFERENCES_FILETYPE;

	HttpXmlDocument prefXml;

	__COUT__ << "Preferences file: " << fn << __E__;

	if(!prefXml.loadXmlDocument(fn))
	{
		__COUT__ << "Preferences are defaults." << __E__;
		// insert defaults, no pref document found
		xmldoc->addTextElementToData(PREF_XML_BGCOLOR_FIELD, PREF_XML_BGCOLOR_DEFAULT);
		xmldoc->addTextElementToData(PREF_XML_DBCOLOR_FIELD, PREF_XML_DBCOLOR_DEFAULT);
		xmldoc->addTextElementToData(PREF_XML_WINCOLOR_FIELD, PREF_XML_WINCOLOR_DEFAULT);
		xmldoc->addTextElementToData(PREF_XML_LAYOUT_FIELD, PREF_XML_LAYOUT_DEFAULT);
	}
	else
	{
		__COUT__ << "Saved Preferences found." << __E__;
		xmldoc->copyDataChildren(prefXml);
	}

	// add settings if super user
	if(includeAccounts && isAdminForGroup(permissionMap))
	{
		__COUT__ << "Admin on our hands" << __E__;

		xmldoc->addTextElementToData(PREF_XML_ACCOUNTS_FIELD, "");

		if(Users_.size() == 0)
		{
			__COUT__ << "Missing users? Attempting to load database" << __E__;
			loadDatabases();
		}

		// get all accounts
		for(uint64_t i = 0; i < Users_.size(); ++i)
		{
			xmldoc->addTextElementToParent("username", Users_[i].username_, PREF_XML_ACCOUNTS_FIELD);
			xmldoc->addTextElementToParent("display_name", Users_[i].displayName_, PREF_XML_ACCOUNTS_FIELD);

			if(Users_[i].email_.size() > i)
			{
				xmldoc->addTextElementToParent("useremail", Users_[i].email_, PREF_XML_ACCOUNTS_FIELD);
			}
			else
			{
				xmldoc->addTextElementToParent("useremail", "", PREF_XML_ACCOUNTS_FIELD);
			}

			xmldoc->addTextElementToParent("permissions", StringMacros::mapToString(Users_[i].permissions_), PREF_XML_ACCOUNTS_FIELD);

			xmldoc->addTextElementToParent("nac", Users_[i].getNewAccountCode().c_str(), PREF_XML_ACCOUNTS_FIELD);
		}
	}

	// get system layout defaults
	fn = (std::string)WEB_LOGIN_DB_PATH + (std::string)USERS_PREFERENCES_PATH + (std::string)SYSTEM_PREFERENCES_PREFIX + "." +
	     (std::string)USERS_PREFERENCES_FILETYPE;
	if(!prefXml.loadXmlDocument(fn))
	{
		__COUT__ << "System Preferences are defaults." << __E__;
		// insert defaults, no pref document found
		xmldoc->addTextElementToData(PREF_XML_SYSLAYOUT_FIELD, PREF_XML_SYSLAYOUT_DEFAULT);
	}
	else
	{
		__COUT__ << "Saved System Preferences found." << __E__;
		xmldoc->copyDataChildren(prefXml);
	}

	__COUTV__(StringMacros::mapToString(permissionMap));

	// add permissions value
	xmldoc->addTextElementToData(PREF_XML_PERMISSIONS_FIELD, StringMacros::mapToString(permissionMap));

	// add user with lock
	xmldoc->addTextElementToData(PREF_XML_USERLOCK_FIELD, usersUsernameWithLock_);

	// add user name
	xmldoc->addTextElementToData(PREF_XML_USERNAME_FIELD, getUsersUsername(uid));

	// add ots owner name
	xmldoc->addTextElementToData(PREF_XML_OTS_OWNER_FIELD, WebUsers::OTS_OWNER);

}  // end insertSettingsForUser()

//==============================================================================
// WebUsers::setGenericPreference
//	each generic preference has its own directory, and each user has their own file
void WebUsers::setGenericPreference(uint64_t uid, const std::string& preferenceName, const std::string& preferenceValue)
{
	uint64_t userIndex = searchUsersDatabaseForUserId(uid);
	//__COUT__ << "setGenericPreference for user: " << UsersUsernameVector[userIndex] <<
	//__E__;

	// force alpha-numeric with dash/underscore
	std::string safePreferenceName = "";
	for(const auto& c : preferenceName)
		if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c >= '-' || c <= '_'))
			safePreferenceName += c;

	std::string dir = (std::string)WEB_LOGIN_DB_PATH + (std::string)USERS_PREFERENCES_PATH + "generic_" + safePreferenceName + "/";

	// attempt to make directory (just in case)
	mkdir(dir.c_str(), 0755);

	std::string fn = Users_[userIndex].username_ + "_" + safePreferenceName + "." + (std::string)USERS_PREFERENCES_FILETYPE;

	__COUT__ << "Preferences file: " << (dir + fn) << __E__;

	FILE* fp = fopen((dir + fn).c_str(), "w");
	if(fp)
	{
		fprintf(fp, "%s", preferenceValue.c_str());
		fclose(fp);
	}
	else
		__COUT_ERR__ << "Preferences file could not be opened for writing!" << __E__;
}  // end setGenericPreference()

//==============================================================================
// WebUsers::getGenericPreference
//	each generic preference has its own directory, and each user has their own file
//	default preference is empty string.
std::string WebUsers::getGenericPreference(uint64_t uid, const std::string& preferenceName, HttpXmlDocument* xmldoc) const
{
	uint64_t userIndex = searchUsersDatabaseForUserId(uid);
	//__COUT__ << "getGenericPreference for user: " << UsersUsernameVector[userIndex] <<
	//__E__;

	// force alpha-numeric with dash/underscore
	std::string safePreferenceName = "";
	for(const auto& c : preferenceName)
		if((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || (c >= '-' || c <= '_'))
			safePreferenceName += c;

	std::string dir = (std::string)WEB_LOGIN_DB_PATH + (std::string)USERS_PREFERENCES_PATH + "generic_" + safePreferenceName + "/";

	std::string fn = Users_[userIndex].username_ + "_" + safePreferenceName + "." + (std::string)USERS_PREFERENCES_FILETYPE;

	__COUT__ << "Preferences file: " << (dir + fn) << __E__;

	// read from preferences file
	FILE* fp = fopen((dir + fn).c_str(), "r");
	if(fp)
	{
		fseek(fp, 0, SEEK_END);
		const long size = ftell(fp);
		char*      line = new char[size + 1];  // std::string with line.reserve(size + 1) does not work for unknown reason
		rewind(fp);
		fread(line, 1, size, fp);
		line[size] = '\0';
		fclose(fp);
		std::string retVal(line, size);
		delete[] line;

		__COUT__ << "Read value (sz = " << retVal.size() << ") " << retVal << __E__;
		if(xmldoc)
			xmldoc->addTextElementToData(safePreferenceName, retVal);
		return retVal;
	}
	else
		__COUT__ << "Using default value." << __E__;

	// default preference is empty string
	if(xmldoc)
		xmldoc->addTextElementToData(safePreferenceName, "");
	return "";
}  // end getGenericPreference()

//==============================================================================
// WebUsers::changeSettingsForUser
void WebUsers::changeSettingsForUser(
    uint64_t uid, const std::string& bgcolor, const std::string& dbcolor, const std::string& wincolor, const std::string& layout, const std::string& syslayout)
{
	std::map<std::string /*groupName*/, WebUsers::permissionLevel_t> permissionMap = getPermissionsForUser(uid);
	if(isInactiveForGroup(permissionMap))
		return;  // not an active user

	uint64_t userIndex = searchUsersDatabaseForUserId(uid);
	__COUT__ << "Changing settings for user: " << Users_[userIndex].username_ << __E__;

	std::string fn =
	    (std::string)WEB_LOGIN_DB_PATH + (std::string)USERS_PREFERENCES_PATH + Users_[userIndex].username_ + "." + (std::string)USERS_PREFERENCES_FILETYPE;

	__COUT__ << "Preferences file: " << fn << __E__;

	HttpXmlDocument prefXml;
	prefXml.addTextElementToData(PREF_XML_BGCOLOR_FIELD, bgcolor);
	prefXml.addTextElementToData(PREF_XML_DBCOLOR_FIELD, dbcolor);
	prefXml.addTextElementToData(PREF_XML_WINCOLOR_FIELD, wincolor);
	prefXml.addTextElementToData(PREF_XML_LAYOUT_FIELD, layout);

	prefXml.saveXmlDocument(fn);

	// if admin privilieges set system default layouts
	if(!isAdminForGroup(permissionMap))
		return;  // not admin

	// set system layout defaults
	fn = (std::string)WEB_LOGIN_DB_PATH + (std::string)USERS_PREFERENCES_PATH + (std::string)SYSTEM_PREFERENCES_PREFIX + "." +
	     (std::string)USERS_PREFERENCES_FILETYPE;

	HttpXmlDocument sysPrefXml;
	sysPrefXml.addTextElementToData(PREF_XML_SYSLAYOUT_FIELD, syslayout);

	sysPrefXml.saveXmlDocument(fn);
}  // end changeSettingsForUser()

//==============================================================================
// WebUsers::setUserWithLock
// if lock is true, set lock user specified
// if lock is false, attempt to unlock user specified
//	return true on success
bool WebUsers::setUserWithLock(uint64_t actingUid, bool lock, const std::string& username)
{
	std::map<std::string /*groupName*/, WebUsers::permissionLevel_t> permissionMap = getPermissionsForUser(actingUid);

	std::string actingUser = getUsersUsername(actingUid);
	bool isUserActive = isUsernameActive(username);

	__COUTV__(actingUser);
	__COUT__ << "Permissions: " << StringMacros::mapToString(permissionMap) << __E__;
	__COUTV__(usersUsernameWithLock_);
	__COUTV__(lock);
	__COUTV__(username);
	__COUTV__(isUserActive);

	if(lock && (isUserActive || !CareAboutCookieCodes_))  // lock and currently active
	{
		if(!CareAboutCookieCodes_ && username != DEFAULT_ADMIN_USERNAME)  // enforce wiz mode only use admin account
		{
			__MCOUT_ERR__("User '" << actingUser << "' tried to lock for a user other than admin in wiz mode. Not allowed." << __E__);
			return false;
		}
		else if(!isAdminForGroup(permissionMap) && actingUser != username)  // enforce normal mode admin privleges
		{
			__MCOUT_ERR__("A non-admin user '" << actingUser << "' tried to lock for a user other than self. Not allowed." << __E__);
			return false;
		}
		usersUsernameWithLock_ = username;
	}
	else if(!lock && usersUsernameWithLock_ == username)  // unlock
		usersUsernameWithLock_ = "";
	else
	{
		if(!isUserActive)
			__MCOUT_ERR__("User '" << username << "' is inactive." << __E__);
		__MCOUT_ERR__("Failed to lock for user '" << username << ".'" << __E__);
		return false;
	}

	__MCOUT_INFO__("User '" << username << "' has locked out the system!" << __E__);

	// save username with lock
	{
		std::string securityFileName = USER_WITH_LOCK_FILE;
		FILE*       fp               = fopen(securityFileName.c_str(), "w");
		if(!fp)
		{
			__COUT_INFO__ << "USER_WITH_LOCK_FILE " << USER_WITH_LOCK_FILE << " not found. Ignoring." << __E__;
		}
		else
		{
			fprintf(fp, "%s", usersUsernameWithLock_.c_str());
			fclose(fp);
		}
	}
	return true;
}  // end setUserWithLock()

//==============================================================================
// WebUsers::modifyAccountSettings
void WebUsers::modifyAccountSettings(
    uint64_t actingUid, uint8_t cmd_type, const std::string& username, const std::string& displayname, const std::string& email, const std::string& permissions)
{
	std::map<std::string /*groupName*/, WebUsers::permissionLevel_t> permissionMap = getPermissionsForUser(actingUid);
	if(!isAdminForGroup(permissionMap))
	{
		// not an admin
		__SS__ << "Only admins can modify user settings." << __E__;
		__SS_THROW__;
	}

	uint64_t i    = searchUsersDatabaseForUserId(actingUid);
	uint64_t modi = searchUsersDatabaseForUsername(username);
	if(modi == 0)
	{
		if(i == 0)
		{
			__COUT_INFO__ << "Admin password reset." << __E__;
			Users_[modi].setModifier(Users_[i].username_);
			Users_[modi].salt_              = "";
			Users_[modi].loginFailureCount_ = 0;
			saveDatabaseToFile(DB_USERS);
			return;
		}
		__SS__ << "Cannot modify first user" << __E__;
		__SS_THROW__;
	}

	if(username.length() < USERNAME_LENGTH)
	{
		__SS__ << "Invalid Username, must be length " << USERNAME_LENGTH << __E__;
		__SS_THROW__;
	}
	if(displayname.length() < DISPLAY_NAME_LENGTH)
	{
		__SS__ << "Invalid Display Name; must be length " << DISPLAY_NAME_LENGTH << __E__;
		__SS_THROW__;
	}

	__COUT__ << "Input Permissions: " << permissions << __E__;
	std::map<std::string /*groupName*/, WebUsers::permissionLevel_t> newPermissionsMap;

	switch(cmd_type)
	{
	case MOD_TYPE_UPDATE:

		__COUT__ << "MOD_TYPE_UPDATE " << username << " := " << permissions << __E__;

		if(modi == NOT_FOUND_IN_DATABASE)
		{
			__SS__ << "User not found!? Should not happen." << __E__;
			__SS_THROW__;
		}

		// enforce unique Display Name
		{
			for(uint64_t i = 0; i < Users_.size(); ++i)
				if(i == modi)
					continue;  // skip target user
				else if(Users_[i].displayName_ == displayname)
				{
					__SS__ << "Display Name '" << displayname << "' already exists! Please choose a unique display name." << __E__;
					__SS_THROW__;
				}
		}

		Users_[modi].displayName_ = displayname;
		Users_[modi].email_       = email;

		{  // handle permissions
			StringMacros::getMapFromString(permissions, newPermissionsMap);
			bool wasInactive = isInactiveForGroup(Users_[modi].permissions_);

			// fix permissions_ if missing default user group
			if(newPermissionsMap.size() == 0)  // default to inactive
				Users_[modi].permissions_[WebUsers::DEFAULT_USER_GROUP] = std::atoi(permissions.c_str());
			else if(newPermissionsMap.size() == 1 && newPermissionsMap.find(WebUsers::DEFAULT_USER_GROUP) == newPermissionsMap.end())
			{
				if(newPermissionsMap.begin()->first == "")
					Users_[modi].permissions_[WebUsers::DEFAULT_USER_GROUP] = newPermissionsMap.begin()->second;
				else  // if a user group attempted, copy settings for default group
				{
					newPermissionsMap[WebUsers::DEFAULT_USER_GROUP] = newPermissionsMap.begin()->second;
					Users_[modi].permissions_                       = newPermissionsMap;
				}
			}
			else
				Users_[modi].permissions_ = newPermissionsMap;

			// If account was inactive and re-activating, then reset fail count and
			// password. 	Note: this is the account unlock mechanism.
			if(wasInactive &&                                   // was inactive
			   !isInactiveForGroup(Users_[modi].permissions_))  // and re-activating
			{
				__COUT__ << "Reactivating " << username << __E__;
				Users_[modi].loginFailureCount_ = 0;
				Users_[modi].salt_              = "";
			}
		}  // end permissions handling

		// save information about modifier
		{
			if(i == NOT_FOUND_IN_DATABASE)
			{
				__SS__ << "Master User not found!? Should not happen." << __E__;
				__SS_THROW__;
			}
			Users_[modi].setModifier(Users_[i].username_);
		}
		break;
	case MOD_TYPE_ADD:
		// Note: username, userId, AND displayName must be unique!

		__COUT__ << "MOD_TYPE_ADD " << username << " - " << displayname << __E__;

		createNewAccount(username, displayname, email);
		// save information about modifier
		{
			if(i == NOT_FOUND_IN_DATABASE)
			{
				__SS__ << "Master User not found!? Should not happen." << __E__;
				__SS_THROW__;
			}
			Users_.back().setModifier(Users_[i].username_);
		}

		if(permissions.size())  // apply permissions
		{
			modifyAccountSettings(actingUid, MOD_TYPE_UPDATE, username, displayname, email, permissions);
			return;
		}
		break;
	case MOD_TYPE_DELETE:
		__COUT__ << "MOD_TYPE_DELETE " << username << " - " << displayname << __E__;
		deleteAccount(username, displayname);
		break;
	default:
		__SS__ << "Undefined command - do nothing " << username << __E__;
		__SS_THROW__;
	}

	saveDatabaseToFile(DB_USERS);
}  // end modifyAccountSettings()
//==============================================================================
// WebUsers::getActiveUserCount
//	return count of active Display Names
size_t WebUsers::getActiveUserCount()
{
	std::set<unsigned int> activeUserIndices;
	for(uint64_t i = 0; i < ActiveSessions_.size(); ++i)
		activeUserIndices.emplace(searchUsersDatabaseForUserId(ActiveSessions_[i].userId_));
	return activeUserIndices.size();
}  // end getActiveUserCount()

//==============================================================================
// WebUsers::getActiveUsersString
//	return comma separated list of active Display Names
std::string WebUsers::getActiveUsersString()
{
	std::set<unsigned int> activeUserIndices;
	for(uint64_t i = 0; i < ActiveSessions_.size(); ++i)
		activeUserIndices.emplace(searchUsersDatabaseForUserId(ActiveSessions_[i].userId_));

	std::string activeUsersString      = "";
	bool        addComma = false;
	for(const auto& i : activeUserIndices)
	{
		if(i >= Users_.size())
			continue;  // skip not found

		if(addComma)
			activeUsersString += ",";
		else
			addComma = true;

		activeUsersString += Users_[i].displayName_;
	}
	if(activeUserIndices.size() == 0 && WebUsers::getSecurity() == WebUsers::SECURITY_TYPE_NONE)  // assume only admin is active
		activeUsersString += WebUsers::DEFAULT_ADMIN_DISPLAY_NAME;

	__COUTV__(activeUsersString);
	return activeUsersString;
}  // end getActiveUsersString()

//==============================================================================
// WebUsers::getAdminUserID
//
uint64_t WebUsers::getAdminUserID()
{
	uint64_t uid = searchUsersDatabaseForUsername(DEFAULT_ADMIN_USERNAME);
	return uid;
}

//==============================================================================
// WebUsers::loadUserWithLock
//	//load username with lock from file
void WebUsers::loadUserWithLock()
{
	char username[300] = "";  // assume username is less than 300 chars

	std::string securityFileName = USER_WITH_LOCK_FILE;
	FILE*       fp               = fopen(securityFileName.c_str(), "r");
	if(!fp)
	{
		__COUT_INFO__ << "USER_WITH_LOCK_FILE " << USER_WITH_LOCK_FILE << " not found. Defaulting to admin lock." << __E__;

		// default to admin lock if no file exists
		sprintf(username, "%s", DEFAULT_ADMIN_USERNAME.c_str());
	}
	else
	{
		fgets(username, 300, fp);
		username[299] = '\0';  // likely does nothing, but make sure there is closure on string
		fclose(fp);
	}

	// attempt to set lock
	__COUT__ << "Attempting to load username with lock: " << username << __E__;

	if(strlen(username) == 0)
	{
		__COUT_INFO__ << "Loaded state for user-with-lock is unlocked." << __E__;
		return;
	}

	uint64_t i = searchUsersDatabaseForUsername(username);
	if(i == NOT_FOUND_IN_DATABASE)
	{
		__COUT_INFO__ << "username " << username << " not found in database. Ignoring." << __E__;
		return;
	}
	__COUT__ << "Setting lock" << __E__;
	setUserWithLock(Users_[i].userId_, true, username);
}  // end loadUserWithLock()

//==============================================================================
// addSystemMessage
//	targetUser can be "*" for all users
void WebUsers::addSystemMessage(const std::string& targetUsersCSV, const std::string& message)
{
	addSystemMessage(targetUsersCSV, "" /*subject*/, message, false /*doEmail*/);
}  // end addSystemMessage()

//==============================================================================
// addSystemMessage
//	targetUser can be "*" for all users
void WebUsers::addSystemMessage(const std::string& targetUsersCSV, const std::string& subject, const std::string& message, bool doEmail)
{
	std::vector<std::string> targetUsers;
	StringMacros::getVectorFromString(targetUsersCSV, targetUsers);
	addSystemMessage(targetUsers, subject, message, doEmail);
}  // end addSystemMessage()

//==============================================================================
// addSystemMessage
//	targetUser can be "*" for all users
//	Note: do not printout message, because if it was a Console trigger, it will fire repeatedly
void WebUsers::addSystemMessage(const std::vector<std::string>& targetUsers, const std::string& subject, const std::string& message, bool doEmail)
{
	__COUT__ << "Before number of users with system messages: " << systemMessages_.size() <<
		", first user has " << (systemMessages_.size()?systemMessages_.begin()->second.size():0) << " messages." << __E__;

	// lock for remainder of scope
	std::lock_guard<std::mutex> lock(systemMessageLock_);

	systemMessageCleanup();

	std::string fullMessage = StringMacros::encodeURIComponent((subject == "" ? "" : (subject + ": ")) + message);

	//	Note: do not printout message, because if it was a Console trigger, it will fire repeatedly
	std::cout << __COUT_HDR__ << "fullMessage: " << fullMessage << __E__;
	__COUTV__(StringMacros::vectorToString(targetUsers));

	std::set<std::string> targetEmails;

	for(const auto& targetUser : targetUsers)
	{
		// reject if message is a repeat for user

		if(targetUser == "" || (targetUser != "*" && targetUser.size() < 3))
		{
			__COUT__ << "Illegal username '" << targetUser << "'" << __E__;
			continue;
		}
		__COUTV__(targetUser);
		// target user might * or <group name>:<permission threshold> or just <username>

		// do special ALL email handling
		if(doEmail && targetUser == "*")
		{
			// for each user, look up email and append
			for(const auto& user : Users_)
			{
				if(user.email_.size() > 5 &&  // few simple valid email checks
				   user.email_.find('@') != std::string::npos && user.email_.find('.') != std::string::npos)
				{
					__COUT__ << "Adding " << user.displayName_ << " email: " << user.email_ << __E__;
					targetEmails.emplace(user.email_);
				}
			}  // end add every user loop

		}  // end all email handling
		else if(targetUser.find(':') != std::string::npos)
		{
			// special group handling.. convert to individual users
			__COUT__ << "Treating as group email target: " << targetUser << __E__;

			std::map<std::string, WebUsers::permissionLevel_t> targetGroupMap;
			StringMacros::getMapFromString(  // re-factor membership string to map
			    targetUser,
			    targetGroupMap);

			__COUTV__(StringMacros::mapToString(targetGroupMap));

			if(targetGroupMap.size() == 1)
			{
				// add users to targetUsers, so the loop will catch them at end

				// loop through all users, and add users that match group spec
				for(const auto& user : Users_)
				{
					WebUsers::permissionLevel_t userLevel = getPermissionLevelForGroup(getPermissionsForUser(user.userId_), targetGroupMap.begin()->first);

					__COUTV__(StringMacros::mapToString(getPermissionsForUser(user.userId_)));
					__COUTV__((int)userLevel);
					__COUTV__(targetGroupMap.begin()->first);

					if(userLevel != WebUsers::PERMISSION_LEVEL_INACTIVE && userLevel >= targetGroupMap.begin()->second &&
					   user.email_.size() > 5 &&  // few simple valid email checks
					   user.email_.find('@') != std::string::npos && user.email_.find('.') != std::string::npos)
					{
						if(doEmail)
						{
							targetEmails.emplace(user.email_);
							__COUT__ << "Adding " << user.displayName_ << " email: " << user.email_ << __E__;
						}
						addSystemMessageToMap(user.displayName_, fullMessage);
					}
				}
			}
			else
				__COUT__ << "target Group Map from '" << targetUser << "' is empty." << __E__;

			continue;  // proceed with user loop, do not add group target message
		}

		// at this point add to system message map (similar to group individual add, but might be '*')

		addSystemMessageToMap(targetUser, fullMessage);

		if(doEmail)  // find user for email
		{
			for(const auto& user : Users_)
			{
				if(user.displayName_ == targetUser)
				{
					if(user.email_.size() > 5 &&  // few simple valid email checks
					   user.email_.find('@') != std::string::npos && user.email_.find('.') != std::string::npos)
					{
						targetEmails.emplace(user.email_);
						__COUT__ << "Adding " << user.displayName_ << " email: " << user.email_ << __E__;
					}
					break;  // user found, exit search loop
				}
			}  // end user search loop
		}

	}  // end target user message add loop

	__COUT__ << "After number of users with system messages: " << systemMessages_.size() << 
		", first user has " << (systemMessages_.size()?systemMessages_.begin()->second.size():0) << " messages." << __E__;
	__COUTV__(targetEmails.size());

	if(doEmail && targetEmails.size())
	{
		__COUTV__(StringMacros::setToString(targetEmails));

		std::string toList   = "";
		bool        addComma = false;
		for(const auto& email : targetEmails)
		{
			if(addComma)
				toList += ", ";
			else
				addComma = true;
			toList += email;
		}

		std::string filename = (std::string)WEB_LOGIN_DB_PATH + (std::string)USERS_DB_PATH + "/.tmp_email.txt";
		FILE*       fp       = fopen(filename.c_str(), "w");
		if(!fp)
		{
			__SS__ << "Could not open email file: " << filename << __E__;
			__SS_THROW__;
		}

		fprintf(fp, "From: %s\n", (WebUsers::OTS_OWNER == "" ? "ots" : (StringMacros::decodeURIComponent(WebUsers::OTS_OWNER) + "_ots")).c_str());
		fprintf(fp, "To: %s\n", toList.c_str());
		fprintf(fp, "Subject: %s\n", subject.c_str());
		fprintf(fp, "Content-Type: text/html\n");
		fprintf(fp, "\n<html><pre>%s</pre></html>", message.c_str());
		fclose(fp);

		StringMacros::exec(("sendmail \"" + toList + "\" < " + filename).c_str());
	}
	else if(doEmail)
		__COUT_WARN__ << "Do email was attempted, but no target users had email addresses specified!" << __E__;

}  // end addSystemMessage()

//==============================================================================
// addSystemMessageToMap
// 	Manages map and adds message for user, does not add repeat messages for target user.
//	targetUser should be display name of user or "*"
void WebUsers::addSystemMessageToMap(const std::string& targetUser, const std::string& fullMessage)
{
	auto it = systemMessages_.find(targetUser);

	// check for repeat messages
	if(it != systemMessages_.end() && it->second.size() && it->second[it->second.size() - 1].message_ == fullMessage)
		return;  // skip user add

	if(it == systemMessages_.end())  // create first message for target user
	{
		systemMessages_.emplace(
		    std::pair<std::string /*toUser*/, std::vector<SystemMessage>>(targetUser, std::vector<SystemMessage>({SystemMessage(fullMessage)})));
		__COUT__ << targetUser << " Current System Messages count = " << 1 << __E__;
	}
	else  // add message
	{
		__COUT__ << __E__;
		it->second.push_back(SystemMessage(fullMessage));
		__COUT__ << it->first << " Current System Messages count = " << it->second.size() << __E__;
	}
}  // end addSystemMessageToMap

//==============================================================================
// getAllSystemMessages
//	Returns last */global system message for statusing
std::pair<std::string, time_t> WebUsers::getLastSystemMessage() const
{
	__COUT__ << "GetLast number of users with system messages: " << systemMessages_.size() <<
		", first user has " << (systemMessages_.size()?systemMessages_.begin()->second.size():0) << " messages." << __E__;

	auto it = systemMessages_.find("*");
	if(it == systemMessages_.end() || it->second.size() == 0)
		return std::make_pair("",0);

	return std::make_pair(it->second.back().message_, it->second.back().creationTime_);
}  // end getLastSystemMessage()

//==============================================================================
// getAllSystemMessages
//	Returns string all all system messages by user (for remote gateway monitoring)
//	Format: targetUser | time | msg | targetUser | time | msg...etc
std::string WebUsers::getAllSystemMessages()
{
	std::string retStr = "";

	// lock for remainder of scope
	std::lock_guard<std::mutex> lock(systemMessageLock_);

	for(auto& userSysMessages : systemMessages_)
	{
		for(auto& userSysMessage : userSysMessages.second)
		{
			if(userSysMessage.deliveredRemote_) continue; //skip messages already deivered remote
			
			if(retStr.size()) retStr += '|';
			retStr += userSysMessages.first; //target display name
			retStr += "|" + std::to_string(userSysMessage.creationTime_);
			retStr += "|" + userSysMessage.message_;
			userSysMessage.deliveredRemote_ = true;
		}
	}
	return retStr;
} //end getAllSystemMessages()

//==============================================================================
// getSystemMessage
//	Deliver | separated system messages (time | msg | time | msg...etc),
//		if there is any in vector set for user or for wildcard *
//	Empty std::string "" returned if no message for targetUser
//	Note: | is an illegal character and will cause GUI craziness
// 	Note: targetUser is by display name
std::string WebUsers::getSystemMessage(const std::string& targetUser)
{
	//__COUT__ << "Number of users with system messages: " << systemMessages_.size() << __E__;

	// lock for remainder of scope
	std::lock_guard<std::mutex> lock(systemMessageLock_);

	// __COUT__ << "Current System Messages: " << targetUser <<
	// std::endl << std::endl;

	std::string retStr = "";
	int         cnt    = 0;
	char        tmp[32];

	//__COUTV__(targetUser);
	auto it = systemMessages_.find(targetUser);
	// for(auto systemMessagePair:systemMessages_)
	//	__COUT__ << systemMessagePair.first << " " << systemMessagePair.second.size() << " "
	//		<< (systemMessagePair.second.size()?systemMessagePair.second[0].message_:"") << __E__;
	// if(it != systemMessages_.end())
	//	__COUTV__(it->second.size());

	for(uint64_t i = 0; it != systemMessages_.end() && i < it->second.size(); ++i)
	{
		// deliver user specific system message
		if(cnt)
			retStr += "|";
		sprintf(tmp, "%lu", it->second[i].creationTime_);
		retStr += std::string(tmp) + "|" + it->second[i].message_;

		it->second[i].delivered_ = true;
		++cnt;
	}
	it = systemMessages_.find("*");
	for(uint64_t i = 0; it != systemMessages_.end() && i < it->second.size(); ++i)
	{
		// deliver "*" system message
		if(cnt)
			retStr += "|";
		sprintf(tmp, "%lu", it->second[i].creationTime_);
		retStr += std::string(tmp) + "|" + it->second[i].message_;

		++cnt;
	}
	//__COUTV__(retStr);

	systemMessageCleanup();
	//__COUT__ << "Number of users with system messages: " << systemMessages_.size() << __E__;
	return retStr;
}  // end getSystemMessage()

//==============================================================================
// systemMessageCleanup
//	Cleanup messages if delivered, and targetUser != wildcard *
//	For all remaining messages, wait some time before removing (e.g. 300 sec)
void WebUsers::systemMessageCleanup()
{
	__COUTT__ << "Before cleanup number of users with system messages: " << systemMessages_.size() <<
		", first user has " << (systemMessages_.size()?systemMessages_.begin()->second.size():0) << " messages." << __E__;
	for(auto& userMessagesPair : systemMessages_)
	{
		for(uint64_t i = 0; i < userMessagesPair.second.size(); ++i)
			if((userMessagesPair.first != "*" && userMessagesPair.second[i].delivered_) ||      // delivered and != *
			   userMessagesPair.second[i].creationTime_ + SYS_CLEANUP_WILDCARD_TIME < time(0))  // expired
			{

				__COUTT__ << userMessagesPair.first << " at time: " << userMessagesPair.second[i].creationTime_ << 
						" system messages: " <<	userMessagesPair.second.size() << __E__;

				// remove
				userMessagesPair.second.erase(userMessagesPair.second.begin() + i);
				--i;  // rewind
			} //end cleanup loop by message

		__COUTT__ << "User '" << userMessagesPair.first << "' remaining system messages: " <<
				userMessagesPair.second.size() << __E__;
	} //end cleanup loop by user
	__COUTT__ << "After cleanup number of users with system messages: " << systemMessages_.size() <<
		", first user has " << (systemMessages_.size()?systemMessages_.begin()->second.size():0) << " messages." << __E__;
}  // end systemMessageCleanup()

//==============================================================================
// WebUsers::getSecurity
const std::string& WebUsers::getSecurity() { return securityType_; }
//==============================================================================
// WebUsers::loadSecuritySelection
void WebUsers::loadSecuritySelection()
{
	std::string securityFileName = SECURITY_FILE_NAME;
	FILE*       fp               = fopen(securityFileName.c_str(), "r");
	char        line[100]        = "";
	if(fp)
		fgets(line, 100, fp);
	unsigned int i = 0;

	// find first character that is not alphabetic
	while(i < strlen(line) && line[i] >= 'A' && line[i] <= 'z')
		++i;
	line[i] = '\0';  // end string at first illegal character

	if(strcmp(line, SECURITY_TYPE_NONE.c_str()) == 0 || strcmp(line, SECURITY_TYPE_DIGEST_ACCESS.c_str()) == 0)
		securityType_ = line;
	else
		securityType_ = SECURITY_TYPE_DEFAULT;

	__COUT__ << "The current security type is " << securityType_ << __E__;

	if(fp)
		fclose(fp);

	if(securityType_ == SECURITY_TYPE_NONE)
		CareAboutCookieCodes_ = false;
	else
		CareAboutCookieCodes_ = true;

	__COUT__ << "CareAboutCookieCodes_: " << CareAboutCookieCodes_ << __E__;
}  // end loadSecuritySelection()()

//==============================================================================
void WebUsers::NACDisplayThread(const std::string& nac, const std::string& user)
{
	INIT_MF("." /*directory used is USER_DATA/LOG/.*/);
	//////////////////////////////////////////////////////////////////////
	// thread notifying the user about the admin new account code
	// notify for 10 seconds (e.g.)

	// child thread
	int i = 0;
	for(; i < 5; ++i)
	{
		std::this_thread::sleep_for(std::chrono::seconds(2));
		__COUT__ << "\n******************************************************************** " << __E__;
		__COUT__ << "\n******************************************************************** " << __E__;
		__COUT__ << "\n\nNew account code = " << nac << " for user: " << user << "\n" << __E__;
		__COUT__ << "\n******************************************************************** " << __E__;
		__COUT__ << "\n******************************************************************** " << __E__;
	}
}  // end NACDisplayThread()

//==============================================================================
void WebUsers::deleteUserData()
{
	__COUT__ << "$$$$$$$$$$$$$$ Deleting ALL service user data... $$$$$$$$$$$$" << __E__;

	// delete Login data
	std::system(("rm -rf " + (std::string)WEB_LOGIN_DB_PATH + HASHES_DB_PATH + "/*").c_str());
	std::system(("rm -rf " + (std::string)WEB_LOGIN_DB_PATH + USERS_DB_PATH + "/*").c_str());
	std::system(("rm -rf " + (std::string)WEB_LOGIN_DB_PATH + USERS_LOGIN_HISTORY_PATH + "/*").c_str());
	std::system(("rm -rf " + (std::string)WEB_LOGIN_DB_PATH + USERS_PREFERENCES_PATH + "/*").c_str());
	std::system(("rm -rf " + (std::string)WEB_LOGIN_DB_PATH + TOOLTIP_DB_PATH).c_str());

	std::string serviceDataPath = __ENV__("SERVICE_DATA_PATH");
	// delete macro maker folders
	std::system(("rm -rf " + std::string(serviceDataPath) + "/MacroData/").c_str());
	std::system(("rm -rf " + std::string(serviceDataPath) + "/MacroHistory/").c_str());
	std::system(("rm -rf " + std::string(serviceDataPath) + "/MacroExport/").c_str());

	// delete console folders
	std::system(("rm -rf " + std::string(serviceDataPath) + "/ConsolePreferences/").c_str());

	// delete code editor folders
	std::system(("rm -rf " + std::string(serviceDataPath) + "/CodeEditorData/").c_str());

	// delete wizard folders
	std::system(("rm -rf " + std::string(serviceDataPath) + "/OtsWizardData/").c_str());

	// delete progress bar folders
	std::system(("rm -rf " + std::string(serviceDataPath) + "/ProgressBarData/").c_str());

	// delete The Supervisor run folders
	std::system(("rm -rf " + std::string(serviceDataPath) + "/RunNumber/").c_str());
	std::system(("rm -rf " + std::string(serviceDataPath) + "/RunControlData/").c_str());

	// delete Visualizer folders
	std::system(("rm -rf " + std::string(serviceDataPath) + "/VisualizerData/").c_str());

	// DO NOT delete active groups file (this messes with people's configuration world,
	// which is not expected when "resetting user info")  std::system(("rm -rf " +
	// std::string(serviceDataPath) + "/ActiveTableGroups.cfg").c_str());

	// delete Logbook folders
	std::system(("rm -rf " + std::string(__ENV__("LOGBOOK_DATA_PATH")) + "/").c_str());

	__COUT__ << "$$$$$$$$$$$$$$ Successfully deleted ALL service user data $$$$$$$$$$$$" << __E__;
}  // end deleteUserData()
