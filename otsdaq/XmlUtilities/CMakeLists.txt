
cet_make_library(LIBRARY_NAME XmlUtilities
	SOURCE ConvertFromXML.cc ConvertToXML.cc DOMTreeErrorReporter.cc HttpXmlDocument.cc XmlDocument.cc XercesPlatform.cc
	LIBRARIES PUBLIC
      XercesC::XercesC
	 LIBRARIES PRIVATE
	 otsdaq::MessageFacility
        )

#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()

install_headers()
install_source()
