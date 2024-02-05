#ifndef _ots_TCPClient_h_
#define _ots_TCPClient_h_

#include <string>
#include "otsdaq/NetworkUtilities/TCPClientBase.h"
#include "otsdaq/NetworkUtilities/TCPTransceiverSocket.h"

namespace ots
{
class TCPClient : public TCPTransceiverSocket, public TCPClientBase
{
  public:
	TCPClient(const std::string& serverIP, int serverPort);
	virtual ~TCPClient(void);
};
}  // namespace ots

#endif
