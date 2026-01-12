#ifndef _ots_test_utils_h_
#define _ots_test_utils_h_

#include "otsdaq/Macros/StringMacros.h"

using namespace ots;

namespace ots::test::util
{
void check_and_make_envs()
{
	//==============================================================================
	// Define environment variables
	//	Note: normally these environment variables are set by ots script

	if(getenv("OTSDAQ_LOG_DIR") == NULL)
		setenv(
		    "OTSDAQ_LOG_DIR", (std::string(__ENV__("USER_DATA")) + "/Logs").c_str(), 1);

	if(getenv("OTSDAQ_LOG_ROOT") == NULL)
		setenv("OTSDAQ_LOG_ROOT", __ENV__("OTSDAQ_LOG_DIR"), 1);

	if(getenv("OTSDAQ_LOG_FHICL") == NULL)
		setenv("OTSDAQ_LOG_FHICL",
		       (std::string(__ENV__("USER_DATA")) +
		        "/MessageFacilityConfigurations/MessageFacilityWithCout_dev.fcl")
		           .c_str(),
		       1);

	// The configuration uses __ENV__("SERVICE_DATA_PATH") in init() so define it if it is not defined
	if(getenv("SERVICE_DATA_PATH") == NULL)
		setenv("SERVICE_DATA_PATH",
		       (std::string(__ENV__("USER_DATA")) + "/ServiceData").c_str(),
		       1);

	// These are needed by
	// otsdaq/otsdaq/ConfigurationDataFormats/ConfigurationInfoReader.cc [207]
	if(getenv("CONFIGURATION_TYPE") == NULL)
		setenv("CONFIGURATION_TYPE", "File", 1);  // Can be File, Database, DatabaseTest

	if(getenv("CONFIGURATION_DATA_PATH") == NULL)
		setenv("CONFIGURATION_DATA_PATH",
		       (std::string(getenv("USER_DATA")) + "/ConfigurationDataExamples").c_str(),
		       1);

	if(getenv("TABLE_INFO_PATH") == NULL)
		setenv("TABLE_INFO_PATH",
		       (std::string(getenv("USER_DATA")) + "/TableInfo").c_str(),
		       1);
	////////////////////////////////////////////////////

	// Some configuration plug-ins use __ENV__("OTSDAQ_LIB") and
	// __ENV__("OTSDAQ_UTILITIES_LIB") in init() so define it 	to a non-sense place is ok
	if(getenv("OTSDAQ_LIB") == NULL)
		setenv("OTSDAQ_LIB", (std::string(getenv("USER_DATA")) + "/").c_str(), 1);
	if(getenv("OTSDAQ_UTILITIES_LIB") == NULL)
		setenv(
		    "OTSDAQ_UTILITIES_LIB", (std::string(getenv("USER_DATA")) + "/").c_str(), 1);

	// Some configuration plug-ins use __ENV__("OTS_MAIN_PORT") in init() so define it
	if(getenv("OTS_MAIN_PORT") == NULL)
		setenv("OTS_MAIN_PORT", "2015", 1);

	// also xdaq envs for XDAQContextTable
	if(getenv("XDAQ_CONFIGURATION_DATA_PATH") == NULL)
		setenv("XDAQ_CONFIGURATION_DATA_PATH",
		       (std::string(getenv("USER_DATA")) + "/XDAQConfigurations").c_str(),
		       1);

	if(getenv("XDAQ_CONFIGURATION_XML") == NULL)
		setenv("XDAQ_CONFIGURATION_XML", "otsConfigurationNoRU_CMake", 1);
}
}  // namespace ots::test::util

#endif
