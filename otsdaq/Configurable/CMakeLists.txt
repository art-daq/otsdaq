cet_make_library(LIBRARY_NAME Configurable
		SOURCE Configurable.cc
		 LIBRARIES
		 otsdaq::ConfigurationInterface
		 otsdaq::XDAQContextTable
)


install_headers()
install_source()

