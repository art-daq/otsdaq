#ifndef _ots_SOAPParameters_h
#define _ots_SOAPParameters_h

#include "otsdaq/SOAPUtilities/DataStructs.h"
#include "otsdaq/SOAPUtilities/SOAPParameter.h"

#include <string>

namespace ots
{
class SOAPParameters : public Parameters<std::string, std::string>
{
  public:
	SOAPParameters(void);
	SOAPParameters(const std::string& name, const std::string& value = "");
	SOAPParameters(SOAPParameter parameter);
	~SOAPParameters(void);
	void addParameter(const std::string& name, const std::string& value = "");
	void addParameter(const std::string& name, const int value);
	int  getValueAsInt(const std::string& name) { return atoi(getValue(name).c_str()); }
};
}  // namespace ots
#endif
