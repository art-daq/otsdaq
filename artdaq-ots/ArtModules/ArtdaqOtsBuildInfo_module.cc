#include "artdaq/ArtModules/BuildInfo_module.hh"

#include "artdaq-core/BuildInfo/GetPackageBuildInfo.hh"
#include "artdaq-ots/BuildInfo/GetPackageBuildInfo.hh"
#include "artdaq/BuildInfo/GetPackageBuildInfo.hh"

#include <string>

namespace ots
{
static std::string instanceName = "ArtdaqOts";
typedef artdaq::BuildInfo<&instanceName,
                          artdaqcore::GetPackageBuildInfo,
                          artdaq::GetPackageBuildInfo,
                          ots::GetPackageBuildInfo>
    ArtdaqOtsBuildInfo;

DEFINE_ART_MODULE(ArtdaqOtsBuildInfo)
}  // namespace ots
