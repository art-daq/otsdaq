#include "otsdaq/NetworkUtilities/TransceiverSocket.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <iostream>

using namespace ots;

//==============================================================================
TransceiverSocket::TransceiverSocket(void) { __COUT__ << "TransceiverSocket constructor " << __E__; }

//==============================================================================
TransceiverSocket::TransceiverSocket(std::string IPAddress, unsigned int port) : Socket(IPAddress, port)
{
	__COUT__ << "TransceiverSocket constructor " << IPAddress << ":" << port << __E__;
}

//==============================================================================
TransceiverSocket::~TransceiverSocket(void) {}

//==============================================================================
// returns 0 on success
int TransceiverSocket::acknowledge(const std::string& buffer, bool verbose)
{
	// lockout other senders for the remainder of the scope
	std::lock_guard<std::mutex> lock(sendMutex_);

	if(verbose)
		__COUT__ << "Acknowledging on Socket Descriptor #: " << socketNumber_ << " from-port: " << ntohs(socketAddress_.sin_port)
		         << " to-port: " << ntohs(ReceiverSocket::fromAddress_.sin_port) << std::endl;

	constexpr size_t MAX_SEND_SIZE = 65500;
	if(buffer.size() > MAX_SEND_SIZE)
	{
		__COUT__ << "Too large! Error writing buffer from port " << ntohs(TransmitterSocket::socketAddress_.sin_port) << ": " << std::endl;
		return -1;
	}

	size_t offset     = 0;
	size_t thisSize   = buffer.size();
	int    sendToSize = sendto(socketNumber_, buffer.c_str() + offset, thisSize, 0, (struct sockaddr*)&(ReceiverSocket::fromAddress_), sizeof(sockaddr_in));

	if(sendToSize <= 0)
	{
		__COUT__ << "Error writing buffer from port " << ntohs(TransmitterSocket::socketAddress_.sin_port) << ": " << strerror(errno) << std::endl;
		return -1;
	}

	return 0;
}
