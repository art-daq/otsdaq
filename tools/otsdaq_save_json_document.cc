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
// otsdaq_save_json_document <path_to_source_JSON> <document_name_to_save>
//

#define TRACE_NAME "SaveJSON_Document"

// #define __FILENAME__ 		(__builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__)
// #define __MF_SUBJECT__ 		__FILENAME__
// #define __MF_DECOR__		(__MF_SUBJECT__)
// #define __SHORTFILE__ 		(__builtin_strstr(&__FILE__[0], "/srcs/") ? __builtin_strstr(&__FILE__[0], "/srcs/") + 6 : __FILE__)
// #define __COUT_HDR_L__ 		"[" << std::dec        << __LINE__ << "]\t"
// #define __COUT_HDR_FL__ 	__SHORTFILE__ << " "   << __COUT_HDR_L__
// #define __COUT_ERR__ 		TLOG(TLVL_ERROR) 
// #define __COUT_INFO__ 		TLOG(TLVL_INFO) 
#undef	__COUT__
#define __COUT__			std::cout << __MF_DECOR__ << __COUT_HDR_FL__ //TLOG(TLVL_DEBUG) //std::cout << __MF_DECOR__ << __COUT_HDR_FL__

using namespace ots;

void SaveJSON_Document(int argc, char* argv[])
{
	// The configuration uses __ENV__("SERVICE_DATA_PATH") in init() so define it if it is not defined
	if(getenv("SERVICE_DATA_PATH") == NULL)
		setenv("SERVICE_DATA_PATH", (std::string(__ENV__("USER_DATA")) + "/ServiceData").c_str(), 1);

	__COUT__ << "=================================================\n";
	__COUT__ << "=================================================\n";
	__COUT__ << "=================================================\n";
	__COUT__ << "\nSaving Trigger Document!" << std::endl;

	__COUT__ << "\n\nusage: Two arguments:\n\t <path_to_source_JSON> <document_name_to_save>"
	          << std::endl << std::endl;


	__COUT__ << "argc = " << argc << std::endl;
	for(int i = 0; i < argc; i++)
		__COUT__ << "argv[" << i << "] = " << argv[i] << std::endl;

	if(argc != 3)
	{
		__COUT__ << "Error! Must provide 2 parameters.\n\n" << std::endl;
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
	////////////////////////////////////////////////////

	// Some configuration plug-ins use __ENV__("OTSDAQ_LIB") and
	// __ENV__("OTSDAQ_UTILITIES_LIB") in init() so define it 	to a non-sense place is ok
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
	__COUT__ << "Saving document..." << std::endl;
	ConfigurationManagerRW  cfgMgrInst("doc_admin");
	ConfigurationManagerRW* cfgMgr = &cfgMgrInst;


	std::FILE* fp = std::fopen(argv[1], "rb");
	if(!fp)
	{
		__COUT__ << "\n\nERROR! Could not open file at " << argv[1] << 
			". Error: " << errno << " - " << strerror(errno) << __E__;
		return;
	}
	std::string json;
	std::fseek(fp, 0, SEEK_END);
	json.resize(std::ftell(fp));
	std::rewind(fp);
	std::fread(&json[0], 1, json.size(), fp);
	std::fclose(fp);

	std::string artad_db_uri = __ENV__("ARTDAQ_DATABASE_URI");
	__COUT__ << artad_db_uri << __E__;
	if(!artad_db_uri.size())
	{
		__COUT__ << "ERROR! ARTDAQ_DATABASE_URI not set." << __E__;
		return;
	}
	ConfigurationInterface* theInterface_ = cfgMgr->getConfigurationInterface();		
	std::pair<std::string, TableVersion> savedDoc = theInterface_->saveCustomJSON(json,argv[2]);
	__COUT__ << "Done with JSON doc save as '" << savedDoc.first << "-v" << savedDoc.second << "'" << __E__;
	return;
} //end SaveJSON_Document()

int main(int argc, char* argv[])
{
	if(getenv("OTSDAQ_LOG_FHICL") == NULL)
		setenv("OTSDAQ_LOG_FHICL", (std::string(__ENV__("USER_DATA")) + "/MessageFacilityConfigurations/MessageFacilityWithCout.fcl").c_str(), 1);

	if(getenv("OTSDAQ_LOG_ROOT") == NULL)
		setenv("OTSDAQ_LOG_ROOT", (std::string(__ENV__("USER_DATA")) + "/Logs").c_str(), 1);

	// INIT_MF("SaveJSON_Document");
	SaveJSON_Document(argc, argv);
	return 0;
}
// BOOST_AUTO_TEST_SUITE_END()
