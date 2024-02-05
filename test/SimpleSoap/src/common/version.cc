#include "config/version.h"
#include <xdaq/version.h>
#include "Tests/SimpleSoap/include/version.h"
#include "xcept/version.h"

GETPACKAGEINFO(SimpleSoap)

void SimpleSoap::checkPackageDependencies()
{
	CHECKDEPENDENCY(config);
	CHECKDEPENDENCY(xcept);
	CHECKDEPENDENCY(xdaq);
}

std::set<std::string, std::less<std::string> > SimpleSoap::getPackageDependencies()
{
	std::set<std::string, std::less<std::string> > dependencies;

	ADDDEPENDENCY(dependencies, config);
	ADDDEPENDENCY(dependencies, xcept);
	ADDDEPENDENCY(dependencies, xdaq);

	return dependencies;
}
