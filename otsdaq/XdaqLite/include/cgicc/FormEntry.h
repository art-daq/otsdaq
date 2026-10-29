// OTS_LITE stand-in for GNU cgicc's FormEntry
#ifndef _cgicc_FormEntry_h_
#define _cgicc_FormEntry_h_

#include <cstdlib>
#include <string>

namespace cgicc
{
class FormEntry
{
  public:
	FormEntry() {}
	FormEntry(const std::string& name, const std::string& value)
	    : name_(name)
	    , value_(value) {}

	std::string getName() const { return name_; }
	std::string getValue() const { return value_; }
	std::string getStrippedValue() const { return value_; }
	long        getIntegerValue(long def = 0) const { return value_.empty() ? def : std::strtol(value_.c_str(), nullptr, 10); }
	double      getDoubleValue(double def = 0) const { return value_.empty() ? def : std::strtod(value_.c_str(), nullptr); }
	bool        isEmpty() const { return value_.empty(); }
	std::string operator*() const { return value_; }

  private:
	std::string name_, value_;
};
}  // namespace cgicc

#endif
