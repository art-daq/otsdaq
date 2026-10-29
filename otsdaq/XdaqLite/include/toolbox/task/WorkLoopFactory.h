// OTS_LITE stand-in for xdaq's toolbox/task/WorkLoopFactory.h
#ifndef _toolbox_task_WorkLoopFactory_h_
#define _toolbox_task_WorkLoopFactory_h_

#include <map>
#include <mutex>
#include <string>

#include "toolbox/task/WorkLoop.h"

namespace toolbox
{
namespace task
{
class WorkLoopFactory
{
  public:
	WorkLoop* getWorkLoop(const std::string& name, const std::string& type)
	{
		std::lock_guard<std::mutex> lk(mutex_);
		std::string                 key = name + "/" + type;
		auto                        it  = loops_.find(key);
		if(it == loops_.end())
			it = loops_.emplace(key, new WorkLoop(name, type)).first;
		return it->second;
	}
	void removeWorkLoop(const std::string& name, const std::string& type)
	{
		std::lock_guard<std::mutex> lk(mutex_);
		auto                        it = loops_.find(name + "/" + type);
		if(it == loops_.end())
			XCEPT_RAISE(xcept::Exception, "WorkLoop '" + name + "' not found");
		delete it->second;
		loops_.erase(it);
	}

  private:
	std::mutex                        mutex_;
	std::map<std::string, WorkLoop*>  loops_;
};

WorkLoopFactory* getWorkLoopFactory();
}  // namespace task
}  // namespace toolbox

#endif
