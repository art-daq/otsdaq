
cet_make_library(LIBRARY_NAME ARTDAQReaderCore
SOURCE ARTDAQReaderProcessorBase.cc
		LIBRARIES PUBLIC
		otsdaq::Configurable
		artdaq::Application
		PRIVATE
		
        )
install_headers()
install_source()
