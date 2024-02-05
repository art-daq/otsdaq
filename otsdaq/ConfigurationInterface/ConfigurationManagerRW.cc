#include "otsdaq/ConfigurationInterface/ConfigurationManagerRW.h"

#include <dirent.h>


using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "ConfigurationManagerRW"

#define TABLE_INFO_PATH std::string(__ENV__("TABLE_INFO_PATH")) + "/"
#define TABLE_INFO_EXT "Info.xml"

#define CORE_TABLE_INFO_FILENAME                                                                                                                   \
	((getenv("SERVICE_DATA_PATH") == NULL) ? (std::string(__ENV__("USER_DATA")) + "/ServiceData") : (std::string(__ENV__("SERVICE_DATA_PATH")))) + \
	    "/CoreTableInfoNames.dat"

std::atomic<bool>			ConfigurationManagerRW::firstTimeConstructed_ = true;

//==============================================================================
// ConfigurationManagerRW
ConfigurationManagerRW::ConfigurationManagerRW(const std::string& username) : ConfigurationManager(username)  // for use as author of new views
{
	__GEN_COUT__ << "Instantiating Config Manager with Write Access! (for " << username << ") time=" << time(0) << " runTimeSeconds()=" << runTimeSeconds() << __E__;

	theInterface_ = ConfigurationInterface::getInstance(false);  // false to use artdaq DB

	//=========================
	// dump names of core tables (so UpdateOTS.sh can copy core tables for user)
	// only if table does not exist
	if(firstTimeConstructed_)
	{
		firstTimeConstructed_ = false;
			
		// make table group history directory here and at Gateway Supervisor (just in case)
		mkdir((ConfigurationManager::LAST_TABLE_GROUP_SAVE_PATH).c_str(), 0755);

		const std::set<std::string>& contextMemberNames  = getContextMemberNames();
		const std::set<std::string>& backboneMemberNames = getBackboneMemberNames();
		const std::set<std::string>& iterateMemberNames  = getIterateMemberNames();

		FILE* fp = fopen((CORE_TABLE_INFO_FILENAME).c_str(), "r");

		if(fp)  // check for all core table names in file, and force their presence
		{
			std::vector<unsigned int> foundVector;
			char                      line[100];
			for(const auto& name : contextMemberNames)
			{
				foundVector.push_back(false);
				rewind(fp);
				while(fgets(line, 100, fp))
				{
					if(strlen(line) < 1)
						continue;
					line[strlen(line) - 1] = '\0';                           // remove endline
					if(strcmp(line, ("ContextGroup/" + name).c_str()) == 0)  // is match?
					{
						foundVector.back() = true;
						break;
					}
				}
			}

			for(const auto& name : backboneMemberNames)
			{
				foundVector.push_back(false);
				rewind(fp);
				while(fgets(line, 100, fp))
				{
					if(strlen(line) < 1)
						continue;
					line[strlen(line) - 1] = '\0';                            // remove endline
					if(strcmp(line, ("BackboneGroup/" + name).c_str()) == 0)  // is match?
					{
						foundVector.back() = true;
						break;
					}
				}
			}

			for(const auto& name : iterateMemberNames)
			{
				foundVector.push_back(false);
				rewind(fp);
				while(fgets(line, 100, fp))
				{
					if(strlen(line) < 1)
						continue;
					line[strlen(line) - 1] = '\0';                           // remove endline
					if(strcmp(line, ("IterateGroup/" + name).c_str()) == 0)  // is match?
					{
						foundVector.back() = true;
						break;
					}
				}
			}

			fclose(fp);

			// open file for appending the missing names
			fp = fopen((CORE_TABLE_INFO_FILENAME).c_str(), "a");
			if(fp)
			{
				unsigned int i = 0;
				for(const auto& name : contextMemberNames)
				{
					if(!foundVector[i])
						fprintf(fp, "\nContextGroup/%s", name.c_str());

					++i;
				}
				for(const auto& name : backboneMemberNames)
				{
					if(!foundVector[i])
						fprintf(fp, "\nBackboneGroup/%s", name.c_str());

					++i;
				}
				for(const auto& name : iterateMemberNames)
				{
					if(!foundVector[i])
						fprintf(fp, "\nIterateGroup/%s", name.c_str());

					++i;
				}
				fclose(fp);
			}
			else
			{
				__SS__ << "Failed to open core table info file for appending: " << CORE_TABLE_INFO_FILENAME << __E__;
				__SS_THROW__;
			}
		}
		else
		{
			fp = fopen((CORE_TABLE_INFO_FILENAME).c_str(), "w");
			if(fp)
			{
				fprintf(fp, "ARTDAQ/*");
				fprintf(fp, "\nConfigCore/*");
				for(const auto& name : contextMemberNames)
					fprintf(fp, "\nContextGroup/%s", name.c_str());
				for(const auto& name : backboneMemberNames)
					fprintf(fp, "\nBackboneGroup/%s", name.c_str());
				for(const auto& name : iterateMemberNames)
					fprintf(fp, "\nIterateGroup/%s", name.c_str());
				fclose(fp);
			}
			else
			{
				__SS__ << "Failed to open core table info file: " << CORE_TABLE_INFO_FILENAME << __E__;
				__SS_THROW__;
			}
		}
	}  // end dump names of core tables

	__GEN_COUTV__(runTimeSeconds());
}  // end constructor

//==============================================================================
// getAllTableInfo
//	Used by ConfigurationGUISupervisor to get all the info for the existing tables.
//	Can also be used to get and cache group info.
//
// if(accumulatedWarnings)
//	this implies allowing column errors and accumulating such errors in given string
const std::map<std::string, TableInfo>& ConfigurationManagerRW::getAllTableInfo(bool               refresh,
                                                                                std::string*       accumulatedWarnings /* = 0 */,
                                                                                const std::string& errorFilterName /* = "" */,
																				bool			   getGroupKeys /* = false */,
																				bool			   getGroupInfo /* = false */,
																				bool			   initializeActiveGroups /* = false */)
{
	// allTableInfo_ is container to be returned

	if(!refresh)
		return allTableInfo_;

	// else refresh!
	allTableInfo_.clear();

	TableBase* table;

	// existing configurations are defined by which infos are in TABLE_INFO_PATH
	// can test that the class exists based on this
	// and then which versions
	__GEN_COUT__ << "======================================================== getAllTableInfo start runtime=" << runTimeSeconds() << __E__;
	__GEN_COUT__ << "Refreshing all! Extracting list of tables..." << __E__;
	DIR*                pDIR;
	struct dirent*      entry;
	std::string         path              = TABLE_INFO_PATH;
	char                fileExt[]         = TABLE_INFO_EXT;
	const unsigned char MIN_TABLE_NAME_SZ = 3;
	if((pDIR = opendir(path.c_str())) != 0)
	{
		while((entry = readdir(pDIR)) != 0)
		{
			// enforce table name length
			if(strlen(entry->d_name) < strlen(fileExt) + MIN_TABLE_NAME_SZ)
				continue;

			// find file names with correct file extenstion
			if(strcmp(&(entry->d_name[strlen(entry->d_name) - strlen(fileExt)]), fileExt) != 0)
				continue;  // skip different extentions

			entry->d_name[strlen(entry->d_name) - strlen(fileExt)] = '\0';  // remove file extension to get table name

			// 0 will force the creation of new instance (and reload from Info)
			table = 0;

			try  // only add valid table instances to maps
			{
				theInterface_->get(table, entry->d_name, 0, 0,
				                   true);  // dont fill
			}
			catch(cet::exception const&)
			{
				if(table)
					delete table;
				table = 0;

				__GEN_COUT__ << "Skipping! No valid class found for... " << entry->d_name << "\n";
				continue;
			}
			catch(std::runtime_error& e)
			{
				if(table)
					delete table;
				table = 0;

				__GEN_COUT__ << "Skipping! No valid class found for... " << entry->d_name << "\n";
				__GEN_COUT__ << "Error: " << e.what() << __E__;

				// for a runtime_error, it is likely that columns are the problem
				//	the Table Editor needs to still fix these.. so attempt to
				// 	proceed.
				if(accumulatedWarnings)
				{
					if(errorFilterName == "" || errorFilterName == entry->d_name)
					{
						*accumulatedWarnings += std::string("\nIn table '") + entry->d_name + "'..." + e.what();  // global accumulate

						__SS__ << "Attempting to allow illegal columns!" << __E__;
						*accumulatedWarnings += ss.str();
					}

					// attempt to recover and build a mock-up
					__GEN_COUT__ << "Attempting to allow illegal columns!" << __E__;

					std::string returnedAccumulatedErrors;
					try
					{
						table = new TableBase(entry->d_name, &returnedAccumulatedErrors);
					}
					catch(...)
					{
						__GEN_COUT__ << "Skipping! Allowing illegal columns didn't work either... " << entry->d_name << "\n";
						continue;
					}
					__GEN_COUT__ << "Error (but allowed): " << returnedAccumulatedErrors << __E__;

					if(errorFilterName == "" || errorFilterName == entry->d_name)
						*accumulatedWarnings += std::string("\nIn table '") + entry->d_name + "'..." + returnedAccumulatedErrors;  // global accumulate
				}
				else
					continue;
			}

			if(nameToTableMap_[entry->d_name])  // handle if instance existed
			{
				// copy the temporary versions! (or else all is lost)
				std::set<TableVersion> versions = nameToTableMap_[entry->d_name]->getStoredVersions();
				for(auto& version : versions)
					if(version.isTemporaryVersion())
					{
						try  // do NOT let TableView::init() throw here
						{
							nameToTableMap_[entry->d_name]->setActiveView(version);
							table->copyView(  // this calls TableView::init()
							    nameToTableMap_[entry->d_name]->getView(),
							    version,
							    username_);
						}
						catch(...)  // do NOT let invalid temporary version throw at this
						            // point
						{
						}  // just trust configurationBase throws out the failed version
					}

				delete nameToTableMap_[entry->d_name];
				nameToTableMap_[entry->d_name] = 0;
			}

			nameToTableMap_[entry->d_name] = table;

			allTableInfo_[entry->d_name].tablePtr_ = table;
			allTableInfo_[entry->d_name].versions_ = theInterface_->getVersions(table);

			// also add any existing temporary versions to all table info
			//	because the interface wont find those versions
			std::set<TableVersion> versions = nameToTableMap_[entry->d_name]->getStoredVersions();
			for(auto& version : versions)
				if(version.isTemporaryVersion())
				{
					allTableInfo_[entry->d_name].versions_.emplace(version);
				}
		}
		closedir(pDIR);
	}
	__GEN_COUT__ << "Extracting list of tables complete." << __E__;


	// call init to load active versions by default, activate with warnings allowed (assuming development going on)
	if(initializeActiveGroups)
	{ 
		__GEN_COUT__ << "Now initializing..." << __E__;
		// if there is a filter name, do not include init warnings (it just scares people in the table editor)
		std::string tmpAccumulateWarnings;
		init(0 /*accumulatedErrors*/, false /*initForWriteAccess*/, accumulatedWarnings ? &tmpAccumulateWarnings : nullptr);

		if(accumulatedWarnings && errorFilterName == "")
			*accumulatedWarnings += tmpAccumulateWarnings;
	}
	__GEN_COUT__ << "======================================================== getAllTableInfo end runtime=" << runTimeSeconds() << __E__;

	// get Group Info too!
	if(getGroupKeys || getGroupInfo)
	{
		allGroupInfo_.clear();
		try
		{
			// build allGroupInfo_ for the ConfigurationManagerRW

			std::set<std::string /*name*/> tableGroups = theInterface_->getAllTableGroupNames();
			__GEN_COUT__ << "Number of Groups: " << tableGroups.size() << __E__;

			__GEN_COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__ << "Group Info start runtime=" << runTimeSeconds() << __E__;
		
			TableGroupKey key;
			std::string   name;
			for(const auto& fullName : tableGroups)
			{
				TableGroupKey::getGroupNameAndKey(fullName, name, key);
				cacheGroupKey(name, key);
			}

			__GEN_COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__ << "Group Keys end runtime=" << runTimeSeconds() << __E__;

			// for each group get member map & comment, author, time, and type for latest key			
			if(getGroupInfo)
			{				
				const int numOfThreads = PROCESSOR_COUNT/2;
				__GEN_COUT__ << " PROCESSOR_COUNT " << PROCESSOR_COUNT << " ==> " << numOfThreads << " threads." << __E__;
				if(numOfThreads < 2) // no multi-threading
					for(auto& groupInfo : allGroupInfo_)
					{
						try
						{
							loadTableGroup(groupInfo.first /*groupName*/,
										groupInfo.second.getLatestKey(),
										false /*doActivate*/,
										&groupInfo.second.latestKeyMemberMap_ /*groupMembers*/,
										0 /*progressBar*/,
										0 /*accumulateErrors*/,
										&groupInfo.second.latestKeyGroupComment_,
										&groupInfo.second.latestKeyGroupAuthor_,
										&groupInfo.second.latestKeyGroupCreationTime_,
										true /*doNotLoadMember*/,
										&groupInfo.second.latestKeyGroupTypeString_);
						}
						catch(...)
						{
							__GEN_COUT_WARN__ << "Error occurred loading latest group info into cache for '"
											<< groupInfo.first << "(" << groupInfo.second.getLatestKey() << ")'..." << __E__;
							groupInfo.second.latestKeyGroupComment_      = ConfigurationManager::UNKNOWN_INFO;
							groupInfo.second.latestKeyGroupAuthor_       = ConfigurationManager::UNKNOWN_INFO;
							groupInfo.second.latestKeyGroupCreationTime_ = ConfigurationManager::UNKNOWN_TIME;
							groupInfo.second.latestKeyGroupTypeString_   = ConfigurationManager::GROUP_TYPE_NAME_UNKNOWN;
							groupInfo.second.latestKeyMemberMap_   	     = {};
						}
					}  // end group info loop
				else //multi-threading
				{
					int threadsLaunched = 0;
					int foundThreadIndex = 0;

					std::vector<std::shared_ptr<std::atomic<bool>>> threadDone;
					for(int i=0;i<numOfThreads;++i)
						threadDone.push_back(std::make_shared<std::atomic<bool>>(true));

					std::vector<std::shared_ptr<ots::GroupInfo>> sharedGroupInfoPtrs;

					for(auto& groupInfo : allGroupInfo_)
					{
						//make temporary group info for thread
						sharedGroupInfoPtrs.push_back(std::make_shared<ots::GroupInfo>());

						if(threadsLaunched >= numOfThreads)
						{
							//find availableThreadIndex
							foundThreadIndex = -1;
							while(foundThreadIndex == -1)
							{
								for(int i=0;i<numOfThreads;++i)
									if(*(threadDone[i]))
									{
										foundThreadIndex = i;
										break;
									}
								if(foundThreadIndex == -1)
								{
									__GEN_COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__ << "Waiting for available thread..." << __E__;
									usleep(10000);
								}
							} //end thread search loop
							threadsLaunched = numOfThreads - 1;
						}					
						__GEN_COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__ << "Starting thread... " << foundThreadIndex << " for " << 
							groupInfo.first << "(" << groupInfo.second.getLatestKey() << ")" << __E__;

						*(threadDone[foundThreadIndex]) = false;

						std::thread([](
							ConfigurationManagerRW* 				theCfgMgr, 
							std::string 							theGroupName, 
							ots::TableGroupKey						theGroupKey,
							std::shared_ptr<ots::GroupInfo>        	theGroupInfo,
		               		std::shared_ptr<std::atomic<bool>> 		theThreadDone) { 
						ConfigurationManagerRW::loadTableGroupThread(theCfgMgr, theGroupName, theGroupKey, theGroupInfo, theThreadDone); },
							this,
							groupInfo.first,
							groupInfo.second.getLatestKey(),
							sharedGroupInfoPtrs.back(),
							threadDone[foundThreadIndex])
		    			.detach();

						++threadsLaunched;
						++foundThreadIndex;
					} //end groupInfo thread loop

					//check for all threads done					
					do
					{
						foundThreadIndex = -1;
						for(int i=0;i<numOfThreads;++i)
							if(!*(threadDone[i]))
							{
								foundThreadIndex = i;
								break;
							}
						if(foundThreadIndex != -1)
						{
							__GEN_COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__ << "Waiting for thread to finish... " << foundThreadIndex << __E__;
							usleep(10000);
						}
					} while(foundThreadIndex != -1); //end thread done search loop

					//threads done now, so copy group info
					size_t i = 0;
					for(auto& groupInfo : allGroupInfo_)
					{
						groupInfo.second.latestKeyGroupComment_      = sharedGroupInfoPtrs[i]->latestKeyGroupComment_;
						groupInfo.second.latestKeyGroupAuthor_       = sharedGroupInfoPtrs[i]->latestKeyGroupAuthor_;
						groupInfo.second.latestKeyGroupCreationTime_ = sharedGroupInfoPtrs[i]->latestKeyGroupCreationTime_;
						groupInfo.second.latestKeyGroupTypeString_   = sharedGroupInfoPtrs[i]->latestKeyGroupTypeString_;
						groupInfo.second.latestKeyMemberMap_   	     = sharedGroupInfoPtrs[i]->latestKeyMemberMap_;
						++i;
					} //end copy group info loop

				} //end multi-thread handling
			}
		}      // end get group info
		catch(const std::runtime_error& e)
		{
			__SS__ << "A fatal error occurred reading the info for all table groups. Error: " << e.what() << __E__;
			__GEN_COUT_ERR__ << "\n" << ss.str();
			if(accumulatedWarnings)
				*accumulatedWarnings += ss.str();
			else
				throw;
		}
		catch(...)
		{
			__SS__ << "An unknown fatal error occurred reading the info for all table groups." << __E__;
			try	{ throw; } //one more try to printout extra info
			catch(const std::exception &e)
			{
				ss << "Exception message: " << e.what();
			}
			catch(...){}
			__GEN_COUT_ERR__ << "\n" << ss.str();
			if(accumulatedWarnings)
				*accumulatedWarnings += ss.str();
			else
				throw;
		}
		__GEN_COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__ << "Group Info end runtime=" << runTimeSeconds() << __E__;
	} //end getGroupInfo
	else
		__GEN_COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__ << "Table Info end runtime=" << runTimeSeconds() << __E__;

	return allTableInfo_;
}  // end getAllTableInfo()
	
//==============================================================================
// loadTableGroupThread()
void ConfigurationManagerRW::loadTableGroupThread(ConfigurationManagerRW* 				cfgMgr, 
													std::string 						groupName, 
													ots::TableGroupKey					groupKey,
													std::shared_ptr<ots::GroupInfo>		groupInfo, 
													std::shared_ptr<std::atomic<bool>> 	threadDone)
try
{
	__COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__ << "Thread started... " << 
		groupName << "(" << groupKey << ")" << __E__;

	cfgMgr->loadTableGroup(groupName/*groupName*/,
		groupKey, //groupInfo->getLatestKey(),
		false /*doActivate*/,
		&(groupInfo->latestKeyMemberMap_) /*groupMembers*/,
		0 /*progressBar*/,
		0 /*accumulateErrors*/,
		&(groupInfo->latestKeyGroupComment_),
		&(groupInfo->latestKeyGroupAuthor_),
		&(groupInfo->latestKeyGroupCreationTime_),
		true /*doNotLoadMember*/,
		&(groupInfo->latestKeyGroupTypeString_));

	*(threadDone) = true;
} // end loadTableGroupThread
catch(...)
{
	__COUT_WARN__ << "Error occurred loading latest group info into cache for '"
		<< groupName << "(" << groupInfo->getLatestKey() << ")'..." << __E__;
	groupInfo->latestKeyGroupComment_      = ConfigurationManager::UNKNOWN_INFO;
	groupInfo->latestKeyGroupAuthor_       = ConfigurationManager::UNKNOWN_INFO;
	groupInfo->latestKeyGroupCreationTime_ = ConfigurationManager::UNKNOWN_TIME;
	groupInfo->latestKeyGroupTypeString_   = ConfigurationManager::GROUP_TYPE_NAME_UNKNOWN;
	groupInfo->latestKeyMemberMap_   	   = {};
	*(threadDone) = true;
} // end loadTableGroupThread catch


//==============================================================================
// compareTableGroupThread()
void ConfigurationManagerRW::compareTableGroupThread(ConfigurationManagerRW* 				cfgMgr, 
													std::string 							groupName, 
													ots::TableGroupKey 						groupKeyToCompare, 
													const std::map<std::string, TableVersion>& groupMemberMap, 
													const std::map<std::string /*name*/, std::string /*alias*/>& memberTableAliases,			
													std::atomic<bool>* 						foundIdentical,
													ots::TableGroupKey* 					identicalKey,			
													std::mutex* 							threadMutex,	
													std::shared_ptr<std::atomic<bool>> 		threadDone)
try
{
	std::map<std::string /*name*/, TableVersion /*version*/> compareToMemberMap;
	std::map<std::string /*name*/, std::string /*alias*/>    compareToMemberTableAliases;
	std::map<std::string /*name*/, std::string /*alias*/>* 	 compareToMemberTableAliasesPtr = nullptr;
	if(memberTableAliases.size()) //only give pointer if necessary, without will load group faster
		compareToMemberTableAliasesPtr = &compareToMemberTableAliases;

	cfgMgr->loadTableGroup(
		groupName,
		groupKeyToCompare,
		false /*doActivate*/,
		&compareToMemberMap /*memberMap*/,
		0, /*progressBar*/
		0, /*accumulatedWarnings*/
		0, /*groupComment*/
		0,
		0, /*null pointers*/
		true /*doNotLoadMember*/,
		0 /*groupTypeString*/,
		compareToMemberTableAliasesPtr);
	
	bool isDifferent = false;
	for(auto& memberPair : groupMemberMap)
	{
		if(memberTableAliases.find(memberPair.first) != memberTableAliases.end())
		{
			// handle this table as alias, not version
			if(compareToMemberTableAliases.find(memberPair.first) == compareToMemberTableAliases.end() ||  // alias is missing
			memberTableAliases.at(memberPair.first) != compareToMemberTableAliases.at(memberPair.first))
			{  // then different
				isDifferent = true;
				break;
			}
			else
				continue;
		}  // else check if compareTo group is using an alias for table
		else if(compareToMemberTableAliases.find(memberPair.first) != compareToMemberTableAliases.end())
		{
			// then different
			isDifferent = true;
			break;

		}                                                                                 // else handle as table version comparison
		else if(compareToMemberMap.find(memberPair.first) == compareToMemberMap.end() ||  // name is missing
				memberPair.second != compareToMemberMap.at(memberPair.first))             // or version mismatch
		{                                                                                
			// then different
			isDifferent = true;
			break;
		}
	}

	// check member size for exact match
	if(!isDifferent && groupMemberMap.size() != compareToMemberMap.size())
		isDifferent = true;  // different size, so not same (groupMemberMap is a subset of memberPairs)

	if(!isDifferent) //found an exact match!
	{
		*foundIdentical = true;
		__COUT__ << "=====> Found exact match with key: " << groupKeyToCompare << __E__;	
		
		std::lock_guard<std::mutex> lock(*threadMutex);	
		*identicalKey = groupKeyToCompare;
	}
	
	*(threadDone) = true;
} // end compareTableGroupThread
catch(...)
{
	__COUT_WARN__ << "Error occurred comparing group '"
		<< groupName << "(" << groupKeyToCompare << ")'..." << __E__;
	
	*(threadDone) = true;
} // end compareTableGroupThread catch

//==============================================================================
// getVersionAliases()
//	get version aliases organized by table, for currently active backbone tables
//	add scratch versions to the alias map returned by ConfigurationManager
std::map<std::string /*table name*/, std::map<std::string /*version alias*/, TableVersion /*aliased version*/>> ConfigurationManagerRW::getVersionAliases(
    void) const
{
	std::map<std::string /*table name*/, std::map<std::string /*version alias*/, TableVersion /*aliased version*/>> retMap =
	    ConfigurationManager::getVersionAliases();

	// always have scratch alias for each table that has a scratch version
	//	overwrite map entry if necessary
	if(!ConfigurationInterface::isVersionTrackingEnabled())
		for(const auto& tableInfo : allTableInfo_)
			for(const auto& version : tableInfo.second.versions_)
				if(version.isScratchVersion())
					retMap[tableInfo.first][ConfigurationManager::SCRATCH_VERSION_ALIAS] = TableVersion(TableVersion::SCRATCH);

	return retMap;
}  // end getVersionAliases()

//==============================================================================
// setActiveGlobalConfiguration
//	load table group and activate
//	deactivates previous table group of same type if necessary
void ConfigurationManagerRW::activateTableGroup(const std::string& tableGroupName, TableGroupKey tableGroupKey, 
	std::string* accumulatedTreeErrors, std::string* groupTypeString)
{
	try
	{
		loadTableGroup(
				tableGroupName,
				tableGroupKey,
				true,                    // loads and activates
				0,                       // no members needed
				0,                       // no progress bar
				accumulatedTreeErrors,  // accumulate warnings or not
				0 /* groupComment */,
				0 /* groupAuthor */,
				0 /* groupCreateTime */,
				false /* doNotLoadMember */,
				groupTypeString);
	}
	catch(...)
	{
		__GEN_COUT_ERR__ << "There were errors, so de-activating group: " << tableGroupName << " (" << tableGroupKey << ")" << __E__;
		try  // just in case any lingering pieces, let's deactivate
		{
			destroyTableGroup(tableGroupName, true);
		}
		catch(...)
		{
		}
		throw;  // re-throw original exception
	}

	__GEN_COUT_INFO__ << "Updating persistent active groups to " << ConfigurationManager::ACTIVE_GROUPS_FILENAME << " ..." << __E__;

	

	__MCOUT_INFO__("Active Context table group: " << theContextTableGroup_ << "("
	                                              << (theContextTableGroupKey_ ? theContextTableGroupKey_->toString().c_str() : "-1") << ")" << __E__);
	__MCOUT_INFO__("Active Backbone table group: " << theBackboneTableGroup_ << "("
	                                               << (theBackboneTableGroupKey_ ? theBackboneTableGroupKey_->toString().c_str() : "-1") << ")" << __E__);
	__MCOUT_INFO__("Active Iterate table group: " << theIterateTableGroup_ << "("
	                                              << (theIterateTableGroupKey_ ? theIterateTableGroupKey_->toString().c_str() : "-1") << ")" << __E__);
	__MCOUT_INFO__("Active Configuration table group: " << theConfigurationTableGroup_ << "("
	                                                    << (theConfigurationTableGroupKey_ ? theConfigurationTableGroupKey_->toString().c_str() : "-1") << ")"
	                                                    << __E__);

	std::string fn = ConfigurationManager::ACTIVE_GROUPS_FILENAME;
	FILE*       fp = fopen(fn.c_str(), "w");
	if(!fp)
	{
		__SS__ << "Fatal Error! Unable to open the file " << ConfigurationManager::ACTIVE_GROUPS_FILENAME << " for editing! Is there a permissions problem?"
		       << __E__;
		__GEN_COUT_ERR__ << ss.str();
		__SS_THROW__;
		return;
	}
	fprintf(fp, "%s\n", theContextTableGroup_.c_str());
	fprintf(fp, "%s\n", theContextTableGroupKey_ ? theContextTableGroupKey_->toString().c_str() : "-1");
	fprintf(fp, "%s\n", theBackboneTableGroup_.c_str());
	fprintf(fp, "%s\n", theBackboneTableGroupKey_ ? theBackboneTableGroupKey_->toString().c_str() : "-1");
	fprintf(fp, "%s\n", theIterateTableGroup_.c_str());
	fprintf(fp, "%s\n", theIterateTableGroupKey_ ? theIterateTableGroupKey_->toString().c_str() : "-1");
	fprintf(fp, "%s\n", theConfigurationTableGroup_.c_str());
	fprintf(fp, "%s\n", theConfigurationTableGroupKey_ ? theConfigurationTableGroupKey_->toString().c_str() : "-1");
	fclose(fp);

	// save last activated group
	{
		std::pair<std::string /*group name*/, TableGroupKey> activatedGroup(std::string(tableGroupName), tableGroupKey);
		if(theConfigurationTableGroupKey_ && theConfigurationTableGroup_ == tableGroupName && *theConfigurationTableGroupKey_ == tableGroupKey)
			ConfigurationManager::saveGroupNameAndKey(activatedGroup, LAST_ACTIVATED_CONFIG_GROUP_FILE);
		else if(theContextTableGroupKey_ && theContextTableGroup_ == tableGroupName && *theContextTableGroupKey_ == tableGroupKey)
			ConfigurationManager::saveGroupNameAndKey(activatedGroup, LAST_ACTIVATED_CONTEXT_GROUP_FILE);
		else if(theBackboneTableGroupKey_ && theBackboneTableGroup_ == tableGroupName && *theBackboneTableGroupKey_ == tableGroupKey)
			ConfigurationManager::saveGroupNameAndKey(activatedGroup, LAST_ACTIVATED_BACKBONE_GROUP_FILE);
		else if(theIterateTableGroupKey_ && theIterateTableGroup_ == tableGroupName && *theIterateTableGroupKey_ == tableGroupKey)
			ConfigurationManager::saveGroupNameAndKey(activatedGroup, LAST_ACTIVATED_ITERATOR_GROUP_FILE);
	}  // end save last activated group

}  // end activateTableGroup()

//==============================================================================
// createTemporaryBackboneView
//	sourceViewVersion of INVALID is from MockUp, else from valid view version
// 	returns temporary version number (which is always negative)
TableVersion ConfigurationManagerRW::createTemporaryBackboneView(TableVersion sourceViewVersion)
{
	__GEN_COUT_INFO__ << "Creating temporary backbone view from version " << sourceViewVersion << __E__;

	// find common available temporary version among backbone members
	TableVersion tmpVersion = TableVersion::getNextTemporaryVersion();  // get the default temporary version
	TableVersion retTmpVersion;
	auto         backboneMemberNames = ConfigurationManager::getBackboneMemberNames();
	for(auto& name : backboneMemberNames)
	{
		retTmpVersion = ConfigurationManager::getTableByName(name)->getNextTemporaryVersion();
		if(retTmpVersion < tmpVersion)
			tmpVersion = retTmpVersion;
	}

	__GEN_COUT__ << "Common temporary backbone version found as " << tmpVersion << __E__;

	// create temporary views from source version to destination temporary version
	for(auto& name : backboneMemberNames)
	{
		retTmpVersion = getTableByName(name)->createTemporaryView(sourceViewVersion, tmpVersion);
		if(retTmpVersion != tmpVersion)
		{
			__SS__ << "Failure! Temporary view requested was " << tmpVersion << ". Mismatched temporary view created: " << retTmpVersion << __E__;
			__GEN_COUT_ERR__ << ss.str();
			__SS_THROW__;
		}
	}

	return tmpVersion;
}  // end createTemporaryBackboneView()

//==============================================================================
TableBase* ConfigurationManagerRW::getTableByName(const std::string& tableName)
{
	if(nameToTableMap_.find(tableName) == nameToTableMap_.end())
	{
		if(tableName == ConfigurationManager::ARTDAQ_TOP_TABLE_NAME)
		{
			__GEN_COUT_WARN__ << "Since target table was the artdaq top configuration level, "
			                     "attempting to help user by appending to core tables file: "
			                  << CORE_TABLE_INFO_FILENAME << __E__;
			FILE* fp = fopen((CORE_TABLE_INFO_FILENAME).c_str(), "a");
			if(fp)
			{
				fprintf(fp, "\nARTDAQ/*");
				fclose(fp);
			}
		}

		__SS__ << "Table not found with name: " << tableName << __E__;
		size_t f;
		if((f = tableName.find(' ')) != std::string::npos)
			ss << "There was a space character found in the table name needle at "
			      "position "
			   << f << " in the string (was this intended?). " << __E__;

		ss << "\nIf you think this table should exist in the core set of tables, try running 'UpdateOTS.sh --tables' to update your tables, then relaunch ots."
		   << __E__;
		ss << "\nTables must be defined in $USER_DATA/TableInfo to exist in ots. Please verify your table definitions, and then restart ots." << __E__;
		__GEN_COUT_ERR__ << "\n" << ss.str();
		__SS_THROW__;
	}
	return nameToTableMap_[tableName];
}  // end getTableByName()

//==============================================================================
// getVersionedTableByName
//	Used by table GUI to load a particular table-version pair as the active version.
// 	This table instance must already exist and be owned by ConfigurationManager.
//	return null pointer on failure, on success return table pointer.
TableBase* ConfigurationManagerRW::getVersionedTableByName(const std::string& tableName,
                                                           TableVersion       version,
                                                           bool               looseColumnMatching /* = false */,
                                                           std::string*       accumulatedErrors /* = 0 */,
                                                           bool               getRawData /* = false */)
{
	auto it = nameToTableMap_.find(tableName);
	if(it == nameToTableMap_.end())
	{
		__SS__ << "\nCan not find table named '" << tableName << "'\n\n\n\nYou need to load the table before it can be used."
		       << "It probably is missing from the member list of the Table "
		          "Group that was loaded?\n\n\n\n\n"
		       << __E__;
		__SS_THROW__;
	}
	TableBase* table = it->second;

	if(version.isTemporaryVersion())
	{
		table->setActiveView(version);

		if(getRawData)
		{
			std::stringstream jsonSs;
			table->getViewP()->printJSON(jsonSs);
			table->getViewP()->doGetSourceRawData(true);
			table->getViewP()->fillFromJSON(jsonSs.str());
		}
	}
	else
	{
		theInterface_->get(table,
		                   tableName,
		                   0 /* groupKey */,
		                   0 /* groupName */,
		                   false /* dontFill */,  // false to fill w/version
		                   version,
		                   false /* resetConfiguration*/,  // false to not reset
		                   looseColumnMatching,
		                   getRawData,
		                   accumulatedErrors);
	}
	return table;
}  // end getVersionedTableByName()

//==============================================================================
// saveNewTable
//	saves version, makes the new version the active version, and returns new version
TableVersion ConfigurationManagerRW::saveNewTable(const std::string& tableName, TableVersion temporaryVersion,
                                                  bool makeTemporary)  //,
// bool saveToScratchVersion)
{
	TableVersion newVersion(temporaryVersion);

	// set author of version
	TableBase* table = getTableByName(tableName);
	table->getTemporaryView(temporaryVersion)->setAuthor(username_);
	// NOTE: author is assigned to permanent versions when saved to DBI

	if(!makeTemporary)  // saveNewVersion makes the new version the active version
		newVersion = theInterface_->saveNewVersion(table, temporaryVersion);
	else  // make the temporary version active
		table->setActiveView(newVersion);

	// if there is a problem, try to recover
	while(!makeTemporary && !newVersion.isScratchVersion() && allTableInfo_[tableName].versions_.find(newVersion) != allTableInfo_[tableName].versions_.end())
	{
		__GEN_COUT_ERR__ << "What happenened!?? ERROR::: new persistent version v" << newVersion
		                 << " already exists!? How is it possible? Retrace your steps and "
		                    "tell an admin."
		                 << __E__;

		// create a new temporary version of the target view
		temporaryVersion = table->createTemporaryView(newVersion);

		if(newVersion.isTemporaryVersion())
			newVersion = temporaryVersion;
		else
			newVersion = TableVersion::getNextVersion(newVersion);

		__GEN_COUT_WARN__ << "Attempting to recover and use v" << newVersion << __E__;

		if(!makeTemporary)  // saveNewVersion makes the new version the active version
			newVersion = theInterface_->saveNewVersion(table, temporaryVersion, newVersion);
		else  // make the temporary version active
			table->setActiveView(newVersion);
	}

	if(newVersion.isInvalid())
	{
		__SS__ << "Something went wrong saving the new version v" << newVersion << ". What happened?! (duplicates? database error?)" << __E__;
		__GEN_COUT_ERR__ << "\n" << ss.str();
		__SS_THROW__;
	}

	// update allTableInfo_ with the new version
	allTableInfo_[tableName].versions_.insert(newVersion);

	// table->getView().print();
	return newVersion;
}  // end saveNewTable()

//==============================================================================
// eraseTemporaryVersion
//	if version is invalid then erases ALL temporary versions
//
//	maintains allTableInfo_ also while erasing
void ConfigurationManagerRW::eraseTemporaryVersion(const std::string& tableName, TableVersion targetVersion)
{
	TableBase* table = getTableByName(tableName);

	table->trimTemporary(targetVersion);

	// if allTableInfo_ is not setup, then done
	if(allTableInfo_.find(tableName) == allTableInfo_.end())
		return;
	// else cleanup table info

	if(targetVersion.isInvalid())
	{
		// erase all temporary versions!
		for(auto it = allTableInfo_[tableName].versions_.begin(); it != allTableInfo_[tableName].versions_.end();
		    /*no increment*/)
		{
			if(it->isTemporaryVersion())
			{
				__GEN_COUT__ << "Removing '" << tableName << "' version info: " << *it << __E__;
				allTableInfo_[tableName].versions_.erase(it++);
			}
			else
				++it;
		}
	}
	else  // erase target version only
	{
		//__GEN_COUT__ << "Removing '" << tableName << "' version info: " << targetVersion << __E__;
		auto it = allTableInfo_[tableName].versions_.find(targetVersion);
		if(it == allTableInfo_[tableName].versions_.end())
		{
			__GEN_COUT__ << "Target '" << tableName << "' version v" << targetVersion << " was not found in info versions..." << __E__;
			return;
		}
		allTableInfo_[tableName].versions_.erase(allTableInfo_[tableName].versions_.find(targetVersion));
	}
}  // end eraseTemporaryVersion()

//==============================================================================
// clearCachedVersions
//	clear ALL cached persistent versions (does not erase temporary versions)
//
//	maintains allTableInfo_ also while erasing (trivial, do nothing)
void ConfigurationManagerRW::clearCachedVersions(const std::string& tableName)
{
	TableBase* table = getTableByName(tableName);

	table->trimCache(0);
}  // end clearCachedVersions()

//==============================================================================
// clearAllCachedVersions
//	clear ALL cached persistent versions (does not erase temporary versions)
//
//	maintains allTableInfo_ also while erasing (trivial, do nothing)
void ConfigurationManagerRW::clearAllCachedVersions()
{
	for(auto configInfo : allTableInfo_)
		configInfo.second.tablePtr_->trimCache(0);
}  // end clearAllCachedVersions()

//==============================================================================
// copyViewToCurrentColumns
TableVersion ConfigurationManagerRW::copyViewToCurrentColumns(const std::string& tableName, TableVersion sourceVersion)
{
	getTableByName(tableName)->reset();

	// make sure source version is loaded
	// need to load with loose column rules!
	TableBase* table = getVersionedTableByName(tableName, TableVersion(sourceVersion), true);

	// copy from source version to a new temporary version
	TableVersion newTemporaryVersion = table->copyView(table->getView(), TableVersion(), username_);

	// update allTableInfo_ with the new version
	allTableInfo_[tableName].versions_.insert(newTemporaryVersion);

	return newTemporaryVersion;
}  // end copyViewToCurrentColumns()

//==============================================================================
// cacheGroupKey
void ConfigurationManagerRW::cacheGroupKey(const std::string& groupName, TableGroupKey key)
{
	allGroupInfo_[groupName].keys_.emplace(key);

	//	__SS__ << "Now keys are: " << __E__;
	//	for(auto& key:allGroupInfo_[groupName].keys_)
	//		ss << "\t" << key << __E__;
	//	__GEN_COUT__ << ss.str() << __E__;
}  // end cacheGroupKey()

//==============================================================================
// getGroupInfo
//	the interface is slow when there are a lot of groups..
//	so plan is to maintain local cache of recent group info
const GroupInfo& ConfigurationManagerRW::getGroupInfo(const std::string& groupName)
{
	//	//NOTE: seems like this filter is taking the long amount of time
	//	std::set<std::string /*name*/> fullGroupNames =
	//			theInterface_->getAllTableGroupNames(groupName); //db filter by
	// group  name

	// so instead caching ourselves...
	auto it = allGroupInfo_.find(groupName);
	if(it == allGroupInfo_.end())
	{
		__SS__ << "Group name '" << groupName << "' not found in group info! (creating empty info)" << __E__;
		__GEN_COUT_WARN__ << ss.str();
		//__SS_THROW__;
		return allGroupInfo_[groupName];
	}
	return it->second;
}  // end getGroupInfo()

//==============================================================================
// findTableGroup
//	return group with same name and same members and same aliases
//	else return invalid key
//
// Note: if aliases, then member alias is matched (not member
//
// Note: this is taking too long when there are a ton of groups.
//	Change to going back only a limited number.. (but the order also comes in alpha order
// from 	theInterface_->getAllTableGroupNames which is a problem for choosing
// the 	most recent to check. )
TableGroupKey ConfigurationManagerRW::findTableGroup(const std::string&                                           groupName,
                                                     const std::map<std::string, TableVersion>&                   groupMemberMap,
                                                     const std::map<std::string /*name*/, std::string /*alias*/>& memberTableAliases)
{
	//	//NOTE: seems like this filter is taking the long amount of time
	//	std::set<std::string /*name*/> fullGroupNames =
	//			theInterface_->getAllTableGroupNames(groupName); //db filter by
	// group  name
	const GroupInfo& groupInfo = getGroupInfo(groupName);

	// std::string name;
	// TableGroupKey key;

	const unsigned int MAX_DEPTH_TO_CHECK = 20;
	unsigned int       keyMinToCheck      = 0;

	if(groupInfo.keys_.size())
		keyMinToCheck = groupInfo.keys_.rbegin()->key();
	if(keyMinToCheck > MAX_DEPTH_TO_CHECK)
	{
		keyMinToCheck -= MAX_DEPTH_TO_CHECK;
		__GEN_COUT__ << "Checking groups back to key... " << keyMinToCheck << __E__;
	}
	else
	{
		keyMinToCheck = 0;
		__GEN_COUT__ << "Checking all groups." << __E__;
	}

	// have min key to check, now loop through and check groups
	
	const int numOfThreads = PROCESSOR_COUNT/2;
	__GEN_COUT__ << " PROCESSOR_COUNT " << PROCESSOR_COUNT << " ==> " << numOfThreads << " threads." << __E__;
	if(numOfThreads < 2) // no multi-threading
	{
		std::map<std::string /*name*/, TableVersion /*version*/> compareToMemberMap;
		std::map<std::string /*name*/, std::string /*alias*/>    compareToMemberTableAliases;
		std::map<std::string /*name*/, std::string /*alias*/>* 	 compareToMemberTableAliasesPtr = nullptr;
		if(memberTableAliases.size())
			compareToMemberTableAliasesPtr = &compareToMemberTableAliases;

		bool isDifferent;
		for(const auto& key : groupInfo.keys_)
		{
			if(key.key() < keyMinToCheck)
				continue;  // skip keys that are too old

			loadTableGroup(groupName,
						key,
						false /*doActivate*/,
						&compareToMemberMap /*memberMap*/,
						0, /*progressBar*/
						0, /*accumulatedWarnings*/
						0, /*groupComment*/
						0, /*groupAuthor*/
						0, /*groupCreateTime*/
						true /*doNotLoadMember*/,
						0 /*groupTypeString*/,
						compareToMemberTableAliasesPtr);

			isDifferent = false;
			for(auto& memberPair : groupMemberMap)
			{
				if(memberTableAliases.find(memberPair.first) != memberTableAliases.end())
				{
					// handle this table as alias, not version
					if(compareToMemberTableAliases.find(memberPair.first) == compareToMemberTableAliases.end() ||  // alias is missing
					memberTableAliases.at(memberPair.first) != compareToMemberTableAliases.at(memberPair.first))
					{  // then different
						isDifferent = true;
						break;
					}
					else
						continue;
				}  // else check if compareTo group is using an alias for table
				else if(compareToMemberTableAliases.find(memberPair.first) != compareToMemberTableAliases.end())
				{
					// then different
					isDifferent = true;
					break;

				}                                                                                 // else handle as table version comparison
				else if(compareToMemberMap.find(memberPair.first) == compareToMemberMap.end() ||  // name is missing
						memberPair.second != compareToMemberMap.at(memberPair.first))             // or version mismatch
				{                                                                                
					// then different
					isDifferent = true;
					break;
				}
			}
			if(isDifferent)
				continue;

			// check member size for exact match
			if(groupMemberMap.size() != compareToMemberMap.size())
				continue;  // different size, so not same (groupMemberMap is a subset of
						// memberPairs)

			__GEN_COUT__ << "Found exact match with key: " << key << __E__;
			// else found an exact match!
			return key;
		}
		__GEN_COUT__ << "No match found - this group is new!" << __E__;
		// if here, then no match found
		return TableGroupKey();  // return invalid key
	}
	else //multi-threading
	{
		int threadsLaunched = 0;
		int foundThreadIndex = 0;
		std::atomic<bool> foundIdentical = false;
		ots::TableGroupKey identicalKey;
		std::mutex threadMutex;

		std::vector<std::shared_ptr<std::atomic<bool>>> threadDone;
		for(int i=0;i<numOfThreads;++i)
			threadDone.push_back(std::make_shared<std::atomic<bool>>(true));
		
		for(const auto& key : groupInfo.keys_)
		{
			if(foundIdentical) break;
			if(key.key() < keyMinToCheck)
				continue;  // skip keys that are too old
				
			if(threadsLaunched >= numOfThreads)
			{
				//find availableThreadIndex
				foundThreadIndex = -1;
				while(foundThreadIndex == -1)
				{
					if(foundIdentical) break;

					for(int i=0;i<numOfThreads;++i)
						if(*(threadDone[i]))
						{
							foundThreadIndex = i;
							break;
						}
					if(foundThreadIndex == -1)
					{
						__GEN_COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__ << "Waiting for available thread..." << __E__;
						usleep(10000);
					}
				} //end thread search loop
				threadsLaunched = numOfThreads - 1;
			}					
			if(foundIdentical) break;

			__GEN_COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__ << "Starting thread... " << foundThreadIndex << __E__;
			*(threadDone[foundThreadIndex]) = false;

			std::thread([](
				ConfigurationManagerRW* 				cfgMgr, 
				std::string 							theGroupName, 
				ots::TableGroupKey						groupKeyToCompare,
				const std::map<std::string, TableVersion>&                   groupMemberMap,
				const std::map<std::string /*name*/, std::string /*alias*/>& memberTableAliases,
				std::atomic<bool>* 						theFoundIdentical,
				ots::TableGroupKey* 					theIdenticalKey,	
				std::mutex* 							theThreadMutex,							
				std::shared_ptr<std::atomic<bool>> 		theThreadDone) { 
			ConfigurationManagerRW::compareTableGroupThread(cfgMgr, theGroupName, groupKeyToCompare, groupMemberMap, memberTableAliases, 
				theFoundIdentical, theIdenticalKey, theThreadMutex, 
							theThreadDone); },
				this,
				groupName,
				key,
				groupMemberMap,
				memberTableAliases,
				&foundIdentical,
				&identicalKey,
				&threadMutex,
				threadDone[foundThreadIndex])
			.detach();

			++threadsLaunched;
			++foundThreadIndex;
		} //end groupInfo thread loop

		//check for all threads done					
		do
		{
			foundThreadIndex = -1;
			for(int i=0;i<numOfThreads;++i)
				if(!*(threadDone[i]))
				{
					foundThreadIndex = i;
					break;
				}
			if(foundThreadIndex != -1)
			{
				__GEN_COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__ << "Waiting for thread to finish... " << foundThreadIndex << __E__;
				usleep(10000);
			}
		} while(foundThreadIndex != -1); //end thread done search loop

		if(foundIdentical)
		{
			__GEN_COUT__ << "Found exact match with key: " << identicalKey << __E__;
			return identicalKey;
		}
		__GEN_COUT__ << "No match found - this group is new!" << __E__;
		// if here, then no match found
		return TableGroupKey();  // return invalid key
	} //end multi-thread handling
} // end findTableGroup()

//==============================================================================
// saveNewTableGroup
//	saves new group and returns the new group key
//	if previousVersion is provided, attempts to just bump that version
//	else, bumps latest version found in db
//
//	Note: groupMembers map will get modified with group metadata table version
TableGroupKey ConfigurationManagerRW::saveNewTableGroup(const std::string&                                      groupName,
                                                        std::map<std::string, TableVersion>&                    groupMembers,
                                                        const std::string&                                      groupComment,
                                                        std::map<std::string /*table*/, std::string /*alias*/>* groupAliases)
{
	// steps:
	//	determine new group key
	//	verify group members
	//	verify groupNameWithKey
	//	verify store

	if(groupMembers.size() == 0)  // do not allow empty groups
	{
		__SS__ << "Empty group member list. Can not create a group without members!" << __E__;
		__SS_THROW__;
	}

	__GEN_COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__ << "saveNewTableGroup runtime=" << runTimeSeconds() << __E__;

	// determine new group key
	TableGroupKey newKey = TableGroupKey::getNextKey(theInterface_->findLatestGroupKey(groupName));

	__GEN_COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__ << "saveNewTableGroup runtime=" << runTimeSeconds() << __E__;

	__GEN_COUT__ << "New Key for group: " << groupName << " found as " << newKey << __E__;

	//	verify group members
	//		- use all table info
	std::map<std::string, TableInfo> allCfgInfo = getAllTableInfo();
	for(auto& memberPair : groupMembers)
	{
		// check member name
		if(allCfgInfo.find(memberPair.first) == allCfgInfo.end())
		{
			__GEN_COUT_ERR__ << "Group member \"" << memberPair.first << "\" not found in database!";

			if(groupMetadataTable_.getTableName() == memberPair.first)
			{
				__GEN_COUT_WARN__ << "Looks like this is the groupMetadataTable_ '" << ConfigurationInterface::GROUP_METADATA_TABLE_NAME
				                  << ".' Note that this table is added to the member map when groups "
				                     "are saved."
				                  << "It should not be part of member map when calling this function." << __E__;
				__GEN_COUT__ << "Attempting to recover." << __E__;
				groupMembers.erase(groupMembers.find(memberPair.first));
			}
			else
			{
				__SS__ << ("Group member not found!") << __E__;
				__SS_THROW__;
			}
		}
		// check member version
		if(allCfgInfo[memberPair.first].versions_.find(memberPair.second) == allCfgInfo[memberPair.first].versions_.end())
		{
			__SS__ << "Group member  \"" << memberPair.first << "\" version \"" << memberPair.second << "\" not found in database!";
			__SS_THROW__;
		}
	}  // end verify members

	__GEN_COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__ << "saveNewTableGroup runtime=" << runTimeSeconds() << __E__;

	// verify group aliases
	if(groupAliases)
	{
		for(auto& aliasPair : *groupAliases)
		{
			// check for alias table in member names
			if(groupMembers.find(aliasPair.first) == groupMembers.end())
			{
				__GEN_COUT_ERR__ << "Group member \"" << aliasPair.first << "\" not found in group member map!";

				__SS__ << ("Alias table not found in member list!") << __E__;
				__SS_THROW__;
			}
		}
	}  // end verify group aliases

	// verify groupNameWithKey and attempt to store
	try
	{
		// save meta data for group; reuse groupMetadataTable_
		std::string groupAliasesString = "";
		if(groupAliases)
			groupAliasesString = StringMacros::mapToString(*groupAliases, "," /*primary delimeter*/, ":" /*secondary delimeter*/);
		__GEN_COUT__ << "Metadata: " << username_ << " " << time(0) << " " << groupComment << " " << groupAliasesString << __E__;

		// to compensate for unusual errors upstream, make sure the metadata table has one
		// row
		while(groupMetadataTable_.getViewP()->getNumberOfRows() > 1)
			groupMetadataTable_.getViewP()->deleteRow(0);
		if(groupMetadataTable_.getViewP()->getNumberOfRows() == 0)
			groupMetadataTable_.getViewP()->addRow();

		// columns are uid,comment,author,time
		groupMetadataTable_.getViewP()->setValue(groupAliasesString, 0, ConfigurationManager::METADATA_COL_ALIASES);
		groupMetadataTable_.getViewP()->setValue(groupComment, 0, ConfigurationManager::METADATA_COL_COMMENT);
		groupMetadataTable_.getViewP()->setValue(username_, 0, ConfigurationManager::METADATA_COL_AUTHOR);
		groupMetadataTable_.getViewP()->setValue(time(0), 0, ConfigurationManager::METADATA_COL_TIMESTAMP);

		// set version to first available persistent version
		groupMetadataTable_.getViewP()->setVersion(TableVersion::getNextVersion(theInterface_->findLatestVersion(&groupMetadataTable_)));

		// groupMetadataTable_.print();

		theInterface_->saveActiveVersion(&groupMetadataTable_);

		__GEN_COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__ << "saveNewTableGroup runtime=" << runTimeSeconds() << __E__;

		// force groupMetadataTable_ to be a member for the group
		groupMembers[groupMetadataTable_.getTableName()] = groupMetadataTable_.getViewVersion();

		theInterface_->saveTableGroup(groupMembers, TableGroupKey::getFullGroupString(groupName, newKey));
		__GEN_COUT__ << "Created table group: " << groupName << ":" << newKey << __E__;
	}
	catch(std::runtime_error& e)
	{
		__GEN_COUT_ERR__ << "Failed to create table group: " << groupName << ":" << newKey << __E__;
		__GEN_COUT_ERR__ << "\n\n" << e.what() << __E__;
		throw;
	}
	catch(...)
	{
		__GEN_COUT_ERR__ << "Failed to create table group: " << groupName << ":" << newKey << __E__;
		throw;
	}

	__GEN_COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__ << "saveNewTableGroup runtime=" << runTimeSeconds() << __E__;

	// store cache of recent groups
	cacheGroupKey(groupName, newKey);

	// at this point succeeded!
	return newKey;
}  // end saveNewTableGroup()

//==============================================================================
// saveNewBackbone
//	makes the new version the active version and returns new version number
//	INVALID will give a new backbone from mockup
TableVersion ConfigurationManagerRW::saveNewBackbone(TableVersion temporaryVersion)
{
	__GEN_COUT_INFO__ << "Creating new backbone from temporary version " << temporaryVersion << __E__;

	// find common available temporary version among backbone members
	TableVersion newVersion(TableVersion::DEFAULT);
	TableVersion retNewVersion;
	auto         backboneMemberNames = ConfigurationManager::getBackboneMemberNames();
	for(auto& name : backboneMemberNames)
	{
		retNewVersion = ConfigurationManager::getTableByName(name)->getNextVersion();
		__GEN_COUT__ << "New version for backbone member (" << name << "): " << retNewVersion << __E__;
		if(retNewVersion > newVersion)
			newVersion = retNewVersion;
	}

	__GEN_COUT__ << "Common new backbone version found as " << newVersion << __E__;

	// create new views from source temporary version
	for(auto& name : backboneMemberNames)
	{
		// saveNewVersion makes the new version the active version
		retNewVersion = getConfigurationInterface()->saveNewVersion(getTableByName(name), temporaryVersion, newVersion);
		if(retNewVersion != newVersion)
		{
			__SS__ << "Failure! New view requested was " << newVersion << ". Mismatched new view created: " << retNewVersion << __E__;
			__GEN_COUT_ERR__ << ss.str();
			__SS_THROW__;
		}
	}

	return newVersion;
}  // end saveNewBackbone()

//==============================================================================
// saveModifiedVersionXML
//
// once source version has been modified in temporary version
//	this function finishes it off.
TableVersion ConfigurationManagerRW::saveModifiedVersion(const std::string& tableName,
                                                         TableVersion       originalVersion,
                                                         bool               makeTemporary,
                                                         TableBase*         table,
                                                         TableVersion       temporaryModifiedVersion,
                                                         bool               ignoreDuplicates /*= false*/,
                                                         bool               lookForEquivalent /*= false*/,
                                                         bool*              foundEquivalent /*= nullptr*/)
{
	bool needToEraseTemporarySource = (originalVersion.isTemporaryVersion() && !makeTemporary);

	if(foundEquivalent)
		*foundEquivalent = false;  // initialize

	// check for duplicate tables already in cache
	if(!ignoreDuplicates)
	{
		__GEN_COUT__ << "Checking for duplicate '" << tableName << "' tables..." << __E__;

		TableVersion duplicateVersion;

		{
			//"DEEP" checking
			//	load into cache 'recent' versions for this table
			//		'recent' := those already in cache, plus highest version numbers not
			// in  cache
			const std::map<std::string, TableInfo>& allTableInfo = getAllTableInfo();  // do not refresh

			auto versionReverseIterator = allTableInfo.at(tableName).versions_.rbegin();  // get reverse iterator
			__GEN_COUT__ << "Filling up '" << tableName << "' cache from " << table->getNumberOfStoredViews() << " to max count of "
			             << table->MAX_VIEWS_IN_CACHE << __E__;
			for(; table->getNumberOfStoredViews() < table->MAX_VIEWS_IN_CACHE && versionReverseIterator != allTableInfo.at(tableName).versions_.rend();
			    ++versionReverseIterator)
			{
				__GEN_COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__ << "'" << tableName << "' versions in reverse order " << *versionReverseIterator << __E__;
				try
				{
					getVersionedTableByName(tableName, *versionReverseIterator);  // load to cache
				}
				catch(const std::runtime_error& e)
				{
					// ignore error
				}
			}
		}

		__GEN_COUT__ << "Checking '" << tableName << "' duplicate..." << __E__;

		duplicateVersion =
		    table->checkForDuplicate(temporaryModifiedVersion,
		                             (!originalVersion.isTemporaryVersion() && !makeTemporary) ? TableVersion() :  // if from persistent to persistent, then
		                                                                                                           // include original version in search
		                                 originalVersion);

		if(lookForEquivalent && !duplicateVersion.isInvalid())
		{
			// found an equivalent!
			__GEN_COUT__ << "Equivalent '" << tableName << "' table found in version v" << duplicateVersion << __E__;

			// if duplicate version was temporary, do not use
			if(duplicateVersion.isTemporaryVersion() && !makeTemporary)
			{
				__GEN_COUT__ << "Need persistent. Duplicate '" << tableName
				             << "' version was temporary. "
				                "Abandoning duplicate."
				             << __E__;
				duplicateVersion = TableVersion();  // set invalid
			}
			else
			{
				// erase and return equivalent version

				// erase modified equivalent version
				eraseTemporaryVersion(tableName, temporaryModifiedVersion);

				// erase original if needed
				if(needToEraseTemporarySource)
					eraseTemporaryVersion(tableName, originalVersion);

				if(foundEquivalent)
					*foundEquivalent = true;

				__GEN_COUT__ << "\t\t Equivalent '" << tableName << "' assigned version: " << duplicateVersion << __E__;

				return duplicateVersion;
			}
		}

		if(!duplicateVersion.isInvalid())
		{
			__SS__ << "This version of table '" << tableName << "' is identical to another version currently cached v" << duplicateVersion
			       << ". No reason to save a duplicate." << __E__;
			__GEN_COUT_ERR__ << "\n" << ss.str();

			// delete temporaryModifiedVersion
			table->eraseView(temporaryModifiedVersion);
			__SS_THROW__;
		}

		__GEN_COUT__ << "Check for duplicate '" << tableName << "' tables complete." << __E__;
	}

	if(makeTemporary)
		__GEN_COUT__ << "\t\t**************************** Save as temporary '" << tableName << "' table version" << __E__;
	else
		__GEN_COUT__ << "\t\t**************************** Save as new '" << tableName << "' table version" << __E__;

	TableVersion newAssignedVersion = saveNewTable(tableName, temporaryModifiedVersion, makeTemporary);

	if(needToEraseTemporarySource)
		eraseTemporaryVersion(tableName, originalVersion);

	__GEN_COUT__ << "\t\t '" << tableName << "' new assigned version: " << newAssignedVersion << __E__;
	return newAssignedVersion;
}  // end saveModifiedVersion()

//==============================================================================
GroupEditStruct::GroupEditStruct(const ConfigurationManager::GroupType& groupType, ConfigurationManagerRW* cfgMgr)
    : groupType_(groupType)
    , originalGroupName_(cfgMgr->getActiveGroupName(groupType))
    , originalGroupKey_(cfgMgr->getActiveGroupKey(groupType))
    , cfgMgr_(cfgMgr)
    , mfSubject_(cfgMgr->getUsername())
{
	if(originalGroupName_ == "" || originalGroupKey_.isInvalid())
	{
		__SS__ << "Error! No active group found for type '" << ConfigurationManager::convertGroupTypeToName(groupType)
		       << ".' There must be an active group to edit the group." << __E__ << __E__ << StringMacros::stackTrace() << __E__;
		__SS_THROW__;
	}

	__GEN_COUT__ << "Extracting Group-Edit Struct for type " << ConfigurationManager::convertGroupTypeToName(groupType) << __E__;

	std::map<std::string, TableVersion> activeTables = cfgMgr->getActiveVersions();

	const std::set<std::string>& memberNames =
	    groupType == ConfigurationManager::GroupType::CONTEXT_TYPE
	        ? ConfigurationManager::getContextMemberNames()
	        : (groupType == ConfigurationManager::GroupType::BACKBONE_TYPE
	               ? ConfigurationManager::getBackboneMemberNames()
	               : (groupType == ConfigurationManager::GroupType::ITERATE_TYPE ? ConfigurationManager::getIterateMemberNames()
	                                                                             : cfgMgr->getConfigurationMemberNames()));

	for(auto& memberName : memberNames)
		try
		{
			groupMembers_.emplace(std::make_pair(memberName, activeTables.at(memberName)));
			groupTables_.emplace(std::make_pair(memberName, TableEditStruct(memberName, cfgMgr)));  // Table ready for editing!
		}
		catch(...)
		{
			__SS__ << "Error! Could not find group member table '" << memberName << "' for group type '"
			       << ConfigurationManager::convertGroupTypeToName(groupType) << ".' All group members must be present to create the group editing structure."
			       << __E__;
			__SS_THROW__;
		}

}  // end GroupEditStruct constructor()

//==============================================================================
GroupEditStruct::~GroupEditStruct()
{
	__GEN_COUT__ << "GroupEditStruct from editing '" << originalGroupName_ << "(" << originalGroupKey_ << ")' Destructing..." << __E__;
	dropChanges();
	__GEN_COUT__ << "GroupEditStruct from editing '" << originalGroupName_ << "(" << originalGroupKey_ << ")' Desctructed." << __E__;
}  // end GroupEditStruct destructor()

//==============================================================================
// Note: if markModified, and table not found in group, this function will try to add it to group
TableEditStruct& GroupEditStruct::getTableEditStruct(const std::string& tableName, bool markModified /*= false*/)
{
	auto it = groupTables_.find(tableName);
	if(it == groupTables_.end())
	{
		if(groupType_ == ConfigurationManager::GroupType::CONFIGURATION_TYPE && markModified)
		{
			__GEN_COUT__ << "Table '" << tableName << "' not found in configuration table members from editing '" << originalGroupName_ << "("
			             << originalGroupKey_ << ")..."
			             << " Attempting to add it!" << __E__;

			// emplace returns pair<object,bool wasAdded>
			auto newIt = groupTables_.emplace(std::make_pair(tableName, TableEditStruct(tableName, cfgMgr_)));  // Table ready for editing!
			if(newIt.second)
			{
				newIt.first->second.modified_ = markModified;  // could indicate 'dirty' immediately in user code, which will cause a save of table
				groupMembers_.emplace(std::make_pair(tableName, newIt.first->second.temporaryVersion_));
				return newIt.first->second;
			}
			__GEN_COUT_ERR__ << "Failed to emplace new table..." << __E__;
		}

		__SS__ << "Table '" << tableName << "' not found in table members from editing '" << originalGroupName_ << "(" << originalGroupKey_ << ")!'" << __E__;
		__SS_THROW__;
	}
	it->second.modified_ = markModified;  // could indicate 'dirty' immediately in user code, which will cause a save of table
	return it->second;
}  // end getTableEditStruct()

//==============================================================================
void GroupEditStruct::dropChanges()
{
	__GEN_COUT__ << "Dropping unsaved changes from editing '" << originalGroupName_ << "(" << originalGroupKey_ << ")'..." << __E__;

	ConfigurationManagerRW* cfgMgr = cfgMgr_;

	// drop all temporary versions
	for(auto& groupTable : groupTables_)
		if(groupTable.second.createdTemporaryVersion_)  // if temporary version created here
		{
			// erase with proper version management
			cfgMgr->eraseTemporaryVersion(groupTable.second.tableName_, groupTable.second.temporaryVersion_);
			groupTable.second.createdTemporaryVersion_ = false;
			groupTable.second.modified_                = false;
		}

	__GEN_COUT__ << "Unsaved changes dropped from editing '" << originalGroupName_ << "(" << originalGroupKey_ << ").'" << __E__;
}  // end GroupEditStruct::dropChanges()

//==============================================================================
void GroupEditStruct::saveChanges(const std::string& groupNameToSave,
                                  TableGroupKey&     newGroupKey,
                                  bool*              foundEquivalentGroupKey /*= nullptr*/,
                                  bool               activateNewGroup /*= false*/,
                                  bool               updateGroupAliases /*= false*/,
                                  bool               updateTableAliases /*= false*/,
                                  TableGroupKey*     newBackboneKey /*= nullptr*/,
                                  bool*              foundEquivalentBackboneKey /*= nullptr*/,
                                  std::string*       accumulatedWarnings /*= nullptr*/)
{
	__GEN_COUT__ << "Saving changes..." << __E__;

	newGroupKey = TableGroupKey();  // invalidate reference parameter
	if(newBackboneKey)
		*newBackboneKey = TableGroupKey();  // invalidate reference parameter
	if(foundEquivalentBackboneKey)
		*foundEquivalentBackboneKey = false;  // clear to start
	ConfigurationManagerRW* cfgMgr = cfgMgr_;

	// save all temporary modified versions
	for(auto& groupTable : groupTables_)
	{
		if(!groupTable.second.modified_)
			continue;  // skip if not modified

		__GEN_COUT__ << "Original version is " << groupTable.second.tableName_ << "-v" << groupTable.second.originalVersion_ << __E__;

		groupMembers_.at(groupTable.first) =
		    cfgMgr->saveModifiedVersion(groupTable.second.tableName_,
		                                groupTable.second.originalVersion_,
		                                true /*make temporary*/,
		                                groupTable.second.table_,
		                                groupTable.second.temporaryVersion_,
		                                true /*ignoreDuplicates*/);  // make temporary version to save persistent version properly

		__GEN_COUT__ << "Temporary target version is " << groupTable.second.tableName_ << "-v" << groupMembers_.at(groupTable.first) << "-v"
		             << groupTable.second.temporaryVersion_ << __E__;

		groupMembers_.at(groupTable.first) = cfgMgr->saveModifiedVersion(groupTable.second.tableName_,
		                                                                 groupTable.second.originalVersion_,
		                                                                 false /*make temporary*/,
		                                                                 groupTable.second.table_,
		                                                                 groupTable.second.temporaryVersion_,
		                                                                 false /*ignoreDuplicates*/,
		                                                                 true /*lookForEquivalent*/);  // save persistent version properly

		__GEN_COUT__ << "Final target version is " << groupTable.second.tableName_ << "-v" << groupMembers_.at(groupTable.first) << __E__;

		groupTable.second.modified_                = false;  // clear modified flag
		groupTable.second.createdTemporaryVersion_ = false;  // modified version is gone
	}                                                        // loop through table edit structs

	for(auto& table : groupMembers_)
	{
		__GEN_COUT__ << table.first << " v" << table.second << __E__;
	}

	__GEN_COUT__ << "Checking for duplicate groups..." << __E__;
	newGroupKey = cfgMgr->findTableGroup(groupNameToSave, groupMembers_);

	if(!newGroupKey.isInvalid())
	{
		__GEN_COUT__ << "Found equivalent group key (" << newGroupKey << ") for " << groupNameToSave << "." << __E__;
		if(foundEquivalentGroupKey)
			*foundEquivalentGroupKey = true;
	}
	else
	{
		newGroupKey = cfgMgr->saveNewTableGroup(groupNameToSave, groupMembers_);
		__GEN_COUT__ << "Saved new Context group key (" << newGroupKey << ") for " << groupNameToSave << "." << __E__;
	}

	bool groupAliasChange = false;
	bool tableAliasChange = false;

	GroupEditStruct backboneGroupEdit(ConfigurationManager::GroupType::BACKBONE_TYPE, cfgMgr);

	if(groupType_ != ConfigurationManager::GroupType::BACKBONE_TYPE && updateGroupAliases)
	{
		// check group aliases ... a la
		// ConfigurationGUISupervisor::handleSetGroupAliasInBackboneXML

		TableEditStruct& groupAliasTable = backboneGroupEdit.getTableEditStruct(ConfigurationManager::GROUP_ALIASES_TABLE_NAME, true /*markModified*/);
		TableView*       tableView       = groupAliasTable.tableView_;

		// unsigned int col;
		unsigned int row = 0;

		std::vector<std::pair<std::string, ConfigurationTree>> aliasNodePairs = cfgMgr->getNode(ConfigurationManager::GROUP_ALIASES_TABLE_NAME).getChildren();
		std::string                                            groupName, groupKey;
		for(auto& aliasNodePair : aliasNodePairs)
		{
			groupName = aliasNodePair.second.getNode("GroupName").getValueAsString();
			groupKey  = aliasNodePair.second.getNode("GroupKey").getValueAsString();

			__GEN_COUT__ << "Group Alias: " << aliasNodePair.first << " => " << groupName << "(" << groupKey << "); row=" << row << __E__;

			if(groupName == originalGroupName_ && TableGroupKey(groupKey) == originalGroupKey_)
			{
				__GEN_COUT__ << "Found alias! Changing group key from (" << originalGroupKey_ << ") to (" << newGroupKey << ")" << __E__;

				groupAliasChange = true;

				tableView->setValueAsString(newGroupKey.toString(), row, tableView->findCol("GroupKey"));
			}

			++row;
		}

		if(groupAliasChange)
		{
			std::stringstream ss;
			tableView->print(ss);
			__GEN_COUT__ << ss.str();
			//
			////			// save or find equivalent
			////			backboneGroupEdit.groupMembers_.at(ConfigurationManager::GROUP_ALIASES_TABLE_NAME) =
			////					cfgMgr->saveModifiedVersion(
			////							groupAliasTable.tableName_,
			////							groupAliasTable.originalVersion_,
			////							false /*makeTemporary*/,
			////							groupAliasTable.table_,
			////							groupAliasTable.temporaryVersion_,
			////							false /*ignoreDuplicates*/,
			////							true /*lookForEquivalent*/);
			//
			//			backboneGroupEdit.groupMembers_.at(ConfigurationManager::GROUP_ALIASES_TABLE_NAME) =
			//					cfgMgr->saveModifiedVersion(
			//							groupAliasTable.tableName_,
			//							groupAliasTable.originalVersion_,
			//							true /*make temporary*/,
			//							groupAliasTable.table_,
			//							groupAliasTable.temporaryVersion_,
			//							true /*ignoreDuplicates*/);  // make temporary version to save persistent version properly
			//
			//			__GEN_COUT__ << "Temporary target version is " <<
			//					groupAliasTable.table_->getTableName() << "-v"
			//					<< backboneGroupEdit.groupMembers_.at(ConfigurationManager::GROUP_ALIASES_TABLE_NAME) << "-v"
			//					<< groupAliasTable.temporaryVersion_ << __E__;
			//
			//			backboneGroupEdit.groupMembers_.at(ConfigurationManager::GROUP_ALIASES_TABLE_NAME) =
			//					cfgMgr->saveModifiedVersion(
			//							groupAliasTable.tableName_,
			//							groupAliasTable.originalVersion_,
			//							false /*make temporary*/,
			//							groupAliasTable.table_,
			//							groupAliasTable.temporaryVersion_,
			//							false /*ignoreDuplicates*/,
			//							true /*lookForEquivalent*/);  // save persistent version properly
			//
			//			__GEN_COUT__
			//			    << "Original version is "
			//				<< groupAliasTable.table_->getTableName() << "-v"
			//			    << groupAliasTable.originalVersion_ << " and new version is v"
			//			    << backboneGroupEdit.groupMembers_.at(ConfigurationManager::GROUP_ALIASES_TABLE_NAME)
			//			    << __E__;
		}
	}  // end updateGroupAliases handling

	if(groupType_ != ConfigurationManager::GroupType::BACKBONE_TYPE && updateTableAliases)
	{
		// update all table version aliases
		TableView* tableView = backboneGroupEdit.getTableEditStruct(ConfigurationManager::VERSION_ALIASES_TABLE_NAME, true /*markModified*/).tableView_;

		for(auto& groupTable : groupTables_)
		{
			if(groupTable.second.originalVersion_ == groupMembers_.at(groupTable.second.tableName_))
				continue;  // skip if no change

			__GEN_COUT__ << "Checking alias... original version is " << groupTable.second.tableName_ << "-v" << groupTable.second.originalVersion_
			             << " and new version is v" << groupMembers_.at(groupTable.second.tableName_) << __E__;

			// unsigned int col;
			unsigned int row = 0;

			std::vector<std::pair<std::string, ConfigurationTree>> aliasNodePairs =
			    cfgMgr->getNode(ConfigurationManager::VERSION_ALIASES_TABLE_NAME).getChildren();
			std::string tableName, tableVersion;
			for(auto& aliasNodePair : aliasNodePairs)
			{
				tableName    = aliasNodePair.second.getNode("TableName").getValueAsString();
				tableVersion = aliasNodePair.second.getNode("Version").getValueAsString();

				__GEN_COUT__ << "Table Alias: " << aliasNodePair.first << " => " << tableName << "-v" << tableVersion << "" << __E__;

				if(tableName == groupTable.second.tableName_ && TableVersion(tableVersion) == groupTable.second.originalVersion_)
				{
					__GEN_COUT__ << "Found alias! Changing icon table version alias." << __E__;

					tableAliasChange = true;

					tableView->setValueAsString(groupMembers_.at(groupTable.second.tableName_).toString(), row, tableView->findCol("Version"));
				}

				++row;
			}
		}

		if(tableAliasChange)
		{
			std::stringstream ss;
			tableView->print(ss);
			__GEN_COUT__ << ss.str();
		}
	}  // end updateTableAliases handling

	//	if backbone modified, save group and activate it

	TableGroupKey localNewBackboneKey;
	if(groupAliasChange || tableAliasChange)
	{
		for(auto& table : backboneGroupEdit.groupMembers_)
		{
			__GEN_COUT__ << table.first << " v" << table.second << __E__;
		}
		backboneGroupEdit.saveChanges(
		    backboneGroupEdit.originalGroupName_, localNewBackboneKey, foundEquivalentBackboneKey ? foundEquivalentBackboneKey : nullptr);

		if(newBackboneKey)
			*newBackboneKey = localNewBackboneKey;
	}

	// acquire all active groups and ignore errors, so that activateTableGroup does not
	// erase other active groups
	{
		__GEN_COUT__ << "Restoring active table groups, before activating new groups..." << __E__;

		std::string localAccumulatedWarnings;
		cfgMgr->restoreActiveTableGroups(false /*throwErrors*/,
		                                 "" /*pathToActiveGroupsFile*/,
		                                 ConfigurationManager::LoadGroupType::ALL_TYPES /*onlyLoadIfBackboneOrContext*/,
		                                 &localAccumulatedWarnings);
	}

	// activate new groups
	if(!localNewBackboneKey.isInvalid())
		cfgMgr->activateTableGroup(backboneGroupEdit.originalGroupName_, localNewBackboneKey, accumulatedWarnings ? accumulatedWarnings : nullptr);

	if(activateNewGroup)
		cfgMgr->activateTableGroup(groupNameToSave, newGroupKey, accumulatedWarnings ? accumulatedWarnings : nullptr);

	__GEN_COUT__ << "Changes saved." << __E__;
}  // end GroupEditStruct::saveChanges()

//==============================================================================
//Used for debugging Configuration calls during development
void ConfigurationManagerRW::testXDAQContext()
{
	if(1) return; //if 0 to debug
	__GEN_COUTV__(runTimeSeconds());

	std::string accumulatedWarningsStr;
	std::string* accumulatedWarnings = &accumulatedWarningsStr;

	// get Group Info too!
	try
	{
		// build allGroupInfo_ for the ConfigurationManagerRW

		std::set<std::string /*name*/> tableGroups = theInterface_->getAllTableGroupNames();
		__GEN_COUT__ << "Number of Groups: " << tableGroups.size() << __E__;

		__GEN_COUTV__(runTimeSeconds());
		TableGroupKey key;
		std::string   name;
		for(const auto& fullName : tableGroups)
		{
			TableGroupKey::getGroupNameAndKey(fullName, name, key);
			cacheGroupKey(name, key);
		}
		__GEN_COUTV__(runTimeSeconds());
		// for each group get member map & comment, author, time, and type for latest key
		for(auto& groupInfo : allGroupInfo_)
		{
			try
			{
				loadTableGroup(groupInfo.first /*groupName*/,
				               groupInfo.second.getLatestKey(),
				               false /*doActivate*/,
				               &groupInfo.second.latestKeyMemberMap_ /*groupMembers*/,
				               0 /*progressBar*/,
				               0 /*accumulateErrors*/,
				               &groupInfo.second.latestKeyGroupComment_,
				               &groupInfo.second.latestKeyGroupAuthor_,
				               &groupInfo.second.latestKeyGroupCreationTime_,
				               true /*doNotLoadMember*/,
				               &groupInfo.second.latestKeyGroupTypeString_);
			}
			catch(const std::runtime_error& e)
			{
				__GEN_COUT_WARN__ << "Error occurred loading latest group info into cache for '"
								 << groupInfo.first
				                  << "(" << groupInfo.second.getLatestKey() << ")': \n" << e.what() << __E__;

				groupInfo.second.latestKeyGroupComment_      = ConfigurationManager::UNKNOWN_INFO;
				groupInfo.second.latestKeyGroupAuthor_       = ConfigurationManager::UNKNOWN_INFO;
				groupInfo.second.latestKeyGroupCreationTime_ = ConfigurationManager::UNKNOWN_TIME;
				groupInfo.second.latestKeyGroupTypeString_   = ConfigurationManager::GROUP_TYPE_NAME_UNKNOWN;
				groupInfo.second.latestKeyMemberMap_   	     = {};
			}
			catch(...)
			{
				__GEN_COUT_WARN__ << "Error occurred loading latest group info into cache for '"
								 << groupInfo.first
				                  << "(" << groupInfo.second.getLatestKey() << ")'..." << __E__;
				groupInfo.second.latestKeyGroupComment_      = ConfigurationManager::UNKNOWN_INFO;
				groupInfo.second.latestKeyGroupAuthor_       = ConfigurationManager::UNKNOWN_INFO;
				groupInfo.second.latestKeyGroupCreationTime_ = ConfigurationManager::UNKNOWN_TIME;
				groupInfo.second.latestKeyGroupTypeString_   = ConfigurationManager::GROUP_TYPE_NAME_UNKNOWN;
				groupInfo.second.latestKeyMemberMap_   	     = {};
			}
		}  // end group info loop
		__GEN_COUTV__(runTimeSeconds());
	}      // end get group info
	catch(const std::runtime_error& e)
	{
		__SS__ << "A fatal error occurred reading the info for all table groups. Error: " << e.what() << __E__;
		__GEN_COUT_ERR__ << "\n" << ss.str();
		if(accumulatedWarnings)
			*accumulatedWarnings += ss.str();
		else
			throw;
	}
	catch(...)
	{
		__SS__ << "An unknown fatal error occurred reading the info for all table groups." << __E__;
		__GEN_COUT_ERR__ << "\n" << ss.str();
		if(accumulatedWarnings)
			*accumulatedWarnings += ss.str();
		else
			throw;
	}
	__GEN_COUT__ << "Group Info end runtime=" << runTimeSeconds() << __E__;
	

	return;
	try
	{
		__GEN_COUT__ << "Loading table..." << __E__;
		loadTableGroup("FETest", TableGroupKey(2));  // Context_1
		ConfigurationTree t = getNode("/FETable/DEFAULT/FrontEndType");

		std::string v;

		__GEN_COUT__ << __E__;
		t.getValue(v);
		__GEN_COUT__ << "Value: " << v << __E__;
		__GEN_COUT__ << "Value index: " << t.getValue<int>() << __E__;

		return;
	}
	catch(...)
	{
		__GEN_COUT__ << "Failed to load table..." << __E__;
	}
}
