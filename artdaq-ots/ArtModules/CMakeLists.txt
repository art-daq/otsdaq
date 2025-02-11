
cet_build_plugin(ArtdaqOtsBuildInfo art::EDProducer
LIBRARIES PRIVATE
artdaq::BuildInfo
artdaq_core::artdaq-core_BuildInfo
otsdaq::artdaq-ots_BuildInfo
)

cet_build_plugin(UDPDump art::EDAnalyzer LIBRARIES PRIVATE artdaq_core::artdaq-core_Data)
cet_build_plugin(JSONDispatcher art::EDAnalyzer LIBRARIES PRIVATE artdaq_core::artdaq-core_Data)

install_headers()
install_source()
install_fhicl(SUBDIRS fcl)
