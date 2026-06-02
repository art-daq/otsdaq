#ifndef OTSDAQ_ARTDAQSUPERVISOR_ARTDAQSUPERVISORTRACECONTROLLER_H
#define OTSDAQ_ARTDAQSUPERVISOR_ARTDAQSUPERVISORTRACECONTROLLER_H

#include "otsdaq/ARTDAQSupervisor/ARTDAQSupervisor.hh"
#include "otsdaq/MessageFacility/ITRACEController.h"

namespace ots
{
class ARTDAQSupervisorTRACEController : public ITRACEController
{
  public:
	ARTDAQSupervisorTRACEController();
	virtual ~ARTDAQSupervisorTRACEController() { theSupervisor_ = nullptr; }

	const ITRACEController::HostTraceLevelMap& getTraceLevels(void) final;
	virtual void                               setTraceLevelMask(std::string const& label,
	                                                             TraceMasks const&  lvl,
	                                                             std::string const& hostname = "localhost",
	                                                             std::string const& mode     = "ALL") final;

	void setSupervisorPtr(ARTDAQSupervisor* ptr) { theSupervisor_ = ptr; }

	/// These functions are defaulted because ARTDAQSupervisorTRACEController doesn't have direct access to the ARTDAQ TRACE Buffers
	virtual bool getIsTriggered(void) { return false; }
	virtual void setTriggerEnable(size_t) {}

	virtual void resetTraceBuffer(void) {}
	virtual void enableTrace(bool enable = true)
	{
		if(enable)
			std::cout << "using enable" << std::endl;
	}  // FIXME

  private:
	ARTDAQSupervisor* theSupervisor_ = nullptr;

	// Cache from the last setTraceLevelMask() call — includes the updated
	// host's full level table (the leaf dumps levels after set). getTraceLevels()
	// uses this instead of a full ots -tt readback when fresh.
	HostTraceLevelMap lastSetLevels_;
	time_t            lastSetTime_ = 0;
};
}  // namespace ots

#endif  // OTSDAQ_ARTDAQSUPERVISOR_ARTDAQSUPERVISORTRACECONTROLLER_H
