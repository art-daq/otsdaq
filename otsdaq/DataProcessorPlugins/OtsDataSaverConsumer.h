#ifndef _ots_OtsDataSaverConsumer_h_
#define _ots_OtsDataSaverConsumer_h_

#include "otsdaq/DataManager/RawDataSaverConsumerBase.h"

namespace ots
{
class OtsDataSaverConsumer : public RawDataSaverConsumerBase
{
  public:
	OtsDataSaverConsumer(std::string              supervisorApplicationUID,
	                     std::string              bufferUID,
	                     std::string              processorUID,
	                     const ConfigurationTree& theXDAQContextConfigTree,
	                     const std::string&       configurationPath);
	virtual ~OtsDataSaverConsumer(void);

	virtual void writePacketHeader(const std::string& data) override;

  protected:
	void writeHeader(void) override;

	unsigned char lastSeqId_;
};

}  // namespace ots

#endif
