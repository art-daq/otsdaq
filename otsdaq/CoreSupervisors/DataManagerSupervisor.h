#ifndef OTSDAQ_OTSDAQ_CORESUPERVISORS_DATAMANAGERSUPERVISOR_H_
#define OTSDAQ_OTSDAQ_CORESUPERVISORS_DATAMANAGERSUPERVISOR_H_

#include "otsdaq/CoreSupervisors/CoreSupervisorBase.h"

namespace ots
{
/// DataManagerSupervisor
///	This class handles a collection of Data Processor plugins. It provides
///	a mechanism for Data Processor Producers to store data in Buffers, and for
///	Data Processor Consumers to retrive data from the Buffers.
class DataManagerSupervisor : public CoreSupervisorBase
{
  public:
	XDAQ_INSTANTIATOR();

	DataManagerSupervisor(xdaq::ApplicationStub* s);
	virtual ~DataManagerSupervisor(void);

  private:
};

}  // namespace ots

#endif  // OTSDAQ_OTSDAQ_CORESUPERVISORS_DATAMANAGERSUPERVISOR_H_
