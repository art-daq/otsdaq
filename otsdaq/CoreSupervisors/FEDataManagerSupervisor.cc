#include "otsdaq/CoreSupervisors/FEDataManagerSupervisor.h"

#include "../ARTDAQDataManager/ARTDAQDataManager.h"
#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/DataManager/DataManager.h"
#include "otsdaq/DataManager/DataManagerSingleton.h"

// #include "otsdaq/FECore/FEProducerVInterface.h"

using namespace ots;

XDAQ_INSTANTIATOR_IMPL(FEDataManagerSupervisor)

//==============================================================================
FEDataManagerSupervisor::FEDataManagerSupervisor(xdaq::ApplicationStub* s, bool artdaqDataManager) : FESupervisor(s)
{
	__SUP_COUT__ << "Constructor." << __E__;

	// WARNING THE ORDER IS IMPORTANT SINCE THE FIRST STATE MACHINE ELEMENT
	//	WILL BE CALLED FIRST DURING TRANSITION!!!!!

	// the FE Interfaces Manager is added first, and then the Data Manager
	//	So on FSM transitions, front-ends will transition first.

	// FEVInterfacesManager gets added in FESupervisor constructor
	__SUP_COUTV__(CoreSupervisorBase::theStateMachineImplementation_.size());

	//
	//	CoreSupervisorBase::theStateMachineImplementation_.push_back(
	//			new FEVInterfacesManager(
	//					CorePropertySupervisorBase::getContextTreeNode(),
	//					CorePropertySupervisorBase::supervisorConfigurationPath_
	//			)
	//	);

	if(artdaqDataManager)
	{
		__SUP_COUT__ << "Adding ARTDAQ Data Manager now...!" << __E__;
		CoreSupervisorBase::theStateMachineImplementation_.push_back(
		    DataManagerSingleton::getInstance<ARTDAQDataManager>(CorePropertySupervisorBase::getContextTreeNode(),
		                                                         CorePropertySupervisorBase::getSupervisorConfigurationPath(),
		                                                         CorePropertySupervisorBase::getSupervisorUID()));
	}
	else
	{
		__SUP_COUT__ << "Adding Data Manager now...!" << __E__;
		CoreSupervisorBase::theStateMachineImplementation_.push_back(
		    DataManagerSingleton::getInstance<DataManager>(CorePropertySupervisorBase::getContextTreeNode(),
		                                                   CorePropertySupervisorBase::getSupervisorConfigurationPath(),
		                                                   CorePropertySupervisorBase::getSupervisorUID()));
	}

	extractDataManager();

	__SUP_COUT__ << "Constructed." << __E__;
}  // end constructor()

//==============================================================================
FEDataManagerSupervisor::~FEDataManagerSupervisor(void)
{
	__SUP_COUT__ << "Destroying..." << __E__;

	// theStateMachineImplementation_ is reset and the object it points to deleted in
	// ~CoreSupervisorBase()  This destructor must happen before the CoreSupervisor
	// destructor

	DataManagerSingleton::deleteInstance(CoreSupervisorBase::getSupervisorUID());
	theStateMachineImplementation_.pop_back();

	__SUP_COUT__ << "Destructed." << __E__;
}  // end destructor()

//==============================================================================
// transitionConfiguring
//	swap order of state machine vector for configuring
//		so that DataManager gets configured first.
void FEDataManagerSupervisor::transitionConfiguring(toolbox::Event::Reference e)
{
	__SUP_COUT__ << "transitionConfiguring" << __E__;

	theDataManager_->parentSupervisorHasFrontends_ = true;

	// Data Manager needs to be configured (instantiate buffers)
	//	before FEVinterfaceManager configures (creates interfaces)

	if(theStateMachineImplementation_.size() != 2)
	{
		__SUP_SS__ << "State machine size is not 2! It is " << theStateMachineImplementation_.size() << ". What happened??" << __E__;
		__SUP_SS_THROW__;
	}

	VStateMachine* p                  = theStateMachineImplementation_[0];
	theStateMachineImplementation_[0] = theStateMachineImplementation_[1];
	theStateMachineImplementation_[1] = p;

	CoreSupervisorBase::transitionConfiguring(e);

	{  // print buffer status
		__SUP_SS__;
		// theDataManager_->dumpStatus((std::ostream*)&ss);
		std::cout << ss.str() << __E__;
	}

	// At this point the buffers have been made with producers and consumers
	//	then the FEs (including FEProducers) were made.
	//	Producers (including FEProducers) and Consumers attach to their DataManager
	//	Buffer during their construction.

	// revert state machine order back
	theStateMachineImplementation_[1] = theStateMachineImplementation_[0];
	theStateMachineImplementation_[0] = p;

}  // end transitionConfiguring()

//==============================================================================
// transitionStarting
//	swap order of state machine vector for starting
//		so that DataManager gets configured first.
//	buffers need to be ready before FEs start writing to them
void FEDataManagerSupervisor::transitionStarting(toolbox::Event::Reference e)
{
	__SUP_COUT__ << "transitionStarting" << __E__;

	// Data Manager needs to be started (start buffers)
	//	before FEVinterfaceManager starts (interfaces write)

	if(theStateMachineImplementation_.size() != 2)
	{
		__SUP_SS__ << "State machine size is not 2! It is " << theStateMachineImplementation_.size() << ". What happened??" << __E__;
	}

	VStateMachine* p                  = theStateMachineImplementation_[0];
	theStateMachineImplementation_[0] = theStateMachineImplementation_[1];
	theStateMachineImplementation_[1] = p;

	CoreSupervisorBase::transitionStarting(e);

	// revert state machine order back
	theStateMachineImplementation_[1] = theStateMachineImplementation_[0];
	theStateMachineImplementation_[0] = p;

}  // end transitionStarting()

//==============================================================================
// transitionResuming
//	swap order of state machine vector for resuming
//		so that DataManager gets configured first.
//	buffers need to be ready before FEs start writing to them
void FEDataManagerSupervisor::transitionResuming(toolbox::Event::Reference e)
{
	__SUP_COUT__ << "transitionStarting" << __E__;

	// Data Manager needs to be resumed (resume buffers)
	//	before FEVinterfaceManager resumes (interfaces write)

	if(theStateMachineImplementation_.size() != 2)
	{
		__SUP_SS__ << "State machine size is not 2! It is " << theStateMachineImplementation_.size() << ". What happened??" << __E__;
	}

	VStateMachine* p                  = theStateMachineImplementation_[0];
	theStateMachineImplementation_[0] = theStateMachineImplementation_[1];
	theStateMachineImplementation_[1] = p;

	CoreSupervisorBase::transitionResuming(e);

	// revert state machine order back
	theStateMachineImplementation_[1] = theStateMachineImplementation_[0];
	theStateMachineImplementation_[0] = p;

}  // end transitionResuming()

//==============================================================================
// extractDataManager
//
//	locates theDataManager in state machines vector and
//		returns 0 if not found.
//
//	Note: this code assumes a CoreSupervisorBase has only one
//		DataManager in its vector of state machines
DataManager* FEDataManagerSupervisor::extractDataManager()
{
	theDataManager_ = 0;

	for(unsigned int i = 0; i < theStateMachineImplementation_.size(); ++i)
	{
		try
		{
			theDataManager_ = dynamic_cast<DataManager*>(theStateMachineImplementation_[i]);
			if(!theDataManager_)
			{
				// dynamic_cast returns null pointer on failure
				throw(std::runtime_error(""));
			}
			__SUP_COUT__ << "State Machine " << i << " WAS of type DataManager" << __E__;

			break;
		}
		catch(...)
		{
			__SUP_COUT__ << "State Machine " << i << " was NOT of type DataManager" << __E__;
		}
	}

	__SUP_COUT__ << "theDataManager pointer = " << theDataManager_ << __E__;

	return theDataManager_;
}  // end extractDataManager()
