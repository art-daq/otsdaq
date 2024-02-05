cet_make_library(LIBRARY_NAME SupervisorInfo
		SOURCE
			AllSupervisorInfo.cc
			SupervisorDescriptorInfoBase.cc
			SupervisorInfo.cc
		 LIBRARIES 
		 PUBLIC
		 ${XDAQ_BASIC_LIB_LIST}
		 PRIVATE
		 otsdaq::MessageFacility
		 otsdaq::ConfigurationInterface
		 otsdaq::XDAQContextTable
)


install_headers()
install_source()
