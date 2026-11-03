#ifndef OTSDAQ_OTSDAQ_DATAMANAGER_MAKEDATAPROCESSOR_H_
#define OTSDAQ_OTSDAQ_DATAMANAGER_MAKEDATAPROCESSOR_H_
/// Using LibraryManager, find the correct library and return an instance
/// of the specified interface.
///
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

#endif  // OTSDAQ_OTSDAQ_DATAMANAGER_MAKEDATAPROCESSOR_H_
