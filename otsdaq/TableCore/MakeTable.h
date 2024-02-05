#ifndef _ots_makeTable_h_
#define _ots_makeTable_h_
// Using LibraryManager, find the correct library and return an instance of the specified
// configuration interface.

#include <string>

namespace ots
{
class TableBase;

TableBase* makeTable(std::string const& tablePluginName);

}  // namespace ots

#endif /* _ots_makeTable_h_ */
