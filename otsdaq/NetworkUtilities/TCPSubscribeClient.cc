#include "otsdaq/NetworkUtilities/TCPSubscribeClient.h"

using namespace ots;

//==============================================================================
TCPSubscribeClient::TCPSubscribeClient(const std::string& serverIP, int serverPort) : TCPClientBase(serverIP, serverPort) {}

//==============================================================================
TCPSubscribeClient::~TCPSubscribeClient(void) {}
