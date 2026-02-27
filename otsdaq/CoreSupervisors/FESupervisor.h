#ifndef _ots_FESupervisor_h_
#define _ots_FESupervisor_h_

#include "otsdaq/CoreSupervisors/CoreSupervisorBase.h"

#include "zmq.hpp"

namespace ots
{
class FEVInterfacesManager;

/// FESupervisor
///	This class handles a collection of front-end interface plugins. It
///	provides an interface to Macro Maker for writes and reads to the front-end interfaces.
///
///	ZeroMQ Publishing:
///	- To enable ZeroMQ publishing, configure the ZMQPublisherEndpoint parameter
///	- Call publish() method to send data over ZeroMQ PUB socket
class FESupervisor : public CoreSupervisorBase
{
	// friend FEVInterface;

  public:
	XDAQ_INSTANTIATOR();

	FESupervisor(xdaq::ApplicationStub* s);
	virtual ~FESupervisor(void);

	xoap::MessageReference         frontEndCommunicationRequest(xoap::MessageReference message);
	xoap::MessageReference         macroMakerSupervisorRequest(xoap::MessageReference message);
	virtual xoap::MessageReference workLoopStatusRequest(
	    xoap::MessageReference message) override;

	virtual void transitionConfiguring(toolbox::Event::Reference event) override;
	virtual void transitionHalting(toolbox::Event::Reference event) override;

	/// Publish a raw binary payload.
	///
	/// multipart format:
	///   frame‑0 : topic (as set in `init()`)
	///   frame‑1 : payload (exactly `sz` bytes taken from `data`)
	///
	/// @param data   Pointer to the payload buffer.
	/// @param sz     Size of the payload in bytes.
	/// @throws std::runtime_error if the socket is not initialised or
	///         the send operation fails.
	void publishData(const char* dataPtr, size_t dataSize);
	bool isPublishingData() const { return dp_isInitialized_; }

  protected:
	FEVInterfacesManager* theFEInterfacesManager_;

	/// Initialise the publisher.
	///
	/// @param endpoint   The ZMQ endpoint to bind to.
	///                   Examples:
	///                       "inproc://my_stream"
	///                       "tcp://127.0.0.1:5555"
	/// @param topic      Topic string.  It is sent as a separate ZMQ
	///                   frame before every payload.  The length is not
	///                   forced – you can pick any size you like.
	/// @throws std::runtime_error on bind failure.
	void initDataPublishing(const std::string& endpoint,
	                        const std::string& topic = "test");

	/// Close the socket and context explicitly.
	/// After a call to `close()` you must call `init()` again before
	/// publishing.
	void closeDataPublishing();

  private:
	FEVInterfacesManager*
	extractFEInterfacesManager();  ///< likely, just used in constructor

	// ZeroMQ Publisher
	zmq::context_t dp_context_;                ///< ZeroMQ context (1 I/O thread by default)
	zmq::socket_t  dp_socket_;                 ///< PUB socket
	std::string    dp_endpoint_;               ///< Cached endpoint string (for debugging)
	std::string    dp_topic_;                  ///< Cached topic string (sent with every payload)
	bool           dp_isInitialized_ = false;  ///< Guard: have we called init() ?
};

}  // namespace ots

#endif
