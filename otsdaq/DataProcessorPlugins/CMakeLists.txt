
#get_cmake_property(_variableNames VARIABLES)
#foreach(_variableName ${_variableNames})
#	message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()

include(otsdaq::dataProcessor)

cet_make_library(LIBRARY_NAME ARTDAQConsumer SOURCE ARTDAQConsumerImpl.cc LIBRARIES PUBLIC otsdaq::ARTDAQReaderCore PRIVATE otsdaq::DataManager)
cet_build_plugin(ARTDAQConsumer otsdaq::dataProcessor LIBRARIES REG otsdaq::ARTDAQConsumer  )

cet_make_library(LIBRARY_NAME ARTDAQProducer SOURCE ARTDAQProducerImpl.cc LIBRARIES PUBLIC otsdaq::ARTDAQReaderCore PRIVATE otsdaq::DataManager)
cet_build_plugin(ARTDAQProducer  otsdaq::dataProcessor LIBRARIES REG otsdaq::ARTDAQProducer )

cet_build_plugin(UDPDataListenerProducer  otsdaq::dataProcessor )

cet_build_plugin(UDPDataStreamerConsumer  otsdaq::dataProcessor )
  
cet_build_plugin(TCPDataReceiverProducer otsdaq::dataProcessor )

cet_build_plugin(TCPDataListenerProducer otsdaq::dataProcessor )

cet_build_plugin(RawDataSaverConsumer  otsdaq::dataProcessor )

cet_make_library(LIBRARY_NAME RawDataVisualizerConsumer SOURCE RawDataVisualizerConsumerImpl.cc 
	LIBRARIES PUBLIC 
		otsdaq::DataManager
		otsdaq::Configurable
		PRIVATE
		 otsdaq::ConfigurationInterface
		 otsdaq::NetworkUtilities  )
cet_build_plugin(RawDataVisualizerConsumer  otsdaq::dataProcessor LIBRARIES REG otsdaq::RawDataVisualizerConsumer  )


cet_build_plugin(OtsDataSaverConsumer otsdaq::dataProcessor )



install_headers()
install_source()
