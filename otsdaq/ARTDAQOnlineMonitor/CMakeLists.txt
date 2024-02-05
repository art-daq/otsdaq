
cet_make_library(LIBRARY_NAME ARTDAQOnlineMonitor
SOURCE ARTDAQOnlineMonitorSupervisor.cc
		 LIBRARIES PRIVATE
		 otsdaq::CoreSupervisors
		 otsdaq::ARTDAQTableBase
		 Boost::thread
		 )

install_headers()
install_source()