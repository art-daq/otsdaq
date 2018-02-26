#ifndef _CoreSupervisors_version_h_
#define _CoreSupervisors_version_h_

#include "config/PackageInfo.h"

#define MYPACKAGE_VERSION_MAJOR 3
#define MYPACKAGE_VERSION_MINOR 0
#define MYPACKAGE_VERSION_PATCH 0
#undef MYPACKAGE_PREVIOUS_VERSIONS  

#define MYPACKAGE_VERSION_CODE PACKAGE_VERSION_CODE(MYPACKAGE_VERSION_MAJOR, MYPACKAGE_VERSION_MINOR, MYPACKAGE_VERSION_PATCH)
#ifndef MYPACKAGE_PREVIOUS_VERSIONS
#define MYPACKAGE_FULL_VERSION_LIST  PACKAGE_VERSION_STRING(MYPACKAGE_VERSION_MAJOR, MYPACKAGE_VERSION_MINOR, MYPACKAGE_VERSION_PATCH)
#else 
#define MYPACKAGE_FULL_VERSION_LIST MYPACKAGE_PREVIOUS_VERSIONS "," PACKAGE_VERSION_STRING(MYPACKAGE_VERSION_MAJOR, MYPACKAGE_VERSION_MINOR, MYPACKAGE_VERSION_PATCH)
#endif 


namespace CoreSupervisors
{
	const std::string package     = "CoreSupervisors";
	const std::string versions    = MYPACKAGE_FULL_VERSION_LIST;
	const std::string summary     = "Core otsdaq supervisor base class";
	const std::string description = "Used for basic supervisor functionality including configuration and state transitions.";
	const std::string authors     = "Ryan Rivera, Lorenzo Uplegger";
	const std::string link        = "http://xdaq.web.cern.ch";
	config::PackageInfo getPackageInfo();
	void checkPackageDependencies() throw (config::PackageInfo::VersionException);
	std::set<std::string, std::less<std::string> > getPackageDependencies();
}

#endif
