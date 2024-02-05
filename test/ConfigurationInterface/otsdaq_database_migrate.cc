// #define BOOST_TEST_MODULE ( databaseconfiguration test)

// #include "boost/test/auto_unit_test.hpp"

#include <dirent.h>
#include <cassert>
#include <iostream>
#include <memory>
#include <string>
#include "otsdaq/ConfigurationInterface/ConfigurationInterface.h"
#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
// #include "otsdaq/TablePlugins/Configurations.h"
// #include "otsdaq/TablePlugins/ConfigurationAliases.h"
// #include "otsdaq/TablePlugins/FETable.h"
// #include "otsdaq/PluginMakers/makeTable.h"
// #include "otsdaq/PluginMakers/MakeInterface.h"
#include "artdaq-database/JsonDocument/JSONDocument.h"
#include "artdaq-database/StorageProviders/FileSystemDB/provider_filedb_index.h"

using namespace ots;

// BOOST_AUTO_TEST_SUITE( databaseconfiguration_test )

void readxml_writedb_configurations()
{
	// artdaq::database::filesystem::index::debug::enable();
	// artdaq::database::jsonutils::debug::enableJSONDocument();

	std::string dbDir = std::string(__ENV__("ARTDAQ_DATABASE_DATADIR"));
	__COUT__ << "Destination DB Directory ARTDAQ_DATABASE_DATADIR: " << dbDir << std::endl;

	if(__ENV__("USER_DATA") == NULL)
		__COUT__ << "Missing env variable: USER_DATA. It must be set!" << std::endl;

	std::vector<std::string> configTables;          // list of tables to migrate
	std::vector<std::string> failedConfigVersions;  // list of tables/versions that failed to migrate

	// normally CONFIGURATION_TYPE is set by StartOTS.sh
	setenv("CONFIGURATION_DATA_PATH", (std::string(__ENV__("USER_DATA")) + "/ConfigurationDataExamples").c_str(), 1);
	std::string configDir = std::string(__ENV__("CONFIGURATION_DATA_PATH")) + '/';

	// CONFIGURATION_TYPE needed by
	// otsdaq/otsdaq/ConfigurationDataFormats/ConfigurationInfoReader.cc [187]  Can
	// be File, Database, DatabaseTest
	setenv("CONFIGURATION_TYPE", "File", 1);

	// add configurations to vector list from directory
	{
		__COUT__ << "ConfigurationDir: " << configDir << std::endl;
		DIR* dp;

		struct dirent* dirp;

		if((dp = opendir(configDir.c_str())) == 0)
		{
			__COUT__ << "ERROR:(" << errno << ").  Can't open directory: " << configDir << std::endl;
			exit(0);
		}

		const unsigned char isDir = 0x4;
		while((dirp = readdir(dp)) != 0)
			if(dirp->d_type == isDir && dirp->d_name[0] != '.')
			{
				__COUT__ << dirp->d_name << std::endl;
				configTables.push_back(dirp->d_name);
			}

		closedir(dp);
	}

	unsigned int configurationsCount = 0, skippedConfigurations = 0, skippedVersions = 0, versionsCount = 0;

	ConfigurationInterface* theInterface_ = ConfigurationInterface::getInstance(true);

	for(unsigned int i = 0; i < configTables.size(); ++i)
	{
		TableBase* base = 0;
		__COUT__ << std::endl;
		__COUT__ << std::endl;
		__COUT__ << (i + 1) << " of " << configTables.size() << ": " << configTables[i] << std::endl;

		try
		{
			theInterface_->get(base, configTables[i], 0, 0,
			                   true);  // load an empty instance, just to get all available version
		}
		catch(cet::exception e)
		{
			__COUT__ << std::endl << e.what() << std::endl;
			__COUT__ << "Caught exception, so skip. (likely not a defined configuration "
			            "class) "
			         << std::endl;

			++skippedConfigurations;
			failedConfigVersions.push_back(configTables[i] + ":*");
			continue;
		}
		++configurationsCount;

		auto version = theInterface_->getVersions(base);

		for(auto currVersion : version)
		{
			__COUT__ << "loading " << configTables[i] << " version " << currVersion << std::endl;

			try
			{
				// reset configurationView and load current version
				theInterface_->get(base, configTables[i], 0, 0, false, currVersion,
				                   true);  // load version 0 for all, first
			}
			catch(std::runtime_error e)
			{
				__COUT__ << std::endl << e.what() << std::endl;
				__COUT__ << "Caught exception for version, so skip. (likely invalid "
				            "column names) "
				         << std::endl;

				++skippedVersions;
				failedConfigVersions.push_back(configTables[i] + ":" + currVersion.toString());
				continue;
			}
			++versionsCount;

			__COUT__ << "loaded " << configTables[i] << std::endl;

			// save the active version
			__COUT__ << "Current version: " << base->getViewVersion() << std::endl;
			__COUT__ << "Current version: " << base->getView().getVersion() << std::endl;

			//
			//		**** switch to db style interface?!!?!? ****   //
			//
			theInterface_ = ConfigurationInterface::getInstance(false);  // true for File interface, false for artdaq database
			//
			//*****************************************
			//*****************************************

			// =========== Save as Current Version Number ========== //
			// uses same version number in migration database
			//
			theInterface_->saveActiveVersion(base);
			//
			// =========== END Save as Current Version Number ========== //

			// =========== Save as New Version Number ========== //
			// if wanted to create a new version number based on this version
			//
			// int tmpView = base->createTemporaryView(currVersion);
			// theInterface_->saveNewVersion(base,tmpView);
			//
			// =========== END Save as Current Version Number ========== //

			__COUT__ << "Version saved " << std::endl;

			//*****************************************
			//*****************************************
			//
			//		**** switch back db style interface?!!?!? ****   //
			//
			theInterface_ = ConfigurationInterface::getInstance(true);  // true for File interface, false for artdaq database
			                                                            //
			                                                            //

			// break;  //uncomment to just do the one version (for debugging)
		}
		delete base;  // cleanup config instance
		              // break;  //uncomment to just do the one config table (for
		              // debugging)
	}

	__COUT__ << "End of migrating Configuration!" << std::endl;

	__COUT__ << "\n\nList of failed configs:versions (size=" << failedConfigVersions.size() << std::endl;
	for(auto& f : failedConfigVersions)
		__COUT__ << f << std::endl;

	__COUT__ << "\n\nEND List of failed configs:versions" << std::endl;

	__COUT__ << "\n\n\tStats:" << std::endl;
	__COUT__ << "\t\tconfigurationsCount: " << configurationsCount << std::endl;
	__COUT__ << "\t\tskippedConfigurations: " << skippedConfigurations << std::endl;
	__COUT__ << "\t\tversionsCount: " << versionsCount << std::endl;
	__COUT__ << "\t\tskippedVersions: " << skippedVersions << std::endl;

	__COUT__ << "\nEnd of migrating Configuration!" << std::endl;

	return;
}

int main(int, char**)
{
	readxml_writedb_configurations();
	return 0;
}
// BOOST_AUTO_TEST_SUITE_END()
