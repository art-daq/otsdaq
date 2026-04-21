cet_make_library(LIBRARY_NAME CgiDataUtilities
SOURCE
CgiDataUtilities.cc
		LIBRARIES PUBLIC
		XDAQ::xgi
		PRIVATE
		otsdaq::Macros
)

install_headers()
install_source()
