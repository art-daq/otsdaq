// OTS_LITE stand-in for xdaq's toolbox/task/WorkLoop.h: a named thread that repeatedly
// calls a bound job until the job returns false or the loop is cancelled.
#ifndef _toolbox_task_WorkLoop_h_
#define _toolbox_task_WorkLoop_h_

#include <atomic>
#include <functional>
#include <string>
#include <thread>

#include "xcept/Exception.h"

namespace toolbox
{
namespace task
{
class WorkLoop;

class ActionSignature
{
  public:
	ActionSignature(const std::string& name, std::function<bool(WorkLoop*)> f) : name_(name), f_(f) {}
	std::string name() const { return name_; }
	bool        invoke(WorkLoop* wl) { return f_(wl); }

  private:
	std::string                    name_;
	std::function<bool(WorkLoop*)> f_;
};

template<class OBJECT>
ActionSignature* bind(OBJECT* obj, bool (OBJECT::*func)(WorkLoop*), const std::string& name)
{
	return new ActionSignature(name, [obj, func](WorkLoop* wl) { return (obj->*func)(wl); });
}

class WorkLoop
{
  public:
	WorkLoop(const std::string& name, const std::string& type) : name_(name), type_(type), job_(nullptr), active_(false), running_(false) {}
	~WorkLoop() { cancel(); }

	std::string getName() const { return name_ + "/" + type_; }
	std::string getType() const { return type_; }
	bool        isActive() const { return active_; }

	void submit(ActionSignature* job)
	{
		if(job_)
			XCEPT_RAISE(xcept::Exception, "WorkLoop '" + name_ + "' already has a job submitted");
		job_ = job;
	}
	void remove(ActionSignature* job)
	{
		if(job_ != job)
			XCEPT_RAISE(xcept::Exception, "Job not found in WorkLoop '" + name_ + "'");
		job_ = nullptr;
	}
	void activate()
	{
		if(active_)
			XCEPT_RAISE(xcept::Exception, "WorkLoop '" + name_ + "' already active");
		if(!job_)
			XCEPT_RAISE(xcept::Exception, "WorkLoop '" + name_ + "' has no job");
		active_  = true;
		running_ = true;
		thread_  = std::thread([this] {
			 while(running_ && job_ && job_->invoke(this))
			 {
			 }
			 active_ = false;
		 });
	}
	// Waits for the job thread to finish, as xdaq's cancel() does.
	void cancel()
	{
		if(!active_ && !thread_.joinable())
			XCEPT_RAISE(xcept::Exception, "WorkLoop '" + name_ + "' is not active");
		running_ = false;
		if(thread_.joinable())
		{
			if(std::this_thread::get_id() == thread_.get_id())
				thread_.detach();
			else
				thread_.join();
		}
		active_ = false;
	}

  private:
	std::string       name_, type_;
	ActionSignature*  job_;
	std::atomic<bool> active_, running_;
	std::thread       thread_;
};
}  // namespace task
}  // namespace toolbox

#endif
