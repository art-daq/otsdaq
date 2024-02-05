#ifndef _ots_ReceiverSocket_h_
#define _ots_ReceiverSocket_h_

#include "otsdaq/NetworkUtilities/Socket.h"

#include <mutex>  //for std::mutex
#include <string>
#include <vector>

namespace ots
{
class ReceiverSocket : public virtual Socket
{
	// TransceiverSocket is a "Friend" class of ReceiverSocket so has access to private
	// members.
	friend class TransceiverSocket;

  public:
	ReceiverSocket(std::string IPAddress, unsigned int port = 0);
	virtual ~ReceiverSocket(void);

	int receive(std::string& buffer, unsigned int timeoutSeconds = 1, unsigned int timeoutUSeconds = 0, bool verbose = false);
	int receive(std::vector<uint32_t>& buffer, unsigned int timeoutSeconds = 1, unsigned int timeoutUSeconds = 0, bool verbose = false);
	int receive(std::string&    buffer,
	            unsigned long&  fromIPAddress,
	            unsigned short& fromPort,
	            unsigned int    timeoutSeconds  = 1,
	            unsigned int    timeoutUSeconds = 0,
	            bool            verbose         = false);
	int receive(std::vector<uint32_t>& buffer,
	            unsigned long&         fromIPAddress,
	            unsigned short&        fromPort,
	            unsigned int           timeoutSeconds  = 1,
	            unsigned int           timeoutUSeconds = 0,
	            bool                   verbose         = false);

  protected:
	ReceiverSocket(void);

  private:
	fd_set             fileDescriptor_;
	struct timeval     timeout_;
	struct sockaddr_in fromAddress_;
	socklen_t          addressLength_;
	int                numberOfBytes_;

	unsigned long  dummyIPAddress_;
	unsigned short dummyPort_;
	unsigned int   readCounter_;

	std::mutex receiveMutex_;  // to make receiver socket thread safe
	                           //	i.e. multiple threads can share a socket and call receive()
};

}  // namespace ots

#endif
