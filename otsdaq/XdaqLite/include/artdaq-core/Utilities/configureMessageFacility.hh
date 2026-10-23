// OTS_LITE stand-in for artdaq-core/Utilities/configureMessageFacility.hh.
// Same signatures; the lite implementation configures messagefacility directly with a
// console destination and (if ARTDAQ_LOG_ROOT is set) a per-application log file.
#ifndef artdaq_core_Utilities_configureMessageFacility_hh
#define artdaq_core_Utilities_configureMessageFacility_hh

#include <string>

namespace fhicl
{
class ParameterSet;
}

namespace artdaq
{
std::string generateMessageFacilityConfiguration(char const* progname, bool useConsole = true, bool printDebug = false, char const* fileExtraName = "");
void        configureTRACE(fhicl::ParameterSet& trace_pset);
void        configureMessageFacility(char const* progname, bool useConsole = true, bool printDebug = false);
std::string setMsgFacAppName(const std::string& appType, unsigned short port);
}  // namespace artdaq

#endif
