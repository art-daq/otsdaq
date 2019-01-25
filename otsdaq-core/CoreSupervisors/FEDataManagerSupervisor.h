#ifndef _ots_FEDataManagerSupervisor_h_
#define _ots_FEDataManagerSupervisor_h_

#include "otsdaq-core/CoreSupervisors/FESupervisor.h"

#include "otsdaq-core/DataManager/DataProducerBase.h"

namespace ots
{
class DataManager;

class FEDataManagerSupervisor: public FESupervisor
{

public:

    XDAQ_INSTANTIATOR();

    FEDataManagerSupervisor              (xdaq::ApplicationStub * s) ;
    virtual ~FEDataManagerSupervisor     (void);


    virtual void 			transitionConfiguring 			(toolbox::Event::Reference e) override;
    virtual void 			transitionStarting 				(toolbox::Event::Reference e) override;
    virtual void 			transitionResuming 				(toolbox::Event::Reference e) override;

protected:
    DataManager*			theDataManager_;
private:
    DataManager*		   	extractDataManager(); //likely, just used in constructor

    std::map<std::string /*feId*/, std::shared_ptr<DataProducerBase> >  feProducers_; //keep map of feProducers (especially for destructor)
};

}

#endif
