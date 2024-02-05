#ifndef _ots_makeConfigurationInterface_h_
#define _ots_makeConfigurationInterface_h_
// Using LibraryManager, find the correct library and return an instance of the specified
// configuration interface.

#include <string>

namespace ots
{
class ConfigurationInterface;

ConfigurationInterface* makeConfigurationInterface(std::string const& interfacePluginName);

}  // namespace ots

#endif /* _ots_makeConfigurationInterface_h_ */
