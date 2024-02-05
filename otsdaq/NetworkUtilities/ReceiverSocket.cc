#include "otsdaq/NetworkUtilities/ReceiverSocket.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"
#include "otsdaq/NetworkUtilities/NetworkConverters.h"

#include <iomanip> /* for setfill */
#include <iostream>
#include <sstream>

#include <arpa/inet.h>
#include <sys/time.h>

using namespace ots;

//==============================================================================
ReceiverSocket::ReceiverSocket(std::string IPAddress, unsigned int port)
    : Socket(IPAddress, port), addressLength_(sizeof(fromAddress_)), numberOfBytes_(0), readCounter_(0)
{
	__COUT__ << "ReceiverSocket constructor " << IPAddress << ":" << port << __E__;
}

//==============================================================================
// protected constructor
ReceiverSocket::ReceiverSocket(void) : addressLength_(sizeof(fromAddress_)), numberOfBytes_(0), readCounter_(0)
{
	__COUT__ << "ReceiverSocket constructor" << __E__;
}

//==============================================================================
ReceiverSocket::~ReceiverSocket(void) {}

//==============================================================================
int ReceiverSocket::receive(std::string& buffer, unsigned int timeoutSeconds, unsigned int timeoutUSeconds, bool verbose)
{
	return receive(buffer, dummyIPAddress_, dummyPort_, timeoutSeconds, timeoutUSeconds, verbose);
}

//==============================================================================
// receive ~~
//	returns 0 on success, -1 on failure
//	NOTE: must call Socket::initialize before receiving!
int ReceiverSocket::receive(
    std::string& buffer, unsigned long& fromIPAddress, unsigned short& fromPort, unsigned int timeoutSeconds, unsigned int timeoutUSeconds, bool verbose)
{
	// lockout other receivers for the remainder of the scope
	std::lock_guard<std::mutex> lock(receiveMutex_);

	// set timeout period for select()
	timeout_.tv_sec  = timeoutSeconds;
	timeout_.tv_usec = timeoutUSeconds;

	FD_ZERO(&fileDescriptor_);
	FD_SET(socketNumber_, &fileDescriptor_);
	select(socketNumber_ + 1, &fileDescriptor_, 0, 0, &timeout_);

	if(FD_ISSET(socketNumber_, &fileDescriptor_))
	{
		buffer.resize(maxSocketSize_);  // NOTE: this is inexpensive according to
		                                // Lorenzo/documentation in C++11 (only increases
		                                // size once and doesn't decrease size)
		if((numberOfBytes_ = recvfrom(socketNumber_, &buffer[0], maxSocketSize_, 0, (struct sockaddr*)&fromAddress_, &addressLength_)) == -1)
		{
			__COUT__ << "At socket with IPAddress: " << getIPAddress() << " port: " << getPort() << std::endl;
			__SS__ << "Error reading buffer from\tIP:\t";
			std::string fromIP = inet_ntoa(fromAddress_.sin_addr);
			fromIPAddress      = fromAddress_.sin_addr.s_addr;
			fromPort           = fromAddress_.sin_port;
			for(int i = 0; i < 4; i++)
			{
				ss << ((fromIPAddress << (i * 8)) & 0xff);
				if(i < 3)
					ss << ".";
			}
			ss << "\tPort\t" << ntohs(fromPort) << " IP " << fromIP << std::endl;
			__COUT__ << "\n" << ss.str();
			return -1;
		}
		// char address[INET_ADDRSTRLEN];
		// inet_ntop(AF_INET, &(fromAddress.sin_addr), address, INET_ADDRSTRLEN);
		fromIPAddress = fromAddress_.sin_addr.s_addr;
		fromPort      = fromAddress_.sin_port;

		//__COUT__ << __PRETTY_FUNCTION__ << "IP: " << std::hex << fromIPAddress <<
		// std::dec << " port: " << fromPort << std::endl;
		//__COUT__ << "Socket Number: " << socketNumber_ << " number of bytes: " <<
		// nOfBytes << std::endl;  gettimeofday(&tvend,NULL);
		//__COUT__ << "started at" << tvbegin.tv_sec << ":" <<tvbegin.tv_usec <<
		// std::endl;
		//__COUT__ << "ended at" << tvend.tv_sec << ":" <<tvend.tv_usec << std::endl;

		// NOTE: this is inexpensive according to Lorenzo/documentation in C++11 (only
		// increases size once and doesn't decrease size)
		buffer.resize(numberOfBytes_);
		readCounter_ = 0;

		if(verbose)  // debug
		{
			std::string fromIP = inet_ntoa(fromAddress_.sin_addr);

			__COUT__ << "Receiving "
			         << " at: " << getIPAddress() << ":" << getPort() << " from: " << fromIP;
			std::cout << ":" << ntohs(fromPort) << " size: " << buffer.size() << std::endl;

			//			std::stringstream ss;
			//			ss << "\tRx";
			//			uint32_t begin = 0;
			//			for(uint32_t i=begin; i<buffer.size(); i++)
			//			{
			//				if(i==begin+2) ss << ":::";
			//				else if(i==begin+10) ss << ":::";
			//				ss << std::setfill('0') << std::setw(2) << std::hex <<
			//(((int16_t)  buffer[i]) &0xFF) << "-" << std::dec;
			//			}
			//			ss << std::endl;
			//			std::cout << ss.str();
		}
	}
	else
	{
		++readCounter_;

		if(verbose)
			__COUT__ << "No new messages for " << timeoutSeconds + timeoutUSeconds / 1000. << "s (Total "
			         << readCounter_ * (timeoutSeconds + timeoutUSeconds / 1000.) << "s). Read request timed out receiving on "
			         << " " << getIPAddress() << ":" << getPort() << std::endl;
		return -1;
	}

	return 0;
}

//==============================================================================
int ReceiverSocket::receive(std::vector<uint32_t>& buffer, unsigned int timeoutSeconds, unsigned int timeoutUSeconds, bool verbose)
{
	return receive(buffer, dummyIPAddress_, dummyPort_, timeoutSeconds, timeoutUSeconds, verbose);
}

//==============================================================================
// receive ~~
//	returns 0 on success, -1 on failure
//	NOTE: must call Socket::initialize before receiving!
int ReceiverSocket::receive(std::vector<uint32_t>& buffer,
                            unsigned long&         fromIPAddress,
                            unsigned short&        fromPort,
                            unsigned int           timeoutSeconds,
                            unsigned int           timeoutUSeconds,
                            bool                   verbose)
{
	// lockout other receivers for the remainder of the scope
	std::lock_guard<std::mutex> lock(receiveMutex_);

	// set timeout period for select()
	timeout_.tv_sec  = timeoutSeconds;
	timeout_.tv_usec = timeoutUSeconds;

	FD_ZERO(&fileDescriptor_);
	FD_SET(socketNumber_, &fileDescriptor_);
	select(socketNumber_ + 1, &fileDescriptor_, 0, 0, &timeout_);
	__COUT__ << "Is this a successful reeeaaad???" << std::endl;

	if(FD_ISSET(socketNumber_, &fileDescriptor_))
	{
		buffer.resize(maxSocketSize_ / sizeof(uint32_t));  // NOTE: this is inexpensive
		                                                   // according to
		                                                   // Lorezno/documentation in
		                                                   // C++11 (only increases size
		                                                   // once and doesn't decrease
		                                                   // size)
		if((numberOfBytes_ = recvfrom(socketNumber_, &buffer[0], maxSocketSize_, 0, (struct sockaddr*)&fromAddress_, &addressLength_)) == -1)
		{
			__COUT__ << "At socket with IPAddress: " << getIPAddress() << " port: " << getPort() << std::endl;
			__SS__ << "Error reading buffer from\tIP:\t";
			for(int i = 0; i < 4; i++)
			{
				ss << ((fromIPAddress << (i * 8)) & 0xff);
				if(i < 3)
					ss << ".";
			}
			ss << "\tPort\t" << fromPort << std::endl;
			__COUT__ << "\n" << ss.str();
			return -1;
		}
		// char address[INET_ADDRSTRLEN];
		// inet_ntop(AF_INET, &(fromAddress.sin_addr), address, INET_ADDRSTRLEN);
		fromIPAddress = fromAddress_.sin_addr.s_addr;
		fromPort      = fromAddress_.sin_port;

		//__COUT__ << __PRETTY_FUNCTION__ << "IP: " << std::hex << fromIPAddress <<
		// std::dec << " port: " << fromPort << std::endl;
		//__COUT__ << "Socket Number: " << socketNumber_ << " number of bytes: " <<
		// nOfBytes << std::endl;  gettimeofday(&tvend,NULL);
		//__COUT__ << "started at" << tvbegin.tv_sec << ":" <<tvbegin.tv_usec <<
		// std::endl;
		//__COUT__ << "ended at" << tvend.tv_sec << ":" <<tvend.tv_usec << std::endl;

		// NOTE: this is inexpensive according to Lorenzo/documentation in C++11 (only
		// increases size once and doesn't decrease size)
		buffer.resize(numberOfBytes_ / sizeof(uint32_t));
		readCounter_ = 0;
	}
	else
	{
		++readCounter_;
		struct sockaddr_in sin;
		socklen_t          len = sizeof(sin);
		getsockname(socketNumber_, (struct sockaddr*)&sin, &len);

		if(verbose)
			__COUT__ << __COUT_HDR_FL__ << "No new messages for " << timeoutSeconds + timeoutUSeconds / 1000. << "s (Total "
			         << readCounter_ * (timeoutSeconds + timeoutUSeconds / 1000.) << "s). Read request timed out for port: " << ntohs(sin.sin_port)
			         << std::endl;
		return -1;
	}
	__COUT__ << "This a successful reeeaaad" << std::endl;
	return 0;
}
