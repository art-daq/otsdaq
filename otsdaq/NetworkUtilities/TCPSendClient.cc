#include "otsdaq/NetworkUtilities/TCPSendClient.h"

using namespace ots;

//==============================================================================
TCPSendClient::TCPSendClient(const std::string& serverIP, int serverPort) : TCPClientBase(serverIP, serverPort) {}

//==============================================================================
TCPSendClient::~TCPSendClient(void) {}
