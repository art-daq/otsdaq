// OTS_LITE stand-in for xdaq's toolbox/fsm/FailedEvent.h
#ifndef _toolbox_fsm_FailedEvent_h_
#define _toolbox_fsm_FailedEvent_h_

#include "toolbox/Event.h"
#include "toolbox/fsm/exception/Exception.h"

namespace toolbox
{
namespace fsm
{
typedef char State;

class FailedEvent : public toolbox::Event
{
  public:
	FailedEvent(xcept::Exception& e, State from, State to, void* originator = nullptr)
	    : toolbox::Event("Fail", originator), exception_(e), from_(from), to_(to)
	{
	}
	xcept::Exception& getException() { return exception_; }
	State             getFromState() const { return from_; }
	State             getToState() const { return to_; }

  private:
	xcept::Exception exception_;
	State            from_, to_;
};
}  // namespace fsm
}  // namespace toolbox

#endif
