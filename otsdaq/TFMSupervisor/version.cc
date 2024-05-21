#include "otsdaq/TFMSupervisor/version.h"
#include <config/version.h>
#include <xcept/version.h>
#include <xdaq/version.h>

GETPACKAGEINFO(TFMSupervisor)

void TFMSupervisor::checkPackageDependencies()
{
	CHECKDEPENDENCY(config);
	CHECKDEPENDENCY(xcept);
	CHECKDEPENDENCY(xdaq);
}

std::set<std::string, std::less<std::string> > TFMSupervisor::getPackageDependencies()
{
	std::set<std::string, std::less<std::string> > dependencies;

	ADDDEPENDENCY(dependencies, config);
	ADDDEPENDENCY(dependencies, xcept);
	ADDDEPENDENCY(dependencies, xdaq);

	return dependencies;
}
