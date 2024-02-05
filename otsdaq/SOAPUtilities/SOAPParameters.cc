#include "otsdaq/SOAPUtilities/SOAPParameters.h"

#include <sstream>

using namespace ots;

//==============================================================================
SOAPParameters::SOAPParameters(void) { ; }

//==============================================================================
SOAPParameters::SOAPParameters(const std::string& name, const std::string& value) : Parameters<std::string, std::string>(name, value) { ; }

//==============================================================================
SOAPParameters::SOAPParameters(SOAPParameter parameter) : Parameters<std::string, std::string>((Parameter<std::string, std::string>)parameter) { ; }

//==============================================================================
SOAPParameters::~SOAPParameters(void) { ; }

//==============================================================================
void SOAPParameters::addParameter(const std::string& name, const std::string& value) { theParameters_[name] = value; }

//==============================================================================
void SOAPParameters::addParameter(const std::string& name, const int value)
{
	std::stringstream sValue;
	sValue << value;
	theParameters_[name] = sValue.str();
}
