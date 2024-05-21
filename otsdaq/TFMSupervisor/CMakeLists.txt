
#get_cmake_property(_variableNames VARIABLES)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()

cet_make_library(LIBRARY_NAME TFMSupervisor
SOURCE
TFMSupervisor.cc
TFMSupervisorTRACEController.cc
version.cc
		 LIBRARIES PRIVATE
         Python::Python
		 artdaq_plugin_support::commanderMaker
		 otsdaq::CoreSupervisors
		 otsdaq::ARTDAQTableBase
		 Boost::filesystem
         xmlrpc_client
         xmlrpc
         xmlrpc_util
)

install_headers()
install_source()
