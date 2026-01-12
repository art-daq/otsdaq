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

// Shared test utilities
#include "otsdaq/Macros/TestUtilities.h"

/// usage:
/// otsdaq_load_json_document <document_name_to_load> <document_version_to_load> <path_to_save_JSON>
///
#define TRACE_NAME "LoadJSON_Document"

#undef __COUT__
#define __COUT__        \
	std::cout           \
	    << __MF_DECOR__ \
	    << __COUT_HDR_FL__  //TLOG(TLVL_DEBUG) //std::cout << __MF_DECOR__ << __COUT_HDR_FL__

using namespace ots;

void LoadJSON_Document(int argc, char* argv[])
{

	__COUT__ << "=================================================\n";
	__COUT__ << "=================================================\n";
	__COUT__ << "=================================================\n";
	__COUT__ << "\nLoading Trigger Document!" << std::endl;

	__COUT__ << "\n\nusage: Two arguments:\n\t <document_name_to_load> "
	            "<document_version_to_load> <path_to_save_JSON>"
	         << std::endl
	         << std::endl;

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
	//	Note: normally these environment variables are set by ots script

	test::util::check_and_make_envs();

	////////////////////////////////////////////////////

	//==============================================================================
	// get prepared with initial source db

	// ConfigurationManager instance immediately loads active groups
	__COUT__ << "Loading JSON Document..." << std::endl;
	ConfigurationManagerRW  cfgMgrInst("load_admin");
	ConfigurationManagerRW* cfgMgr = &cfgMgrInst;

	ConfigurationInterface* theInterface_ = cfgMgr->getConfigurationInterface();
	std::string             json =
	    theInterface_->loadCustomJSON(argv[1], TableVersion(atoi(argv[2])));
	__COUTVS__(3, json);
	FILE* fp = std::fopen(argv[3], "w");
	if(!fp)
	{
		__COUT__ << "\n\nERROR! Could not open file at " << argv[1]
		         << ". Error: " << errno << " - " << strerror(errno) << __E__;
		return;
	}
	fputs(json.c_str(), fp);
	fclose(fp);
	return;
}  //end LoadJSON_Document()

int main(int argc, char* argv[])
{
	if(getenv("OTSDAQ_LOG_FHICL") == NULL)
		setenv("OTSDAQ_LOG_FHICL",
		       (std::string(__ENV__("USER_DATA")) +
		        "/MessageFacilityConfigurations/MessageFacilityWithCout.fcl")
		           .c_str(),
		       1);

	if(getenv("OTSDAQ_LOG_ROOT") == NULL)
		setenv(
		    "OTSDAQ_LOG_ROOT", (std::string(__ENV__("USER_DATA")) + "/Logs").c_str(), 1);

	// INIT_MF("LoadJSON_Document");
	LoadJSON_Document(argc, argv);
	return 0;
}
// BOOST_AUTO_TEST_SUITE_END()
