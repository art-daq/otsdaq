
cet_register_export_set(SET_NAME SCPluginTypes NAMESPACE otsdaq_plugin_types)
cet_register_export_set(SET_NAME SCPluginSupport NAMESPACE otsdaq_plugin_support)

cet_make_library(LIBRARY_NAME SlowControlsInterface INTERFACE
		EXPORT_SET SCPluginTypes
		SOURCE SlowControlsVInterface.h
		 LIBRARIES INTERFACE
		 otsdaq::Configurable
		 otsdaq::FiniteStateMachine
)


cet_make_library(LIBRARY_NAME slowControlsMaker
  EXPORT_SET SCPluginSupport
  SOURCE MakeSlowControls.cc
  LIBRARIES
  PUBLIC
  otsdaq_plugin_types::SlowControlsInterface
  PRIVATE
  fhiclcpp::fhiclcpp
  cetlib::cetlib
)

cet_register_export_set(SET_NAME slowControlsCore SET_DEFAULT)
cet_make_library(LIBRARY_NAME SlowControlsCore
	SOURCE SlowControlsVInterface.cc
		LIBRARIES PUBLIC
		otsdaq_plugin_types::SlowControlsInterface
                )


include(BasicPlugin)
cet_write_plugin_builder(otsdaq::slowControls art::plugin Modules
SUFFIX slowcontrols INSTALL_BUILDER
LIBRARIES CONDITIONAL otsdaq::SlowControlsCore)

install_headers()
install_source()
