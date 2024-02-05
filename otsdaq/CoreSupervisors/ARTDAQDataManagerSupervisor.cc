#include "otsdaq/CoreSupervisors/ARTDAQDataManagerSupervisor.h"

#include "../ARTDAQDataManager/ARTDAQDataManager.h"
#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/DataManager/DataManagerSingleton.h"
#include "otsdaq/FECore/FEVInterfacesManager.h"

using namespace ots;

XDAQ_INSTANTIATOR_IMPL(ARTDAQDataManagerSupervisor)

//==============================================================================
ARTDAQDataManagerSupervisor::ARTDAQDataManagerSupervisor(xdaq::ApplicationStub* s) : CoreSupervisorBase(s)
{
	__SUP_COUT__ << "Constructor." << __E__;

	CoreSupervisorBase::theStateMachineImplementation_.push_back(
	    DataManagerSingleton::getInstance<ARTDAQDataManager>(CorePropertySupervisorBase::getContextTreeNode(),
	                                                         CorePropertySupervisorBase::getSupervisorConfigurationPath(),
	                                                         CorePropertySupervisorBase::getSupervisorUID()));

	__SUP_COUT__ << "Constructed." << __E__;
}  // end constructor()

//==============================================================================
ARTDAQDataManagerSupervisor::~ARTDAQDataManagerSupervisor(void)
{
	__SUP_COUT__ << "Destructor." << __E__;

	DataManagerSingleton::deleteInstance(CorePropertySupervisorBase::getSupervisorUID());
	theStateMachineImplementation_.pop_back();

	__SUP_COUT__ << "Destructed." << __E__;
}  // end destructor()
