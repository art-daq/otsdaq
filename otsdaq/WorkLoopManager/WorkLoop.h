#ifndef _ots_WorkLoop_h
#define _ots_WorkLoop_h

#include <toolbox/task/WorkLoop.h>
#include <string>
#include "toolbox/lang/Class.h"

namespace ots
{
class WorkLoop : public virtual toolbox::lang::Class
{
  public:
	WorkLoop(const std::string& name);
	virtual ~WorkLoop(void);

	void startWorkLoop(void);
	bool stopWorkLoop(void);
	bool isActive(void) const;

  protected:
	volatile bool continueWorkLoop_;

	virtual bool workLoopThread(toolbox::task::WorkLoop* workLoop) = 0;

	// Getters
	const std::string& getWorkLoopName(void) const { return workLoopName_; }

  private:
	const std::string               workLoopName_;
	const std::string               workLoopType_;
	toolbox::task::WorkLoop*        workLoop_;
	toolbox::task::ActionSignature* job_;
};

}  // namespace ots

#endif
