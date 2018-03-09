#include "config/version.h"
#include <xcept/version.h>
#include <xdaq/version.h>
#include "otsdaq-core/GatewaySupervisor/version.h"


GETPACKAGEINFO(GatewaySupervisor)


//========================================================================================================================
void GatewaySupervisor::checkPackageDependencies() throw (config::PackageInfo::VersionException)
{
	CHECKDEPENDENCY(config);
	CHECKDEPENDENCY(xcept );
	CHECKDEPENDENCY(xdaq  );
}

//========================================================================================================================
std::set<std::string, std::less<std::string> > GatewaySupervisor::getPackageDependencies()
{
	std::set<std::string, std::less<std::string> > dependencies;

	ADDDEPENDENCY(dependencies,config);
	ADDDEPENDENCY(dependencies,xcept );
	ADDDEPENDENCY(dependencies,xdaq  );

	return dependencies;
}