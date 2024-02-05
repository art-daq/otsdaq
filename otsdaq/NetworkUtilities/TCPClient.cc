#include "otsdaq/NetworkUtilities/TCPClient.h"

using namespace ots;

//==============================================================================
TCPClient::TCPClient(const std::string& serverIP, int serverPort) : TCPClientBase(serverIP, serverPort) {}

//==============================================================================
TCPClient::~TCPClient(void) {}
