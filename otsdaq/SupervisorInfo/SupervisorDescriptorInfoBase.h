#ifndef _ots_SupervisorTableBase_h_
#define _ots_SupervisorTableBase_h_

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#if __GNUC__ >= 8
#pragma GCC diagnostic ignored "-Wcatch-value"
#endif
#if __GNUC__ >= 9
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#endif
#include <xdaq/Application.h>
#pragma GCC diagnostic pop
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/XDAQApplicationMacros.h"

#include <map>
#include <string>

namespace ots
{
// key is the crate number
typedef std::map<xdata::UnsignedIntegerT, XDAQ_CONST_CALL xdaq::ApplicationDescriptor*> SupervisorDescriptors;

class SupervisorDescriptorInfoBase
{
	friend class SupervisorInfo;  //"Friend" class needs access to private members
  public:
	SupervisorDescriptorInfoBase(void);
	SupervisorDescriptorInfoBase(xdaq::ApplicationContext* applicationContext);
	virtual ~SupervisorDescriptorInfoBase(void);

  protected:
	virtual void init(xdaq::ApplicationContext* applicationContext);
	virtual void destroy();

	const SupervisorDescriptors& getAllDescriptors(void) const;

  protected:
	SupervisorDescriptors allSupervisors_;
};
}  // namespace ots
#endif
