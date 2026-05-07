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

	/// acknowledge() responds to last receive location.
	/// When enableRetransmission is true, each sent packet is prepended with an 8-byte
	/// retransmission header so the receiver can detect dropped packets and request
	/// retransmission. The header format is:
	///   [0-1] magic marker 0xD2C4 (network byte order)
	///   [2-3] packet index (0-based, network byte order uint16)
	///   [4-5] total packet count (network byte order uint16)
	///   [6-7] payload size in this packet (network byte order uint16)
	int acknowledge(const std::string& buffer,
	                bool               verbose                = false,
	                size_t             maxChunkSize           = 1500,
	                unsigned int       interPacketGapUSeconds = 0,
	                bool               enableRetransmission   = false);

	/// receiveAll() receives a multi-packet retransmission-mode response.
	/// It assembles the full message from individually-headered packets,
	/// detects missing packets by index, and requests retransmission
	/// from the sender for any dropped packets.
	/// Returns 0 on success (assembled buffer placed in 'buffer'), -1 on failure.
	int receiveAll(std::string& buffer,
	               unsigned int timeoutSeconds       = 5,
	               unsigned int retransmitMaxRetries  = 10,
	               bool         verbose               = false);

	std::string sendAndReceive(Socket&            toSocket,
	                           const std::string& sendBuffer,
	                           unsigned int       timeoutSeconds             = 1,
	                           unsigned int       timeoutUSeconds            = 0,
	                           bool               verbose                    = false,
	                           unsigned int       interPacketTimeoutUSeconds = 10000);

	/// sendAndReceiveAll() sends a command then uses the retransmission protocol
	/// to reliably receive the full multi-packet response. The sender must use
	/// acknowledge() with enableRetransmission=true. This method handles:
	///   1. Flushing and sending the request
	///   2. Receiving all retransmission-headered packets
	///   3. Detecting missing packets and sending retransmit requests
	///   4. Assembling and returning the complete response
	/// Throws on timeout or error.
	std::string sendAndReceiveAll(Socket&            toSocket,
	                              const std::string& sendBuffer,
	                              unsigned int       timeoutSeconds         = 5,
	                              unsigned int       retransmitMaxRetries   = 10,
	                              bool               verbose                = false);

	/// Retransmission protocol constants
	static constexpr uint16_t RETRANSMIT_MAGIC      = 0xD2C4;
	static constexpr size_t   RETRANSMIT_HEADER_SIZE = 8;

  protected:
	TransceiverSocket(void);

  private:
	std::mutex sendAndReceiveMutex_;
};

}  // namespace ots

#endif
