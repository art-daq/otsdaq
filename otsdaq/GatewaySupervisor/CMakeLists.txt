cet_make_library(LIBRARY_NAME GatewaySupervisor
SOURCE
GatewaySupervisor.cc
Iterator.cc
version.cc
		 LIBRARIES PRIVATE
		 otsdaq::CoreSupervisors
		 otsdaq::IterateTable
		 otsdaq::CgiDataUtilities
		 otsdaq::WorkLoopManager
		 otsdaq::CodeEditor
		 otsdaq::SupervisorInfo
		 otsdaq::NetworkUtilities
		 otsdaq_plugin_support::runInfoMaker
		 xgi # From XDAQ
)


install_headers()
install_source()
