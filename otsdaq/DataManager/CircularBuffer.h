#ifndef _ots_CircularBuffer_h_
#define _ots_CircularBuffer_h_

#include "otsdaq/DataManager/BufferImplementation.h"
#include "otsdaq/DataManager/CircularBufferBase.h"

#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <atomic>
#include <iostream>
#include <map>
#include <string>

namespace ots
{
template<class D, class H>
class CircularBuffer : public CircularBufferBase
{
  public:
	CircularBuffer(const std::string& dataBufferId);
	virtual ~CircularBuffer(void);

	void         reset(void);  // This DOES NOT reset the consumer list
	void         resetConsumerList(void);
	bool         isEmpty(void) const;
	unsigned int getTotalNumberOfSubBuffers(void) const;
	unsigned int getProducerBufferSize(const std::string& producerID) const;

	inline int read(D& buffer, const std::string& consumerID)
	{
		H dummyHeader;
		return read(buffer, dummyHeader, consumerID);
	}

	inline int read(D& buffer, H& header, const std::string& consumerID)
	{
		setNextProducerBuffer(consumerID);
		unsigned int readCounter = theBuffer_.size() - 1;
		//		++megaCounter_;
		//		if(megaCounter_%10000000 == 0)
		//			std::cout << __COUT_HDR_FL__ << __COUT_HDR_FL__
		//			<< "Consumer: " << consumerID
		//			<< " Reading producer: " << lastReadBuffer_[consumerID]->first
		//			<< " Buffer empty? " << lastReadBuffer_[consumerID]->second.isEmpty()
		//			<< " written buffers: " <<
		// lastReadBuffer_[consumerID]->second.numberOfWrittenBuffers()
		//			<< std::endl;
		int readReturnVal;
		while((readReturnVal = lastReadBuffer_[consumerID]->second.read(buffer, header, consumerID)) < 0 && readCounter > 0)
		{
			setNextProducerBuffer(consumerID);
			--readCounter;
		}
		return readReturnVal;
	}

	int read(D*& buffer, H*& header, const std::string& consumerID)
	{
		setNextProducerBuffer(consumerID);
		unsigned int readCounter = theBuffer_.size() - 1;
		//		++megaCounter_;
		//		if(megaCounter_%10000000 == 0)
		//			std::cout << __COUT_HDR_FL__ << __COUT_HDR_FL__
		//			<< "Consumer: " << consumerID
		//			<< " Reading producer: " << lastReadBuffer_[consumerID]->first
		//			<< " Buffer empty? " << lastReadBuffer_[consumerID]->second.isEmpty()
		//			<< " written buffers: " <<
		// lastReadBuffer_[consumerID]->second.numberOfWrittenBuffers()
		//			<< std::endl;
		int readReturnVal;
		while((readReturnVal = lastReadBuffer_[consumerID]->second.read(buffer, header, consumerID)) < 0 && readCounter > 0)
		{
			setNextProducerBuffer(consumerID);
			--readCounter;
		}
		return readReturnVal;
	}

	BufferImplementation<D, H>& getLastReadBuffer(const std::string& consumerID) { return lastReadBuffer_[consumerID]->second; }
	BufferImplementation<D, H>& getBuffer(const std::string& producerID)
	{
		// __COUTV__(producerID);
		// __COUTV__(int(theBuffer_.find(producerID) == theBuffer_.end()));
		return theBuffer_[producerID];
	}

	// void unregisterConsumer   (const std::string& consumerID);
	// void unregisterProducer   (const std::string& producerID);

  private:
	std::map<std::string /*producer id*/, BufferImplementation<D, H> /*one producer, many consumers*/> theBuffer_;

	void registerProducer(const std::string& producerID, unsigned int numberOfSubBuffers = 100);
	void registerConsumer(const std::string& consumerID, CircularBufferBase::ConsumerPriority priority);

	void setNextProducerBuffer(const std::string& consumer);

	std::map<std::string /*consumer id*/,
	         /*iterator within producer map*/ typename std::map<std::string, BufferImplementation<D, H>>::iterator>
	    lastReadBuffer_;

	std::map<std::string, CircularBufferBase::ConsumerPriority> consumers_;
};
#include "otsdaq/DataManager/CircularBuffer.icc"

}  // namespace ots
#endif
