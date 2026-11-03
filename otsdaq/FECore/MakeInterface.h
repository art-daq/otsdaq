#ifndef OTSDAQ_OTSDAQ_FECORE_MAKEINTERFACE_H_
#define OTSDAQ_OTSDAQ_FECORE_MAKEINTERFACE_H_
/// Using LibraryManager, find the correct library and return an instance
/// of the specified interface.
///
#include <memory>
#include <string>

namespace ots
{
class FEVInterface;
class ConfigurationTree;

std::unique_ptr<FEVInterface> makeInterface(
    const std::string&       interfacePluginName,
    const std::string&       interfaceUID,
    const ConfigurationTree& configurationTree,
    const std::string&       pathToInterfaceConfiguration);
}  // namespace ots

#endif  // OTSDAQ_OTSDAQ_FECORE_MAKEINTERFACE_H_
