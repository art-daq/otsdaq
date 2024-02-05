
cet_register_export_set(SET_NAME RIPluginTypes NAMESPACE otsdaq_plugin_types)
cet_register_export_set(SET_NAME RIPluginSupport NAMESPACE otsdaq_plugin_support)

cet_make_library(LIBRARY_NAME RunInfoInterface INTERFACE
		EXPORT_SET RIPluginTypes
		SOURCE RunInfoVInterface.h
		 LIBRARIES INTERFACE
		 otsdaq::Configurable
		 otsdaq::Macros
)


cet_make_library(LIBRARY_NAME runInfoMaker
  EXPORT_SET RIPluginSupport
  SOURCE MakeRunInfo.cc
  LIBRARIES
  PUBLIC
  otsdaq_plugin_types::RunInfoInterface
  PRIVATE
  fhiclcpp::fhiclcpp
  cetlib::cetlib
)

cet_register_export_set(SET_NAME finiteStateMachine SET_DEFAULT)
cet_make_library(LIBRARY_NAME FiniteStateMachine
	SOURCE FiniteStateMachine.cc RunControlStateMachine.cc
		LIBRARIES PUBLIC
		otsdaq::ProgressBar
		${XDAQ_BASIC_LIB_LIST}
		PRIVATE
		otsdaq::Macros
		otsdaq::SOAPUtilities
                )

include(BasicPlugin)
cet_write_plugin_builder(otsdaq::runInfo art::plugin Modules
SUFFIX runInfo INSTALL_BUILDER
LIBRARIES CONDITIONAL otsdaq::FiniteStateMachine)

install_headers()
install_source()
