// OTS_LITE stand-in for xdaq's config/PackageInfo.h: the package-version boilerplate every
// otsdaq Supervisor's version.h/version.cc uses (GETPACKAGEINFO, CHECKDEPENDENCY, ...).
// Dependency checks are no-ops in lite; the version strings are kept so they still display.
#ifndef _config_PackageInfo_h_
#define _config_PackageInfo_h_

#include <set>
#include <string>

#define PACKAGE_VERSION_CODE(MAJOR, MINOR, PATCH) (((MAJOR) << 16) | ((MINOR) << 8) | (PATCH))
#define PACKAGE_VERSION_STRING_IMPL(X) #X
#define PACKAGE_VERSION_STRING_EXPAND(X) PACKAGE_VERSION_STRING_IMPL(X)
#define PACKAGE_VERSION_STRING(MAJOR, MINOR, PATCH) \
	PACKAGE_VERSION_STRING_EXPAND(MAJOR) "." PACKAGE_VERSION_STRING_EXPAND(MINOR) "." PACKAGE_VERSION_STRING_EXPAND(PATCH)

namespace config
{
class PackageInfo
{
  public:
	PackageInfo(const std::string& project, const std::string& package, const std::string& versions, const std::string& summary, const std::string& description, const std::string& authors, const std::string& link)
	    : project_(project), package_(package), versions_(versions), summary_(summary), description_(description), authors_(authors), link_(link)
	{
	}
	std::string getProject() const { return project_; }
	std::string getPackage() const { return package_; }
	std::string getVersions() const { return versions_; }
	std::string getSummary() const { return summary_; }
	std::string getDescription() const { return description_; }
	std::string getAuthors() const { return authors_; }
	std::string getLink() const { return link_; }

  private:
	std::string project_, package_, versions_, summary_, description_, authors_, link_;
};
}  // namespace config

#define GETPACKAGEINFO(NAMESPACE)                                                                                                            \
	config::PackageInfo NAMESPACE::getPackageInfo()                                                                                          \
	{                                                                                                                                        \
		return config::PackageInfo(NAMESPACE::project, NAMESPACE::package, NAMESPACE::versions, NAMESPACE::summary, NAMESPACE::description, \
		                           NAMESPACE::authors, NAMESPACE::link);                                                                     \
	}

#define CHECKDEPENDENCY(NAMESPACE) \
	do                             \
	{                              \
	} while(0)
#define ADDDEPENDENCY(SET, NAMESPACE) (SET).insert(#NAMESPACE)

#endif
