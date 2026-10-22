#ifndef OTSDAQ_OTSDAQ_NETWORKUTILITIES_TCPSENDCLIENT_H_
#define OTSDAQ_OTSDAQ_NETWORKUTILITIES_TCPSENDCLIENT_H_

#include <string>
#include "otsdaq/NetworkUtilities/TCPClientBase.h"
#include "otsdaq/NetworkUtilities/TCPTransmitterSocket.h"

namespace ots
{
class TCPSendClient : public TCPTransmitterSocket, public TCPClientBase
{
  public:
	// TCPSendClient();
	TCPSendClient(const std::string& serverIP, int serverPort);
	virtual ~TCPSendClient(void);
};
}  // namespace ots
#endif  // OTSDAQ_OTSDAQ_NETWORKUTILITIES_TCPSENDCLIENT_H_
