#ifndef _ots_ConfigurationManagerRW_h_
#define _ots_ConfigurationManagerRW_h_

#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"

// clang-format off

namespace ots
{
struct TableInfo
{
	TableInfo()
	    :  // constructor
	    tablePtr_(0)
	{
	}

	std::set<TableVersion> 				versions_;
	TableBase*             				tablePtr_;
	std::string            				accumulatedWarnings_;
};  //end TableInfo struct

struct GroupInfo
{
	friend class
	    ConfigurationManagerRW;  //ConfigurationManagerRW can access GroupInfo private members

	GroupInfo()
	    :  //constructor
	    latestKeyGroupAuthor_			(ConfigurationManager::UNKNOWN_INFO)
	    , latestKeyGroupComment_		(ConfigurationManager::UNKNOWN_INFO)
	    , latestKeyGroupCreationTime_	(ConfigurationManager::UNKNOWN_TIME)
	    , latestKeyGroupTypeString_		(ConfigurationManager::GROUP_TYPE_NAME_UNKNOWN)
	{
	}

	const std::set<TableGroupKey>& 	getKeys() 						const { return keys_; }
	const TableGroupKey&           	getLatestKey() 					const { return latestKey_; }
	const std::string& 				getLatestKeyGroupAuthor() 		const { return latestKeyGroupAuthor_; }
	const std::string& 				getLatestKeyGroupComment() 		const { return latestKeyGroupComment_; }
	const std::string& 				getLatestKeyGroupCreationTime() const { return latestKeyGroupCreationTime_;	}
	const std::string& 				getLatestKeyGroupTypeString() 	const {	return latestKeyGroupTypeString_; }
	const std::map<std::string /*name*/,
		TableVersion /*version*/>&	getLatestKeyMemberMap() 		const { return latestKeyMemberMap_;	}
	TableGroupKey 					getLastKey() 					const {	if(keys_.size()) return *(keys_.rbegin()); else	return TableGroupKey();	}

  private:
	std::set<TableGroupKey> 			keys_;
	TableGroupKey           			latestKey_;
	std::string             			latestKeyGroupAuthor_, latestKeyGroupComment_,
										latestKeyGroupCreationTime_, latestKeyGroupTypeString_;
	std::map<std::string /*name*/,
		TableVersion /*version*/> 		latestKeyMemberMap_;
};  //end GroupInfo struct

#define __GET_TABLE_PTR__(X) getTablePtr<X>(QUOTE(X))

//==============================================================================
/// ConfigurationManagerRW
///	This is the ConfigurationManger with write access
///	This class inherits all public function from ConfigurationManager
/// and is a "Friend" class of ConfigurationManager so has access to private members.
class ConfigurationManagerRW : public ConfigurationManager
{
  public:
	ConfigurationManagerRW(const std::string& username);


	//==============================================================================
	/// Getters
	const std::string&      					getUsername						(void) const { return username_; }
	ConfigurationInterface* 					getConfigurationInterface		(void) const { return theInterface_; }

	const std::map<std::string, TableInfo>& 	getAllTableInfo					(bool refresh = false,
																				std::string* accumulatedWarnings = 0,
																				const std::string& errorFilterName = "",
																				bool getGroupKeys = false,
																				bool getGroupInfo = false,
																				bool initializeActiveGroups = false);
	std::map<std::string /*tableName*/,
			 std::map<std::string /*aliasName*/,
			 TableVersion /*version*/> >		getVersionAliases				(void) const;

	template<class T>
	T* 											getTablePtr						(const std::string& tableName) { return (T*)getTableByName(tableName); }
	TableBase*    								getVersionedTableByName			(const std::string& tableName, TableVersion version, bool looseColumnMatching = false, std::string* accumulatedErrors = 0, bool getRawData = false, bool touchLastAccessTime = true) /* make protected function accessible to RW users */	{ return ConfigurationManager::getVersionedTableByName(tableName, version, looseColumnMatching, accumulatedErrors, getRawData, touchLastAccessTime);	}
	time_t    									getVersionCreationTime			(const std::string& tableName, TableVersion version);
	time_t    									getVersionLastAccessTime		(const std::string& tableName, TableVersion version);
	void    									preloadVersionCreationTimes		(void);  ///< parallel load of all version creation times into the process-wide cache
	TableBase*    								getTableByName					(const std::string& tableName);
	TableGroupKey 								findTableGroup					(const std::string& groupName,
																				 const std::map<std::string, TableVersion>& 					groupMembers,
																				 const std::map<std::string /*name*/, std::string /*alias*/>& 	groupAliases =	std::map<std::string /*name*/, std::string /*alias*/>());
	TableBase* 									getMetadataTable				(TableVersion fillVersion = TableVersion()); ///< created for use in otsdaq_flatten_system_aliases and otsdaq_export_system_aliases, e.g.

	//==============================================================================
	/// Setters
	const std::string&      					setUsername						(const std::string& username) { username_ = username; return username_; }

	//==============================================================================
	/// modifiers of generic TableBase
	TableVersion 								saveNewTable					(const std::string& tableName, TableVersion temporaryVersion = TableVersion(), bool makeTemporary = false);  ///<, bool saveToScratchVersion = false);
	TableVersion 								saveModifiedVersion				(
																				const std::string&      tableName,
																				TableVersion            originalVersion,
																				bool                    makeTemporary,
																				TableBase*              config,
																				TableVersion            temporaryModifiedVersion,
																				bool                    ignoreDuplicates = false,
																				bool 					lookForEquivalent = false,
																				bool*					foundEquivalent = nullptr);

	TableVersion 								updateTableCells				(
																				const std::string& 	tableName,
																				const std::map<std::string /*uid*/,
																					std::map<std::string /*colName*/, std::string /*value*/>>& cellUpdates,
																				const std::string& 	author,
																				TableVersion 		sourceVersion = TableVersion(),
																				const std::string& 	versionAlias = "",
																				const std::string& 	sourceAlias = "");

	TableVersion 								copyViewToCurrentColumns		(const std::string& tableName, TableVersion sourceVersion);
	void         								eraseTemporaryVersion			(const std::string& tableName, TableVersion targetVersion = TableVersion());
	void         								clearCachedVersions				(const std::string& tableName);
	void         								clearAllCachedVersions			(void);

	//==============================================================================
	/// modifiers of table groups
	void 										activateTableGroup				(const std::string& tableGroupName, TableGroupKey tableGroupKey, std::string* accumulatedTreeErrors = 0, std::string* groupTypeString = 0);

	TableVersion 								createTemporaryBackboneView		(TableVersion sourceViewVersion = TableVersion());  ///< -1, from MockUp, else from valid backbone view version
	TableVersion 								saveNewBackbone					(TableVersion temporaryVersion 	= TableVersion());

	//==============================================================================
	/// modifiers of a table group based on alias, e.g. "Physics"
	TableGroupKey 								saveNewTableGroup				(const std::string& groupName, 	std::map<std::string, TableVersion>& 					groupMembers,
																												const std::string& 										groupComment = TableViewColumnInfo::DATATYPE_COMMENT_DEFAULT,
																												std::map<std::string /*table*/, std::string /*alias*/>* groupAliases = 0);

	//==============================================================================
	/// public group cache handling
	const GroupInfo&                        	getGroupInfo					(const std::string& groupName, bool attemptToReloadKeys = false);
	const std::map<std::string, GroupInfo>& 	getAllGroupInfo					(void) { return allGroupInfo_; }
	void 										loadTableGroup					(
																				const std::string&                                     tableGroupName,
																				const TableGroupKey&                                   tableGroupKey,
																				bool                                                   doActivate         = false,
																				std::map<std::string, TableVersion>*                   groupMembers       = 0,
																				ProgressBar*                                           progressBar        = 0,
																				std::string*                                           accumulateWarnings = 0,
																				std::string*                                           groupComment       = 0,
																				std::string*                                           groupAuthor        = 0,
																				std::string*                                           groupCreateTime    = 0,
																				bool                                                   doNotLoadMember    = false,
																				std::string*                                           groupTypeString    = 0,
																				std::map<std::string /*name*/, std::string /*alias*/>* groupAliases       = 0,
																				ConfigurationManager::LoadGroupType					   groupTypeToLoad    = ConfigurationManager::LoadGroupType::ALL_TYPES,
																				bool												   ignoreVersionTracking = false);

	void 										testXDAQContext					(void);  ///< for debugging

  public:
	static void 								loadTableInfoThread				(ConfigurationManagerRW* 			cfgMgr,
																				std::string 						tableName,
																				TableBase*        				    existingTable,
																				std::shared_ptr<ots::TableInfo>		tableInfo,
																				std::shared_ptr<std::atomic<bool>> 	threadDone);
	static void 								loadTableGroupThread			(ConfigurationManagerRW* 			cfgMgr,
																				std::string							groupName,
																				ots::TableGroupKey					groupKey,
																				std::shared_ptr<ots::GroupInfo>		theGroupInfo,
																				std::shared_ptr<std::atomic<bool>> 	theThreadDone);
	static void 								compareTableGroupThread			(ConfigurationManagerRW* 			cfgMgr,
																				std::string 						groupName,
																				ots::TableGroupKey 					groupKeyToCompare,
																				const std::map<std::string, TableVersion>& groupMemberMap,
																				const std::map<std::string /*name*/, std::string /*alias*/>& memberTableAliases,
																				std::atomic<bool>* 					theFoundIdentical,
																				ots::TableGroupKey* 				theIdenticalKey,
																				std::mutex* 						theThreadMutex,
																				std::shared_ptr<std::atomic<bool>> 	theThreadDone);
  private:

	//==============================================================================
	/// private members
	std::map<std::string, TableInfo> 								allTableInfo_; //local cache of table info
	std::map<std::string, GroupInfo> 								allGroupInfo_; //local cache of group info

	//process-wide version creation time cache: persistent versions are immutable, so
	//	creation times can be cached forever; shared by all instances (i.e. all user
	//	sessions) within this process
	static std::mutex												versionCreationTimeCacheMutex_;
	static std::map<std::string /*tableName*/,
		std::map<TableVersion, time_t>>								versionCreationTimeCache_;

	static std::atomic<bool>										firstTimeConstructed_;
};

//==============================================================================
/// TableEditStruct public class
///
struct TableEditStruct
{
	/// everything needed for editing a table
	TableBase*   table_;
	TableView*   tableView_;
	TableVersion temporaryVersion_, originalVersion_;
	bool         createdTemporaryVersion_;  ///< indicates if temp version was created here
	bool         modified_;                 ///< indicates if temp version was modified
	std::string  tableName_;
	const std::string mfSubject_;

	/////
	TableEditStruct()
	{
		__SS__ << "impossible!" << std::endl;
		ss << StringMacros::stackTrace();
		__SS_THROW__;
	}
	TableEditStruct(const std::string& tableName, ConfigurationManagerRW* cfgMgr, bool markModified = false)
		: createdTemporaryVersion_(false), modified_(markModified), tableName_(tableName)
		, mfSubject_(cfgMgr->getUsername())
	{
		//__COUT__ << "Creating Table-Edit Struct for " << tableName_ << std::endl;
		table_ = cfgMgr->getTableByName(tableName_);

		//if no active version or if not temporary, setup new temporary version
		if(!table_->isActive() ||
				!(originalVersion_ = table_->getView().getVersion()).isTemporaryVersion())
		{
			//__COUT__ << "Original '" << tableName_ << "' version is v" << originalVersion_ << std::endl;

			// create temporary version for editing
			temporaryVersion_ = table_->createTemporaryView(originalVersion_);
			cfgMgr->saveNewTable(
				tableName_,
				temporaryVersion_,
				true);  ///< proper bookkeeping for temporary version with the new version

			__COUT__ << "Created '" << tableName_ << "' temporary version " << temporaryVersion_ << std::endl;
			createdTemporaryVersion_ = true;
		}
		//else  // else table is already temporary version
			//__COUT__ << "Using '" << tableName_ << "' temporary version " << temporaryVersion_ << std::endl;

		tableView_ = table_->getViewP();
	}
};  // end TableEditStruct declaration

//==============================================================================
/// GroupEditStruct public class
///
struct GroupEditStruct
{
	/// everything needed for editing a group and its tables
private:
	std::map<std::string, TableVersion> 	groupMembers_;
	std::map<std::string, TableEditStruct> 	groupTables_;
public:
	const ConfigurationManager::GroupType	groupType_;
	const std::string 						originalGroupName_;
	const TableGroupKey						originalGroupKey_;
private:
	ConfigurationManagerRW* 				cfgMgr_;
	const std::string 						mfSubject_;

public:
	/////
	GroupEditStruct()
		: groupType_(ConfigurationManager::GroupType::CONFIGURATION_TYPE) {__SS__ << "impossible!" << __E__; __SS_THROW__;}
	GroupEditStruct(const ConfigurationManager::GroupType& groupType, ConfigurationManagerRW* cfgMgr);

	~GroupEditStruct();

	void 				dropChanges				(void);
	void 				saveChanges				(
												const std::string& 	groupNameToSave,
												TableGroupKey& 		newGroupKey,
												bool* 				foundEquivalentGroupKey 	= nullptr,
												bool 				activateNewGroup 			= false,
												bool 				updateGroupAliases 			= false,
												bool				updateTableAliases 			= false,
												TableGroupKey* 		newBackboneKey 				= nullptr,
												bool* 				foundEquivalentBackboneKey 	= nullptr,
												std::string* 		accumulatedWarnings 		= nullptr);

	TableEditStruct& 	getTableEditStruct		(const std::string& tableName, bool markModified = false);

};  // end GroupEditStruct declaration

// clang-format on
}  // namespace ots

#endif
