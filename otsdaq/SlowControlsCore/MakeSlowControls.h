#ifndef _ots_MakeSlowControls_h_
#define _ots_MakeSlowControls_h_
// Using LibraryManager, find the correct library and return an instance of the specified
// Slow Controls Interface.

#include <string>

namespace ots
{
class SlowControlsVInterface;
class ConfigurationTree;

SlowControlsVInterface* makeSlowControls(const std::string& slowControlsPluginName,
                                         const std::string& slowControlsUID  // Key value for (eventual) ControlsDashboard
                                                                             // Table in Configuration
                                         ,
                                         const ConfigurationTree& configurationTree  // Pass the big tree
                                         ,
                                         const std::string& pathToControlsConfiguration);  // Path to ControlsDashboard Table

}  // namespace ots

#endif /* _ots_MakeSlowControls_h_ */
