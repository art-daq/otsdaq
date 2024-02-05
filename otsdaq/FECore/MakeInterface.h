#ifndef _ots_MakeInterface_h_
#define _ots_MakeInterface_h_
// Using LibraryManager, find the correct library and return an instance
// of the specified interface.

#include <memory>
#include <string>

namespace ots
{
class FEVInterface;
class ConfigurationTree;

std::unique_ptr<FEVInterface> makeInterface(const std::string&       interfacePluginName,
                                            const std::string&       interfaceUID,
                                            const ConfigurationTree& configurationTree,
                                            const std::string&       pathToInterfaceConfiguration);
}  // namespace ots

#endif /* _ots_MakeInterface_h_ */
