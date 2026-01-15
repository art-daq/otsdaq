#include "otsdaq/ConfigurationInterface/Database_configInterface.h"
#include "otsdaq/Macros/ConfigurationInterfacePluginMacros.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <algorithm>
#include <iostream>
#include <iterator>
#include <string>

#include "artdaq-database/BasicTypes/basictypes.h"
#include "artdaq-database/ConfigurationDB/configurationdbifc.h"
#include "otsdaq/TableCore/TableBase.h"

#include "artdaq-database/ConfigurationDB/configuration_common.h"
#include "artdaq-database/ConfigurationDB/dispatch_common.h"
#include "artdaq-database/StorageProviders/FileSystemDB/provider_filedb.h"
#include "artdaq-database/StorageProviders/FileSystemDB/provider_filedb_index.h"

//artdaq database may set TRACE_NAME
#ifdef TRACE_NAME
#undef TRACE_NAME
#endif
#define TRACE_NAME __MF_DECOR__

using namespace ots;

using artdaq::database::basictypes::FhiclData;
using artdaq::database::basictypes::JsonData;

using ots::DatabaseConfigurationInterface;
using table_version_map_t = ots::DatabaseConfigurationInterface::table_version_map_t;

namespace db            = artdaq::database::configuration;
using VersionInfoList_t = db::ConfigurationInterface::VersionInfoList_t;

constexpr auto default_dbprovider = "filesystem";
constexpr auto default_entity     = "OTSROOT";

//==============================================================================
DatabaseConfigurationInterface::DatabaseConfigurationInterface()
{
#ifdef ARTDAQ_DATABASE_DEBUG_ENABLE
	// to enable debugging
	{
		artdaq::database::configuration::debug::ExportImport();
		artdaq::database::configuration::debug::ManageAliases();
		artdaq::database::configuration::debug::ManageConfigs();
		artdaq::database::configuration::debug::ManageDocuments();
		artdaq::database::configuration::debug::Metadata();

		artdaq::database::configuration::debug::detail::ExportImport();
		artdaq::database::configuration::debug::detail::ManageAliases();
		artdaq::database::configuration::debug::detail::ManageConfigs();
		artdaq::database::configuration::debug::detail::ManageDocuments();
		artdaq::database::configuration::debug::detail::Metadata();

		artdaq::database::configuration::debug::options::OperationBase();
		artdaq::database::configuration::debug::options::BulkOperations();
		artdaq::database::configuration::debug::options::ManageDocuments();
		artdaq::database::configuration::debug::options::ManageConfigs();
		artdaq::database::configuration::debug::options::ManageAliases();

		artdaq::database::configuration::debug::MongoDB();
		artdaq::database::configuration::debug::UconDB();
		artdaq::database::configuration::debug::FileSystemDB();

		artdaq::database::filesystem::index::debug::enable();

		// THIS TURNS OFF TRACE SLOW PATH!!! (bug? Gennadiy says was trying to avoid slowing down TRACE with too many messages on slow path)
		artdaq::database::filesystem::debug::enable();

		// artdaq::database::mongo::debug::enable();

		// artdaq::database::docrecord::debug::JSONDocumentBuilder();
		// artdaq::database::docrecord::debug::JSONDocument();

		// debug::registerUngracefullExitHandlers();
		//  artdaq::database::useFakeTime(true);
		artdaq::database::configuration::Multitasker();
		TRACE_CNTL("modeS", true);  //TURN BACK ON TRACE SLOW PATH
	}
#endif

	std::string envVar = __ENV__("ARTDAQ_DATABASE_URI");
	if(envVar.length() &&
	   envVar[0] != 'f')  //e.g., filesystemdb:///path/filesystemdb/test_db
		IS_FILESYSTEM_DB = false;
	else
		IS_FILESYSTEM_DB = true;
	__COUTV__(IS_FILESYSTEM_DB);
}  //end constructor()

//==============================================================================
/// read table from database
/// version = -1 means latest version
void DatabaseConfigurationInterface::fill(TableBase* table, TableVersion version) const
{
	auto start = std::chrono::high_resolution_clock::now();

	auto ifc = db::ConfigurationInterface{default_dbprovider};

	auto versionstring = version.toString();

	auto result = ifc.template loadVersion<decltype(table), JsonData>(
	    table, versionstring, default_entity);

	auto end = std::chrono::high_resolution_clock::now();
	auto duration =
	    std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	__COUTT__ << "Time taken to call DatabaseConfigurationInterface::fill(tableName="
	          << table->getTableName() << ", version=" << versionstring << ") "
	          << duration << " milliseconds." << std::endl;

	if(result.first)
	{
		// make sure version is set.. not clear it was happening in loadVersion
		table->getViewP()->setVersion(version);
		return;
	}
	if(result.second.find("failed to create a client session") != std::string::npos ||
	   result.second.find("closed connection. calling hello") != std::string::npos)
	{
		__SS__ << "Error at time: " << time(0)
		       << "\n\n======> Database Interface Error while filling '"
		       << table->getTableName() << "' version '" << versionstring
		       << "' - it appears that the connection to the database been lost. Please "
		          "check the database server and route to server.\n\n"
		       << "Here is the error detail:\n\n"
		       << result.second << "\n\n"
		       << StringMacros::stackTrace() << __E__;
		__SS_ONLY_THROW__;
	}

	__SS__ << "\n\n======> Database Interface Error while filling '"
	       << table->getTableName() << "' version '" << versionstring
	       << "' - are you sure this version exists? Or has the connection to the "
	          "database been lost?\n\n"
	       << "Here is the error detail:\n\n"
	       << result.second << __E__;
	__SS_ONLY_THROW__;
}  // end fill()

//==============================================================================
/// write table to database
void DatabaseConfigurationInterface::saveActiveVersion(const TableBase* table,
                                                       bool             overwrite) const
try
{
	auto start = std::chrono::high_resolution_clock::now();

	auto ifc = db::ConfigurationInterface{default_dbprovider};

	auto versionstring = table->getView().getVersion().toString();
	__COUTTV__(versionstring);
	std::stringstream preSaveJSONss;
	table->getView().printJSON(preSaveJSONss);

	// auto result =
	//	ifc.template storeVersion<decltype(configuration), JsonData>(configuration,
	// versionstring, default_entity);
	auto result = overwrite ? ifc.template overwriteVersion<decltype(table), JsonData>(
	                              table, versionstring, default_entity)
	                        : ifc.template storeVersion<decltype(table), JsonData>(
	                              table, versionstring, default_entity);

	auto end = std::chrono::high_resolution_clock::now();
	auto duration =
	    std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	__COUTT__ << "Time taken to call "
	             "DatabaseConfigurationInterface::saveActiveVersion(tableName="
	          << table->getTableName() << ", versionstring=" << versionstring
	          << " overwrite=" << overwrite << ") " << duration << " milliseconds"
	          << std::endl;

	__COUTTV__(result.first);
	__COUTVS__(10, result.second);

	if(result.first)
	{
		//removing readback check - it seems collision checking was fixed by https://github.com/art-daq/artdaq-database/pull/36
		if(0)
		{  //check that table save worked (FIXME -- this is temporary while waiting for permanent solution from artdaq-database developments)

			TableBase localDocLoader(
			    table->getTableName());  //can not use special table when filling
			localDocLoader.changeVersionAndActivateView(
			    localDocLoader.createTemporaryView(), table->getView().getVersion());
			fill(&localDocLoader, table->getView().getVersion());

			std::stringstream postSaveJSONss;
			localDocLoader.getView().printJSON(postSaveJSONss);

			__COUTVS__(2, preSaveJSONss.str());
			__COUTVS__(2, postSaveJSONss.str());
			bool same = true;
			//compare and ignore white space (since json might shift)
			{
				auto   preSaveJSON  = preSaveJSONss.str();
				auto   postSaveJSON = postSaveJSONss.str();
				size_t prec = 0, postc = 0;
				for(; prec < preSaveJSON.size() && postc < postSaveJSON.size();
				    ++prec, ++postc)
				{
					if(preSaveJSON[prec] == '\n' || preSaveJSON[prec] == '\t' ||
					   preSaveJSON[prec] == ' ')
					{
						//advance only prec to skip whitespace
						--postc;
						continue;
					}
					else if(postSaveJSON[postc] == '\n' || postSaveJSON[postc] == '\t' ||
					        postSaveJSON[postc] == ' ')
					{
						//advance only postc to skip whitespace
						--prec;
						continue;
					}
					if(preSaveJSON[prec] != postSaveJSON[postc])
					{
						__COUTT__ << "Mismatch at preSaveJSON[" << prec
						          << "] != postSaveJSON[" << postc << "] ... "
						          << preSaveJSON.substr(prec, 30)
						          << " != " << postSaveJSON.substr(postc, 30) << __E__;
						same = false;
						break;
					}
				}
				//Note: is ok to not consider the case when prec != postc
				//	because json must have a closing bracket and must have been matched to be same.
			}

			if(same)
				__COUTT__ << "Same";
			else
			{
				__COUT__ << "NOT Same";
				auto end = std::chrono::high_resolution_clock::now();
				auto duration =
				    std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
				        .count();
				__COUTT__
				    << "Time taken to call "
				       "DatabaseConfigurationInterface::saveActiveVersion(tableName="
				    << table->getTableName() << ", versionstring=" << versionstring
				    << ") " << duration << " milliseconds" << std::endl;

				__SS__ << "Error saving table '" << table->getTableName() << "'-v"
				       << versionstring
				       << " (perhaps there was a collision with another user saving the "
				          "same table name/version?! Please try again with an "
				          "incremented table version). "
				       << "Expected data size is " << preSaveJSONss.str().size()
				       << " and readback found size of " << postSaveJSONss.str().size()
				       << " with character mismatches." << __E__;
				__SS_THROW__;
			}

			auto end = std::chrono::high_resolution_clock::now();
			auto duration =
			    std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
			        .count();
			__COUTT__ << "Time taken to call "
			             "DatabaseConfigurationInterface::saveActiveVersion(tableName="
			          << table->getTableName() << ", versionstring=" << versionstring
			          << ") " << duration << " milliseconds" << std::endl;

		}  //end check that table save worked
		return;
	}

	__SS__ << "Return value indicates error in Database Interface saveActiveVersion "
	          "attempting to save "
	       << table->getTableName() << "-v" << table->getView().getVersion().toString()
	       << ": " << result.second << __E__;
	__SS_THROW__;
}  //end saveActiveVersion()
catch(std::exception const& e)
{
	__SS__ << "Database Interface Exception in saveActiveVersion attempting to save "
	       << table->getTableName() << "-v" << table->getView().getVersion().toString()
	       << ": " << e.what() << __E__;
	__SS_THROW__;
}  //end saveActiveVersion() catch
catch(...)
{
	__SS__
	    << "Database Interface Unknown exception in saveActiveVersion attempting to save "
	    << table->getTableName() << "-v" << table->getView().getVersion().toString()
	    << "." << __E__;
	__SS_THROW__;
}  //end saveActiveVersion() catch

//==============================================================================
/// find the latest configuration version by configuration type
TableVersion DatabaseConfigurationInterface::findLatestVersion(
    const TableBase* table) const noexcept
{
	auto versions = getVersions(table);

	if(TTEST(1))
	{
		__COUTT__ << "Table Name: " << table->getTableName() << __E__;
		__SS__ << "All Versions: ";
		for(auto& v : versions)
			ss << v << " ";
		ss << __E__;
		__COUTT__ << "\n" << ss.str();
	}

	if(!versions.size())
		return TableVersion();  // return INVALID

	return *(versions.rbegin());
}  //end findLatestVersion()

//==============================================================================
/// find all configuration versions by configuration type
std::set<TableVersion> DatabaseConfigurationInterface::getVersions(
    const TableBase* table) const noexcept
try
{
	auto start = std::chrono::high_resolution_clock::now();

	auto ifc    = db::ConfigurationInterface{default_dbprovider};
	auto result = ifc.template getVersions<decltype(table)>(table, default_entity);

	auto end = std::chrono::high_resolution_clock::now();
	auto duration =
	    std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	__COUTT__
	    << "Time taken to call DatabaseConfigurationInterface::getVersions(tableName="
	    << table->getTableName() << ") " << duration << " milliseconds." << std::endl;

	auto resultSet = std::set<TableVersion>{};
	for(std::string const& version : result)
		resultSet.insert(TableVersion(std::stol(version, 0, 10)));

	__COUTVS__(10, StringMacros::setToString(resultSet));

	return resultSet;
}  //end getVersions()
catch(std::exception const& e)
{
	__COUT_WARN__ << "Database Interface Exception:" << e.what() << "\n";
	return {};
}  //end getVersions() catch

//==============================================================================
/// returns a list of all configuration names
std::set<std::string /*name*/> DatabaseConfigurationInterface::getAllTableNames() const
try
{
	auto start = std::chrono::high_resolution_clock::now();

	auto ifc                    = db::ConfigurationInterface{default_dbprovider};
	auto collection_name_prefix = std::string{};

	auto result = ifc.listCollections(collection_name_prefix);

	auto end = std::chrono::high_resolution_clock::now();
	auto duration =
	    std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	__COUTT__
	    << "Time taken to call "
	       "DatabaseConfigurationInterface::getAllTableNames(collection_name_prefix="
	    << collection_name_prefix << ") " << duration << " milliseconds." << std::endl;

	return result;
}  //end getAllTableNames()
catch(std::exception const& e)
{
	__SS__ << "Database Interface Exception:" << e.what() << "\n";
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "Database Interface Unknown exception.\n";
	__SS_THROW__;
}  //end getAllTableNames() catch

//==============================================================================
/// find all configuration groups in database
std::set<std::string /*name*/> DatabaseConfigurationInterface::getAllTableGroupNames(
    std::string const& filterString) const
try
{
	auto start = std::chrono::high_resolution_clock::now();

	auto ifc = db::ConfigurationInterface{default_dbprovider};

	auto result = std::set<std::string>();

	if(filterString == "")
		result = ifc.findGlobalConfigurations("*");  // GConfig will return all GConfig*
		                                             // with filesystem db.. for mongodb
		                                             // would require reg expr
	else
		result = ifc.findGlobalConfigurations(filterString + "*");  // GConfig will return
		                                                            // all GConfig* with
		                                                            // filesystem db.. for
	// mongodb would require
	// reg expr
	auto end = std::chrono::high_resolution_clock::now();
	auto duration =
	    std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	__COUTT__ << "Time taken to call "
	             "DatabaseConfigurationInterface::getAllTableGroupNames(filterString="
	          << filterString << ") " << duration << " milliseconds." << std::endl;

	return result;
}  //end getAllTableGroupNames()
catch(std::exception const& e)
{
	__SS__ << "Filter string '" << filterString
	       << "' yielded Database Interface Exception:" << e.what() << "\n";
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "Filter string '" << filterString
	       << "' yielded Database Interface Unknown exception.\n";
	__SS_THROW__;
}  //end getAllTableGroupNames() catch

//==============================================================================
/// find the latest configuration group key by group name
/// 	if not found, return invalid
TableGroupKey DatabaseConfigurationInterface::findLatestGroupKey(
    const std::string& groupName) const noexcept
{
	//attempt to use cache first! (potentially way faster .04 s vs 4 s)
	try
	{
		TableBase localGroupMemberCacheLoader(
		    true /*special table*/
		    ,  //special table only allows 1 view in cache and does not load schema (which is perfect for this temporary table),,
		    TableBase::GROUP_CACHE_PREPEND + groupName);
		TableVersion lastestGroupCacheKey =
		    findLatestVersion(&localGroupMemberCacheLoader);
		__COUTTV__(lastestGroupCacheKey);
		if(!lastestGroupCacheKey.isInvalid())
			return TableGroupKey(lastestGroupCacheKey.version());
	}
	catch(...)
	{
		__COUT__ << "Ignoring cache loading error." << __E__;
	}

	std::set<TableGroupKey> keys = DatabaseConfigurationInterface::getKeys(groupName);
	if(keys.size())  // if keys exist, return the last
		return *(keys.crbegin());

	// else, return invalid
	return TableGroupKey();
}  //end findLatestGroupKey()

//==============================================================================
/// find all configuration groups in database
std::set<TableGroupKey /*key*/> DatabaseConfigurationInterface::getKeys(
    const std::string& groupName) const
{
	std::set<TableGroupKey>        retSet;
	std::set<std::string /*name*/> names = getAllTableGroupNames();
	for(auto& n : names)
		if(n.find(groupName) == 0)
			retSet.insert(TableGroupKey(n));
	return retSet;
}  // end getKeys()

//==============================================================================
/// return the contents of a configuration group
table_version_map_t DatabaseConfigurationInterface::getTableGroupMembers(
    std::string const& tableGroup, bool includeMetaDataTable /* = false */) const
try
{
	auto start = std::chrono::high_resolution_clock::now();

	//Flow (motivation -- getTableGroupMembers() is super slow; can be 3 to 15 seconds):
	//	when saveTableGroup() is called
	//		saveDocument (collection: "GroupCache" + tableGroup, version: groupKey)
	//			containing --> table group members
	//	when getTableGroupMembers() is called
	//		loadDocument (collection: "GroupCache" + tableGroup, version: groupKey)
	// 			if succeeds, use that
	//			else continue with db extended lookup
	//				AND create cache file (in this way slowly populating the cache even without saves)

	// format: groupName + "_v" + groupKey
	try
	{
		table_version_map_t retMap = getCachedTableGroupMembers(tableGroup);
		__COUTV__(tableGroup);
		__COUTS__(20) << (StringMacros::mapToString(retMap));

		if(!includeMetaDataTable)
		{
			// remove special meta data table from member map
			auto metaTable = retMap.find(TableBase::GROUP_METADATA_TABLE_NAME);
			if(metaTable != retMap.end())
				retMap.erase(metaTable);
		}

		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
		                    std::chrono::high_resolution_clock::now() - start)
		                    .count();
		__COUTT__ << "Time taken to call "
		             "DatabaseConfigurationInterface::getTableGroupMembers(tableGroup="
		          << tableGroup << ") " << duration << " milliseconds." << std::endl;
		return retMap;
	}
	catch(...)  //ignore error and proceed with standard db access
	{
		__COUTT__ << "Ignoring error "
		             "DatabaseConfigurationInterface::getTableGroupMembers(tableGroup="
		          << tableGroup << ") " << __E__;
	}

	auto ifc    = db::ConfigurationInterface{default_dbprovider};
	auto result = ifc.loadGlobalConfiguration(tableGroup);

	if(TTEST(1))
	{
		for(auto& item : result)
			__COUTT__ << "====================> " << item.configuration << ": "
			          << item.version << __E__;
	}

	auto to_map = [](auto const& inputList, bool includeMetaDataTable) {
		auto resultMap = table_version_map_t{};

		std::for_each(inputList.begin(), inputList.end(), [&resultMap](auto const& info) {
			resultMap[info.configuration] = std::stol(info.version, 0, 10);
		});

		if(!includeMetaDataTable)
		{
			// remove special meta data table from member map
			auto metaTable = resultMap.find(TableBase::GROUP_METADATA_TABLE_NAME);
			if(metaTable != resultMap.end())
				resultMap.erase(metaTable);
		}
		return resultMap;
	};

	table_version_map_t retMap = to_map(result, includeMetaDataTable);

	//now create cache for next time!
	saveTableGroupMemberCache(retMap, tableGroup);

	__COUTT__ << "Loaded db member map string " << StringMacros::mapToString(retMap)
	          << __E__;

	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
	                    std::chrono::high_resolution_clock::now() - start)
	                    .count();
	__COUTT__ << "Time taken to call "
	             "DatabaseConfigurationInterface::getTableGroupMembers(tableGroup="
	          << tableGroup << ") " << duration << " milliseconds." << std::endl;

	return retMap;
}  // end getTableGroupMembers()
catch(std::exception const& e)
{
	__SS__ << "Database Interface Exception getting Group's member tables for '"
	       << tableGroup << "':\n\n"
	       << e.what() << "\n";
	if(std::string(e.what()).find("connection refused") != std::string::npos)
	{
		ss << "\n\nConnection to database refused. Perhaps your ssh tunnel has "
		      "closed?\n\n";
	}
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "Database Interface Unknown exception getting Group's member tables for '"
	       << tableGroup << ".'\n";
	__COUT_ERR__ << ss.str();
	__SS_THROW__;
}  // end getTableGroupMembers() catch

//==============================================================================
/// get cached Table Group members
///	throw exception on failure or missing cache
table_version_map_t DatabaseConfigurationInterface::getCachedTableGroupMembers(
    std::string const& tableGroup) const
try
{
	table_version_map_t retMap;

	//Flow:
	//	when saveTableGroup() is called
	//		saveDocument (collection: "GroupCache_" + tableGroup, version: groupKey)
	//			containing --> table group members
	//	when getTableGroupMembers() is called
	//		loadDocument (collection: "GroupCache_" + tableGroup, version: groupKey)
	// 			if succeeds, use that
	//			else continue with db extended lookup
	//				AND create cache file (in this way slowly populating the cache even without saves)

	// tableGroup format: groupName + "_v" + groupKey

	std::size_t vi        = tableGroup.rfind("_v");
	std::string groupName = tableGroup.substr(0, vi);
	std::string groupKey  = tableGroup.substr(vi + 2);
	__COUTT__ << "Getting cache for " << groupName << "(" << groupKey << ")" << __E__;

	TableBase localGroupMemberCacheSaver(
	    true /*special table*/
	    ,  //special table only allows 1 view in cache and does not load schema (which is perfect for this temporary table),
	    TableBase::GROUP_CACHE_PREPEND + groupName);
	TableVersion localVersion(atoi(groupKey.c_str()));

	//if filesystem db, as of April 2024, artdaq_database returned latest version when version is missing...
	if(IS_FILESYSTEM_DB)
	{
		__COUTT__ << "IS_FILESYSTEM_DB=true, so checking cached keys for " << groupName
		          << "(" << groupKey << ")" << __E__;
		std::set<TableVersion> versions = getVersions(&localGroupMemberCacheSaver);
		if(versions.find(localVersion) == versions.end())
		{
			__SS__ << "Cached member table versions not found for " << groupName << "("
			       << groupKey << ")" << __E__;
			__SS_THROW__;
		}
	}

	localGroupMemberCacheSaver.changeVersionAndActivateView(
	    localGroupMemberCacheSaver.createTemporaryView(), localVersion);

	fill(&localGroupMemberCacheSaver, localVersion);

	__COUTS__(20) << "Loaded cache member map string "
	              << localGroupMemberCacheSaver.getViewP()->getCustomStorageData()
	              << __E__;

	{  //get table member map from cleaned json string
		//remove json { } and all " characters
		std::string        jsonClean = "";
		const std::string& json =
		    localGroupMemberCacheSaver.getViewP()->getCustomStorageData();
		for(auto& c : json)
			if(c == '{' || c == '}' || c == '"' || c == ' ')
				continue;
			else
				jsonClean += c;
		__COUTVS__(21, jsonClean);
		StringMacros::getMapFromString(jsonClean, retMap);
	}

	__COUTS__(20) << "Loaded cache member map string "
	              << StringMacros::mapToString(retMap) << __E__;

	return retMap;
}  //end getCachedTableGroupMembers()
catch(std::exception const& e)
{
	__SS__ << "Database Interface Exception getCachedTableGroupMembers for '"
	       << tableGroup << "':\n\n"
	       << e.what() << "\n";
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "Database Interface Unknown exception getCachedTableGroupMembers for '"
	       << tableGroup << ".'\n";
	__SS_THROW__;
}  //end getCachedTableGroupMembers() catch

//==============================================================================
/// create a new configuration group from the contents map
void DatabaseConfigurationInterface::saveTableGroupMemberCache(
    table_version_map_t const& memberMap, std::string const& tableGroup) const
try
{
	//Flow:
	//	when saveTableGroup() is called
	//		saveDocument (collection: "GroupCache_" + tableGroup, version: groupKey)
	//			containing --> table group members
	//	when getTableGroupMembers() is called
	//		loadDocument (collection: "GroupCache_" + tableGroup, version: groupKey)
	// 			if succeeds, use that
	//			else continue with db extended lookup
	//				AND create cache file (in this way slowly populating the cache even without saves)

	// tableGroup format: groupName + "_v" + groupKey

	std::size_t vi        = tableGroup.rfind("_v");
	std::string groupName = tableGroup.substr(0, vi);
	std::string groupKey  = tableGroup.substr(vi + 2);
	__COUTT__ << "Saving cache for " << groupName << "(" << groupKey << ")" << __E__;

	TableBase localGroupMemberCacheSaver(
	    true /*special table*/
	    ,  //special table only allows 1 view in cache and does not load schema (which is perfect for this temporary table),
	    TableBase::GROUP_CACHE_PREPEND + groupName);
	localGroupMemberCacheSaver.changeVersionAndActivateView(
	    localGroupMemberCacheSaver.createTemporaryView(),
	    TableVersion(atoi(groupKey.c_str())));

	{  //set custom storage data
		std::stringstream groupCacheData;
		groupCacheData << "{ ";
		for(const auto& member : memberMap)
			groupCacheData << (member.first == memberMap.begin()->first ? "" : ", ")
			               <<  //skip comma on first
			    "\"" << member.first << "\" : \"" << member.second << "\"";
		groupCacheData << "}";

		localGroupMemberCacheSaver.getViewP()->setCustomStorageData(groupCacheData.str());
	}  //end set custom storage data

	__COUTT__ << "Saving member map string "
	          << localGroupMemberCacheSaver.getViewP()->getCustomStorageData() << __E__;

	__COUTT__ << "Saving cache table "
	          << localGroupMemberCacheSaver.getView().getTableName() << "("
	          << localGroupMemberCacheSaver.getView().getVersion().toString() << ")"
	          << __E__;

	// save to db, and do not allow overwrite
	saveActiveVersion(&localGroupMemberCacheSaver, false /* overwrite */);

}  //end saveTableGroupMemberCache()
catch(std::exception const& e)
{
	__SS__ << "Database Interface Exception saveTableGroupMemberCache for '" << tableGroup
	       << "':\n\n"
	       << e.what() << "\n";
	__COUT_ERR__ << ss.str();
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "Database Interface Unknown exception saveTableGroupMemberCache for '"
	       << tableGroup << ".'\n";
	__COUT_ERR__ << ss.str();
	__SS_THROW__;
}  //end saveTableGroupMemberCache() catch

//==============================================================================
/// create a new configuration group from the contents map
void DatabaseConfigurationInterface::saveTableGroup(table_version_map_t const& memberMap,
                                                    std::string const& tableGroup) const
try
{
	if(memberMap.size() == 0)
	{
		__SS__ << "Error: Attempting to save table group '" << tableGroup
		       << "' with empty member map! Please provide at least one member table for "
		          "the group."
		       << __E__;
		__SS_THROW__;
	}

	auto start = std::chrono::high_resolution_clock::now();

	auto ifc = db::ConfigurationInterface{default_dbprovider};

	//======
	/// Lambda function to convert map to list
	auto to_list = [](auto const& inputMap) {
		auto resultList = VersionInfoList_t{};
		std::transform(
		    inputMap.begin(),
		    inputMap.end(),
		    std::back_inserter(resultList),
		    [](auto const& mapEntry) {
			    return VersionInfoList_t::value_type{
			        mapEntry.first, mapEntry.second.toString(), default_entity};
		    });

		return resultList;
	};
	__COUTTV__(StringMacros::mapToString(memberMap));

	auto result = IS_FILESYSTEM_DB
	                  ? ifc.storeGlobalConfiguration(to_list(memberMap), tableGroup)
	                  : ifc.storeGlobalConfiguration_mt(to_list(memberMap), tableGroup);

	__COUTTV__(result.first);
	__COUTVS__(10, result.second);

	auto end = std::chrono::high_resolution_clock::now();
	auto duration =
	    std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	__COUTT__
	    << "Time taken to call DatabaseConfigurationInterface::saveTableGroup(tableGroup="
	    << tableGroup << ") " << duration << " milliseconds." << std::endl;

	if(result.first)
	{
		if(0)  //removing readback check - it seems collision checking was fixed by https://github.com/art-daq/artdaq-database/pull/36
		{  //check that group save worked (FIXME -- this is temporary while waiting for permanent solution from artdaq-database developments)
			auto readbackResult = ifc.loadGlobalConfiguration(tableGroup);

			if(TTEST(1))
				for(auto& item : readbackResult)
					__COUTT__ << "--==> " << item.configuration << ": " << item.version
					          << __E__;

			size_t countOfMatches = 0;
			for(auto& item : readbackResult)
			{
				__COUTT__ << "====================> " << item.configuration << ": "
				          << item.version << __E__;
				const auto& it = memberMap.find(item.configuration);
				if(it == memberMap.end() ||
				   it->second != TableVersion(std::stol(item.version, 0, 10)))
				{
					auto end = std::chrono::high_resolution_clock::now();
					auto duration =
					    std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
					        .count();
					__COUTT__
					    << "Time taken to call "
					       "DatabaseConfigurationInterface::saveTableGroup(tableGroup="
					    << tableGroup << ") " << duration << " milliseconds."
					    << std::endl;
					__SS__ << "Error saving group '" << tableGroup
					       << "' (perhaps there was a collision with another user saving "
					          "the same group name?! Please try again with an "
					          "incremented group key)). Table '"
					       << item.configuration << "'-v" << item.version
					       << " was unexpectedly read back as a member table after the "
					          "attempted group save. Expected member tables of group '"
					       << tableGroup << "' are as follows:" << __E__;
					for(const auto& memberPair : memberMap)
						ss << "\t" << memberPair.first << "-v" << memberPair.second
						   << __E__;
					__SS_THROW__;
				}
				else
					++countOfMatches;
			}  //end individual member check

			//make sure all members were found
			if(countOfMatches != memberMap.size())
			{
				auto end = std::chrono::high_resolution_clock::now();
				auto duration =
				    std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
				        .count();
				__COUTT__ << "Time taken to call "
				             "DatabaseConfigurationInterface::saveTableGroup(tableGroup="
				          << tableGroup << ") " << duration << " milliseconds."
				          << std::endl;
				__SS__ << "Error saving group '" << tableGroup
				       << "' (perhaps there was a collision with another user saving the "
				          "same group name?! Please try again with an incremented group "
				          "key). "
				       << "Expected group count is " << memberMap.size() << ", and found "
				       << countOfMatches << " matching tables during readback check."
				       << __E__;
				__SS_THROW__;
			}
			__COUTT__ << "Readback check passed." << __E__;
		}  //end check that group save worked

		{
			auto end = std::chrono::high_resolution_clock::now();
			auto duration =
			    std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
			        .count();
			__COUTT__ << "Time taken to call "
			             "DatabaseConfigurationInterface::saveTableGroup(tableGroup="
			          << tableGroup << ") " << duration << " milliseconds." << std::endl;
		}

		//now save to db cache for reverse index lookup of group members
		try
		{
			saveTableGroupMemberCache(memberMap, tableGroup);
		}
		catch(...)
		{
			__COUT_WARN__ << "Ignoring errors during saveTableGroupMemberCache()"
			              << __E__;
		}

		{
			auto end = std::chrono::high_resolution_clock::now();
			auto duration =
			    std::chrono::duration_cast<std::chrono::milliseconds>(end - start)
			        .count();
			__COUTT__ << "Time taken to call "
			             "DatabaseConfigurationInterface::saveTableGroup(tableGroup="
			          << tableGroup << ") " << duration << " milliseconds." << std::endl;
		}

		return;
	}

	__SS__ << "Return value indicates failure to save group '" << tableGroup << "':\n"
	       << result.second << __E__;
	__SS_THROW__;
}  // end saveTableGroup()
catch(std::exception const& e)
{
	__SS__ << "Database Interface Exception saveTableGroup for '" << tableGroup
	       << "':\n\n"
	       << e.what() << "\n";
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "Database Interface Unknown exception saveTableGroup for '" << tableGroup
	       << ".'\n";
	__SS_THROW__;
}  //end saveTableGroup() catch

//==============================================================================
/// Save a json string as a document in the document database.
std::pair<std::string, TableVersion> DatabaseConfigurationInterface::saveCustomJSON(
    const std::string& json, const std::string& documentNameToSave) const
try
{
	__COUTT__ << "Saving doc '" << documentNameToSave << "'" << __E__;

	TableBase localDocSaver(
	    true /*special table*/
	    ,  //special table only allows 1 view in cache and does not load schema (which is perfect for this temporary table)
	    TableBase::JSON_DOC_PREPEND + documentNameToSave);

	std::set<TableVersion> versions = getVersions(&localDocSaver);
	TableVersion           version;
	if(versions.size())
		version = TableVersion::getNextVersion(*versions.rbegin());
	else
		version = TableVersion::DEFAULT;
	__COUTV__(version);

	localDocSaver.changeVersionAndActivateView(localDocSaver.createTemporaryView(),
	                                           version);

	localDocSaver.getViewP()->setCustomStorageData(json);

	__COUTS__(10) << "Saving JSON string: "
	              << localDocSaver.getViewP()->getCustomStorageData() << __E__;

	__COUTT__ << "Saving JSON doc as " << localDocSaver.getView().getTableName() << "("
	          << localDocSaver.getView().getVersion().toString() << ")" << __E__;

	// save to db, and do not allow overwrite
	saveActiveVersion(&localDocSaver, false /* overwrite */);

	return std::make_pair(localDocSaver.getTableName(),
	                      localDocSaver.getView().getVersion());
}  //end saveCustomJSON()
catch(std::exception const& e)
{
	__SS__ << "Database Interface Exception saveCustomJSON for '" << documentNameToSave
	       << "':\n\n"
	       << e.what() << "\n";
	__COUT_ERR__ << ss.str();
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "Database Interface Unknown exception saveCustomJSON for '"
	       << documentNameToSave << ".'\n";
	__COUT_ERR__ << ss.str();
	__SS_THROW__;
}  //end saveCustomJSON() catch

//==============================================================================
/// Load a document in the document database and return content as a json string
std::string DatabaseConfigurationInterface::loadCustomJSON(
    const std::string& documentNameToLoad, TableVersion documentVersionToLoad) const
try
{
	__COUTT__ << "Loading doc '" << documentNameToLoad << "-v" << documentVersionToLoad
	          << "'" << __E__;

	TableBase localDocLoader(
	    true /*special table*/
	    ,  //special table only allows 1 view in cache and does not load schema (which is perfect for this temporary table),
	    TableBase::JSON_DOC_PREPEND + documentNameToLoad);

	localDocLoader.changeVersionAndActivateView(localDocLoader.createTemporaryView(),
	                                            documentVersionToLoad);

	fill(&localDocLoader, documentVersionToLoad);

	__COUTS__(10) << "Loaded JSON doc string "
	              << localDocLoader.getViewP()->getCustomStorageData() << __E__;

	return localDocLoader.getViewP()->getCustomStorageData();
}  //end loadCustomJSON()
catch(std::exception const& e)
{
	__SS__ << "Database Interface Exception saveCustomJSON for '" << documentNameToLoad
	       << "-v" << documentVersionToLoad << "':\n\n"
	       << e.what() << "\n";
	__COUTS__(3) << ss.str();
	__SS_ONLY_THROW__;
}
catch(...)
{
	__SS__ << "Database Interface Unknown exception saveCustomJSON for '"
	       << documentNameToLoad << "-v" << documentVersionToLoad << ".'\n";
	__COUTS__(3) << ss.str();
	__SS_ONLY_THROW__;
}  //end loadCustomJSON() catch

//==============================================================================
/// findGroupsWithTable() returns the set of table groups that contain the specified table
/// name and version
std::set<std::string /*group*/> DatabaseConfigurationInterface::findGroupsWithTable(
    std::string const& tableName, TableVersion version) const
try
{
	auto start = std::chrono::high_resolution_clock::now();

	auto ifc = db::ConfigurationInterface{default_dbprovider};

	std::set<std::string> returnSet;  // =
	    // ifc.findGlobalConfigurationsContaining(tableName, version.toString());

	__COUTT__ << "Number of Groups containing table '" << tableName << "-v" << version
	          << "': " << returnSet.size() << __E__;

	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
	                    std::chrono::high_resolution_clock::now() - start)
	                    .count();
	__COUTT__ << "Time taken to call "
	             "DatabaseConfigurationInterface::findGroupsWithTable(table="
	          << tableName << "-v" << version << ") " << duration << " milliseconds."
	          << std::endl;

	return returnSet;
}  //end findGroupsWithTable()
catch(std::exception const& e)
{
	__SS__ << "Database Interface Exception running findGroupsWithTable for '"
	       << tableName << "-v" << version << "':\n\n"
	       << e.what() << "\n";
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "Database Interface Unknown exception running findGroupsWithTable for '"
	       << tableName << "-v" << version << ".'\n";
	__SS_THROW__;
}  //end findGroupsWithTable() catch

DEFINE_OTS_CONFIGURATION_INTERFACE(DatabaseConfigurationInterface)
