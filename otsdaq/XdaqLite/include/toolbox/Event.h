// OTS_LITE stand-in for xdaq's toolbox/Event.h
#ifndef _toolbox_Event_h_
#define _toolbox_Event_h_

#include <memory>
#include <string>

namespace toolbox
{
class Event
{
  public:
	typedef std::shared_ptr<Event> Reference;

	Event(const std::string& type, void* originator = nullptr) : type_(type), originator_(originator) {}
	virtual ~Event() {}

	std::string type() const { return type_; }
	void*       originator() const { return originator_; }

  private:
	std::string type_;
	void*       originator_;
};
}  // namespace toolbox

#endif
