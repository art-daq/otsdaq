#ifndef _ots_ARTDAQDataManagerSupervisor_h_
#define _ots_ARTDAQDataManagerSupervisor_h_

#include "otsdaq/CoreSupervisors/CoreSupervisorBase.h"

namespace ots
{
// ARTDAQDataManagerSupervisor
//	This class handles a single artdaq Board Reader instance.
class ARTDAQDataManagerSupervisor : public CoreSupervisorBase
{
  public:
	XDAQ_INSTANTIATOR();

	ARTDAQDataManagerSupervisor(xdaq::ApplicationStub* s);
	virtual ~ARTDAQDataManagerSupervisor(void);

  private:
};

}  // namespace ots

#endif
