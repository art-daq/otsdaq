#ifndef OTSDAQ_OTSDAQ_NETWORKUTILITIES_TCPCLIENTBASE_H_
#define OTSDAQ_OTSDAQ_NETWORKUTILITIES_TCPCLIENTBASE_H_

#include "otsdaq/NetworkUtilities/TCPTransceiverSocket.h"

#include <string>

namespace ots
{
class TCPClientBase : public virtual TCPSocket
{
  public:
	// TCPClientBase();
	TCPClientBase(const std::string& serverIP, int serverPort);
	virtual ~TCPClientBase(void);

	bool connect(int retry = -1, unsigned int sleepMilliSeconds = 1000);
	bool disconnect(void);

  private:
	std::string fServerIP;
	int         fServerPort;
	bool        fConnected;

	void resolveServer(std::string& serverIP);
};
}  // namespace ots
#endif // OTSDAQ_OTSDAQ_NETWORKUTILITIES_TCPCLIENTBASE_H_
