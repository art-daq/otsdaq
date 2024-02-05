#ifndef _ots_DataProcessor_h_
#define _ots_DataProcessor_h_

#include <string>
#include "otsdaq/DataManager/CircularBuffer.h"
#include "otsdaq/DataManager/CircularBufferBase.h"
#include "otsdaq/WorkLoopManager/WorkLoop.h"

namespace ots
{
// DataProcessor
//	This class provides common functionality for Data Producers and Consumers.
class DataProcessor
{
  public:
	DataProcessor(std::string supervisorApplicationUID, std::string bufferUID, std::string processorUID);
	virtual ~DataProcessor(void);

	virtual void registerToBuffer(void) = 0;
	// virtual void unregisterFromBuffer(void) = 0;

	virtual void configure(void)                            = 0;
	virtual void startProcessingData(std::string runNumber) = 0;
	virtual void stopProcessingData(void)                   = 0;
	virtual void pauseProcessingData(void) { stopProcessingData(); }
	virtual void resumeProcessingData(void) { startProcessingData(""); }

	// Getters
	const std::string& getProcessorID(void) const { return processorUID_; }

	void setCircularBuffer(CircularBufferBase* circularBuffer);

  protected:
	const std::string   supervisorApplicationUID_;
	const std::string   bufferUID_;
	const std::string   processorUID_;
	CircularBufferBase* theCircularBuffer_;
};

}  // namespace ots

#endif
