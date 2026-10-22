#ifndef OTSDAQ_OTSDAQ_NETWORKUTILITIES_TCPSUBSCRIBECLIENT_H_
#define OTSDAQ_OTSDAQ_NETWORKUTILITIES_TCPSUBSCRIBECLIENT_H_

#include <string>
#include "otsdaq/NetworkUtilities/TCPClientBase.h"
#include "otsdaq/NetworkUtilities/TCPTransceiverSocket.h"

namespace ots
{
class TCPSubscribeClient : public TCPReceiverSocket, public TCPClientBase
{
  public:
	// TCPSubscribeClient();
	TCPSubscribeClient(const std::string& serverIP, int serverPort);
	virtual ~TCPSubscribeClient(void);
};
}  // namespace ots
#endif  // OTSDAQ_OTSDAQ_NETWORKUTILITIES_TCPSUBSCRIBECLIENT_H_
