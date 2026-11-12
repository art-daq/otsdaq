#include "otsdaq/Macros/PluginFactory.h"  // cet::BasicPluginFactory, or dlopen in maxlite

#include "otsdaq/TableCore/MakeTable.h"
#include "otsdaq/TableCore/TableBase.h"

namespace ots
{
TableBase* makeTable(std::string const& tablePluginName)
{
	static ots::PluginFactory basicPluginInterfaceFactory("table", "make");

	// TLOG_DEBUG(30) << "Making TablePlugin " << tablePluginName;
	auto ptr = basicPluginInterfaceFactory.makePlugin<TableBase*>(tablePluginName);

	// TLOG_DEBUG(30) << "Table Plugin " << tablePluginName << " is at " << static_cast<void*>(ptr);
	return ptr;
}
}  // namespace ots
