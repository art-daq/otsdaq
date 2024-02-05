
  include(artdaq::commandableGenerator)
  cet_build_plugin(UDPReceiver artdaq::commandableGenerator LIBRARIES REG otsdaq::artdaq-ots_Overlays canvas::canvas)

install_headers()
install_source()
