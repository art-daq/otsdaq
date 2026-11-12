#ifndef _ots_PluginFactory_h_
#define _ots_PluginFactory_h_

/// ots::PluginFactory
///	Loads otsdaq plugin libraries (tables, FE interfaces, data processors, ...).
///	In lite and full builds this is cetlib's cet::BasicPluginFactory.
///	In ots maxlite (no cetlib) it is a small dlopen/dlsym equivalent with the same
///	makePlugin<Result, Args...>(name, args...) call and the same library naming rule:
///		lib[<prefix>_]<name>_<suffix>.so, searched in CET_PLUGIN_PATH, then LD_LIBRARY_PATH.

#ifndef OTS_MAXLITE

#include <cetlib/BasicPluginFactory.h>
namespace ots
{
using PluginFactory = cet::BasicPluginFactory;
}

#else

#include <string>
#include <utility>

namespace ots
{
class PluginFactory
{
  public:
	explicit PluginFactory(std::string const& suffix    = "plugin",
	                       std::string const& makerName = "makePlugin")
	    : suffix_(suffix), makerName_(makerName)
	{
	}

	/// find library lib..._<libspec>_<suffix>.so and call its maker function
	template<typename RESULT_TYPE, typename... ARGS>
	RESULT_TYPE makePlugin(std::string const& libspec, ARGS&&... args) const
	{
		using maker_t = RESULT_TYPE (*)(ARGS...);
		maker_t maker = reinterpret_cast<maker_t>(findMaker(libspec));
		return maker(std::forward<ARGS>(args)...);
	}

  private:
	void* findMaker(std::string const& libspec) const;  ///< throws std::runtime_error

	std::string suffix_;
	std::string makerName_;
};
}  // namespace ots

#endif  // OTS_MAXLITE

#endif
