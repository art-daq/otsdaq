cet_make_library(LIBRARY_NAME MessageFacility
	SOURCE MessageFacility.cc TRACEController.cc
  LIBRARIES PUBLIC
  otsdaq::Macros
  artdaq-core::Utilities
  )

install_headers()
install_source()
