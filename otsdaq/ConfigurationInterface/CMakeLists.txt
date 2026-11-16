cet_register_export_set(SET_NAME CIPluginTypes NAMESPACE otsdaq_plugin_types)
cet_register_export_set(SET_NAME CIPluginSupport NAMESPACE otsdaq_plugin_support)

if(DEFINED ENV{OTS_LITE})
	set(OTS_ARTDAQ_APPLICATION otsdaq::XdaqLite)
else()
	set(OTS_ARTDAQ_APPLICATION artdaq::Application)
endif()

cet_make_library(LIBRARY_NAME IConfigurationInterface INTERFACE
		EXPORT_SET CIPluginTypes
		SOURCE ConfigurationInterface.h
		 LIBRARIES INTERFACE
		 otsdaq::TableCore
		 otsdaq::ProgressBar
		 otsdaq::Macros
)

cet_make_library(LIBRARY_NAME configInterfaceMaker
  EXPORT_SET CIPluginSupport
  SOURCE MakeConfigurationInterface.cc
  LIBRARIES
  PUBLIC
  otsdaq_plugin_types::IConfigurationInterface
  PRIVATE
  ${OTS_PLUGIN_FACTORY_LIBRARIES} # cetlib+fhiclcpp, or dl in maxlite
)


cet_register_export_set(SET_NAME configurationInterface SET_DEFAULT)
cet_make_library(LIBRARY_NAME ConfigurationInterface
		SOURCE ConfigurationInterface.cc ConfigurationHandler.cc ConfigurationManager.cc ConfigurationManagerRW.cc TimeFormatter.cc ConfigurationTree.cc
		 LIBRARIES PUBLIC
		 otsdaq_plugin_support::configInterfaceMaker
		 XercesC::XercesC
		 PRIVATE
		 otsdaq::XmlUtilities
		 ${OTS_ARTDAQ_APPLICATION}
)

include(BasicPlugin)
cet_write_plugin_builder(otsdaq::configInterface art::plugin Modules
SUFFIX configInterface INSTALL_BUILDER
LIBRARIES CONDITIONAL otsdaq::ConfigurationInterface)

include(otsdaq::configInterface)
if(NOT DEFINED ENV{OTS_MAXLITE}) # maxlite has no artdaq-database; it uses the Text plugin
cet_build_plugin(Database otsdaq::configInterface
LIBRARIES REG
artdaq-database::ConfigurationDB
otsdaq::XmlUtilities
PRIVATE
artdaq-database::ConfigurationDB
)
endif()

cet_build_plugin(File otsdaq::configInterface)

# plain text store (textdb://), no third-party dependencies
cet_build_plugin(Text otsdaq::configInterface)

install_headers()
install_source()
