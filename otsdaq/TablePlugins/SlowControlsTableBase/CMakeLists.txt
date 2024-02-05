
cet_make_library(LIBRARY_NAME SlowControlsTableBase
SOURCE
SlowControlsTableBase.cc
		LIBRARIES PUBLIC
  		otsdaq::ConfigurationInterface
)

install_headers()
install_source()
