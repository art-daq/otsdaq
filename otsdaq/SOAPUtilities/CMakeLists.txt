cet_make_library(LIBRARY_NAME SOAPUtilities
SOURCE
SOAPCommand.cc
SOAPMessenger.cc
SOAPParameters.cc
SOAPUtilities.cc
		   LIBRARIES PUBLIC
		   otsdaq::MessageFacility
		   ${XDAQ_BASIC_LIB_LIST}
)

install_headers()
install_source()
