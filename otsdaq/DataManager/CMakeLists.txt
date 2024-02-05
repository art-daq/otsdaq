

cet_register_export_set(SET_NAME DPPluginTypes NAMESPACE otsdaq_plugin_types)
cet_register_export_set(SET_NAME DPPluginSupport NAMESPACE otsdaq_plugin_support)

cet_make_library(LIBRARY_NAME IDataProcessor INTERFACE
		EXPORT_SET DPPluginTypes
		SOURCE DataProcessor.h
		 LIBRARIES INTERFACE
		 otsdaq::MessageFacility
		 otsdaq::WorkLoopManager
)

cet_make_library(LIBRARY_NAME dataProcessorMaker
  EXPORT_SET DPPluginSupport
  SOURCE MakeDataProcessor.cc
  LIBRARIES
  PUBLIC
  otsdaq_plugin_types::IDataProcessor
  PRIVATE
  fhiclcpp::fhiclcpp
  cetlib::cetlib
)


cet_register_export_set(SET_NAME dataManager SET_DEFAULT)
cet_make_library(LIBRARY_NAME DataManager
SOURCE CircularBufferBase.cc DataConsumer.cc DataManager.cc DataManagerSingleton.cc DataProcessor.cc DataProducer.cc DataProducerBase.cc RawDataSaverConsumerBase.cc
		LIBRARIES 
		otsdaq_plugin_support::dataProcessorMaker
		PRIVATE
		otsdaq::WorkLoopManager
		otsdaq::ConfigurationInterface
		otsdaq::Configurable
        )

include(BasicPlugin)
cet_write_plugin_builder(otsdaq::dataProcessor art::plugin Modules
SUFFIX processor INSTALL_BUILDER
LIBRARIES CONDITIONAL otsdaq::DataManager
		otsdaq::Configurable
		 otsdaq::ConfigurationInterface
		 otsdaq::NetworkUtilities)

install_headers()
install_source()
