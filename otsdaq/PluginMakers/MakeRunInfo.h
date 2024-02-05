#ifndef _ots_MakeRunInfo_h_
#define _ots_MakeRunInfo_h_
// Using LibraryManager, find the correct library and return an instance of the specified
// Run Info Interface.

#include <string>

namespace ots
{
class RunInfoVInterface;
class ConfigurationTree;

RunInfoVInterface* makeRunInfo(const std::string& runInfoPluginName, const std::string& runInfoUID);

}  // namespace ots

#endif /* _ots_MakeRunInfo_h_ */
