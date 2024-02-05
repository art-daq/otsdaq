#ifndef _ots_DataConsumer_h_
#define _ots_DataConsumer_h_

#include <map>
#include <string>
#include "otsdaq/DataManager/DataProcessor.h"

namespace ots
{
// DataConsumer
//	This class provides base class functionality for Data Consumer plugin classes to
//	extracts and process streaming data from a Buffer.
class DataConsumer : public DataProcessor, public virtual WorkLoop
{
  public:
	enum ConsumerPriority
	{
		LowConsumerPriority,  // If the buffers are full because a low priority consumer
		                      // didn't emptied them then overwrite
		HighConsumerPriority  // Can't overwrite but need to wait for sometime before
		                      // writing a buffer
	};
	DataConsumer(std::string supervisorApplicationUID, std::string bufferUID, std::string processorUID, ConsumerPriority priority);
	virtual ~DataConsumer(void);

	virtual void registerToBuffer(void);
	// virtual void         unregisterFromBuffer  	(void);

	virtual void configure(void) { ; }
	virtual void startProcessingData(std::string runNumber);
	virtual void stopProcessingData(void);

	// Copies the buffer into the passed parameters
	template<class D, class H>
	int read(D& buffer, H& header)
	{
		return static_cast<CircularBuffer<D, H>*>(theCircularBuffer_)->read(buffer, header, processorUID_);
	}

	// Fast version where you point to the buffer without copying
	template<class D, class H>
	int read(D*& buffer, H*& header)
	{
		return static_cast<CircularBuffer<D, H>*>(theCircularBuffer_)->read(buffer, header, processorUID_);
	}

	template<class D, class H>
	int setReadSubBuffer(void)
	{
		return static_cast<CircularBuffer<D, H>*>(theCircularBuffer_)
		    ->getLastReadBuffer(DataProcessor::processorUID_)
		    .setReadSubBuffer(DataProcessor::processorUID_);
	}

	template<class D, class H>
	int read(D& buffer)
	{
		return static_cast<CircularBuffer<D, H>*>(theCircularBuffer_)->read(buffer, processorUID_);
	}

	ConsumerPriority getPriority(void);

  private:
	ConsumerPriority priority_;
};

}  // namespace ots

#endif
