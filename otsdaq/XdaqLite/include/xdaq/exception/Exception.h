// OTS_LITE stand-in for xdaq's xdaq/exception/Exception.h
#ifndef _xdaq_exception_Exception_h_
#define _xdaq_exception_Exception_h_

#include "xcept/Exception.h"

namespace xdaq
{
namespace exception
{
class Exception : public xcept::Exception
{
  public:
	using xcept::Exception::Exception;
};
}  // namespace exception
}  // namespace xdaq

#endif
