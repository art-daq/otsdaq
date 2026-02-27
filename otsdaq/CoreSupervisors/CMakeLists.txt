
#get_cmake_property(_variableNames VARIABLES)
#foreach(_variableName ${_variableNames})
#	message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()

find_package(cppzmq REQUIRED) # for publish/subscribe of data from FE to Supervisor

cet_make_library(LIBRARY_NAME CoreSupervisors
		SOURCE
		ARTDAQDataManagerSupervisor.cc
		ARTDAQFEDataManagerSupervisor.cc
		ConfigurationSupervisorBase.cc
		CorePropertySupervisorBase.cc
		CoreSupervisorBase.cc
		DataManagerSupervisor.cc
		FEDataManagerSupervisor.cc
		FESupervisor.cc
		version.cc
		LIBRARIES PUBLIC
		artdaq::DAQdata
		otsdaq::ConfigurationInterface
		otsdaq::XmlUtilities
		otsdaq::DesktopIconTable
		otsdaq::MessageFacility
		otsdaq::WebUsersUtilities
		otsdaq::FiniteStateMachine
		otsdaq::XDAQContextTable
		${XDAQ_BASIC_LIB_LIST}
		config # From XDAQ
		otsdaq::FECore
		otsdaq::SOAPUtilities
		otsdaq::CgiDataUtilities
		otsdaq::ARTDAQTableBase
		otsdaq::ARTDAQDataManager
		xgi # From XDAQ for xgi::Output::getHTTPResponseHeader
		cppzmq # for publish/subscribe of data from FE to Supervisor
		)

install_headers()
install_source()
