#include "otsdaq/CoreSupervisors/ConfigurationSupervisorBase.h"
#include "otsdaq/TablePlugins/XDAQContextTable/XDAQContextTable.h"

using namespace ots;

//==============================================================================
/// getConfigurationStatusXML
void ConfigurationSupervisorBase::getConfigurationStatusXML(
    HttpXmlDocument& xmlOut, ConfigurationManagerRW* cfgMgr, const std::string& username)
{
	std::map<std::string /*type*/, std::pair<std::string /*groupName*/, TableGroupKey>>
	    activeGroupMap = cfgMgr->getActiveTableGroups();

	for(auto& type : activeGroupMap)
	{
		xmlOut.addTextElementToData(type.first + "-ActiveGroupName", type.second.first);
		xmlOut.addTextElementToData(type.first + "-ActiveGroupKey",
		                            type.second.second.toString());
		//__SUP_COUT__ << "ActiveGroup " << type.first << " " << type.second.first << "("
		//<< type.second.second << ")" << __E__;
	}
	try  // try to get matching group alias for active groups
	{
		std::map<std::string, TableVersion> activeVersions = cfgMgr->getActiveVersions();
		std::string                         groupAliasesTableName =
		    ConfigurationManager::GROUP_ALIASES_TABLE_NAME;
		if(activeVersions.find(groupAliasesTableName) != activeVersions.end())
		{
			// have an active group aliases table at this point
			std::vector<std::pair<std::string, ConfigurationTree>> aliasNodePairs =
			    cfgMgr->getNode(groupAliasesTableName).getChildren();

			std::string groupName, groupKey, groupComment, groupType;
			std::string activeGroupName, activeGroupKey;
			for(auto& type : activeGroupMap)
			{
				activeGroupName = type.second.first;
				activeGroupKey  = type.second.second.toString();
				for(auto& aliasNodePair : aliasNodePairs)
				{
					groupName =
					    aliasNodePair.second.getNode("GroupName").getValueAsString();
					groupKey =
					    aliasNodePair.second.getNode("GroupKey").getValueAsString();
					if(groupName == activeGroupName && groupKey == activeGroupKey)
					{  // found match!
						xmlOut.addTextElementToData(type.first + "-ActiveGroupAlias",
						                            aliasNodePair.first);
						break;
					}
				}  // end alias match search loop
			}      // end active group loop
		}          // end handling of matching group alias to active groups
		           // else ignore missing active group alias table or active backbone
	}
	catch(...)
	{
		__COUT__ << "Ignoring failure getting alias for active groups" << __E__;
	}

	// always add version tracking bool
	xmlOut.addTextElementToData(
	    "versionTracking",
	    ConfigurationInterface::isVersionTrackingEnabled() ? "ON" : "OFF");

	xmlOut.addTextElementToData("configUsername", username);

}  // end getConfigurationStatusXML()

//==============================================================================
/// handleCreateTableXML
///
///	Save the detail of specific table specified
///		by tableName and version
///		...starting from dataOffset
///
///	Note: if starting version is -1 start from mock-up
void ConfigurationSupervisorBase::handleCreateTableXML(HttpXmlDocument&        xmlOut,
                                                       ConfigurationManagerRW* cfgMgr,
                                                       const std::string&      tableName,
                                                       TableVersion            version,
                                                       bool               makeTemporary,
                                                       const std::string& data,
                                                       const int&         dataOffset,
                                                       const std::string& author,
                                                       const std::string& comment,
                                                       bool               sourceTableAsIs,
                                                       bool lookForEquivalent)
try
{
	//__COUT__ << "handleCreateTableXML: " << tableName << " version: " <<
	// version
	//		<< " dataOffset: " << dataOffset << __E__;

	//__COUT__ << "data: " << data << __E__;

	// create temporary version from starting version
	if(!version.isInvalid())  // if not using mock-up, make sure starting version is
	                          // loaded
	{
		try
		{
			cfgMgr->getVersionedTableByName(tableName, version);
		}
		catch(...)
		{
			// force to mockup
			version = TableVersion();
		}
	}

	TableBase* table = cfgMgr->getTableByName(tableName);

	// check that the source version has the right number of columns
	//	if there is a mismatch, start from mockup
	if(!version.isInvalid())  // if not using mock-up, then the starting version is the
	                          // active one
	{
		// compare active to mockup column counts
		if(table->getViewP()->getDataColumnSize() !=
		       table->getMockupViewP()->getNumberOfColumns() ||
		   table->getViewP()->getSourceColumnMismatch() != 0)
		{
			__COUT__ << "table->getViewP()->getNumberOfColumns() "
			         << table->getViewP()->getNumberOfColumns() << __E__;
			__COUT__ << "table->getMockupViewP()->getNumberOfColumns() "
			         << table->getMockupViewP()->getNumberOfColumns() << __E__;
			__COUT__ << "table->getViewP()->getSourceColumnMismatch() "
			         << table->getViewP()->getSourceColumnMismatch() << __E__;
			__COUT_INFO__
			    << "Source view v" << version
			    << " has a mismatch in the number of columns, so using mockup as source."
			    << __E__;
			version = TableVersion();  // invalid = mockup
		}
	}

	bool ignoreDuplicates = false;
	if(!version.isTemporaryVersion() && sourceTableAsIs &&
	   table->getViewP()->getSourceColumnNames().size() !=
	       table->getViewP()->getDataColumnSize())
	{
		__COUT__ << "table->getViewP()->getNumberOfColumns() "
		         << table->getViewP()->getNumberOfColumns() << __E__;
		__COUTV__(table->getViewP()->getSourceColumnNames().size());
		__COUT_INFO__
		    << "Source view v" << version
		    << " has a mismatch in the number of columns, so forcing new version saved."
		    << __E__;
		ignoreDuplicates = true;
	}

	// create a temporary version from the source version
	TableVersion temporaryVersion = table->createTemporaryView(version);

	__COUT__ << "\t\ttemporaryVersion: " << temporaryVersion << __E__;

	TableView* cfgView = table->getTemporaryView(temporaryVersion);

	int retVal;
	try
	{
		// returns -1 on error that data was unchanged
		retVal =
		    sourceTableAsIs ? 0 : cfgView->fillFromEncodedCSV(data, dataOffset, author);

		if(retVal == 1)  // data was same but columns are different!
		{
			__COUT__ << "Data was the same, but columns have changed!" << __E__;
			__COUTV__(sourceTableAsIs);
			__COUTV__(lookForEquivalent);
		}

		cfgView->setURIEncodedComment(comment);
		__COUT__ << "Table comment was set to:\n\t" << cfgView->getComment() << __E__;
	}
	catch(...)  // erase temporary view before re-throwing error
	{
		__COUT__ << "Caught error while editing. Erasing temporary version." << __E__;
		table->eraseView(temporaryVersion);
		throw;
	}

	// Note: be careful with any further table operations at this point..
	//	must catch errors and erase temporary version on failure.

	// only consider it an error if source version was persistent version
	//	allow it if source version is temporary and we are making a persistent version now
	//	also, allow it if version tracking is off.
	if(retVal < 0 && (!version.isTemporaryVersion() || makeTemporary) &&
	   ConfigurationInterface::isVersionTrackingEnabled())
	{
		if(!version.isInvalid() &&       // if source version was mockup, then consider it
		                                 // attempt to create a blank table
		   !version.isScratchVersion())  // if source version was scratch, then consider
		                                 // it attempt to make it persistent
		{
			__SS__ << "No rows were modified! No reason to fill a view with same content."
			       << __E__;
			__COUT_ERR__ << "\n" << ss.str();
			// delete temporaryVersion
			table->eraseView(temporaryVersion);
			__SS_THROW__;
		}
		else if(version.isInvalid())
			__COUT__ << "This was interpreted as an attempt to create a blank table."
			         << __E__;
		else if(version.isScratchVersion())
			__COUT__ << "This was interpreted as an attempt to make a persistent "
			            "version of the scratch table."
			         << __E__;
		else
		{
			__SS__;
			__THROW__(ss.str() + "impossible!");
		}
	}
	else if(retVal < 0 && (version.isTemporaryVersion() && !makeTemporary))
	{
		__COUT__ << "Allowing the static data because this is converting from "
		            "temporary to persistent version."
		         << __E__;
	}
	else if(retVal < 0 && !ConfigurationInterface::isVersionTrackingEnabled())
	{
		__COUT__ << "Allowing the static data because version tracking is OFF." << __E__;
	}
	else if(retVal < 0)
	{
		__SS__ << "This should not be possible! Fatal error." << __E__;
		// delete temporaryVersion
		table->eraseView(temporaryVersion);
		__SS_THROW__;
	}

	// note: if sourceTableAsIs, accept equivalent versions
	auto newVersion = ConfigurationSupervisorBase::saveModifiedVersionXML(
	    xmlOut,
	    cfgMgr,
	    tableName,
	    version,
	    makeTemporary,
	    table,
	    temporaryVersion,
	    ignoreDuplicates /*ignoreDuplicates*/,
	    lookForEquivalent || sourceTableAsIs /*lookForEquivalent*/);

	if(ignoreDuplicates && sourceTableAsIs)  // reset cache for this table
	{
		table = cfgMgr->getTableByName(tableName);
		table->eraseView(newVersion);
	}
}  // end handleCreateTableXML()
catch(std::runtime_error& e)
{
	__SS__ << "Error saving new table!\n\n " << e.what() << __E__;
	__COUT__ << "\n" << ss.str() << __E__;
	xmlOut.addTextElementToData("Error", ss.str());
}
catch(...)
{
	__SS__ << "Error saving new table!" << __E__;
	try
	{
		throw;
	}  //one more try to printout extra info
	catch(const std::exception& e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...)
	{
	}
	__COUT__ << "\n" << ss.str() << __E__;
	xmlOut.addTextElementToData("Error", ss.str());
}  // end handleCreateTableXML() catch

//==============================================================================
/// saveModifiedVersionXML
///
/// once source version has been modified in temporary version
///	this function finishes it off.
TableVersion ConfigurationSupervisorBase::saveModifiedVersionXML(
    HttpXmlDocument&        xmlOut,
    ConfigurationManagerRW* cfgMgr,
    const std::string&      tableName,
    TableVersion            originalVersion,
    bool                    makeTemporary,
    TableBase*              table,
    TableVersion            temporaryModifiedVersion,
    bool                    ignoreDuplicates,
    bool                    lookForEquivalent)
{
	bool         foundEquivalent;
	TableVersion newAssignedVersion =
	    cfgMgr->saveModifiedVersion(tableName,
	                                originalVersion,
	                                makeTemporary,
	                                table,
	                                temporaryModifiedVersion,
	                                ignoreDuplicates,
	                                lookForEquivalent,
	                                &foundEquivalent);

	xmlOut.addTextElementToData("savedName", tableName);
	xmlOut.addTextElementToData("savedVersion", newAssignedVersion.toString());

	if(foundEquivalent)
	{
		xmlOut.addTextElementToData("foundEquivalentVersion", "1");
		xmlOut.addTextElementToData(tableName + "_foundEquivalentVersion", "1");
	}
	return newAssignedVersion;
}  // end saveModifiedVersionXML()

//==============================================================================
///	handleCreateTableGroupXML
///
///		Save a new TableGroup:
///			Search for existing TableGroupKeys for this TableGroup
///			Append a "bumped" system key to name
///			Save based on list of tableName/TableVersion
///
///		tableList parameter is comma separated table name and version
///
///		Note: if version of -1 (INVALID/MOCKUP) is given and there are no other existing
/// table versions... 			a new table version is generated using the mockup table.
///
///		Table Version Alias Handling:
///			Allow table versions to be specified as an alias with ALIAS: preamble. Aliased
/// versions 			will be translated according to the active backbone at activation
/// time.
///
///
void ConfigurationSupervisorBase::handleCreateTableGroupXML(
    HttpXmlDocument&        xmlOut,
    ConfigurationManagerRW* cfgMgr,
    const std::string&      groupName,
    const std::string&      tableList,
    bool                    allowDuplicates,
    bool                    ignoreWarnings,
    const std::string&      groupComment,
    bool                    lookForEquivalent)
try
{
	__COUTT__ << "handleCreateTableGroupXML start runtime=" << cfgMgr->runTimeSeconds()
	          << __E__;

	xmlOut.addTextElementToData("AttemptedNewGroupName", groupName);

	// Only need table info for aliases and to check table names are valid
	//  ... so do not refresh, unless there is no info present.
	std::string                             accumulatedWarnings;
	const std::map<std::string, TableInfo>& allTableInfo =
	    cfgMgr->getAllTableInfo();  //no need to refresh
	if(!allTableInfo.size())  //getAllTableInfo() will update allTableInfo by reference
		cfgMgr->getAllTableInfo(true /* refresh */,
		                        &accumulatedWarnings,
		                        "" /* errorFilterName */,
		                        true /* getGroupKeys*/,
		                        false /* getGroupInfo */,
		                        true /* initializeActiveGroups */);

	if(accumulatedWarnings.size())
		__COUT_WARN__ << "Ignoring these errors: " << accumulatedWarnings << __E__;

	// cfgMgr->loadConfigurationBackbone(); //already loaded by initializeActiveGroups of getAllTableInfo

	__COUTT__ << "handleCreateTableGroupXML loaded runtime=" << cfgMgr->runTimeSeconds()
	          << __E__;

	std::map<std::string /*tableName*/,
	         std::map<std::string /*aliasName*/, TableVersion /*version*/>>
	    versionAliases = cfgMgr->getVersionAliases();
	//	for(const auto& aliases : versionAliases)
	//		for(const auto& alias : aliases.second)
	//			__COUT__ << aliases.first << " " << alias.first << " " << alias.second
	//			             << __E__;

	std::map<std::string /*name*/, TableVersion /*version*/> groupMembers;
	std::map<std::string /*name*/, std::string /*alias*/>    memberTableAliases;

	std::string  name, versionStr, alias;
	TableVersion version;
	auto         c = tableList.find(',', 0);
	auto         i = c;
	i              = 0;  // auto used to get proper index/length type
	while(c < tableList.length())
	{
		// add the table and version pair to the map
		name = tableList.substr(i, c - i);
		i    = c + 1;
		c    = tableList.find(',', i);
		if(c == std::string::npos)  // missing version list entry?!
		{
			__SS__ << "Incomplete Table Name-Version pair!" << __E__;
			__COUT_ERR__ << "\n" << ss.str();
			xmlOut.addTextElementToData("Error", ss.str());
			return;
		}

		versionStr = tableList.substr(i, c - i);
		i          = c + 1;
		c          = tableList.find(',', i);

		//__COUT__ << "name: " << name << __E__;
		//__COUT__ << "versionStr: " << versionStr << __E__;

		// check if version is an alias and convert
		if(versionStr.find(ConfigurationManager::ALIAS_VERSION_PREAMBLE) == 0)
		{
			alias =
			    versionStr.substr(ConfigurationManager::ALIAS_VERSION_PREAMBLE.size());

			__COUT__ << "Found alias " << name << " " << versionStr << __E__;

			// convert alias to version
			if(versionAliases.find(name) != versionAliases.end() &&
			   versionAliases[name].find(alias) != versionAliases[name].end())
			{
				version = versionAliases[name][alias];
				__COUT__ << name << " version alias '" << alias
				         << "'translated to: " << version << __E__;

				memberTableAliases[name] = alias;
			}
			else
			{
				__SS__ << "version alias '"
				       << versionStr.substr(
				              ConfigurationManager::ALIAS_VERSION_PREAMBLE.size())
				       << "' was not found in active version aliases! Please check your "
				          "active backbone!"
				       << __E__;
				__COUT_ERR__ << "\n" << ss.str();
				xmlOut.addTextElementToData("Error", ss.str());
				return;
			}
		}
		else
			version = TableVersion(versionStr);

		if(version.isTemporaryVersion())
		{
			__SS__ << "Groups can not be created using temporary member tables. "
			       << "Table member '" << name << "' with temporary version '" << version
			       << "' is illegal." << __E__;
			xmlOut.addTextElementToData("Error", ss.str());
			return;
		}

		// enforce that table exists
		if(allTableInfo.find(name) == allTableInfo.end())
		{
			__SS__ << "Groups can not be created using mock-up member tables of "
			          "undefined tables. "
			       << "Table member '" << name << "' is not defined." << __E__;
			xmlOut.addTextElementToData("Error", ss.str());
			return;
		}

		if(version.isMockupVersion())
		{
			// if mockup, then generate a new persistent version to use based on mockup
			TableBase* table = cfgMgr->getTableByName(name);
			// create a temporary version from the mockup as source version
			TableVersion temporaryVersion = table->createTemporaryView();
			__COUT__ << "\t\ttemporaryVersion: " << temporaryVersion << __E__;

			// if other versions exist check for another mockup, and use that instead
			__COUT__ << "Creating version from mock-up for name: " << name
			         << " inputVersionStr: " << versionStr << __E__;

			// set table comment
			table->getTemporaryView(temporaryVersion)
			    ->setComment("Auto-generated from mock-up.");

			// finish off the version creation
			version = ConfigurationSupervisorBase::saveModifiedVersionXML(
			    xmlOut,
			    cfgMgr,
			    name,
			    TableVersion() /*original source is mockup*/,
			    false /* makeTemporary */,
			    table,
			    temporaryVersion /*temporary modified version*/,
			    false /*ignore duplicates*/,
			    true /*look for equivalent*/);

			__COUT__ << "Using mockup version: " << version << __E__;
		}

		//__COUT__ << "version: " << version << __E__;
		groupMembers[name] = version;
	}  // end member verification loop

	__COUTT__ << "handleCreateTableGroupXML tables saved runtime="
	          << cfgMgr->runTimeSeconds() << __E__;

	__COUTV__(StringMacros::mapToString(memberTableAliases));

	if(!allowDuplicates)
	{
		__COUT__ << "Checking for duplicate groups..." << __E__;
		try
		{
			TableGroupKey foundKey =
			    cfgMgr->findTableGroup(groupName, groupMembers, memberTableAliases);
			__COUTT__ << "handleCreateTableGroupXML group duplicates checked runtime="
			          << cfgMgr->runTimeSeconds() << __E__;
			if(!foundKey.isInvalid())
			{
				// return found equivalent key
				xmlOut.addTextElementToData("TableGroupName", groupName);
				xmlOut.addTextElementToData("TableGroupKey", foundKey.toString());

				if(lookForEquivalent)
				{
					__COUT__ << "Found equivalent group key (" << foundKey << ") for "
					         << groupName << "." << __E__;
					// allow this equivalent group to be the response without an error
					xmlOut.addTextElementToData("foundEquivalentKey", "1");  // indicator

					// insert get table info
					handleGetTableGroupXML(xmlOut,
					                       cfgMgr,
					                       groupName,
					                       foundKey,
					                       ignoreWarnings,
					                       true /* cacheOnly */);
					return;
				}
				else  // treat as error, if not looking for equivalent
				{
					__COUT__ << "Treating duplicate group as error." << __E__;
					__SS__ << ("Failed to create table group: " + groupName +
					           ". It is a duplicate of an existing group key (" +
					           foundKey.toString() + ")");
					__COUT_ERR__ << ss.str() << __E__;
					xmlOut.addTextElementToData("Error", ss.str());
					return;
				}
			}

			__COUT__ << "Check for duplicate groups complete." << __E__;
		}
		catch(...)
		{
			__COUT_WARN__ << "Ignoring errors looking for duplicate groups! Proceeding "
			                 "with new group creation."
			              << __E__;
		}
	}

	// check the group for errors before creating group
	try
	{
		cfgMgr->loadMemberMap(groupMembers);

		std::string accumulateErrors = "";
		for(auto& groupMemberPair : groupMembers)
		{
			TableView* cfgViewPtr =
			    cfgMgr->getTableByName(groupMemberPair.first)->getViewP();
			if(cfgViewPtr->getDataColumnSize() != cfgViewPtr->getNumberOfColumns() ||
			   cfgViewPtr->getSourceColumnMismatch() !=
			       0)  // check for column size mismatch
			{
				// const std::set<std::string> srcColNames = cfgViewPtr->getSourceColumnNames();
				__SS__ << "\n\nThere were errors found in loading a member table "
				       << groupMemberPair.first << ":v" << cfgViewPtr->getVersion()
				       << ". Please see the details below:\n\n"
				       << cfgViewPtr->getMismatchColumnInfo();

				__COUT_ERR__ << "\n" << ss.str();
				xmlOut.addTextElementToData("Error", ss.str());
				return;
			}
		}
	}
	catch(std::runtime_error& e)
	{
		__SS__ << "Failed to create table group: " << groupName
		       << ".\nThere were problems loading the chosen members:\n\n"
		       << e.what() << __E__;
		__COUT_ERR__ << "\n" << ss.str();
		xmlOut.addTextElementToData("Error", ss.str());
		return;
	}
	catch(...)
	{
		__SS__ << "Failed to create table group: " << groupName << __E__;
		try
		{
			throw;
		}  //one more try to printout extra info
		catch(const std::exception& e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...)
		{
		}
		__COUT_ERR__ << "\n" << ss.str();
		xmlOut.addTextElementToData("Error", ss.str());
		return;
	}

	__COUTT__ << "handleCreateTableGroupXML group members init checked runtime="
	          << cfgMgr->runTimeSeconds() << __E__;

	// check the tree for warnings before creating group
	std::string accumulateTreeErrs;
	cfgMgr->getChildren(&groupMembers, &accumulateTreeErrs);
	if(accumulateTreeErrs != "")
	{
		__COUT_WARN__ << "\n" << accumulateTreeErrs << __E__;
		if(!ignoreWarnings)
		{
			xmlOut.addTextElementToData("TreeErrors", accumulateTreeErrs);
			return;
		}
	}

	__COUTT__ << "handleCreateTableGroupXML tree checked runtime="
	          << cfgMgr->runTimeSeconds() << __E__;

	TableGroupKey newKey;
	try
	{
		__COUT__ << "Saving new group..." << __E__;
		newKey = cfgMgr->saveNewTableGroup(
		    groupName, groupMembers, groupComment, &memberTableAliases);
	}
	catch(std::runtime_error& e)
	{
		__SS__ << "Failed to create table group: " << groupName << __E__;
		ss << "\n\n" << e.what() << __E__;
		__COUT_ERR__ << ss.str();
		xmlOut.addTextElementToData("Error", ss.str());
		return;
	}
	catch(...)
	{
		__SS__ << "Failed to create table group: " << groupName << __E__;
		try
		{
			throw;
		}  //one more try to printout extra info
		catch(const std::exception& e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...)
		{
		}
		__COUT_ERR__ << ss.str();
		xmlOut.addTextElementToData("Error", ss.str());
		return;
	}

	__COUTT__ << "handleCreateTableGroupXML group saved runtime="
	          << cfgMgr->runTimeSeconds() << __E__;

	// insert get table info
	__COUT__ << "Loading new table group..." << __E__;
	handleGetTableGroupXML(
	    xmlOut, cfgMgr, groupName, newKey, ignoreWarnings, true /* cacheOnly */);

	__COUTT__ << "handleCreateTableGroupXML end runtime=" << cfgMgr->runTimeSeconds()
	          << __E__;

}  // end handleCreateTableGroupXML()
catch(std::runtime_error& e)
{
	__SS__ << "Error saving table group!\n\n " << e.what() << __E__;
	__COUT_ERR__ << "\n" << ss.str() << __E__;
	xmlOut.addTextElementToData("Error", ss.str());
}
catch(...)
{
	__SS__ << "Error saving table group!" << __E__;
	try
	{
		throw;
	}  //one more try to printout extra info
	catch(const std::exception& e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...)
	{
	}
	__COUT_ERR__ << "\n" << ss.str() << __E__;
	xmlOut.addTextElementToData("Error", ss.str());
}  // end handleCreateTableGroupXML() catch

//==============================================================================
/// handleGetTableGroupXML
///
///	give the detail of specific table specified
///		groupKey=-1 returns latest
///
///	Find historical group keys
///		and figure out all member configurations versions
///
///	Use cacheOnly to only return group info previously cached (avoid reloading from database)
///
///
///	return this information
///	<group name=xxx key=xxx>
///		<historical key=xxx>
///		<historical key=xxx>
///		....
///		<table name=xxx version=xxx />
///			<historical version=xxx>
///			<historical version=xxx>
///			...
///		</table>
///		<table name=xxx version=xxx>
///		...
///		</table>
void ConfigurationSupervisorBase::handleGetTableGroupXML(
    HttpXmlDocument&        xmlOut,
    ConfigurationManagerRW* cfgMgr,
    const std::string&      groupName,
    TableGroupKey           groupKey,
    bool                    ignoreWarnings /* = false */,
    bool                    cacheOnly /* = false */)
try
{
	// char                 tmpIntStr[100];
	xercesc::DOMElement *parentEl, *configEl;

	// steps:
	//	if invalid key, get latest key
	//	get specific group with key
	//		give member names and versions
	//		get all table groups to locate historical keys
	//	get all groups to find historical keys

	//	std::set<std::string /*name+version*/> allGroups =
	//			cfgMgr->getConfigurationInterface()->getAllTableGroupNames(groupName);
	//	std::string name;
	//	TableGroupKey key;
	//	//put them in a set to sort them as TableGroupKey defines for operator<
	//	std::set<TableGroupKey> sortedKeys;
	//	for(auto& group: allGroups)
	//	{
	//		//now uses database filter
	//		TableGroupKey::getGroupNameAndKey(group,name,key);
	//		//if(name == groupName)
	//		sortedKeys.emplace(key);
	//	}

	__COUTTV__(cacheOnly);

	//======
	/// Lambda function to output historic values in spans
	auto SpanToXML = [](auto const& sortedKeys, auto& xmlOut) {
		//add lo and hi spans, instead of each individual value
		size_t lo = -1, hi = -1;
		for(auto& keyInOrder : sortedKeys)
		{
			if(lo == size_t(-1))  //establish start of potential span
			{
				hi = lo = keyInOrder.key();
				continue;
			}
			else if(hi + 1 == keyInOrder.key())  //span is growing
			{
				hi = keyInOrder.key();
				continue;
			}
			//else jump by more than one, so close out span

			if(lo == hi)  //single value
				xmlOut.addNumberElementToData("HistoricalTableGroupKey", lo);
			else  //span
				xmlOut.addTextElementToData(
				    "HistoricalTableGroupKey",
				    "_" + std::to_string(lo) + "_" + std::to_string(hi));
			hi = lo = keyInOrder.key();
		}

		if(lo != size_t(-1))  //check if last one to do!
		{
			if(lo == hi)  //single value
				xmlOut.addNumberElementToData("HistoricalTableGroupKey", lo);
			else  //span
				xmlOut.addTextElementToData(
				    "HistoricalTableGroupKey",
				    "_" + std::to_string(lo) + "_" + std::to_string(hi));
		}
	};  //end local lambda SpanToXML()
	auto vSpanToXML = [](auto const& sortedKeys, auto& xmlOut, auto& configEl) {
		//add lo and hi spans, instead of each individual value
		size_t lo = -1, hi = -1;
		for(auto& keyInOrder : sortedKeys)
		{
			if(lo == size_t(-1))  //establish start of potential span
			{
				hi = lo = keyInOrder.version();
				continue;
			}
			else if(hi + 1 == keyInOrder.version())  //span is growing
			{
				hi = keyInOrder.version();
				continue;
			}
			//else jump by more than one, so close out span

			if(lo == hi)  //single value
				xmlOut.addNumberElementToParent("TableExistingVersion", lo, configEl);
			else  //span
				xmlOut.addTextElementToParent(
				    "TableExistingVersion",
				    "_" + std::to_string(lo) + "_" + std::to_string(hi),
				    configEl);
			hi = lo = keyInOrder.version();
		}

		if(lo != size_t(-1))  //check if last one to do!
		{
			if(lo == hi)  //single value
				xmlOut.addNumberElementToParent("TableExistingVersion", lo, configEl);
			else  //span
				xmlOut.addTextElementToParent(
				    "TableExistingVersion",
				    "_" + std::to_string(lo) + "_" + std::to_string(hi),
				    configEl);
		}
	};  //end local lambda vSpanToXML()

	{
		const GroupInfo& groupInfo =
		    cfgMgr->getGroupInfo(groupName, !cacheOnly /* attemptToReloadKeys */);
		const std::set<TableGroupKey>& sortedKeys = groupInfo.getKeys();  // rename

		__COUTT__ << groupName << " keys: " << StringMacros::setToString(sortedKeys)
		          << __E__;
		__COUTT__ << "Active groups: "
		          << StringMacros::mapToString(cfgMgr->getActiveTableGroups()) << __E__;
		__COUTT__ << "Active tables: "
		          << StringMacros::mapToString(cfgMgr->getActiveVersions()) << __E__;

		if(!cacheOnly &&
		   (groupKey
		        .isInvalid() ||  // if invalid or not found, likely key info not loaded, so get latest
		    sortedKeys.find(groupKey) == sortedKeys.end() ||
		    sortedKeys.size() < 2))
		{
			// report error if group key not found
			if(!groupKey.isInvalid() || sortedKeys.size() == 0)
			{
				// attempt to reload all group info and power through
				std::string accumulatedWarnings;
				__COUTT__ << "Attempting full table refresh (assuming cache not yet "
				             "established)."
				          << __E__;
				/*const std::map<std::string, TableInfo>& allTableInfo = */ cfgMgr
				    ->getAllTableInfo(true /* refresh */,
				                      &accumulatedWarnings,
				                      "" /* errorFilterName */,
				                      true /* getGroupKeys*/,
				                      false /* getGroupInfo */,
				                      true /* initializeActiveGroups */);
				__COUTT__ << "After full table refresh (assuming cache not yet "
				             "established) so ignoring these errors: "
				          << accumulatedWarnings << __E__;

				// xmlOut.addTextElementToData("Error", ss.str());

				const GroupInfo& groupInfo2 = cfgMgr->getGroupInfo(groupName);
				const std::set<TableGroupKey>& sortedKeys2 =
				    groupInfo2.getKeys();  // rename

				if(sortedKeys2.find(groupKey) == sortedKeys2.end())
				{
					__SS__ << "Group key " << groupKey << " was not found for group '"
					       << groupName << "!'" << __E__;
					ss << "Her are the found " << sortedKeys2.size() << " '" << groupName
					   << "' keys: " << __E__;
					for(auto& keyInOrder : sortedKeys2)
						ss << "\t" << keyInOrder << __E__;
					__COUT_WARN__ << "\n" << ss.str() << __E__;
				}
				SpanToXML(sortedKeys2, xmlOut);
			}
			else
			{
				if(sortedKeys.size())
					groupKey = *sortedKeys.rbegin();
				__COUT_WARN__
				    << "Group key requested was invalid or not found, going with latest "
				    << groupKey << __E__;

				// add all other sorted keys for this groupName
				SpanToXML(sortedKeys, xmlOut);
			}
		}
		else
		{
			// add all other sorted keys for this groupName
			SpanToXML(sortedKeys, xmlOut);
		}

		if(cfgMgr->getActiveVersions().size() == 0)
		{
			__COUTT__
			    << "There are no active tables. Attempting to initialize active groups."
			    << __E__;
			//if no active tables, attempt to init active groups (it should prevent confusing warnings to users complaining about a partially loaded configuration)
			std::string tmpAccumulateWarnings;
			cfgMgr->init(0 /*accumulatedErrors*/,
			             false /*initForWriteAccess*/,
			             &tmpAccumulateWarnings);
			__COUTT__ << "Now Active tables: "
			          << StringMacros::mapToString(cfgMgr->getActiveVersions()) << __E__;
			__COUTT__ << "Ingoring warnings during init of active groups: "
			          << tmpAccumulateWarnings << __E__;
		}
	}

	xmlOut.addTextElementToData("TableGroupName", groupName);
	xmlOut.addTextElementToData("TableGroupKey", groupKey.toString());

	parentEl = xmlOut.addTextElementToData("TableGroupMembers", "");

	//	get specific group with key
	std::map<std::string /*name*/, TableVersion /*version*/> memberMap;
	std::map<std::string /*name*/, std::string /*alias*/>    groupMemberAliases;

	__COUT__ << "groupName=" << groupName << __E__;
	__COUT__ << "groupKey=" << groupKey << __E__;

	const std::map<std::string, TableInfo>& allTableInfo = cfgMgr->getAllTableInfo();
	std::map<std::string, TableInfo>::const_iterator it;

	// load group so comments can be had
	//	and also group metadata (author, comment, createTime)
	try
	{
		std::string groupAuthor, groupComment, groupCreationTime, groupTypeString;
		std::string accumulateTreeErrors;

		__COUTV__(ignoreWarnings);
		cfgMgr->loadTableGroup(groupName,
		                       groupKey,
		                       false /*doActivate*/,
		                       &memberMap,
		                       0 /*progressBar*/,
		                       ignoreWarnings ? 0 : /*accumulateTreeErrors*/
		                           &accumulateTreeErrors,
		                       &groupComment,
		                       &groupAuthor,
		                       &groupCreationTime,
		                       false /*doNotLoadMember*/,
		                       &groupTypeString,
		                       &groupMemberAliases);

		if(accumulateTreeErrors != "")
		{
			__COUTV__(accumulateTreeErrors);
			xmlOut.addTextElementToData("TreeErrors", accumulateTreeErrors);
		}

		xmlOut.addTextElementToData("TableGroupAuthor", groupAuthor);
		xmlOut.addTextElementToData("TableGroupComment", groupComment);
		xmlOut.addTextElementToData("TableGroupCreationTime", groupCreationTime);
		xmlOut.addTextElementToData("TableGroupType", groupTypeString);
	}
	catch(const std::runtime_error& e)
	{
		__SS__ << "Table group \"" + groupName + "(" + groupKey.toString() + ")" +
		              "\" members can not be loaded!\n\n" + e.what()
		       << __E__;
		__COUT_ERR__ << ss.str();
		xmlOut.addTextElementToData("Error", ss.str());
		// return;
	}
	catch(...)
	{
		__SS__ << "Table group \"" + groupName + "(" + groupKey.toString() + ")" +
		              "\" members can not be loaded!"
		       << __E__;
		try
		{
			throw;
		}  //one more try to printout extra info
		catch(const std::exception& e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...)
		{
		}
		__COUT_ERR__ << ss.str();
		xmlOut.addTextElementToData("Error", ss.str());
		// return;
	}

	__COUTV__(StringMacros::mapToString(groupMemberAliases));

	std::map<std::string /*table name*/,
	         std::map<std::string /*version alias*/, TableVersion /*aliased version*/>>
	    versionAliases = cfgMgr->getVersionAliases();

	__COUT__ << "# of table version aliases: " << versionAliases.size() << __E__;

	// Seperate loop to get name and version
	for(auto& memberPair : memberMap)
	{
		xmlOut.addTextElementToParent("MemberName", memberPair.first, parentEl);

		// if member is in groupMemberAliases, then alias version
		if(groupMemberAliases.find(memberPair.first) != groupMemberAliases.end())
		{
			configEl = xmlOut.addTextElementToParent(
			    "MemberVersion",
			    ConfigurationManager::ALIAS_VERSION_PREAMBLE +
			        groupMemberAliases[memberPair.first],  // return the ALIAS:<alias>
			    parentEl);
			//also include actual version for reference
			xmlOut.addTextElementToParent(
			    "ProvenanceMemberVersion", memberPair.second.toString(), configEl);
			// AND warn if actual version mismatches current alias!!
			// if no current backbone alias for this member, or alias version does not match member version
			auto vit = versionAliases.find(memberPair.first);
			if(vit == versionAliases.end() ||  //tableName is not in backbone aliases
			   vit->second.find(groupMemberAliases[memberPair.first]) ==
			       vit->second.end() ||  //alias is not in backbone aliases for this table
			   vit->second[groupMemberAliases[memberPair.first]] !=
			       memberPair
			           .second)  //backbone's alias version does not match provenance member version
			{
				__SS__ << "Warning: Version alias mismatch with active Backbone! <b>\"" +
				              groupName + "(" + groupKey.toString() + ")" +
				              "\"</b> group member table <b>'"
				       << memberPair.first << "'</b> is using version alias <b>'"
				       << groupMemberAliases[memberPair.first] << "'</b>";
				if(vit == versionAliases.end() ||  //tableName is not in backbone aliases
				   vit->second.find(groupMemberAliases[memberPair.first]) ==
				       vit->second.end())
				{
					ss << " which no longer has a valid translation in the active "
					      "Backbone! "
					   << " The original provenance version is <b>" << memberPair.first
					   << "-v" << memberPair.second
					   << "</b>.\n\nPlease consider whether this is an issue, or if you "
					      "should first recreate this group with updated Backbone table "
					      "alias translations before activating.";

					//also flag that active backbone translation is invalid
					xmlOut.addTextElementToParent("ActiveBackboneAliasVersion",
					                              TableVersion().toString() /* invalid*/,
					                              configEl);
				}
				else
				{
					ss << " which translates to <b>" << memberPair.first << "-v"
					   << vit->second[groupMemberAliases[memberPair.first]]
					   << "</b> with current active Backbone group."
					   << " There is a mismatch with the original provenance version of "
					      "<b>"
					   << memberPair.first << "-v" << memberPair.second
					   << "</b>.\n\nPlease consider whether this is an issue, or if you "
					      "should first recreate this group with updated Backbone table "
					      "alias translations before activating."
					   << __E__;

					//also include mismatching active backone translation
					xmlOut.addTextElementToParent(
					    "ActiveBackboneAliasVersion",
					    vit->second[groupMemberAliases[memberPair.first]].toString(),
					    configEl);
				}
				__COUT_WARN__ << "\n" << ss.str() << __E__;
				xmlOut.addTextElementToData("Warning", ss.str());
			}  //end alias warning handling
			else
			{
				__COUT__ << "\"" + groupName + "(" + groupKey.toString() + ")" +
				                "\" group member table '"
				         << memberPair.first << "' is using version alias '"
				         << groupMemberAliases[memberPair.first]
				         << "' which currently matches the active Backbone translation "
				            "version v"
				         << memberPair.second << " = v"
				         << vit->second[groupMemberAliases[memberPair.first]] << __E__;
			}
		}
		else
			configEl = xmlOut.addTextElementToParent(
			    "MemberVersion", memberPair.second.toString(), parentEl);

		it = allTableInfo.find(memberPair.first);
		if(it == allTableInfo.end())
		{
			if(!cacheOnly)  //only an 'error' if not cacheOnly
				xmlOut.addTextElementToData(
				    "Error", "Table \"" + memberPair.first + "\" can not be retrieved!");
			continue;
		}

		xmlOut.addTextElementToParent(
		    "MemberComment", it->second.tablePtr_->getView().getComment(), parentEl);

		if(versionAliases.find(it->first) != versionAliases.end())
			for(auto& aliasVersion : versionAliases[it->first])
				xmlOut.addTextElementToParent(
				    "TableExistingVersion",
				    ConfigurationManager::ALIAS_VERSION_PREAMBLE + aliasVersion.first,
				    configEl);

		vSpanToXML(it->second.versions_, xmlOut, configEl);
	}  //end member map loop

}  // end handleGetTableGroupXML()
catch(std::runtime_error& e)
{
	__SS__ << ("Error getting table group!\n\n" + std::string(e.what())) << __E__;
	__COUT_ERR__ << "\n" << ss.str();
	xmlOut.addTextElementToData("Error", ss.str());
}
catch(...)
{
	__SS__ << ("Error getting table group!\n\n") << __E__;
	try
	{
		throw;
	}  //one more try to printout extra info
	catch(const std::exception& e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...)
	{
	}
	__COUT_ERR__ << "\n" << ss.str();
	xmlOut.addTextElementToData("Error", ss.str());
}  // end handleGetTableGroupXML() catch

//==============================================================================
bool ConfigurationSupervisorBase::handleAddDesktopIconXML(
    HttpXmlDocument&        xmlOut,
    ConfigurationManagerRW* cfgMgr,
    const std::string&      iconCaption,
    const std::string&      iconAltText,
    const std::string&      iconFolderPath,
    const std::string&      iconImageURL,
    const std::string&      iconWindowURL,
    const std::string&      iconPermissions,
    std::string             windowLinkedApp /*= ""*/,
    unsigned int            windowLinkedAppLID /*= 0*/,
    bool                    enforceOneWindowInstance /*= false*/,
    const std::string&      windowParameters /*= ""*/)
try
{
	cfgMgr->getAllTableInfo(true /* refresh */);

	const std::string& author = cfgMgr->getUsername();

	__COUTV__(author);
	__COUTV__(iconCaption);
	__COUTV__(iconAltText);
	__COUTV__(iconFolderPath);
	__COUTV__(iconImageURL);
	__COUTV__(iconWindowURL);
	__COUTV__(iconPermissions);
	__COUTV__(windowLinkedApp);
	__COUTV__(windowLinkedAppLID);
	__COUTV__(enforceOneWindowInstance);

	__COUTV__(windowParameters);  // map: CSV list

	// steps:
	//	activate active context
	//		modify desktop table and desktop parameters table
	//		save, activate, and modify alias
	// just to match syntax in ConfiguratGUI
	//	tmpCfgMgr.activateTableGroup(
	//			tmpCfgMgr.getActiveGroupName(ConfigurationManager::GROUP_TYPE_NAME_CONTEXT),
	//			tmpCfgMgr.getActiveGroupKey(ConfigurationManager::GROUP_TYPE_NAME_CONTEXT)
	//			);

	cfgMgr->restoreActiveTableGroups(
	    true /*throwErrors*/,
	    "" /*pathToActiveGroupsFile*/,
	    ConfigurationManager::LoadGroupType::
	        ONLY_BACKBONE_OR_CONTEXT_TYPES /*onlyLoadIfBackboneOrContext*/
	);

	const std::string backboneGroupName =
	    cfgMgr->getActiveGroupName(ConfigurationManager::GroupType::BACKBONE_TYPE);

	GroupEditStruct contextGroupEdit(ConfigurationManager::GroupType::CONTEXT_TYPE,
	                                 cfgMgr);

	// Steps:
	//	- Create record in DesktopIconTable
	//	- Create parameter records in DesktopWindowParameterTable
	//	- Create new Context group
	//	- Update Aliases from old Context group to new Context group
	//	- Activate new group

	TableEditStruct& iconTable = contextGroupEdit.getTableEditStruct(
	    DesktopIconTable::ICON_TABLE, true /*markModified*/);
	TableEditStruct& parameterTable = contextGroupEdit.getTableEditStruct(
	    DesktopIconTable::PARAMETER_TABLE, true /*markModified*/);
	TableEditStruct& appTable = contextGroupEdit.getTableEditStruct(
	    ConfigurationManager::XDAQ_APPLICATION_TABLE_NAME);

	// Create record in DesktopIconTable
	try
	{
		unsigned int row;
		std::string  iconUID        = "";
		std::string  decodedCaption = StringMacros::decodeURIComponent(iconCaption);

		for(unsigned int i = 0; i < decodedCaption.size(); ++i)
			if((decodedCaption[i] >= 'a' && decodedCaption[i] <= 'z') ||
			   (decodedCaption[i] >= 'A' && decodedCaption[i] <= 'Z') ||
			   (decodedCaption[i] >= '0' && decodedCaption[i] <= '9'))
				iconUID += decodedCaption[i];

		// create icon record
		row = iconTable.tableView_->addRow(
		    author, true /*incrementUniqueData*/, "generatedIcon" + iconUID);
		iconUID =
		    iconTable.tableView_->getDataView()[row][iconTable.tableView_->getColUID()];

		__COUTV__(row);
		__COUTV__(iconUID);

		// set icon status true
		iconTable.tableView_->setValueAsString(
		    "1", row, iconTable.tableView_->getColStatus());

		// set caption value
		iconTable.tableView_->setURIEncodedValue(
		    iconCaption,
		    row,
		    iconTable.tableView_->findCol(DesktopIconTable::COL_CAPTION));
		// set alt text value
		iconTable.tableView_->setURIEncodedValue(
		    iconAltText,
		    row,
		    iconTable.tableView_->findCol(DesktopIconTable::COL_ALTERNATE_TEXT));
		// set force one instance value
		iconTable.tableView_->setValueAsString(
		    enforceOneWindowInstance ? "1" : "0",
		    row,
		    iconTable.tableView_->findCol(DesktopIconTable::COL_FORCE_ONLY_ONE_INSTANCE));
		// set permissions value
		iconTable.tableView_->setURIEncodedValue(
		    iconPermissions,
		    row,
		    iconTable.tableView_->findCol(DesktopIconTable::COL_PERMISSIONS));
		// set image URL value
		iconTable.tableView_->setURIEncodedValue(
		    iconImageURL,
		    row,
		    iconTable.tableView_->findCol(DesktopIconTable::COL_IMAGE_URL));
		// set window URL value
		iconTable.tableView_->setURIEncodedValue(
		    iconWindowURL,
		    row,
		    iconTable.tableView_->findCol(DesktopIconTable::COL_WINDOW_CONTENT_URL));
		// set folder value
		iconTable.tableView_->setURIEncodedValue(
		    iconFolderPath,
		    row,
		    iconTable.tableView_->findCol(DesktopIconTable::COL_FOLDER_PATH));

		// create link to icon app
		if(windowLinkedAppLID > 0)
		{
			__COUTV__(windowLinkedAppLID);

			int appRow = appTable.tableView_->findRow(
			    appTable.tableView_->findCol(XDAQContextTable::colApplication_.colId_),
			    windowLinkedAppLID);
			windowLinkedApp =
			    appTable.tableView_
			        ->getDataView()[appRow][appTable.tableView_->getColUID()];
			__COUT__ << "Found app by LID: " << windowLinkedApp << __E__;
		}  // end linked app LID handling

		if(windowLinkedApp != "" && windowLinkedApp != "undefined" &&
		   windowLinkedApp != TableViewColumnInfo::DATATYPE_STRING_DEFAULT)
		{
			// first check that UID exists
			//	if not, interpret as app class type and
			//	check for unique 'enabled' app with class type
			__COUTV__(windowLinkedApp);

			if(!windowLinkedAppLID)  // no need to check if LID lookup happened already
			{
				try
				{
					windowLinkedApp = StringMacros::decodeURIComponent(windowLinkedApp);
					/*	int appRow      = */ appTable.tableView_->findRow(
					    appTable.tableView_->getColUID(), windowLinkedApp);
				}
				catch(const std::runtime_error& e)
				{
					// attempt to treat like class, and take first match
					try
					{
						int appRow = appTable.tableView_->findRow(
						    appTable.tableView_->findCol(
						        XDAQContextTable::colApplication_.colClass_),
						    windowLinkedApp);
						windowLinkedApp =
						    appTable.tableView_
						        ->getDataView()[appRow][appTable.tableView_->getColUID()];
					}
					catch(...)
					{
						// failed to treat like class, so throw original
						__SS__
						    << "Failed to create an icon linking to XDAQ Supervisor app '"
						    << windowLinkedApp
						    << ".' Please make sure the Supervisor exists in the "
						    << ConfigurationManager::XDAQ_APPLICATION_TABLE_NAME
						    << " table. "
						    << "\n\nThe following error occurred: " << e.what() << __E__;
						appTable.tableView_->print(ss);
						__SS_THROW__;
					}
				}
			}
			__COUTV__(windowLinkedApp);

			iconTable.tableView_->setValueAsString(
			    ConfigurationManager::XDAQ_APPLICATION_TABLE_NAME,
			    row,
			    iconTable.tableView_->findCol(DesktopIconTable::COL_APP_LINK));
			iconTable.tableView_->setValueAsString(
			    windowLinkedApp,
			    row,
			    iconTable.tableView_->findCol(DesktopIconTable::COL_APP_LINK_UID));
		}  // end create app link

		// parse parameters
		std::map<std::string, std::string> parameters;

		__COUTV__(windowParameters);
		StringMacros::getMapFromString(windowParameters, parameters);

		// create link to icon parameters
		if(parameters.size())
		{
			// set parameter link table
			iconTable.tableView_->setValueAsString(
			    DesktopIconTable::PARAMETER_TABLE,
			    row,
			    iconTable.tableView_->findCol(DesktopIconTable::COL_PARAMETER_LINK));
			// set parameter link Group ID
			iconTable.tableView_->setValueAsString(
			    iconUID + "_Parameters",
			    row,
			    iconTable.tableView_->findCol(DesktopIconTable::COL_PARAMETER_LINK_GID));

			__COUTV__(StringMacros::mapToString(parameters));

			unsigned int gidCol =
			    parameterTable.tableView_->findCol(DesktopIconTable::COL_PARAMETER_GID);

			// remove all existing records from groupID (e.g. parameters leftover from manual manipulations)
			std::vector<unsigned int /*row*/> rowsInGroup =
			    parameterTable.tableView_->getGroupRows(
			        gidCol, iconUID + "_Parameters" /*groupID*/);

			__COUTV__(StringMacros::vectorToString(rowsInGroup));

			// go through vector backwards to maintain row integrity
			for(unsigned int r = rowsInGroup.size() - 1; r < rowsInGroup.size(); --r)
				parameterTable.tableView_->removeRowFromGroup(
				    rowsInGroup[r],
				    gidCol,
				    iconUID + "_Parameters" /*groupID*/,
				    true /*deleteRowIfNoGroupLeft*/);

			// create new parameters
			for(const auto& parameter : parameters)
			{
				// create parameter record
				row = parameterTable.tableView_->addRow(
				    author, true /*incrementUniqueData*/, "generatedParameter");

				// set parameter status true
				parameterTable.tableView_->setValueAsString(
				    "1", row, parameterTable.tableView_->getColStatus());
				// set parameter Group ID
				parameterTable.tableView_->setValueAsString(
				    iconUID + "_Parameters", row, gidCol);
				// set parameter key
				parameterTable.tableView_->setURIEncodedValue(
				    parameter.first,
				    row,
				    parameterTable.tableView_->findCol(
				        DesktopIconTable::COL_PARAMETER_KEY));
				// set parameter value
				parameterTable.tableView_->setURIEncodedValue(
				    parameter.second,
				    row,
				    parameterTable.tableView_->findCol(
				        DesktopIconTable::COL_PARAMETER_VALUE));
			}  // end parameter loop

			std::stringstream ss;
			parameterTable.tableView_->print(ss);
			__COUT__ << ss.str();

			parameterTable.tableView_
			    ->init();  // verify new table (throws runtime_errors)

		}  // end create parameters link

		std::stringstream ss;
		iconTable.tableView_->print(ss);
		__COUT__ << ss.str();

		iconTable.tableView_->init();  // verify new table (throws runtime_errors)
	}
	catch(...)
	{
		__COUT__ << "Icon table errors while saving. Erasing all newly "
		            "created table versions."
		         << __E__;

		throw;  // re-throw
	}           // end catch

	__COUT__ << "Edits complete for new desktop icon, now making persistent tables."
	         << __E__;

	// all edits are complete and tables verified

	// Remaining steps:
	//	save tables
	//	save new context group and activate it
	//	check for aliases ...
	//		if tables aliased.. update table aliases in backbone
	//		if context group aliased, update group aliases in backbone
	//	if backbone modified, save group and activate it

	TableGroupKey newContextKey;
	bool          foundEquivalentContextKey;
	TableGroupKey newBackboneKey;
	bool          foundEquivalentBackboneKey;

	contextGroupEdit.saveChanges(contextGroupEdit.originalGroupName_,
	                             newContextKey,
	                             &foundEquivalentContextKey,
	                             true /*activateNewGroup*/,
	                             true /*updateGroupAliases*/,
	                             true /*updateTableAliases*/,
	                             &newBackboneKey,
	                             &foundEquivalentBackboneKey);

	xmlOut.addTextElementToData("contextGroupName", contextGroupEdit.originalGroupName_);
	xmlOut.addTextElementToData("contextGroupKey", newContextKey.toString());

	xmlOut.addTextElementToData("backboneGroupName", backboneGroupName);
	xmlOut.addTextElementToData("backboneGroupKey", newBackboneKey.toString());

	// always add active table groups to xml response
	ConfigurationSupervisorBase::getConfigurationStatusXML(xmlOut, cfgMgr, author);

	return true;
}  // end handleAddDesktopIconXML()
catch(std::runtime_error& e)
{
	__SS__ << "Error adding Desktop Icon!\n\n " << e.what() << __E__;
	__COUT__ << "\n" << ss.str() << __E__;
	xmlOut.addTextElementToData("Error", ss.str());
	return false;
}
catch(...)
{
	__SS__ << "Error adding Desktop Icon!" << __E__;
	try
	{
		throw;
	}  //one more try to printout extra info
	catch(const std::exception& e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...)
	{
	}
	__COUT__ << "\n" << ss.str() << __E__;
	xmlOut.addTextElementToData("Error", ss.str());
	return false;
}  // end handleAddDesktopIconXML() catch

//==============================================================================
void ConfigurationSupervisorBase::recursiveCopyTreeUIDNode(
    HttpXmlDocument&        xmlOut,
    ConfigurationManagerRW* cfgMgr,
    std::map<std::string /*modified table*/, TableVersion /* modified version */>&
                       modifiedTablesMap,
    const unsigned int startingDepth,
    const unsigned int depth,
    const unsigned int numberOfInstances,
    TableView*         cfgView,
    const std::string& uidToCopy)
try
{
	__COUTV__(startingDepth);
	__COUTV__(depth);
	__COUTV__(numberOfInstances);

	// throw std::runtime_error("hello");

	// Steps:
	//	Assume temporary table version already created correctly for recursive level
	//	Assume already decided it is correct to copy record at row parameter
	//	Assume after modifications the version saving is handled above this function call
	//	1. copy the target row
	//	- if depth,
	//		2. - check source row, for secondary copies through links
	//		- if a link is found, check that there is not unanimous pointing by siblings
	//			-- if unanimous pointing by siblings, do not do secondary copy, just point
	//			3. -- if not unanimous,
	//				* use/create temporary version of child table
	//				* for each instance
	//					- recursive secondary copy (depth-1)
	//				* save child table
	//

	// Step 1. copy the target row
	unsigned int col = cfgView->getColUID();
	unsigned int row = cfgView->findRow(col, uidToCopy);

	__COUT__ << "Copying " << cfgView->getTableName() << " v" << cfgView->getVersion()
	         << " row=" << row << " record=" << uidToCopy
	         << " instances=" << numberOfInstances << __E__;

	cfgView->print();
	// for(unsigned int i = 0; i < numberOfInstances; ++i)
	cfgView->copyRows(cfgMgr->getUsername(),
	                  *cfgView /*source table*/,
	                  row,
	                  1 /*srcRowsToCopy*/,
	                  -1 /*destOffsetRow*/,
	                  true /*generateUniqueDataColumns*/,
	                  uidToCopy /*baseNameAutoUID*/);  // make the name similar

	// if no secondary copies, done now
	// check for secondary copies
	return;

	// secondary table copies
	std::string  tableName = "secondary";
	TableVersion version(modifiedTablesMap.at(tableName));

	TableBase* table = cfgMgr->getTableByName(tableName);
	try
	{
		table->setActiveView(version);
	}
	catch(...)
	{
		if(version.isTemporaryVersion())
			throw;  // if temporary, there is no hope to find lost version

		__COUT__ << "Failed to find stored version, so attempting to "
		            "load version: "
		         << tableName << " v" << version << __E__;
		cfgMgr->getVersionedTableByName(tableName, version);
	}

	__COUT__ << tableName << " active version is " << table->getViewVersion() << __E__;

	if(version != table->getViewVersion())
	{
		__SS__ << "Target table version (" << version
		       << ") is not the currently active version (" << table->getViewVersion()
		       << "). Try refreshing the tree." << __E__;
		__SS_THROW__;
	}

	// version handling:
	//	always make a new temporary-version from source-version
	//	edit temporary-version
	//		if edit fails
	//			delete temporary-version
	//		else
	//			return new temporary-version
	//			if source-version was temporary
	//				then delete source-version
	TableVersion temporaryVersion = table->createTemporaryView(version);

	__COUT__ << "Created temporary version " << temporaryVersion << __E__;

	cfgView = table->getTemporaryView(temporaryVersion);
	cfgView->init();  // prepare maps

	try  // while editing
	{
		// edit...

		cfgView->init();  // verify new table (throws runtime_errors)
	}
	catch(...)  // erase temporary view before re-throwing error
	{
		__COUT__ << "Caught error while editing. Erasing temporary version." << __E__;
		table->eraseView(temporaryVersion);
		throw;
	}

	ConfigurationSupervisorBase::saveModifiedVersionXML(
	    xmlOut,
	    cfgMgr,
	    tableName,
	    version,
	    true /*make temporary*/,
	    table,
	    temporaryVersion,
	    true /*ignoreDuplicates*/);  // save temporary version properly
}
catch(std::runtime_error& e)
{
	__SS__ << ("Error copying tree target '" + uidToCopy + "' at depth " +
	           std::to_string(startingDepth - depth) + " in table '" +
	           cfgView->getTableName() + ".' " + std::string(e.what()))
	       << __E__;
	__COUT__ << "\n" << ss.str() << __E__;
	xmlOut.addTextElementToData("Error", ss.str());
}
catch(...)
{
	__SS__ << ("Error copying tree target '" + uidToCopy + "' at depth " +
	           std::to_string(startingDepth - depth) + " in table '" +
	           cfgView->getTableName() + ".' ")
	       << __E__;
	try
	{
		throw;
	}  //one more try to printout extra info
	catch(const std::exception& e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...)
	{
	}
	__COUT__ << "\n" << ss.str() << __E__;
	xmlOut.addTextElementToData("Error", ss.str());
}  // end recursiveCopyTreeUIDNode
