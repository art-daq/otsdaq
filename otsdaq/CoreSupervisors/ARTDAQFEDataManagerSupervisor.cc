#include "otsdaq/CoreSupervisors/ARTDAQFEDataManagerSupervisor.h"

#include "otsdaq/ARTDAQDataManager/ARTDAQDataManager.h"
#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/DataManager/DataManagerSingleton.h"
#include "otsdaq/FECore/FEVInterfacesManager.h"

using namespace ots;

XDAQ_INSTANTIATOR_IMPL(ARTDAQFEDataManagerSupervisor)

//==============================================================================
// The intention is that the artdaq Board Reader would be a consumer
//	extracting data that the front-end places in the buffer.
ARTDAQFEDataManagerSupervisor::ARTDAQFEDataManagerSupervisor(xdaq::ApplicationStub* s) : FEDataManagerSupervisor(s, true /*artdaq Data Manager*/)
{
	//	__SUP_COUT__ << "Constructor." << std::endl;
	//
	//	//WARNING THE ORDER IS IMPORTANT SINCE THE FIRST STATE MACHINE ELEMENT
	//	//	WILL BE CALLED FIRST DURING TRANSITION!!!!!
	//
	//	//the FE Interfaces Manager is added first, and then the Data Manager
	//	//	So on FSM transitions, front-ends will transition first.
	//
	//	//FEVInterfacesManager gets added in FESupervisor constructor
	//	__SUP_COUTV__(CoreSupervisorBase::theStateMachineImplementation_.size());
	//
	////
	////	CoreSupervisorBase::theStateMachineImplementation_.push_back(
	////			new FEVInterfacesManager(
	////					CorePropertySupervisorBase::getContextTreeNode(),
	////					CorePropertySupervisorBase::supervisorConfigurationPath_
	////			)
	////	);
	//	__SUP_COUT__ << "Adding Data Manager now...!" << __E__;
	//	CoreSupervisorBase::theStateMachineImplementation_.push_back(
	//			DataManagerSingleton::getInstance<ARTDAQDataManager>(
	//					CorePropertySupervisorBase::getContextTreeNode(),
	//					CorePropertySupervisorBase::supervisorConfigurationPath_,
	//					CorePropertySupervisorBase::supervisorApplicationUID_
	//			)
	//	);
	//
	//	extractDataManager();

	__SUP_COUT__ << "Constructed." << __E__;
}  // end constructor()

//==============================================================================
ARTDAQFEDataManagerSupervisor::~ARTDAQFEDataManagerSupervisor(void)
{
	//	__SUP_COUT__ << "Destroying..." << std::endl;
	//
	//	//theStateMachineImplementation_ is reset and the object it points to deleted in
	//~CoreSupervisorBase()
	//	//This destructor must happen before the CoreSupervisor destructor
	//
	//	DataManagerSingleton::deleteInstance(CoreSupervisorBase::supervisorApplicationUID_);
	//	theStateMachineImplementation_.pop_back();

	__SUP_COUT__ << "Destructed." << __E__;
}  // end destructor()
