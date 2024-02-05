#ifndef _ots_MakeDataProcessor_h_
#define _ots_MakeDataProcessor_h_
// Using LibraryManager, find the correct library and return an instance
// of the specified interface.

#include <string>

namespace ots
{
class DataProcessor;
class ConfigurationTree;

DataProcessor* makeDataProcessor(std::string const&       processorPluginName,
                                 std::string const&       supervisorApplicationUID,
                                 std::string const&       bufferUID,
                                 std::string const&       processorUID,
                                 ConfigurationTree const& configurationTree,
                                 std::string const&       pathToInterfaceConfiguration);
}  // namespace ots

#endif /* _ots_MakeDataProcessor_h_ */
