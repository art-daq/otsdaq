
cet_register_export_set(SET_NAME FEPluginTypes NAMESPACE otsdaq_plugin_types)
cet_register_export_set(SET_NAME FEPluginSupport NAMESPACE otsdaq_plugin_support)

cet_make_library(LIBRARY_NAME IFEInterface INTERFACE
		EXPORT_SET FEPluginTypes
		SOURCE FEVInterface.h
		 LIBRARIES INTERFACE
		 otsdaq::MessageFacility
		 artdaq::DAQdata
		 ${XDAQ_XERCES-C}
)



cet_make_library(LIBRARY_NAME FrontEndInterfaceMaker
  EXPORT_SET FEPluginSupport
  SOURCE MakeInterface.cc
  LIBRARIES
  PUBLIC
  otsdaq_plugin_types::IFEInterface
  PRIVATE
  fhiclcpp::fhiclcpp
  cetlib::cetlib
)


cet_register_export_set(SET_NAME feCore SET_DEFAULT)
cet_make_library(LIBRARY_NAME FECore
	SOURCE FEProducerVInterface.cc FESlowControlsChannel.cc FESlowControlsWorkLoop.cc FEVInterface.cc FEVInterfacesManager.cc
		 LIBRARIES PUBLIC
		 otsdaq_plugin_support::FrontEndInterfaceMaker
		 otsdaq::DataManager
		 otsdaq::WorkLoopManager
		 otsdaq::Configurable
		 otsdaq::FiniteStateMachine
		 otsdaq::SupervisorInfo
		 artdaq::DAQdata
		 PRIVATE
		 otsdaq::NetworkUtilities
		 otsdaq::ConfigurationInterface
		 otsdaq::SOAPUtilities
)


include(BasicPlugin)
cet_write_plugin_builder(otsdaq::FEInterface art::plugin Modules
SUFFIX interface INSTALL_BUILDER
LIBRARIES CONDITIONAL otsdaq::FECore)


install_headers()
install_source()
