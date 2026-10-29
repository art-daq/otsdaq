// OTS_LITE stand-in for GNU cgicc's HTTPCookie
#ifndef _cgicc_HTTPCookie_h_
#define _cgicc_HTTPCookie_h_

#include <string>

namespace cgicc
{
class HTTPCookie
{
  public:
	HTTPCookie()
	    : maxAge_(0)
	    , secure_(false) {}
	HTTPCookie(const std::string& name, const std::string& value)
	    : name_(name)
	    , value_(value)
	    , maxAge_(0)
	    , secure_(false) {}
	std::string getName() const { return name_; }
	std::string getValue() const { return value_; }
	void        setMaxAge(unsigned long a) { maxAge_ = a; }
	void        setPath(const std::string& p) { path_ = p; }
	void        setSecure(bool s) { secure_ = s; }

  private:
	std::string   name_, value_, path_;
	unsigned long maxAge_;
	bool          secure_;
};
}  // namespace cgicc

#endif
