#include "config/version.h"
#include <xcept/version.h>
#include <xdaq/version.h>
#include "otsdaq/WizardSupervisor/version.h"

GETPACKAGEINFO(WizardSupervisor)

//==============================================================================
void WizardSupervisor::checkPackageDependencies()
{
	CHECKDEPENDENCY(config);
	CHECKDEPENDENCY(xcept);
	CHECKDEPENDENCY(xdaq);
}

//==============================================================================
std::set<std::string, std::less<std::string> > WizardSupervisor::getPackageDependencies()
{
	std::set<std::string, std::less<std::string> > dependencies;

	ADDDEPENDENCY(dependencies, config);
	ADDDEPENDENCY(dependencies, xcept);
	ADDDEPENDENCY(dependencies, xdaq);

	return dependencies;
}
