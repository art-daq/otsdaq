#include "otsdaq/CoreSupervisors/DataManagerSupervisor.h"
#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/DataManager/DataManager.h"
#include "otsdaq/DataManager/DataManagerSingleton.h"

using namespace ots;

XDAQ_INSTANTIATOR_IMPL(DataManagerSupervisor)

//==============================================================================
DataManagerSupervisor::DataManagerSupervisor(xdaq::ApplicationStub* s) : CoreSupervisorBase(s)
{
	__SUP_COUT__ << "Constructor." << __E__;

	CoreSupervisorBase::theStateMachineImplementation_.push_back(
	    DataManagerSingleton::getInstance<DataManager>(CorePropertySupervisorBase::getContextTreeNode(),
	                                                   CorePropertySupervisorBase::getSupervisorConfigurationPath(),
	                                                   CorePropertySupervisorBase::getSupervisorUID()));

	__SUP_COUT__ << "Constructed." << __E__;
}  // end constructor()

//==============================================================================
DataManagerSupervisor::~DataManagerSupervisor(void)
{
	__SUP_COUT__ << "Destroying..." << std::endl;

	DataManagerSingleton::deleteInstance(CorePropertySupervisorBase::getSupervisorUID());
	theStateMachineImplementation_.pop_back();

	__SUP_COUT__ << "Destructed." << __E__;
}  // end destructor()
