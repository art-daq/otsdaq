#ifndef OTSDAQ_OTSDAQ_FINITESTATEMACHINE_MAKERUNINFO_H_
#define OTSDAQ_OTSDAQ_FINITESTATEMACHINE_MAKERUNINFO_H_
/// Using LibraryManager, find the correct library and return an instance of the specified
/// Run Info Interface.
///
#include <string>

namespace ots
{
class RunInfoVInterface;
class ConfigurationTree;

RunInfoVInterface* makeRunInfo(const std::string& runInfoPluginName,
                               const std::string& runInfoUID);
/// ,  // Key value for (eventual) Run Info in Configuration
/// const ConfigurationTree& configurationTree,  // Pass the big tree
/// const std::string& pathToControlsConfiguration);  // Path to ControlsDashboard Table
///
}  // namespace ots

#endif  // OTSDAQ_OTSDAQ_FINITESTATEMACHINE_MAKERUNINFO_H_
