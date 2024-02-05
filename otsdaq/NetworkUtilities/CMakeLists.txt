cet_make_library(LIBRARY_NAME NetworkUtilities 
SOURCE
NetworkConverters.cc
NetworkDevice.cc
ReceiverSocket.cc
Socket.cc
TCPClient.cc
TCPClientBase.cc
TCPListenServer.cc
TCPPacket.cc
TCPPublishServer.cc
TCPReceiverSocket.cc
TCPSendClient.cc
TCPServer.cc
TCPServerBase.cc
TCPSocket.cc
TCPSubscribeClient.cc
TCPTransceiverSocket.cc
TCPTransmitterSocket.cc
TransceiverSocket.cc
TransmitterSocket.cc
UDPDataStreamerBase.cc
        LIBRARIES PUBLIC
        PRIVATE
        otsdaq::MessageFacility
        Boost::regex
        Threads::Threads
)

install_headers()
install_source()
