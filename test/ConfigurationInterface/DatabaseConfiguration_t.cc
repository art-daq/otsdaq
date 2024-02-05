
#define BOOST_TEST_MODULE (databaseconfiguration test)

#include "boost/test/auto_unit_test.hpp"

#include <dirent.h>
#include <cassert>
#include <iostream>
#include <memory>
#include <string>
// #include <otsdaq_demo/otsdaq-demo/FEInterfaces/FEWROtsUDPFSSRInterface.h>
// #include
//<otsdaq_demo/otsdaq-demo/UserConfigurationDataFormats/FEWROtsUDPFSSRInterfaceConfiguration.h>
#include "otsdaq/ConfigurationInterface/ConfigurationInterface.h"
#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
// #include "otsdaq/TablePlugins/Configurations.h"
// #include "otsdaq/TablePlugins/ConfigurationAliases.h"
// #include "otsdaq/TablePlugins/FETable.h"
#include "artdaq-database/JsonDocument/JSONDocument.h"
#include "artdaq-database/StorageProviders/FileSystemDB/provider_filedb_index.h"
#include "otsdaq/PluginMakers/MakeInterface.h"
#include "otsdaq/PluginMakers/MakeInterfaceTable.h"

using namespace ots;

BOOST_AUTO_TEST_SUITE(databaseconfiguration_test)

BOOST_AUTO_TEST_CASE(readxml_writedb_configurations)
{
	// artdaq::database::filesystem::index::debug::enable();
	// artdaq::database::jsonutils::debug::enableJSONDocument();

	std::vector<std::string> configTables;

	// normally CONFIGURATION_TYPE is set by StartOTS.sh
	setenv("CONFIGURATION_DATA_PATH", (std::string(__ENV__("USER_DATA")) + "/ConfigurationDataExamples").c_str(), 1);
	std::string configDir = std::string(__ENV__("CONFIGURATION_DATA_PATH")) + '/';

	// CONFIGURATION_TYPE needed by
	// otsdaq/otsdaq/ConfigurationDataFormats/ConfigurationInfoReader.cc [187]  Can
	// be File, Database, DatabaseTest
	setenv("CONFIGURATION_TYPE", "File", 1);

	// add configurations to vector list from directory
	{
		__COUT__ << "ConfigurationDir: " << configDir << __E__;
		DIR* dp;

		struct dirent* dirp;

		if((dp = opendir(configDir.c_str())) == 0)
		{
			__COUT__ << "ERROR:(" << errno << ").  Can't open directory: " << configDir << __E__;
			exit(0);
		}

		const unsigned char isDir = 0x4;
		while((dirp = readdir(dp)) != 0)
			if(dirp->d_type == isDir && dirp->d_name[0] != '.')
			{
				__COUT__ << dirp->d_name << __E__;
				configTables.push_back(dirp->d_name);
			}

		closedir(dp);
	}

	ConfigurationInterface* theInterface_ = ConfigurationInterface::getInstance(true);

	for(unsigned int i = 0; i < configTables.size(); ++i)
	{
		theInterface_   = ConfigurationInterface::getInstance(true);
		TableBase* base = 0;
		__COUT__ << __E__;
		__COUT__ << __E__;
		__COUT__ << (i + 1) << " of " << configTables.size() << ": " << configTables[i] << __E__;

		theInterface_->get(base, configTables[i], 0, 0, false,
		                   TableVersion(TableVersion::DEFAULT));  // load version 0 for all

		__COUT__ << "loaded " << configTables[i] << __E__;

		// save the active version
		__COUT__ << "Current version: " << base->getViewVersion() << __E__;

		//
		//		**** switch to db style interface?!!?!? ****   //
		//
		theInterface_ = ConfigurationInterface::getInstance(false);
		//
		//

		// theInterface_->saveActiveVersion(base); //saves current version

		TableVersion tmpView = base->createTemporaryView(TableVersion(TableVersion::DEFAULT));
		theInterface_->saveNewVersion(base, tmpView);

		delete base;  // cleanup config instance

		// break;
	}

	__COUT__ << "end of debugging Configuration!" << __E__;
	return;
}

BOOST_AUTO_TEST_CASE(readdb_writexml_configurations)
{
	// return;
	std::vector<std::string> configTables;

	// normally CONFIGURATION_TYPE is set by StartOTS.sh
	setenv("CONFIGURATION_DATA_PATH", (std::string(__ENV__("USER_DATA")) + "/ConfigurationDataExamples").c_str(), 1);
	std::string configDir = std::string(__ENV__("CONFIGURATION_DATA_PATH")) + '/';

	// CONFIGURATION_TYPE needed by
	// otsdaq/otsdaq/ConfigurationDataFormats/ConfigurationInfoReader.cc [187]  Can
	// be File, Database, DatabaseTest
	setenv("CONFIGURATION_TYPE", "File", 1);

	// add configurations to vector list from directory
	{
		__COUT__ << "ConfigurationDir: " << configDir << __E__;
		DIR* dp;

		struct dirent* dirp;

		if((dp = opendir(configDir.c_str())) == 0)
		{
			__COUT__ << "ERROR:(" << errno << ").  Can't open directory: " << configDir << __E__;
			exit(0);
		}

		const unsigned char isDir = 0x4;
		while((dirp = readdir(dp)) != 0)
			if(dirp->d_type == isDir && dirp->d_name[0] != '.')
			{
				__COUT__ << dirp->d_name << __E__;
				configTables.push_back(dirp->d_name);
			}

		closedir(dp);
	}

	ConfigurationInterface* theInterface_ = ConfigurationInterface::getInstance(false);

	for(unsigned int i = 0; i < configTables.size(); ++i)
	{
		theInterface_   = ConfigurationInterface::getInstance(false);
		TableBase* base = 0;
		__COUT__ << __E__;
		__COUT__ << __E__;
		__COUT__ << (i + 1) << " of " << configTables.size() << ": " << configTables[i] << __E__;

		theInterface_->get(base, configTables[i], 0, 0, false,
		                   TableVersion(TableVersion::DEFAULT));  // load version 0 for all

		__COUT__ << "loaded " << configTables[i] << __E__;

		// save the active version
		__COUT__ << "Current version: " << base->getViewVersion() << __E__;

		//
		//		**** switch to db style interface?!!?!? ****   //
		//
		// theInterface_ = ConfigurationInterface::getInstance(true);
		//
		//

		TableVersion tmpView = base->createTemporaryView(TableVersion(TableVersion::DEFAULT));
		theInterface_->saveNewVersion(base, tmpView);

		delete base;  // cleanup config instance
		              // break;
	}

	__COUT__ << "end of debugging Configuration!" << __E__;
	return;
}

BOOST_AUTO_TEST_SUITE_END()
