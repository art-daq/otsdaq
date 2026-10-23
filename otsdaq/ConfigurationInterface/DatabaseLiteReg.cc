// Plugin registration half of the OTS_LITE "Database" configuration interface
// (cet_build_plugin splits a plugin into an implementation library and a registration
// library; the implementation is DatabaseLite_configInterface.cc).
#include "otsdaq/ConfigurationInterface/DatabaseLite_configInterface.h"
#include "otsdaq/Macros/ConfigurationInterfacePluginMacros.h"

using namespace ots;

DEFINE_OTS_CONFIGURATION_INTERFACE(DatabaseLiteConfigurationInterface)
