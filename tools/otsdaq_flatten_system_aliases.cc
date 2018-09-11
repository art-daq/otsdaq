#include <string>
#include <iostream>
#include <memory>
#include <cassert>
#include <dirent.h>
#include "otsdaq-core/ConfigurationInterface/ConfigurationManagerRW.h"
#include "otsdaq-core/ConfigurationInterface/ConfigurationInterface.h"
//#include "artdaq-database/StorageProviders/FileSystemDB/provider_filedb_index.h"
//#include "artdaq-database/JsonDocument/JSONDocument.h"

//usage:
// otsdaq_flatten_system_aliases <baseFlatVersion> <pathToSwapIn (optional)>
//
// if baseFlatVersion is invalid or temporary nothing is saved in the new db
//	(Note: this can be used to swap dbs using pathToSwapIn)

using namespace ots;


void FlattenActiveSystemAliasConfigurationGroups(int argc, char* argv[])
{
	std::cout << "=================================================\n";
	std::cout << "=================================================\n";
	std::cout << "=================================================\n";
	__COUT__<< "\nFlattening Active System Aliases!" << std::endl;

	std::cout << "\n\nusage: Two arguments:\n\t pathToSwapIn <baseFlatVersion> <pathToSwapIn (optional)> \n\n" <<
			"\t Default values: baseFlatVersion = 0, pathToSwapIn = \"\" \n\n" <<
			std::endl;

	std::cout << "\n\nNote: you can optionally just swap databases (and not modify their contents at all)" <<
			" by providing an invalid baseFlatVersion of -1.\n\n" <<
			std::endl;

	std::cout << "\n\nNote: This assumes artdaq db file type interface. " <<
			"The current database/ will be moved to database_<linuxtime>/ " <<
			"and if a pathToSwapIn is specified it will be copied to database/ " <<
			"before saving the currently active groups.\n\n" <<
			std::endl;

	std::cout << "argc = " << argc << std::endl;
	for(int i = 0; i < argc; i++)
		std::cout << "argv[" << i << "] = " << argv[i] << std::endl;

	if(argc < 2)
	{
		std::cout << "Error! Must provide at least one parameter.\n\n" << std::endl;
		return;
	}

	//determine if "h"elp was first parameter
	std::string flatVersionStr = argv[1];
	if(flatVersionStr.find('h') != std::string::npos)
	{
		std::cout << "Recognized parameter 1. as a 'help' option. Usage was printed. Exiting." << std::endl;
		return;
	}

	int flatVersion = 0;
	std::string pathToSwapIn = "";
	if(argc >= 2)
		sscanf(argv[1],"%d",&flatVersion);
	if(argc >= 3)
		pathToSwapIn = argv[2];

	__COUT__<< "flatVersion = " << flatVersion << std::endl;
	__COUT__<< "pathToSwapIn = " << pathToSwapIn << std::endl;

	//return;
	//==============================================================================
	//Define environment variables
	//	Note: normally these environment variables are set by StartOTS.sh

	//These are needed by otsdaq/otsdaq-core/ConfigurationDataFormats/ConfigurationInfoReader.cc [207]
	setenv("CONFIGURATION_TYPE","File",1);	//Can be File, Database, DatabaseTest
	setenv("CONFIGURATION_DATA_PATH",(std::string(getenv("USER_DATA")) + "/ConfigurationDataExamples").c_str(),1);
	setenv("CONFIGURATION_INFO_PATH",(std::string(getenv("USER_DATA")) + "/ConfigurationInfo").c_str(),1);
	////////////////////////////////////////////////////

	//Some configuration plug-ins use getenv("SERVICE_DATA_PATH") in init() so define it
	setenv("SERVICE_DATA_PATH",(std::string(getenv("USER_DATA")) + "/ServiceData").c_str(),1);

	//Some configuration plug-ins use getenv("OTSDAQ_LIB") and getenv("OTSDAQ_UTILITIES_LIB") in init() so define it
	//	to a non-sense place is ok
	setenv("OTSDAQ_LIB",(std::string(getenv("USER_DATA")) + "/").c_str(),1);
	setenv("OTSDAQ_UTILITIES_LIB",(std::string(getenv("USER_DATA")) + "/").c_str(),1);

	//Some configuration plug-ins use getenv("OTS_MAIN_PORT") in init() so define it
	setenv("OTS_MAIN_PORT","2015",1);

	//also xdaq envs for XDAQContextConfiguration
	setenv("XDAQ_CONFIGURATION_DATA_PATH",(std::string(getenv("USER_DATA")) + "/XDAQConfigurations").c_str(),1);
	setenv("XDAQ_CONFIGURATION_XML","otsConfigurationNoRU_CMake",1);
	////////////////////////////////////////////////////

	//==============================================================================
	//get prepared with initial source db

	//ConfigurationManager instance immediately loads active groups
	std::cout << "\n\n\n" << __COUT_HDR_FL__ << "Loading active Aliases..." << std::endl;
	ConfigurationManagerRW cfgMgrInst("flatten_admin");
	ConfigurationManagerRW *cfgMgr = &cfgMgrInst;


	//create set of groups to persist
	//	include active context
	//	include active backbone
	//	include active iterate group
	//	include active config group
	//		(keep key translation separate activeGroupKeys)
	//	include all groups with system aliases

	//for group in set
	//	load/activate group and flatten tables to flatVersion to new DB
	//		save new version to modifiedTables
	//	save group with flatVersion key to new DB
	//		save new key to groupSet
	//	++flatVersion

	//reload the active backbone (using activeGroupKeys)
	//	modify group aliases and table aliases properly based on groupSet and modifiedTables
	//	save new backbone with flatVersion to new DB

	//backup the file ConfigurationManager::ACTIVE_GROUP_FILENAME with time
	// 	and change the ConfigurationManager::ACTIVE_GROUP_FILENAME
	//	to reflect new group names/keys


	/* map<<groupName, origKey>, newKey> */
	std::map<std::pair<std::string,ConfigurationGroupKey>,
		ConfigurationGroupKey> 					groupSet;
	/* <tableName, <origVersion, newVersion> >*/
	std::map<std::pair<std::string,ConfigurationVersion>,
		ConfigurationVersion>					modifiedTables;
	std::map<std::string, std::pair<ConfigurationGroupKey,
		ConfigurationGroupKey>>					activeGroupKeys;
	std::map<std::pair<std::string,ConfigurationGroupKey>,
		std::string> 							groupErrors;

	std::string									activeBackboneGroupName = "";
	std::string									activeContextGroupName = "";
	std::string									activeIterateGroupName = "";
	std::string									activeConfigGroupName = "";

	std::string									nowTime = std::to_string(time(0));

	std::string									thenTime = "";
	if(pathToSwapIn != "") //get target then time
	{
		thenTime = pathToSwapIn.substr(pathToSwapIn.rfind('_')+1);
		std::cout << __COUT_HDR_FL__<< "thenTime = " << thenTime << std::endl;
		//return;
	}

	//add active groups to set
	std::map<std::string, std::pair<std::string, ConfigurationGroupKey>> activeGroupsMap =
			cfgMgr->getActiveConfigurationGroups();

	for(const auto &activeGroup: activeGroupsMap)
	{
		groupSet.insert(std::pair<
				std::pair<std::string,ConfigurationGroupKey>,
				ConfigurationGroupKey> (
						std::pair<std::string,ConfigurationGroupKey>(
								activeGroup.second.first,
								activeGroup.second.second),
								ConfigurationGroupKey())
		);
		activeGroupKeys.insert(std::pair<
				std::string,
				std::pair<ConfigurationGroupKey,ConfigurationGroupKey>> (
						activeGroup.second.first,
						std::pair<ConfigurationGroupKey,ConfigurationGroupKey>(
								activeGroup.second.second,
								ConfigurationGroupKey()))
		);

		if(activeGroup.first == ConfigurationManager::ACTIVE_GROUP_NAME_BACKBONE)
		{
			activeBackboneGroupName = activeGroup.second.first;
			std::cout << __COUT_HDR_FL__<< "found activeBackboneGroupName = " <<
					activeBackboneGroupName << std::endl;
		}
		else if(activeGroup.first == ConfigurationManager::ACTIVE_GROUP_NAME_CONTEXT)
		{
			activeContextGroupName = activeGroup.second.first;
			std::cout << __COUT_HDR_FL__<< "found activeContextGroupName = " <<
					activeContextGroupName << std::endl;
		}
		else if(activeGroup.first == ConfigurationManager::ACTIVE_GROUP_NAME_ITERATE)
		{
			activeIterateGroupName = activeGroup.second.first;
			std::cout << __COUT_HDR_FL__<< "found activeIterateGroupName = " <<
					activeIterateGroupName << std::endl;
		}
		else if(activeGroup.first == ConfigurationManager::ACTIVE_GROUP_NAME_CONFIGURATION)
		{
			activeConfigGroupName = activeGroup.second.first;
			std::cout << __COUT_HDR_FL__<< "found activeConfigGroupName = " <<
					activeConfigGroupName << std::endl;
		}
	}

	//add system alias groups to set
	const std::string groupAliasesTableName = ConfigurationManager::GROUP_ALIASES_CONFIG_NAME;
	std::map<std::string, ConfigurationVersion> activeVersions = cfgMgr->getActiveVersions();
	if(activeVersions.find(groupAliasesTableName) == activeVersions.end())
	{
		__SS__ << "\nActive version of " << groupAliasesTableName << " missing! " <<
				groupAliasesTableName << " is a required member of the Backbone configuration group." <<
				"\n\nLikely you need to activate a valid Backbone group." <<
				std::endl;
		__SS_THROW__;
	}


	std::vector<std::pair<std::string,ConfigurationTree> > aliasNodePairs =
			cfgMgr->getNode(groupAliasesTableName).getChildren();
	for(auto& groupPair:aliasNodePairs)
		groupSet.insert(std::pair<
				std::pair<std::string,ConfigurationGroupKey>,
				ConfigurationGroupKey> (
						std::pair<std::string,ConfigurationGroupKey>(
								groupPair.second.getNode("GroupName").getValueAsString(),
								ConfigurationGroupKey(groupPair.second.getNode("GroupKey").getValueAsString())),
								ConfigurationGroupKey())
		);

	std::cout << __COUT_HDR_FL__<< "Identified groups:" << std::endl;
	for(auto& group:groupSet)
		std::cout << __COUT_HDR_FL__<< group.first.first << " " << group.first.second << std::endl;
	std::cout << __COUT_HDR_FL__<< std::endl;
	std::cout << __COUT_HDR_FL__<< std::endl;

	//return;
	//==============================================================================
	//prepare to manipulate directories
	std::string currentDir = getenv("ARTDAQ_DATABASE_URI");

	if(currentDir.find("filesystemdb://") != 0)
	{
		__SS__ << "filesystemdb:// was not found in $ARTDAQ_DATABASE_URI!" << std::endl;
		__COUT_ERR__ << "\n" << ss.str();
		__SS_THROW__;
	}

	currentDir = currentDir.substr(std::string("filesystemdb://").length());
	while(currentDir.length() && currentDir[currentDir.length()-1] == '/') //remove trailing '/'s
		currentDir = currentDir.substr(0,currentDir.length()-1);
	std::string moveToDir = currentDir + "_" + nowTime;
	if(argc < 2)
	{
		__SS__ << ("Aborting move! Must at least give version argument to flatten to!") << std::endl;
		__COUT_ERR__ << "\n" << ss.str();
		__SS_THROW__;
	}

	if(pathToSwapIn != "")
	{
		DIR *dp;
		if((dp = opendir(pathToSwapIn.c_str())) == 0)
		{
			std::cout << __COUT_HDR_FL__<< "ERROR:(" << errno << ").  Can't open directory: " << pathToSwapIn << std::endl;
			exit(0);
		}
		closedir(dp);
	}

	//handle directory swap
	__COUT__<< "Moving current directory: \t" << currentDir << std::endl;
	__COUT__<< "\t... to: \t\t" << moveToDir << std::endl;
	//return;
	rename(currentDir.c_str(),moveToDir.c_str());

	if(pathToSwapIn != "")
	{
		__COUT__<< "Swapping in directory: \t" << pathToSwapIn << std::endl;
		__COUT__<< "\t.. to: \t\t" << currentDir << std::endl;
		rename(pathToSwapIn.c_str(),currentDir.c_str());

		//also swap in active groups file
		//check if original active file exists
		std::string activeGroupsFile = ConfigurationManager::ACTIVE_GROUP_FILENAME + "." + thenTime;
		FILE *fp = fopen(activeGroupsFile.c_str(),"r");
		if(fp)
		{
			__COUT__ << "Swapping active groups file: \t" <<
					activeGroupsFile << std::endl;
			__COUT__<< "\t.. to: \t\t" <<
					ConfigurationManager::ACTIVE_GROUP_FILENAME << std::endl;
			rename(activeGroupsFile.c_str(),
					ConfigurationManager::ACTIVE_GROUP_FILENAME.c_str());
		}
	}





	ConfigurationInterface* theInterface_ = ConfigurationInterface::getInstance(false); //true for File interface, false for artdaq database;
	ConfigurationView* cfgView;
	ConfigurationBase* config;

	bool errDetected;
	std::string accumulateErrors = "";
	int count = 0;

	std::map<std::string /*name*/, ConfigurationVersion> 			memberMap;
	std::map<std::string /*name*/, std::string /*alias*/> 	groupAliases;
	std::string 			groupComment;
	std::string 			groupAuthor;
	std::string	 			groupCreateTime;
	time_t					groupCreateTime_t;
	ConfigurationBase*		groupMetadataTable = cfgMgr->getMetadataTable();

	//don't do anything more if flatVersion is not persistent
	if(ConfigurationVersion(flatVersion).isInvalid() ||
			ConfigurationVersion(flatVersion).isTemporaryVersion())
	{
		__COUT__<< "\n\nflatVersion " << ConfigurationVersion(flatVersion) <<
				" is an invalid or temporary version. Skipping to end!" << std::endl;
		goto CLEAN_UP;
	}

	for(auto& groupPair:groupSet)
	{
		errDetected = false;

		__COUT__<< "****************************" << std::endl;
		__COUT__<< "Loading members for " <<
				groupPair.first.first <<
				"(" << groupPair.first.second << ")" <<
				std::endl;
		__COUT__<< "flatVersion = " << flatVersion << std::endl;

		//handle directory swap BACK
		if(pathToSwapIn != "")
		{
			__COUT__<< "REVERT by Swapping back directory: \t" << currentDir << std::endl;
			__COUT__<< "\t.. to: \t\t" << pathToSwapIn << std::endl;
			if(rename(currentDir.c_str(),pathToSwapIn.c_str()) < 0)
			{
				__SS__ << "Problem!" << std::endl;
				__SS_THROW__;
			}

		}
		else if(count) //if not first time, move currentDir to temporarily holding area
		{
			__COUT__<< "REVERT by Moving directory: \t" << currentDir << std::endl;
			__COUT__<< "\t.. to temporary directory: \t\t" << (moveToDir+"_tmp") << std::endl;
			if(rename(currentDir.c_str(),(moveToDir+"_tmp").c_str()) < 0)
			{
				__SS__ << "Problem!" << std::endl;
				__SS_THROW__;
			}
		}


		__COUT__<< "REVERT by Moving directory: \t" << moveToDir << std::endl;
		__COUT__<< "\t... to: \t\t" << currentDir << std::endl;
		if(rename(moveToDir.c_str(),currentDir.c_str()) < 0)
		{
			__SS__ << "Problem!" << std::endl;
			__SS_THROW__;
		}


		//=========================
		//load group, group metadata, and tables from original DB
		try
		{
			cfgMgr->loadConfigurationGroup(
					groupPair.first.first,
					groupPair.first.second,
					true /*doActivate*/,
					&memberMap/*memberMap*/,0 /*progressBar*/,&accumulateErrors,
					&groupComment,
					&groupAuthor,
					&groupCreateTime,
					false /*doNotLoadMember*/,
					0 /*groupTypeString*/,
					&groupAliases
					);
		}
		catch(std::runtime_error& e)
		{

			__COUT__<< "Error was caught loading members for " <<
					groupPair.first.first <<
					"(" << groupPair.first.second << ")" <<
					std::endl;
			__COUT__<< e.what() << std::endl;
			errDetected = true;
		}
		catch(...)
		{
			__COUT__<< "Error was caught loading members for " <<
					groupPair.first.first <<
					"(" << groupPair.first.second << ")" <<
					std::endl;
			errDetected = true;
		}

		//=========================

		//if(count == 2) break;


		//handle directory swap
		__COUT__<< "Moving current directory: \t" << currentDir << std::endl;
		__COUT__<< "\t... to: \t\t" << moveToDir << std::endl;
		if(rename(currentDir.c_str(),moveToDir.c_str()) < 0)
		{
			__SS__ << "Problem!" << std::endl;
			__SS_THROW__;
		}

		if(pathToSwapIn != "")
		{
			__COUT__<< "Swapping in directory: \t" << pathToSwapIn << std::endl;
			__COUT__<< "\t.. to: \t\t" << currentDir << std::endl;
			if(rename(pathToSwapIn.c_str(),currentDir.c_str()) < 0)
			{
				__SS__ << "Problem!" << std::endl;
				__SS_THROW__;
			}
		}
		else if(count) //if not first time, replace from temporarily holding area
		{
			__COUT__<< "Moving temporary directory: \t" << (moveToDir+"_tmp") << std::endl;
			__COUT__<< "\t.. to current directory: \t\t" << currentDir << std::endl;
			if(rename((moveToDir+"_tmp").c_str(),currentDir.c_str()) < 0)
			{
				__SS__ << "Problem!" << std::endl;
				__SS_THROW__;
			}
		}


		//exit loop if any (loading) failure
		if(errDetected)
		{
			//goto CLEAN_UP;

			//power on if group failed
			//	and record error

			groupErrors.insert(std::pair<
					std::pair<std::string,ConfigurationGroupKey>,
					std::string> (
							std::pair<std::string,ConfigurationGroupKey>(
									groupPair.first.first,
									groupPair.first.second),
									"Error caught loading the group."));
			continue;
		}




		//=========================
		//save group and its tables with new key and versions!
		try
		{
			//saving tables
			for(auto &memberPair:memberMap)
			{
				__COUT__<< memberPair.first << ":v" << memberPair.second << std::endl;

				//check if table has already been modified by a previous group
				//	(i.e. two groups using the same version of a table)
				if(modifiedTables.find(std::pair<std::string,ConfigurationVersion>(
						memberPair.first,
						memberPair.second
						)) != modifiedTables.end())
				{

					__COUT__<< "Table was already modified!" << std::endl;
					memberPair.second = modifiedTables[std::pair<std::string,ConfigurationVersion>(
							memberPair.first,
							memberPair.second
							)];
					__COUT__<< "\t to...\t" <<
							memberPair.first << ":v" << memberPair.second << std::endl;
					continue;
				}

				//change the version of the active view to flatVersion and save it
				config = cfgMgr->getConfigurationByName(memberPair.first);
				cfgView = config->getViewP();
				cfgView->setVersion(ConfigurationVersion(flatVersion));
				theInterface_->saveActiveVersion(config);

				//set it back for the table so that future groups can re-use cached version
				cfgView->setVersion(memberPair.second); //IMPORTANT


				//save new version to modifiedTables
				modifiedTables.insert(std::pair<
						std::pair<std::string,ConfigurationVersion>,
						ConfigurationVersion> (
								std::pair<std::string,ConfigurationVersion>(
										memberPair.first,
										memberPair.second),
										ConfigurationVersion(flatVersion))
				);

				memberPair.second = flatVersion; //change version in the member map

				__COUT__<< "\t to...\t" <<
						memberPair.first << ":v" << memberPair.second << std::endl;
			}

			//Note: this code copies actions in ConfigurationManagerRW::saveNewConfigurationGroup

			//add meta data
			__COUTV__(StringMacros::mapToString(groupAliases));
			__COUTV__(groupComment);
			__COUTV__(groupAuthor);
			__COUTV__(groupCreateTime);
			sscanf(groupCreateTime.c_str(),"%ld",&groupCreateTime_t);
			__COUTV__(groupCreateTime_t);

			//to compensate for unusual errors upstream, make sure the metadata table has one row
			while(groupMetadataTable->getViewP()->getNumberOfRows() > 1)
				groupMetadataTable->getViewP()->deleteRow(0);
			if(groupMetadataTable->getViewP()->getNumberOfRows() == 0)
				groupMetadataTable->getViewP()->addRow();

			//columns are uid,comment,author,time
			//ConfigurationManager::METADATA_COL_ALIASES TODO
			groupMetadataTable->getViewP()->setValue(
					StringMacros::mapToString(groupAliases,
							"," /*primary delimiter*/,":" /*secondary delimeter*/),
							0,ConfigurationManager::METADATA_COL_ALIASES);
			groupMetadataTable->getViewP()->setValue(groupComment		,0,ConfigurationManager::METADATA_COL_COMMENT);
			groupMetadataTable->getViewP()->setValue(groupAuthor		,0,ConfigurationManager::METADATA_COL_AUTHOR);
			groupMetadataTable->getViewP()->setValue(groupCreateTime_t	,0,ConfigurationManager::METADATA_COL_TIMESTAMP);

			//set version of metadata table
			groupMetadataTable->getViewP()->setVersion(ConfigurationVersion(flatVersion));
			theInterface_->saveActiveVersion(groupMetadataTable);

			//force groupMetadataTable_ to be a member for the group
			memberMap[groupMetadataTable->getConfigurationName()] =
					groupMetadataTable->getViewVersion();

			//memberMap should now consist of members with new flat version, so save group
			theInterface_->saveConfigurationGroup(memberMap,
					ConfigurationGroupKey::getFullGroupString(
							groupPair.first.first,
							ConfigurationGroupKey(flatVersion)));

			//and modify groupSet and activeGroupKeys keys
			groupPair.second = ConfigurationGroupKey(flatVersion);

			//if this is an active group, save key change
			if(activeGroupKeys.find(groupPair.first.first) != activeGroupKeys.end() &&
					activeGroupKeys[groupPair.first.first].first == groupPair.first.second)
				activeGroupKeys[groupPair.first.first].second =
						ConfigurationGroupKey(flatVersion);
		}
		catch(std::runtime_error& e)
		{
			__COUT__<< "Error was caught saving group " <<
					groupPair.first.first <<
					" (" << groupPair.first.second << ") " <<
					std::endl;
			__COUT__<< e.what() << std::endl;

			groupErrors.insert(std::pair<
					std::pair<std::string,ConfigurationGroupKey>,
					std::string> (
							std::pair<std::string,ConfigurationGroupKey>(
									groupPair.first.first,
									groupPair.first.second),
									"Error caught saving the group."));
		}
		catch(...)
		{
			__COUT__<< "Error was caught saving group " <<
					groupPair.first.first <<
					" (" << groupPair.first.second << ") " <<
					std::endl;

			groupErrors.insert(std::pair<
					std::pair<std::string,ConfigurationGroupKey>,
					std::string> (
							std::pair<std::string,ConfigurationGroupKey>(
									groupPair.first.first,
									groupPair.first.second),
									"Error caught saving the group."));
		}
		//=========================


		//increment flat version
		++flatVersion;
		++count;
	}

	//record in readme for moveto
	{
		FILE *fp = fopen((moveToDir + "/README_otsdaq_flatten.txt").c_str(),"a");
		if(!fp)
			__COUT__<< "\tError opening README file!" << std::endl;
		else
		{
			time_t rawtime;
			struct tm * timeinfo;
			char buffer [200];

			time (&rawtime);
			timeinfo = localtime (&rawtime);
			strftime (buffer,200,"%b %d, %Y %I:%M%p %Z",timeinfo);

			fprintf(fp,"This database was moved from...\n\t %s \nto...\n\t %s \nat this time \n\t %lu \t %s\n\n\n",
					currentDir.c_str(),moveToDir.c_str(),time(0),buffer);

			fclose(fp);
		}
	}


	//record in readme for swapin
	{
		FILE *fp = fopen((currentDir + "/README_otsdaq_flatten.txt").c_str(),"a");

		if(!fp)
			__COUT__<< "\tError opening README file!" << std::endl;
		else
		{
			time_t rawtime;
			struct tm * timeinfo;
			char buffer [200];

			time (&rawtime);
			timeinfo = localtime (&rawtime);
			strftime (buffer,200,"%b %d, %Y %I:%M:%S%p %Z",timeinfo);

			fprintf(fp,"This database was moved from...\t %s \t to...\t %s at this time \t %lu \t %s\n\n",
					pathToSwapIn.c_str(),currentDir.c_str(),time(0),buffer);
			fclose(fp);
		}
	}

//	//print resulting all groups
//
//	std::cout << "\n\n" << __COUT_HDR_FL__ << "Resulting Groups:" << std::endl;
//	for(const auto &group: groupSet)
//		__COUT__<< group.first.first << ": " <<
//			group.first.second << " => " << group.second << std::endl;
//	std::cout << "\n\n" << __COUT_HDR_FL__ << "Resulting Groups end." << std::endl;
//
//
//	//print resulting active groups
//
//	std::cout << "\n\n" << __COUT_HDR_FL__ << "Resulting Active Groups:" << std::endl;
//	for(const auto &activeGroup: activeGroupKeys)
//		__COUT__<< activeGroup.first << ": " <<
//			activeGroup.second.first << " => " << activeGroup.second.second << std::endl;
//
//	__COUT__<< activeBackboneGroupName << " is the " <<
//			ConfigurationManager::ACTIVE_GROUP_NAME_BACKBONE << "." << std::endl;
//	std::cout << "\n\n" << __COUT_HDR_FL__ << "Resulting Active Groups end." << std::endl;





	//reload the active backbone (using activeGroupKeys)
	//	modify group aliases and table aliases properly based on groupSet and modifiedTables
	//	save new backbone with flatVersion to new DB

	if(activeBackboneGroupName == "")
	{
		__COUT__<< "No active Backbone table identified." << std::endl;
		goto CLEAN_UP;
	}

	std::cout << "\n\n" << __COUT_HDR_FL__ <<
			"Modifying the active Backbone table to reflect new table versions and group keys." <<
			std::endl;

	{
		 cfgMgr->loadConfigurationGroup(
						activeBackboneGroupName,
						activeGroupKeys[activeBackboneGroupName].second,
						true,&memberMap,0,&accumulateErrors);

		//modify Group Aliases Configuration and Version Aliases Configuration to point
		//	at DEFAULT and flatVersion respectively

		const std::string groupAliasesName = ConfigurationManager::GROUP_ALIASES_CONFIG_NAME;
		const std::string versionAliasesName = ConfigurationManager::VERSION_ALIASES_CONFIG_NAME;


		std::map<std::string, ConfigurationVersion> activeMap = cfgMgr->getActiveVersions();

		//modify Group Aliases Configuration
		if(activeMap.find(groupAliasesName) != activeMap.end())
		{
			__COUT__<< "\n\nModifying " << groupAliasesName << std::endl;
			config = cfgMgr->getConfigurationByName(groupAliasesName);
			cfgView = config->getViewP();

			unsigned int col1 = cfgView->findCol("GroupName");
			unsigned int col2 = cfgView->findCol("GroupKey");

			//cfgView->print();

			//change all key entries found to the new key and delete rows for groups not found
			bool found;
			for(unsigned int row = 0; row<cfgView->getNumberOfRows(); ++row )
			{
				found = false;
				for(const auto &group: groupSet)
					if(group.second.isInvalid()) continue;
					else if(cfgView->getDataView()[row][col1] == group.first.first &&
							cfgView->getDataView()[row][col2] == group.first.second.toString())
					{
						//found a matching group/key pair
						__COUT__<<
								"Changing row " << row << " for " <<
								cfgView->getDataView()[row][col1] << " key=" <<
								cfgView->getDataView()[row][col2] << " to NEW key=" <<
								group.second << std::endl;
						cfgView->setValue(
								group.second.toString(),
								row,col2);
						found = true;
						break;
					}

				if(!found) //delete row
					cfgView->deleteRow(row--);
			}
			//cfgView->print();
		}

		//modify Version Aliases Configuration
		if(activeMap.find(versionAliasesName) != activeMap.end())
		{
			__COUT__<< "\n\nModifying " << versionAliasesName << std::endl;
			config = cfgMgr->getConfigurationByName(versionAliasesName);
			cfgView = config->getViewP();
			unsigned int col1 = cfgView->findCol("ConfigurationName");
			unsigned int col2 = cfgView->findCol("Version");

			//change all version entries to the new version and delete rows with no match
			bool found;
			for(unsigned int row = 0; row<cfgView->getNumberOfRows(); ++row )
			{
				found = false;
				for(const auto &table:modifiedTables)
					if(cfgView->getDataView()[row][col1] == table.first.first &&
							cfgView->getDataView()[row][col2] == table.first.second.toString())
					{
						//found a matching group/key pair
						__COUT__<< "Changing row " << row << " for " <<
								cfgView->getDataView()[row][col1] << " version=" <<
								cfgView->getDataView()[row][col2] << " to NEW version=" <<
								table.second << std::endl;
						cfgView->setValue(table.second.toString(),row,col2);
						found = true;
						break;
					}

				if(!found) //delete row
					cfgView->deleteRow(row--);
			}
		}



		//save new Group Aliases Configuration and Version Aliases Configuration

		__COUT__<< groupAliasesName << ":v" <<
				memberMap[groupAliasesName] << std::endl;
		//change the version of the active view to flatVersion and save it
		config = cfgMgr->getConfigurationByName(groupAliasesName);
		cfgView = config->getViewP();
		cfgView->setVersion(ConfigurationVersion(flatVersion));
		theInterface_->saveActiveVersion(config);

		memberMap[groupAliasesName] = flatVersion; //change version in the member map

		__COUT__<< "\t to...\t" <<
				groupAliasesName << ":v" << memberMap[groupAliasesName] << std::endl;



		__COUT__<< versionAliasesName << ":v" <<
				memberMap[versionAliasesName] << std::endl;
		//change the version of the active view to flatVersion and save it
		config = cfgMgr->getConfigurationByName(versionAliasesName);
		cfgView = config->getViewP();
		cfgView->setVersion(ConfigurationVersion(flatVersion));
		theInterface_->saveActiveVersion(config);

		memberMap[versionAliasesName] = flatVersion; //change version in the member map

		__COUT__<< "\t to...\t" <<
				versionAliasesName << ":v" << memberMap[versionAliasesName] << std::endl;

		//memberMap should now consist of members with new flat version, so save
		theInterface_->saveConfigurationGroup(memberMap,
				ConfigurationGroupKey::getFullGroupString(
						activeBackboneGroupName,
						ConfigurationGroupKey(flatVersion)));

		activeGroupKeys[activeBackboneGroupName].second =
								ConfigurationGroupKey(flatVersion);

		__COUT__<< "New to-be-active backbone group " << activeBackboneGroupName <<
				":v" << activeGroupKeys[activeBackboneGroupName].second << std::endl;
	}



	//backup the file ConfigurationManager::ACTIVE_GROUP_FILENAME with time
	// 	and change the ConfigurationManager::ACTIVE_GROUP_FILENAME
	//	to reflect new group names/keys

	{
		std::cout << "\n\n" << __COUT_HDR_FL__ << "Manipulating the Active Groups file..." << std::endl;

		//check if original active file exists
		FILE *fp = fopen(ConfigurationManager::ACTIVE_GROUP_FILENAME.c_str(),"r");
		if(!fp)
		{
			__SS__ << "Original active groups file '" << ConfigurationManager::ACTIVE_GROUP_FILENAME
					<< "' not found." << std::endl;
			goto CLEAN_UP;
		}

		__COUT__<< "Backing up file: " << ConfigurationManager::ACTIVE_GROUP_FILENAME
				<< std::endl;

		fclose(fp);

		std::string renameFile = ConfigurationManager::ACTIVE_GROUP_FILENAME + "." + nowTime;
		rename(ConfigurationManager::ACTIVE_GROUP_FILENAME.c_str(),
				renameFile.c_str());

		__COUT__<< "Backup file name: " << renameFile
				<< std::endl;

		ConfigurationGroupKey         	*theConfigurationGroupKey_,	*theContextGroupKey_, 	*theBackboneGroupKey_,	*theIterateGroupKey_;
		std::string						theConfigurationGroup_, 	theContextGroup_, 		theBackboneGroup_,		theIterateGroup_;

		theConfigurationGroup_ = activeConfigGroupName;
		theConfigurationGroupKey_ = &(activeGroupKeys[activeConfigGroupName].second);

		theContextGroup_ = activeContextGroupName;
		theContextGroupKey_ = &(activeGroupKeys[activeContextGroupName].second);

		theBackboneGroup_ = activeBackboneGroupName;
		theBackboneGroupKey_ = &(activeGroupKeys[activeBackboneGroupName].second);

		theIterateGroup_ = activeIterateGroupName;
		theIterateGroupKey_ = &(activeGroupKeys[activeIterateGroupName].second);


		//the following is copied from ConfigurationManagerRW::activateConfigurationGroup
		{
			__COUT__<< "Updating persistent active groups to " <<
					ConfigurationManager::ACTIVE_GROUP_FILENAME << " ..."  << std::endl;

			std::string fn = ConfigurationManager::ACTIVE_GROUP_FILENAME;
			FILE *fp = fopen(fn.c_str(),"w");
			if(!fp) return;

			fprintf(fp,"%s\n",theContextGroup_.c_str());
			fprintf(fp,"%s\n",theContextGroupKey_?theContextGroupKey_->toString().c_str():"-1");
			fprintf(fp,"%s\n",theBackboneGroup_.c_str());
			fprintf(fp,"%s\n",theBackboneGroupKey_?theBackboneGroupKey_->toString().c_str():"-1");
			fprintf(fp,"%s\n",theConfigurationGroup_.c_str());
			fprintf(fp,"%s\n",theConfigurationGroupKey_?theConfigurationGroupKey_->toString().c_str():"-1");
			fprintf(fp,"%s\n",theIterateGroup_.c_str());
			fprintf(fp,"%s\n",theIterateGroupKey_?theIterateGroupKey_->toString().c_str():"-1");
			fclose(fp);
		}
	}

	//print resulting all groups

	std::cout << "\n\n" << __COUT_HDR_FL__ << "Resulting Groups:" << std::endl;
	for(const auto &group: groupSet)
		__COUT__<< "\t" << group.first.first << ": " <<
			group.first.second << " => " << group.second << std::endl;
	std::cout << "\n\n" << __COUT_HDR_FL__ << "Resulting Groups end." << std::endl;


	//print resulting active groups

	std::cout << "\n\n" << __COUT_HDR_FL__ << "Resulting Active Groups:" << std::endl;
	for(const auto &activeGroup: activeGroupKeys)
		__COUT__<< "\t" << activeGroup.first << ": " <<
			activeGroup.second.first << " => " << activeGroup.second.second << std::endl;

	__COUT__<< activeBackboneGroupName << " is the " <<
			ConfigurationManager::ACTIVE_GROUP_NAME_BACKBONE << "." << std::endl;
	std::cout << "\n\n" << __COUT_HDR_FL__ << "Resulting Active Groups end." << std::endl;


CLEAN_UP:
	//==============================================================================
	std::cout << "\n\n" << __COUT_HDR_FL__ << "End of Flattening Active Configuration Groups!\n\n\n" << std::endl;



	__COUT__<< "****************************" << std::endl;
	__COUT__<< "There were " << groupSet.size() <<
			" groups considered, and there were " << groupErrors.size() <<
			" errors found handling those groups." << std::endl;
	__COUT__<< "The following errors were found handling the groups:" << std::endl;
	for(auto& groupErr:groupErrors)
		__COUT__<< "\t" << groupErr.first.first << " " << groupErr.first.second <<
			": \t" << groupErr.second << std::endl;
	__COUT__<< "End of errors.\n\n" << std::endl;


	__COUT__<< "Run the following to return to your previous database structure:" <<
			std::endl;
	__COUT__<< "\t otsdaq_flatten_system_aliases -1 " << moveToDir <<
			"\n\n" << std::endl;


	return;
}

int main(int argc, char* argv[])
{
	FlattenActiveSystemAliasConfigurationGroups(argc,argv);
	return 0;
}
//BOOST_AUTO_TEST_SUITE_END()

