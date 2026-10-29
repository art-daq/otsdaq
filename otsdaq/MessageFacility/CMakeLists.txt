if(DEFINED ENV{OTS_LITE})
	set(OTS_ARTDAQ_CORE_UTILITIES otsdaq::XdaqLite)
else()
	set(OTS_ARTDAQ_CORE_UTILITIES artdaq-core::Utilities)
endif()

cet_make_library(LIBRARY_NAME MessageFacility
	SOURCE MessageFacility.cc TRACEController.cc
  LIBRARIES PUBLIC
  otsdaq::Macros
  ${OTS_ARTDAQ_CORE_UTILITIES}
  )

install_headers()
install_source()
