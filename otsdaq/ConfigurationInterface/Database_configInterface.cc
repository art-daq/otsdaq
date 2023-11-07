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
using config_version_map_t = ots::DatabaseConfigurationInterface::config_version_map_t;

namespace db            = artdaq::database::configuration;
using VersionInfoList_t = db::ConfigurationInterface::VersionInfoList_t;

constexpr auto default_dbprovider = "filesystem";
constexpr auto default_entity     = "OTSROOT";

//==============================================================================
DatabaseConfigurationInterface::DatabaseConfigurationInterface()
{
#ifdef DEBUG_ENABLE
	// to enable debugging
	if(0)
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

		artdaq::database::filesystem::debug::enable();
		artdaq::database::mongo::debug::enable();

		artdaq::database::docrecord::debug::JSONDocumentBuilder();
		artdaq::database::docrecord::debug::JSONDocument();

		// debug::registerUngracefullExitHandlers();
		//  artdaq::database::useFakeTime(true);
		artdaq::database::configuration::Multitasker();
	}
#endif
}

//==============================================================================
// read configuration from database
// version = -1 means latest version
void DatabaseConfigurationInterface::fill(TableBase* configuration, TableVersion version) const

{
	auto start = std::chrono::high_resolution_clock::now();

	auto ifc = db::ConfigurationInterface{default_dbprovider};

	auto versionstring = version.toString();

	// configuration->getViewP()->setUniqueStorageIdentifier(storageUID);
	auto result = ifc.template loadVersion<decltype(configuration), JsonData>(configuration, versionstring, default_entity);

	auto end      = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	__COUT_TYPE__(TLVL_DEBUG+20) << __COUT_HDR__ << "Time taken to call DatabaseConfigurationInterface::fill(tableName=" << configuration->getTableName() << ", version=" << versionstring << ") "
	         << duration << " milliseconds." << std::endl;

	if(result.first)
	{
		// make sure version is set.. not clear it was happening in loadVersion
		configuration->getViewP()->setVersion(version);
		return;
	}
	__SS__ << "\n\nDBI Error while filling '" << configuration->getTableName() << "' version '" << versionstring << "' - are you sure this version exists?\n"
	       << "Here is the error:\n\n"
	       << result.second << __E__;
	__SS_ONLY_THROW__;
}  // end fill()

//==============================================================================
// write configuration to database
void DatabaseConfigurationInterface::saveActiveVersion(const TableBase* configuration, bool overwrite) const

{
	auto start = std::chrono::high_resolution_clock::now();

	auto ifc = db::ConfigurationInterface{default_dbprovider};
	// configuration->getView().getUniqueStorageIdentifier()

	auto versionstring = configuration->getView().getVersion().toString();
	//__COUT__ << "versionstring: " << versionstring << "\n";

	// auto result =
	//	ifc.template storeVersion<decltype(configuration), JsonData>(configuration,
	// versionstring, default_entity);
	auto result = overwrite ? ifc.template overwriteVersion<decltype(configuration), JsonData>(configuration, versionstring, default_entity)
	                        : ifc.template storeVersion<decltype(configuration), JsonData>(configuration, versionstring, default_entity);

	auto end      = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	__COUT_TYPE__(TLVL_DEBUG+20) << __COUT_HDR__ << "Time taken to call DatabaseConfigurationInterface::saveActiveVersion(tableName=" << configuration->getTableName()
	         << ", versionstring=" << versionstring << ") " << duration << " milliseconds" << std::endl;

	if(result.first)
		return;

	__SS__ << "DBI Error:" << result.second << __E__;
	__SS_THROW__;
}

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
}

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
	__COUT_TYPE__(TLVL_DEBUG+20) << __COUT_HDR__ << "Time taken to call DatabaseConfigurationInterface::getVersions(tableName=" << table->getTableName() << ") " << duration << " milliseconds."
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
}
catch(std::exception const& e)
{
	__COUT_WARN__ << "DBI Exception:" << e.what() << "\n";
	return {};
}

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
	__COUT_TYPE__(TLVL_DEBUG+20) << __COUT_HDR__ << "Time taken to call DatabaseConfigurationInterface::getAllTableNames(collection_name_prefix=" << collection_name_prefix << ") " << duration
	         << " milliseconds." << std::endl;

	return result;
}
catch(std::exception const& e)
{
	__SS__ << "DBI Exception:" << e.what() << "\n";
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "DBI Unknown exception.\n";
	try	{ throw; } //one more try to printout extra info
	catch(const std::exception &e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...){}
	__SS_THROW__;
}

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
	__COUT_TYPE__(TLVL_DEBUG+20) << __COUT_HDR__ << "Time taken to call DatabaseConfigurationInterface::getAllTableGroupNames(filterString=" << filterString << ") " << duration << " milliseconds."
	         << std::endl;

	return result;
}
catch(std::exception const& e)
{
	__SS__ << "Filter string '" << filterString << "' yielded DBI Exception:" << e.what() << "\n";
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "Filter string '" << filterString << "' yielded DBI Unknown exception.\n";
	try	{ throw; } //one more try to printout extra info
	catch(const std::exception &e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...){}
	__SS_THROW__;
}

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
config_version_map_t DatabaseConfigurationInterface::getTableGroupMembers(std::string const& tableGroup, bool includeMetaDataTable) const
try
{
	auto start = std::chrono::high_resolution_clock::now();

	auto ifc    = db::ConfigurationInterface{default_dbprovider};
	auto result = ifc.loadGlobalConfiguration(tableGroup);

	auto end      = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	__COUT_TYPE__(TLVL_DEBUG+20) << __COUT_HDR__ << "Time taken to call DatabaseConfigurationInterface::getTableGroupMembers(tableGroup=" << tableGroup << ") " << duration << " milliseconds."
	         << std::endl;

	//	for(auto &item:result)
	//		__COUT__ << "====================>" << item.configuration << ": " <<
	// item.version << __E__;

	auto to_map = [](auto const& inputList, bool includeMetaDataTable) {
		auto resultMap = config_version_map_t{};

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

	return to_map(result, includeMetaDataTable);
}  // end getTableGroupMembers()
catch(std::exception const& e)
{
	__SS__ << "DBI Exception getting Group's member tables for '" << tableGroup << "':\n\n" << e.what() << "\n";	
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "DBI Unknown exception getting Group's member tables for '" << tableGroup << ".'\n";
	try	{ throw; } //one more try to printout extra info
	catch(const std::exception &e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...){}
	__SS_THROW__;
}

//==============================================================================
// create a new configuration group from the contents map
void DatabaseConfigurationInterface::saveTableGroup(config_version_map_t const& configurationMap, std::string const& configurationGroup) const
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

	auto result = ifc.storeGlobalConfiguration_mt(to_list(configurationMap), configurationGroup);

	auto end      = std::chrono::high_resolution_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
	__COUT_TYPE__(TLVL_DEBUG+20) << __COUT_HDR__ << "Time taken to call DatabaseConfigurationInterface::saveTableGroup(configurationGroup=" << configurationGroup << ") " << duration
	         << " milliseconds." << std::endl;

	if(result.first)
		return;

	__THROW__(result.second);
}  // end saveTableGroup()
catch(std::exception const& e)
{
	__SS__ << "DBI Exception:" << e.what() << "\n";
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "DBI Unknown exception.\n";
	try	{ throw; } //one more try to printout extra info
	catch(const std::exception &e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...){}
	__SS_THROW__;
}

DEFINE_OTS_CONFIGURATION_INTERFACE(DatabaseConfigurationInterface)
