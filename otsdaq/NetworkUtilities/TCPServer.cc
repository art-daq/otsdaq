#include "otsdaq/NetworkUtilities/TCPServer.h"
#include <errno.h>   // errno
#include <string.h>  // errno
#include "otsdaq/NetworkUtilities/TCPTransceiverSocket.h"

#include <iostream>
using namespace ots;

//==============================================================================
TCPServer::TCPServer(unsigned int serverPort, unsigned int maxNumberOfClients) : TCPServerBase(serverPort, maxNumberOfClients), fInDestructor(false)

{
	fReceiveTimeout.tv_sec  = 0;
	fReceiveTimeout.tv_usec = 0;
	fSendTimeout.tv_sec     = 0;
	fSendTimeout.tv_usec    = 0;
	// fAcceptFuture = std::async(std::launch::async, &TCPServer::acceptConnections,
	// this);
}

//==============================================================================
TCPServer::~TCPServer(void) { fInDestructor = true; }

//==============================================================================
// time out or protection for this receive method?
// void TCPServer::connectClient(int fdClientSocket)
void TCPServer::connectClient(TCPTransceiverSocket* socket)
{
	while(true)
	{
		// std::cout << __PRETTY_FUNCTION__ << "Waiting for message for socket  #: " << socket->getSocketId() << std::endl;
		std::string message;
		try
		{
			// The server should only communicate with packets
			//...otherwise the messages could be incomplete
			message = socket->receivePacket();
		}
		catch(const std::exception& e)
		{
			if(!fInDestructor)
			{
				std::cout << __PRETTY_FUNCTION__ << "Error client socket #" << socket->getSocketId() << ": " << e.what()
				          << std::endl;  // Client connection must have closed
				TCPServerBase::closeClientSocket(socket->getSocketId());
				interpretMessage("Error: " + std::string(e.what()));
			}
			return;
		}

		// std::cout << __PRETTY_FUNCTION__
		//<< "Received message:-" << message << "-"
		//<< "Message Length=" << message.length() << " From socket #: " << socket->getSocketId() << std::endl;
		std::string messageToClient = interpretMessage(message);

		// Send back something only if there is actually a message to be sent!
		if(messageToClient != "")
		{
			// std::cout << __PRETTY_FUNCTION__ << "Sending back message:-" << messageToClient << "-(nbytes=" << messageToClient.length() << ") to socket #: "
			// << socket->getSocketId() << std::endl;
			socket->sendPacket(messageToClient);
		}
		// else
		// 	std::cout << __PRETTY_FUNCTION__ << "Not sending anything back to socket  #: " << socket->getSocketId() << std::endl;

		// std::cout << __PRETTY_FUNCTION__ << "After message sent now checking for more... socket #: " << socket->getSocketId() << std::endl;
	}
	// If the socket is removed then this line will crash.
	// It is crucial then to have the return when the exception is caught and the socket is closed!
	std::cout << __PRETTY_FUNCTION__ << "Thread done for socket  #: " << socket->getSocketId() << std::endl;
	//	std::cout << __PRETTY_FUNCTION__ << "Thread done for socket!" << std::endl;
}

//==============================================================================
void TCPServer::acceptConnections()
{
	// std::pair<std::unordered_map<int, TCPTransceiverSocket>::iterator, bool> element;
	while(true)
	{
		try
		{
			TCPTransceiverSocket* clientSocket = acceptClient<TCPTransceiverSocket>();
			clientSocket->setReceiveTimeout(fReceiveTimeout.tv_sec, fReceiveTimeout.tv_usec);
			clientSocket->setSendTimeout(fSendTimeout.tv_sec, fSendTimeout.tv_usec);
			if(fConnectedClientsFuture.find(clientSocket->getSocketId()) != fConnectedClientsFuture.end())
				fConnectedClientsFuture.erase(fConnectedClientsFuture.find(clientSocket->getSocketId()));
			fConnectedClientsFuture.emplace(clientSocket->getSocketId(), std::async(std::launch::async, &TCPServer::connectClient, this, clientSocket));
			// fConnectedClientsFuture.emplace(clientSocket->getSocketId(), std::thread(&TCPServer::connectClient, this, clientSocket));
		}
		catch(int e)
		{
			if(e == E_SHUTDOWN)
				break;
		}
	}
	// fAcceptPromise.set_value(true);
}

//==============================================================================
void TCPServer::setReceiveTimeout(unsigned int timeoutSeconds, unsigned int timeoutMicroseconds)
{
	fReceiveTimeout.tv_sec  = timeoutSeconds;
	fReceiveTimeout.tv_usec = timeoutMicroseconds;
}

//==============================================================================
void TCPServer::setSendTimeout(unsigned int timeoutSeconds, unsigned int timeoutMicroseconds)
{
	fSendTimeout.tv_sec  = timeoutSeconds;
	fSendTimeout.tv_usec = timeoutMicroseconds;
}
