#ifndef _FEWSupervisor_version_h_
#define _FEWSupervisor_version_h_

#include "config/PackageInfo.h"

#define MYPACKAGE_VERSION_MAJOR OTSDAQ_MAJOR_VERSION
#define MYPACKAGE_VERSION_MINOR OTSDAQ_MINOR_VERSION
#define MYPACKAGE_VERSION_PATCH OTSDAQ_PATCH_VERSION
#undef MYPACKAGE_PREVIOUS_VERSIONS

#define MYPACKAGE_VERSION_CODE PACKAGE_VERSION_CODE(MYPACKAGE_VERSION_MAJOR, MYPACKAGE_VERSION_MINOR, MYPACKAGE_VERSION_PATCH)
#ifndef MYPACKAGE_PREVIOUS_VERSIONS
#define MYPACKAGE_FULL_VERSION_LIST PACKAGE_VERSION_STRING(MYPACKAGE_VERSION_MAJOR, MYPACKAGE_VERSION_MINOR, MYPACKAGE_VERSION_PATCH)
#else
#define MYPACKAGE_FULL_VERSION_LIST \
	MYPACKAGE_PREVIOUS_VERSIONS "," PACKAGE_VERSION_STRING(MYPACKAGE_VERSION_MAJOR, MYPACKAGE_VERSION_MINOR, MYPACKAGE_VERSION_PATCH)
#endif

namespace SimpleSoap
{
const std::string                              product     = "otsdaq";
const std::string                              package     = "SimpleSoap";
const std::string                              versions    = MYPACKAGE_FULL_VERSION_LIST;
const std::string                              summary     = "Hullo Mauro Example";
const std::string                              description = "A simple XDAQ application";
const std::string                              authors     = "Dario Menasce";
const std::string                              link        = "http://xdaq.web.cern.ch";
config::PackageInfo                            getPackageInfo();
void                                           checkPackageDependencies();
std::set<std::string, std::less<std::string> > getPackageDependencies();
}  // namespace SimpleSoap

#endif
