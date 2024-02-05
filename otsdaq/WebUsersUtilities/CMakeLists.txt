cet_make_library(LIBRARY_NAME WebUsersUtilities
SOURCE RemoteWebUsers.cc
WebUsers.cc
		 LIBRARIES PUBLIC
		 otsdaq::MessageFacility
		 otsdaq::SOAPUtilities
		 otsdaq::TableCore
		 PRIVATE
		 otsdaq::XmlUtilities
		 otsdaq::CgiDataUtilities
		 otsdaq::SupervisorInfo
)

install_headers()
install_source()
