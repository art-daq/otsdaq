#ifndef _ots_BufferImplementation_h_
#define _ots_BufferImplementation_h_

#include "otsdaq/DataManager/CircularBufferBase.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/StringMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <atomic>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace ots
{
template<class D, class H>
class BufferImplementation
{
	struct ConsumerStruct
	{
		ConsumerStruct() : priority_(CircularBufferBase::LowConsumerPriority), readPointer_(0), subBuffersStatus_(nullptr) {}

		CircularBufferBase::ConsumerPriority priority_;
		int                                  readPointer_;
		std::atomic_bool*                    subBuffersStatus_;  // Status of the Circular Buffer:
	};

  public:
	BufferImplementation(const std::string& producerName = "", unsigned int numberOfSubBuffers = 100);
	BufferImplementation(const BufferImplementation<D, H>& toCopy);
	BufferImplementation<D, H>& operator=(const BufferImplementation<D, H>& toCopy);
	virtual ~BufferImplementation(void);

	void init(void);
	void reset(void);
	void resetConsumerList(void);
	void registerConsumer(const std::string& name, CircularBufferBase::ConsumerPriority priority);
	// void unregisterConsumer            	(const std::string& name);
	int attachToEmptySubBuffer(D*& data, H*& header);
	int setWrittenSubBuffer(void);
	int write(const D& buffer, const H& header = H());
	int read(D& buffer, const std::string& consumer);
	int read(D& buffer, H& header, const std::string& consumer);
	int read(D*& buffer, H*& header, const std::string& consumer);
	int setReadSubBuffer(const std::string& consumer);  // Must be used in conjunction
	                                                    // with attachToEmptySubBuffer
	                                                    // because it attach to the
	                                                    // nextWritePointer buffer

	bool         isEmpty(void) const;
	unsigned int bufferSize(void) const { return numberOfSubBuffers_; }
	unsigned int numberOfWrittenBuffers(void) const;

	const std::map<std::string, ConsumerStruct>& getConsumers(void) const { return consumers_; };

	void dumpStatus(std::ostream* out = (std::ostream*)&(std::cout)) const;

  protected:
	const std::string mfSubject_;

  private:
	enum
	{
		ErrorBufferFull          = -1,
		ErrorBufferLocked        = -2,
		ErrorBufferNotAvailable  = -3,
		ErrorReadBufferOutOfSync = -4
	};

	const std::string                     producerName_;
	unsigned int                          numberOfSubBuffers_;
	std::map<std::string, ConsumerStruct> consumers_;         // Pointers to the blocks which the consumers are reading
	int                                   writePointer_;      // Pointer to the available free buffer, -1 means no free buffers!
	std::atomic_bool*                     subBuffersStatus_;  // Status of the Circular Buffer:
	std::vector<H>                        headers_;           // Buffer Header
	std::vector<D>                        subBuffers_;        // Buffers filled with data
	const bool                            bufferFree_;

	unsigned int      nextWritePointer(void);
	unsigned int      nextReadPointer(const std::string& consumer);
	int               getFreeBufferIndex(void);  // can return -1 if there are no free buffers!
	unsigned int      getReadPointer(const std::string& consumer);
	void              setWritten(unsigned int subBuffer);
	void              setFree(unsigned int subBuffer, const std::string& consumer);
	std::atomic_bool& isFree(unsigned int subBuffer) const;
	std::atomic_bool& isFree(unsigned int subBuffer, const std::string& consumer) const;

	H&   getHeader(unsigned int subBuffer);
	D&   getSubBuffer(unsigned int subBuffer);
	void writeSubBuffer(unsigned int subBuffer, const D& buffer, const H& header);
};
#include "otsdaq/DataManager/BufferImplementation.icc"

}  // namespace ots
#endif
