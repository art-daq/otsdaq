#ifndef _ots_ARTDAQFEDataManagerSupervisor_h_
#define _ots_ARTDAQFEDataManagerSupervisor_h_

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

#endif
