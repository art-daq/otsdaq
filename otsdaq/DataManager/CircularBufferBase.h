#ifndef _ots_CircularBufferBase_h_
#define _ots_CircularBufferBase_h_

#include <string>

namespace ots
{
class DataProcessor;

// CircularBufferBase
//	This class is the base class for the otsdaq Buffer
class CircularBufferBase
{
  public:
	enum ConsumerPriority
	{
		LowConsumerPriority,  // If the buffers are full because a low priority consumer
		                      // didn't emptied them then overwrite
		HighConsumerPriority  // Can't overwrite but need to wait for sometime before
		                      // writing a buffer
	};

	CircularBufferBase(const std::string& bufferID);
	virtual ~CircularBufferBase(void);

	virtual void reset(void) = 0;
	void         registerProducer(DataProcessor* producer, unsigned int numberOfSubBuffers = 100);
	void         registerConsumer(DataProcessor* consumer);
	// void unregisterProducer(DataProcessor*  producer);
	// void unregisterConsumer(DataProcessor*  consumer);

	virtual bool         isEmpty(void) const                                        = 0;
	virtual unsigned int getTotalNumberOfSubBuffers(void) const                     = 0;
	virtual unsigned int getProducerBufferSize(const std::string& producerID) const = 0;

  protected:
	virtual void registerProducer(const std::string& producerID, unsigned int numberOfSubBuffers = 100) = 0;
	virtual void registerConsumer(const std::string& consumerID, ConsumerPriority priority)             = 0;
	//    virtual void unregisterProducer			(const std::string& producerID) = 0;
	//    virtual void unregisterConsume			r(const std::string& consumerID) = 0;

	std::string dataBufferId_;
	std::string mfSubject_;
};
}  // namespace ots
#endif
