#ifndef _ots_SOAPParameter_h
#define _ots_SOAPParameter_h

#include "otsdaq/SOAPUtilities/DataStructs.h"

#include <string>

namespace ots
{
class SOAPParameter : public Parameter<std::string, std::string>
{
  public:
	SOAPParameter(std::string name = "", std::string value = "") : Parameter<std::string, std::string>(name, value) { ; }
	~SOAPParameter(void) { ; }

	bool isEmpty(void)
	{
		if(name_ == "" && value_ == "")
			return true;
		return false;
	}
};
}  // namespace ots
#endif
