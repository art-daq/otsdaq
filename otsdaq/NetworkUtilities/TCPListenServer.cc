#include "otsdaq/NetworkUtilities/TCPListenServer.h"
#include "otsdaq/NetworkUtilities/TCPReceiverSocket.h"

#include <iostream>

using namespace ots;

//==============================================================================
TCPListenServer::TCPListenServer(unsigned int serverPort, unsigned int maxNumberOfClients) : TCPServerBase(serverPort, maxNumberOfClients) {}

//==============================================================================
TCPListenServer::~TCPListenServer(void)
{
	//	std::cout << __PRETTY_FUNCTION__ << "Done" << std::endl;
}

std::string ots::TCPListenServer::receivePacket()
{
	if(!fConnectedClients.empty())
	{
		auto it = fConnectedClients.find(lastReceived);
		if(it == fConnectedClients.end() || ++it == fConnectedClients.end())
			it = fConnectedClients.begin();
		lastReceived = it->first;
		TLOG(25, "TCPListenServer") << "Reading from socket " << lastReceived << ", there are " << fConnectedClients.size() << " clients connected.";
		return dynamic_cast<TCPReceiverSocket*>(it->second)->receivePacket();
	}
	throw std::runtime_error("No clients connected!");
}

//==============================================================================
void TCPListenServer::acceptConnections()
{
	while(getAccept())
	{
		try
		{
			// __attribute__((unused)) TCPTransmitterSocket* clientSocket = acceptClient<TCPTransmitterSocket>();
			acceptClient<TCPReceiverSocket>();
		}
		catch(int e)
		{
			if(e == E_SHUTDOWN)
				break;
		}
	}
	// fAcceptPromise.set_value(true);
}
