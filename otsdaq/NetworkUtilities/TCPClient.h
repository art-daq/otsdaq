#ifndef OTSDAQ_OTSDAQ_NETWORKUTILITIES_TCPCLIENT_H_
#define OTSDAQ_OTSDAQ_NETWORKUTILITIES_TCPCLIENT_H_

#include "otsdaq/NetworkUtilities/TCPClientBase.h"
#include "otsdaq/NetworkUtilities/TCPTransceiverSocket.h"

#include <string>

namespace ots
{
class TCPClient : public TCPTransceiverSocket, public TCPClientBase
{
  public:
	TCPClient(const std::string& serverIP, int serverPort);
	virtual ~TCPClient(void);
};
}  // namespace ots

#endif // OTSDAQ_OTSDAQ_NETWORKUTILITIES_TCPCLIENT_H_
