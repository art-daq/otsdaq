#include "otsdaq/CoreSupervisors/version.h"
#include <config/version.h>
#include <xcept/version.h>
#include <xdaq/version.h>

GETPACKAGEINFO(CoreSupervisors)

void CoreSupervisors::checkPackageDependencies()
{
	CHECKDEPENDENCY(config);
	CHECKDEPENDENCY(xcept);
	CHECKDEPENDENCY(xdaq);
}

std::set<std::string, std::less<std::string> > CoreSupervisors::getPackageDependencies()
{
	std::set<std::string, std::less<std::string> > dependencies;

	ADDDEPENDENCY(dependencies, config);
	ADDDEPENDENCY(dependencies, xcept);
	ADDDEPENDENCY(dependencies, xdaq);

	return dependencies;
}
