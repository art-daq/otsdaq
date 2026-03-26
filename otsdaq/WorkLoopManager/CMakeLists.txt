cet_make_library(LIBRARY_NAME WorkLoopManager
	SOURCE WorkLoop.cc WorkLoopManager.cc
		   LIBRARIES
		   PUBLIC
		   XDAQ::toolbox
           XDAQ::xoap
           XDAQ::cgicc
		   PRIVATE
		   otsdaq::XmlUtilities
		   otsdaq::SOAPUtilities
)

install_headers()
install_source()
