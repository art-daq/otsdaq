#ifndef _ots_FESlowControlsWorkLoop_h_
#define _ots_FESlowControlsWorkLoop_h_

#include "otsdaq/WorkLoopManager/WorkLoop.h"

#include <iostream>
#include <string>

namespace ots
{
class FEVInterface;

class FESlowControlsWorkLoop : public WorkLoop
{
  public:
	FESlowControlsWorkLoop(const std::string& name, FEVInterface* interface) : WorkLoop(name), interface_(interface) {}
	~FESlowControlsWorkLoop() { ; }  // do not own interface_, so do not delete

	bool workLoopThread(toolbox::task::WorkLoop* workLoop);

	bool getContinueWorkLoop() { return continueWorkLoop_; }

  private:
	FEVInterface* interface_;
};

}  // namespace ots

#endif
