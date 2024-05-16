cet_make_library(LIBRARY_NAME WizardSupervisor
SOURCE
WizardSupervisor.cc
version.cc
		 LIBRARIES PRIVATE
		 otsdaq::CoreSupervisors
		 otsdaq::CgiDataUtilities
		 otsdaq::GatewaySupervisor
          otsdaq::SupervisorInfo
	  Boost::filesystem
)
install_headers()
install_source()
