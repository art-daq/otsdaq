#ifndef _TCPReceiverSocket_h_
#define _TCPReceiverSocket_h_

#include <chrono>
#include <string>
#include "otsdaq/NetworkUtilities/TCPPacket.h"
#include "otsdaq/NetworkUtilities/TCPSocket.h"

namespace ots
{
class TCPReceiverSocket : public virtual TCPSocket
{
  public:
	TCPReceiverSocket(int socketId = invalidSocketId);
	virtual ~TCPReceiverSocket();
	// TCPReceiverSocket(TCPReceiverSocket const&)  = delete ;
	TCPReceiverSocket(TCPReceiverSocket&& theTCPReceiverSocket) = default;

	template<class T>
	T receive(void)
	{
		T buffer;
		buffer.resize(maxSocketSize);
		int length = receive(static_cast<char*>(&buffer.at(0)), maxSocketSize);
		if(length == -1)
			length = 0;
		buffer.resize(length);
		// std::cout << __PRETTY_FUNCTION__ << "Message received-" << fBuffer << "-" <<
		// std::endl;
		return buffer;  // c++11 doesn't make a copy anymore when returned
	}
	std::string receivePacket(std::chrono::milliseconds timeout = std::chrono::milliseconds(5));
	void        setReceiveTimeout(unsigned int timeoutSeconds, unsigned int timeoutMicroSeconds);

  private:
	int                           receive(char* buffer, std::size_t bufferSize = maxSocketSize, int timeoutMicroSeconds = -1);
	static constexpr unsigned int maxSocketSize = 65536;
};
}  // namespace ots
#endif
