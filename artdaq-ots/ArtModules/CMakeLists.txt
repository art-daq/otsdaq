
cet_build_plugin(ArtdaqOtsBuildInfo art::EDProducer
LIBRARIES PRIVATE
artdaq::BuildInfo
artdaq-core::BuildInfo
otsdaq::artdaq-ots_BuildInfo
)

cet_build_plugin(UDPDump art::EDAnalyzer LIBRARIES PRIVATE artdaq-core::Data)
cet_build_plugin(JSONDispatcher art::EDAnalyzer LIBRARIES PRIVATE artdaq-core::Data)

install_headers()
install_source()
install_fhicl(SUBDIRS fcl)
