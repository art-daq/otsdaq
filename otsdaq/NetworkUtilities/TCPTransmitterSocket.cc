#include "otsdaq/NetworkUtilities/TCPTransmitterSocket.h"
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdexcept>
#include "otsdaq/NetworkUtilities/TCPPacket.h"
// #include <iostream>

using namespace ots;

//==============================================================================
TCPTransmitterSocket::TCPTransmitterSocket(int socketId) : TCPSocket(socketId) {}

//==============================================================================
TCPTransmitterSocket::~TCPTransmitterSocket(void) {}

//==============================================================================
void TCPTransmitterSocket::sendPacket(char const* buffer, std::size_t size)
{
	send(TCPPacket::encode(buffer, size));
}

//==============================================================================
void TCPTransmitterSocket::sendPacket(const std::string& buffer)
{
	send(TCPPacket::encode(buffer));
}

//==============================================================================
void TCPTransmitterSocket::send(char const* buffer,
                                std::size_t size,
                                bool        forceEmptyPacket)
{
	if(size == 0 && !forceEmptyPacket)
	{
		std::cout << __PRETTY_FUNCTION__ << "I am sorry but I won't send an empty packet!"
		          << std::endl;
		return;
	}
	std::size_t totalSent = 0;
	while(totalSent < size)
	{
		ssize_t sentBytes =
		    ::send(getSocketId(), buffer + totalSent, size - totalSent, MSG_NOSIGNAL);
		if(sentBytes > 0)
		{
			totalSent += static_cast<std::size_t>(sentBytes);
			continue;
		}

		if(sentBytes == 0)
		{
			throw std::runtime_error("Write: returned 0 bytes, connection may be closed");
		}

		switch(errno)
		{
		// case EINVAL:
		// case EBADF:
		// case ECONNRESET:
		// case ENXIO:
		case EPIPE: {
			// Fatal error. Programming bug
			throw std::runtime_error(std::string("Write: critical error: ") +
			                         strerror(errno));
		}
		// case EDQUOT:
		// case EFBIG:
		// case EIO:
		// case ENETDOWN:
		// case ENETUNREACH:
		case ENOSPC: {
			// Resource acquisition failure or device error
			throw std::runtime_error(std::string("Write: resource failure: ") +
			                         strerror(errno));
		}
		case EINTR:
			// Interrupted by signal; retry send.
			continue;
		case EAGAIN:
			// Temporary back-pressure; retry send.
			continue;
		default: {
			throw std::runtime_error(std::string("Write: returned -1: ") +
			                         strerror(errno));
		}
		}
	}
}

//==============================================================================
void TCPTransmitterSocket::send(const std::string& buffer)
{
	send(&buffer.at(0), buffer.size());
}

//==============================================================================
void TCPTransmitterSocket::send(const std::vector<char>& buffer)
{
	send(&buffer.at(0), buffer.size());
}

//==============================================================================
void TCPTransmitterSocket::send(const std::vector<uint16_t>& buffer)
{
	send((const char*)&buffer.at(0), buffer.size());
}

//==============================================================================
void TCPTransmitterSocket::setSendTimeout(unsigned int timeoutSeconds,
                                          unsigned int timeoutMicroSeconds)
{
	struct timeval tv;
	tv.tv_sec  = timeoutSeconds;
	tv.tv_usec = timeoutMicroSeconds;
	setsockopt(getSocketId(), SOL_SOCKET, SO_SNDTIMEO, (const char*)&tv, sizeof tv);
}
