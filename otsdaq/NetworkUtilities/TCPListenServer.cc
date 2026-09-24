#include "otsdaq/NetworkUtilities/TCPListenServer.h"
#include "otsdaq/NetworkUtilities/TCPReceiverSocket.h"

#include <poll.h>
#include <iostream>
#include <thread>
#include <utility>
#include <vector>

using namespace ots;

constexpr std::chrono::milliseconds TCPListenServer::kPollTimeout;

//==============================================================================
TCPListenServer::TCPListenServer(unsigned int serverPort, unsigned int maxNumberOfClients)
    : TCPServerBase(serverPort, maxNumberOfClients)
{
}

//==============================================================================
TCPListenServer::~TCPListenServer(void)
{
	//	std::cout << __PRETTY_FUNCTION__ << "Done" << std::endl;
}

//==============================================================================
TCPReceiverSocket* TCPListenServer::waitForReadableClient(
    std::chrono::milliseconds timeout)
{
	// Snapshot under the lock; the accept thread may insert while we poll.
	std::vector<std::pair<int, TCPSocket*>> clients;
	{
		std::lock_guard<std::mutex> lock(fClientsMutex);
		clients.assign(fConnectedClients.begin(), fConnectedClients.end());
	}

	if(clients.empty())
	{
		std::this_thread::sleep_for(timeout);
		return nullptr;
	}

	std::vector<struct pollfd> fds(clients.size());
	for(size_t i = 0; i < clients.size(); ++i)
	{
		fds[i].fd      = clients[i].first;
		fds[i].events  = POLLIN;
		fds[i].revents = 0;
	}

	int ready = ::poll(fds.data(), fds.size(), static_cast<int>(timeout.count()));
	if(ready <= 0)
		return nullptr;  // timeout, or EINTR: caller simply tries again

	// Round-robin among the ready clients, starting after the one served last.
	size_t start = 0;
	for(size_t i = 0; i < clients.size(); ++i)
		if(clients[i].first == lastReceived)
		{
			start = i + 1;
			break;
		}
	for(size_t k = 0; k < clients.size(); ++k)
	{
		size_t i = (start + k) % clients.size();
		// POLLHUP/POLLERR/POLLNVAL are "ready" too: the read will return 0 or fail,
		// the caller throws, and the client is removed.
		if(fds[i].revents & (POLLIN | POLLHUP | POLLERR | POLLNVAL))
		{
			lastReceived = clients[i].first;
			TLOG(25, "TCPListenServer")
			    << "Reading from socket " << lastReceived << ", there are "
			    << clients.size() << " clients connected.";
			return dynamic_cast<TCPReceiverSocket*>(clients[i].second);
		}
	}
	return nullptr;
}

//==============================================================================
std::string TCPListenServer::receivePacket()
{
	TCPReceiverSocket* client = waitForReadableClient(kPollTimeout);
	if(client == nullptr)
		return "";
	const int socketId = client->getSocketId();
	try
	{
		// poll() said readable, so the length header is there or the peer hung up;
		// once the header is in, the body follows from the same sender.
		return client->receivePacket(std::chrono::milliseconds(100));
	}
	catch(...)
	{
		removeClient(socketId);
		throw;
	}
}

//==============================================================================
void TCPListenServer::acceptConnections()
{
	while(getAccept())
	{
		try
		{
			// __attribute__((unused)) TCPTransmitterSocket* clientSocket = acceptClient<TCPTransmitterSocket>();
			acceptClient<TCPReceiverSocket>();
		}
		catch(int e)
		{
			if(e == E_SHUTDOWN)
				break;
		}
	}
	// fAcceptPromise.set_value(true);
}
