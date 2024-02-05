#include "otsdaq/NetworkUtilities/TransmitterSocket.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <iomanip> /* for setfill */
#include <iostream>
#include <sstream>

using namespace ots;

//==============================================================================
TransmitterSocket::TransmitterSocket(void) { __COUT__ << "TransmitterSocket constructor " << __E__; }

//==============================================================================
TransmitterSocket::TransmitterSocket(const std::string& IPAddress, unsigned int port) : Socket(IPAddress, port)
{
	__COUT__ << "TransmitterSocket constructor " << IPAddress << ":" << port << __E__;
}

//==============================================================================
TransmitterSocket::~TransmitterSocket(void) {}

//==============================================================================
int TransmitterSocket::send(Socket& toSocket, const std::string& buffer, bool verbose)
{
	// lockout other senders for the remainder of the scope
	std::lock_guard<std::mutex> lock(sendMutex_);

	//	__COUT__ << "Socket Descriptor #: " << socketNumber_ <<
	//			" from-port: " << ntohs(socketAddress_.sin_port) <<
	//			" to-port: " << ntohs(toSocket.getSocketAddress().sin_port) << std::endl;

	constexpr size_t MAX_SEND_SIZE = 65500;
	size_t           offset        = 0;
	int              sts           = 1;
	bool             delay         = false;
	while(offset < buffer.size() && sts > 0)
	{
		auto thisSize = buffer.size() - offset > MAX_SEND_SIZE ? MAX_SEND_SIZE : buffer.size() - offset;

		if(verbose)  // debug
		{
			__COUT__ << "Sending "
			         << " from: " << getIPAddress() << ":" << ntohs(socketAddress_.sin_port) << " to: " << toSocket.getIPAddress() << ":"
			         << ntohs(toSocket.getSocketAddress().sin_port) << " size: " << thisSize << " remaining = " << (buffer.size() - offset - thisSize)
			         << std::endl;
			//			std::stringstream ss;
			//			ss << "\t";
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

		if(delay)
			usleep(10000);
		else
			delay = true;
		sts = sendto(socketNumber_, buffer.c_str() + offset, thisSize, 0, (struct sockaddr*)&(toSocket.getSocketAddress()), sizeof(sockaddr_in));
		offset += sts;
	}

	if(sts <= 0)
	{
		__COUT__ << "Error writing buffer for port " << ntohs(socketAddress_.sin_port) << ": " << strerror(errno) << std::endl;
		return -1;
	}
	return 0;
}

//==============================================================================
int TransmitterSocket::send(Socket& toSocket, const std::vector<uint16_t>& buffer, bool /*verbose*/)
{
	// lockout other senders for the remainder of the scope
	std::lock_guard<std::mutex> lock(sendMutex_);

	//	__COUT__ << "Socket Descriptor #: " << socketNumber_ <<
	//			" from-port: " << ntohs(socketAddress_.sin_port) <<
	//			" to-port: " << ntohs(toSocket.getSocketAddress().sin_port) << std::endl;

	constexpr size_t MAX_SEND_SIZE = 1500;
	size_t           offset        = 0;
	int              sts           = 1;

	while(offset < buffer.size() && sts > 0)
	{
		auto thisSize = 2 * (buffer.size() - offset) > MAX_SEND_SIZE ? MAX_SEND_SIZE : 2 * (buffer.size() - offset);
		sts           = sendto(socketNumber_, &buffer[0] + offset, thisSize, 0, (struct sockaddr*)&(toSocket.getSocketAddress()), sizeof(sockaddr_in));
		offset += sts / 2;
	}

	if(sts <= 0)
	{
		__COUT__ << "Error writing buffer for port " << ntohs(socketAddress_.sin_port) << ": " << strerror(errno) << std::endl;
		return -1;
	}
	return 0;
}

//==============================================================================
int TransmitterSocket::send(Socket& toSocket, const std::vector<uint32_t>& buffer, bool /*verbose*/)
{
	// lockout other senders for the remainder of the scope
	std::lock_guard<std::mutex> lock(sendMutex_);

	//	__COUT__ << "Socket Descriptor #: " << socketNumber_ <<
	//			" from-port: " << ntohs(socketAddress_.sin_port) <<
	//			" to-port: " << ntohs(toSocket.getSocketAddress().sin_port) << std::endl;

	if(sendto(socketNumber_, &buffer[0], buffer.size() * sizeof(uint32_t), 0, (struct sockaddr*)&(toSocket.getSocketAddress()), sizeof(sockaddr_in)) <
	   (int)(buffer.size() * sizeof(uint32_t)))
	{
		__COUT__ << "Error writing buffer for port " << ntohs(socketAddress_.sin_port) << std::endl;
		return -1;
	}
	return 0;
}
