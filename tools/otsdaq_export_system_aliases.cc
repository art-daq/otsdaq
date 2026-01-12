#include "otsdaq/MessageFacility/MessageFacility.h"

#include <dirent.h>
#include <cassert>
#include <iostream>
#include <memory>
#include <string>

#include "otsdaq/ConfigurationInterface/ConfigurationInterface.h"
#include "otsdaq/ConfigurationInterface/ConfigurationManagerRW.h"
// #include "artdaq-database/StorageProviders/FileSystemDB/provider_filedb_index.h"
// #include "artdaq-database/JsonDocument/JSONDocument.h"

// Shared test utilities
#include "otsdaq/Macros/TestUtilities.h"

/// Exports all active groups and system alias groups (specified by the active backbone)
///		to the path specified. Each group's member tables will be organized in a folder with metadata.
///		Each table is exported as a json format text file.
/// usage:
/// otsdaq_export_system_aliases <path to export to>
///
///
using namespace ots;

void ExportActiveSystemAliasTableGroups(int argc, char* argv[])
{
	std::cout << "=================================================\n";
	std::cout << "=================================================\n";
	std::cout << "=================================================\n";
	__COUT__ << "\nExporting Active System Aliases!" << std::endl;

	std::cout << "\n\nusage: One argument:\n\t <export path> \n\n" << std::endl;

	std::cout << "argc = " << argc << std::endl;
	for(int i = 0; i < argc; i++)
		std::cout << "argv[" << i << "] = " << argv[i] << std::endl;

	if(argc != 2)
	{
		std::cout << "Error! Must provide one parameter.\n\n" << std::endl;
		return;
	}

	std::string exportPath = argv[1];
	__COUTV__(exportPath);

	// make directory just in case
	mkdir(exportPath.c_str(), 0755);

	DIR* dp;
	if((dp = opendir(exportPath.c_str())) == 0)
	{
		__COUT_ERR__ << "ERROR:(" << errno
		             << ").  Can't open directory for export: " << exportPath
		             << std::endl;
		exit(0);
	}
	closedir(dp);

	// return;
	
	//==============================================================================
	// Define environment variables
	//	Note: normally these environment variables are set by ots script

	test::util::check_and_make_envs();
	////////////////////////////////////////////////////

	//==============================================================================
	// get prepared with initial source db

	// ConfigurationManager instance immediately loads active groups
	__COUT__ << "Loading active Aliases..." << std::endl;

	std::string ARTDAQ_DATABASE_URI = __ENV__("ARTDAQ_DATABASE_URI");
	__COUTV__(ARTDAQ_DATABASE_URI);

	// return;

	ConfigurationManagerRW  cfgMgrInst("export_admin");
	ConfigurationManagerRW* cfgMgr = &cfgMgrInst;

	if(0)
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
		__COUTV__(groups.size());
		for(auto& group : groups)
		{
			__COUTV__(group.first);
		}
	}

	if(1)
	{
		std::string accumulatedWarnings;
		cfgMgr->restoreActiveTableGroups(false /*throwErrors*/,
		                                 "" /*pathToActiveGroupsFile*/,
		                                 ConfigurationManager::LoadGroupType::ALL_TYPES,
		                                 &accumulatedWarnings);

		__COUT__ << "Done Loading active groups." << std::endl;
	}
	// return;

	// create set of groups to persist
	//	include active context
	//	include active backbone
	//	include active iterate group
	//	include active config group
	//		(keep key translation separate activeGroupKeys)
	//	include all groups with system aliases

	// for each group in set
	//	load/activate group
	//		export each table as json doc to a folder <group name>_<export time>/

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

	// add active groups to set
	std::map<std::string, std::pair<std::string, TableGroupKey>> activeGroupsMap =
	    cfgMgr->getActiveTableGroups();

	bool foundAnyActiveGroups = false;

	for(const auto& activeGroup : activeGroupsMap)
	{
		if(activeGroup.second.second.TableGroupKey::isInvalid())
			continue;

		groupSet.insert(std::pair<std::pair<std::string, TableGroupKey>, TableGroupKey>(
		    std::pair<std::string, TableGroupKey>(activeGroup.second.first,
		                                          activeGroup.second.second),
		    TableGroupKey()));
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

	if(activeBackboneGroupName == "" || !foundAnyActiveGroups)
	{
		__SS__ << "Did not find valid active groups to export! "
		          "Must have a backbone at least. Is the current database URI correct? "
		          "ARTDAQ_DATABASE_URI = "
		       << ARTDAQ_DATABASE_URI << std::endl;
		__SS_THROW__;
	}

	__COUT__ << "Identified active groups:" << std::endl;
	for(auto& group : groupSet)
		__COUT__ << " ==> Group to export: " << group.first.first << " ("
		         << group.first.second << ")" << std::endl;
	// return;

	// add system alias groups to set
	const std::string groupAliasesTableName =
	    ConfigurationManager::GROUP_ALIASES_TABLE_NAME;
	std::map<std::string, TableVersion> activeVersions = cfgMgr->getActiveVersions();
	if(activeVersions.find(groupAliasesTableName) == activeVersions.end())
	{
		__SS__ << "\nActive version of " << groupAliasesTableName << " missing! "
		       << groupAliasesTableName
		       << " is a required member of the Backbone configuration group."
		       << "\n\nLikely you need to activate a valid Backbone group." << std::endl;
		__SS_THROW__;
	}

	std::vector<std::pair<std::string, ConfigurationTree>> aliasNodePairs =
	    cfgMgr->getNode(groupAliasesTableName).getChildren();
	for(auto& groupPair : aliasNodePairs)
		groupSet.insert(std::pair<std::pair<std::string, TableGroupKey>, TableGroupKey>(
		    std::pair<std::string, TableGroupKey>(
		        groupPair.second.getNode("GroupName").getValueAsString(),
		        TableGroupKey(groupPair.second.getNode("GroupKey").getValueAsString())),
		    TableGroupKey()));

	//at this point, all groups to export have been identified! ------------
	__COUT__ << "All identified groups:" << std::endl;
	for(auto& group : groupSet)
		__COUT__ << " ==> Group to export: " << group.first.first << " ("
		         << group.first.second << ")" << std::endl;
	__COUT__ << std::endl;
	__COUT__ << std::endl;

	if(!groupSet.size())
	{
		__SS__ << "No groups identified to export!" << __E__;
		__SS_THROW__;
	}

	ConfigurationInterface* theInterface_ = ConfigurationInterface::getInstance(
	    ConfigurationInterface::CONFIGURATION_MODE::ARTDAQ_DATABASE);

	//now, for each group in set
	//	load/activate group
	//		export each table as json doc to a folder <group name>_<export time>/

	bool errDetected = false;
	for(auto& group : groupSet)
	{
		__COUT__ << " ==> Group to export: " << group.first.first << " ("
		         << group.first.second << ")" << std::endl;

		std::string groupPath =
		    exportPath + "/" + group.first.first + "_" + group.first.second.str();
		__COUTV__(groupPath);
		//check if output directory already exists, and do not overwrite throw error
		{
			DIR* dp;
			if((dp = opendir(groupPath.c_str())) != 0)
			{
				closedir(dp);
				__COUT__ << "ERROR: Can't export to directory (already exists! Please "
				            "choose a different export path.): "
				         << groupPath << std::endl;
				exit(0);
			}
		}
		mkdir(groupPath.c_str(), 0755);

		if(group.first.first ==
		   activeBackboneGroupName)  //create special metadata file to flag the active Backbone group
		{
			std::string groupIsBackbonePath = groupPath + "/" + "groupIsBackbone.txt";
			__COUTV__(groupIsBackbonePath);
			FILE* fp = std::fopen(groupIsBackbonePath.c_str(), "w");
			if(!fp)
			{
				__COUT_ERR__ << "\n\nERROR! Could not open file at "
				             << groupIsBackbonePath << ". Error: " << errno << " - "
				             << strerror(errno) << __E__;
				return;
			}
			fputs("1", fp);
			fclose(fp);
		}

		std::string                                           accumulateErrors = "";
		std::map<std::string /*name*/, TableVersion>          memberMap;
		std::map<std::string /*name*/, std::string /*alias*/> groupAliases;
		std::string                                           groupComment;
		std::string                                           groupAuthor;
		std::string                                           groupCreateTime;
		std::string                                           groupTypeString;

		//=========================
		// load group, group metadata, and tables from original DB
		try
		{
			accumulateErrors = "";
			cfgMgr->loadTableGroup(group.first.first,
			                       group.first.second,
			                       true /*doActivate*/,
			                       0,  //&memberMap /*memberMap*/,
			                       0 /*progressBar*/,
			                       &accumulateErrors,
			                       &groupComment,
			                       &groupAuthor,
			                       &groupCreateTime,
			                       false /*doNotLoadMember*/,
			                       &groupTypeString /*groupTypeString*/,
			                       &groupAliases);
		}
		catch(std::runtime_error& e)
		{
			__COUT__ << "Error was caught loading members for " << group.first.first
			         << "(" << group.first.second << ")" << std::endl;
			__COUT__ << e.what() << std::endl;
			errDetected = true;
		}
		catch(...)
		{
			__COUT__ << "Error was caught loading members for " << group.first.first
			         << "(" << group.first.second << ")" << std::endl;
			errDetected = true;
		}

		//Record if group is a Backbone group, because Aliases need to be handled specially on import
		if(groupTypeString == ConfigurationManager::GROUP_TYPE_NAME_BACKBONE)
		{
			__COUT__ << "\t\tFound backbone type group!" << __E__;
		}

		//get full member map with Metadata table
		{
			// std::map<std::string /*name*/, TableVersion /*version*/> memberMap =
			memberMap = theInterface_->getTableGroupMembers(
			    TableGroupKey::getFullGroupString(group.first.first, group.first.second),
			    true /*include meta data table*/);

			// save meta data table separately, since there is no table definition, and then remove from member map
			auto metaTablePair = memberMap.find(TableBase::GROUP_METADATA_TABLE_NAME);
			if(metaTablePair != memberMap.end())
			{
				__COUT__ << TableBase::GROUP_METADATA_TABLE_NAME << ":v"
				         << metaTablePair->second << std::endl;

				std::string tablePath = groupPath + "/" +
				                        TableBase::GROUP_METADATA_TABLE_NAME + "_v" +
				                        metaTablePair->second.str() + ".json";
				__COUTV__(tablePath);

				auto groupMetadataTable = cfgMgr->getMetadataTable(metaTablePair->second);

				__COUTV__(tablePath);
				std::stringstream json;
				groupMetadataTable->getViewP()->printJSON(json);

				__COUTV__(tablePath);
				FILE* fp = std::fopen(tablePath.c_str(), "w");
				if(!fp)
				{
					__COUT_ERR__ << "\n\nERROR! Could not open file at " << tablePath
					             << ". Error: " << errno << " - " << strerror(errno)
					             << __E__;
					return;
				}
				fputs(json.str().c_str(), fp);
				fclose(fp);

				memberMap.erase(
				    metaTablePair);  // remove from member map that is returned

			}  // end metadata handling
			else
			{
				__COUT_ERR__ << "Ignoring that groupMetadataTable_ is missing for group '"
				             << group.first.first << "(" << group.first.second
				             << "). Going with anonymous defaults." << __E__;
			}
		}

		// TableView* cfgView;
		// TableBase* config;
		//=========================
		// export the group tables to json documents!
		try
		{
			// saving tables
			for(auto& memberPair : memberMap)
			{
				__COUT__ << memberPair.first << ":v" << memberPair.second << std::endl;

				std::string tablePath = groupPath + "/" + memberPair.first + "_v" +
				                        memberPair.second.str() + ".json";
				__COUTV__(tablePath);

				std::stringstream json;
				cfgMgr->getTableByName(memberPair.first)->getViewP()->printJSON(json);

				FILE* fp = std::fopen(tablePath.c_str(), "w");
				if(!fp)
				{
					__COUT_ERR__ << "\n\nERROR! Could not open file at " << tablePath
					             << ". Error: " << errno << " - " << strerror(errno)
					             << __E__;
					return;
				}
				fputs(json.str().c_str(), fp);
				fclose(fp);

				// change the version of the active view to flatVersion and save it
				// config  = cfgMgr->getTableByName(memberPair.first);
				// cfgView = config->getViewP();
				// cfgView->setVersion(TableVersion(flatVersion));
				// theInterface_->saveActiveVersion(config);

			}  //end member table loop

			// Note: this code copies actions in ConfigurationManagerRW::saveNewTableGroup

			// add meta data
			__COUTV__(StringMacros::mapToString(groupAliases));
			__COUTV__(groupComment);
			__COUTV__(groupAuthor);
			__COUTV__(groupCreateTime);
			// time_t        groupCreateTime_t;
			// sscanf(groupCreateTime.c_str(), "%ld", &groupCreateTime_t);
			// __COUTV__(groupCreateTime_t);

			// theInterface_->saveActiveVersion(groupMetadataTable);

			// memberMap should now consist of members with new flat version, so save
			// group
			// theInterface_->saveTableGroup(
			//     memberMap,
			//     TableGroupKey::getFullGroupString(groupPair.first.first,
			//                                       TableGroupKey(flatVersion)));
		}
		catch(std::runtime_error& e)
		{
			__COUT__ << "Error was caught exporting group " << group.first.first << " ("
			         << group.first.second << ") " << std::endl;
			__COUT__ << e.what() << std::endl;

			groupErrors.insert(
			    std::pair<std::pair<std::string, TableGroupKey>, std::string>(
			        std::pair<std::string, TableGroupKey>(group.first.first,
			                                              group.first.second),
			        "Error caught exporting the group."));
		}
		catch(...)
		{
			__COUT__ << "Error was caught saving group " << group.first.first << " ("
			         << group.first.second << ") " << std::endl;

			groupErrors.insert(
			    std::pair<std::pair<std::string, TableGroupKey>, std::string>(
			        std::pair<std::string, TableGroupKey>(group.first.first,
			                                              group.first.second),
			        "Error caught exporting the group."));
		}
	}  //end group export loop

	if(errDetected)
	{
		__COUT_ERR__ << "There was an error detected while exporting groups." << __E__;
	}

	__COUT_INFO__ << "Exported group summary: " << groupSet.size() << std::endl;
	for(auto& group : groupSet)
		__COUT_INFO__ << " ==> Group: " << group.first.first << " (" << group.first.second
		              << ")" << std::endl;

	__COUT_INFO__ << "****************************" << std::endl;
	__COUT_INFO__ << "There were " << groupSet.size()
	              << " groups considered, and there were " << groupErrors.size()
	              << " errors found handling those groups. The groups were exported in "
	                 "text/json format to the export path: "
	              << exportPath << std::endl;
	if(groupErrors.size())
	{
		__COUT_ERR__ << "There were " << groupErrors.size()
		             << " errors found while loading and exporting groups. The "
		                "following errors were found handling the groups:"
		             << std::endl;
		for(auto& groupErr : groupErrors)
			__COUT_ERR__ << "\t" << groupErr.first.first << " " << groupErr.first.second
			             << ": \t" << groupErr.second << std::endl;
		__COUT_ERR__ << "End of errors.\n\n" << std::endl;
	}
	else
		__COUT_INFO__ << "There were NO ERRORS found while loading and exporting groups."
		              << __E__;

}  //end ExportActiveSystemAliasTableGroups()

int main(int argc, char* argv[])
{
	ExportActiveSystemAliasTableGroups(argc, argv);
	return 0;
}
// BOOST_AUTO_TEST_SUITE_END()
