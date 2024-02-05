
cet_make_library(LIBRARY_NAME ARTDAQDataManager 
SOURCE ARTDAQDataManager.cc
		LIBRARIES PRIVATE
		
		otsdaq::DataManager
		otsdaq::ARTDAQReaderCore
		otsdaq::ARTDAQProducer
		otsdaq::ARTDAQConsumer
		artdaq::BuildInfo
        
        )
install_headers()
install_source()
