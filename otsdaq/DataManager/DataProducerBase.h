#ifndef _ots_DataProducerBase_h_
#define _ots_DataProducerBase_h_

#include "otsdaq/DataManager/CircularBuffer.h"
#include "otsdaq/DataManager/DataProcessor.h"
#include "otsdaq/Macros/BinaryStringMacros.h"

#include <string>

namespace ots
{
// DataProducerBase
//	This class provides base class functionality for Data Producer plugin classes to
//	receive incoming streaming data and places it in a Buffer.
class DataProducerBase : public DataProcessor
{
  public:
	DataProducerBase(const std::string& supervisorApplicationUID, const std::string& bufferUID, const std::string& processorUID, unsigned int bufferSize = 100);
	virtual ~DataProducerBase(void);

	virtual void registerToBuffer(void);
	// virtual void         unregisterFromBuffer  	(void);

	template<class D, class H>
	int attachToEmptySubBuffer(D*& data, H*& header)
	{
		return static_cast<CircularBuffer<D, H>*>(theCircularBuffer_)->getBuffer(DataProcessor::processorUID_).attachToEmptySubBuffer(data, header);
	}

	template<class D, class H>
	int setWrittenSubBuffer(void)
	{
		// __COUT__ << __E__;
		return static_cast<CircularBuffer<D, H>*>(theCircularBuffer_)->getBuffer(DataProcessor::processorUID_).setWrittenSubBuffer();
	}

	template<class D, class H>
	int write(const D& buffer)
	{
		return static_cast<CircularBuffer<D, H>*>(theCircularBuffer_)->getBuffer(DataProcessor::processorUID_).write(buffer);
	}

	template<class D, class H>
	int write(const D& buffer, const H& header)
	{
		return static_cast<CircularBuffer<D, H>*>(theCircularBuffer_)->getBuffer(DataProcessor::processorUID_).write(buffer, header);
	}

	unsigned int getBufferSize(void) const { return bufferSize_; }

  private:
	const unsigned int bufferSize_;
};

}  // namespace ots

#endif
