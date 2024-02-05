#ifndef _ots_Socket_h_
#define _ots_Socket_h_

#include <netinet/in.h>
#include <sys/types.h>
#include <string>

namespace ots
{
class Socket
{
  public:
	Socket(const std::string& IPAddress, unsigned int port = 0);
	virtual ~Socket(void);

	virtual void              initialize(unsigned int socketReceiveBufferSize = defaultSocketReceiveSize_);
	const struct sockaddr_in& getSocketAddress(void);
	const std::string&        getIPAddress() { return IPAddress_; }
	uint16_t                  getPort();

  protected:
	enum
	{
		maxSocketSize_            = 65536,
		defaultSocketReceiveSize_ = 0x10000
	};

	Socket(void);
	struct sockaddr_in socketAddress_;
	int                socketNumber_;
	// unsigned int maxSocketSize_;

  protected:
	enum
	{
		FirstSocketPort = 10000,
		LastSocketPort  = 15000
	};
	std::string  IPAddress_;
	unsigned int requestedPort_;
};

}  // namespace ots

#endif
