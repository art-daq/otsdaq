#ifndef _ots_RawDataSaverConsumerBase_h_
#define _ots_RawDataSaverConsumerBase_h_

#include "otsdaq/Configurable/Configurable.h"
#include "otsdaq/DataManager/DataConsumer.h"

#include <fstream>
#include <string>

namespace ots
{
class RawDataSaverConsumerBase : public DataConsumer, public Configurable
{
  public:
	RawDataSaverConsumerBase(std::string              supervisorApplicationUID,
	                         std::string              bufferUID,
	                         std::string              processorUID,
	                         const ConfigurationTree& theXDAQContextConfigTree,
	                         const std::string&       configurationPath);
	virtual ~RawDataSaverConsumerBase(void);

  protected:
	virtual void openFile(std::string runNumber);
	virtual void closeFile(void);
	virtual void save(const std::string& data);
	virtual void writeHeader(void) { ; }
	virtual void writeFooter(void) { ; }
	virtual void writePacketHeader(const std::string& /*data*/)
	{  // unsigned char quadWordsCount = (data.length()-2)/8; outFile_.write(
	   // (char*)&quadWordsCount, 1);
		;
	}
	virtual void writePacketFooter(const std::string& /*data*/) { ; }
	virtual void startProcessingData(std::string runNumber) override;
	virtual void stopProcessingData(void) override;
	virtual bool workLoopThread(toolbox::task::WorkLoop* workLoop) override;
	virtual void fastRead(void);
	virtual void slowRead(void);

	std::ofstream outFile_;
	// For fast read
	std::string*                        dataP_;
	std::map<std::string, std::string>* headerP_;
	// For slow read
	std::string                        data_;
	std::map<std::string, std::string> header_;

	std::string  filePath_;
	std::string  fileRadix_;
	long         maxFileSize_;
	std::string  currentRunNumber_;
	unsigned int currentSubRunNumber_;
};

}  // namespace ots

#endif
