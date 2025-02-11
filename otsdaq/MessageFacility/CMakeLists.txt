cet_make_library(LIBRARY_NAME MessageFacility
	SOURCE MessageFacility.cc TRACEController.cc
  LIBRARIES PUBLIC
  otsdaq::Macros
  artdaq_core::artdaq-core_Utilities
  )

install_headers()
install_source()
