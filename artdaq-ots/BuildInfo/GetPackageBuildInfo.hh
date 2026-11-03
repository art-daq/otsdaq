#ifndef OTSDAQ_ARTDAQ_OTS_BUILDINFO_GETPACKAGEBUILDINFO_HH_
#define OTSDAQ_ARTDAQ_OTS_BUILDINFO_GETPACKAGEBUILDINFO_HH_

#include "artdaq-core/Data/PackageBuildInfo.hh"

#include <string>

namespace ots
{
struct GetPackageBuildInfo
{
	static artdaq::PackageBuildInfo getPackageBuildInfo();
};

}  // namespace ots

#endif  // OTSDAQ_ARTDAQ_OTS_BUILDINFO_GETPACKAGEBUILDINFO_HH_
