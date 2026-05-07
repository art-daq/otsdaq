#ifndef _ots_TransceiverSocket_h_
#define _ots_TransceiverSocket_h_

#include "otsdaq/NetworkUtilities/ReceiverSocket.h"
#include "otsdaq/NetworkUtilities/TransmitterSocket.h"

#include <string>
#include <vector>

namespace ots
{
class TransceiverSocket : public TransmitterSocket, public ReceiverSocket
{
  public:
	TransceiverSocket(std::string IPAddress, unsigned int port = 0);
	virtual ~TransceiverSocket(void);

	/// acknowledge() responds to last receive location.
	/// When enableRetransmission is true, delegates to sendAll() for reliable
	/// multi-packet transfer with retransmit handling.
	int acknowledge(const std::string& buffer,
	                bool               verbose                = false,
	                size_t             maxChunkSize           = 1500,
	                unsigned int       interPacketGapUSeconds = 0,
	                bool               enableRetransmission   = false);

	/// sendAll() sends a buffer to the last receive address using the
	/// retransmission protocol. This is fully self-contained: it builds
	/// headered packets, sends them all, then waits for retransmit requests
	/// from the receiver and resends any missing packets. Only returns when
	/// the transfer is complete (receiver sends "done") or timeout expires.
	///
	/// Each packet is prepended with an 8-byte retransmission header:
	///   [0-1] magic marker 0xD2C4 (network byte order)
	///   [2-3] packet index (0-based, network byte order uint16)
	///   [4-5] total packet count (network byte order uint16)
	///   [6-7] payload size in this packet (network byte order uint16)
	///
	/// Returns 0 on success.
	int sendAll(const std::string& buffer,
	            bool               verbose                = false,
	            size_t             maxChunkSize           = 65500,
	            unsigned int       interPacketGapUSeconds = 0);

	/// receiveAll() receives a multi-packet retransmission-mode response.
	/// It assembles the full message from individually-headered packets,
	/// detects missing packets by index, and requests retransmission
	/// from the sender for any dropped packets.
	/// Returns 0 on success (assembled buffer placed in 'buffer'), -1 on failure.
	int receiveAll(std::string& buffer,
	               unsigned int timeoutSeconds       = 5,
	               unsigned int retransmitMaxRetries = 10,
	               bool         verbose              = false);

	std::string sendAndReceive(Socket&            toSocket,
	                           const std::string& sendBuffer,
	                           unsigned int       timeoutSeconds             = 1,
	                           unsigned int       timeoutUSeconds            = 0,
	                           bool               verbose                    = false,
	                           unsigned int       interPacketTimeoutUSeconds = 10000);

	/// sendAndReceiveAll() sends a command then uses the retransmission protocol
	/// to reliably receive the full multi-packet response. The sender must use
	/// acknowledge() with enableRetransmission=true (or sendAll()). This method:
	///   1. Flushes and sends the request
	///   2. Receives all retransmission-headered packets
	///   3. Detects missing packets and sends retransmit requests
	///   4. Assembles and returns the complete response
	/// Throws on timeout or error.
	std::string sendAndReceiveAll(Socket&            toSocket,
	                              const std::string& sendBuffer,
	                              unsigned int       timeoutSeconds       = 5,
	                              unsigned int       retransmitMaxRetries = 10,
	                              bool               verbose              = false);

	/// Retransmission protocol constants
	static constexpr uint16_t RETRANSMIT_MAGIC       = 0xD2C4;
	static constexpr size_t   RETRANSMIT_HEADER_SIZE = 8;

  protected:
	TransceiverSocket(void);

  private:
	std::mutex sendAndReceiveMutex_;
};

}  // namespace ots

#endif
