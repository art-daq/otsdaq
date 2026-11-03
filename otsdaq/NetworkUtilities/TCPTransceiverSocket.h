#ifndef OTSDAQ_OTSDAQ_NETWORKUTILITIES_TCPTRANSCEIVERSOCKET_H_
#define OTSDAQ_OTSDAQ_NETWORKUTILITIES_TCPTRANSCEIVERSOCKET_H_

#include "otsdaq/NetworkUtilities/TCPReceiverSocket.h"
#include "otsdaq/NetworkUtilities/TCPTransmitterSocket.h"

#include <string>

namespace ots
{
/// A class that can read/write to a socket
class TCPTransceiverSocket : public TCPReceiverSocket, public TCPTransmitterSocket
{
  public:
	TCPTransceiverSocket(int socketId = invalidSocketId);
	virtual ~TCPTransceiverSocket(void);
	TCPTransceiverSocket(TCPTransceiverSocket const&)                    = delete;
	TCPTransceiverSocket(TCPTransceiverSocket&& theTCPTransceiverSocket) = default;

	std::string sendAndReceivePacket(const std::string& sendBuffer);
	std::string sendAndReceive(const std::string& sendBuffer);
};
}  // namespace ots
#endif  // OTSDAQ_OTSDAQ_NETWORKUTILITIES_TCPTRANSCEIVERSOCKET_H_
