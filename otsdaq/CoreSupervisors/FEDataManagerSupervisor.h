#ifndef OTSDAQ_OTSDAQ_CORESUPERVISORS_FEDATAMANAGERSUPERVISOR_H_
#define OTSDAQ_OTSDAQ_CORESUPERVISORS_FEDATAMANAGERSUPERVISOR_H_

#include "otsdaq/CoreSupervisors/FESupervisor.h"

// #include "otsdaq/DataManager/DataProducerBase.h"

namespace ots
{
class DataManager;

class FEDataManagerSupervisor : public FESupervisor
{
  public:
	XDAQ_INSTANTIATOR();

	FEDataManagerSupervisor(xdaq::ApplicationStub* s, bool artdaqDataManager = false);
	virtual ~FEDataManagerSupervisor(void);

	virtual void transitionConfiguring(toolbox::Event::Reference e) override;
	virtual void transitionStarting(toolbox::Event::Reference e) override;
	virtual void transitionResuming(toolbox::Event::Reference e) override;

  protected:
	DataManager* theDataManager_;

  private:
	DataManager* extractDataManager();  ///< likely, just used in constructor
};

}  // namespace ots

#endif  // OTSDAQ_OTSDAQ_CORESUPERVISORS_FEDATAMANAGERSUPERVISOR_H_
