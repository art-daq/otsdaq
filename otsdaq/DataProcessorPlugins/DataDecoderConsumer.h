#ifndef OTSDAQ_OTSDAQ_DATAPROCESSORPLUGINS_DATADECODERCONSUMER_H_
#define OTSDAQ_OTSDAQ_DATAPROCESSORPLUGINS_DATADECODERCONSUMER_H_

#include "otsdaq/Configurable/Configurable.h"
#include "otsdaq/DataDecoders/DataDecoder.h"
#include "otsdaq/DataManager/DataConsumer.h"

#include <string>

namespace ots
{
class DataDecoderConsumer : public DataDecoder, public DataConsumer, public Configurable
{
  public:
	DataDecoderConsumer(std::string              supervisorApplicationUID,
	                    std::string              bufferUID,
	                    std::string              processorUID,
	                    const ConfigurationTree& theXDAQContextConfigTree,
	                    const std::string&       configurationPath);
	virtual ~DataDecoderConsumer(void);

  protected:
	bool workLoopThread(toolbox::task::WorkLoop* workLoop);
};

}  // namespace ots

#endif  // OTSDAQ_OTSDAQ_DATAPROCESSORPLUGINS_DATADECODERCONSUMER_H_
