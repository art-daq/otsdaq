#ifndef _ots_RawDataSaverConsumer_h_
#define _ots_RawDataSaverConsumer_h_

// This is the basic example of a raw data saver plugin.
// It can be used as is and it will save the data taken from a buffer in binary format
// without adding anything extra.  If you are planning on specializing any methods you
// should inherit from RawDataSaverConsumerBase the same way this consumer is doing.

#include "otsdaq/DataManager/RawDataSaverConsumerBase.h"

namespace ots
{
class RawDataSaverConsumer : public RawDataSaverConsumerBase
{
  public:
	RawDataSaverConsumer(std::string              supervisorApplicationUID,
	                     std::string              bufferUID,
	                     std::string              processorUID,
	                     const ConfigurationTree& theXDAQContextConfigTree,
	                     const std::string&       configurationPath);
	virtual ~RawDataSaverConsumer(void);
};

}  // namespace ots

#endif
