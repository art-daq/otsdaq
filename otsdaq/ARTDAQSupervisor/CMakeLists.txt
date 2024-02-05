
#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()

cet_make_library(LIBRARY_NAME ARTDAQSupervisor
SOURCE
ARTDAQSupervisor.cc
ARTDAQSupervisorTRACEController.cc
version.cc
		 LIBRARIES PRIVATE
         Python::Python
		 artdaq_plugin_support::commanderMaker
		 otsdaq::CoreSupervisors
		 otsdaq::ARTDAQTableBase
)

install_headers()
install_source()
