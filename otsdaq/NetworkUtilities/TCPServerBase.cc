// #ifndef BEAGLEBONE
// #include "otsdaq_cmsburninbox/BeagleBone/BeagleBoneUtils/TCPServerBase.h"
// #else
#include "otsdaq/NetworkUtilities/TCPServerBase.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/NetworkUtilities/TCPTransmitterSocket.h"

// #endif

#include <arpa/inet.h>
#include <errno.h>   // errno
#include <string.h>  // errno
#include <sys/socket.h>
#include <iostream>
#include <thread>

// #include <sys/socket.h>
// #include <netinet/in.h>
// #include <netdb.h>

using namespace ots;

//==============================================================================
TCPServerBase::TCPServerBase(unsigned int serverPort, unsigned int maxNumberOfClients)
    : fMaxNumberOfClients(maxNumberOfClients), fServerPort(serverPort), fAccept(true)
{
	// 0 or -1 means no restrictions on the number of clients
	if(fMaxNumberOfClients == 0)
		fMaxNumberOfClients = (unsigned)-1;
	// CANNOT GO IN THE CONSTRUCTOR OR IT MIGHT START BEFORE THE CHILD CLASS CONSTRUCTOR IS FULLY CONSTRUCTED
	// THIS MIGHT RESULT IN THE CALL OF THE VIRTUAL TCPServerBase::acceptConnections
	// startAccept();
}

//==============================================================================
TCPServerBase::~TCPServerBase(void)
{
	__COUT__ << "Shutting down accept for socket: " << getSocketId() << std::endl;
	shutdownAccept();
	while(fAcceptFuture.valid() && fAcceptFuture.wait_for(std::chrono::milliseconds(
	                                   100)) != std::future_status::ready)
	{
		__COUT__ << "Server accept still running" << std::endl;
		shutdownAccept();
	}
	//__COUT__ << "Closing connected client sockets for socket: " << getSocketId() << std::endl;
	closeClientSockets();
	//__COUT__ << "Closed all sockets connected to server: " << getSocketId() << std::endl;
}

//==============================================================================
void TCPServerBase::startAccept(void)
{
	struct sockaddr_in checkAddr;
	socklen_t          addrLen = sizeof(checkAddr);
	if(::getsockname(getSocketId(), (struct sockaddr*)&checkAddr, &addrLen) == 0 &&
	   checkAddr.sin_port != 0)
	{
		__COUT__ << "Socket " << getSocketId() << " is already bound to port "
		         << ntohs(checkAddr.sin_port) << std::endl;
		return;
	}

	//	__COUT__ << "Begin startAccept" << std::endl;
	int opt = 1;  // SO_REUSEADDR - man socket(7)
	if(::setsockopt(getSocketId(), SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int)) == -1)
	{
		close();
		__SS__ << "Setsockopt: " << strerror(errno) << __E__;
		__SS_THROW__;
	}

	struct sockaddr_in serverAddr;
	bzero((char*)&serverAddr, sizeof(serverAddr));
	serverAddr.sin_family      = AF_INET;
	serverAddr.sin_port        = htons(fServerPort);
	serverAddr.sin_addr.s_addr = INADDR_ANY;

	if(::bind(getSocketId(), (struct sockaddr*)&serverAddr, sizeof(serverAddr)) != 0)
	{
		close();
		__SS__ << "Bind: " << strerror(errno) << " (port " << fServerPort << ")" << __E__;
		__SS_THROW__;
	}
	// freeaddrinfo(serverAddr); // all done with this structure

	if(::listen(getSocketId(), fMaxConnectionBacklog) != 0)
	{
		close();
		__SS__ << "Listen: " << strerror(errno) << __E__;
		__SS_THROW__;
	}

	fAccept = true;
	fAcceptFuture =
	    std::async(std::launch::async, &TCPServerBase::acceptConnections, this);
	//	__COUT__ << "Done startAccept" << std::endl;
}

/// An accepts waits for a connection and returns the opened socket number
//==============================================================================
int TCPServerBase::accept(bool blocking)
{
	__COUT__ << "Now server accept connections on socket: " << getSocketId() << std::endl;
	if(getSocketId() == invalidSocketId)
	{
		__SS__ << "Accept called on a bad socket object (this object was moved)" << __E__;
		__SS_THROW__;
	}

	struct sockaddr_storage clientAddress;  // connector's address information
	socklen_t               clientAddressSize = sizeof(clientAddress);
	int                     clientSocket      = invalidSocketId;
	if(blocking)
	{
		//__COUT__ << "Number of connected clients: " << fConnectedClients.size() << std::endl;
		// clientSocket = ::accept4(getSocketId(),(struct sockaddr *)&clientAddress,  &clientAddressSize, 0);
		// unsigned counter = 0;
		__COUT__ << "Client list on input:\n";
		for(auto it = fConnectedClients.begin(); it != fConnectedClients.end(); it++)
		{
			__COUT__ << " --> Client: " << it->first << " : " << it->second << std::endl;
		}
		while(true)
		{
			clientSocket = ::accept(
			    getSocketId(), (struct sockaddr*)&clientAddress, &clientAddressSize);
			__COUT__ << ": clientSocket returned = " << clientSocket << std::endl;

			//FIXME: Commenting out this line to avoid seg-fault in the case there are two clients connecting from the same process...
			// pingActiveClients();  // This message is to check if there are clients that disconnected and, if so, they are removed from the client list
			if(fAccept && fMaxNumberOfClients > 0 &&
			   fConnectedClients.size() >= fMaxNumberOfClients)
			{
				send(clientSocket, "Too many clients connected!", 27, 0);
				::shutdown(clientSocket, SHUT_WR);
				continue;
			}
			break;
		}
		__COUT__ << "fAccept? " << fAccept << std::endl;
		if(!fAccept)
		{
			throw E_SHUTDOWN;
		}
		else if(clientSocket == invalidSocketId)
		{
			__COUT__ << "New socket invalid?: " << clientSocket << " errno: " << errno
			         << std::endl;
			__SS__ << "Accept: " << strerror(errno) << __E__;
			__SS_THROW__;
		}

		__COUT__ << "Server just accepted a connection on socket: " << getSocketId()
		         << " Client socket: " << clientSocket << std::endl;
		return clientSocket;
	}
	else
	{
		constexpr int  sleepMSeconds   = 5;
		constexpr int  timeoutSeconds  = 0;
		constexpr int  timeoutUSeconds = 1000;
		struct timeval timeout;
		timeout.tv_sec  = timeoutSeconds;
		timeout.tv_usec = timeoutUSeconds;

		fd_set fdSet;

		while(fAccept)
		{
			FD_ZERO(&fdSet);
			FD_SET(getSocketId(), &fdSet);
			select(getSocketId() + 1, &fdSet, 0, 0, &timeout);

			if(FD_ISSET(getSocketId(), &fdSet))
			{
				struct sockaddr_in clientAddress;
				socklen_t          socketSize = sizeof(clientAddress);
				// int newSocketFD = ::accept4(fdServerSocket_,(struct sockaddr*)&clientAddress,&socketSize, (pushOnly_ ? SOCK_NONBLOCK : 0));
				clientSocket = ::accept(
				    getSocketId(),
				    (struct sockaddr*)&clientAddress,
				    &socketSize);  // Blocking since select goes in timeout if there is nothing
				if(clientSocket == invalidSocketId)
				{
					__COUT__ << "New socket invalid?: " << clientSocket
					         << " errno: " << errno << std::endl;
					__SS__ << "Accept: " << strerror(errno) << __E__;
					__SS_THROW__;
				}
				return clientSocket;
			}
			std::this_thread::sleep_for(std::chrono::milliseconds(sleepMSeconds));
		}
		throw E_SHUTDOWN;
	}
}

//==============================================================================
/// This method is called in the distructor so I need to wait for the threads to be done!
void TCPServerBase::closeClientSockets(void)
{
	for(auto& socket : fConnectedClients)
	{
		try
		{
			socket.second->sendClose();
		}
		catch(const std::exception& e)
		{
			// I can get here with the TCPPubishServer because it doesn't keep track of the clients that might have already disconnected
			// Just do nothing!
			__COUT__ << e.what() << '\n';
		}

		auto clientThread = fConnectedClientsFuture.find(socket.first);
		if(clientThread != fConnectedClientsFuture.end())
			clientThread->second.wait();  // Waiting for client thread
		delete socket.second;
	}
	fConnectedClients.clear();
	fConnectedClientsFuture.clear();
}

//==============================================================================
void TCPServerBase::closeClientSocket(int socket)
{
	// This method is called inside the thread itself so it cannot call the removeClientSocketFuture!!!
	auto it = fConnectedClients.find(socket);
	if(it != fConnectedClients.end())
	{
		if(it->second->getSocketId() == socket)
		{
			try
			{
				if(it->second != nullptr)
					it->second->sendClose();
			}
			catch(const std::exception& e)
			{
				// I can get here with the TCPPubishServer because it doesn't keep track of the clients that might have already disconnected
				// Just do nothing!
				__COUT__ << e.what() << '\n';
			}
			delete it->second;
			fConnectedClients.erase(it);
		}
		else
		{
			__SS__
			    << "SocketId in fConnectedClients != socketId in TCPSocket! Impossible!!!"
			    << __E__;
			__SS_THROW__;
		}
	}
}

//==============================================================================
void TCPServerBase::broadcastPacket(const char* message, std::size_t length)
{
	broadcastPacket(std::string(message, length));
}

//==============================================================================
void TCPServerBase::broadcastPacket(const std::string& message)
{
	for(auto it = fConnectedClients.begin(); it != fConnectedClients.end(); it++)
	{
		try
		{
			if(auto sock = dynamic_cast<TCPTransmitterSocket*>(it->second);
			   sock != nullptr)
				sock->sendPacket(message);
		}
		catch(const std::exception& e)
		{
			// __COUT__ << "I don't think that this error is possible because I close the socket when I get disconnected...if you see this then you should
			// contact Lorenzo Uplegger" << std::endl;
			// __COUT__ << "This should only happen with the TCPSubscribeServer because it doesn't keep track of the connected clients..." << std::endl;
			// __COUT__ << "Error: " << e.what() << std::endl;
			if(fConnectedClientsFuture.find(it->first) != fConnectedClientsFuture.end())
				fConnectedClientsFuture.erase(fConnectedClientsFuture.find(it->first));
			delete it->second;
			fConnectedClients.erase(it--);
		}
	}
}

//========================================================================================================================
void TCPServerBase::broadcast(const char* message, std::size_t length)
{
	//	std::lock_guard<std::mutex> lock(clientsMutex_);
	for(auto it = fConnectedClients.begin(); it != fConnectedClients.end(); it++)
	{
		try
		{
			if(auto sock = dynamic_cast<TCPTransmitterSocket*>(it->second);
			   sock != nullptr)
				sock->send(message, length);
		}
		catch(const std::exception& e)
		{
			// __COUT__ << "I don't think that this error is possible because I close the socket when I get disconnected...if you see this then you should
			// contact Lorenzo Uplegger" << std::endl;
			// __COUT__ << "This should only happen with the TCPSubscribeServer because it doesn't keep track of the connected clients..." << std::endl;
			// __COUT__ << "Error: " << e.what() << std::endl;
			if(fConnectedClientsFuture.find(it->first) != fConnectedClientsFuture.end())
				fConnectedClientsFuture.erase(fConnectedClientsFuture.find(it->first));
			delete it->second;
			fConnectedClients.erase(it--);
		}
	}
}

//==============================================================================
void TCPServerBase::broadcast(const std::string& message)
{
	for(auto it = fConnectedClients.begin(); it != fConnectedClients.end(); it++)
	{
		try
		{
			if(auto sock = dynamic_cast<TCPTransmitterSocket*>(it->second);
			   sock != nullptr)
				sock->send(message);
		}
		catch(const std::exception& e)
		{
			// __COUT__ << "I don't think that this error is possible because I close the socket when I get disconnected...if you see this then you should
			// contact Lorenzo Uplegger" << std::endl;
			// __COUT__ << "This should only happen with the TCPSubscribeServer because it doesn't keep track of the connected clients..." << std::endl;
			// __COUT__ << "Error: " << e.what() << std::endl;
			if(fConnectedClientsFuture.find(it->first) != fConnectedClientsFuture.end())
				fConnectedClientsFuture.erase(fConnectedClientsFuture.find(it->first));
			delete it->second;
			fConnectedClients.erase(it--);
		}
	}
}

//==============================================================================
void TCPServerBase::broadcast(const std::vector<char>& message)
{
	for(auto it = fConnectedClients.begin(); it != fConnectedClients.end(); it++)
	{
		try
		{
			if(auto sock = dynamic_cast<TCPTransmitterSocket*>(it->second);
			   sock != nullptr)
				sock->send(message);
		}
		catch(const std::exception& e)
		{
			// __COUT__ << "I don't think that this error is possible because I close the socket when I get disconnected...if you see this then you should
			// contact Lorenzo Uplegger" << std::endl;
			// __COUT__ << "This should only happen with the TCPSubscribeServer because it doesn't keep track of the connected clients..." << std::endl;
			// __COUT__ << "Error: " << e.what() << std::endl;
			if(fConnectedClientsFuture.find(it->first) != fConnectedClientsFuture.end())
				fConnectedClientsFuture.erase(fConnectedClientsFuture.find(it->first));
			delete it->second;
			fConnectedClients.erase(it--);
		}
	}
}

//==============================================================================
void TCPServerBase::broadcast(const std::vector<uint16_t>& message)
{
	for(auto it = fConnectedClients.begin(); it != fConnectedClients.end(); it++)
	{
		try
		{
			if(auto sock = dynamic_cast<TCPTransmitterSocket*>(it->second);
			   sock != nullptr)
				sock->send(message);
		}
		catch(const std::exception& e)
		{
			// __COUT__ << "This should only happen with the TCPSubscribeServer because it doesn't keep track of the connected clients..." << std::endl;
			__COUT__ << "Error: " << e.what() << std::endl;
			if(fConnectedClientsFuture.find(it->first) != fConnectedClientsFuture.end())
			{
				__COUT__ << "Removing client entry from future connected clients list\n";
				fConnectedClientsFuture.erase(fConnectedClientsFuture.find(it->first));
			}
			__COUT__ << "Removing client entry from connected clients list\n";
			delete it->second;
			fConnectedClients.erase(it--);
		}
	}
}

//==============================================================================
void TCPServerBase::pingActiveClients()
{
	for(auto it = fConnectedClients.begin(); it != fConnectedClients.end(); it++)
	{
		__COUT__ << "Pinging client " << it->first << " : " << it->second << std::endl;
		try
		{
			if(auto sock = dynamic_cast<TCPTransmitterSocket*>(it->second);
			   sock != nullptr)
				sock->send("", 0, true);
		}
		catch(const std::exception& e)
		{
			// __COUT__ << "I don't think that this error is possible because I close the socket when I get disconnected...if you see this then you should
			// contact Lorenzo Uplegger" << std::endl;
			// __COUT__ << "This should only happen with the TCPSubscribeServer because it doesn't keep track of the connected clients..." << std::endl;
			__COUT__ << "Error: " << e.what() << std::endl;
			if(fConnectedClientsFuture.find(it->first) != fConnectedClientsFuture.end())
			{
				__COUT__ << "Removing client entry from future connected clients list\n";
				fConnectedClientsFuture.erase(fConnectedClientsFuture.find(it->first));
			}
			__COUT__ << "Removing client entry from connected clients list\n";
			delete it->second;
			fConnectedClients.erase(it--);
		}
	}
}

//==============================================================================
void TCPServerBase::shutdownAccept()
{
	fAccept = false;
	shutdown(getSocketId(), SHUT_RD);
}
