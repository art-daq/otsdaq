
cet_make_library(LIBRARY_NAME ARTDAQTableBase
SOURCE ARTDAQTableBase.cc
		LIBRARIES 
        PUBLIC
  		otsdaq::ConfigurationInterface
        PRIVATE
  		otsdaq::XDAQContextTable
)

install_headers()
install_source()
