cet_make_library(LIBRARY_NAME WorkLoopManager
	SOURCE WorkLoop.cc WorkLoopManager.cc
		   LIBRARIES
		   PUBLIC
		   ${XDAQ_BASIC_LIB_LIST}
		   PRIVATE
		   otsdaq::XmlUtilities
		   otsdaq::SOAPUtilities
)

install_headers()
install_source()
