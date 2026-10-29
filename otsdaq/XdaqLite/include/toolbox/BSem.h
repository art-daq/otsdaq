// OTS_LITE stand-in for xdaq's toolbox/BSem.h (binary semaphore)
#ifndef _toolbox_BSem_h_
#define _toolbox_BSem_h_

#include <condition_variable>
#include <mutex>

namespace toolbox
{
class BSem
{
  public:
	enum State
	{
		EMPTY = 0,
		FULL  = 1
	};
	explicit BSem(State initial = EMPTY, bool /*recursive*/ = false)
	    : available_(initial == FULL) {}

	void take()
	{
		std::unique_lock<std::mutex> lk(mutex_);
		cv_.wait(lk, [this] { return available_; });
		available_ = false;
	}
	void give()
	{
		{
			std::lock_guard<std::mutex> lk(mutex_);
			available_ = true;
		}
		cv_.notify_one();
	}

  private:
	std::mutex              mutex_;
	std::condition_variable cv_;
	bool                    available_;
};
}  // namespace toolbox

#endif
