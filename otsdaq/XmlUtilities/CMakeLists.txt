
cet_make_library(LIBRARY_NAME XmlUtilities
	SOURCE ConvertFromXML.cc ConvertToXML.cc DOMTreeErrorReporter.cc HttpXmlDocument.cc XmlDocument.cc
	LIBRARIES PUBLIC
	 ${XDAQ_XERCES-C}
	 LIBRARIES PRIVATE
	 otsdaq::MessageFacility
	 Boost::regex
        )

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()

install_headers()
install_source()
