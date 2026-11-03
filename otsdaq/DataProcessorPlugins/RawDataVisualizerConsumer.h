#ifndef _ots_RawDataVisualizerConsumer_h_
#define _ots_RawDataVisualizerConsumer_h_

#include "otsdaq/Configurable/Configurable.h"
#include "otsdaq/DataManager/DataConsumer.h"

#include <mutex>
#include <string>

namespace ots
{
class ConfigurationManager;

class RawDataVisualizerConsumer : public DataConsumer, public Configurable
{
  public:
	RawDataVisualizerConsumer(std::string              supervisorApplicationUID,
	                          std::string              bufferUID,
	                          std::string              processorUID,
	                          const ConfigurationTree& theXDAQContextConfigTree,
	                          const std::string&       configurationPath);
	virtual ~RawDataVisualizerConsumer(void);

	void startProcessingData(std::string runNumber) override;
	void stopProcessingData(void) override;

	// Returns a copy: data_ is overwritten by the workloop thread while HTTP
	// request threads read it, so a reference would be a data race.
	std::string getLastRawDataBuffer(void)
	{
		std::lock_guard<std::mutex> lock(dataMutex_);
		return data_;
	}
	// Receive time (ms since epoch) and running count of packets read from the buffer
	void getLastRawDataInfo(uint64_t& packetTimeMs, uint64_t& packetCount)
	{
		std::lock_guard<std::mutex> lock(dataMutex_);
		packetTimeMs = lastPacketTimeMs_;
		packetCount  = packetCount_;
	}

  private:
	bool workLoopThread(toolbox::task::WorkLoop* workLoop) override;
	void fastRead(void);
	void slowRead(void);

	/// For fast read
	std::string*                        dataP_;
	std::map<std::string, std::string>* headerP_;
	/// For slow read
	std::string                        data_;
	std::map<std::string, std::string> header_;
	std::mutex                         dataMutex_;
	uint64_t                           lastPacketTimeMs_ = 0;
	uint64_t                           packetCount_      = 0;
};
}  // namespace ots

#endif
