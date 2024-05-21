#ifndef OTSDAQ_TFMSUPERVISOR_TFMSUPERVISORTRACECONTROLLER_H
#define OTSDAQ_TFMSUPERVISOR_TFMSUPERVISORTRACECONTROLLER_H

#include "otsdaq/TFMSupervisor/TFMSupervisor.hh"
#include "otsdaq/MessageFacility/ITRACEController.h"

namespace ots
{
class TFMSupervisorTRACEController : public ITRACEController
{
  public:
	TFMSupervisorTRACEController();
	virtual ~TFMSupervisorTRACEController() { theSupervisor_ = nullptr; }

	const ITRACEController::HostTraceLevelMap& getTraceLevels(void) final;
	virtual void                               setTraceLevelMask(std::string const& label,
	                                                             TraceMasks const&  lvl,
	                                                             std::string const& hostname = "localhost",
	                                                             std::string const& mode     = "ALL") final;

	void setSupervisorPtr(TFMSupervisor* ptr) { theSupervisor_ = ptr; }

	// These functions are defaulted because TFMSupervisorTRACEController doesn't have direct access to the TFM TRACE Buffers
	virtual bool getIsTriggered(void) { return false; }
	virtual void setTriggerEnable(size_t) {}

	virtual void resetTraceBuffer(void) {}
	virtual void enableTrace(bool enable = true)
	{
		if(enable)
			std::cout << "using enable" << std::endl;
	}  // FIXME

  private:
	TFMSupervisor* theSupervisor_;
};
}  // namespace ots

#endif  // OTSDAQ_TFMSUPERVISOR_TFMSUPERVISORTRACECONTROLLER_H
