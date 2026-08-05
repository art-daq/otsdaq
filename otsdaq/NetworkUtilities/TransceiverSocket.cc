#include "otsdaq/NetworkUtilities/TransceiverSocket.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <arpa/inet.h>
#include <unistd.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <mutex>
#include <set>
#include <thread>
#include <vector>

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
/// When enableRetransmission is true, uses sendAll() to send the buffer with
/// retransmission headers, then waits for retransmit requests from the receiver.
int TransceiverSocket::acknowledge(const std::string& buffer,
                                   bool               verbose /* = false */,
                                   size_t             maxChunkSize /* = 1500 */,
                                   unsigned int       interPacketGapUSeconds /* = 0 */,
                                   bool               enableRetransmission /* = false */)
{
	if(verbose)
		__COUTT__ << "Acknowledging on Socket Descriptor #: " << socketNumber_
		          << " from-port: " << ntohs(socketAddress_.sin_port)
		          << " to-port: " << ntohs(ReceiverSocket::fromAddress_.sin_port)
		          << " retransmission: " << (enableRetransmission ? "ON" : "OFF")
		          << std::endl;

	if(!enableRetransmission)
	{
		//====================================================================
		// Original non-retransmission mode (unchanged behavior)
		//====================================================================
		// lockout other senders for the remainder of this scope
		std::lock_guard<std::mutex> lock(sendMutex_);

		const size_t MAX_SEND_SIZE =
		    maxChunkSize > 65500u ? static_cast<size_t>(65500u) : maxChunkSize;
		size_t offset      = 0;
		int    sendToSize  = 1;
		int    sizeInBytes = 1;

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
			if(interPacketGapUSeconds > 0 && offset < buffer.size() && sendToSize > 0)
				usleep(interPacketGapUSeconds);
		}

		if(sendToSize <= 0)
		{
			__SS__ << "Error writing buffer from port "
			       << ntohs(TransmitterSocket::socketAddress_.sin_port) << ": "
			       << strerror(errno) << std::endl;
			__SS_THROW__;
		}
		return 0;
	}

	//====================================================================
	// Retransmission mode: delegate entirely to sendAll() which handles
	// packet building, initial send, and retransmit request handling.
	//====================================================================
	return sendAll(buffer, verbose, maxChunkSize, interPacketGapUSeconds);
}  //end acknowledge()

//==============================================================================
/// sendAll() sends a buffer to the last receive address (fromAddress_) using the
/// retransmission protocol. Fully self-contained:
///   1. Builds all packets with 8-byte retransmission headers
///   2. Sends all packets
///   3. Waits for retransmit requests from the receiver
///   4. Resends requested packets
///   5. Returns when receiver sends "done" or timeout expires
///
/// Returns 0 on success.
int TransceiverSocket::sendAll(const std::string& buffer,
                               bool               verbose /* = false */,
                               size_t             maxChunkSize /* = 65500 */,
                               unsigned int       interPacketGapUSeconds /* = 0 */)
{
	if(verbose)
		__COUT__ << "sendAll: retransmission-mode send on Socket Descriptor #: "
		         << socketNumber_ << " from-port: " << ntohs(socketAddress_.sin_port)
		         << " to-port: " << ntohs(ReceiverSocket::fromAddress_.sin_port)
		         << " buffer size: " << buffer.size() << __E__;

	const size_t MAX_SEND_SIZE =
	    maxChunkSize > 65500u ? static_cast<size_t>(65500u) : maxChunkSize;

	// The payload per packet is reduced by the header size
	const size_t payloadMax = MAX_SEND_SIZE > RETRANSMIT_HEADER_SIZE
	                              ? MAX_SEND_SIZE - RETRANSMIT_HEADER_SIZE
	                              : 1;

	// Calculate total number of packets. The on-wire header carries the count
	// as uint16_t, so reject buffers that would require more than 65535 packets.
	const size_t packetsNeeded = (buffer.size() + payloadMax - 1) / payloadMax;
	if(packetsNeeded > std::numeric_limits<uint16_t>::max())
	{
		__SS__ << "sendAll: buffer size " << buffer.size() << " requires "
		       << packetsNeeded
		       << " packets, which exceeds the uint16_t protocol limit of "
		       << std::numeric_limits<uint16_t>::max() << " (payloadMax=" << payloadMax
		       << ")." << std::endl;
		__SS_THROW__;
	}
	uint16_t totalPackets = static_cast<uint16_t>(packetsNeeded);
	if(totalPackets == 0)
		totalPackets = 1;  // send at least one packet even for empty buffer

	if(verbose)
		__COUT__ << "sendAll: sending " << totalPackets << " packets for "
		         << buffer.size() << " bytes, payloadMax=" << payloadMax << __E__;

	// Build and cache all packets (header + payload) for retransmit use
	std::vector<std::string> packets(totalPackets);
	{
		size_t offset = 0;
		for(uint16_t pi = 0; pi < totalPackets; ++pi)
		{
			size_t payloadSize = (buffer.size() - offset) > payloadMax
			                         ? payloadMax
			                         : (buffer.size() - offset);

			char     header[RETRANSMIT_HEADER_SIZE];
			uint16_t netMagic   = htons(RETRANSMIT_MAGIC);
			uint16_t netIndex   = htons(pi);
			uint16_t netTotal   = htons(totalPackets);
			uint16_t netPaySize = htons(static_cast<uint16_t>(payloadSize));
			std::memcpy(header + 0, &netMagic, 2);
			std::memcpy(header + 2, &netIndex, 2);
			std::memcpy(header + 4, &netTotal, 2);
			std::memcpy(header + 6, &netPaySize, 2);

			packets[pi].assign(header, RETRANSMIT_HEADER_SIZE);
			packets[pi].append(buffer, offset, payloadSize);
			offset += payloadSize;
		}
	}

	// Send all packets initially (lock sendMutex_ for the burst)
	{
		std::lock_guard<std::mutex> lock(sendMutex_);
		for(uint16_t pi = 0; pi < totalPackets; ++pi)
		{
			int sendToSize = sendto(socketNumber_,
			                        packets[pi].data(),
			                        packets[pi].size(),
			                        0,
			                        (struct sockaddr*)&(ReceiverSocket::fromAddress_),
			                        sizeof(sockaddr_in));
			if(sendToSize <= 0)
			{
				__SS__ << "sendAll: error writing packet " << pi << "/" << totalPackets
				       << " from port " << ntohs(socketAddress_.sin_port) << ": "
				       << strerror(errno) << std::endl;
				__SS_THROW__;
			}
			if(verbose)
				__COUTT__ << "sendAll: sent packet " << pi << "/" << totalPackets
				          << " size=" << packets[pi].size() << std::endl;

			if(interPacketGapUSeconds > 0 && pi + 1 < totalPackets)
				usleep(interPacketGapUSeconds);
		}
	}

	// Wait for retransmit requests from receiver.
	// Retransmit request format: magic(2 bytes) + list of uint16 missing indices
	// Done signal format:        magic(2 bytes) + 0xFFFF(2 bytes)
	const unsigned int retransmitTimeoutSeconds = 5;
	const unsigned int maxRetransmitRounds      = 20;

	for(unsigned int round = 0; round < maxRetransmitRounds; ++round)
	{
		std::string retransmitRequest;
		int         rc = receive(retransmitRequest,
                         retransmitTimeoutSeconds,
                         0 /*timeoutUSeconds*/,
                         false /*verbose*/);
		if(rc < 0)
		{
			// Timeout - assume receiver got everything (or gave up)
			if(verbose)
				__COUT__ << "sendAll: no retransmit request after "
				         << retransmitTimeoutSeconds
				         << "s timeout, assuming transfer complete." << __E__;
			break;
		}

		if(retransmitRequest.size() < 4)
			continue;

		uint16_t reqMagic;
		std::memcpy(&reqMagic, retransmitRequest.data(), 2);
		reqMagic = ntohs(reqMagic);
		if(reqMagic != RETRANSMIT_MAGIC)
			continue;

		// Check for "done" signal (magic + 0xFFFF)
		uint16_t firstVal;
		std::memcpy(&firstVal, retransmitRequest.data() + 2, 2);
		firstVal = ntohs(firstVal);
		if(firstVal == 0xFFFF)
		{
			if(verbose)
				__COUT__ << "sendAll: received 'all done' from receiver." << __E__;
			break;
		}

		// Parse list of missing packet indices and resend them
		size_t numIndices = (retransmitRequest.size() - 2) / 2;
		if(verbose)
			__COUT__ << "sendAll: retransmit request for " << numIndices
			         << " packets (round " << round << ")." << __E__;

		// Lock sendMutex_ for the resend burst
		std::lock_guard<std::mutex> lock(sendMutex_);
		for(size_t i = 0; i < numIndices; ++i)
		{
			uint16_t missingIdx;
			std::memcpy(&missingIdx, retransmitRequest.data() + 2 + i * 2, 2);
			missingIdx = ntohs(missingIdx);

			if(missingIdx < totalPackets)
			{
				int sendToSize = sendto(socketNumber_,
				                        packets[missingIdx].data(),
				                        packets[missingIdx].size(),
				                        0,
				                        (struct sockaddr*)&(ReceiverSocket::fromAddress_),
				                        sizeof(sockaddr_in));
				if(sendToSize <= 0)
				{
					__SS__ << "sendAll: error resending packet " << missingIdx << ": "
					       << strerror(errno) << std::endl;
					__SS_THROW__;
				}
				if(verbose)
					__COUTT__ << "sendAll: resent packet " << missingIdx << std::endl;

				if(interPacketGapUSeconds > 0)
					usleep(interPacketGapUSeconds);
			}
			else
			{
				__COUT_WARN__ << "sendAll: retransmit request for invalid packet index "
				              << missingIdx << " (total=" << totalPackets << ")" << __E__;
			}
		}
	}

	return 0;
}  //end sendAll()

//==============================================================================
/// Receives one packet with the specified timeout, then attempts to receive
/// additional packets with interPacketTimeoutUSeconds timeout to handle multi-packet responses.
/// Returns the combined received buffer or throws on error/timeout.
std::string TransceiverSocket::sendAndReceive(
    Socket&            toSocket,
    const std::string& sendBuffer,
    unsigned int       timeoutSeconds /* = 1 */,
    unsigned int       timeoutUSeconds /* = 0 */,
    bool               verbose /* = false */,
    unsigned int       interPacketTimeoutUSeconds /* = 10000 */)
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

	//assume response may be multiple packets! (and give interPacketTimeoutUSeconds unless called with lower timeout)
	size_t      extraPackets = 0;
	std::string receiveBuffer2;
	while(receive(receiveBuffer2,
	              0 /*timeoutSeconds*/,
	              (timeoutSeconds == 0 && timeoutUSeconds < interPacketTimeoutUSeconds)
	                  ? timeoutUSeconds
	                  : interPacketTimeoutUSeconds,
	              verbose) >= 0)
	{
		++extraPackets;
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

//==============================================================================
/// receiveAll() receives a multi-packet retransmission-mode response.
/// Packets are expected to have an 8-byte retransmission header:
///   [0-1] magic 0xD2C4  (network byte order)
///   [2-3] packet index   (network byte order uint16)
///   [4-5] total packets  (network byte order uint16)
///   [6-7] payload size   (network byte order uint16)
///
/// After all initial packets are received (or timeout), missing packets are
/// identified and a retransmit request is sent back to the sender containing
/// the magic marker followed by the list of missing packet indices. This
/// repeats up to retransmitMaxRetries times. When all packets are received,
/// a "done" signal (magic + 0xFFFF) is sent to the sender.
///
/// Returns 0 on success (assembled buffer placed in 'buffer'), -1 on failure.
int TransceiverSocket::receiveAll(std::string& buffer,
                                  unsigned int timeoutSeconds /* = 5 */,
                                  unsigned int retransmitMaxRetries /* = 10 */,
                                  bool         verbose /* = false */)
{
	using clock = std::chrono::steady_clock;
	auto start  = clock::now();

	// Map of packet index -> payload data
	std::map<uint16_t, std::string> receivedPackets;
	uint16_t                        totalPackets = 0;
	bool                            totalKnown   = false;

	if(verbose)
		__COUT__ << "receiveAll: waiting for retransmission-mode packets, timeout="
		         << timeoutSeconds << "s" << __E__;

	// Phase 1: Receive all initial packets until timeout
	// Use a per-packet timeout that is shorter than the overall timeout,
	// so we can detect "no more packets arriving" vs "still waiting for first"
	const unsigned int interPacketTimeoutUSeconds = 100000;  // 100ms between packets
	bool               firstPacketReceived        = false;

	while(true)
	{
		std::string rawPacket;
		int         rc = receive(rawPacket,
                         firstPacketReceived ? 0 : timeoutSeconds,
                         firstPacketReceived ? interPacketTimeoutUSeconds : 0,
                         false /*verbose*/);

		if(rc < 0)
		{
			if(!firstPacketReceived)
			{
				// Never received any packet at all
				if(verbose)
					__COUT__ << "receiveAll: timeout waiting for first packet after "
					         << timeoutSeconds << "s" << __E__;
				return -1;
			}
			// Timeout between packets - move to retransmit phase
			break;
		}

		// Check for retransmission header
		if(rawPacket.size() < RETRANSMIT_HEADER_SIZE)
		{
			// Too small to be a retransmission packet - might be a non-retransmit
			// response; just return it as-is
			if(!firstPacketReceived)
			{
				buffer = rawPacket;
				return 0;
			}
			// Skip malformed packet during multi-packet receive
			if(verbose)
				__COUT_WARN__ << "receiveAll: skipping undersized packet ("
				              << rawPacket.size() << " bytes)" << __E__;
			continue;
		}

		// Parse header
		uint16_t magic, packetIndex, pktTotal, payloadSize;
		std::memcpy(&magic, rawPacket.data() + 0, 2);
		std::memcpy(&packetIndex, rawPacket.data() + 2, 2);
		std::memcpy(&pktTotal, rawPacket.data() + 4, 2);
		std::memcpy(&payloadSize, rawPacket.data() + 6, 2);
		magic       = ntohs(magic);
		packetIndex = ntohs(packetIndex);
		pktTotal    = ntohs(pktTotal);
		payloadSize = ntohs(payloadSize);

		if(magic != RETRANSMIT_MAGIC)
		{
			// Not a retransmission packet - if first packet, return as-is
			if(!firstPacketReceived)
			{
				buffer = rawPacket;
				return 0;
			}
			if(verbose)
				__COUT_WARN__ << "receiveAll: skipping packet with bad magic 0x"
				              << std::hex << magic << std::dec << __E__;
			continue;
		}

		firstPacketReceived = true;
		totalPackets        = pktTotal;
		totalKnown          = true;

		// Extract payload (everything after the 8-byte header, limited by payloadSize)
		size_t actualPayload = rawPacket.size() - RETRANSMIT_HEADER_SIZE;
		if(actualPayload > payloadSize)
			actualPayload = payloadSize;

		receivedPackets[packetIndex] =
		    rawPacket.substr(RETRANSMIT_HEADER_SIZE, actualPayload);

		if(verbose)
			__COUTT__ << "receiveAll: received packet " << packetIndex << "/"
			          << totalPackets << " payload=" << actualPayload
			          << " total_received=" << receivedPackets.size() << std::endl;

		// Check if we have all packets
		if(totalKnown && receivedPackets.size() >= static_cast<size_t>(totalPackets))
			break;

		// Check overall timeout
		auto elapsed =
		    std::chrono::duration_cast<std::chrono::seconds>(clock::now() - start);
		if(elapsed.count() >=
		   static_cast<long>(timeoutSeconds * (retransmitMaxRetries + 1)))
		{
			if(verbose)
				__COUT_WARN__ << "receiveAll: overall timeout reached" << __E__;
			break;
		}
	}

	// Phase 2: Retransmit missing packets
	if(totalKnown && receivedPackets.size() < static_cast<size_t>(totalPackets))
	{
		for(unsigned int retry = 0; retry < retransmitMaxRetries; ++retry)
		{
			// Build list of missing packet indices
			std::set<uint16_t> missing;
			for(uint16_t i = 0; i < totalPackets; ++i)
			{
				if(receivedPackets.find(i) == receivedPackets.end())
					missing.insert(i);
			}

			if(missing.empty())
				break;

			if(verbose)
				__COUT__ << "receiveAll: retry " << retry + 1 << "/"
				         << retransmitMaxRetries << ", requesting retransmit of "
				         << missing.size() << " packets" << __E__;

			// Build retransmit request: magic(2 bytes) + list of uint16 indices
			std::string retransmitReq;
			retransmitReq.resize(2 + missing.size() * 2);
			uint16_t netMagic = htons(RETRANSMIT_MAGIC);
			std::memcpy(&retransmitReq[0], &netMagic, 2);
			size_t pos = 2;
			for(uint16_t idx : missing)
			{
				uint16_t netIdx = htons(idx);
				std::memcpy(&retransmitReq[pos], &netIdx, 2);
				pos += 2;
			}

			// Send retransmit request back to sender (acknowledge to last receive addr)
			{
				// Use sendto directly to fromAddress_ (the sender)
				int sendToSize = sendto(socketNumber_,
				                        retransmitReq.data(),
				                        retransmitReq.size(),
				                        0,
				                        (struct sockaddr*)&(ReceiverSocket::fromAddress_),
				                        sizeof(sockaddr_in));
				if(sendToSize <= 0)
				{
					__COUT_WARN__ << "receiveAll: failed to send retransmit request: "
					              << strerror(errno) << __E__;
				}
			}

			// Receive retransmitted packets
			while(true)
			{
				std::string rawPacket;
				int         rc = receive(
                    rawPacket, timeoutSeconds, 0 /*timeoutUSeconds*/, false /*verbose*/);
				if(rc < 0)
					break;  // timeout, will retry

				if(rawPacket.size() < RETRANSMIT_HEADER_SIZE)
					continue;

				uint16_t magic2, packetIndex2, pktTotal2, payloadSize2;
				std::memcpy(&magic2, rawPacket.data() + 0, 2);
				std::memcpy(&packetIndex2, rawPacket.data() + 2, 2);
				std::memcpy(&pktTotal2, rawPacket.data() + 4, 2);
				std::memcpy(&payloadSize2, rawPacket.data() + 6, 2);
				magic2       = ntohs(magic2);
				packetIndex2 = ntohs(packetIndex2);
				pktTotal2    = ntohs(pktTotal2);
				payloadSize2 = ntohs(payloadSize2);

				if(magic2 != RETRANSMIT_MAGIC)
					continue;

				size_t actualPayload2 = rawPacket.size() - RETRANSMIT_HEADER_SIZE;
				if(actualPayload2 > payloadSize2)
					actualPayload2 = payloadSize2;

				receivedPackets[packetIndex2] =
				    rawPacket.substr(RETRANSMIT_HEADER_SIZE, actualPayload2);

				if(verbose)
					__COUTT__ << "receiveAll: retransmit received packet " << packetIndex2
					          << "/" << totalPackets
					          << " total_received=" << receivedPackets.size()
					          << std::endl;

				// Check if we now have all packets
				if(receivedPackets.size() >= static_cast<size_t>(totalPackets))
					break;
			}

			if(receivedPackets.size() >= static_cast<size_t>(totalPackets))
				break;
		}
	}

	// Phase 3: Send "done" acknowledgment to sender (magic + 0xFFFF)
	{
		std::string doneSignal(4, '\0');
		uint16_t    netMagic = htons(RETRANSMIT_MAGIC);
		uint16_t    netDone  = htons(0xFFFF);
		std::memcpy(&doneSignal[0], &netMagic, 2);
		std::memcpy(&doneSignal[2], &netDone, 2);
		sendto(socketNumber_,
		       doneSignal.data(),
		       doneSignal.size(),
		       0,
		       (struct sockaddr*)&(ReceiverSocket::fromAddress_),
		       sizeof(sockaddr_in));
	}

	// Phase 4: Assemble full buffer in order
	if(!totalKnown || receivedPackets.empty())
	{
		__SS__ << "receiveAll: failed to receive any retransmission-mode packets"
		       << __E__;
		__SS_THROW__;
	}

	if(receivedPackets.size() < static_cast<size_t>(totalPackets))
	{
		// Build list of still-missing indices for the error message
		std::string missingStr;
		for(uint16_t i = 0; i < totalPackets; ++i)
		{
			if(receivedPackets.find(i) == receivedPackets.end())
			{
				if(!missingStr.empty())
					missingStr += ", ";
				missingStr += std::to_string(i);
			}
		}
		__SS__ << "receiveAll: failed to receive all packets after "
		       << retransmitMaxRetries << " retransmit retries. "
		       << "Received " << receivedPackets.size() << "/" << totalPackets
		       << " packets. Missing indices: [" << missingStr << "]" << __E__;
		__SS_THROW__;
	}

	// Assemble in order
	buffer.clear();
	for(uint16_t i = 0; i < totalPackets; ++i)
		buffer += receivedPackets[i];

	if(verbose)
		__COUT__ << "receiveAll: successfully assembled " << buffer.size()
		         << " bytes from " << totalPackets << " packets in "
		         << std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() -
		                                                                  start)
		                .count()
		         << " ms" << __E__;

	return 0;
}  //end receiveAll()

//==============================================================================
/// sendAndReceiveAll() sends a command then reliably receives the full
/// multi-packet response using the retransmission protocol.
/// The remote sender must use acknowledge() with enableRetransmission=true.
///
/// This mirrors sendAndReceive() but uses receiveAll() instead of the
/// simple multi-packet loop, providing:
///   - Packet ordering via indexed headers
///   - Dropped packet detection via known total count
///   - Automatic retransmit requests for missing packets
///   - Fully assembled, ordered response buffer
///
/// Throws on timeout or error.
std::string TransceiverSocket::sendAndReceiveAll(
    Socket&            toSocket,
    const std::string& sendBuffer,
    unsigned int       timeoutSeconds /* = 5 */,
    unsigned int       retransmitMaxRetries /* = 10 */,
    bool               verbose /* = false */)
{
	using clock = std::chrono::steady_clock;
	auto start  = clock::now();

	// lockout other sender and receive attempts for the remainder of the scope
	std::lock_guard<std::mutex> lock(sendAndReceiveMutex_);

	flush();  // make sure nothing to read before sending
	send(toSocket, sendBuffer, verbose);

	__COUTT__ << " ----> Time sendAndReceiveAll '" << sendBuffer
	          << "' (socketNumber=" << socketNumber_ << ") check ==> "
	          << std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() -
	                                                                   start)
	                 .count()
	          << " milliseconds. PID=" << getpid()
	          << " TID=" << std::this_thread::get_id() << std::endl;

	std::string receiveBuffer;
	if(receiveAll(receiveBuffer, timeoutSeconds, retransmitMaxRetries, verbose) < 0)
	{
		__SS__ << "Timeout (" << timeoutSeconds
		       << " s) or Error receiving retransmission response from remote ip:port "
		       << toSocket.getIPAddress() << ":" << toSocket.getPort()
		       << " to this ip:port " << Socket::getIPAddress() << ":"
		       << Socket::getPort() << __E__;
		__SS_ONLY_THROW__;
	}

	__COUTT__ << " ----> Time sendAndReceiveAll complete: " << receiveBuffer.size()
	          << " bytes (socketNumber=" << socketNumber_ << ") ==> "
	          << std::chrono::duration_cast<std::chrono::milliseconds>(clock::now() -
	                                                                   start)
	                 .count()
	          << " milliseconds. PID=" << getpid()
	          << " TID=" << std::this_thread::get_id() << std::endl;

	return receiveBuffer;
}  //end sendAndReceiveAll()
