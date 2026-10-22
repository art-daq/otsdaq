#ifndef _TCPListenServer_h_
#define _TCPListenServer_h_

#include "otsdaq/NetworkUtilities/TCPReceiverSocket.h"
#include "otsdaq/NetworkUtilities/TCPServerBase.h"

#include "TRACE/trace.h"

#include <chrono>

namespace ots
{
/// TCPListenServer
///	Accepts any number of clients and lets one reader thread pull data from whichever
///	client has something to say. Reads never block on an idle client: the reader
///	polls all clients, picks a ready one round-robin, and reads from it. A client
///	whose peer closed the connection is dropped from the list on the spot.
class TCPListenServer : public TCPServerBase
{
  public:
	TCPListenServer(unsigned int serverPort, unsigned int maxNumberOfClients = -1);
	virtual ~TCPListenServer(void);

	/// Raw read from a ready client. Returns an empty T if no client had data within
	/// the poll timeout. Throws if the chosen client's connection closed (after
	/// removing it).
	template<class T>
	T receive();
	/// Length-prefixed packet from a ready client. Same timeout/throw semantics.
	std::string receivePacket();

  protected:
	void acceptConnections() override;

	/// Wait up to timeout for any client to become readable and return it, rotating
	/// through clients so a chatty one cannot starve the others. nullptr on timeout
	/// or when no clients are connected (the timeout is still honoured, so callers
	/// do not spin).
	TCPReceiverSocket* waitForReadableClient(std::chrono::milliseconds timeout);

	static constexpr std::chrono::milliseconds kPollTimeout{5};

	int lastReceived = -1;
};

template<class T>
inline T TCPListenServer::receive()
{
	TCPReceiverSocket* client = waitForReadableClient(kPollTimeout);
	if(client == nullptr)
		return T();
	const int socketId = client->getSocketId();
	try
	{
		return client->receive<T>();
	}
	catch(...)
	{
		removeClient(socketId);
		throw;
	}
}
}  // namespace ots
#endif
