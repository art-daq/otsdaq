#include <dirent.h>
#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include "otsdaq/ConfigurationInterface/ConfigurationInterface.h"
#include "otsdaq/ConfigurationInterface/ConfigurationManagerRW.h"

// usage:
// otsdaq_import_system_aliases <pathOfDatabaseToImport> <(optional) prependLabel>
//
// all system aliases are imported to current db and are prepended with a label
//

using namespace ots;

void ImportSystemAliasTableGroups(int argc, char* argv[])
{
	__COUT__ << "=================================================\n";
	__COUT__ << "=================================================\n";
	__COUT__ << "=================================================\n";
	__COUT__ << "Importing External System Aliases!" << std::endl;

	std::string       prependBaseName         = "Imported";
	const std::string groupAliasesTableName   = ConfigurationManager::GROUP_ALIASES_TABLE_NAME;
	const std::string versionAliasesTableName = ConfigurationManager::VERSION_ALIASES_TABLE_NAME;

	std::cout << "\n\nusage: Two arguments:\n\n\t otsdaq_import_system_aliases "
	             "<path_to_import_database_folder> <path_to_active_groups_file> "
	             "<import_prepend_base_name (optional)> \n\n"
	          << "\t\t Default values: \n\t\t\timport_prepend_base_name = \"" << prependBaseName << "\" "
	          << "\n\n"
	          << "\t\tfor example:\n\n"
	          << "\t\t\totsdaq_import_system_aliases "
	             "~/databaseToImport/filesystemdb/test_db "
	             "~/UserDataToImport/ServiceData/ActiveTableGroups.cfg"
	          << __E__;

	std::cout << "\n\n\tExample active groups file content:\n\n"
	          << "testContext\n2\nTableEditWizBackbone\n3\ndefaultConfig\n1\n\n"
	          << __E__;

	std::cout << "\n\nNote: This assumes artdaq db file type interface. "
	          << "The current database/ will be backed up to database_<linuxtime>/ "
	          << "before importing the active groups.\n\n"
	          << __E__;

	__COUTV__(argc);
	for(int i = 0; i < argc; i++)
		std::cout << "argv[" << i << "] = " << argv[i] << std::endl;

	if(argc < 3)
	{
		__COUT__ << "Must provide at least two parameters (the database and active "
		            "groups files for the import).\n\n\n"
		         << __E__;
		return;
	}

	// remove trailing /'s
	while(strlen(argv[1]) && argv[1][strlen(argv[1]) - 1] == '/')
		argv[1][strlen(argv[1]) - 1] = '\0';

	// determine if "h"elp was first parameter
	std::string pathToImportDatabase = argv[1];
	if(pathToImportDatabase.size() && (pathToImportDatabase[0] == 'h' || pathToImportDatabase[0] == '-'))
	{
		__COUT__ << "Recognized parameter 1. as a 'help' option. Usage was printed. Exiting." << std::endl;
		return;
	}

	std::string pathToImportActiveGroups = "";
	if(argc >= 3)
		pathToImportActiveGroups = argv[2];
	if(argc >= 4)
		prependBaseName = argv[3];

	__COUTV__(pathToImportDatabase);
	__COUTV__(pathToImportActiveGroups);
	__COUTV__(prependBaseName);

	//==============================================================================
	// Define environment variables
	//	Note: normally these environment variables are set by StartOTS.sh

	// These are needed by
	// otsdaq/otsdaq/ConfigurationDataFormats/ConfigurationInfoReader.cc [207]
	setenv("CONFIGURATION_TYPE", "File", 1);  // Can be File, Database, DatabaseTest
	setenv("CONFIGURATION_DATA_PATH", (std::string(__ENV__("USER_DATA")) + "/ConfigurationDataExamples").c_str(), 1);
	setenv("TABLE_INFO_PATH", (std::string(__ENV__("USER_DATA")) + "/TableInfo").c_str(), 1);
	////////////////////////////////////////////////////

	// Some configuration plug-ins use __ENV__("SERVICE_DATA_PATH") in init() so define it
	setenv("SERVICE_DATA_PATH", (std::string(__ENV__("USER_DATA")) + "/ServiceData").c_str(), 1);

	// Some configuration plug-ins use __ENV__("OTSDAQ_LIB") and
	// __ENV__("OTSDAQ_UTILITIES_LIB") in init() so define it 	to a non-sense place is ok
	setenv("OTSDAQ_LIB", (std::string(__ENV__("USER_DATA")) + "/").c_str(), 1);
	setenv("OTSDAQ_UTILITIES_LIB", (std::string(__ENV__("USER_DATA")) + "/").c_str(), 1);

	// Some configuration plug-ins use __ENV__("OTS_MAIN_PORT") in init() so define it
	setenv("OTS_MAIN_PORT", "2015", 1);

	// also xdaq envs for XDAQContextTable
	setenv("XDAQ_CONFIGURATION_DATA_PATH", (std::string(__ENV__("USER_DATA")) + "/XDAQConfigurations").c_str(), 1);
	setenv("XDAQ_CONFIGURATION_XML", "otsConfigurationNoRU_CMake", 1);
	////////////////////////////////////////////////////

	//==============================================================================

	// Steps:
	//
	//	-- create empty map of import alias to original groupName & groupKey
	//	-- create empty map of original groupName & groupKey to new groupKey
	//
	//	-- create empty map of import alias to original tableName & tableVersion
	//	-- create empty map of original tableName & tableVersion to new tableVersion
	//
	//	-- in current-db extract info
	//		- get set of existing group aliases
	//			. load current aliases from current-db
	//		- get set of existing table aliases
	//			. load current aliases from current-db
	//
	//	-- swap to import-db, clear cache, and create vector of groups to import
	//		- get active groups from user data file
	//		- get aliases from active backbone group
	//			. check basename+alias for collision with existing group aliases and throw
	// error if collision
	//			. add map record of basename+alias to original groupName & groupKey
	//			. check basename+alias for collision with existing table aliases and throw
	// error if collision
	//			. add map record of basename+alias to original tableName & tableVersion
	//
	//	-- for each group in set
	//		- swap to import-db
	//		- load/activate group
	//		- swap to current-db
	//		- for each member table in group
	//			. check map of original tableName & tableVersion to new tableName &
	// tableVersion, to prevent making a new table
	//			. if needed, save active tables as next table version (using interface)
	//				, add map record of original tableName & tableVersion to new tableName
	//&  tableVersion
	//		- save new group with associated table versions
	//			. add map record of original groupName & groupKey to new groupName &
	// groupKey
	//
	//
	//	-- in current-db after loop...
	//		- destroy and reload active groups
	// 	-- insert new aliases for imported groups
	//		- should be basename+alias connection to (hop through maps) new groupName &
	// groupKey
	// 	-- insert new aliases for imported tables
	//		- should be basename+alias connection to (hop through maps) new tableName &
	// tableVersion
	//	-- save new backbone tables and save new backbone group
	//	-- backup the file ConfigurationManager::ACTIVE_GROUPS_FILENAME with time
	//	-- activate the new backbone group
	//

	//==============================================================================

	// create objects

	// std::map<std::string /*tableName*/,
	//	TableVersion /*next persisten version*/>
	// tableNextVersionMap;

	std::map<std::string /*importGroupAlias*/,
	         /*original*/ std::pair<std::string /*groupName*/, TableGroupKey>>
	    originalGroupAliasMap;

	std::map</*original*/ std::pair<std::string /*groupName*/, TableGroupKey>,
	         /*new*/ TableGroupKey>
	    groupSet;

	std::map<std::string /*importTableAlias*/,
	         /*original*/ std::pair<std::string /*tableName*/, TableVersion>>
	    originalTableAliasMap;

	std::map</*original*/ std::pair<std::string /*tableName*/, TableVersion>,
	         /*new*/ TableVersion>
	    newTableVersionMap;

	std::map</*original*/ std::pair<std::string, TableGroupKey>, std::string /*error string*/> groupErrors;

	// get prepared with initial source db

	// ConfigurationManager instance immediately loads active groups
	__COUT__ << "Getting started..." << std::endl;
	ConfigurationManagerRW  cfgMgrInst("import_aliases");
	ConfigurationManagerRW* cfgMgr            = &cfgMgrInst;
	bool                    importedDbInPlace = false;


	{
		std::string accumulatedWarnings;
		cfgMgr->restoreActiveTableGroups(false /*throwErrors*/,
					"" /*pathToActiveGroupsFile*/,
					ConfigurationManager::LoadGroupType::ALL_TYPES,
					&accumulatedWarnings
				);

		__COUT__ << "Done Loading active groups: \n" << accumulatedWarnings << std::endl;
	}

	__COUT__ << "Configuration manager initialized." << __E__;

	std::string nowTime    = std::to_string(time(0));
	std::string currentDir = __ENV__("ARTDAQ_DATABASE_URI");

	if(currentDir.find("filesystemdb://") != 0)
	{
		__SS__ << "filesystemdb:// was not found in $ARTDAQ_DATABASE_URI!" << std::endl;
		__COUT_ERR__ << "\n" << ss.str();
		__SS_THROW__;
	}

	currentDir = currentDir.substr(std::string("filesystemdb://").length());
	while(currentDir.length() && currentDir[currentDir.length() - 1] == '/')  // remove trailing '/'s
		currentDir = currentDir.substr(0, currentDir.length() - 1);

	__COUTV__(currentDir);

	std::string backupDir     = currentDir + "_" + nowTime;
	std::string importDir     = pathToImportDatabase + "_" + nowTime;
	std::string tmpCurrentDir = currentDir + "_tmp_" + nowTime;
	// std::string tmpImportDir = pathToImportDatabase + "_tmp_" + nowTime;

	//	-- get set of existing aliases
	std::map<std::string /*table name*/, std::map<std::string /*version alias*/, TableVersion /*aliased version*/>> existingTableAliases =
	    cfgMgr->ConfigurationManager::getVersionAliases();
	std::map<std::string /*alias*/, std::pair<std::string /*group name*/, TableGroupKey>> existingGroupAliases = cfgMgr->getActiveGroupAliases();

	//	//NOT NEEDED -- just ask db on fly for next version //	- fill map of table name
	// to next persistent table version
	//	{
	//		const std::map<std::string, TableInfo>& allCfgInfo =
	// cfgMgr->getAllConfigurationInfo(); 		for(auto& mapPair : allCfgInfo)
	//		{
	//			tableNextVersionMap[mapPair.first] =
	//					mapPair.second.versions_.size()?
	//							*(mapPair.second.versions_.rbegin()):
	//							TableVersion(TableVersion::DEFAULT);
	//		}
	//	}
	//
	//	__COUTV__(StringMacros::mapToString(tableNextVersionMap));

	//	-- swap to import-db and clear cache
	{
		// back up current directory now
		__COUT__ << "Backing up current database at '" << currentDir << "' to '" << backupDir << "'" << __E__;
		std::system(("cp -r " + currentDir + " " + backupDir).c_str());

		__COUT__ << "Backing up current database at '" << pathToImportDatabase << "' to '" << importDir << "'" << __E__;
		std::system(("cp -r " + pathToImportDatabase + " " + importDir).c_str());

		cfgMgr->destroy();

		__COUT__ << "Swap to import-db" << std::endl;
		if(rename(currentDir.c_str(), tmpCurrentDir.c_str()) < 0)
		{
			__SS__ << "Problem!" << std::endl;
			__SS_THROW__;
		}
		if(rename(importDir.c_str(), currentDir.c_str()) < 0)
		{
			__SS__ << "Problem!" << std::endl;
			__SS_THROW__;
		}
		importedDbInPlace = true;
	}

	try
	{
		//		- get active groups from user data file
		cfgMgr->restoreActiveTableGroups(true /*throwErrors*/, pathToImportActiveGroups);

		// add active groups to set
		std::map<std::string, std::pair<std::string, TableGroupKey>> activeGroupsMap = cfgMgr->getActiveTableGroups();

		for(const auto& activeGroup : activeGroupsMap)
		{
			if(activeGroup.second.second.isInvalid())
				continue;
			if(activeGroup.second.first == "")
				continue;

			__COUTV__(activeGroup.second.first);
			__COUTV__(activeGroup.second.second);

			groupSet.insert(std::pair<std::pair<std::string, TableGroupKey>, TableGroupKey>(
			    std::pair<std::string, TableGroupKey>(activeGroup.second.first, activeGroup.second.second), TableGroupKey()));
		}

		// add system alias groups to set
		std::map<std::string, TableVersion> activeVersions = cfgMgr->getActiveVersions();
		if(activeVersions.find(groupAliasesTableName) == activeVersions.end())
		{
			__SS__ << "\nActive version of " << groupAliasesTableName << " missing! " << groupAliasesTableName
			       << " is a required member of the Backbone configuration group."
			       << "\n\nLikely you need to activate a valid Backbone group." << std::endl;
			__SS_THROW__;
		}
		if(activeVersions.find(versionAliasesTableName) == activeVersions.end())
		{
			__SS__ << "\nActive version of " << versionAliasesTableName << " missing! " << versionAliasesTableName
			       << " is a required member of the Backbone configuration group."
			       << "\n\nLikely you need to activate a valid Backbone group." << std::endl;
			__SS_THROW__;
		}

		std::vector<std::pair<std::string, ConfigurationTree>> aliasNodePairs = cfgMgr->getNode(groupAliasesTableName).getChildren();
		std::string                                            aliasName;
		for(auto& aliasPair : aliasNodePairs)
		{
			if(TableGroupKey(aliasPair.second.getNode("GroupKey").getValueAsString()).isInvalid())
				continue;
			if(aliasPair.second.getNode("GroupName").getValueAsString() == "")
				continue;

			aliasName = aliasPair.second.getNode("GroupKeyAlias").getValueAsString();
			if(aliasName == "")
				continue;

			if(aliasName[0] >= 'a' && aliasName[0] <= 'z')  // capitalize the name
				aliasName[0] -= 32;
			aliasName = prependBaseName + aliasName;
			__COUTV__(aliasName);
			__COUTV__(aliasPair.second.getNode("GroupName").getValueAsString());
			__COUTV__(aliasPair.second.getNode("GroupKey").getValueAsString());

			//			. check basename+alias for collision with existing group aliases
			// and  throw error if collision
			if(existingGroupAliases.find(aliasName) != existingGroupAliases.end())
			{
				__SS__ << "Conflicting group alias '" << aliasName << "' found!" << __E__;
				__SS_THROW__;
			}

			groupSet.insert(std::pair<std::pair<std::string, TableGroupKey>, TableGroupKey>(
			    std::pair<std::string, TableGroupKey>(aliasPair.second.getNode("GroupName").getValueAsString(),
			                                          TableGroupKey(aliasPair.second.getNode("GroupKey").getValueAsString())),
			    TableGroupKey()));

			originalGroupAliasMap[aliasName] = std::pair<std::string, TableGroupKey>(aliasPair.second.getNode("GroupName").getValueAsString(),
			                                                                         TableGroupKey(aliasPair.second.getNode("GroupKey").getValueAsString()));
		}  // end group aliases loop

		aliasNodePairs = cfgMgr->getNode(versionAliasesTableName).getChildren();
		for(auto& aliasPair : aliasNodePairs)
		{
			if(TableVersion(aliasPair.second.getNode("Version").getValueAsString()).isInvalid())
				continue;
			if(aliasPair.second.getNode("TableName").getValueAsString() == "")
				continue;
			aliasName = aliasPair.second.getNode("VersionAlias").getValueAsString();
			if(aliasName == "")
				continue;

			if(aliasName[0] >= 'a' && aliasName[0] <= 'z')  // capitalize the name
				aliasName[0] -= 32;
			aliasName = prependBaseName + aliasName;
			__COUTV__(aliasPair.second.getNode("TableName").getValueAsString());
			__COUTV__(aliasName);
			__COUTV__(aliasPair.second.getNode("Version").getValueAsString());

			//			. check basename+alias for collision with existing table aliases
			// and  throw error if collision
			if(existingTableAliases.find(aliasName) != existingTableAliases.end())
			{
				__SS__ << "Conflicting table version alias '" << aliasName << "' found!" << __E__;
				__SS_THROW__;
			}

			originalTableAliasMap[aliasName] = std::pair<std::string, TableVersion>(aliasPair.second.getNode("TableName").getValueAsString(),
			                                                                        TableVersion(aliasPair.second.getNode("Version").getValueAsString()));
		}  // end table aliases loop
	}      // end extract group set
	catch(const std::runtime_error& e)
	{
		__COUT_ERR__ << "There was a fatal error: " << e.what() << __E__;

		__COUT__ << std::endl;
		__COUT__ << std::endl;
		__COUT__ << "Run the following to return to your previous database structure:" << std::endl;
		__COUT__ << "\t otsdaq_flatten_system_aliases -1 " << backupDir << "\n\n" << std::endl;
		__COUT__ << std::endl;
		__COUT__ << std::endl;
		return;
	}

	__COUTV__(StringMacros::mapToString(existingGroupAliases));
	__COUTV__(StringMacros::mapToString(existingTableAliases));

	__COUT__ << std::endl;
	__COUT__ << std::endl;
	__COUT__ << "Identified groups:" << std::endl;
	for(auto& group : groupSet)
		__COUT__ << "\t" << group.first.first << " " << group.first.second << std::endl;
	__COUT__ << std::endl;
	__COUT__ << std::endl;

	__COUT__ << "Identified group aliases:" << std::endl;
	for(auto& groupAlias : originalGroupAliasMap)
		__COUT__ << "\t" << groupAlias.first << " ==> " << groupAlias.second.first << "-" << groupAlias.second.second << std::endl;
	__COUT__ << std::endl;
	__COUT__ << std::endl;

	//==============================================================================
	//	-- for each group in set

	ConfigurationInterface* theInterface_ = ConfigurationInterface::getInstance(false);  // true for File interface, false for artdaq database;
	TableView*              cfgView;
	TableBase*              config;
	TableVersion            newVersion;
	TableGroupKey           newKey;

	bool        errDetected;
	std::string accumulateErrors = "";
	int         count            = 0;

	std::map<std::string, TableVersion>                   memberMap;
	std::map<std::string /*name*/, std::string /*alias*/> groupAliases;
	std::string                                           groupComment;
	std::string                                           groupAuthor;
	std::string                                           groupCreateTime;
	time_t                                                groupCreateTime_t;
	TableBase*                                            groupMetadataTable = cfgMgr->getMetadataTable();

	__COUT__ << "Proceeding with handling of identified groups..." << __E__;

	//	-- for each group in set
	for(auto& groupPair : groupSet)
	{
		cfgMgr->destroy();

		errDetected = false;

		//- swap to import-db (first time already in import-db
		//- load/activate group

		__COUTV__(importedDbInPlace);
		//	-- swap to import-db
		if(!importedDbInPlace)
		{
			__COUT__ << "Swap to import-db" << std::endl;
			if(rename(currentDir.c_str(), tmpCurrentDir.c_str()) < 0)
			{
				__SS__ << "Problem!" << std::endl;
				__SS_THROW__;
			}
			if(rename(importDir.c_str(), currentDir.c_str()) < 0)
			{
				__SS__ << "Problem!" << std::endl;
				__SS_THROW__;
			}
			importedDbInPlace = true;
		}

		// cfgMgr->restoreActiveTableGroups(true
		// /*throwErrors*/,pathToImportActiveGroups);

		__COUT__ << "****************************" << std::endl;
		__COUT__ << "Loading members for " << groupPair.first.first << "(" << groupPair.first.second << ")" << std::endl;
		__COUTV__(count);

		//=========================
		// load group, group metadata, and tables from original DB
		try
		{
			cfgMgr->loadTableGroup(groupPair.first.first,
			                       groupPair.first.second,
			                       true /*doActivate*/,
			                       &memberMap /*memberMap*/,
			                       0 /*progressBar*/,
			                       &accumulateErrors,
			                       &groupComment,
			                       &groupAuthor,
			                       &groupCreateTime,
			                       false /*doNotLoadMember*/,
			                       0 /*groupTypeString*/,
			                       &groupAliases);
		}
		catch(std::runtime_error& e)
		{
			__COUT__ << "Error was caught loading members for " << groupPair.first.first << "(" << groupPair.first.second << ")" << std::endl;
			__COUT__ << e.what() << std::endl;
			errDetected = true;
		}
		catch(...)
		{
			__COUT__ << "Error was caught loading members for " << groupPair.first.first << "(" << groupPair.first.second << ")" << std::endl;
			errDetected = true;
		}

		//=========================

		// if(count == 2) break;

		// exit loop if any (loading) failure
		if(errDetected)
		{
			// goto CLEAN_UP;

			// power on if group failed
			//	and record error

			groupErrors.insert(std::pair<std::pair<std::string, TableGroupKey>, std::string>(
			    std::pair<std::string, TableGroupKey>(groupPair.first.first, groupPair.first.second), "Error caught loading the group."));
			continue;
		}

		//	-- swap to current-db
		if(importedDbInPlace)
		{
			__COUT__ << "Swap to current-db" << std::endl;
			if(rename(currentDir.c_str(), importDir.c_str()) < 0)
			{
				__SS__ << "Problem!" << std::endl;
				__SS_THROW__;
			}
			if(rename(tmpCurrentDir.c_str(), currentDir.c_str()) < 0)
			{
				__SS__ << "Problem!" << std::endl;
				__SS_THROW__;
			}
			importedDbInPlace = false;
		}

		//=========================
		// save group and its tables with new key and versions!
		try
		{
			// saving tables
			for(auto& memberPair : memberMap)
			{
				__COUT__ << memberPair.first << ":v" << memberPair.second << std::endl;

				// check if table has already been modified by a previous group
				//	(i.e. two groups using the same version of a table)
				if(newTableVersionMap.find(std::pair<std::string, TableVersion>(memberPair.first, memberPair.second)) != newTableVersionMap.end())
				{
					__COUT__ << "Table was already modified!" << std::endl;
					memberPair.second = newTableVersionMap[std::pair<std::string, TableVersion>(memberPair.first, memberPair.second)];
					__COUT__ << "\t to...\t" << memberPair.first << ":v" << memberPair.second << std::endl;
					continue;
				}

				// change the version of the active view to next available version and
				// save it
				config  = cfgMgr->getTableByName(memberPair.first);
				cfgView = config->getViewP();
				// newVersion = theInterface_->saveNewVersion(config, temporaryVersion);
				newVersion = TableVersion::getNextVersion(theInterface_->findLatestVersion(config));
				__COUTV__(newVersion);
				cfgView->setVersion(newVersion);
				theInterface_->saveActiveVersion(config);

				// set it back for the table so that future groups can re-use cached
				// version
				// FIXME -- RAR note: I do not understand why this was important.. seems
				// like it will not help since the cache is destroyed for each group
				// cfgView->setVersion(memberPair.second); //IMPORTANT

				// save new version to modifiedTables
				newTableVersionMap.insert(std::pair<std::pair<std::string, TableVersion>, TableVersion>(
				    std::pair<std::string, TableVersion>(memberPair.first, memberPair.second), newVersion));

				memberPair.second = newVersion;  // change version in the member map

				__COUT__ << "\t to...\t" << memberPair.first << ":v" << memberPair.second << std::endl;
			}  // end member map loop

			__COUT__ << "Member map completed" << __E__;
			__COUTV__(StringMacros::mapToString(memberMap));

			// Note: this code copies actions in ConfigurationManagerRW::saveNewTableGroup

			// add meta data
			__COUTV__(StringMacros::mapToString(groupAliases));
			__COUTV__(groupComment);
			__COUTV__(groupAuthor);
			__COUTV__(groupCreateTime);
			sscanf(groupCreateTime.c_str(), "%ld", &groupCreateTime_t);
			__COUTV__(groupCreateTime_t);

			// to compensate for unusual errors upstream, make sure the metadata table has
			// one row
			while(groupMetadataTable->getViewP()->getNumberOfRows() > 1)
				groupMetadataTable->getViewP()->deleteRow(0);
			if(groupMetadataTable->getViewP()->getNumberOfRows() == 0)
				groupMetadataTable->getViewP()->addRow();

			// columns are uid,comment,author,time
			// ConfigurationManager::METADATA_COL_ALIASES TODO
			groupMetadataTable->getViewP()->setValue(
			    StringMacros::mapToString(groupAliases, "," /*primary delimiter*/, ":" /*secondary delimeter*/), 0, ConfigurationManager::METADATA_COL_ALIASES);
			groupMetadataTable->getViewP()->setValue(groupComment, 0, ConfigurationManager::METADATA_COL_COMMENT);
			groupMetadataTable->getViewP()->setValue(groupAuthor, 0, ConfigurationManager::METADATA_COL_AUTHOR);
			groupMetadataTable->getViewP()->setValue(groupCreateTime_t, 0, ConfigurationManager::METADATA_COL_TIMESTAMP);

			// set version of metadata table
			newVersion = TableVersion::getNextVersion(theInterface_->findLatestVersion(groupMetadataTable));
			__COUTV__(newVersion);
			groupMetadataTable->getViewP()->setVersion(newVersion);
			theInterface_->saveActiveVersion(groupMetadataTable);

			// force groupMetadataTable_ to be a member for the group
			memberMap[groupMetadataTable->getTableName()] = groupMetadataTable->getViewVersion();

			// memberMap should now consist of members with new flat version, so save
			// group
			newKey = TableGroupKey::getNextKey(theInterface_->findLatestGroupKey(groupPair.first.first));

			__COUTV__(newKey);

			// memberMap should now consist of members with new flat version, so save
			theInterface_->saveTableGroup(memberMap, TableGroupKey::getFullGroupString(groupPair.first.first, newKey));

			// and modify groupSet and activeGroupKeys keys
			groupPair.second = newKey;
		}
		catch(std::runtime_error& e)
		{
			__COUT__ << "Error was caught saving group " << groupPair.first.first << " (" << groupPair.first.second << ") " << std::endl;
			__COUT__ << e.what() << std::endl;

			groupErrors.insert(std::pair<std::pair<std::string, TableGroupKey>, std::string>(
			    std::pair<std::string, TableGroupKey>(groupPair.first.first, groupPair.first.second), "Error caught saving the group."));
		}
		catch(...)
		{
			__COUT__ << "Error was caught saving group " << groupPair.first.first << " (" << groupPair.first.second << ") " << std::endl;

			groupErrors.insert(std::pair<std::pair<std::string, TableGroupKey>, std::string>(
			    std::pair<std::string, TableGroupKey>(groupPair.first.first, groupPair.first.second), "Error caught saving the group."));
		}
		//=========================

		// increment
		++count;
	}  // end group loop

	__COUT__ << "Completed group and table saving for " << count << " groups." << __E__;
	__COUT__ << "Created tables:" << std::endl;
	for(auto& tablePair : newTableVersionMap)
		__COUT__ << "\t" << tablePair.first.first << "-v" << tablePair.first.second << " ==> " << tablePair.second << std::endl;
	__COUT__ << std::endl;
	__COUT__ << std::endl;
	__COUT__ << "Created groups:" << std::endl;
	for(auto& group : groupSet)
		__COUT__ << "\t" << group.first.first << "(" << group.first.second << ") ==> " << group.second << std::endl;

	//	-- in current-db after loop...
	//	-- swap to current-db
	if(importedDbInPlace)
	{
		__COUT__ << "Swap to current-db" << std::endl;
		if(rename(currentDir.c_str(), importDir.c_str()) < 0)
		{
			__SS__ << "Problem!" << std::endl;
			__SS_THROW__;
		}
		if(rename(tmpCurrentDir.c_str(), currentDir.c_str()) < 0)
		{
			__SS__ << "Problem!" << std::endl;
			__SS_THROW__;
		}
		importedDbInPlace = false;
	}

	// record in readme for current-db
	{
		FILE* fp = fopen((currentDir + "/README_otsdaq_import.txt").c_str(), "a");

		if(!fp)
			__COUT__ << "\tError opening README file!" << std::endl;
		else
		{
			time_t     rawtime;
			struct tm* timeinfo;
			char       buffer[200];

			time(&rawtime);
			timeinfo = localtime(&rawtime);
			strftime(buffer, 200, "%b %d, %Y %I:%M:%S%p %Z", timeinfo);

			fprintf(fp,
			        "This database...\t %s \t received an import from...\t %s \t at this "
			        "time \t %lu \t %s\n\n",
			        currentDir.c_str(),
			        pathToImportDatabase.c_str(),
			        time(0),
			        buffer);
			fclose(fp);
		}
	}

	//	-- in current-db after loop...
	//		- destroy and reload active groups
	cfgMgr->destroy();
	cfgMgr->restoreActiveTableGroups(true /*throwErrors*/);

	// 	-- insert new aliases for imported groups
	//		- should be basename+alias connection to (hop through maps) new groupName &
	// groupKey
	// 	-- insert new aliases for imported tables
	//		- should be basename+alias connection to (hop through maps) new tableName &
	// tableVersion
	//	-- save new backbone tables and save new backbone group

	__COUT__ << "Modifying the active Backbone table to reflect new table versions and "
	            "group keys."
	         << std::endl;

	try
	{
		// modify Group Aliases Table and Version Aliases Table to
		//	include new groups and tables

		std::string activeBackboneGroupName = cfgMgr->getActiveGroupName(ConfigurationManager::GroupType::BACKBONE_TYPE);
		cfgMgr->loadTableGroup(
		    activeBackboneGroupName, cfgMgr->getActiveGroupKey(ConfigurationManager::GroupType::BACKBONE_TYPE), true, &memberMap, 0, &accumulateErrors);

		std::map<std::string, TableVersion> activeMap = cfgMgr->getActiveVersions();

		// modify Group Aliases Table
		if(activeMap.find(groupAliasesTableName) != activeMap.end())
		{
			__COUT__ << "\n\nModifying " << groupAliasesTableName << std::endl;
			config  = cfgMgr->getTableByName(groupAliasesTableName);
			cfgView = config->getViewP();

			unsigned int col0 = cfgView->findCol("GroupKeyAlias");
			unsigned int col1 = cfgView->findCol("GroupName");
			unsigned int col2 = cfgView->findCol("GroupKey");
			unsigned int row;

			cfgView->print();

			// 	-- insert new aliases for imported groups
			//		- should be basename+alias connection to (hop through maps) new
			// groupName & groupKey
			for(auto& aliasPair : originalGroupAliasMap)
			{
				auto groupIt = groupSet.find(std::pair<std::string, TableGroupKey>(aliasPair.second.first, aliasPair.second.second));

				if(groupIt == groupSet.end())
				{
					__COUT__ << "Error! Could not find the new entry for the original group " << aliasPair.second.first << "(" << aliasPair.second.second << ")"
					         << __E__;
					continue;
				}
				row = cfgView->addRow("import_aliases", true /*incrementUniqueData*/);
				cfgView->setValue(aliasPair.first, row, col0);
				cfgView->setValue(aliasPair.second.first, row, col1);
				cfgView->setValue(groupIt->second.toString(), row, col2);
			}  // end group alias edit

			cfgView->print();
		}

		// modify Version Aliases Table
		if(activeMap.find(versionAliasesTableName) != activeMap.end())
		{
			__COUT__ << "\n\nModifying " << versionAliasesTableName << std::endl;
			config            = cfgMgr->getTableByName(versionAliasesTableName);
			cfgView           = config->getViewP();
			unsigned int col0 = cfgView->findCol("VersionAlias");
			unsigned int col1 = cfgView->findCol("TableName");
			unsigned int col2 = cfgView->findCol("Version");

			unsigned int row;

			cfgView->print();

			// 	-- insert new aliases for imported tables
			//		- should be basename+alias connection to (hop through maps) new
			// tableName & tableVersion
			for(auto& aliasPair : originalTableAliasMap)
			{
				auto tableIt = newTableVersionMap.find(std::pair<std::string, TableVersion>(aliasPair.second.first, aliasPair.second.second));

				if(tableIt == newTableVersionMap.end())
				{
					__COUT__ << "Error! Could not find the new entry for the original table " << aliasPair.second.first << "(" << aliasPair.second.second << ")"
					         << __E__;
					continue;
				}
				row = cfgView->addRow("import_aliases", true /*incrementUniqueData*/);
				cfgView->setValue(aliasPair.first, row, col0);
				cfgView->setValue(aliasPair.second.first, row, col1);
				cfgView->setValue(tableIt->second.toString(), row, col2);
			}  // end group alias edit

			cfgView->print();
		}

		// save new Group Aliases Table and Version Aliases Table

		// change the version of the active view to flatVersion and save it
		config     = cfgMgr->getTableByName(groupAliasesTableName);
		cfgView    = config->getViewP();
		newVersion = TableVersion::getNextVersion(theInterface_->findLatestVersion(config));
		__COUTV__(newVersion);
		cfgView->setVersion(newVersion);
		theInterface_->saveActiveVersion(config);

		memberMap[groupAliasesTableName] = newVersion;  // change version in the member
		                                                // map

		__COUT__ << "\t to...\t" << groupAliasesTableName << ":v" << memberMap[groupAliasesTableName] << std::endl;

		__COUT__ << versionAliasesTableName << ":v" << memberMap[versionAliasesTableName] << std::endl;
		// change the version of the active view to flatVersion and save it
		config     = cfgMgr->getTableByName(versionAliasesTableName);
		cfgView    = config->getViewP();
		newVersion = TableVersion::getNextVersion(theInterface_->findLatestVersion(config));
		__COUTV__(newVersion);
		cfgView->setVersion(newVersion);
		theInterface_->saveActiveVersion(config);

		memberMap[versionAliasesTableName] = newVersion;  // change version in the member map

		__COUT__ << "\t to...\t" << versionAliasesTableName << ":v" << memberMap[versionAliasesTableName] << std::endl;

		__COUT__ << "Backbone member map completed" << __E__;
		__COUTV__(StringMacros::mapToString(memberMap));

		newKey = TableGroupKey::getNextKey(theInterface_->findLatestGroupKey(activeBackboneGroupName));

		__COUTV__(newKey);

		// memberMap should now consist of members with new flat version, so save
		theInterface_->saveTableGroup(memberMap, TableGroupKey::getFullGroupString(activeBackboneGroupName, newKey));

		std::string renameFile = ConfigurationManager::ACTIVE_GROUPS_FILENAME + "." + nowTime;
		rename(ConfigurationManager::ACTIVE_GROUPS_FILENAME.c_str(), renameFile.c_str());

		__COUT__ << "Backing up '" << ConfigurationManager::ACTIVE_GROUPS_FILENAME << "' to ... '" << renameFile << "'" << std::endl;

		cfgMgr->activateTableGroup(activeBackboneGroupName,
		                           newKey);  // and write to active group file

	}  // end try
	catch(const std::runtime_error& e)
	{
		__COUT_ERR__ << "There was a fatal error during backbone modification: " << e.what() << __E__;

		goto CLEAN_UP;
	}

	// print resulting all groups

	std::cout << "\n\n" << __COUT_HDR_FL__ << "Resulting Groups:" << std::endl;
	for(const auto& group : groupSet)
		__COUT__ << "\t" << group.first.first << ": " << group.first.second << " => " << group.second << std::endl;
	std::cout << "\n\n" << __COUT_HDR_FL__ << "Resulting Groups end." << std::endl;

CLEAN_UP:
	//==============================================================================
	__COUT__ << "End of Importing Active Table Groups!\n\n\n" << std::endl;

	__COUT__ << "****************************" << std::endl;
	__COUT__ << "There were " << groupSet.size() << " groups considered, and there were " << groupErrors.size() << " errors found handling those groups."
	         << std::endl;
	__COUT__ << "The following errors were found handling the groups:" << std::endl;
	for(auto& groupErr : groupErrors)
		__COUT__ << "\t" << groupErr.first.first << " " << groupErr.first.second << ": \t" << groupErr.second << std::endl;
	__COUT__ << "End of errors.\n\n" << std::endl;

	__COUT__ << "Run the following to return to your previous database structure:" << std::endl;
	__COUT__ << "\t otsdaq_flatten_system_aliases -1 " << backupDir << "\n\n" << std::endl;

	return;
}

int main(int argc, char* argv[])
{
	ImportSystemAliasTableGroups(argc, argv);
	return 0;
}
// BOOST_AUTO_TEST_SUITE_END()
