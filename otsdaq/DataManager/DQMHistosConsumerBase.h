#ifndef OTSDAQ_OTSDAQ_DATAMANAGER_DQMHISTOSCONSUMERBASE_H_
#define OTSDAQ_OTSDAQ_DATAMANAGER_DQMHISTOSCONSUMERBASE_H_

#include <mutex>
#include <string>
#include "otsdaq/DataManager/DataConsumer.h"
#include "otsdaq/RootUtilities/DQMHistosBase.h"

namespace ots
{
class DQMHistosConsumerBase : public DQMHistosBase, public DataConsumer
{
  public:
	DQMHistosConsumerBase(std::string supervisorApplicationUID,
	                      std::string bufferUID,
	                      std::string processorUID,
	                      ConsumerPriority /*priority*/)
	    : WorkLoop(processorUID)
	    , DataConsumer(
	          supervisorApplicationUID, bufferUID, processorUID, LowConsumerPriority)
	{
		;
	}
	virtual ~DQMHistosConsumerBase(void) { ; }
	std::mutex& getFillHistoMutex(void) { return fillHistoMutex_; }

  protected:
	std::mutex fillHistoMutex_;
};
}  // namespace ots

#endif  // OTSDAQ_OTSDAQ_DATAMANAGER_DQMHISTOSCONSUMERBASE_H_
