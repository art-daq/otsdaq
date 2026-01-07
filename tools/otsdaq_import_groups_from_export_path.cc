#include "otsdaq/MessageFacility/MessageFacility.h"

#include <dirent.h>
#include <cassert>
#include <iostream>
#include <memory>
#include <string>

#include "otsdaq/ConfigurationInterface/ConfigurationInterface.h"
#include "otsdaq/ConfigurationInterface/ConfigurationManagerRW.h"

/// Imports all groups and group/table aliases (from backbone group) found at path
///		to the current database and active backbone group. New table version and group keys will be assigned
///		to the imported tables and groups.
/// usage:
/// otsdaq_import_groups_from_export_path <path to import from>
///
///
using namespace ots;

void ImportTableGroupsFromPath(int argc, char* argv[])
{
	// The configuration uses __ENV__("SERVICE_DATA_PATH") in init() so define it if it is not defined
	if(getenv("SERVICE_DATA_PATH") == NULL)
		setenv("SERVICE_DATA_PATH",
		       (std::string(__ENV__("USER_DATA")) + "/ServiceData").c_str(),
		       1);

	std::cout << "=================================================\n";
	std::cout << "=================================================\n";
	std::cout << "=================================================\n";
	__COUT__ << "\nImporting Groups!" << std::endl;

	std::cout << "\n\nusage: One argument:\n\t <import path> \n\n" << std::endl;

	std::cout << "argc = " << argc << std::endl;
	for(int i = 0; i < argc; i++)
		std::cout << "argv[" << i << "] = " << argv[i] << std::endl;

	if(argc < 2)
	{
		std::cout << "Error! Must provide at least one parameter.\n\n" << std::endl;
		return;
	}

	std::string importPath  = argv[1];
	std::string prepend     = argc > 2 ? argv[2] : "";  //get prepend arg or empty default
	bool		forceBackboneSave = argc > 3 ? true : false;  //get force backbone save arg or false
	int         flatVersion = 0;

	__COUTV__(importPath);
	__COUTV__(flatVersion);
	__COUTV__(prepend);
	__COUTV__(forceBackboneSave);

	//==============================================================================
	// Steps:
	//
	//	-- create empty map of import alias to original groupName & groupKey
	//	-- create empty map of original groupName & groupKey to new groupKey
	//
	//	-- create empty map of import alias to original tableName & tableVersion
	//	-- create empty map of original tableName & tableVersion to new tableVersion
	//
	//	-- check import basename+alias for collision with existing group aliases and throw
	// error if collision
	//	-- check import basename+alias for collision with existing table aliases and throw
	// error if collision
	//
	//	-- track map of import alias --> import group name/key --> new group key
	//	-- track map of import table name --> import version --> new table version, to prevent looking multiple times for duplicate tables
	//	-- for each group to import
	//		- for each table in group to import
	//			. load json into table view, check that table view is unique
	//			. if not unique update member map version
	//		- save (modified) member map as new group
	//		- report to user import alias --> import group/table name/key --> new group/table key
	//
	//	-- reload active backbone group
	// 	-- insert new aliases for imported groups in current active backbone
	//		- should be basename+alias connection to (hop through maps) new groupName & groupKey
	// 	-- insert new aliases for imported tables
	//		- should be basename+alias connection to (hop through maps) new tableName & tableVersion
	//	-- save new backbone tables and save new backbone group
	//	-- backup the file ConfigurationManager::ACTIVE_GROUPS_FILENAME with time
	//	-- activate the new backbone group
	//
	//==============================================================================

	// clang-format off
	std::map<std::string /*importGroupAlias*/,
		/*original*/ std::pair<std::string /*groupName*/, TableGroupKey /*origKey*/>>	importGroupAliasMap;
	std::map<std::pair<std::string /*groupName*/, TableGroupKey /*origKey*/>,
		TableGroupKey /*newKey*/> 														importGroupMap;
	std::map<std::string /*importTableAlias*/,
	         /*original*/ std::pair<std::string /*tableName*/,
			 TableVersion /*origVersion*/>>												importTableAliasMap;
	std::map</*original*/ std::pair<std::string /*tableName*/, TableVersion /*origVersion*/>,
	         /*newVersion*/ TableVersion>												importTableMap;
	// clang-format on

	std::string   importedBackboneGroupName = "";
	TableGroupKey importedBackboneGroupKey;

	// add groups to vector list from directory
	{
		DIR*           dp;
		struct dirent* dirp;
		if((dp = opendir(importPath.c_str())) == 0)
		{
			__COUT_ERR__ << "ERROR:(" << errno
			             << ").  Can't open directory: " << importPath << std::endl;
			exit(0);
		}

		const unsigned char isDir = 0x4;
		while((dirp = readdir(dp)) != 0)
			if(dirp->d_type == isDir && dirp->d_name[0] != '.')  //if a directory type
			{
				__COUT__ << "Directory: " << dirp->d_name << std::endl;

				auto split = StringMacros::getVectorFromString(dirp->d_name, {'_'});

				if(split.size() != 2)
					continue;

				importGroupMap.insert(
				    std::pair<
				        std::pair<std::string /*groupName*/, TableGroupKey /*origKey*/>,
				        TableGroupKey /*newKey*/>(
				        std::pair<std::string, TableGroupKey>(split[0], split[1]),
				        TableGroupKey()));

				//check if group is the active backbone to import
				std::string groupIsBackbonePath =
				    importPath + "/" + dirp->d_name + "/" + "groupIsBackbone.txt";
				__COUTV__(groupIsBackbonePath);
				FILE* fp = std::fopen(groupIsBackbonePath.c_str(), "r");
				if(fp)
				{
					fclose(fp);
					__COUT_INFO__ << "Found imported Backbone as " << split[0] << " ("
					              << split[1] << ")" << __E__;
					importedBackboneGroupName = split[0];
					importedBackboneGroupKey  = TableGroupKey(split[1]);
				}
			}  //end found group directory handling

		closedir(dp);
	}

	__COUT__ << "Identified groups to import:" << std::endl;
	for(auto& group : importGroupMap)
		__COUT__ << " ==> Group to import: " << group.first.first << " ("
		         << group.first.second << ")" << std::endl;
	__COUTV__(importedBackboneGroupName);

	if(!importGroupMap.size())
	{
		__SS__ << "No groups identified to import!" << __E__;
		__SS_THROW__;
	}
	// return;

	//==============================================================================
	// Define environment variables
	//	Note: normally these environment variables are set by ots script

	// These are needed by
	// otsdaq/otsdaq/ConfigurationDataFormats/ConfigurationInfoReader.cc [207]
	setenv("CONFIGURATION_TYPE", "File", 1);  // Can be File, Database, DatabaseTest
	setenv("CONFIGURATION_DATA_PATH",
	       (std::string(getenv("USER_DATA")) + "/ConfigurationDataExamples").c_str(),
	       1);
	setenv(
	    "TABLE_INFO_PATH", (std::string(getenv("USER_DATA")) + "/TableInfo").c_str(), 1);
	////////////////////////////////////////////////////

	// Some configuration plug-ins use __ENV__("OTSDAQ_LIB") and
	// __ENV__("OTSDAQ_UTILITIES_LIB") in init() so define it 	to a non-sense place is ok
	setenv("OTSDAQ_LIB", (std::string(getenv("USER_DATA")) + "/").c_str(), 1);
	setenv("OTSDAQ_UTILITIES_LIB", (std::string(getenv("USER_DATA")) + "/").c_str(), 1);

	// Some configuration plug-ins use __ENV__("OTS_MAIN_PORT") in init() so define it
	setenv("OTS_MAIN_PORT", "2015", 1);

	// also xdaq envs for XDAQContextTable
	setenv("XDAQ_CONFIGURATION_DATA_PATH",
	       (std::string(getenv("USER_DATA")) + "/XDAQConfigurations").c_str(),
	       1);
	setenv("XDAQ_CONFIGURATION_XML", "otsConfigurationNoRU_CMake", 1);
	////////////////////////////////////////////////////

	//==============================================================================
	// get prepared with initial source db

	// ConfigurationManager instance immediately loads active groups
	__COUT__ << "Loading active Backbone..." << std::endl;

	std::string ARTDAQ_DATABASE_URI = __ENV__("ARTDAQ_DATABASE_URI");
	__COUTV__(ARTDAQ_DATABASE_URI);

	// return;

	ConfigurationManagerRW  cfgMgrInst("import_admin");
	ConfigurationManagerRW* cfgMgr = &cfgMgrInst;

	//get all table/group info
	{
		std::string                             accumulatedWarnings;
		const std::map<std::string, TableInfo>& allTableInfo =
		    cfgMgr->getAllTableInfo(true /* refresh */,
		                            &accumulatedWarnings,
		                            "" /* errorFilterName */,
		                            true /* getGroupKeys*/,
		                            false /* getGroupInfo */,
		                            true /* initializeActiveGroups */);
		__COUTV__(allTableInfo.size());
		auto groups = cfgMgr->getAllGroupInfo();
		__COUTTV__(groups.size());
		if(TTEST(1))
			for(auto& group : groups)
			{
				__COUTTV__(group.first);
			}
	}

	ConfigurationInterface* theInterface_ = ConfigurationInterface::getInstance(
	    ConfigurationInterface::CONFIGURATION_MODE::ARTDAQ_DATABASE);

	if(0)
	{
		__COUT__ << "Test finding equivalent backbone table versions" << __E__;

		auto table =
		    cfgMgr->getTableByName(ConfigurationManager::VERSION_ALIASES_TABLE_NAME);
		__COUTV__(table->getTableName());
		__COUTV__(table->getViewVersion());

		auto         cfgView = table->getViewP();
		unsigned int col0    = cfgView->findCol("VersionAlias");
		unsigned int col1    = cfgView->findCol("TableName");
		unsigned int col2    = cfgView->findCol("Version");

		unsigned int row;

		cfgView->print();

		TableVersion duplicateVersion;

		// 	-- insert new aliases for imported tables
		//		- should be basename+alias connection to (hop through maps) new
		// tableName & tableVersion
		for(auto& aliasPair : importTableAliasMap)
		{
			__COUT__ << "\t" << aliasPair.first << " ==> " << aliasPair.second.first
			         << "-" << aliasPair.second.second << std::endl;

			auto tableIt = importTableMap.find(std::pair<std::string, TableVersion>(
			    aliasPair.second.first, aliasPair.second.second));

			if(tableIt == importTableMap.end())
			{
				__COUT__ << "Error! Could not find the new entry for the original table "
				         << aliasPair.second.first << "(" << aliasPair.second.second
				         << ")" << __E__;
				continue;
			}
			row =
			    cfgView->addRow(prepend + "import_aliases", true /*incrementUniqueData*/);
			cfgView->setValue(prepend + aliasPair.first, row, col0);
			cfgView->setValue(aliasPair.second.first, row, col1);
			cfgView->setValue(tableIt->second.toString(), row, col2);
		}  // end group alias edit

		if(!importTableAliasMap.size())
			duplicateVersion =
			    table->getViewVersion();  //mark duplicate as self if nothing to add

		cfgView->print();
		TableVersion originalVersion =
		    table
		        ->getViewVersion();  //save version because cache fill will change active version

		if(duplicateVersion.isInvalid())
		{
			auto tableName = ConfigurationManager::VERSION_ALIASES_TABLE_NAME;
			__COUT__ << "Checking for duplicate '" << tableName << "' tables..." << __E__;

			{
				//"DEEP" checking
				//	load into cache 'recent' versions for this table
				//		'recent' := those already in cache, plus highest version numbers not in cache
				const std::map<std::string, TableInfo>& allTableInfo =
				    cfgMgr->getAllTableInfo();  // do not refresh

				auto versionReverseIterator =
				    allTableInfo.at(tableName)
				        .versions_.rbegin();  // get reverse iterator
				__COUT__ << "Filling up '" << tableName << "' cache from "
				         << table->getNumberOfStoredViews() << " to max count of "
				         << table->MAX_VIEWS_IN_CACHE << __E__;
				for(;
				    table->getNumberOfStoredViews() < table->MAX_VIEWS_IN_CACHE &&
				    versionReverseIterator != allTableInfo.at(tableName).versions_.rend();
				    ++versionReverseIterator)
				{
					__COUTT__ << "'" << tableName << "' versions in reverse order "
					          << *versionReverseIterator << __E__;
					try
					{
						cfgMgr->getVersionedTableByName(
						    tableName,
						    *versionReverseIterator);  // load to cache
					}
					catch(const std::runtime_error& e)
					{
						// ignore error
						__COUTT__ << "'" << tableName << "' version failed to load: "
						          << *versionReverseIterator << __E__;
					}
				}
			}

			__COUT__ << "Checking '" << tableName << "' for duplicate..." << __E__;
			duplicateVersion =
			    table->checkForDuplicate(originalVersion,
			                             TableVersion());  // then all versions in search

		}  //end check duplicate

		//return the original version to active
		table->setActiveView(originalVersion);

		if(!duplicateVersion.isInvalid())
		{
			// found an equivalent!
			__COUT__ << "Equivalent table found in version v" << duplicateVersion
			         << __E__;
		}
		else
		{
			__COUTV__(table->getViewVersion());
			auto newVersion =
			    TableVersion::getNextVersion(theInterface_->findLatestVersion(table));
			__COUTV__(newVersion);
			// cfgView->setVersion(newVersion);
			// theInterface_->saveActiveVersion(table);
		}

	}  //end test version alias new version

	// return;

	//if missing Active Groups File, create empty backbone group (e.g., to seed a new database)
	FILE* fp = nullptr;
	if(!(fp = fopen(ConfigurationManager::ACTIVE_GROUPS_FILENAME.c_str(), "r")))
	{
		__COUT_INFO__ << "Identified missing Active Groups File: "
		              << ConfigurationManager::ACTIVE_GROUPS_FILENAME << __E__;
		__COUT_INFO__ << "Creating an empty Backbone group... group name will match "
		                 "imported backbone name: "
		              << importedBackboneGroupName << __E__;

		std::map<std::string, TableVersion> backboneMemberMap;
		for(auto& memberName : ConfigurationManager::getBackboneMemberNames())
		{
			//create empty mockup version
			// if mockup, then generate a new persistent version to use based on mockup
			TableBase* table = cfgMgr->getTableByName(memberName);
			// create a temporary version from the mockup as source version
			TableVersion temporaryVersion = table->createTemporaryView();
			__COUT__ << "\t\ttemporaryVersion: " << temporaryVersion << __E__;

			// if other versions exist check for another mockup, and use that instead
			__COUT__ << "Creating version from mock-up for name: " << memberName
			         << " temporaryVersion: " << temporaryVersion << __E__;

			// set table comment
			table->getTemporaryView(temporaryVersion)
			    ->setComment("Auto-generated from mock-up.");

			// finish off the version creation
			bool foundEquivalent;
			auto newAssignedVersion =
			    cfgMgr->saveModifiedVersion(memberName,
			                                TableVersion() /*original source is mockup*/,
			                                false /*makeTemporary*/,
			                                table,
			                                temporaryVersion,
			                                false /*ignoreDuplicates*/,
			                                true /*lookForEquivalent*/,
			                                &foundEquivalent);
			if(foundEquivalent)
				__COUT__ << "Found equivalent version: " << newAssignedVersion << __E__;
			else
				__COUT__ << "Created new version: " << newAssignedVersion << __E__;

			backboneMemberMap[memberName] = newAssignedVersion;
		}
		__COUTV__(StringMacros::mapToString(backboneMemberMap));

		bool foundExistingEmptyBackbone = false;
		{  //check if group is a duplicate
			__COUT__ << "Checking for duplicate groups..." << __E__;
			try
			{
				TableGroupKey foundKey = cfgMgr->findTableGroup(
				    importedBackboneGroupName,
				    backboneMemberMap);  //, memberTableAliases); std::map<std::string /*name*/, std::string /*alias*/>    memberTableAliases

				if(!foundKey.isInvalid())
				{
					__COUT_WARN__ << "Found equivalent empty backbone group key ("
					              << foundKey << ") for " << importedBackboneGroupName
					              << ". Skipping creation and activating existing key!"
					              << __E__;
					foundExistingEmptyBackbone = true;

					//	-- activate the existing empty backbone group
					cfgMgr->activateTableGroup(
					    importedBackboneGroupName,
					    foundKey);  // and write to active group file
				}

				__COUT__ << "Check for empty backbone duplicate groups complete."
				         << __E__;
			}
			catch(...)
			{
				__COUT_WARN__ << "Ignoring errors looking for empty backbone duplicate "
				                 "groups! Proceeding "
				                 "with new group creation."
				              << __E__;
			}
		}  //end check if group is a duplicate
		__COUTV__(foundExistingEmptyBackbone);

		if(!foundExistingEmptyBackbone)  //save new empty backbone
		{
			TableGroupKey newKey = TableGroupKey::getNextKey(
			    theInterface_->findLatestGroupKey(importedBackboneGroupName));

			// save group, and retry on save collision
			uint16_t retries = 0;
			while(1)
			{
				__COUT__ << "New Key for empty backbone group: "
				         << importedBackboneGroupName << " found as " << newKey << __E__;

				try
				{
					theInterface_->saveTableGroup(backboneMemberMap,
					                              TableGroupKey::getFullGroupString(
					                                  importedBackboneGroupName, newKey));
				}
				catch(const std::runtime_error& e)
				{
					__COUT__ << "Caught runtime_error exception during group save."
					         << __E__;
					if(std::string(e.what()).find("there was a collision") !=
					   std::string::npos)
					{
						__COUT_WARN__
						    << "There was a collision saving the new group "
						    << importedBackboneGroupName << "(" << newKey
						    << "), trying incremented group key... retries=" << retries
						    << __E__;
						if(++retries > 3)  //give up
							throw;
						newKey = TableGroupKey::getNextKey(newKey);  //increment group key
						__COUT__ << "New Key for group: " << importedBackboneGroupName
						         << " found as " << newKey << __E__;
						continue;
					}
					else
						throw;
				}
				__COUT__ << "Created new empty backbone table group: "
				         << importedBackboneGroupName << "(" << newKey << ")" << __E__;
				break;
			}  //end collission retry loop

			//	-- activate the new empty backbone group
			cfgMgr->activateTableGroup(importedBackboneGroupName,
			                           newKey);  // and write to active group file
		}

		__COUT_INFO__ << "Empty backbone group now activated!" << __E__;
		// return;
	}
	else if(fp)
		fclose(fp);

	//load active backbone
	{
		std::string accumulatedWarnings;
		cfgMgr->restoreActiveTableGroups(
		    false /*throwErrors*/,
		    "" /*pathToActiveGroupsFile*/,
		    ConfigurationManager::LoadGroupType::ONLY_BACKBONE_TYPE,
		    &accumulatedWarnings);

		__COUT__ << "Done Loading active backbone." << std::endl;
	}

	// return;

	/* <tableName, <origVersion, newVersion> >*/
	std::map<std::pair<std::string, TableVersion>, TableVersion>   modifiedTables;
	std::map<std::string, std::pair<TableGroupKey, TableGroupKey>> activeGroupKeys;
	std::map<std::pair<std::string, TableGroupKey>, std::string>   groupErrors;

	std::string activeBackboneGroupName = "";
	std::string activeContextGroupName  = "";
	std::string activeIterateGroupName  = "";
	std::string activeConfigGroupName   = "";

	std::string nowTime = std::to_string(time(0));

	// Find active backbone ---------------
	std::map<std::string, std::pair<std::string, TableGroupKey>> activeGroupsMap =
	    cfgMgr->getActiveTableGroups();

	bool foundAnyActiveGroups = false;

	for(const auto& activeGroup : activeGroupsMap)
	{
		if(activeGroup.second.second.TableGroupKey::isInvalid())
			continue;

		activeGroupKeys.insert(
		    std::pair<std::string, std::pair<TableGroupKey, TableGroupKey>>(
		        activeGroup.second.first,
		        std::pair<TableGroupKey, TableGroupKey>(activeGroup.second.second,
		                                                TableGroupKey())));

		if(activeGroup.first == ConfigurationManager::GROUP_TYPE_NAME_BACKBONE)
		{
			activeBackboneGroupName = activeGroup.second.first;
			__COUT__ << "found activeBackboneGroupName = " << activeBackboneGroupName
			         << std::endl;
			foundAnyActiveGroups = true;
		}
		else if(activeGroup.first == ConfigurationManager::GROUP_TYPE_NAME_CONTEXT)
		{
			activeContextGroupName = activeGroup.second.first;
			__COUT__ << "found activeContextGroupName = " << activeContextGroupName
			         << std::endl;
			foundAnyActiveGroups = true;
		}
		else if(activeGroup.first == ConfigurationManager::GROUP_TYPE_NAME_ITERATE)
		{
			activeIterateGroupName = activeGroup.second.first;
			__COUT__ << "found activeIterateGroupName = " << activeIterateGroupName
			         << std::endl;
			foundAnyActiveGroups = true;
		}
		else if(activeGroup.first == ConfigurationManager::GROUP_TYPE_NAME_CONFIGURATION)
		{
			activeConfigGroupName = activeGroup.second.first;
			__COUT__ << "found activeConfigGroupName = " << activeConfigGroupName
			         << std::endl;
			foundAnyActiveGroups = true;
		}
	}
	__COUT__ << "Identified active groups: " << activeGroupsMap.size() << std::endl;
	for(auto& group : activeGroupsMap)
		__COUT__ << " ==> Active Group of type " << group.first << ": "
		         << group.second.first << " (" << group.second.second << ")" << std::endl;

	__COUT__ << "Identified groups to import: " << importGroupMap.size() << std::endl;
	for(auto& group : importGroupMap)
		__COUT__ << " ==> Group to import: " << group.first.first << " ("
		         << group.first.second << ")" << std::endl;
	__COUTV__(importedBackboneGroupName);
	__COUTV__(prepend);

	if(activeBackboneGroupName == "" || !foundAnyActiveGroups)
	{
		__SS__
		    << "Did not find valid active groups in current database as starting point "
		       "for import! "
		       "Must have a backbone at least. Is the current database URI correct? "
		       "ARTDAQ_DATABASE_URI = "
		    << ARTDAQ_DATABASE_URI
		    << "\n\n*** Note: The active groups are specified by the Active Groups file: "
		    << ConfigurationManager::ACTIVE_GROUPS_FILENAME
		    << "\n*** If you want an empty backbone to be generated for you as a "
		       "starting point, please delete the Active Groups file."
		    << std::endl;
		__SS_THROW__;
	}
	//return; //comment for production functionality

	// ------------
	// At this point, all groups to import have been identified! and which import is Backbone has been identified ------------
	// now check for any alias conflicts between current Backbone and Backbone-to-Import

	{  //check import backbone group and table aliases for uniqueness
		std::string importBackbonePath = importPath + "/" + importedBackboneGroupName +
		                                 "_" + importedBackboneGroupKey.str();
		__COUTV__(importBackbonePath);

		std::map<std::string /* tableName */, TableVersion /* tableVersion */>
		    memberTableSet;
		{  //get member table file names
			DIR*           dp;
			struct dirent* dirp;
			if((dp = opendir(importBackbonePath.c_str())) == 0)
			{
				__COUT_ERR__ << "ERROR:(" << errno
				             << ").  Can't open directory: " << importBackbonePath
				             << std::endl;
				exit(0);
			}

			const unsigned char isDir = 0x4;
			while((dirp = readdir(dp)) != 0)
				if(dirp->d_type != isDir && strlen(dirp->d_name) > 5 &&
				   dirp->d_name[strlen(dirp->d_name) - 5] == '.' &&
				   dirp->d_name[strlen(dirp->d_name) - 4] == 'j' &&
				   dirp->d_name[strlen(dirp->d_name) - 3] == 's' &&
				   dirp->d_name[strlen(dirp->d_name) - 2] == 'o' &&
				   dirp->d_name[strlen(dirp->d_name) - 1] ==
				       'n')  //if not directory w/extension .json
				{
					__COUT__ << dirp->d_name << std::endl;

					auto split = StringMacros::getVectorFromString(dirp->d_name, {'_'});

					if(split.size() != 2)
						continue;

					memberTableSet.insert(std::pair<std::string, TableVersion>(
					    split[0], split[1].substr(1, split[1].size() - 6)));
				}  //end found group directory handling

			closedir(dp);
		}  //end load of backbone alias table names/versions from directory

		{  //verify group aliases are unique
			std::vector<std::pair<std::string, ConfigurationTree>> currentAliasNodePairs =
			    cfgMgr->getNode(ConfigurationManager::GROUP_ALIASES_TABLE_NAME)
			        .getChildren();
			std::set<std::string /* current aliases */> currentAliases;

			__COUTV__(cfgMgr->getNode(ConfigurationManager::GROUP_ALIASES_TABLE_NAME)
			              .getTableName());
			__COUTV__(cfgMgr->getNode(ConfigurationManager::GROUP_ALIASES_TABLE_NAME)
			              .getTableVersion());

			__COUT__ << "Existing group aliases:" << __E__;
			for(auto& groupPair : currentAliasNodePairs)
			{
				__COUT__ << " ==> Current group alias "
				         << groupPair.second.getNode("GroupKeyAlias").getValueAsString()
				         << ": "
				         << groupPair.second.getNode("GroupName").getValueAsString()
				         << " ("
				         << TableGroupKey(
				                groupPair.second.getNode("GroupKey").getValueAsString())
				         << ")" << __E__;

				currentAliases.emplace(
				    groupPair.second.getNode("GroupKeyAlias").getValueAsString());
			}

			std::string fullpath =
			    importBackbonePath + "/" +
			    ConfigurationManager::GROUP_ALIASES_TABLE_NAME + "_v" +
			    memberTableSet.at(ConfigurationManager::GROUP_ALIASES_TABLE_NAME).str() +
			    ".json";

			std::string json;
			std::FILE*  fp = std::fopen(fullpath.c_str(), "rb");
			if(!fp)
			{
				__SS__ << "Could not open file at " << fullpath << ". Error: " << errno
				       << " - " << strerror(errno) << __E__;
				__SS_THROW__;
			}
			std::fseek(fp, 0, SEEK_END);
			json.resize(std::ftell(fp));
			std::rewind(fp);
			std::fread(&json[0], 1, json.size(), fp);
			std::fclose(fp);

			__COUTV__(json);

			auto aliasTable =
			    cfgMgr->getTableByName(ConfigurationManager::GROUP_ALIASES_TABLE_NAME);
			auto temporaryVersion = aliasTable->createTemporaryView();
			auto aliasView        = aliasTable->getViewP(temporaryVersion);

			aliasView->fillFromJSON(json);
			aliasView->print();

			auto aliasCol     = aliasView->findCol("GroupKeyAlias");
			auto groupNameCol = aliasView->findCol("GroupName");
			auto groupKeyCol  = aliasView->findCol("GroupKey");

			__COUT__ << "Existing group aliases: " << currentAliases.size() << __E__;
			for(auto& currentAlias : currentAliases)
				__COUTV__(currentAlias);

			__COUT__ << "Checking that group aliases to import are unique:" << __E__;
			for(size_t row = 0; row < aliasView->getNumberOfRows(); ++row)
			{
				std::string aliasName = aliasView->getValueAsString(row, aliasCol);
				__COUT__ << " ==> Group alias to import at row " << row << ": "
				         << aliasName << __E__;
				if(currentAliases.find(prepend + aliasName) != currentAliases.end())
				{
					__SS__ << "The imported prepend + group alias of '" << prepend
					       << "' + '" << aliasName
					       << "' already exists as a group alias in the current database."
					       << " Please modify the prepend string to make the imported "
					          "alias names unique."
					       << __E__;
					__SS_THROW__;
				}
				importGroupAliasMap[aliasName] =
				    std::pair<std::string /*groupName*/, TableGroupKey /*origKey*/>(
				        aliasView->getValueAsString(row, groupNameCol),
				        aliasView->getValueAsString(row, groupKeyCol));
			}
			__COUT__ << "Verified group aliases to import are unique." << __E__;
		}  //end verify group aliases are unique

		{  //verify table aliases are unique
			std::vector<std::pair<std::string, ConfigurationTree>> currentAliasNodePairs =
			    cfgMgr->getNode(ConfigurationManager::VERSION_ALIASES_TABLE_NAME)
			        .getChildren();
			std::set<std::string /* current aliases */> currentAliases;

			__COUTV__(cfgMgr->getNode(ConfigurationManager::VERSION_ALIASES_TABLE_NAME)
			              .getTableName());
			__COUTV__(cfgMgr->getNode(ConfigurationManager::VERSION_ALIASES_TABLE_NAME)
			              .getTableVersion());

			__COUT__ << "Existing table aliases: " << currentAliasNodePairs.size()
			         << __E__;
			for(auto& groupPair : currentAliasNodePairs)
			{
				__COUT__ << " ==> Current table alias "
				         << groupPair.second.getNode("VersionAlias").getValueAsString()
				         << ": "
				         << groupPair.second.getNode("TableName").getValueAsString()
				         << " ("
				         << TableGroupKey(
				                groupPair.second.getNode("Version").getValueAsString())
				         << ")" << __E__;

				currentAliases.emplace(
				    groupPair.second.getNode("VersionAlias").getValueAsString());
			}

			std::string fullpath =
			    importBackbonePath + "/" +
			    ConfigurationManager::VERSION_ALIASES_TABLE_NAME + "_v" +
			    memberTableSet.at(ConfigurationManager::VERSION_ALIASES_TABLE_NAME)
			        .str() +
			    ".json";

			std::string json;
			std::FILE*  fp = std::fopen(fullpath.c_str(), "rb");
			if(!fp)
			{
				__SS__ << "Could not open file at " << fullpath << ". Error: " << errno
				       << " - " << strerror(errno) << __E__;
				__SS_THROW__;
			}
			std::fseek(fp, 0, SEEK_END);
			json.resize(std::ftell(fp));
			std::rewind(fp);
			std::fread(&json[0], 1, json.size(), fp);
			std::fclose(fp);

			__COUTV__(json);

			auto aliasTable =
			    cfgMgr->getTableByName(ConfigurationManager::VERSION_ALIASES_TABLE_NAME);
			auto temporaryVersion = aliasTable->createTemporaryView();
			auto aliasView        = aliasTable->getViewP(temporaryVersion);

			aliasView->fillFromJSON(json);
			aliasView->print();

			auto aliasCol        = aliasView->findCol("VersionAlias");
			auto tablepNameCol   = aliasView->findCol("TableName");
			auto tableVersionCol = aliasView->findCol("Version");

			__COUT__ << "Existing table aliases:" << __E__;
			for(auto& currentAlias : currentAliases)
				__COUTV__(currentAlias);

			__COUT__ << "Checking that table aliases to import are unique:" << __E__;
			for(size_t row = 0; row < aliasView->getNumberOfRows(); ++row)
			{
				std::string aliasName = aliasView->getValueAsString(row, aliasCol);
				__COUT__ << " ==> Table alias to import at row " << row << ": "
				         << aliasName << __E__;
				if(currentAliases.find(prepend + aliasName) != currentAliases.end())
				{
					__SS__ << "The imported prepend + table alias of '" << prepend
					       << "' + '" << aliasName
					       << "' already exists as a table alias in the current database."
					       << " Please modify the prepend string to make the imported "
					          "alias names unique."
					       << __E__;
					__SS_THROW__;
				}
				importTableAliasMap[aliasName] =
				    std::pair<std::string /*tableName*/, TableVersion /*origVersion*/>(
				        aliasView->getValueAsString(row, tablepNameCol),
				        aliasView->getValueAsString(row, tableVersionCol));
			}
			__COUT__ << "Verified table aliases to import are unique." << __E__;
		}  //end verify table aliases are unique

	}  //end check import backbone group aliases for uniqueness

	__COUT__ << "Identified group aliases to import:" << std::endl;
	for(auto& groupAlias : importGroupAliasMap)
		__COUT__ << "\t" << groupAlias.first << " ==> " << groupAlias.second.first << " ("
		         << groupAlias.second.second << ")" << std::endl;
	__COUT__ << "Identified table aliases to import:" << std::endl;
	for(auto& tableAlias : importTableAliasMap)
		__COUT__ << "\t" << tableAlias.first << " ==> " << tableAlias.second.first << "-v"
		         << tableAlias.second.second << std::endl;
	// return;

	//Now...
	//	-- for each group to import
	//		- for each table in group to import
	//			. load json into table view, check that table view is unique
	//			. if not unique update member map version
	//		- save (modified) member map as new group
	//		- report to user import alias --> import group name/key --> new group key
	//
	bool anyNewGroupSaved = false;
	__COUT__ << "Importing member tables..." << __E__;
	for(auto& group : importGroupMap)
	{
		__COUT__ << " ==> Group to import: " << group.first.first << " ("
		         << group.first.second << ")" << std::endl;

		std::string importGroupPath =
		    importPath + "/" + group.first.first + "_" + group.first.second.str();
		__COUTV__(importGroupPath);

		std::map<std::string /* tableName */, TableVersion /* tableVersion */>
		    memberTableSet;
		{  //get member table file names
			DIR*           dp;
			struct dirent* dirp;
			if((dp = opendir(importGroupPath.c_str())) == 0)
			{
				__COUT_ERR__ << "ERROR:(" << errno
				             << ").  Can't open directory: " << importGroupPath
				             << std::endl;
				exit(0);
			}

			const unsigned char isDir = 0x4;
			while((dirp = readdir(dp)) != 0)
				if(dirp->d_type != isDir && strlen(dirp->d_name) > 5 &&
				   dirp->d_name[strlen(dirp->d_name) - 5] == '.' &&
				   dirp->d_name[strlen(dirp->d_name) - 4] == 'j' &&
				   dirp->d_name[strlen(dirp->d_name) - 3] == 's' &&
				   dirp->d_name[strlen(dirp->d_name) - 2] == 'o' &&
				   dirp->d_name[strlen(dirp->d_name) - 1] ==
				       'n')  //if not directory w/extension .json
				{
					// __COUT__ << dirp->d_name << std::endl;

					auto split = StringMacros::getVectorFromString(dirp->d_name, {'_'});

					if(split.size() != 2)
						continue;

					memberTableSet.insert(std::pair<std::string, TableVersion>(
					    split[0], split[1].substr(1, split[1].size() - 6)));
				}  //end found group directory handling

			closedir(dp);
		}  //end load of group member table names/versions from directory

		std::map<std::string, TableVersion> groupMembers;
		for(auto& member : memberTableSet)
		{
			__COUT__ << "     ==> Member table to import: " << member.first << " v"
			         << member.second << std::endl;

			std::string fullpath = importGroupPath + "/" + member.first + "_v" +
			                       member.second.str() + ".json";

			std::string json;
			std::FILE*  fp = std::fopen(fullpath.c_str(), "rb");
			if(!fp)
			{
				__SS__ << "Could not open file at " << fullpath << ". Error: " << errno
				       << " - " << strerror(errno) << __E__;
				__SS_THROW__;
			}
			std::fseek(fp, 0, SEEK_END);
			json.resize(std::ftell(fp));
			std::rewind(fp);
			std::fread(&json[0], 1, json.size(), fp);
			std::fclose(fp);

			__COUTV__(json);

			auto memberTable = member.first == TableBase::GROUP_METADATA_TABLE_NAME
			                       ? cfgMgr->getMetadataTable()
			                       : cfgMgr->getTableByName(member.first);

			TableVersion newAssignedVersion;
			if(member.first == TableBase::GROUP_METADATA_TABLE_NAME)
			{
				//only one view ever for meta data table
				auto tableView = memberTable->getViewP();

				tableView->print();
				//clear all data from table (fill does not clear)
				while(tableView->getNumberOfRows() > 0)
					tableView->deleteRow(0);
				tableView->print();

				tableView->fillFromJSON(json);
				tableView->print();

				// set metadata table version to first available persistent version
				newAssignedVersion = TableVersion::getNextVersion(
				    theInterface_->findLatestVersion(memberTable));
				__COUTV__(newAssignedVersion);
				tableView->setVersion(newAssignedVersion);
				memberTable->getViewP()->print();

				// save table, and retry on save collision
				uint16_t retries = 0;
				while(1)
				{
					try
					{
						theInterface_->saveActiveVersion(memberTable);
					}
					catch(const std::runtime_error& e)
					{
						__COUT__ << "Caught runtime_error exception during table save: " << e.what()
						         << __E__;
						if(std::string(e.what()).find("there was a collision") !=
						   std::string::npos)
						{
							__COUT_WARN__
							    << "There was a collision saving the new table "
							    << *tableView << "(" << newAssignedVersion
							    << "), trying incremented table version... retries="
							    << retries << __E__;
							if(++retries > 3)  //give up
								throw;
							newAssignedVersion = TableVersion::getNextVersion(
							    newAssignedVersion);  //increment table version
							tableView->setVersion(newAssignedVersion);
							__COUT__ << "New version for table: " << *tableView
							         << " found as " << newAssignedVersion << __E__;
							continue;
						}
						else
							throw;
					}

					__COUT__ << "Created table: " << *tableView << "-v"
					         << newAssignedVersion << __E__;
					break;
				}  //end collission retry loop

				__COUTV__(memberTable->getViewVersion());
			}
			else  //else normal member table
			{
				auto temporaryVersion = memberTable->createTemporaryView();
				auto tableView        = memberTable->getViewP(temporaryVersion);

				tableView->fillFromJSON(json);
				tableView->print();

				bool foundEquivalent;
				newAssignedVersion =
				    cfgMgr->saveModifiedVersion(member.first,
				                                temporaryVersion,
				                                false /*makeTemporary*/,
				                                memberTable,
				                                temporaryVersion,
				                                false /*ignoreDuplicates*/,
				                                true /*lookForEquivalent*/,
				                                &foundEquivalent);
				if(foundEquivalent)
					__COUT__ << "Found equivalent version: " << newAssignedVersion
					         << __E__;
			}
			__COUTV__(newAssignedVersion);

			//assembly importTableMap at this point
			importTableMap[std::make_pair(member.first, member.second)] =
			    newAssignedVersion;
			groupMembers[member.first] = newAssignedVersion;
			// return;
		} //end member table import and find loop

		__COUT__ << "Tables imported so far: " << importTableMap.size() << std::endl;
		for(auto& table : importTableMap)
			__COUT__ << " ==> Member table imported: " << table.first.first << " v"
			         << table.first.second << " ==> v" << table.second << std::endl;

		__COUT__ << "Saving group '" << group.first.first
		         << "' members: " << groupMembers.size() << std::endl;

		std::map<std::string, TableVersion> groupMembersWithoutMeta;
		for(auto& table : groupMembers)
		{
			__COUT__ << " ==> Saving group w/Member table: " << table.first << " v"
			         << table.second << std::endl;

			if(table.first != TableBase::GROUP_METADATA_TABLE_NAME)
				groupMembersWithoutMeta[table.first] = table.second;
		}

		{  //check if group is a duplicate
			__COUT__ << "Checking for duplicate groups..." << __E__;
			try
			{
				TableGroupKey foundKey = cfgMgr->findTableGroup(
				    group.first.first,
				    groupMembersWithoutMeta);  //, memberTableAliases); std::map<std::string /*name*/, std::string /*alias*/>    memberTableAliases

				if(!foundKey.isInvalid())
				{
					__COUT_WARN__ << "Found equivalent group key (" << foundKey
					              << ") for " << group.first.first << ". Skipping import!"
					              << __E__;
					//update key import transformation map
					importGroupMap.at(
					    std::make_pair(group.first.first, group.first.second)) = foundKey;
					continue;
				}

				__COUT__ << "Check for duplicate groups complete." << __E__;
			}
			catch(...)
			{
				__COUT_WARN__
				    << "Ignoring errors looking for duplicate groups! Proceeding "
				       "with new group creation."
				    << __E__;
			}
		}  //end check if group is a duplicate
		// return;

		TableGroupKey newKey = TableGroupKey::getNextKey(
		    theInterface_->findLatestGroupKey(group.first.first));
		__COUT__ << "New Key for group: " << group.first.first << " found as " << newKey
		         << __E__;

		// save group, and retry on save collision
		uint16_t retries = 0;
		while(1)
		{
			try
			{
				theInterface_->saveTableGroup(
				    groupMembers,
				    TableGroupKey::getFullGroupString(group.first.first, newKey));
			}
			catch(const std::runtime_error& e)
			{
				__COUT__ << "Caught runtime_error exception during group save: " << e.what() << __E__;
				if(std::string(e.what()).find("there was a collision") !=
				   std::string::npos)
				{
					__COUT_WARN__
					    << "There was a collision saving the new group "
					    << group.first.first << "(" << newKey
					    << "), trying incremented group key... retries=" << retries
					    << __E__;
					if(++retries > 3)  //give up
						throw;
					newKey = TableGroupKey::getNextKey(newKey);  //increment group key
					__COUT__ << "New Key for group: " << group.first.first << " found as "
					         << newKey << __E__;
					continue;
				}
				else
					throw;
			}
			__COUT__ << "Created table group: " << group.first.first << "(" << newKey
			         << ")" << __E__;
			break;
		}  //end collission retry loop

		anyNewGroupSaved = true;

		//update key import transformation map
		importGroupMap.at(std::make_pair(group.first.first, group.first.second)) = newKey;
		// return;
	}  //end group import loop

	if(!forceBackboneSave && !anyNewGroupSaved)
	{
		__SS__ << "All groups to import already exist in current db! Was the wrong db "
		          "selected from which to import?"
		       << __E__;
		__SS_THROW__;
	}

	//		- report to user import alias --> import group name/key --> new group key
	__COUT__ << "Tables imported summary: " << importTableMap.size() << std::endl;
	for(auto& table : importTableMap)
		__COUT__ << " ==> Table imported: " << table.first.first << " v"
		         << table.first.second << " ==> v" << table.second << std::endl;

	__COUT__ << "Groups imported summary: " << importGroupMap.size() << std::endl;
	for(auto& group : importGroupMap)
		__COUT__ << " ==> Group imported: " << group.first.first << " ("
		         << group.first.second << ") ==> (" << group.second << ")" << std::endl;

	// return;

	// Done making groups, now...
	// 	-- insert new aliases for imported groups/tables in current active backbone
	//		- should be basename+alias connection to (hop through maps) new groupName & groupKey
	{  //insert new aliases for imported groups in current active backbone
		auto table =
		    cfgMgr->getTableByName(ConfigurationManager::GROUP_ALIASES_TABLE_NAME);
		__COUTV__(table->getTableName());
		__COUTV__(table->getViewVersion());

	}  //end insert new aliases for imported groups in current active backbone
	{  //insert new aliases for imported tables in current active backbone
		auto table =
		    cfgMgr->getTableByName(ConfigurationManager::VERSION_ALIASES_TABLE_NAME);
		__COUTV__(table->getTableName());
		__COUTV__(table->getViewVersion());

	}  //end insert new aliases for imported tables in current active backbone

	std::map<std::string, TableVersion> backboneMemberMap;
	{
		std::string accumulatedWarnings, accumulateErrors;
		cfgMgr->restoreActiveTableGroups(
		    false /*throwErrors*/,
		    "" /*pathToActiveGroupsFile*/,
		    ConfigurationManager::LoadGroupType::
		        ALL_TYPES,  //must do ALL_TYPES to not affect activeTablesGroup file
		    &accumulatedWarnings);

		activeBackboneGroupName =
		    cfgMgr->getActiveGroupName(ConfigurationManager::GroupType::BACKBONE_TYPE);
		cfgMgr->loadTableGroup(
		    activeBackboneGroupName,
		    cfgMgr->getActiveGroupKey(ConfigurationManager::GroupType::BACKBONE_TYPE),
		    true,
		    &backboneMemberMap,
		    0,
		    &accumulateErrors);
		__COUT__ << "Done re-loading active backbone: " << activeBackboneGroupName << " ("
		         << cfgMgr->getActiveGroupKey(
		                ConfigurationManager::GroupType::BACKBONE_TYPE)
		         << ")" << std::endl;
	}

	__COUT__ << "Modifying the active Backbone table to reflect new table versions and "
	            "group keys."
	         << std::endl;

	// Done making groups, now...
	// 	-- insert new aliases for imported groups/tables in current active backbone
	//		- should be basename+alias connection to (hop through maps) new groupName & groupKey
	{  //insert new aliases for imported groups in current active backbone
		auto table =
		    cfgMgr->getTableByName(ConfigurationManager::GROUP_ALIASES_TABLE_NAME);
		__COUTV__(table->getTableName());
		__COUTV__(table->getViewVersion());

		auto cfgView = table->getViewP();

		unsigned int col0 = cfgView->findCol("GroupKeyAlias");
		unsigned int col1 = cfgView->findCol("GroupName");
		unsigned int col2 = cfgView->findCol("GroupKey");
		unsigned int row;

		cfgView->print();

		TableVersion duplicateVersion;

		// 	-- insert new aliases for imported groups
		//		- should be basename+alias connection to (hop through maps) new
		// groupName & groupKey
		for(auto& aliasPair : importGroupAliasMap)
		{
			__COUT__ << "\t" << aliasPair.first << " ==> " << aliasPair.second.first
			         << "-v" << aliasPair.second.second << std::endl;

			auto groupIt = importGroupMap.find(std::pair<std::string, TableGroupKey>(
			    aliasPair.second.first, aliasPair.second.second));

			if(groupIt == importGroupMap.end())
			{
				__COUT__ << "Error! Could not find the new entry for the original group "
				         << aliasPair.second.first << "(" << aliasPair.second.second
				         << ")" << __E__;
				continue;
			}
			row =
			    cfgView->addRow(prepend + "import_aliases", true /*incrementUniqueData*/);
			cfgView->setValue(prepend + aliasPair.first, row, col0);
			cfgView->setValue(aliasPair.second.first, row, col1);
			cfgView->setValue(groupIt->second.toString(), row, col2);
		}  // end group alias edit

		if(!importGroupAliasMap.size())
			duplicateVersion =
			    table->getViewVersion();  //mark duplicate as self if nothing to add

		cfgView->print();

		TableVersion originalVersion =
		    table
		        ->getViewVersion();  //save version because cache fill will change active version
		if(duplicateVersion.isInvalid())
		{
			auto tableName = ConfigurationManager::GROUP_ALIASES_TABLE_NAME;
			__COUT__ << "Checking for duplicate '" << tableName << "' tables..." << __E__;

			{
				//"DEEP" checking
				//	load into cache 'recent' versions for this table
				//		'recent' := those already in cache, plus highest version numbers not in cache
				const std::map<std::string, TableInfo>& allTableInfo =
				    cfgMgr->getAllTableInfo();  // do not refresh

				auto versionReverseIterator =
				    allTableInfo.at(tableName)
				        .versions_.rbegin();  // get reverse iterator
				__COUT__ << "Filling up '" << tableName << "' cache from "
				         << table->getNumberOfStoredViews() << " to max count of "
				         << table->MAX_VIEWS_IN_CACHE << __E__;
				for(;
				    table->getNumberOfStoredViews() < table->MAX_VIEWS_IN_CACHE &&
				    versionReverseIterator != allTableInfo.at(tableName).versions_.rend();
				    ++versionReverseIterator)
				{
					__COUTT__ << "'" << tableName << "' versions in reverse order "
					          << *versionReverseIterator << __E__;
					try
					{
						cfgMgr->getVersionedTableByName(
						    tableName,
						    *versionReverseIterator);  // load to cache
					}
					catch(const std::runtime_error& e)
					{
						// ignore error
						__COUTT__ << "'" << tableName << "' version failed to load: "
						          << *versionReverseIterator << __E__;
					}
				}
			}

			__COUT__ << "Checking '" << tableName << "' for duplicate..." << __E__;
			duplicateVersion =
			    table->checkForDuplicate(originalVersion,
			                             TableVersion());  // then all versions in search

		}  //end check duplicate

		//return the original version to active
		table->setActiveView(originalVersion);

		if(!duplicateVersion.isInvalid())
		{
			// found an equivalent!
			__COUT__ << "Equivalent " << ConfigurationManager::GROUP_ALIASES_TABLE_NAME
			         << " table found in version v" << duplicateVersion << __E__;
			backboneMemberMap.at(ConfigurationManager::GROUP_ALIASES_TABLE_NAME) =
			    duplicateVersion;
		}
		else
		{
			auto newVersion =
			    TableVersion::getNextVersion(theInterface_->findLatestVersion(table));
			__COUTV__(newVersion);
			cfgView->setVersion(newVersion);

			// save table, and retry on save collision
			uint16_t retries = 0;
			while(1)
			{
				try
				{
					theInterface_->saveActiveVersion(table);
				}
				catch(const std::runtime_error& e)
				{
					__COUT__ << "Caught runtime_error exception during table save."
					         << __E__;
					if(std::string(e.what()).find("there was a collision") !=
					   std::string::npos)
					{
						__COUT_WARN__ << "There was a collision saving the new table "
						              << *cfgView << "(" << newVersion
						              << "), trying incremented table version... retries="
						              << retries << __E__;
						if(++retries > 3)  //give up
							throw;
						newVersion = TableVersion::getNextVersion(
						    newVersion);  //increment table version
						cfgView->setVersion(newVersion);
						__COUT__ << "New version for table: " << *cfgView << " found as "
						         << newVersion << __E__;
						continue;
					}
					else
						throw;
				}

				__COUT__ << "Created table: " << *cfgView << "-v" << newVersion << __E__;
				break;
			}  //end collission retry loop

			__COUT__ << "Updated backbone table "
			         << ConfigurationManager::GROUP_ALIASES_TABLE_NAME << " from v"
			         << backboneMemberMap.at(
			                ConfigurationManager::GROUP_ALIASES_TABLE_NAME)
			         << " to v" << newVersion << std::endl;
			backboneMemberMap.at(ConfigurationManager::GROUP_ALIASES_TABLE_NAME) =
			    newVersion;  // change version in the member map
		}

	}  //end insert new aliases for imported groups in current active backbone
	{  //insert new aliases for imported tables in current active backbone
		auto table =
		    cfgMgr->getTableByName(ConfigurationManager::VERSION_ALIASES_TABLE_NAME);
		__COUTV__(table->getTableName());
		__COUTV__(table->getViewVersion());

		auto         cfgView = table->getViewP();
		unsigned int col0    = cfgView->findCol("VersionAlias");
		unsigned int col1    = cfgView->findCol("TableName");
		unsigned int col2    = cfgView->findCol("Version");
		__COUTV__(table->getViewVersion());

		unsigned int row;

		cfgView->print();

		TableVersion duplicateVersion;

		// 	-- insert new aliases for imported tables
		//		- should be basename+alias connection to (hop through maps) new
		// tableName & tableVersion
		for(auto& aliasPair : importTableAliasMap)
		{
			__COUT__ << "\t" << aliasPair.first << " ==> " << aliasPair.second.first
			         << "-" << aliasPair.second.second << std::endl;

			auto tableIt = importTableMap.find(std::pair<std::string, TableVersion>(
			    aliasPair.second.first, aliasPair.second.second));

			if(tableIt == importTableMap.end())
			{
				__COUT__ << "Error! Could not find the new entry for the original table "
				         << aliasPair.second.first << "(" << aliasPair.second.second
				         << ")" << __E__;
				continue;
			}
			row =
			    cfgView->addRow(prepend + "import_aliases", true /*incrementUniqueData*/);
			cfgView->setValue(prepend + aliasPair.first, row, col0);
			cfgView->setValue(aliasPair.second.first, row, col1);
			cfgView->setValue(tableIt->second.toString(), row, col2);
		}  // end group alias edit
		__COUTV__(table->getViewVersion());

		if(!importTableAliasMap.size())
			duplicateVersion =
			    table->getViewVersion();  //mark duplicate as self if nothing to add

		__COUTV__(table->getViewVersion());
		cfgView->print();
		__COUTV__(table->getViewVersion());

		TableVersion originalVersion =
		    table
		        ->getViewVersion();  //save version because cache fill will change active version
		if(duplicateVersion.isInvalid())
		{
			auto tableName = ConfigurationManager::VERSION_ALIASES_TABLE_NAME;
			__COUT__ << "Checking for duplicate '" << tableName << "' tables..." << __E__;

			{
				//"DEEP" checking
				//	load into cache 'recent' versions for this table
				//		'recent' := those already in cache, plus highest version numbers not in cache
				const std::map<std::string, TableInfo>& allTableInfo =
				    cfgMgr->getAllTableInfo();  // do not refresh
				__COUTV__(table->getViewVersion());

				auto versionReverseIterator =
				    allTableInfo.at(tableName)
				        .versions_.rbegin();  // get reverse iterator
				__COUT__ << "Filling up '" << tableName << "' cache from "
				         << table->getNumberOfStoredViews() << " to max count of "
				         << table->MAX_VIEWS_IN_CACHE << __E__;
				for(;
				    table->getNumberOfStoredViews() < table->MAX_VIEWS_IN_CACHE &&
				    versionReverseIterator != allTableInfo.at(tableName).versions_.rend();
				    ++versionReverseIterator)
				{
					__COUTT__ << "'" << tableName << "' versions in reverse order "
					          << *versionReverseIterator << __E__;
					try
					{
						cfgMgr->getVersionedTableByName(
						    tableName,
						    *versionReverseIterator);  // load to cache
					}
					catch(const std::runtime_error& e)
					{
						// ignore error
						__COUTT__ << "'" << tableName << "' version failed to load: "
						          << *versionReverseIterator << __E__;
					}
					__COUTV__(table->getViewVersion());
				}
			}

			__COUT__ << "Checking '" << tableName << "' for duplicate..." << __E__;
			duplicateVersion =
			    table->checkForDuplicate(originalVersion,
			                             TableVersion());  // then all versions in search

			__COUTV__(table->getViewVersion());
		}  //end check duplicate

		//return the original version to active
		table->setActiveView(originalVersion);

		if(!duplicateVersion.isInvalid())
		{
			// found an equivalent!
			__COUT__ << "Equivalent " << ConfigurationManager::VERSION_ALIASES_TABLE_NAME
			         << " table found in version v" << duplicateVersion << __E__;
			backboneMemberMap.at(ConfigurationManager::VERSION_ALIASES_TABLE_NAME) =
			    duplicateVersion;  // change version in the member map
		}
		else
		{
			__COUTV__(table->getViewVersion());
			auto newVersion =
			    TableVersion::getNextVersion(theInterface_->findLatestVersion(table));
			__COUTV__(newVersion);
			cfgView->setVersion(newVersion);
			__COUTV__(cfgView->getVersion().toString());
			// table->setActiveView(newVersion);
			__COUTV__(table->getViewVersion());

			// save table, and retry on save collision
			uint16_t retries = 0;
			while(1)
			{
				try
				{
					theInterface_->saveActiveVersion(table);
				}
				catch(const std::runtime_error& e)
				{
					__COUT__ << "Caught runtime_error exception during table save."
					         << __E__;
					if(std::string(e.what()).find("there was a collision") !=
					   std::string::npos)
					{
						__COUT_WARN__ << "There was a collision saving the new table "
						              << *cfgView << "(" << newVersion
						              << "), trying incremented table version... retries="
						              << retries << __E__;
						if(++retries > 3)  //give up
							throw;
						newVersion = TableVersion::getNextVersion(
						    newVersion);  //increment table version
						cfgView->setVersion(newVersion);
						__COUT__ << "New version for table: " << *cfgView << " found as "
						         << newVersion << __E__;
						continue;
					}
					else
						throw;
				}

				__COUT__ << "Created table: " << *cfgView << "-v" << newVersion << __E__;
				break;
			}  //end collission retry loop

			__COUT__ << "Updated backbone table "
			         << ConfigurationManager::VERSION_ALIASES_TABLE_NAME << " from v"
			         << backboneMemberMap.at(
			                ConfigurationManager::VERSION_ALIASES_TABLE_NAME)
			         << " to v" << newVersion << std::endl;
			backboneMemberMap.at(ConfigurationManager::VERSION_ALIASES_TABLE_NAME) =
			    newVersion;  // change version in the member map
		}

	}  //end insert new aliases for imported tables in current active backbone

	__COUT_INFO__ << "Done inserting new aliases in active Backbone." << __E__;

	// return;

	//	-- save new backbone tables and save new backbone group, then activate
	{
		__COUT__ << "Backbone member map to create:" << std::endl;
		for(auto& member : backboneMemberMap)
			__COUT__ << " ==> Member to create: " << member.first << "-v" << member.second
			         << std::endl;

		bool foundExistingEmptyBackbone = false;
		{  //check if group is a duplicate
			__COUT__ << "Checking for duplicate backbone groups..." << __E__;
			try
			{
				TableGroupKey foundKey = cfgMgr->findTableGroup(
				    activeBackboneGroupName,
				    backboneMemberMap);  //, memberTableAliases); std::map<std::string /*name*/, std::string /*alias*/>    memberTableAliases

				if(!foundKey.isInvalid())
				{
					__COUT_WARN__ << "Found equivalent empty backbone group key ("
					              << foundKey << ") for " << activeBackboneGroupName
					              << ". Skipping creation and activating existing key!"
					              << __E__;
					foundExistingEmptyBackbone = true;

					//	-- backup the file ConfigurationManager::ACTIVE_GROUPS_FILENAME with time
					std::string renameFile =
					    ConfigurationManager::ACTIVE_GROUPS_FILENAME + "." + nowTime;
					rename(ConfigurationManager::ACTIVE_GROUPS_FILENAME.c_str(),
					       renameFile.c_str());

					__COUT__ << "Backing up '"
					         << ConfigurationManager::ACTIVE_GROUPS_FILENAME
					         << "' to ... '" << renameFile << "'" << std::endl;

					//	-- activate the existing empty backbone group
					cfgMgr->activateTableGroup(
					    activeBackboneGroupName,
					    foundKey);  // and write to active group file
				}

				__COUT__ << "Check for existing backbone duplicate groups complete."
				         << __E__;
			}
			catch(...)
			{
				__COUT_WARN__ << "Ignoring errors looking for existing backbone "
				                 "duplicate groups! Proceeding "
				                 "with new group creation."
				              << __E__;
			}
		}  //end check if group is a duplicate
		__COUTV__(foundExistingEmptyBackbone);

		if(!foundExistingEmptyBackbone)
		{
			auto newKey = TableGroupKey::getNextKey(
			    theInterface_->findLatestGroupKey(activeBackboneGroupName));

			__COUT__ << "Updating backbone group from ("
			         << cfgMgr->getActiveGroupKey(
			                ConfigurationManager::GroupType::BACKBONE_TYPE)
			         << ") to (" << newKey << ")" << __E__;

			// memberMap should now consist of members with new flat version, so save

			// save group, and retry on save collision
			uint16_t retries = 0;
			while(1)
			{
				try
				{
					theInterface_->saveTableGroup(backboneMemberMap,
					                              TableGroupKey::getFullGroupString(
					                                  activeBackboneGroupName, newKey));
				}
				catch(const std::runtime_error& e)
				{
					__COUT__ << "Caught runtime_error exception during group save."
					         << __E__;
					if(std::string(e.what()).find("there was a collision") !=
					   std::string::npos)
					{
						__COUT_WARN__
						    << "There was a collision saving the new group "
						    << activeBackboneGroupName << "(" << newKey
						    << "), trying incremented group key... retries=" << retries
						    << __E__;
						if(++retries > 3)  //give up
							throw;
						newKey = TableGroupKey::getNextKey(newKey);  //increment group key
						__COUT__ << "New Key for group: " << activeBackboneGroupName
						         << " found as " << newKey << __E__;
						continue;
					}
					else
						throw;
				}

				__COUT__ << "Created table group: " << activeBackboneGroupName << "("
				         << newKey << ")" << __E__;
				break;
			}  //end collission retry loop

			//	-- backup the file ConfigurationManager::ACTIVE_GROUPS_FILENAME with time
			std::string renameFile =
			    ConfigurationManager::ACTIVE_GROUPS_FILENAME + "." + nowTime;
			rename(ConfigurationManager::ACTIVE_GROUPS_FILENAME.c_str(),
			       renameFile.c_str());

			__COUT__ << "Backing up '" << ConfigurationManager::ACTIVE_GROUPS_FILENAME
			         << "' to ... '" << renameFile << "'" << std::endl;

			//	-- activate the new backbone group
			cfgMgr->activateTableGroup(activeBackboneGroupName,
			                           newKey);  // and write to active group file
		}
	}

	__COUT__ << "End of Importing Table Groups from path!\n\n\n" << std::endl;

}  //end ImportTableGroupsFromPath()

int main(int argc, char* argv[])
{
	if(getenv("OTSDAQ_LOG_FHICL") == NULL)
		setenv("OTSDAQ_LOG_FHICL",
		       (std::string(__ENV__("USER_DATA")) +
		        "/MessageFacilityConfigurations/MessageFacilityWithCout.fcl")
		           .c_str(),
		       1);

	if(getenv("OTSDAQ_LOG_ROOT") == NULL)
		setenv(
		    "OTSDAQ_LOG_ROOT", (std::string(__ENV__("USER_DATA")) + "/Logs").c_str(), 1);

	INIT_MF("ImportGroupsFromPath");
	try
	{
		ImportTableGroupsFromPath(argc, argv);
	}
	catch(...)
	{
		__COUT_ERR__ << "Unhandled exception caught in main()!" << __E__
			<< StringMacros::stackTrace() << std::endl;
		throw;
	}

	return 0;

}
// BOOST_AUTO_TEST_SUITE_END()
