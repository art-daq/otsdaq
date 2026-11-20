#ifndef _ots_FEDataManagerSupervisor_h_
#define _ots_FEDataManagerSupervisor_h_

#include "otsdaq/CoreSupervisors/FESupervisor.h"

// #include "otsdaq/DataManager/DataProducerBase.h"

namespace ots
{
class DataManager;

class FEDataManagerSupervisor : public FESupervisor
{
  public:
	XDAQ_INSTANTIATOR();

	/// If derivedClassAddsDataManager is true, the derived class constructor must add
	///	its own Data Manager to theStateMachineImplementation_ and then call extractDataManager()
	FEDataManagerSupervisor(xdaq::ApplicationStub* s,
	                        bool derivedClassAddsDataManager = false);
	virtual ~FEDataManagerSupervisor(void);

	virtual void transitionConfiguring(toolbox::Event::Reference e) override;
	virtual void transitionStarting(toolbox::Event::Reference e) override;
	virtual void transitionResuming(toolbox::Event::Reference e) override;

  protected:
	DataManager* extractDataManager();  ///< used in constructor (also of derived classes)

	DataManager* theDataManager_;
};

}  // namespace ots

#endif
