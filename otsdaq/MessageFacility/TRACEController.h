#ifndef OTSDAQ_MESSAGEFACILITY_TRACECONTROLLER_H
#define OTSDAQ_MESSAGEFACILITY_TRACECONTROLLER_H

#include "otsdaq/MessageFacility/ITRACEController.h"

namespace ots
{
class TRACEController : public ITRACEController
{
  public:
	TRACEController() : ITRACEController(){};
	virtual ~TRACEController() = default;

	virtual const HostTraceLevelMap& getTraceLevels(void);
	virtual void setTraceLevelMask(std::string const& label, TraceMasks const& lvl, std::string const& hostname = "localhost", std::string const& mode = "ALL");

	virtual bool getIsTriggered(void);
	virtual void setTriggerEnable(size_t entriesAfterTrigger);

	virtual void resetTraceBuffer(void);
	virtual void enableTrace(bool enable = true);
};
}  // namespace ots

#endif  // OTSDAQ_MESSAGEFACILITY_TRACECONTROLLER_H
