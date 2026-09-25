#ifndef _ots_TCPServerBase_h_
#define _ots_TCPServerBase_h_

#include <future>
#include <map>
#include <mutex>
#include <string>
#include <vector>
// #include <thread>
#include <atomic>
#include "otsdaq/NetworkUtilities/TCPSocket.h"

namespace ots
{
class TCPServerBase : public virtual TCPSocket
{
  public:
	TCPServerBase(
	    unsigned int serverPort,
	    unsigned int maxNumberOfClients = 0);  ///< Means as many unsigned allows
	virtual ~TCPServerBase(void);

	void startAccept(void);
	void shutdownAccept(void);
	void broadcastPacket(const char* message, std::size_t length);
	void broadcastPacket(const std::string& message);
	void broadcast(const char* message, std::size_t length);
	void broadcast(const std::string& message);
	void broadcast(const std::vector<char>& message);
	void broadcast(const std::vector<uint16_t>& message);

  protected:
	virtual void acceptConnections() = 0;

	void closeClientSocket(int socket);
	template<class T>
	T* acceptClient(bool blocking = true)
	{
		int socketId = accept(blocking);
		std::lock_guard<std::mutex> lock(fClientsMutex);
		fConnectedClients.emplace(socketId, new T(socketId));
		return dynamic_cast<T*>(fConnectedClients[socketId]);
	}

	/// Drop a client whose peer has gone away (no shutdown handshake, just close).
	void removeClient(int socketId);
	/// Snapshot of connected client socket ids, safe to call from any thread.
	std::vector<int> getClientSocketIds(void) const;

	void pingActiveClients(void);

	// std::promise<bool>        fAcceptPromise;
	// fClientsMutex guards fConnectedClients against the accept thread inserting while
	// a reader thread iterates. Only acceptClient, removeClient, getClientSocketIds and
	// TCPListenServer's receive path take it; the older broadcast/ping paths do not.
	mutable std::mutex               fClientsMutex;
	std::map<int, TCPSocket*>        fConnectedClients;
	std::map<int, std::future<void>> fConnectedClientsFuture;
	const int                        E_SHUTDOWN = 0;
	bool                             getAccept() { return fAccept.load(); }

  private:
	void closeClientSockets(
	    void);  ///< This one will also wait until the socket thread is done!
	int accept(bool blocking = true);

	const int        fMaxConnectionBacklog = 5;
	unsigned int     fMaxNumberOfClients;
	unsigned int     fServerPort;
	std::atomic_bool fAccept;
	//	std::thread       fAcceptThread;
	std::future<void> fAcceptFuture;
};
}  // namespace ots

#endif
