#ifndef OTSDAQ_OTSDAQ_CORESUPERVISORS_ARTDAQFEDATAMANAGERSUPERVISOR_H_
#define OTSDAQ_OTSDAQ_CORESUPERVISORS_ARTDAQFEDATAMANAGERSUPERVISOR_H_

#include "otsdaq/CoreSupervisors/FEDataManagerSupervisor.h"

namespace ots
{
class ARTDAQFEDataManagerSupervisor : public FEDataManagerSupervisor
{
  public:
	XDAQ_INSTANTIATOR();

	ARTDAQFEDataManagerSupervisor(xdaq::ApplicationStub* s);
	virtual ~ARTDAQFEDataManagerSupervisor(void);

  private:
};

}  // namespace ots

#endif  // OTSDAQ_OTSDAQ_CORESUPERVISORS_ARTDAQFEDATAMANAGERSUPERVISOR_H_
