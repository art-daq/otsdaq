#include "otsdaq-core/NetworkUtilities/Socket.h"
#include "otsdaq-core/MessageFacility/MessageFacility.h"
#include "otsdaq-core/Macros/CoutHeaderMacros.h"

#include <iostream>
#include <cassert>
#include <sstream>

#include <unistd.h>
#include <arpa/inet.h>
//#include <sys/socket.h>
#include <netdb.h>
//#include <ifaddrs.h>
//#include <sys/ioctl.h>
//#if defined(SIOCGIFHWADDR)
//#include <net/if.h>
//#else
//#include <net/if_dl.h>
//#endif
//#include <cstdlib>
#include <cstring>
//#include <cstdio>

using namespace ots;


//========================================================================================================================
Socket::Socket(void)
{
	__SS__ << "ERROR: This method should never be called. There is something wrong in your inheritance scheme!" << std::endl;
	__COUT__ << "\n" << ss.str();
	//FIXME: this is getting called during configure?!
	//throw std::runtime_error(ss.str());
}

//========================================================================================================================
Socket::Socket(const std::string &IPAddress, unsigned int port)
: socketNumber_(-1)
, IPAddress_   (IPAddress)
, requestedPort_        (port)
//    maxSocketSize_(maxSocketSize)
{
	__COUT__ << std::endl;
	//network stuff
    socketAddress_.sin_family = AF_INET;// use IPv4 host byte order
    socketAddress_.sin_port   = htons(port);// short, network byte order

    __COUT__ << "IPAddress: " << IPAddress << " port: " << port << " htons: " <<  socketAddress_.sin_port << std::endl;
    if(inet_aton(IPAddress.c_str(), &socketAddress_.sin_addr) == 0)
    {
    	__SS__ << "FATAL: Invalid IP/Port combination. Is it already in use? " <<
    			IPAddress << "/" << port << std::endl;
        //assert(0); //RAR changed to exception on 8/17/2016
        __COUT__ << "\n" << ss.str();
        throw std::runtime_error(ss.str());
    }

    memset(&(socketAddress_.sin_zero), '\0', 8);// zero the rest of the struct
}

//========================================================================================================================
Socket::~Socket(void)
{
    __COUT__ << "CLOSING THE SOCKET #" << socketNumber_  << " IP: " << IPAddress_ << " port: " << getPort() << " htons: " << socketAddress_.sin_port << std::endl;
	if(socketNumber_ != -1)
		close(socketNumber_);
}

//========================================================================================================================
void Socket::initialize(void)
{
    __COUT__ << " htons: " <<  socketAddress_.sin_port << std::endl;
    struct addrinfo  hints;
    struct addrinfo* res;
    int status    =  0;

    // first, load up address structs with getaddrinfo():
    memset(&hints, 0, sizeof hints);
    hints.ai_family   = AF_INET;   // use IPv4 for OtsUDPHardware
    hints.ai_socktype = SOCK_DGRAM;// SOCK_DGRAM
    hints.ai_flags    = AI_PASSIVE;// fill in my IP for me

    bool socketInitialized = false;
    int fromPort = FirstSocketPort;
    int toPort   = LastSocketPort;

    if(ntohs(socketAddress_.sin_port) != 0)
        fromPort = toPort = ntohs(socketAddress_.sin_port);

    std::stringstream port;

    for(int p=fromPort; p<=toPort && !socketInitialized; p++)
    {
        port.str("");
        port << p;
        std::cout << __COUT_HDR_FL__ << __LINE__ << "]\tBinding port: " << port.str() << std::endl;
        socketAddress_.sin_port   = htons(p);// short, network byte order

        if((status = getaddrinfo(NULL, port.str().c_str(), &hints, &res)) != 0)
        {
            std::cout << __COUT_HDR_FL__ << __LINE__ << "]\tGetaddrinfo error status: " << status << std::endl;
            continue;
        }

        // make a socket:
        socketNumber_ = socket(res->ai_family, res->ai_socktype, res->ai_protocol);

        std::cout << __COUT_HDR_FL__ << __LINE__ << "]\tSocket Number: " << socketNumber_ << " for port: " << ntohs(socketAddress_.sin_port) << " initialized." << std::endl;
        // bind it to the port we passed in to getaddrinfo():
        if(bind(socketNumber_, res->ai_addr, res->ai_addrlen) == -1)
        {
            std::cout << __COUT_HDR_FL__ << "Error********Error********Error********Error********Error********Error" << std::endl;
            std::cout << __COUT_HDR_FL__ << "FAILED BIND FOR PORT: " << port.str() << " ON IP: " << IPAddress_ << std::endl;
            std::cout << __COUT_HDR_FL__ << "Error********Error********Error********Error********Error********Error" << std::endl;
            socketNumber_ = 0;
        }
        else
        {
            std::cout << __COUT_HDR_FL__ << ":):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):)" << std::endl;
            std::cout << __COUT_HDR_FL__ << ":):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):)" << std::endl;
            std::cout << __COUT_HDR_FL__ << "SOCKET ON PORT: " << port.str() << " ON IP: " << IPAddress_ << " INITIALIZED OK!" << std::endl;
            std::cout << __COUT_HDR_FL__ << ":):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):)" << std::endl;
            std::cout << __COUT_HDR_FL__ << ":):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):):)" << std::endl;
            char yes = '1';
            setsockopt(socketNumber_,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(int));
            socketInitialized = true;
            std::cout << __COUT_HDR_FL__ << __LINE__ << "]\tSocket Number: " << socketNumber_ << " for port: " << ntohs(socketAddress_.sin_port) << " initialized." << std::endl;
        }

        freeaddrinfo(res); // free the linked-list
    }

    if(!socketInitialized)
    {
    	std::stringstream ss;
		ss << __COUT_HDR_FL__ << "FATAL: Socket could not initialize socket. Perhaps it is already in use?" << std::endl;
		std::cout << ss.str();
		throw std::runtime_error(ss.str());
    }
}

uint16_t Socket::getPort()
{
	struct sockaddr_in sin;
	socklen_t len = sizeof(sin);
	getsockname(socketNumber_, (struct sockaddr *)&sin, &len);
	return ntohs(sin.sin_port);
}

//========================================================================================================================
const struct sockaddr_in& Socket::getSocketAddress(void)
{
	return socketAddress_;
}

