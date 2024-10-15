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

		// artdaq::database::filesystem::index::debug::enable();

		// THIS TURNS OFF TRACE SLOW PATH!!! (bug? Gennadiy says was trying to avoid slowing down TRACE with too many messages on slow path) 
		artdaq::database::filesystem::debug::enable();

		// artdaq::database::mongo::debug::enable();

		// artdaq::database::docrecord::debug::JSONDocumentBuilder();
		// artdaq::database::docrecord::debug::JSONDocument();

		// debug::registerUngracefullExitHandlers();
		//  artdaq::database::useFakeTime(true);
		artdaq::database::configuration::Multitasker();
		TRACE_CNTL("modeS", true); //TURN BACK ON TRACE SLOW PATH
	}
#endif

	std::string envVar = __ENV__("ARTDAQ_DATABASE_URI");
	if(envVar.length() && envVar[0] != 'f')  //e.g., filesystemdb:///path/filesystemdb/test_db
		IS_FILESYSTEM_DB = false;
	else 
		IS_FILESYSTEM_DB = true;
	__COUTV__(IS_FILESYSTEM_DB);
} //end constructor()

//==============================================================================
// read table from database
// version = -1 means latest version
void DatabaseConfigurationInterface::fill(TableBase* table, TableVersion version) const

{
	auto start = std::chrono::high_resolution_clock::now();

	auto ifc = db::ConfigurationInterface{default_dbprovider};

	auto versionstring = version.toString();

	auto result = ifc.template loadVersion<decltype(table), JsonData>(table, versionstring, default_entity);

	auto end      = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	__COUTT__ << "Time taken to call DatabaseConfigurationInterface::fill(tableName=" << table->getTableName() << ", version=" << versionstring << ") "
	         << duration << " milliseconds." << std::endl;

	if(result.first)
	{
		// make sure version is set.. not clear it was happening in loadVersion
		table->getViewP()->setVersion(version);
		return;
	}
	__SS__ << "\n\nDBI Error while filling '" << table->getTableName() << "' version '" << versionstring << "' - are you sure this version exists?\n"
	       << "Here is the error:\n\n"
	       << result.second << __E__;
	__SS_ONLY_THROW__;
}  // end fill()

//==============================================================================
// write table to database
void DatabaseConfigurationInterface::saveActiveVersion(const TableBase* table, bool overwrite) const

{
	auto start = std::chrono::high_resolution_clock::now();

	auto ifc = db::ConfigurationInterface{default_dbprovider};

	auto versionstring = table->getView().getVersion().toString();
	//__COUT__ << "versionstring: " << versionstring << "\n";

	// auto result =
	//	ifc.template storeVersion<decltype(configuration), JsonData>(configuration,
	// versionstring, default_entity);
	auto result = overwrite ? ifc.template overwriteVersion<decltype(table), JsonData>(table, versionstring, default_entity)
	                        : ifc.template storeVersion<decltype(table), JsonData>(table, versionstring, default_entity);

	auto end      = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	__COUTT__ << "Time taken to call DatabaseConfigurationInterface::saveActiveVersion(tableName=" << table->getTableName()
	         << ", versionstring=" << versionstring << ") " << duration << " milliseconds" << std::endl;

	if(result.first)
		return;

	__SS__ << "DBI saveActiveVersion Error:" << result.second << __E__;
	__SS_THROW__;
} //end saveActiveVersion()

//==============================================================================
// find the latest configuration version by configuration type
TableVersion DatabaseConfigurationInterface::findLatestVersion(const TableBase* table) const noexcept
{
	auto versions = getVersions(table);

	// __COUT__ << "Table Name: " << table->getTableName() << __E__;
	// __SS__ << "All Versions: ";
	// for(auto& v : versions)
	// 	ss << v << " ";
	// ss << __E__;
	// __COUT__ << ss.str();

	if(!versions.size())
		return TableVersion();  // return INVALID

	return *(versions.rbegin());
} //end findLatestVersion()

//==============================================================================
// find all configuration versions by configuration type
std::set<TableVersion> DatabaseConfigurationInterface::getVersions(const TableBase* table) const noexcept
try
{
	auto start = std::chrono::high_resolution_clock::now();

	auto ifc    = db::ConfigurationInterface{default_dbprovider};
	auto result = ifc.template getVersions<decltype(table)>(table, default_entity);

	auto end      = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	__COUTT__ << "Time taken to call DatabaseConfigurationInterface::getVersions(tableName=" << table->getTableName() << ") " << duration << " milliseconds."
	         << std::endl;

	auto resultSet = std::set<TableVersion>{};
	for(std::string const& version : result)
		resultSet.insert(TableVersion(std::stol(version, 0, 10)));

	//	auto to_set = [](auto const& inputList)
	//	{
	//		auto resultSet = std::set<TableVersion>{};
	//		std::for_each(inputList.begin(), inputList.end(),
	//				[&resultSet](std::string const& version)
	//				{ resultSet.insert(std::stol(version, 0, 10)); });
	//		return resultSet;
	//	};

	// auto vs = to_set(result);
	// for(auto &v:vs)
	//	__COUT__ << "\tversion " << v << __E__;

	return resultSet;  // to_set(result);
} //end getVersions()
catch(std::exception const& e)
{
	__COUT_WARN__ << "DBI Exception:" << e.what() << "\n";
	return {};
} //end getVersions() catch

//==============================================================================
// returns a list of all configuration names
std::set<std::string /*name*/> DatabaseConfigurationInterface::getAllTableNames() const
try
{
	auto start = std::chrono::high_resolution_clock::now();

	auto ifc                    = db::ConfigurationInterface{default_dbprovider};
	auto collection_name_prefix = std::string{};

	auto result = ifc.listCollections(collection_name_prefix);

	auto end      = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	__COUTT__ << "Time taken to call DatabaseConfigurationInterface::getAllTableNames(collection_name_prefix=" << collection_name_prefix << ") " << duration
	         << " milliseconds." << std::endl;

	return result;
} //end getAllTableNames()
catch(std::exception const& e)
{
	__SS__ << "DBI Exception:" << e.what() << "\n";
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "DBI Unknown exception.\n";
	__SS_THROW__;
} //end getAllTableNames() catch

//==============================================================================
// find all configuration groups in database
std::set<std::string /*name*/> DatabaseConfigurationInterface::getAllTableGroupNames(std::string const& filterString) const
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
	auto end      = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	__COUTT__ << "Time taken to call DatabaseConfigurationInterface::getAllTableGroupNames(filterString=" << filterString << ") " << duration << " milliseconds."
	         << std::endl;

	return result;
} //end getAllTableGroupNames()
catch(std::exception const& e)
{
	__SS__ << "Filter string '" << filterString << "' yielded DBI Exception:" << e.what() << "\n";
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "Filter string '" << filterString << "' yielded DBI Unknown exception.\n";
	__SS_THROW__;
} //end getAllTableGroupNames() catch

//==============================================================================
// find the latest configuration group key by group name
// 	if not found, return invalid
TableGroupKey DatabaseConfigurationInterface::findLatestGroupKey(const std::string& groupName) const noexcept
{
	std::set<TableGroupKey> keys = DatabaseConfigurationInterface::getKeys(groupName);
	if(keys.size())  // if keys exist, bump the last
		return *(keys.crbegin());

	// else, return invalid
	return TableGroupKey();
}

//==============================================================================
// find all configuration groups in database
std::set<TableGroupKey /*key*/> DatabaseConfigurationInterface::getKeys(const std::string& groupName) const
{
	std::set<TableGroupKey>        retSet;
	std::set<std::string /*name*/> names = getAllTableGroupNames();
	for(auto& n : names)
		if(n.find(groupName) == 0)
			retSet.insert(TableGroupKey(n));
	return retSet;
}

//==============================================================================
// return the contents of a configuration group
table_version_map_t DatabaseConfigurationInterface::getTableGroupMembers(std::string const& tableGroup, bool includeMetaDataTable /* = false */) const
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
		__COUT_TYPE__(TLVL_DEBUG+20) << __COUT_HDR__ << (StringMacros::mapToString(retMap));

		if(!includeMetaDataTable)
		{
			// remove special meta data table from member map
			auto metaTable = retMap.find(GROUP_METADATA_TABLE_NAME);
			if(metaTable != retMap.end())
				retMap.erase(metaTable);
		}

		auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
		__COUTT__ << "Time taken to call DatabaseConfigurationInterface::getTableGroupMembers(tableGroup=" << tableGroup << ") " << duration << " milliseconds."
	         << std::endl;
		return retMap;
	}
	catch(...) //ignore error and proceed with standard db access
	{
		__COUTT__ << "Ignoring error DatabaseConfigurationInterface::getTableGroupMembers(tableGroup=" << tableGroup << ") " << __E__;
	} 

	auto ifc    = db::ConfigurationInterface{default_dbprovider};
	auto result = ifc.loadGlobalConfiguration(tableGroup);

	// for(auto &item:result)
	// 	__COUTT__ << "====================> " << item.configuration << ": " << item.version << __E__;

	auto to_map = [](auto const& inputList, bool includeMetaDataTable) 
	{
		auto resultMap = table_version_map_t{};

		std::for_each(inputList.begin(), inputList.end(), [&resultMap](auto const& info) { resultMap[info.configuration] = std::stol(info.version, 0, 10); });

		if(!includeMetaDataTable)
		{
			// remove special meta data table from member map
			auto metaTable = resultMap.find(GROUP_METADATA_TABLE_NAME);
			if(metaTable != resultMap.end())
				resultMap.erase(metaTable);
		}
		return resultMap;
	};

	table_version_map_t retMap = to_map(result, includeMetaDataTable);	

	//now create cache for next time!
	saveTableGroupMemberCache(retMap, tableGroup);

	__COUTT__ << "Loaded db member map string " <<
		StringMacros::mapToString(retMap) << __E__;

	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start).count();
	__COUTT__ << "Time taken to call DatabaseConfigurationInterface::getTableGroupMembers(tableGroup=" << tableGroup << ") " << duration << " milliseconds."
	         << std::endl;

	return retMap;
}  // end getTableGroupMembers()
catch(std::exception const& e)
{
	__SS__ << "DBI Exception getting Group's member tables for '" << tableGroup << "':\n\n" << e.what() << "\n";
	if(std::string(e.what()).find("connection refused") != std::string::npos)
	{
		ss << "\n\nConnection to database refused. Perhaps your ssh tunnel has closed?\n\n";
	}
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "DBI Unknown exception getting Group's member tables for '" << tableGroup << ".'\n";
	__COUT_ERR__ << ss.str();
	__SS_THROW__;
}  // end getTableGroupMembers() catch

//==============================================================================
// get cached Table Group members
//	throw exception on failure or missing cache
table_version_map_t DatabaseConfigurationInterface::getCachedTableGroupMembers(std::string const& tableGroup) const
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

	
	std::size_t vi = tableGroup.rfind("_v");
	std::string groupName = tableGroup.substr(0,vi);
	std::string groupKey = tableGroup.substr(vi+2);
	__COUTT__ << "Getting cache for " << groupName << "(" << groupKey << ")" << __E__;

	TableBase localGroupMemberCacheSaver(TableBase::GROUP_CACHE_PREPEND + groupName);	
	TableVersion localVersion(atoi(groupKey.c_str()));
	
	//if filesystem db, as of April 2024, artdaq_database returned latest version when version is missing...
	if(IS_FILESYSTEM_DB)
	{		
		__COUTT__ << "IS_FILESYSTEM_DB=true, so checking cached keys for " << groupName << "(" << groupKey << ")" << __E__;
		std::set<TableVersion> versions = getVersions(&localGroupMemberCacheSaver);
		if(versions.find(localVersion) == versions.end())
		{
			__SS__ << "Cached member table versions not found for " << groupName << "(" << groupKey << ")" << __E__;
			__SS_THROW__;
		}
	}

	localGroupMemberCacheSaver.changeVersionAndActivateView(
		localGroupMemberCacheSaver.createTemporaryView(), 
		localVersion);

	fill(&localGroupMemberCacheSaver,localVersion);

	__COUT_TYPE__(TLVL_DEBUG+20) << __COUT_HDR__ << "Loaded cache member map string " <<
		localGroupMemberCacheSaver.getViewP()->getCustomStorageData() << __E__;
	
	StringMacros::getMapFromString(localGroupMemberCacheSaver.getViewP()->getCustomStorageData(),retMap);

	__COUT_TYPE__(TLVL_DEBUG+20) << __COUT_HDR__ << "Loaded cache member map string " <<
		StringMacros::mapToString(retMap) << __E__;
		
	return retMap;
} //end getCachedTableGroupMembers()
catch(std::exception const& e)
{
	__SS__ << "DBI Exception getCachedTableGroupMembers for '" << tableGroup << "':\n\n" << e.what() << "\n";
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "DBI Unknown exception getCachedTableGroupMembers for '" << tableGroup << ".'\n";
	__SS_THROW__;
} //end getCachedTableGroupMembers() catch

//==============================================================================
// create a new configuration group from the contents map
void DatabaseConfigurationInterface::saveTableGroupMemberCache(table_version_map_t const& memberMap, std::string const& tableGroup) const
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


	
	std::size_t vi = tableGroup.rfind("_v");
	std::string groupName = tableGroup.substr(0,vi);
	std::string groupKey = tableGroup.substr(vi+2);
	__COUTT__ << "Saving cache for " << groupName << "(" << groupKey << ")" << __E__;

	TableBase localGroupMemberCacheSaver(TableBase::GROUP_CACHE_PREPEND + groupName);
	localGroupMemberCacheSaver.changeVersionAndActivateView(
		localGroupMemberCacheSaver.createTemporaryView(), 
		TableVersion(atoi(groupKey.c_str())));

	{ //set custom storage data
		std::stringstream groupCacheData;
		groupCacheData << "{ ";
		for(const auto& member : memberMap)
			groupCacheData << (member.first == memberMap.begin()->first?"":", ") << //skip comma on first 
				"\"" << member.first << "\" : \"" << member.second << "\"";
		groupCacheData << "}";

		localGroupMemberCacheSaver.getViewP()->setCustomStorageData(groupCacheData.str());
	} //end set custom storage data

	__COUTT__ << "Saving member map string " <<
		localGroupMemberCacheSaver.getViewP()->getCustomStorageData() << __E__;

	__COUTT__ << "Saving cache table " <<
		localGroupMemberCacheSaver.getView().getTableName() << "(" <<
		localGroupMemberCacheSaver.getView().getVersion().toString() << ")" << __E__;

 
	// save to db, and do not allow overwrite
	saveActiveVersion(&localGroupMemberCacheSaver, false /* overwrite */);
	
} //end saveTableGroupMemberCache()
catch(std::exception const& e)
{
	__SS__ << "DBI Exception saveTableGroupMemberCache for '" << tableGroup << "':\n\n" << e.what() << "\n";
	__COUT_ERR__ << ss.str();
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "DBI Unknown exception saveTableGroupMemberCache for '" << tableGroup << ".'\n";
	__COUT_ERR__ << ss.str();
	__SS_THROW__;
} //end saveTableGroupMemberCache() catch

//==============================================================================
// create a new configuration group from the contents map
void DatabaseConfigurationInterface::saveTableGroup(table_version_map_t const& memberMap, std::string const& tableGroup) const
try
{
	auto start = std::chrono::high_resolution_clock::now();

	auto ifc = db::ConfigurationInterface{default_dbprovider};

	auto to_list = [](auto const& inputMap) {
		auto resultList = VersionInfoList_t{};
		std::transform(inputMap.begin(), inputMap.end(), std::back_inserter(resultList), [](auto const& mapEntry) {
			return VersionInfoList_t::value_type{mapEntry.first, mapEntry.second.toString(), default_entity};
		});

		return resultList;
	};

	auto result = IS_FILESYSTEM_DB?
		ifc.storeGlobalConfiguration(to_list(memberMap), tableGroup):
		ifc.storeGlobalConfiguration_mt(to_list(memberMap), tableGroup);

	auto end      = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	__COUTT__ << "Time taken to call DatabaseConfigurationInterface::saveTableGroup(tableGroup=" << tableGroup << ") " << duration
	         << " milliseconds." << std::endl;

	if(result.first)
	{
		//now save to db cache for reverse index lookup of group members
		try
		{
			saveTableGroupMemberCache(memberMap,tableGroup);
		}
		catch(...)
		{
			__COUT_WARN__ << "Ignoring errors during saveTableGroupMemberCache()" << __E__;
		}
		
		return;
	}

	__THROW__(result.second);
}  // end saveTableGroup()
catch(std::exception const& e)
{
	__SS__ << "DBI Exception saveTableGroup for '" << tableGroup << "':\n\n" << e.what() << "\n";
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "DBI Unknown exception saveTableGroup for '" << tableGroup << ".'\n";
	__SS_THROW__;
} //end saveTableGroup() catch




//==============================================================================
std::pair<std::string, TableVersion> DatabaseConfigurationInterface::saveCustomJSON(const std::string& json, const std::string& documentNameToSave) const
try
{
	__COUTT__ << "Saving doc '" << documentNameToSave << "'" << __E__;

	TableBase localDocSaver(TableBase::JSON_DOC_PREPEND + documentNameToSave);

	std::set<TableVersion> versions = getVersions(&localDocSaver);
	TableVersion version;
	if(versions.size())
		version = TableVersion::getNextVersion(*versions.rbegin());
	else 
		version = TableVersion::DEFAULT;
	__COUTV__(version);

	localDocSaver.changeVersionAndActivateView(
		localDocSaver.createTemporaryView(), 
		version);

	localDocSaver.getViewP()->setCustomStorageData(json);

	__COUTT__ << "Saving JSON string: " <<
		localDocSaver.getViewP()->getCustomStorageData() << __E__;

	__COUTT__ << "Saving JSON doc as " <<
		localDocSaver.getView().getTableName() << "(" <<
		localDocSaver.getView().getVersion().toString() << ")" << __E__;

 
	// save to db, and do not allow overwrite
	saveActiveVersion(&localDocSaver, false /* overwrite */);
	
	return std::make_pair(localDocSaver.getTableName(),
		localDocSaver.getView().getVersion());
} //end saveCustomJSON()
catch(std::exception const& e)
{
	__SS__ << "DBI Exception saveCustomJSON for '" << documentNameToSave << "':\n\n" << e.what() << "\n";
	__COUT_ERR__ << ss.str();
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "DBI Unknown exception saveCustomJSON for '" << documentNameToSave << ".'\n";
	__COUT_ERR__ << ss.str();
	__SS_THROW__;
} //end saveCustomJSON() catch


//==============================================================================
std::string DatabaseConfigurationInterface::loadCustomJSON(const std::string& documentNameToLoad, TableVersion documentVersionToLoad) const
try
{
	
	__COUTT__ << "Loading doc '" << documentNameToLoad << "-v" <<
		documentVersionToLoad << "'" << __E__;

	TableBase localDocLoader(TableBase::JSON_DOC_PREPEND + documentNameToLoad);

	localDocLoader.changeVersionAndActivateView(
		localDocLoader.createTemporaryView(), 
		documentVersionToLoad);

	fill(&localDocLoader,documentVersionToLoad);

	__COUTT__ << "Loaded JSON doc string " <<
		localDocLoader.getViewP()->getCustomStorageData() << __E__;
	
	return localDocLoader.getViewP()->getCustomStorageData();
} //end loadCustomJSON()
catch(std::exception const& e)
{
	__SS__ << "DBI Exception saveCustomJSON for '" << documentNameToLoad << "-v" <<
		documentVersionToLoad << "':\n\n" << e.what() << "\n";
	__COUT_ERR__ << ss.str();
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "DBI Unknown exception saveCustomJSON for '" << documentNameToLoad << "-v" <<
		documentVersionToLoad << ".'\n";
	__COUT_ERR__ << ss.str();
	__SS_THROW__;
} //end loadCustomJSON() catch

DEFINE_OTS_CONFIGURATION_INTERFACE(DatabaseConfigurationInterface)
