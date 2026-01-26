#include "otsdaq/NetworkUtilities/TransceiverSocket.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <iostream>
#include <thread>  // std::this_thread

using namespace ots;

//==============================================================================
TransceiverSocket::TransceiverSocket(void)
{
	__COUT__ << "TransceiverSocket constructor " << __E__;
}

//==============================================================================
TransceiverSocket::TransceiverSocket(std::string IPAddress, unsigned int port)
    : Socket(IPAddress, port)
{
	__COUT__ << "TransceiverSocket constructor " << IPAddress << ":" << port << __E__;
}

//==============================================================================
TransceiverSocket::~TransceiverSocket(void) {}

//==============================================================================
/// returns 0 on success
int TransceiverSocket::acknowledge(const std::string& buffer, bool verbose)
{
	// lockout other senders for the remainder of the scope
	std::lock_guard<std::mutex> lock(sendMutex_);

	if(verbose)
		__COUTT__ << "Acknowledging on Socket Descriptor #: " << socketNumber_
		          << " from-port: " << ntohs(socketAddress_.sin_port)
		          << " to-port: " << ntohs(ReceiverSocket::fromAddress_.sin_port)
		          << std::endl;

	constexpr size_t MAX_SEND_SIZE = 1500;
	size_t           offset        = 0;
	int              sendToSize    = 1;

	int sizeInBytes = 1;

	while(offset < buffer.size() && sendToSize > 0)
	{
		auto thisSize = sizeInBytes * (buffer.size() - offset) > MAX_SEND_SIZE
		                    ? MAX_SEND_SIZE
		                    : sizeInBytes * (buffer.size() - offset);
		if(verbose)
			__COUTTV__(thisSize);
		sendToSize = sendto(socketNumber_,
		                    &buffer[0] + offset,
		                    thisSize,
		                    0,
		                    (struct sockaddr*)&(ReceiverSocket::fromAddress_),
		                    sizeof(sockaddr_in));
		offset += sendToSize / sizeInBytes;
	}

	if(sendToSize <= 0)
	{
		__SS__ << "Error writing buffer from port "
		       << ntohs(TransmitterSocket::socketAddress_.sin_port) << ": "
		       << strerror(errno) << std::endl;
		__SS_THROW__;  //return -1;
	}

	return 0;
}  //end acknowledge()

//==============================================================================
std::string TransceiverSocket::sendAndReceive(Socket&            toSocket,
                                              const std::string& sendBuffer,
                                              unsigned int       timeoutSeconds /* = 1 */,
                                              unsigned int timeoutUSeconds /* = 0 */,
                                              bool         verbose /* = false */)
{
	using clock = std::chrono::steady_clock;
	auto start  = clock::now();

	// lockout other sender and receive attempts for the remainder of the scope
	std::lock_guard<std::mutex> lock(
	    sendAndReceiveMutex_);  //note that TransmitterSocket::sendMutex_ is not enough

	flush();  //make sure nothing to read before sending
	send(toSocket, sendBuffer, verbose);

	__COUTT__ << " ----> Time sendAndReceive '" << sendBuffer
	          << "' (socketNumber=" << socketNumber_ << ") check ==> "
	          << std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() -
	                                                                   start)
	                 .count()
	          << " milliseconds. PID=" << getpid()
	          << " TID=" << std::this_thread::get_id() << std::endl;

	std::string receiveBuffer;
	if(receive(receiveBuffer, timeoutSeconds, timeoutUSeconds, verbose) < 0)
	{
		__SS__ << "Timeout (" << timeoutSeconds + timeoutUSeconds / 1000000.
		       << " s) or Error receiving response buffer from remote ip:port "
		       << toSocket.getIPAddress() << ":" << toSocket.getPort()
		       << " to this ip:port " << Socket::getIPAddress() << ":"
		       << Socket::getPort() << __E__;
		__SS_ONLY_THROW__;
	}

	__COUTT__ << " ----> Time sendAndReceive '" << sendBuffer << "' got "
	          << receiveBuffer.size() << " (socketNumber=" << socketNumber_
	          << ") check ==> "
	          << std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() -
	                                                                   start)
	                 .count()
	          << " milliseconds. PID=" << getpid()
	          << " TID=" << std::this_thread::get_id() << std::endl;

	//assume response may be multiple packets! (and give 10 ms unless called with lower timeout)
	std::string receiveBuffer2;
	while(receive(receiveBuffer2,
	              0 /*timeoutSeconds*/,
	              (timeoutSeconds == 0 && timeoutUSeconds < 10000)
	                  ? timeoutUSeconds
	                  : 10000 /*timeoutUSeconds*/,
	              verbose) >= 0)
	{
		receiveBuffer += receiveBuffer2;  //append

		__COUTT__ << " ----> Time sendAndReceive +" << receiveBuffer2.size()
		          << " check ==> "
		          << std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() -
		                                                                   start)
		                 .count()
		          << " milliseconds." << std::endl;
	}

	__COUTT__ << " ----> Time sendAndReceive " << receiveBuffer.size() << " check ==> "
	          << std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() -
	                                                                   start)
	                 .count()
	          << " milliseconds." << std::endl;

	return receiveBuffer;
}  //end sendAndReceive()
