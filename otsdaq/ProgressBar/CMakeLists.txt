cet_make_library(LIBRARY_NAME ProgressBar 
SOURCE ProgressBar.cc
	LIBRARIES PUBLIC
	otsdaq::MessageFacility
	)

install_headers()
install_source()
