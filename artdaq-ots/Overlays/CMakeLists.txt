cet_make_library(LIBRARY_NAME artdaq-ots_Overlays
SOURCE FragmentType.cc UDPFragment.cc
    LIBRARIES PUBLIC
  artdaq_core::artdaq-core_Utilities
  artdaq_core::artdaq-core_Data
  TRACE::TRACE
  )

  include(ArtdaqCorePlugins)
  cet_build_plugin(OTS artdaq::fragmentNameHelper LIBRARIES REG otsdaq::artdaq-ots_Overlays)


install_headers()
install_source()
