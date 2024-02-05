#define BOOST_TEST_MODULE (databaseinterface test)

#include "boost/test/auto_unit_test.hpp"

#include <time.h> /* time */
#include <cassert>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>

#include "otsdaq/ConfigurationInterface/DatabaseConfigurationInterface.h"

#include "artdaq-database/JsonDocument/JSONDocument.h"

#include "artdaq-database/ConfigurationDB/configuration_common.h"
#include "artdaq-database/ConfigurationDB/dispatch_common.h"

#include "artdaq-database/JsonDocument/JSONDocument.h"
#include "artdaq-database/JsonDocument/JSONDocumentBuilder.h"
#include "artdaq-database/StorageProviders/FileSystemDB/provider_filedb.h"
#include "artdaq-database/StorageProviders/MongoDB/provider_mongodb.h"

#include "artdaq-database/DataFormats/Json/json_reader.h"
// #include "artdaq-database/DataFormats/common/helper_functions.h"
// #include "artdaq-database/DataFormats/common/shared_literals.h"

namespace ots
{
struct TableViewEx : public TableView
{
	void printJSON(std::stringstream& ss) const { ss << _json; }
	int  fillFromJSON(std::string const& newjson)
	{
		_json = newjson;
		return 0;
	}
	std::string _json = "{ \"testJSON\" : 123 }";
};

struct TestConfiguration001 final : public TableBase
{
	TestConfiguration001() : TableBase("TestConfiguration001") { init(0); }
	void        init(ConfigurationManager* configManager) { activeTableView_ = &view; }
	TableViewEx view;
};

struct TestConfiguration002 final : public TableBase
{
	TestConfiguration002() : TableBase("TestConfiguration002") { init(0); }
	void        init(ConfigurationManager* configManager) { activeTableView_ = &view; }
	TableViewEx view;
};
}  // namespace ots

struct TestData
{
	TestData()
	{
		/*
	artdaq::database::filesystem::debug::enable();
	artdaq::database::mongo::debug::enable();
	// artdaq::database::jsonutils::debug::enableJSONDocument();
	// artdaq::database::jsonutils::debug::enableJSONDocumentBuilder();

	artdaq::database::configuration::debug::enableFindConfigsOperation();
	artdaq::database::configuration::debug::enableCreateConfigsOperation();

	artdaq::database::configuration::debug::options::enableOperationBase();
	artdaq::database::configuration::debug::options::enableOperationManageConfigs();
	artdaq::database::configuration::debug::detail::enableCreateConfigsOperation();
	artdaq::database::configuration::debug::detail::enableFindConfigsOperation();

	artdaq::database::configuration::debug::enableDBOperationMongo();
	artdaq::database::configuration::debug::enableDBOperationFileSystem();

//    debug::registerUngracefullExitHandlers();
	artdaq::database::dataformats::useFakeTime(true);
 */
		std::cout << "setup fixture\n";
	}

	~TestData() { std::cout << "setup fixture\n"; }

	void updateConfigCount(int count) { _oldConfigCount = count; }

	int version() { return _version; }
	int oldConfigCount() { return _oldConfigCount; }

	const int _version = (srand(time(NULL)), rand() % 99999 + 100000);

	int _oldConfigCount = 0;
};

using namespace ots;

TestData fixture;

BOOST_AUTO_TEST_SUITE(databaseinterface_test)

BOOST_AUTO_TEST_CASE(configure_tests)
{
	std::cout << "TestData::version=" << fixture.version() << "\n";
	return;
}

BOOST_AUTO_TEST_CASE(store_configuration)
{
	std::shared_ptr<TableBase> cfg1 = std::make_shared<TestConfiguration001>();
	auto                       ifc  = DatabaseConfigurationInterface();

	cfg1->getViewP()->setVersion(fixture.version());

	BOOST_CHECK_EQUAL(cfg1->getViewP()->getVersion(), fixture.version());

	//  std::cout << "Table Version " <<
	//  std::to_string(cfg1->getViewP()->getVersion() ) << "\n";

	ifc.saveActiveVersion(cfg1.get());

	return;
}

BOOST_AUTO_TEST_CASE(load_configuration)
{
	std::shared_ptr<TableBase> cfg1 = std::make_shared<TestConfiguration001>();
	auto                       ifc  = DatabaseConfigurationInterface();

	ifc.fill(cfg1.get(), TableVersion(fixture.version()));

	return;
}

BOOST_AUTO_TEST_CASE(store_global_configuration)
{
	std::shared_ptr<TableBase> cfg1 = std::make_shared<TestConfiguration001>();
	std::shared_ptr<TableBase> cfg2 = std::make_shared<TestConfiguration002>();

	auto ifc = DatabaseConfigurationInterface();

	cfg1->getViewP()->setVersion(fixture.version() + 1);
	cfg2->getViewP()->setVersion(fixture.version() + 2);

	ifc.saveActiveVersion(cfg1.get());

	ifc.saveActiveVersion(cfg2.get());

	auto map                  = DatabaseConfigurationInterface::config_version_map_t{};
	map[cfg1->getTableName()] = cfg1->getView().getVersion();
	map[cfg2->getTableName()] = cfg2->getView().getVersion();

	auto tableName = std::string{"config"} + std::to_string(fixture.version());

	auto list = ifc.getAllTableGroupNames();

	fixture.updateConfigCount(list.size());

	ifc.saveTableGroup(map, tableName);

	return;
}

BOOST_AUTO_TEST_CASE(load_global_configuration)
{
	auto ifc = DatabaseConfigurationInterface();

	auto tableName = std::string{"config"} + std::to_string(fixture.version());

	auto map = ifc.getTableGroupMembers(tableName);

	BOOST_CHECK_EQUAL(map.size(), 2);

	std::shared_ptr<TableBase> cfg1 = std::make_shared<TestConfiguration001>();
	std::shared_ptr<TableBase> cfg2 = std::make_shared<TestConfiguration002>();

	BOOST_CHECK_EQUAL(map.at(cfg1->getTableName()), fixture.version() + 1);
	BOOST_CHECK_EQUAL(map.at(cfg2->getTableName()), fixture.version() + 2);

	return;
}

BOOST_AUTO_TEST_CASE(find_all_global_configurations)
{
	auto ifc = DatabaseConfigurationInterface();

	auto list = ifc.getAllTableGroupNames();

	BOOST_CHECK_EQUAL(list.size(), fixture.oldConfigCount() + 1);

	auto tableName = std::string{"config"} + std::to_string(fixture.version());

	auto found = (std::find(list.begin(), list.end(), tableName) != list.end());

	BOOST_CHECK_EQUAL(found, true);

	return;
}

BOOST_AUTO_TEST_CASE(list_configuration_types)
{
	auto ifc = DatabaseConfigurationInterface();

	auto list = ifc.getAllTableNames();

	BOOST_CHECK_EQUAL(list.size(), 2);

	std::shared_ptr<TableBase> cfg1 = std::make_shared<TestConfiguration001>();
	std::shared_ptr<TableBase> cfg2 = std::make_shared<TestConfiguration002>();

	auto found1 = (std::find(list.begin(), list.end(), cfg1->getTableName()) != list.end());

	BOOST_CHECK_EQUAL(found1, true);

	auto found2 = (std::find(list.begin(), list.end(), cfg2->getTableName()) != list.end());

	BOOST_CHECK_EQUAL(found2, true);

	return;
}

BOOST_AUTO_TEST_CASE(find_configuration_version)
{
	auto ifc = DatabaseConfigurationInterface();

	std::shared_ptr<TableBase> cfg1 = std::make_shared<TestConfiguration001>();

	auto list = ifc.getVersions(cfg1.get());

	auto found1 = (std::find(list.begin(), list.end(), fixture.version()) != list.end());

	BOOST_CHECK_EQUAL(found1, true);

	auto found2 = (std::find(list.begin(), list.end(), fixture.version() + 1) != list.end());

	BOOST_CHECK_EQUAL(found2, true);

	return;
}

BOOST_AUTO_TEST_CASE(find_latest_configuration_version)
{
	auto ifc = DatabaseConfigurationInterface();

	std::shared_ptr<TableBase> cfg1 = std::make_shared<TestConfiguration001>();

	auto version = ifc.findLatestVersion(cfg1.get());

	auto list = ifc.getVersions(cfg1.get());

	std::cout << "Found versions\n";

	for(auto version : list)
	{
		std::cout << version << ", ";
	}

	std::cout << "\nGot latest version: " << version << "\n";

	return;
}

BOOST_AUTO_TEST_SUITE_END()
