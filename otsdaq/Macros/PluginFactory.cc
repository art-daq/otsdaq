// ots maxlite only (see PluginFactory.h): dlopen-based plugin lookup
#include "otsdaq/Macros/PluginFactory.h"

#include <dirent.h>
#include <dlfcn.h>
#include <cstdlib>
#include <map>
#include <mutex>
#include <stdexcept>
#include <vector>

namespace
{
std::mutex                                   pluginMutex_;
std::map<std::string /*path*/, void* /*dl*/> openLibraries_;

std::vector<std::string> searchDirectories(void)
{
	std::vector<std::string> dirs;
	for(const char* var : {"CET_PLUGIN_PATH", "LD_LIBRARY_PATH"})
	{
		const char* value = getenv(var);
		if(!value)
			continue;
		std::string list(value);
		size_t      start = 0;
		while(start <= list.size())
		{
			size_t end = list.find(':', start);
			if(end == std::string::npos)
				end = list.size();
			if(end > start)
				dirs.push_back(list.substr(start, end - start));
			start = end + 1;
		}
	}
	return dirs;
}

/// file name lib<middle>_<suffix>.so where middle is <libspec> or ends with _<libspec>
bool matches(const std::string& file,
             const std::string& libspec,
             const std::string& suffix)
{
	const std::string tail = "_" + suffix + ".so";
	if(file.size() <= 3 + tail.size() || file.compare(0, 3, "lib") != 0 ||
	   file.compare(file.size() - tail.size(), tail.size(), tail) != 0)
		return false;
	const std::string middle = file.substr(3, file.size() - 3 - tail.size());
	return middle == libspec ||
	       (middle.size() > libspec.size() &&
	        middle.compare(middle.size() - libspec.size(), libspec.size(), libspec) ==
	            0 &&
	        middle[middle.size() - libspec.size() - 1] == '_');
}
}  // namespace

//==============================================================================
void* ots::PluginFactory::findMaker(std::string const& libspec) const
{
	std::lock_guard<std::mutex> lock(pluginMutex_);

	std::string path;
	for(const auto& dir : searchDirectories())
	{
		DIR* dp = opendir(dir.c_str());
		if(!dp)
			continue;
		while(struct dirent* entry = readdir(dp))
			if(matches(entry->d_name, libspec, suffix_))
			{
				path = dir + "/" + entry->d_name;
				break;
			}
		closedir(dp);
		if(!path.empty())
			break;
	}
	if(path.empty())
		throw std::runtime_error(
		    "Library specification \"" + libspec +
		    "\" does not correspond to any library in CET_PLUGIN_PATH "
		    "or LD_LIBRARY_PATH of type \"" +
		    suffix_ + "\" (expected lib..._" + libspec + "_" + suffix_ + ".so).");

	void*& handle = openLibraries_[path];
	if(!handle)
	{
		handle = dlopen(path.c_str(), RTLD_LAZY | RTLD_GLOBAL);
		if(!handle)
		{
			const char* err = dlerror();
			openLibraries_.erase(path);
			throw std::runtime_error("Unable to load plugin library " + path + ": " +
			                         (err ? err : "unknown dlopen error"));
		}
	}
	dlerror();  // clear
	void* symbol = dlsym(handle, makerName_.c_str());
	if(!symbol)
	{
		const char* err = dlerror();
		throw std::runtime_error("Plugin library " + path + " has no function \"" +
		                         makerName_ + "\": " + (err ? err : "symbol not found"));
	}
	return symbol;
}  // end findMaker()
