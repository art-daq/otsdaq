cet_make_library(LIBRARY_NAME SOAPUtilities
SOURCE
SOAPCommand.cc
SOAPMessenger.cc
SOAPParameters.cc
SOAPUtilities.cc
		   LIBRARIES PUBLIC
		   otsdaq::MessageFacility
           XDAQ::xdaq
           XDAQ::xoap
           XDAQ::xcept
           XercesC::XercesC
)

install_headers()
install_source()
