#ifndef _ots_TransceiverSocket_h_
#define _ots_TransceiverSocket_h_

#include "otsdaq/NetworkUtilities/ReceiverSocket.h"
#include "otsdaq/NetworkUtilities/TransmitterSocket.h"

#include <string>

namespace ots
{
class TransceiverSocket : public TransmitterSocket, public ReceiverSocket
{
  public:
	TransceiverSocket(std::string IPAddress, unsigned int port = 0);
	virtual ~TransceiverSocket(void);

	int acknowledge(const std::string& buffer,
	                bool               verbose = false);  // responds to last receive location
  protected:
	TransceiverSocket(void);
};

}  // namespace ots

#endif
