cet_make_library(LIBRARY_NAME BitManipulator 
SOURCE
BitManipulator.cc
         LIBRARIES PRIVATE otsdaq::MessageFacility
        )

install_headers()
install_source()
