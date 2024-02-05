#ifndef _ots_DataManager_h_
#define _ots_DataManager_h_

#include "otsdaq/Configurable/Configurable.h"
#include "otsdaq/DataManager/CircularBuffer.h"
#include "otsdaq/FiniteStateMachine/VStateMachine.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <map>
#include <memory>
#include <string>
#include <vector>

namespace ots
{
class DataProcessor;
class DataProducerBase;
class DataConsumer;
class CircularBufferBase;

// DataManager
//	This class is the base class that handles a collection of Buffers and associated Data
// Processor plugins.
class DataManager : public Configurable, public VStateMachine
{
  public:
	DataManager(const ConfigurationTree& theXDAQContextConfigTree, const std::string& supervisorConfigurationPath);
	virtual ~DataManager(void);

	// State Machine Methods
	virtual void configure(void);
	virtual void halt(void);
	virtual void pause(void);
	virtual void resume(void);
	virtual void start(std::string runNumber);
	virtual void stop(void);

	template<class D, class H>
	void configureBuffer(const std::string& bufferUID)
	{
		buffers_[bufferUID].buffer_ = new CircularBuffer<D, H>(bufferUID);
		buffers_[bufferUID].status_ = Initialized;
	}

	void registerProducer(const std::string& bufferUID,
	                      DataProducerBase*  producer);  // The data manager becomes the
	                                                    // owner of the producer object!
	void registerConsumer(const std::string& bufferUID,
	                      DataConsumer*      consumer);  // The data manager becomes the owner
	                                                // of the consumer object!

	void unregisterFEProducer(const std::string& bufferID, const std::string& feProducerID);

	// void unregisterConsumer		(const std::string& bufferID, const std::string&
	// consumerID);  void unregisterProducer		(const std::string& bufferID, const
	// std::string& producerID);

	void dumpStatus(std::ostream* out = (std::ostream*)&(std::cout)) const;

  protected:
	void destroyBuffers(void);  //!!!!!Delete all Buffers and all the pointers of the
	                            //! producers and consumers
	// void destroyBuffer     		(const std::string& bufferUID);//!!!!!Delete all the
	// pointers of the producers and consumers

	void configureAllBuffers(void);
	void startAllBuffers(const std::string& runNumber);
	void stopAllBuffers(void);
	void resumeAllBuffers(void);
	void pauseAllBuffers(void);

	void configureBuffer(const std::string& bufferUID);
	void startBuffer(const std::string& bufferUID, std::string runNumber);
	void stopBuffer(const std::string& bufferUID);
	void resumeBuffer(const std::string& bufferUID);
	void pauseBuffer(const std::string& bufferUID);

	enum BufferStatus
	{
		Initialized,
		Running
	};

	struct Buffer
	{
		CircularBufferBase*            buffer_;
		std::vector<DataProducerBase*> producers_;
		std::vector<DataConsumer*>     consumers_;
		BufferStatus                   status_;
	};
	std::map<std::string /*dataBufferId*/, Buffer /*CircularBuffer:=Map of Producer to Buffer Implementations*/> buffers_;

  public:
	bool parentSupervisorHasFrontends_;  // if parent supervisor has front-ends, then
	                                     // allow no producers... that will be checked
	                                     // later by parent supervisor

	const std::map<std::string /*dataBufferId*/, Buffer>& getBuffers(void) const { return buffers_; }
};

}  // namespace ots

#endif
