cet_register_export_set(SET_NAME TPluginTypes NAMESPACE otsdaq_plugin_types)
cet_register_export_set(SET_NAME TPluginSupport NAMESPACE otsdaq_plugin_support)

cet_make_library(LIBRARY_NAME ITableBase INTERFACE
		EXPORT_SET TPluginTypes
		SOURCE TableBase.h
		LIBRARIES INTERFACE
		otsdaq::MessageFacility
)



cet_make_library(LIBRARY_NAME tablePluginMaker
  EXPORT_SET TPluginSupport
  SOURCE MakeTable.cc
  LIBRARIES
  PUBLIC
  otsdaq_plugin_types::ITableBase
  PRIVATE
  fhiclcpp::fhiclcpp
  cetlib::cetlib
)

cet_register_export_set(SET_NAME tableCore SET_DEFAULT)
cet_make_library(LIBRARY_NAME TableCore
	SOURCE TableBase.cc TableGroupKey.cc TableInfoReader.cc TableVersion.cc TableView.cc TableViewColumnInfo.cc
		 LIBRARIES PUBLIC
		otsdaq::MessageFacility
		otsdaq_plugin_support::tablePluginMaker
		LIBRARIES PRIVATE
		otsdaq::XmlUtilities
)

include(BasicPlugin)
cet_write_plugin_builder(otsdaq::table art::plugin Modules
SUFFIX table INSTALL_BUILDER
LIBRARIES CONDITIONAL otsdaq::TableCore otsdaq::ConfigurationInterface)


install_headers()
install_source()
  
