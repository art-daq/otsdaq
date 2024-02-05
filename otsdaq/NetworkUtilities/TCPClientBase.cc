#include "otsdaq/NetworkUtilities/TCPClientBase.h"
#include "otsdaq/Macros/CoutMacros.h"

#include <arpa/inet.h>  // inet_aton
#include <netdb.h>
#include <netinet/in.h>  // inet_aton, struct sockaddr_in
#include <strings.h>     // bzero
#include <boost/regex.hpp>
#include <iostream>
#include <thread>

using namespace ots;

//==============================================================================
TCPClientBase::TCPClientBase(const std::string& serverIP, int serverPort) : fServerIP(serverIP), fServerPort(serverPort), fConnected(false) {}

//==============================================================================
TCPClientBase::~TCPClientBase(void)
{
	//__COUT__ << "Closing TCPSocket #" << getSocketId() << std::endl;
	if(fConnected)
		close();
	//__COUT__ << "TCPSocket #" << getSocketId() << " closed." << std::endl;
}

//==============================================================================
bool TCPClientBase::connect(int retry, unsigned int sleepMilliSeconds)
{
	std::cout << __PRETTY_FUNCTION__ << "Connecting Client socket to server name-" << fServerIP << "-serverPort: " << fServerPort << " already connected? "
	          << fConnected << std::endl;
	if(fConnected)
	{
		std::stringstream error;
		error << "ERROR: This client is already connected. This must never happens. It probably means that the connect method is called multiple times before "
		         "the TCPClient has been disconnected.";
		throw std::runtime_error(error.str());
	}

	std::cout << __PRETTY_FUNCTION__ << "Connecting Client socket to server name-" << fServerIP << "-serverPort: " << fServerPort << std::endl;
	std::string serverName = fServerIP;
	resolveServer(fServerIP);
	__COUT__ << "Connecting Client socket to server ip  -" << fServerIP << "-serverPort: " << fServerPort << std::endl;
	int                status = invalidSocketId;
	struct sockaddr_in serverSocketAddress;
	serverSocketAddress.sin_family      = AF_INET;
	serverSocketAddress.sin_port        = htons(fServerPort);
	serverSocketAddress.sin_addr.s_addr = inet_addr(fServerIP.c_str());

	int totalTries = retry;
	while(!fConnected && (unsigned int)retry-- > 0)
	{
		std::cout << __PRETTY_FUNCTION__ << "Trying to connect" << std::endl;
		TCPSocket::open();
		status = ::connect(getSocketId(), (struct sockaddr*)&serverSocketAddress, sizeof(serverSocketAddress));
		std::cout << __PRETTY_FUNCTION__ << "Done Connect with status: " << status << std::endl;
		if(status == -1)
		{
			if((unsigned int)retry > 0)
			{
				std::cout << __PRETTY_FUNCTION__ << "WARNING: Can't connect to " << serverName << ". The server might still be down...Sleeping "
				          << sleepMilliSeconds << "ms and then retry " << (unsigned int)retry << " more times." << std::endl;
				std::this_thread::sleep_for(std::chrono::milliseconds(sleepMilliSeconds));
				continue;
			}
			else
			{
				std::stringstream error;
				error << "ERROR: Can't connect to " << serverName << ". The server might still be down after " << totalTries * sleepMilliSeconds / 1000.
				      << " seconds and " << totalTries << " connection attempts!";
				fConnected = false;
				throw std::runtime_error(error.str());
			}
		}

		//		if (sendBufferSize > 0)
		//		{
		//			int       socketLength       = 0;
		//			socklen_t sizeOfSocketLength = sizeof(socketLength);
		//			status = getsockopt(getSocketId(), SOL_SOCKET, SO_SNDBUF,
		//&socketLength, &sizeOfSocketLength); 			__COUT__
		//<< "TCPConnect sendBufferSize initial: " << socketLength << " status/errno=" <<
		// status << "/" << errno << " sizeOfSocketLength=" << sizeOfSocketLength <<
		// std::endl;
		//
		//			socketLength = sendBufferSize;
		//			status = setsockopt(getSocketId(), SOL_SOCKET, SO_SNDBUF,
		//&socketLength,  sizeOfSocketLength); 			if (status == -1)
		// std::cout <<
		//__PRETTY_FUNCTION__ <<  "Error with setsockopt sendBufferSize " << errno <<
		// std::endl ; 			socketLength = 0;
		//			status = getsockopt(getSocketId(), SOL_SOCKET, SO_SNDBUF,
		//&socketLength, &sizeOfSocketLength); 			if (socketLength < (sendBufferSize
		//* 2)) 				__COUT__ <<  "sendBufferSize " <<
		// socketLength
		//<< " not expected (" << sendBufferSize << " status/errno=" << status << "/" <<
		// errno << ")" << std::endl; 			else 				std::cout <<
		//__PRETTY_FUNCTION__ <<  "sendBufferSize " << socketLength << " status/errno=" <<
		// status << "/" << errno << std::endl;
		//		}
		__COUT__ << "Succesfully connected to server " << fServerIP << " port: " << fServerPort << std::endl;
		fConnected = true;
	}

	return fConnected;
}
//==============================================================================
bool TCPClientBase::disconnect(void)
{
	if(fConnected)
	{
		TCPSocket::sendClose();
		TCPSocket::close();
		fConnected = false;
	}
	return !fConnected;
}

//==============================================================================
// private
void TCPClientBase::resolveServer(std::string& serverIP)
{
	const std::string ipv4(
	    "(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)"
	    "\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)"
	    "\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)"
	    "\\.(25[0-5]|2[0-4][0-9]|[01]?[0-9][0-9]?)");
	boost::regex ip_regex(ipv4.c_str());

	// __COUT__ << "Checking exp1: " << serverIP << std::endl;
	if(boost::regex_match(serverIP, ip_regex))  // It is already in the correct format!
		return;
	else if(serverIP == "localhost" || serverIP == "localhost.localdomain")
	{
		serverIP = "127.0.0.1";
	}
	else
	{
		struct hostent* resolvedHost = ::gethostbyname(serverIP.c_str());
		if(resolvedHost == NULL)
		{
			throw std::runtime_error(serverIP + " is unavailable and can't be resolved!");
			abort();
		}
		in_addr* address = (in_addr*)resolvedHost->h_addr;
		serverIP         = inet_ntoa(*address);
		std::cout << "IP: (" << serverIP << ")\n";
	}
}
