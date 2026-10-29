// OTS_LITE stand-in for xdaq's toolbox/lang/Class.h (used only as a virtual base)
#ifndef _toolbox_lang_Class_h_
#define _toolbox_lang_Class_h_

#include <functional>  // toolbox::lang::Class is the lowest-level xdaq include many otsdaq headers reach; std::function is expected from it

namespace toolbox
{
namespace lang
{
class Class
{
  public:
	virtual ~Class() {}
};
}  // namespace lang
}  // namespace toolbox

#endif
