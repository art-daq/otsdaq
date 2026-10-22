#ifndef _ots_DataProcessor_h_
#define _ots_DataProcessor_h_

#include <atomic>
#include <cstdint>
#include <map>
#include <string>
#include "otsdaq/DataManager/CircularBuffer.h"
#include "otsdaq/DataManager/CircularBufferBase.h"
#include "otsdaq/WorkLoopManager/WorkLoop.h"

namespace ots
{
/// DataProcessor
///	This class provides common functionality for Data Producers and Consumers.
class DataProcessor
{
  public:
	DataProcessor(std::string supervisorApplicationUID,
	              std::string bufferUID,
	              std::string processorUID);
	virtual ~DataProcessor(void);

	virtual void registerToBuffer(void) = 0;
	// virtual void unregisterFromBuffer(void) = 0;

	virtual void configure(void)                            = 0;
	virtual void startProcessingData(std::string runNumber) = 0;
	virtual void stopProcessingData(void)                   = 0;
	virtual void pauseProcessingData(void) { stopProcessingData(); }
	virtual void resumeProcessingData(void) { startProcessingData(""); }

	/// Getters
	const std::string& getProcessorID(void) const { return processorUID_; }

	void setCircularBuffer(CircularBufferBase* circularBuffer);

	/// Throughput counters, monotonically increasing since construction.
	///	Plugins bump these in their work loops; monitoring code computes rates from
	///	deltas. Cheap enough to leave on permanently.
	uint64_t getStatPackets(void) const { return statPackets_.load(std::memory_order_relaxed); }
	uint64_t getStatBytes(void) const { return statBytes_.load(std::memory_order_relaxed); }
	uint64_t getStatErrors(void) const { return statErrors_.load(std::memory_order_relaxed); }
	uint64_t getStatBusyNanos(void) const { return statBusyNanos_.load(std::memory_order_relaxed); }

	/// Plugin-specific status items (e.g. socket backlog) as name/value strings.
	virtual std::map<std::string, std::string> getExtraStatus(void) const
	{
		return std::map<std::string, std::string>();
	}

  protected:
	void countPacket(uint64_t bytes)
	{
		statPackets_.fetch_add(1, std::memory_order_relaxed);
		statBytes_.fetch_add(bytes, std::memory_order_relaxed);
	}
	void countError(void) { statErrors_.fetch_add(1, std::memory_order_relaxed); }
	void countBusyNanos(uint64_t nanos)
	{
		statBusyNanos_.fetch_add(nanos, std::memory_order_relaxed);
	}

	const std::string   supervisorApplicationUID_;
	const std::string   bufferUID_;
	const std::string   processorUID_;
	CircularBufferBase* theCircularBuffer_;

  private:
	std::atomic<uint64_t> statPackets_{0};
	std::atomic<uint64_t> statBytes_{0};
	std::atomic<uint64_t> statErrors_{0};
	std::atomic<uint64_t> statBusyNanos_{0};
};

}  // namespace ots

#endif
