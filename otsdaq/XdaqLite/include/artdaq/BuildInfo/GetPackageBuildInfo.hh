// OTS_LITE stand-in for artdaq/BuildInfo/GetPackageBuildInfo.hh
#ifndef artdaq_BuildInfo_GetPackageBuildInfo_hh
#define artdaq_BuildInfo_GetPackageBuildInfo_hh

#include <string>

namespace artdaq
{
struct PackageBuildInfo
{
	std::string getPackageName() const { return "artdaq"; }
	std::string getPackageVersion() const { return "lite"; }
	std::string getBuildTimestamp() const { return ""; }
};
struct GetPackageBuildInfo
{
	static PackageBuildInfo getPackageBuildInfo() { return PackageBuildInfo(); }
};
}  // namespace artdaq

#endif
