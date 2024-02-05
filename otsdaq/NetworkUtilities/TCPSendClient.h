#ifndef _TCPSendClient_h_
#define _TCPSendClient_h_

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
#endif
