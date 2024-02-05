#include "otsdaq/NetworkUtilities/Socket.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <cassert>
#include <iostream>
#include <sstream>

#include <arpa/inet.h>
#include <unistd.h>
// #include <sys/socket.h>
#include <netdb.h>
// #include <ifaddrs.h>
// #include <sys/ioctl.h>
// #if defined(SIOCGIFHWADDR)
// #include <net/if.h>
// #else
// #include <net/if_dl.h>
// #endif
// #include <cstdlib>
#include <cstring>
// #include <cstdio>

using namespace ots;

//==============================================================================
Socket::Socket(const std::string& IPAddress, unsigned int port) : socketNumber_(-1), IPAddress_(IPAddress), requestedPort_(port)
//    maxSocketSize_(maxSocketSize)
{
	__COUT__ << "Socket constructor " << IPAddress << ":" << port << __E__;

	if(port >= (1 << 16))
	{
		__SS__ << "FATAL: Invalid Port " << port << ". Max port number is " << (1 << 16) - 1 << "." << std::endl;
		// assert(0); //RAR changed to exception on 8/17/2016
		__COUT_ERR__ << "\n" << ss.str();
		__SS_THROW__;
	}

	// network stuff
	socketAddress_.sin_family = AF_INET;      // use IPv4 host byte order
	socketAddress_.sin_port   = htons(port);  // short, network byte order

	__COUT__ << "IPAddress: " << IPAddress << " port: " << port << " htons: " << socketAddress_.sin_port << std::endl;

	if(inet_aton(IPAddress.c_str(), &socketAddress_.sin_addr) == 0)
	{
		__SS__ << "FATAL: Invalid IP:Port combination. Please verify... " << IPAddress << ":" << port << std::endl;
		// assert(0); //RAR changed to exception on 8/17/2016
		__COUT_ERR__ << "\n" << ss.str();
		__SS_THROW__;
	}

	memset(&(socketAddress_.sin_zero), '\0', 8);  // zero the rest of the struct

	__COUT__ << "Constructed socket for port " << ntohs(socketAddress_.sin_port) << "=" << getPort() << " htons: " << socketAddress_.sin_port << std::endl;
}

//==============================================================================
// protected constructor
Socket::Socket(void)
{
	__SS__ << "ERROR: This method should never be called. This is the protected "
	          "constructor. There is something wrong in your inheritance scheme!"
	       << std::endl;
	__COUT_ERR__ << "\n" << ss.str();

	__SS_THROW__;
}

//==============================================================================
Socket::~Socket(void)
{
	__COUT__ << "CLOSING THE SOCKET #" << socketNumber_ << " IP: " << IPAddress_ << " port: " << getPort() << " htons: " << socketAddress_.sin_port
	         << std::endl;
	if(socketNumber_ != -1)
		close(socketNumber_);
}

//==============================================================================
void Socket::initialize(unsigned int socketReceiveBufferSize)
{
	__COUT__ << "Initializing port " << ntohs(socketAddress_.sin_port) << " htons: " << socketAddress_.sin_port << std::endl;
	struct addrinfo  hints;
	struct addrinfo* res;
	int              status = 0;

	// first, load up address structs with getaddrinfo():
	memset(&hints, 0, sizeof hints);
	hints.ai_family   = AF_INET;     // use IPv4 for OtsUDPHardware
	hints.ai_socktype = SOCK_DGRAM;  // SOCK_DGRAM
	hints.ai_flags    = AI_PASSIVE;  // fill in my IP for me

	bool socketInitialized = false;
	int  fromPort          = FirstSocketPort;
	int  toPort            = LastSocketPort;

	if(ntohs(socketAddress_.sin_port) != 0)
		fromPort = toPort = ntohs(socketAddress_.sin_port);

	std::stringstream port;

	for(int p = fromPort; p <= toPort && !socketInitialized; p++)
	{
		port.str("");
		port << p;
		__COUT__ << "]\tBinding port: " << port.str() << std::endl;
		socketAddress_.sin_port = htons(p);  // short, network byte order

		if((status = getaddrinfo(NULL, port.str().c_str(), &hints, &res)) != 0)
		{
			__COUT__ << "]\tGetaddrinfo error status: " << status << std::endl;
			continue;
		}

		// make a socket:
		socketNumber_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

		__COUT__ << "]\tSocket Number: " << socketNumber_ << " for port: " << ntohs(socketAddress_.sin_port) << " initialized." << std::endl;
		// bind it to the port we passed in to getaddrinfo():
		if(bind(socketNumber_, res->ai_addr, res->ai_addrlen) == -1)
		{
			__COUT__ << "Error********Error********Error********Error********Error******"
			            "**Error"
			         << std::endl;
			__COUT__ << "FAILED BIND FOR PORT: " << port.str() << " ON IP: " << IPAddress_ << std::endl;
			__COUT__ << "Error********Error********Error********Error********Error******"
			            "**Error"
			         << std::endl;
			socketNumber_ = 0;
		}
		else
		{
			__COUT__ << ":):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):"
			            "):):):)"
			         << std::endl;
			__COUT__ << ":):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):"
			            "):):):)"
			         << std::endl;
			__COUT__ << "SOCKET ON PORT: " << port.str() << " ON IP: " << IPAddress_ << " INITIALIZED OK!" << std::endl;
			__COUT__ << ":):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):"
			            "):):):)"
			         << std::endl;
			__COUT__ << ":):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):"
			            "):):):)"
			         << std::endl;
			char yes = '1';
			setsockopt(socketNumber_, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));
			socketInitialized = true;
			__COUT__ << "]\tSocket Number: " << socketNumber_ << " for port: " << ntohs(socketAddress_.sin_port) << " initialized." << std::endl;
		}

		freeaddrinfo(res);  // free the linked-list
	}

	if(!socketInitialized)
	{
		__SS__ << "FATAL: Socket could not initialize socket (IP=" << IPAddress_ << ", Port=" << ntohs(socketAddress_.sin_port)
		       << "). Perhaps it is already in use?" << std::endl;
		std::cout << ss.str();
		__SS_THROW__;
	}

	__COUT__ << "Setting socket receive buffer size = " << socketReceiveBufferSize << " 0x" << std::hex << socketReceiveBufferSize << std::dec << __E__;
	if(setsockopt(socketNumber_, SOL_SOCKET, SO_RCVBUF, (char*)&socketReceiveBufferSize, sizeof(socketReceiveBufferSize)) < 0)
	{
		__COUT_ERR__ << "Failed to set socket receive size to " << socketReceiveBufferSize << ". Attempting to revert to default." << std::endl;

		socketReceiveBufferSize = defaultSocketReceiveSize_;

		__COUT__ << "Setting socket receive buffer size = " << socketReceiveBufferSize << " 0x" << std::hex << socketReceiveBufferSize << std::dec << __E__;
		if(setsockopt(socketNumber_, SOL_SOCKET, SO_RCVBUF, (char*)&socketReceiveBufferSize, sizeof(socketReceiveBufferSize)) < 0)
		{
			__SS__ << "Failed to set socket receive size to " << socketReceiveBufferSize << ". Attempting to revert to default." << std::endl;
			std::cout << ss.str();
			__SS_THROW__;
		}
	}
}

uint16_t Socket::getPort()
{
	return ntohs(socketAddress_.sin_port);

	//	//else extract from socket
	//	struct sockaddr_in sin;
	//	socklen_t len = sizeof(sin);
	//	getsockname(socketNumber_, (struct sockaddr *)&sin, &len);
	//	return ntohs(sin.sin_port);
}

//==============================================================================
const struct sockaddr_in& Socket::getSocketAddress(void) { return socketAddress_; }
