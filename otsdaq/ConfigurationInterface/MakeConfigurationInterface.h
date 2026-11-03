#ifndef OTSDAQ_OTSDAQ_CONFIGURATIONINTERFACE_MAKECONFIGURATIONINTERFACE_H_
#define OTSDAQ_OTSDAQ_CONFIGURATIONINTERFACE_MAKECONFIGURATIONINTERFACE_H_
/// Using LibraryManager, find the correct library and return an instance of the specified
/// configuration interface.
///
#include <string>

namespace ots
{
class ConfigurationInterface;

ConfigurationInterface* makeConfigurationInterface(
    std::string const& interfacePluginName);

}  // namespace ots

#endif  // OTSDAQ_OTSDAQ_CONFIGURATIONINTERFACE_MAKECONFIGURATIONINTERFACE_H_
