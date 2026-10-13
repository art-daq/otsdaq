#ifndef OTSDAQ_OTSDAQ_DATAMANAGER_DATAPRODUCER_H_
#define OTSDAQ_OTSDAQ_DATAMANAGER_DATAPRODUCER_H_

#include "otsdaq/DataManager/DataProducerBase.h"
#include "otsdaq/WorkLoopManager/WorkLoop.h"

namespace ots
{
/// DataProducer
///	This class provides base class functionality for Data Producer plugin classes to
///	receive incoming streaming data and places it in a Buffer.
class DataProducer : public DataProducerBase, public virtual WorkLoop
{
  public:
	DataProducer(std::string  supervisorApplicationUID,
	             std::string  bufferUID,
	             std::string  processorUID,
	             unsigned int bufferSize = 100);
	virtual ~DataProducer(void);

	virtual void configure(void) { ; }
	virtual void startProcessingData(std::string runNumber);
	virtual void stopProcessingData(void);
};

}  // namespace ots

#endif  // OTSDAQ_OTSDAQ_DATAMANAGER_DATAPRODUCER_H_
