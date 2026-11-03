#ifndef OTSDAQ_OTSDAQ_CORESUPERVISORS_ARTDAQDATAMANAGERSUPERVISOR_H_
#define OTSDAQ_OTSDAQ_CORESUPERVISORS_ARTDAQDATAMANAGERSUPERVISOR_H_

#include "otsdaq/CoreSupervisors/CoreSupervisorBase.h"

namespace ots
{
/// ARTDAQDataManagerSupervisor
///	This class handles a single artdaq Board Reader instance.
class ARTDAQDataManagerSupervisor : public CoreSupervisorBase
{
  public:
	XDAQ_INSTANTIATOR();

	ARTDAQDataManagerSupervisor(xdaq::ApplicationStub* s);
	virtual ~ARTDAQDataManagerSupervisor(void);

  private:
};

}  // namespace ots

#endif  // OTSDAQ_OTSDAQ_CORESUPERVISORS_ARTDAQDATAMANAGERSUPERVISOR_H_
