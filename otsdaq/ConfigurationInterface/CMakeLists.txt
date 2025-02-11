cet_register_export_set(SET_NAME CIPluginTypes NAMESPACE otsdaq_plugin_types)
cet_register_export_set(SET_NAME CIPluginSupport NAMESPACE otsdaq_plugin_support)

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
  fhiclcpp::fhiclcpp
  cetlib::cetlib
)


cet_register_export_set(SET_NAME configurationInterface SET_DEFAULT)
cet_make_library(LIBRARY_NAME ConfigurationInterface
		SOURCE ConfigurationInterface.cc ConfigurationHandler.cc ConfigurationManager.cc ConfigurationManagerRW.cc TimeFormatter.cc ConfigurationTree.cc
		 LIBRARIES PUBLIC
		 otsdaq_plugin_support::configInterfaceMaker
		 PRIVATE
		 otsdaq::XmlUtilities
		 artdaq::Application
)

include(BasicPlugin)
cet_write_plugin_builder(otsdaq::configInterface art::plugin Modules
SUFFIX configInterface INSTALL_BUILDER
LIBRARIES CONDITIONAL otsdaq::ConfigurationInterface)

include(otsdaq::configInterface)
cet_build_plugin(Database otsdaq::configInterface
LIBRARIES REG
artdaq_database::artdaq-database_ConfigurationDB
PRIVATE
artdaq_database::artdaq-database_ConfigurationDB
)

cet_build_plugin(File otsdaq::configInterface)

install_headers()
install_source()
