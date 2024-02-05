#include <dirent.h>
#include <cassert>
#include <iostream>
#include <memory>
#include <string>

#include "otsdaq/ConfigurationInterface/ConfigurationInterface.h"
#include "otsdaq/ConfigurationInterface/ConfigurationManagerRW.h"
// #include "artdaq-database/StorageProviders/FileSystemDB/provider_filedb_index.h"
// #include "artdaq-database/JsonDocument/JSONDocument.h"

// usage:
// otsdaq_flatten_system_aliases <baseFlatVersion> <pathToSwapIn (optional)>
//
// if baseFlatVersion is invalid or temporary nothing is saved in the new db
//	(Note: this can be used to swap dbs using pathToSwapIn)

using namespace ots;

void FixNewTableFields(int argc, char* argv[])
{
	std::cout << "=================================================\n";
	std::cout << "=================================================\n";
	std::cout << "=================================================\n";
	__COUT__ << "\nFixing new table fields!" << __E__;

	std::cout << "\n\nusage: Two arguments:\n\t <pathToSwapIn (optional)> \n\n"
	          << "\t Default values: pathToSwapIn = \"\" \n\n"
	          << __E__;

	std::cout << "\n\nNote: This assumes artdaq db file type interface. "
	          << "The current database/ will be moved to database_<linuxtime>/ "
	          << "unless a pathToSwapIn is specified, in which case the path will "
	          << "be copied overwriting database/ \n\n"
	          << __E__;

	std::cout << "argc = " << argc << __E__;
	for(int i = 0; i < argc; i++)
		std::cout << "argv[" << i << "] = " << argv[i] << __E__;

	if(argc > 2)
	{
		std::cout << "Error! Must provide at most one parameter.\n\n" << __E__;
		return;
	}

	// determine if "h"elp was first parameter
	std::string pathToSwapIn = "";
	if(argc >= 2)
		pathToSwapIn = argv[1];

	if(pathToSwapIn == "-h" || pathToSwapIn == "--help")
	{
		std::cout << "Recognized parameter 1 as a 'help' option. Usage was printed. Exiting." << __E__;
		return;
	}

	__COUTV__(pathToSwapIn);

	// return;
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
	// get prepared with initial source db

	// ConfigurationManager instance immediately loads active groups
	__COUT__ << "Loading active Aliases..." << __E__;
	ConfigurationManagerRW  cfgMgrInst("flatten_admin");
	ConfigurationManagerRW* cfgMgr = &cfgMgrInst;

	{
		std::string accumulatedWarnings;
		cfgMgr->restoreActiveTableGroups(false /*throwErrors*/,
					"" /*pathToActiveGroupsFile*/,
					ConfigurationManager::LoadGroupType::ALL_TYPES,
					&accumulatedWarnings
				);

		__COUT__ << "Done Loading active groups: \n" << accumulatedWarnings << std::endl;
	}

	// create set of groups to persist
	//	include active context
	//	include active backbone
	//	include active iterate group
	//	include active config group
	//		(keep key translation separate activeGroupKeys)
	//	include all groups with system aliases

	// for group in set
	//	load/activate group and flatten tables to flatVersion to new DB
	//		save new version to modifiedTables
	//	save group with flatVersion key to new DB
	//		save new key to groupSet
	//	++flatVersion

	// reload the active backbone (using activeGroupKeys)
	//	modify group aliases and table aliases properly based on groupSet and
	// modifiedTables 	save new backbone with flatVersion to new DB

	// backup the file ConfigurationManager::ACTIVE_GROUPS_FILENAME with time
	// 	and change the ConfigurationManager::ACTIVE_GROUPS_FILENAME
	//	to reflect new group names/keys

	/* map<<groupName, origKey>, newKey> */
	std::map<std::pair<std::string, TableGroupKey>, TableGroupKey> groupSet;
	/* <tableName, <origVersion, newVersion> >*/
	std::map<std::pair<std::string, TableVersion>, TableVersion>   modifiedTables;
	std::map<std::string, std::pair<TableGroupKey, TableGroupKey>> activeGroupKeys;
	std::map<std::pair<std::string, TableGroupKey>, std::string>   groupErrors;

	std::string activeBackboneGroupName = "";
	std::string activeContextGroupName  = "";
	std::string activeIterateGroupName  = "";
	std::string activeConfigGroupName   = "";

	std::string nowTime = std::to_string(time(0));

	std::string thenTime = "";
	if(pathToSwapIn != "")  // get target then time
	{
		thenTime = pathToSwapIn.substr(pathToSwapIn.rfind('_') + 1);
		__COUT__ << "thenTime = " << thenTime << __E__;
		// return;
	}

	// add active groups to set
	std::map<std::string, std::pair<std::string, TableGroupKey>> activeGroupsMap = cfgMgr->getActiveTableGroups();

	for(const auto& activeGroup : activeGroupsMap)
	{
		groupSet.insert(std::pair<std::pair<std::string, TableGroupKey>, TableGroupKey>(
		    std::pair<std::string, TableGroupKey>(activeGroup.second.first, activeGroup.second.second), TableGroupKey()));
		activeGroupKeys.insert(std::pair<std::string, std::pair<TableGroupKey, TableGroupKey>>(
		    activeGroup.second.first, std::pair<TableGroupKey, TableGroupKey>(activeGroup.second.second, TableGroupKey())));

		if(activeGroup.first == ConfigurationManager::GROUP_TYPE_NAME_BACKBONE)
		{
			activeBackboneGroupName = activeGroup.second.first;
			__COUT__ << "found activeBackboneGroupName = " << activeBackboneGroupName << __E__;
		}
		else if(activeGroup.first == ConfigurationManager::GROUP_TYPE_NAME_CONTEXT)
		{
			activeContextGroupName = activeGroup.second.first;
			__COUT__ << "found activeContextGroupName = " << activeContextGroupName << __E__;
		}
		else if(activeGroup.first == ConfigurationManager::GROUP_TYPE_NAME_ITERATE)
		{
			activeIterateGroupName = activeGroup.second.first;
			__COUT__ << "found activeIterateGroupName = " << activeIterateGroupName << __E__;
		}
		else if(activeGroup.first == ConfigurationManager::GROUP_TYPE_NAME_CONFIGURATION)
		{
			activeConfigGroupName = activeGroup.second.first;
			__COUT__ << "found activeConfigGroupName = " << activeConfigGroupName << __E__;
		}
	}

	// add system alias groups to set
	const std::string                   groupAliasesTableName = ConfigurationManager::GROUP_ALIASES_TABLE_NAME;
	std::map<std::string, TableVersion> activeVersions        = cfgMgr->getActiveVersions();
	if(activeVersions.find(groupAliasesTableName) == activeVersions.end())
	{
		__SS__ << "\nActive version of " << groupAliasesTableName << " missing! " << groupAliasesTableName
		       << " is a required member of the Backbone configuration group."
		       << "\n\nLikely you need to activate a valid Backbone group." << __E__;
		__SS_THROW__;
	}

	std::vector<std::pair<std::string, ConfigurationTree>> aliasNodePairs = cfgMgr->getNode(groupAliasesTableName).getChildren();
	for(auto& groupPair : aliasNodePairs)
		groupSet.insert(std::pair<std::pair<std::string, TableGroupKey>, TableGroupKey>(
		    std::pair<std::string, TableGroupKey>(groupPair.second.getNode("GroupName").getValueAsString(),
		                                          TableGroupKey(groupPair.second.getNode("GroupKey").getValueAsString())),
		    TableGroupKey()));

	__COUT__ << "Identified groups:" << __E__;
	for(auto& group : groupSet)
		__COUT__ << group.first.first << " " << group.first.second << __E__;
	__COUT__ << __E__;
	__COUT__ << __E__;

	// return;
	//==============================================================================
	// prepare to manipulate directories
	std::string currentDir = __ENV__("ARTDAQ_DATABASE_URI");

	if(currentDir.find("filesystemdb://") != 0)
	{
		__SS__ << "filesystemdb:// was not found in $ARTDAQ_DATABASE_URI!" << __E__;
		__SS_THROW__;
	}

	currentDir = currentDir.substr(std::string("filesystemdb://").length());
	while(currentDir.length() && currentDir[currentDir.length() - 1] == '/')  // remove trailing '/'s
		currentDir = currentDir.substr(0, currentDir.length() - 1);
	std::string moveToDir = currentDir + "_" + nowTime;

	if(pathToSwapIn != "")
	{
		DIR* dp;
		if((dp = opendir(pathToSwapIn.c_str())) == 0)
		{
			__COUT__ << "ERROR:(" << errno << ").  Can't open directory: " << pathToSwapIn << __E__;
			exit(0);
		}
		closedir(dp);
	}

	// handle directory swap
	__COUT__ << "Copying current directory: \t" << currentDir << __E__;
	__COUT__ << "\t... to: \t\t" << moveToDir << __E__;
	// return;
	rename(currentDir.c_str(), moveToDir.c_str());

	if(pathToSwapIn != "")  // move the swap in directory in
	{
		__COUT__ << "Swapping in directory: \t" << pathToSwapIn << __E__;
		__COUT__ << "\t.. to: \t\t" << currentDir << __E__;
		rename(pathToSwapIn.c_str(), currentDir.c_str());

		// also swap in active groups file
		// check if original active file exists
		std::string activeGroupsFile = ConfigurationManager::ACTIVE_GROUPS_FILENAME + "." + thenTime;
		FILE*       fp               = fopen(activeGroupsFile.c_str(), "r");
		if(fp)
		{
			__COUT__ << "Swapping active groups file: \t" << activeGroupsFile << __E__;
			__COUT__ << "\t.. to: \t\t" << ConfigurationManager::ACTIVE_GROUPS_FILENAME << __E__;
			rename(activeGroupsFile.c_str(), ConfigurationManager::ACTIVE_GROUPS_FILENAME.c_str());
		}

		__COUT__ << "Path swapped in. Done." << __E__;
		return;
	}
	else  // copy the bkup back
		std::system(("cp -r " + moveToDir + " " + currentDir).c_str());

	// return;

	//=============
	// now ready to save each member table of active groups
	//	to new version fixing column names.

	// int flatVersion = 0;

	// ConfigurationInterface* theInterface_ = ConfigurationInterface::getInstance(false);
	// //true for File interface, false for artdaq database;
	TableView* cfgView;
	TableBase* config;

	bool        errDetected      = false;
	std::string accumulateErrors = "";

	std::map<std::string /*name*/, TableVersion>          memberMap;
	std::map<std::string /*name*/, std::string /*alias*/> groupAliases;
	std::string                                           groupComment;
	//	std::string 			groupAuthor;
	//	std::string	 			groupCreateTime;
	//	time_t					groupCreateTime_t;
	//	TableBase*		groupMetadataTable = cfgMgr->getMetadataTable();

	//	//don't do anything more if flatVersion is not persistent
	//	if(TableVersion(flatVersion).isInvalid() ||
	//			TableVersion(flatVersion).isTemporaryVersion())
	//	{
	//		__COUT__<< "\n\nflatVersion " << TableVersion(flatVersion) <<
	//				" is an invalid or temporary version. Skipping to end!" << __E__;
	//		goto CLEAN_UP;
	//	}

	for(auto& groupPair : groupSet)
	{
		errDetected = false;

		__COUT__ << "****************************" << __E__;
		__COUT__ << "Loading members for " << groupPair.first.first << "(" << groupPair.first.second << ")" << __E__;

		//=========================
		// load group, group metadata, and tables from original DB
		try
		{
			cfgMgr->loadTableGroup(groupPair.first.first,
			                       groupPair.first.second,
			                       false /*doActivate*/,
			                       &memberMap /*memberMap*/,
			                       0 /*progressBar*/,
			                       &accumulateErrors,
			                       &groupComment,
			                       0,  //&groupAuthor,
			                       0,  //&groupCreateTime,
			                       false /*doNotLoadMember*/,
			                       0 /*groupTypeString*/,
			                       &groupAliases);
		}
		catch(std::runtime_error& e)
		{
			__COUT__ << "Error was caught loading members for " << groupPair.first.first << "(" << groupPair.first.second << ")" << __E__;
			__COUT__ << e.what() << __E__;
			errDetected = true;
		}
		catch(...)
		{
			__COUT__ << "Error was caught loading members for " << groupPair.first.first << "(" << groupPair.first.second << ")" << __E__;
			errDetected = true;
		}

		//=========================

		// note error if any (loading) failure
		if(errDetected)
		{
			// power on if group failed
			//	and record error

			groupErrors.insert(std::pair<std::pair<std::string, TableGroupKey>, std::string>(
			    std::pair<std::string, TableGroupKey>(groupPair.first.first, groupPair.first.second), "Error caught loading the group."));
			continue;
		}

		//=========================
		// save group and its tables with new key and versions!
		try
		{
			__COUT__ << "Before member map: " << StringMacros::mapToString(memberMap) << __E__;

			// saving tables
			for(auto& memberPair : memberMap)
			{
				__COUT__ << memberPair.first << ":v" << memberPair.second << __E__;

				// check if table has already been modified by a previous group
				//	(i.e. two groups using the same version of a table)
				if(modifiedTables.find(std::pair<std::string, TableVersion>(memberPair.first, memberPair.second)) != modifiedTables.end())
				{
					__COUT__ << "Table was already modified!" << __E__;
					memberPair.second = modifiedTables[std::pair<std::string, TableVersion>(memberPair.first, memberPair.second)];
					__COUT__ << "\t to...\t" << memberPair.first << ":v" << memberPair.second << __E__;
					continue;
				}

				// save new version, and then record new version in map

				// first copy to new column names
				TableVersion temporaryVersion = cfgMgr->copyViewToCurrentColumns(memberPair.first /*table name*/, memberPair.second /*source version*/
				);

				// then save temporary to persistent version
				TableVersion persistentVersion = cfgMgr->saveNewTable(memberPair.first /*table name*/, temporaryVersion);

				//				//change the version of the active view to flatVersion and
				// save  it 				config =
				// cfgMgr->getTableByName(memberPair.first); 				cfgView =
				// config->getViewP();
				//				cfgView->setVersion(TableVersion(flatVersion));
				//				theInterface_->saveActiveVersion(config);
				//
				//				//set it back for the table so that future groups can
				// re-use  cached version
				// cfgView->setVersion(memberPair.second);
				////IMPORTANT

				// save new version to modifiedTables
				modifiedTables.insert(std::pair<std::pair<std::string, TableVersion>, TableVersion>(
				    std::pair<std::string, TableVersion>(memberPair.first, memberPair.second), persistentVersion));

				memberPair.second = persistentVersion;  // change version in the member
				                                        // map

				__COUT__ << "\t to...\t" << memberPair.first << ":v" << memberPair.second << __E__;
			}  // end table member loop

			// now save new group
			__COUT__ << "After member map: " << StringMacros::mapToString(memberMap) << __E__;

			// return;

			TableGroupKey newGroupKey = cfgMgr->saveNewTableGroup(groupPair.first.first /*groupName*/, memberMap, groupComment, &groupAliases);

			//
			//
			//
			//			//Note: this code copies actions in
			// ConfigurationManagerRW::saveNewTableGroup
			//
			//			//add meta data
			//			__COUTV__(StringMacros::mapToString(groupAliases));
			//			__COUTV__(groupComment);
			//			__COUTV__(groupAuthor);
			//			__COUTV__(groupCreateTime);
			//			sscanf(groupCreateTime.c_str(),"%ld",&groupCreateTime_t);
			//			__COUTV__(groupCreateTime_t);
			//
			//			//to compensate for unusual errors upstream, make sure the
			// metadata  table has one row
			//			while(groupMetadataTable->getViewP()->getNumberOfRows() > 1)
			//				groupMetadataTable->getViewP()->deleteRow(0);
			//			if(groupMetadataTable->getViewP()->getNumberOfRows() == 0)
			//				groupMetadataTable->getViewP()->addRow();
			//
			//			//columns are uid,comment,author,time
			//			//ConfigurationManager::METADATA_COL_ALIASES TODO
			//			groupMetadataTable->getViewP()->setValue(
			//					StringMacros::mapToString(groupAliases,
			//							"," /*primary delimiter*/,":" /*secondary
			// delimeter*/),
			//							0,ConfigurationManager::METADATA_COL_ALIASES);
			//			groupMetadataTable->getViewP()->setValue(groupComment
			//,0,ConfigurationManager::METADATA_COL_COMMENT);
			//			groupMetadataTable->getViewP()->setValue(groupAuthor
			//,0,ConfigurationManager::METADATA_COL_AUTHOR);
			//			groupMetadataTable->getViewP()->setValue(groupCreateTime_t
			//,0,ConfigurationManager::METADATA_COL_TIMESTAMP);
			//
			//			//set version of metadata table
			//			groupMetadataTable->getViewP()->setVersion(TableVersion(flatVersion));
			//			theInterface_->saveActiveVersion(groupMetadataTable);
			//
			//			//force groupMetadataTable_ to be a member for the group
			//			memberMap[groupMetadataTable->getTableName()] =
			//					groupMetadataTable->getViewVersion();
			//
			//			//memberMap should now consist of members with new flat version,
			// so  save group 			theInterface_->saveTableGroup(memberMap,
			//					TableGroupKey::getFullGroupString(
			//							groupPair.first.first,
			//							TableGroupKey(flatVersion)));
			//

			// and modify groupSet and activeGroupKeys keys
			groupPair.second = TableGroupKey(newGroupKey);

			// if this is an active group, save key change
			if(activeGroupKeys.find(groupPair.first.first) != activeGroupKeys.end() && activeGroupKeys[groupPair.first.first].first == groupPair.first.second)
				activeGroupKeys[groupPair.first.first].second = TableGroupKey(newGroupKey);
		}
		catch(std::runtime_error& e)
		{
			__COUT__ << "Error was caught saving group " << groupPair.first.first << " (" << groupPair.first.second << ") " << __E__;
			__COUT__ << e.what() << __E__;

			groupErrors.insert(std::pair<std::pair<std::string, TableGroupKey>, std::string>(
			    std::pair<std::string, TableGroupKey>(groupPair.first.first, groupPair.first.second), "Error caught saving the group."));
		}
		catch(...)
		{
			__COUT__ << "Error was caught saving group " << groupPair.first.first << " (" << groupPair.first.second << ") " << __E__;

			groupErrors.insert(std::pair<std::pair<std::string, TableGroupKey>, std::string>(
			    std::pair<std::string, TableGroupKey>(groupPair.first.first, groupPair.first.second), "Error caught saving the group."));
		}
		//=========================

	}  // end group loop

	// record in readme of moveToDir
	{
		FILE* fp = fopen((moveToDir + "/README_fix_new_table_fields.txt").c_str(), "a");
		if(!fp)
			__COUT__ << "\tError opening README file!" << __E__;
		else
		{
			time_t     rawtime;
			struct tm* timeinfo;
			char       buffer[200];

			time(&rawtime);
			timeinfo = localtime(&rawtime);
			strftime(buffer, 200, "%b %d, %Y %I:%M%p %Z", timeinfo);

			fprintf(fp,
			        "This database %s \n\t is a backup of %s \n\t BEFORE forcing to new "
			        "table fields \n\t and was created at this time \n\t %lu \t %s\n\n\n",
			        currentDir.c_str(),
			        moveToDir.c_str(),
			        time(0),
			        buffer);

			fclose(fp);
		}
	}

	// record in readme for currentDir
	{
		FILE* fp = fopen((currentDir + "/README_otsdaq_flatten.txt").c_str(), "a");

		if(!fp)
			__COUT__ << "\tError opening README file!" << __E__;
		else
		{
			time_t     rawtime;
			struct tm* timeinfo;
			char       buffer[200];

			time(&rawtime);
			timeinfo = localtime(&rawtime);
			strftime(buffer, 200, "%b %d, %Y %I:%M:%S%p %Z", timeinfo);

			fprintf(fp,
			        "This database %s \n\t was forced to new table fields \n\t at this "
			        "time \n\t %lu \t %s\n\n\n",
			        currentDir.c_str(),
			        time(0),
			        buffer);

			fclose(fp);
		}
	}

	//	//print resulting all groups
	//
	//	__COUT__  << "Resulting Groups:" << __E__;
	//	for(const auto &group: groupSet)
	//		__COUT__<< group.first.first << ": " <<
	//			group.first.second << " => " << group.second << __E__;
	//	__COUT__  << "Resulting Groups end." << __E__;
	//
	//
	//	//print resulting active groups
	//
	//	__COUT__  << "Resulting Active Groups:" << __E__;
	//	for(const auto &activeGroup: activeGroupKeys)
	//		__COUT__<< activeGroup.first << ": " <<
	//			activeGroup.second.first << " => " << activeGroup.second.second << __E__;
	//
	//	__COUT__<< activeBackboneGroupName << " is the " <<
	//			ConfigurationManager::GROUP_TYPE_NAME_BACKBONE << "." << __E__;
	//	__COUT__  << "Resulting Active Groups end." << __E__;

	// reload the active backbone (using activeGroupKeys)
	//	modify group aliases and table aliases properly based on groupSet and
	// modifiedTables 	save new backbone with flatVersion to new DB

	if(activeBackboneGroupName == "")
	{
		__COUT__ << "No active Backbone table identified." << __E__;
		goto CLEAN_UP;
	}

	__COUT__ << "Modifying the active Backbone table to reflect new table versions and "
	            "group keys."
	         << __E__;

	{  // start active backbone group handling

		cfgMgr->loadTableGroup(activeBackboneGroupName,
		                       activeGroupKeys[activeBackboneGroupName].second,
		                       true /*doActivate*/,
		                       &memberMap,
		                       0 /*progressBar*/,
		                       &accumulateErrors,
		                       &groupComment);

		// modify Group Aliases Table and Version Aliases Table to point
		//	at DEFAULT and flatVersion respectively

		const std::string groupAliasesName   = ConfigurationManager::GROUP_ALIASES_TABLE_NAME;
		const std::string versionAliasesName = ConfigurationManager::VERSION_ALIASES_TABLE_NAME;

		std::map<std::string, TableVersion> activeMap = cfgMgr->getActiveVersions();

		// modify Group Aliases Table
		if(activeMap.find(groupAliasesName) != activeMap.end())
		{
			__COUT__ << "\n\nModifying " << groupAliasesName << __E__;

			// now save new group
			__COUT__ << "Before member map: " << StringMacros::mapToString(memberMap) << __E__;

			// save new Group Aliases table and Version Aliases table
			// first save new group aliases table
			__COUT__ << groupAliasesName << ":v" << memberMap[groupAliasesName] << __E__;

			// first copy to new column names
			TableVersion temporaryVersion = cfgMgr->copyViewToCurrentColumns(groupAliasesName /*table name*/, memberMap[groupAliasesName] /*source version*/
			);

			config = cfgMgr->getTableByName(groupAliasesName);
			config->setActiveView(temporaryVersion);
			cfgView = config->getViewP();

			unsigned int col1 = cfgView->findCol("GroupName");
			unsigned int col2 = cfgView->findCol("GroupKey");

			cfgView->print();

			// change all key entries found to the new key and delete rows for groups not
			// found
			bool found;
			for(unsigned int row = 0; row < cfgView->getNumberOfRows(); ++row)
			{
				found = false;
				for(const auto& group : groupSet)
					if(group.second.isInvalid())
						continue;
					else if(cfgView->getDataView()[row][col1] == group.first.first && cfgView->getDataView()[row][col2] == group.first.second.toString())
					{
						// found a matching group/key pair
						__COUT__ << "Changing row " << row << " for " << cfgView->getDataView()[row][col1] << " key=" << cfgView->getDataView()[row][col2]
						         << " to NEW key=" << group.second << __E__;
						cfgView->setValue(group.second.toString(), row, col2);
						found = true;
						break;
					}

				if(!found)  // delete row
					cfgView->deleteRow(row--);
			}

			cfgView->print();

			// then save temporary to persistent version
			TableVersion persistentVersion = cfgMgr->saveNewTable(groupAliasesName /*table name*/, temporaryVersion);

			//		//change the version of the active view to flatVersion and save it
			//		config = cfgMgr->getTableByName(groupAliasesName);
			//		cfgView = config->getViewP();
			//		cfgView->setVersion(TableVersion(flatVersion));
			//		theInterface_->saveActiveVersion(config);

			memberMap[groupAliasesName] = persistentVersion;  // change version in the member map

			__COUT__ << "\t to...\t" << groupAliasesName << ":v" << memberMap[groupAliasesName] << __E__;

		}  // done modifying group aliases

		// modify Version Aliases Table
		if(activeMap.find(versionAliasesName) != activeMap.end())
		{
			__COUT__ << "\n\nModifying " << versionAliasesName << __E__;

			// first save new version aliases table
			__COUT__ << versionAliasesName << ":v" << memberMap[versionAliasesName] << __E__;

			// first copy to new column names
			TableVersion temporaryVersion = cfgMgr->copyViewToCurrentColumns(versionAliasesName /*table name*/, memberMap[versionAliasesName] /*source version*/
			);

			config = cfgMgr->getTableByName(versionAliasesName);
			config->setActiveView(temporaryVersion);
			cfgView           = config->getViewP();
			unsigned int col1 = cfgView->findCol("TableName");
			unsigned int col2 = cfgView->findCol("Version");

			// change all version entries to the new version and delete rows with no match
			bool found;
			for(unsigned int row = 0; row < cfgView->getNumberOfRows(); ++row)
			{
				found = false;
				for(const auto& table : modifiedTables)
					if(cfgView->getDataView()[row][col1] == table.first.first && cfgView->getDataView()[row][col2] == table.first.second.toString())
					{
						// found a matching group/key pair
						__COUT__ << "Changing row " << row << " for " << cfgView->getDataView()[row][col1] << " version=" << cfgView->getDataView()[row][col2]
						         << " to NEW version=" << table.second << __E__;
						cfgView->setValue(table.second.toString(), row, col2);
						found = true;
						break;
					}

				if(!found)  // delete row
					cfgView->deleteRow(row--);
			}

			// then save temporary to persistent version
			TableVersion persistentVersion = cfgMgr->saveNewTable(versionAliasesName /*table name*/, temporaryVersion);

			//		//change the version of the active view to flatVersion and save it
			//		config = cfgMgr->getTableByName(versionAliasesName);
			//		cfgView = config->getViewP();
			//		cfgView->setVersion(TableVersion(flatVersion));
			//		theInterface_->saveActiveVersion(config);

			memberMap[versionAliasesName] = persistentVersion;  // change version in the member map

			__COUT__ << "\t to...\t" << versionAliasesName << ":v" << memberMap[versionAliasesName] << __E__;

		}  // done modifying version aliases

		// now save new group
		__COUT__ << "After member map: " << StringMacros::mapToString(memberMap) << __E__;

		TableGroupKey newGroupKey = cfgMgr->saveNewTableGroup(
		    activeBackboneGroupName /*groupName*/, memberMap, groupComment, 0 /*groupAliases*/);  // Do we need groupAliases for backbone here?

		// TableGroupKey cfgMgr->saveNewTableGroup
		//		theInterface_->saveTableGroup(memberMap,
		//				TableGroupKey::getFullGroupString(
		//						activeBackboneGroupName,
		//						TableGroupKey(flatVersion)));

		activeGroupKeys[activeBackboneGroupName].second = TableGroupKey(newGroupKey);

		__COUT__ << "New to-be-active backbone group " << activeBackboneGroupName << ":v" << activeGroupKeys[activeBackboneGroupName].second << __E__;
	}  // end active backbone group handling

	// backup the file ConfigurationManager::ACTIVE_GROUPS_FILENAME with time
	// 	and change the ConfigurationManager::ACTIVE_GROUPS_FILENAME
	//	to reflect new group names/keys

	{
		__COUT__ << "Manipulating the Active Groups file..." << __E__;

		// check if original active file exists
		FILE* fp = fopen(ConfigurationManager::ACTIVE_GROUPS_FILENAME.c_str(), "r");
		if(!fp)
		{
			__SS__ << "Original active groups file '" << ConfigurationManager::ACTIVE_GROUPS_FILENAME << "' not found." << __E__;
			goto CLEAN_UP;
		}

		__COUT__ << "Backing up file: " << ConfigurationManager::ACTIVE_GROUPS_FILENAME << __E__;

		fclose(fp);

		std::string renameFile = ConfigurationManager::ACTIVE_GROUPS_FILENAME + "." + nowTime;
		rename(ConfigurationManager::ACTIVE_GROUPS_FILENAME.c_str(), renameFile.c_str());

		__COUT__ << "Backup file name: " << renameFile << __E__;

		TableGroupKey *theConfigurationTableGroupKey_, *theContextTableGroupKey_, *theBackboneTableGroupKey_, *theIterateTableGroupKey_;
		std::string    theConfigurationTableGroup_, theContextTableGroup_, theBackboneTableGroup_, theIterateTableGroup_;

		theConfigurationTableGroup_    = activeConfigGroupName;
		theConfigurationTableGroupKey_ = &(activeGroupKeys[activeConfigGroupName].second);

		theContextTableGroup_    = activeContextGroupName;
		theContextTableGroupKey_ = &(activeGroupKeys[activeContextGroupName].second);

		theBackboneTableGroup_    = activeBackboneGroupName;
		theBackboneTableGroupKey_ = &(activeGroupKeys[activeBackboneGroupName].second);

		theIterateTableGroup_    = activeIterateGroupName;
		theIterateTableGroupKey_ = &(activeGroupKeys[activeIterateGroupName].second);

		// the following is copied from ConfigurationManagerRW::activateTableGroup
		{
			__COUT__ << "Updating persistent active groups to " << ConfigurationManager::ACTIVE_GROUPS_FILENAME << " ..." << __E__;

			std::string fn = ConfigurationManager::ACTIVE_GROUPS_FILENAME;
			FILE*       fp = fopen(fn.c_str(), "w");
			if(!fp)
				return;

			fprintf(fp, "%s\n", theContextTableGroup_.c_str());
			fprintf(fp, "%s\n", theContextTableGroupKey_ ? theContextTableGroupKey_->toString().c_str() : "-1");
			fprintf(fp, "%s\n", theBackboneTableGroup_.c_str());
			fprintf(fp, "%s\n", theBackboneTableGroupKey_ ? theBackboneTableGroupKey_->toString().c_str() : "-1");
			fprintf(fp, "%s\n", theConfigurationTableGroup_.c_str());
			fprintf(fp, "%s\n", theConfigurationTableGroupKey_ ? theConfigurationTableGroupKey_->toString().c_str() : "-1");
			fprintf(fp, "%s\n", theIterateTableGroup_.c_str());
			fprintf(fp, "%s\n", theIterateTableGroupKey_ ? theIterateTableGroupKey_->toString().c_str() : "-1");
			fclose(fp);
		}
	}

	// print resulting all groups

	__COUT__ << "Resulting Groups:" << __E__;
	for(const auto& group : groupSet)
		__COUT__ << "\t" << group.first.first << ": " << group.first.second << " => " << group.second << __E__;
	__COUT__ << "Resulting Groups end." << __E__;

	// print resulting active groups

	__COUT__ << "Resulting Active Groups:" << __E__;
	for(const auto& activeGroup : activeGroupKeys)
		__COUT__ << "\t" << activeGroup.first << ": " << activeGroup.second.first << " => " << activeGroup.second.second << __E__;

	__COUT__ << activeBackboneGroupName << " is the " << ConfigurationManager::GROUP_TYPE_NAME_BACKBONE << "." << __E__;
	__COUT__ << "Resulting Active Groups end." << __E__;

CLEAN_UP:
	//==============================================================================
	__COUT__ << "End of Flattening Active Table Groups!\n\n\n" << __E__;

	__COUT__ << "****************************" << __E__;
	__COUT__ << "There were " << groupSet.size() << " groups considered, and there were " << groupErrors.size() << " errors found handling those groups."
	         << __E__;
	__COUT__ << "The following errors were found handling the groups:" << __E__;
	for(auto& groupErr : groupErrors)
		__COUT__ << "\t" << groupErr.first.first << " " << groupErr.first.second << ": \t" << groupErr.second << __E__;
	__COUT__ << "End of errors.\n\n" << __E__;

	__COUT__ << "Run the following to return to your previous database structure:" << __E__;
	__COUT__ << "\t otsdaq_fix_new_table_fiels " << moveToDir << "\n\n" << __E__;

	return;
}  // end FixNewTableFields()

int main(int argc, char* argv[])
{
	FixNewTableFields(argc, argv);
	return 0;
}
// BOOST_AUTO_TEST_SUITE_END()
