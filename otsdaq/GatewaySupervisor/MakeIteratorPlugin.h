#ifndef _ots_MakeIteratorPlugin_h_
#define _ots_MakeIteratorPlugin_h_
/// Using the cetlib plugin library manager, find the library named
///	<iteratorPluginName>_iteratorPlugin.so and return a new instance.
///	Throws if the library is not in the built environment.
#include <memory>
#include <string>

namespace ots
{
class IteratorPluginVInterface;

std::unique_ptr<IteratorPluginVInterface> makeIteratorPlugin(const std::string& iteratorPluginName);

}  // namespace ots

#endif /* _ots_MakeIteratorPlugin_h_ */
