#ifndef _ots_SupervisorTableBase_h_
#define _ots_SupervisorTableBase_h_

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <xdaq/Application.h>
#pragma GCC diagnostic pop
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/XDAQApplicationMacros.h"

#include <map>
#include <string>

namespace ots
{
// key is the crate number
typedef std::map<xdata::UnsignedIntegerT, XDAQ_CONST_CALL xdaq::ApplicationDescriptor*>
    SupervisorDescriptors;

class SupervisorDescriptorInfoBase
{
  public:
	SupervisorDescriptorInfoBase(void);
	virtual ~SupervisorDescriptorInfoBase(void);

	virtual void init(xdaq::ApplicationContext* applicationContext);

	const SupervisorDescriptors& getDataManagerDescriptors(void) const;
	const SupervisorDescriptors& getFEDescriptors(void) const;
	const SupervisorDescriptors& getDTCDescriptors(void) const;
	const SupervisorDescriptors& getFEDataManagerDescriptors(void) const;
	// const SupervisorDescriptors& getARTDAQFEDescriptors         (void) const;
	const SupervisorDescriptors& getARTDAQFEDataManagerDescriptors(void) const;
	const SupervisorDescriptors& getARTDAQDataManagerDescriptors(void) const;
	const SupervisorDescriptors& getARTDAQBuilderDescriptors(void) const;
	const SupervisorDescriptors& getARTDAQDataLoggerDescriptors(void) const;
	const SupervisorDescriptors& getARTDAQDispatcherDescriptors(void) const;
	const SupervisorDescriptors& getVisualDescriptors(void) const;

	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* getSupervisorDescriptor(void) const;
	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* getLogbookDescriptor(void) const;
	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* getWizardDescriptor(void) const;
	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* getDataManagerDescriptor(
	    xdata::UnsignedIntegerT instance) const;
	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* getFEDescriptor(
	    xdata::UnsignedIntegerT instance) const;
	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* getDTCDescriptor(
	    xdata::UnsignedIntegerT instance) const;
	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* getFEDataManagerDescriptor(
	    xdata::UnsignedIntegerT instance) const;
	// xdaq::ApplicationDescriptor* getARTDAQFEDescriptor
	// (xdata::UnsignedIntegerT instance) const;
	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* getARTDAQFEDataManagerDescriptor(
	    xdata::UnsignedIntegerT instance) const;
	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* getARTDAQDataManagerDescriptor(
	    xdata::UnsignedIntegerT instance) const;
	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* getARTDAQBuilderDescriptor(
	    xdata::UnsignedIntegerT instance) const;
	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* getARTDAQDataLoggerDescriptor(
	    xdata::UnsignedIntegerT instance) const;
	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* getARTDAQDispatcherDescriptor(
	    xdata::UnsignedIntegerT instance) const;
	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* getVisualDescriptor(
	    xdata::UnsignedIntegerT instance) const;

	std::string getFEURL(xdata::UnsignedIntegerT instance) const;

	//    std::string getARTDAQFEURL         (xdata::UnsignedIntegerT instance) const;
	//    std::string getARTDAQBuilderURL    (xdata::UnsignedIntegerT instance) const;
	//    std::string getARTDAQDataLoggerURL (xdata::UnsignedIntegerT instance) const;
	//    std::string getARTDAQDispatcherURL (xdata::UnsignedIntegerT instance) const;

  protected:
	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* theSupervisor_;
	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* theWizard_;
	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* theLogbookSupervisor_;
	SupervisorDescriptors                        theVisualSupervisors_;
	SupervisorDescriptors                        theDataManagerSupervisors_;
	SupervisorDescriptors                        theFESupervisors_;
	SupervisorDescriptors                        theDTCSupervisors_;
	SupervisorDescriptors                        theFEDataManagerSupervisors_;
	// SupervisorDescriptors        theARTDAQFESupervisors_;
	SupervisorDescriptors theARTDAQFEDataManagerSupervisors_;
	SupervisorDescriptors theARTDAQDataManagerSupervisors_;
	SupervisorDescriptors theARTDAQBuilderSupervisors_;
	SupervisorDescriptors theARTDAQDataLoggerSupervisors_;
	SupervisorDescriptors theARTDAQDispatcherSupervisors_;
};
}  // namespace ots
#endif
