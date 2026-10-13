#ifndef OTSDAQ_OTSDAQ_NETWORKUTILITIES_TCPPUBLISHSERVER_H_
#define OTSDAQ_OTSDAQ_NETWORKUTILITIES_TCPPUBLISHSERVER_H_

#include "otsdaq/NetworkUtilities/TCPServerBase.h"

namespace ots
{
class TCPPublishServer : public TCPServerBase
{
  public:
	TCPPublishServer(unsigned int serverPort, unsigned int maxNumberOfClients = -1);
	virtual ~TCPPublishServer(void);

  protected:
	void acceptConnections() override;
};
}  // namespace ots
#endif // OTSDAQ_OTSDAQ_NETWORKUTILITIES_TCPPUBLISHSERVER_H_
