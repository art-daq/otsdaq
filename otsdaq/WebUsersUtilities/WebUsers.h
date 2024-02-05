#ifndef _ots_Utilities_WebUsers_h_
#define _ots_Utilities_WebUsers_h_

#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/StringMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"
#include "otsdaq/SOAPUtilities/SOAPMessenger.h"
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#include <xgi/Method.h>  //for cgicc::Cgicc
#pragma GCC diagnostic pop

#include <iostream>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#define WEB_LOGIN_DB_PATH std::string(__ENV__("SERVICE_DATA_PATH")) + "/LoginData/"
#define WEB_LOGIN_CERTDATA_PATH std::string(__ENV__("CERT_DATA_PATH"))
#define HASHES_DB_PATH "HashesData/"
#define USERS_DB_PATH "UsersData/"
#define USERS_LOGIN_HISTORY_PATH USERS_DB_PATH + "UserLoginHistoryData/"
#define USERS_PREFERENCES_PATH USERS_DB_PATH + "UserPreferencesData/"
#define TOOLTIP_DB_PATH USERS_DB_PATH + "/TooltipData/"

// clang-format off

namespace ots
{
class HttpXmlDocument;

// WebUsers
//	This class provides the functionality for managing all otsdaq user account preferences
//	and permissions.
class WebUsers
{
  public:
	WebUsers();

	enum
	{
		SESSION_ID_LENGTH     		= 512,
		COOKIE_CODE_LENGTH    		= 512,
		NOT_FOUND_IN_DATABASE 		= uint64_t(-1),
		ACCOUNT_INACTIVE 			= uint64_t(-2),
		ACCOUNT_BLACKLISTED 		= uint64_t(-3),
		ACCOUNT_ERROR_THRESHOLD 	= uint64_t(-5),
		USERNAME_LENGTH       		= 4,
		DISPLAY_NAME_LENGTH   		= 4,
	};

	enum
	{
		MOD_TYPE_UPDATE,
		MOD_TYPE_ADD,
		MOD_TYPE_DELETE
	};

	using permissionLevel_t = uint8_t;
	enum
	{
		PERMISSION_LEVEL_ADMIN 		= WebUsers::permissionLevel_t(-1),  // max permission level!
		PERMISSION_LEVEL_EXPERT   	= 100,
		PERMISSION_LEVEL_USER     	= 10,
		PERMISSION_LEVEL_NOVICE   	= 1,
		PERMISSION_LEVEL_INACTIVE 	= 0,
	};

	static const std::string OTS_OWNER; //defined by environment variable, e.g. experiment name

	static const std::string DEFAULT_ADMIN_USERNAME;
	static const std::string DEFAULT_ADMIN_DISPLAY_NAME;
	static const std::string DEFAULT_ADMIN_EMAIL;
	static const std::string DEFAULT_ITERATOR_USERNAME;
	static const std::string DEFAULT_STATECHANGER_USERNAME;
	static const std::string DEFAULT_USER_GROUP;

	static const std::string REQ_NO_LOGIN_RESPONSE;
	static const std::string REQ_NO_PERMISSION_RESPONSE;
	static const std::string REQ_USER_LOCKOUT_RESPONSE;
	static const std::string REQ_LOCK_REQUIRED_RESPONSE;
	static const std::string REQ_ALLOW_NO_USER;

	static const std::string SECURITY_TYPE_NONE;
	static const std::string SECURITY_TYPE_DIGEST_ACCESS;
	static const std::string SECURITY_TYPE_DEFAULT;

	struct User
	{
		//"Users" database associations ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		//
		//	Maintain list of existing Usernames and associate the following:
		// 		- permissions map of group name to permission level (e.g. users, experts, masters) 0 to 255
		// 		note: all users are at least in group WebUsers::DEFAULT_USER_GROUP
		// 			0 	:= account inactive, not allowed to login (e.g. could be due to too many failed login attempts)
		//			1 	:= normal user
		//			255 := admin for things in group
		// 		permission level is determined by finding the highest permission level number (0 to
		// 		255) for an allowed group.. then that permission level is compared to the threshold.
		//
		// 		- Last Login attempt time, and last USERS_LOGIN_HISTORY_SIZE successful logins
		// 		- Name to display
		// 		- random salt, before first login salt is empty string ""
		// 		- Keep count of login attempt failures. Limit failures per unit time (e.g. 5 per hour)
		//		- Preferences (e.g. color scheme, etc)  Username appends to preferences file, and login history file
		//		- UsersLastModifierUsernameVector - is username of last admin user to modify something about account
		//		- UsersLastModifierTimeVector - is time of last modify by an admin user
		User():lastLoginAttempt_(0),accountCreationTime_(0),loginFailureCount_(0),
				lastModifierTime_(time(0)*100000 + (clock()%100000)) {}

		void setModifier(const std::string& modifierUsername)
		{
			lastModifierUsername_ = modifierUsername;
			lastModifierTime_ = time(0)*100000 + (clock()%100000); //clock used for NAC randomness
		}

		void loadModifierUsername(const std::string& modifierUsername)
		{
			lastModifierUsername_ = modifierUsername;
		}

		time_t& accessModifierTime() { return lastModifierTime_; }

		time_t getModifierTime(bool convertToRealTime = false) const { return (convertToRealTime?lastModifierTime_/100000:lastModifierTime_); }
		const std::string& getModifierUsername() const { return lastModifierUsername_; }
		std::string getNewAccountCode() const {

			if(salt_ != "")  // only give nac if account has not been activated yet with password
				return "";

			char charTimeStr[10];
			sprintf(charTimeStr, "%5.5d", int(lastModifierTime_ & 0xffff));
			return charTimeStr;
		} //end getNewAccountCode()

		std::string 			username_, email_, displayName_, salt_;
		std::map<std::string /*groupName*/, WebUsers::permissionLevel_t> permissions_;
		uint64_t 				userId_;
		time_t					lastLoginAttempt_, accountCreationTime_;
		uint8_t					loginFailureCount_;

	private:
		std::string				lastModifierUsername_;
		time_t					lastModifierTime_;
	}; //end User struct

	struct LoginSession
	{
		//"Login Session" database associations ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		// 	Generate random sessionId when receive a unique user ID (UUID)
		// 	reject UUID that have been used recently (e.g. last 5 minutes)
		// 	Maintain list of active sessionIds and associated UUID
		// 	remove from list if been idle after some time or login attempts (e.g. 5 minutes or
		// 		3 login attempts)  maybe track IP address, to block multiple failed login attempts
		// 		from same IP.  Use sessionId to un-jumble login attempts, lookup using UUID

		std::string 			id_, uuid_, ip_;
		time_t					startTime_;
		uint8_t					loginAttempts_;
	}; //end LoginSession struct

	struct ActiveSession
	{
		//"Active Session" database associations ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		// 	Maintain list of valid cookieCodes and associated user - all requests
		//		must come with a valid cookieCode, else server fails request.
		// 	On logout request, invalidate cookieCode.
		// 	cookieCode expires after some idle time (e.g. 5 minutes) and
		// 		is renewed and possibly changed each request.
		//	"single user - multiple locations" issue resolved using ActiveSessionIndex
		//		 where each independent login starts a new thread of cookieCodes tagged with
		// 		ActiveSessionIndex if cookieCode not refreshed, then return most recent cookie code

		std::string 			cookieCode_, ip_;
		uint64_t				userId_, sessionIndex_;
		time_t					startTime_;
	}; //end ActiveSession struct

	struct Hash
	{
		//"Hashes" database associations ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		// Maintain list of acceptable encoded (SHA-512) salt+user+pw's

		std::string 			hash_;
		time_t					accessTime_; // last login month resolution, blurred by 1/2 month
	}; //end Hash struct

	enum
	{
		SYS_CLEANUP_WILDCARD_TIME = 30,  // 30 seconds
	};

	struct SystemMessage
	{
		// Members for system messages ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		//	Set of vectors to delivers system messages to active users of the Web Gui
		//		When a message is generated, systemMessageLock is set,
		//			message is added and the vector set deliveredFlag = false,
		//			and systemMessageLock is unset.
		//		When a message is delivered deliveredFlag = true,
		//		During systemMessageCleanup(), systemMessageLock is set, delivered messages are removed,
		//			and systemMessageLock is unset.
		//"SystemMessage" database associations ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
		// Maintain list of user system messages:
		//	time, message, deliveredFlag

		SystemMessage(const std::string& message)
		: message_			(message)
		, creationTime_		(time(0))
		, delivered_		(false)
		{} //end constructor

		std::string 			message_;
		time_t					creationTime_;
		bool					delivered_; //flag
	}; //end SystemMessage struct

	void        			addSystemMessage			(const std::string& targetUsersCSV, const std::string& message);
	void        			addSystemMessage			(const std::string& targetUsersCSV, const std::string& subject, const std::string& message, bool doEmail);
	void        			addSystemMessage			(const std::vector<std::string>& targetUsers, const std::string& subject, const std::string& message, bool doEmail);
	std::string 			getSystemMessage			(const std::string& targetUser);

  private:
	void					addSystemMessageToMap		(const std::string& targetUser, const std::string& fullMessage); //private because target already vetted
	void                   	systemMessageCleanup		(void);
	std::mutex				systemMessageLock_;
	std::map<std::string /*toUserDisplayName*/,std::vector<SystemMessage>> systemMessages_;


  public:


	struct RequestUserInfo
	{
		// WebUsers is a "Friend" class of RequestUserInfo so has access to private
		// members.
		friend class WebUsers;

		RequestUserInfo(const std::string& requestType, const std::string& cookieCode)
		    : requestType_(requestType)
		    , cookieCode_(cookieCode)
		    , uid_(-1)  // init to invalid user, since only WebUser owner will have access
		                // to uid. RemoteWebUsers will see invalid uid.
		{
		}

		//------- setters --------//
		//===========================================
		// setGroupPermissionLevels
		bool setGroupPermissionLevels(const std::string& groupPermissionLevelsString)
		{
			//__COUTV__(groupPermissionLevelsString);
			permissionLevel_ = 0;  // default to inactive, i.e. no access

			StringMacros::getMapFromString(  // re-factor membership string to set
			    groupPermissionLevelsString,
			    groupPermissionLevelMap_);
			getGroupPermissionLevel();  // setup permissionLevel_

			//__COUTV__((unsigned int)permissionLevel_);
			return true;  // was fully setup
		}                 // end setGroupPermissionLevels()

		//------- getters --------//
		const std::map<std::string /*groupName*/, WebUsers::permissionLevel_t>&
		getGroupPermissionLevels() const
		{
			return groupPermissionLevelMap_;
		}
		//===========================================
		// getGroupPermissionLevel
		//	sets up permissionLevel based on already prepared RequestUserInfo members
		const WebUsers::permissionLevel_t& getGroupPermissionLevel()
		{
			permissionLevel_ = 0;  // default to inactive, i.e. no access

			// check groups allowed
			//	i.e. if user is a member of one of the groups allowed
			//			then consider for highest permission level
			bool matchedAcceptGroup = false;
			for(const auto& userGroupPair : groupPermissionLevelMap_)
				if(StringMacros::inWildCardSet(  // if group is in allowed groups
				       userGroupPair.first,
				       groupsAllowed_) &&  // AND...
				   userGroupPair.second >
				       permissionLevel_)  // if is a new high level, then...
				{
					permissionLevel_ =
					    userGroupPair.second;  // take as new permission level
					matchedAcceptGroup = true;
				}

			// if no group match in groups allowed, then failed
			if(!matchedAcceptGroup && groupsAllowed_.size())
			{
				__COUT_INFO__
				    << "User (@" << ip_
				    << ") has insufficient group permissions: user is in these groups... "
				    << StringMacros::mapToString(groupPermissionLevelMap_)
				    << " and the allowed groups are... "
				    << StringMacros::setToString(groupsAllowed_) << std::endl;
				return permissionLevel_;
			}

			// if no access groups specified, then check groups disallowed
			if(!groupsAllowed_.size())
			{
				for(const auto& userGroupPair : groupPermissionLevelMap_)
					if(StringMacros::inWildCardSet(userGroupPair.first,
					                               groupsDisallowed_))
					{
						__COUT_INFO__
						    << "User (@" << ip_
						    << ") is in a disallowed group: user is in these groups... "
						    << StringMacros::mapToString(groupPermissionLevelMap_)
						    << " and the disallowed groups are... "
						    << StringMacros::setToString(groupsDisallowed_) << std::endl;
						return permissionLevel_;
					}
			}

			// if no groups have been explicitly allowed nor disallowed
			// then permission level should come from WebUsers::DEFAULT_USER_GROUP
			auto findIt = groupPermissionLevelMap_.find(WebUsers::DEFAULT_USER_GROUP);
			if(findIt != groupPermissionLevelMap_.end())
			{
				// found default group, take permission level
				permissionLevel_ = findIt->second;
			}

			return permissionLevel_;
		}  // end getGroupPermissionLevel()

		inline bool isInactive() const
		{
			return permissionLevel_ == WebUsers::PERMISSION_LEVEL_INACTIVE;
		}
		inline bool isAdmin() const
		{
			return permissionLevel_ == WebUsers::PERMISSION_LEVEL_ADMIN;
		}

		// members extracted from supervisor properties on a per request type basis
		const std::string& requestType_;
		std::string        cookieCode_;

		bool automatedCommand_, NonXMLRequestType_, NoXmlWhiteSpace_;
		bool checkLock_, requireLock_, allowNoUser_, requireSecurity_;

		std::set<std::string> groupsAllowed_, groupsDisallowed_;

		WebUsers::permissionLevel_t permissionLevel_, permissionsThreshold_;
		std::string                 ip_;
		uint64_t    uid_ /*only WebUser owner has access to uid, RemoteWebUsers do not*/;
		std::string username_, displayName_, usernameWithLock_;
		uint64_t    activeUserSessionIndex_;

	  private:
		std::map<std::string /*groupName*/, WebUsers::permissionLevel_t>
		    groupPermissionLevelMap_;
	}; //end RequestUserInfo struct

	// for the gateway supervisor to check request access
	// if false, gateway request handling code should just return.. out is handled on
	// false; on true, out is untouched
	bool xmlRequestOnGateway(cgicc::Cgicc&              cgi,
	                         std::ostringstream*        out,
	                         HttpXmlDocument*           xmldoc,
	                         WebUsers::RequestUserInfo& userInfo);

  public:

	// used by gateway and other supervisors to verify requests consistently
	static void initializeRequestUserInfo(cgicc::Cgicc&              cgi,
	                                      WebUsers::RequestUserInfo& userInfo);
	static bool checkRequestAccess(cgicc::Cgicc&              cgi,
	                               std::ostringstream*        out,
	                               HttpXmlDocument*           xmldoc,
	                               WebUsers::RequestUserInfo& userInfo,
	                               bool                       isWizardMode = false,
								   const std::string& 		  wizardModeSequence = "");

	void        createNewAccount(const std::string& username,
	                             const std::string& displayName,
	                             const std::string& email);
	void        cleanupExpiredEntries(std::vector<std::string>* loggedOutUsernames = 0);
	std::string createNewLoginSession(const std::string& uuid, const std::string& ip);

	uint64_t attemptActiveSession(const std::string& uuid,
	                              std::string&       jumbledUser,
	                              const std::string& jumbledPw,
	                              std::string&       newAccountCode,
	                              const std::string& ip);
	uint64_t attemptActiveSessionWithCert(const std::string& uuid,
	                                      std::string&       jumbledEmail,
	                                      std::string&       cookieCode,
	                                      std::string&       username,
	                                      const std::string& ip);
	uint64_t isCookieCodeActiveForLogin(const std::string& uuid,
	                                    std::string&       cookieCode,
	                                    std::string&       username);
	bool     cookieCodeIsActiveForRequest(
	        std::string& cookieCode,
	        std::map<std::string /*groupName*/, WebUsers::permissionLevel_t>*
	                           userPermissions        = 0,
	        uint64_t*          uid                    = 0,
	        const std::string& ip                     = "0",
	        bool               refresh                = true,
	        std::string*       userWithLock           = 0,
	        uint64_t*          activeUserSessionIndex = 0);
	uint64_t cookieCodeLogout(const std::string& cookieCode,
	                          bool               logoutOtherUserSessions,
	                          uint64_t*          uid = 0,
	                          const std::string& ip  = "0");
	bool     checkIpAccess(const std::string& ip);

	std::string getUsersDisplayName(uint64_t uid);
	std::string getUsersUsername(uint64_t uid);
	uint64_t    getActiveSessionCountForUser(uint64_t uid);
	std::map<std::string /*groupName*/, WebUsers::permissionLevel_t>
	            getPermissionsForUser(uint64_t uid);
	void        insertSettingsForUser(uint64_t         uid,
	                                  HttpXmlDocument* xmldoc,
	                                  bool             includeAccounts = false);
	std::string getGenericPreference(uint64_t           uid,
	                                 const std::string& preferenceName,
	                                 HttpXmlDocument*   xmldoc = 0) const;

	void        changeSettingsForUser(uint64_t           uid,
	                                  const std::string& bgcolor,
	                                  const std::string& dbcolor,
	                                  const std::string& wincolor,
	                                  const std::string& layout,
	                                  const std::string& syslayout);
	void        setGenericPreference(uint64_t           uid,
	                                 const std::string& preferenceName,
	                                 const std::string& preferenceValue);
	static void tooltipCheckForUsername(const std::string& username,
	                                    HttpXmlDocument*   xmldoc,
	                                    const std::string& srcFile,
	                                    const std::string& srcFunc,
	                                    const std::string& srcId);
	static void tooltipSetNeverShowForUsername(const std::string& username,
	                                           HttpXmlDocument*   xmldoc,
	                                           const std::string& srcFile,
	                                           const std::string& srcFunc,
	                                           const std::string& srcId,
	                                           bool               doNeverShow,
	                                           bool               temporarySilence);

	void modifyAccountSettings(uint64_t           actingUid,
	                           uint8_t            cmd_type,
	                           const std::string& username,
	                           const std::string& displayname,
	                           const std::string& email,
	                           const std::string& permissions);
	bool setUserWithLock(uint64_t actingUid, bool lock, const std::string& username);
	std::string getUserWithLock(void) { return usersUsernameWithLock_; }

	std::string getActiveUsersString(void);

	bool getUserInfoForCookie(std::string& cookieCode,
	                          std::string* userName,
	                          std::string* displayName        = 0,
	                          uint64_t*    activeSessionIndex = 0);

	bool        isUsernameActive(const std::string& username) const;
	bool        isUserIdActive(uint64_t uid) const;
	uint64_t    getAdminUserID(void);
	const std::string& getSecurity(void);

	static void deleteUserData(void);

	static void resetAllUserTooltips(const std::string& userNeedle = "*");
	static void silenceAllUserTooltips(const std::string& username);

	static void NACDisplayThread(const std::string& nac, const std::string& user);

	void saveActiveSessions(void);
	void loadActiveSessions(void);

  private:
	inline WebUsers::permissionLevel_t getPermissionLevelForGroup(
	    const std::map<std::string /*groupName*/, WebUsers::permissionLevel_t>& permissionMap,
	    const std::string& groupName = WebUsers::DEFAULT_USER_GROUP);
	inline bool isInactiveForGroup(
			const std::map<std::string /*groupName*/, WebUsers::permissionLevel_t>& permissionMap,
	    const std::string& groupName = WebUsers::DEFAULT_USER_GROUP);
	inline bool isAdminForGroup(
			const std::map<std::string /*groupName*/, WebUsers::permissionLevel_t>& permissionMap,
	    const std::string& groupName = WebUsers::DEFAULT_USER_GROUP);

	void         loadSecuritySelection(void);
	void         loadUserWithLock(void);
	unsigned int hexByteStrToInt(const char* h);
	void         intToHexStr(uint8_t i, char* h);
	std::string  sha512(const std::string& user,
	                    const std::string& password,
	                    std::string&       salt);
	std::string  dejumble(const std::string& jumbledUser, const std::string& sessionId);
	std::string  createNewActiveSession(uint64_t           uid,
	                                    const std::string& ip      = "0",
	                                    uint64_t           asIndex = 0);
	bool         addToHashesDatabase(const std::string& hash);
	std::string  genCookieCode(void);
	std::string  refreshCookieCode(unsigned int i, bool enableRefresh = true);
	bool deleteAccount(const std::string& username, const std::string& displayName);
	void incrementIpBlacklistCount(const std::string& ip);

	void saveToDatabase(FILE*              fp,
	                    const std::string& field,
	                    const std::string& value,
	                    uint8_t            type       = DB_SAVE_OPEN_AND_CLOSE,
	                    bool               addNewLine = true);
	bool saveDatabaseToFile(uint8_t db);
	bool loadDatabases(void);

	uint64_t searchUsersDatabaseForUsername			(const std::string& username) const;
	uint64_t searchUsersDatabaseForDisplayName		(const std::string& displayName) const;
	uint64_t searchUsersDatabaseForUserEmail		(const std::string& useremail) const;
	uint64_t searchUsersDatabaseForUserId			(uint64_t uid) const;
	uint64_t searchLoginSessionDatabaseForUUID		(const std::string& uuid) const;
	uint64_t searchHashesDatabaseForHash			(const std::string& hash);
	uint64_t searchActiveSessionDatabaseForCookie	(const std::string& cookieCode) const;

	static std::string getTooltipFilename(const std::string& username,
	                                      const std::string& srcFile,
	                                      const std::string& srcFunc,
	                                      const std::string& srcId);
	std::string        getUserEmailFromFingerprint(const std::string& fingerprint);

	enum
	{
		DB_USERS,
		DB_HASHES
	};

	enum
	{
		DB_SAVE_OPEN_AND_CLOSE,
		DB_SAVE_OPEN,
		DB_SAVE_CLOSE
	};

	std::unordered_map<std::string, std::string> certFingerprints_;

	static const std::vector<std::string> UsersDatabaseEntryFields_, HashesDatabaseEntryFields_;
	bool                     CareAboutCookieCodes_;
	std::string              securityType_;

	//"Login Session" database associations ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	std::vector<LoginSession> LoginSessions_;

	// Generate random sessionId when receive a unique user ID (UUID)
	// reject UUID that have been used recently (e.g. last 5 minutes)
	// Maintain list of active sessionIds and associated UUID
	// remove from list if been idle after some time or login attempts (e.g. 5 minutes or
	// 3 login attempts)  maybe track IP address, to block multiple failed login attempts
	// from same IP.  Use sessionId to un-jumble login attempts, lookup using UUID
//	std::vector<std::string> LoginSessionIdVector, LoginSessionUUIDVector,
//	    LoginSessionIpVector;
//	std::vector<time_t>  LoginSessionStartTimeVector;
//	std::vector<uint8_t> LoginSessionAttemptsVector;
	enum
	{
		LOGIN_SESSION_EXPIRATION_TIME = 5 * 60,  // 5 minutes
		LOGIN_SESSION_ATTEMPTS_MAX = 5,  // 5 attempts on same session, forces new session
	};

	//"Active Session" database associations ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	std::vector<ActiveSession> ActiveSessions_;

	// Maintain list of valid cookieCodes and associated user
	// all request must come with a valid cookieCode, else server fails request
	// On logout request, invalidate cookieCode
	// cookieCode expires after some idle time (e.g. 5 minutes) and
	// is renewed and possibly changed each request
	//"single user - multiple locations" issue resolved using ActiveSessionIndex
	// where each independent login starts a new thread of cookieCodes tagged with
	// ActiveSessionIndex  if cookieCode not refreshed, then return most recent cookie
	// code
//	std::vector<std::string> ActiveSessionCookieCodeVector, ActiveSessionIpVector;
//	std::vector<uint64_t>    ActiveSessionUserIdVector, ActiveSessionIndex;
//	std::vector<time_t>      ActiveSessionStartTimeVector;
	enum
	{
		ACTIVE_SESSION_EXPIRATION_TIME = 120 * 60,  // 120 minutes, cookie is changed
		                                            // every half period of
		                                            // ACTIVE_SESSION_EXPIRATION_TIME
		ACTIVE_SESSION_COOKIE_OVERLAP_TIME =
		    10 * 60,  // 10 minutes of overlap when new cookie is generated
		ACTIVE_SESSION_STALE_COOKIE_LIMIT =
		    10,  // 10 stale cookies allowed for each active user
	};

	//"Users" database associations ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	std::vector<User>	Users_;
	// Maintain list of acceptable Usernames and associate:
	// permissions
	// map of group name to permission level (e.g. users, experts, masters) 0 to 255
	// note: all users are at least in group WebUsers::DEFAULT_USER_GROUP
	// 0 	:= account inactive, not allowed to login (e.g. could be due to too many
	// failed login attempts)  1 	:= normal user  255 	:= admin for things in group
	// permission level is determined by finding the highest permission level number (0 to
	// 255) for an allowed 	group.. then that permission level is compared to the
	// threshold
	//
	// Last Login attempt time, and last USERS_LOGIN_HISTORY_SIZE successful logins
	// Name to display
	// random salt, before first login salt is empty string ""
	// Keep count of login attempt failures. Limit failures per unit time (e.g. 5 per
	// hour)  Preferences (e.g. color scheme, etc)  Username appends to preferences file,
	// and login history file  UsersLastModifierUsernameVector - is username of last
	// master user to modify something about account  UsersLastModifierTimeVector - is
	// time of last modify by a master user
//	std::vector<std::string> UsersUsernameVector, UsersUserEmailVector,
//	    UsersDisplayNameVector, UsersSaltVector, UsersLastModifierUsernameVector;
//	std::vector<std::map<std::string /*groupName*/, WebUsers::permissionLevel_t> >
//	                      UsersPermissionsVector;
//	std::vector<uint64_t> UsersUserIdVector;
//	std::vector<time_t>   UsersLastLoginAttemptVector, UsersAccountCreatedTimeVector,
//	    UsersLastModifiedTimeVector;
//	std::vector<uint8_t> UsersLoginFailureCountVector;
	uint64_t             usersNextUserId_;
	enum
	{
		USERS_LOGIN_HISTORY_SIZE  = 20,
		USERS_GLOBAL_HISTORY_SIZE = 1000,
		USERS_MAX_LOGIN_FAILURES  = 20,
	};
	std::string usersUsernameWithLock_;

	std::vector<std::string> UsersLoggedOutUsernames_;

	//"Hashes" database associations ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	std::vector<Hash> 		Hashes_;

	// Maintain list of acceptable encoded (SHA-512) salt+user+pw's
//	std::vector<std::string> HashesVector;
//	std::vector<time_t>      HashesAccessTimeVector;

	enum
	{
		IP_BLACKLIST_COUNT_THRESHOLD = 200,
	};
	std::map<std::string /*ip*/, uint32_t /*errorCount*/> ipBlacklistCounts_;

	std::mutex				webUserMutex_;
};
}  // namespace ots

// clang-format on

#endif
