// OTS_LITE stand-in for xdaq's toolbox/fsm/exception/Exception.h
#ifndef _toolbox_fsm_exception_Exception_h_
#define _toolbox_fsm_exception_Exception_h_

#include "xcept/Exception.h"

namespace toolbox
{
namespace fsm
{
namespace exception
{
class Exception : public xcept::Exception
{
  public:
	using xcept::Exception::Exception;
};
}  // namespace exception
}  // namespace fsm
}  // namespace toolbox

#endif
