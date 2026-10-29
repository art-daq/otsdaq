// OTS_LITE stand-in for GNU cgicc's CgiInput: the source a Cgicc object parses a request from.
#ifndef _cgicc_CgiInput_h_
#define _cgicc_CgiInput_h_

#include <string>

#include "cgicc/CgiEnvironment.h"

namespace cgicc
{
class CgiInput
{
  public:
	virtual ~CgiInput() {}
	virtual const CgiEnvironment& getEnvironment() const = 0;
	virtual std::string           getQueryString() const = 0;
	virtual std::string           getPostData() const    = 0;
};
}  // namespace cgicc

#endif
