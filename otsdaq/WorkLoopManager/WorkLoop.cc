#include "otsdaq/WorkLoopManager/WorkLoop.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <toolbox/task/WorkLoopFactory.h>

#include <unistd.h>
#include <iostream>

using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "Workloop"
#undef __COUT_HDR__
#define __COUT_HDR__ (std::string("Workloop-") + WorkLoop::workLoopName_ + "\t<> ")

//==============================================================================
WorkLoop::WorkLoop(const std::string& name)
    : continueWorkLoop_(false)
    , workLoopName_(name)
    , workLoopType_("waiting")
    , workLoop_(0)
    , job_(toolbox::task::bind(this, &WorkLoop::workLoopThread, workLoopName_))
{
	__COUT__ << "Constructed." << __E__;
}

//==============================================================================
WorkLoop::~WorkLoop(void)
{
	__COUT__ << "Destructor." << __E__;
	if(stopWorkLoop())
		toolbox::task::getWorkLoopFactory()->removeWorkLoop(workLoopName_, workLoopType_);
	__COUT__ << "Destructed." << __E__;
}

//==============================================================================
void WorkLoop::startWorkLoop(void)
{
	__COUT__ << "Starting WorkLoop: " << workLoopName_ << __E__;
	continueWorkLoop_ = true;
	try
	{
		workLoop_ = toolbox::task::getWorkLoopFactory()->getWorkLoop(workLoopName_, workLoopType_);
	}
	catch(xcept::Exception& e)
	{
		__COUT__ << "ERROR: Can't create WorkLoop job for " << workLoopName_ << __E__;
		stopWorkLoop();
	}

	if(workLoop_->isActive())
		return;  // This might be a consumer producer running at the same time so it has
		         // been activated already

	try
	{
		workLoop_->submit(job_);
	}
	catch(xcept::Exception& e)
	{
		__COUT__ << "ERROR: Can't submit WorkLoop job for " << workLoopName_ << __E__;
		stopWorkLoop();
	}

	try
	{
		workLoop_->activate();
	}
	catch(xcept::Exception& e)
	{
		__COUT__ << "ERROR: Can't activate WorkLoop job for " << workLoopName_ << " Very likely because the name " << workLoopName_ << " is not unique!"
		         << __E__;
		stopWorkLoop();
	}
}  // end startWorkLoop()

//==============================================================================
bool WorkLoop::stopWorkLoop()
{
	__COUT__ << "Stopping WorkLoop: " << workLoopName_ << __E__;

	continueWorkLoop_ = false;
	if(workLoop_ == 0)
	{
		__COUT__ << "MEASSAGE: WorkLoop " << workLoopName_ << " was not created at all! This message will be commented in the future" << __E__;
		return false;
	}

	__COUT__ << "initially workLoop_->isActive() " << (workLoop_->isActive() ? "yes" : "no") << __E__;

	try
	{
		// THis method waits until the workloop job returns! Super cool!
		workLoop_->cancel();
	}
	catch(xcept::Exception& e)
	{
		__COUT__ << "WARNING: Can't cancel WorkLoop job for " << workLoopName_ << " because probably it has never been activated!" << __E__;

		__COUT__ << "workLoop_->isActive() " << (workLoop_->isActive() ? "yes" : "no") << __E__;
		return true;
	}

	try
	{
		workLoop_->remove(job_);
	}
	catch(xcept::Exception& e)
	{
		// ATTENTION!
		// If the workloop job thread returns false, then the workloop job is
		// automatically removed and it can't be removed again Leaving this try catch
		// allows me to be general in the job threads so I can return true (repeat loop)
		// or false ( loop only once) without crashing
		__COUT__ << "WARNING: Can't remove request WorkLoop: " << workLoopName_ << __E__;
		__COUT__ << "workLoop_->isActive() " << (workLoop_->isActive() ? "yes" : "no") << __E__;
	}

	__COUT__ << "Stopped WorkLoop: " << workLoopName_ << __E__;
	__COUT__ << "workLoop_->isActive() " << (workLoop_->isActive() ? "yes" : "no") << __E__;
	return true;
}  // end stopWorkLoop()

//==============================================================================
bool WorkLoop::isActive(void) const { return workLoop_->isActive() && continueWorkLoop_; }  // end isActive
