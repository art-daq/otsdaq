#include "otsdaq/SupervisorInfo/SupervisorDescriptorInfoBase.h"

#include <xdaq/ContextDescriptor.h>
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <cassert>

using namespace ots;

//==============================================================================
SupervisorDescriptorInfoBase::SupervisorDescriptorInfoBase(void) {}

//==============================================================================
SupervisorDescriptorInfoBase::SupervisorDescriptorInfoBase(xdaq::ApplicationContext* applicationContext) { init(applicationContext); }

//==============================================================================
SupervisorDescriptorInfoBase::~SupervisorDescriptorInfoBase() {}

//==============================================================================
void SupervisorDescriptorInfoBase::destroy() { allSupervisors_.clear(); }

//==============================================================================
void SupervisorDescriptorInfoBase::init(xdaq::ApplicationContext* applicationContext)
{
	if(applicationContext->getDefaultZone()->getApplicationGroup("daq") == 0)
	{
		__SS__ << "Could not find xdaq application group \"daq\" (Must not be present in "
		          "the xdaq context configuration)"
		       << __E__;
		__SS_THROW__;
	}

	__COUT__ << "Init" << __E__;

	//	//There is only one and only Supervisor! (Or Config Wizard!!)
	//	theSupervisor_ =
	//*(applicationContext->getDefaultZone()->getApplicationGroup("daq")->getApplicationDescriptors("ots::Supervisor").begin());
	//	if(0)//theSupervisor_ == 0)
	//		__COUT__ << "Note: Could not find xdaq application descriptor
	//\"ots::Supervisor\" (Must not be present in the xdaq context configuration)" <<
	//__E__;
	//
	//	theWizard_ =
	//*(applicationContext->getDefaultZone()->getApplicationGroup("daq")->getApplicationDescriptors("ots::OtsConfigurationWizardSupervisor").begin());
	//	if(0)//theWizard_ == 0)
	//		__COUT__ << "Note: Could not find xdaq application descriptor
	//\"ots::OtsConfigurationWizardSupervisor\" (Must not be present in the xdaq context
	// configuration)" << __E__;
	//
	//	if(theWizard_ == 0 && theSupervisor_ == 0)
	//	{
	//		__SS__ << "Must have THE ots::Supervisor (or THE
	// ots::OtsConfigurationWizardSupervisor) as part of the context configuration!" <<
	//__E__;
	//		__COUT_ERR__ << "\n" << ss.str();
	//		__SS_THROW__;
	//	}

	std::set<XDAQ_CONST_CALL xdaq::ApplicationDescriptor*> appDescriptors;

	// get allSupervisors_

	// allFETypeSupervisors_.clear();
	allSupervisors_.clear();
	appDescriptors = applicationContext->getDefaultZone()->getApplicationGroup("daq")->getApplicationDescriptors();
	for(auto& it : appDescriptors)
	{
		auto /*<it,bool*/ retPair =
		    allSupervisors_.emplace(std::pair<xdata::UnsignedIntegerT, XDAQ_CONST_CALL xdaq::ApplicationDescriptor*>(it->getLocalId(), it));
		if(!retPair.second)
		{
			__SS__ << "Error! Duplicate Application IDs are not allowed. ID =" << it->getLocalId() << __E__;
			__SS_THROW__;
		}
	}
}

//==============================================================================
const SupervisorDescriptors& SupervisorDescriptorInfoBase::getAllDescriptors(void) const { return allSupervisors_; }
