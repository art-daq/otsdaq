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

// usage:
// otsdaq_load_json_document <document_name_to_load> <document_version_to_load> <path_to_save_JSON>


#define TRACE_NAME "LoadJSON_Document"

#undef	__COUT__
#define __COUT__			std::cout << __MF_DECOR__ << __COUT_HDR_FL__ //TLOG(TLVL_DEBUG) //std::cout << __MF_DECOR__ << __COUT_HDR_FL__


using namespace ots;

void LoadJSON_Document(int argc, char* argv[])
{
	// The configuration uses __ENV__("SERVICE_DATA_PATH") in init() so define it if it is not defined
	if(getenv("SERVICE_DATA_PATH") == NULL)
		setenv("SERVICE_DATA_PATH", (std::string(__ENV__("USER_DATA")) + "/ServiceData").c_str(), 1);

	__COUT__ << "=================================================\n";
	__COUT__ << "=================================================\n";
	__COUT__ << "=================================================\n";
	__COUT__ << "\nLoading Trigger Document!" << std::endl;

	__COUT__ << "\n\nusage: Two arguments:\n\t <document_name_to_load> <document_version_to_load> <path_to_save_JSON>"
	          << std::endl << std::endl;


	__COUT__ << "argc = " << argc << std::endl;
	for(int i = 0; i < argc; i++)
		__COUT__ << "argv[" << i << "] = " << argv[i] << std::endl;

	if(argc != 4)
	{
		__COUT__ << "\n\nError! Must provide 3 parameters.\n\n" << std::endl;
		return;
	}

	
	//==============================================================================
	// Define environment variables
	//	Note: normally these environment variables are set by StartOTS.sh

	// These are needed by
	// otsdaq/otsdaq/ConfigurationDataFormats/ConfigurationInfoReader.cc [207]
	// setenv("CONFIGURATION_TYPE", "File", 1);  // Can be File, Database, DatabaseTest
	// setenv("CONFIGURATION_DATA_PATH", (std::string(getenv("USER_DATA")) + "/ConfigurationDataExamples").c_str(), 1);
	// setenv("TABLE_INFO_PATH", (std::string(getenv("USER_DATA")) + "/TableInfo").c_str(), 1);
	// ////////////////////////////////////////////////////

	// // Some configuration plug-ins use __ENV__("OTSDAQ_LIB") and
	// // __ENV__("OTSDAQ_UTILITIES_LIB") in init() so define it 	to a non-sense place is ok
	// setenv("OTSDAQ_LIB", (std::string(getenv("USER_DATA")) + "/").c_str(), 1);
	// setenv("OTSDAQ_UTILITIES_LIB", (std::string(getenv("USER_DATA")) + "/").c_str(), 1);

	// // Some configuration plug-ins use __ENV__("OTS_MAIN_PORT") in init() so define it
	// setenv("OTS_MAIN_PORT", "2015", 1);

	// // also xdaq envs for XDAQContextTable
	// setenv("XDAQ_CONFIGURATION_DATA_PATH", (std::string(getenv("USER_DATA")) + "/XDAQConfigurations").c_str(), 1);
	// setenv("XDAQ_CONFIGURATION_XML", "otsConfigurationNoRU_CMake", 1);
	////////////////////////////////////////////////////

	//==============================================================================
	// get prepared with initial source db

	// ConfigurationManager instance immediately loads active groups
	__COUT__ << "Loading JSON Document..." << std::endl;
	ConfigurationManagerRW  cfgMgrInst("load_admin");
	ConfigurationManagerRW* cfgMgr = &cfgMgrInst;


	ConfigurationInterface* theInterface_ = cfgMgr->getConfigurationInterface();
	std::string json = theInterface_->loadCustomJSON(argv[1],TableVersion(atoi(argv[2])));
	__COUTV__(json);
	FILE* fp = std::fopen(argv[3], "w");
	if(!fp)
	{
		__COUT__ << "\n\nERROR! Could not open file at " << argv[1] << 
			". Error: " << errno << " - " << strerror(errno) << __E__;
		return;
	}
	fputs(json.c_str(),fp);
	fclose(fp);
	return;
} //end LoadJSON_Document()

int main(int argc, char* argv[])
{
	if(getenv("OTSDAQ_LOG_FHICL") == NULL)
		setenv("OTSDAQ_LOG_FHICL", (std::string(__ENV__("USER_DATA")) + "/MessageFacilityConfigurations/MessageFacilityWithCout.fcl").c_str(), 1);

	if(getenv("OTSDAQ_LOG_ROOT") == NULL)
		setenv("OTSDAQ_LOG_ROOT", (std::string(__ENV__("USER_DATA")) + "/Logs").c_str(), 1);

	// INIT_MF("LoadJSON_Document");
	LoadJSON_Document(argc, argv);
	return 0;
}
// BOOST_AUTO_TEST_SUITE_END()
