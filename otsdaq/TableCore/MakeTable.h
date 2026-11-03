#ifndef OTSDAQ_OTSDAQ_TABLECORE_MAKETABLE_H_
#define OTSDAQ_OTSDAQ_TABLECORE_MAKETABLE_H_
/// Using LibraryManager, find the correct library and return an instance of the specified
/// configuration interface.
///
#include <string>

namespace ots
{
class TableBase;

TableBase* makeTable(std::string const& tablePluginName);

}  // namespace ots

#endif  // OTSDAQ_OTSDAQ_TABLECORE_MAKETABLE_H_
