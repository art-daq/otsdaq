// OTS_LITE stand-in for GNU cgicc's HTTPResponseHeader (only the surface otsdaq uses, plus
// what the lite HTTP front end needs to emit a response).
#ifndef _cgicc_HTTPResponseHeader_h_
#define _cgicc_HTTPResponseHeader_h_

#include <string>
#include <vector>

#include "cgicc/HTTPCookie.h"

namespace cgicc
{
class HTTPResponseHeader
{
  public:
	HTTPResponseHeader(const std::string& version = "HTTP/1.1", int status = 200, const std::string& reason = "OK") : version_(version), status_(status), reason_(reason) {}

	HTTPResponseHeader& addHeader(const std::string& name, const std::string& value)
	{
		headers_.emplace_back(name, value);
		return *this;
	}
	HTTPResponseHeader& setCookie(const HTTPCookie& c)
	{
		cookies_.push_back(c);
		return *this;
	}
	HTTPResponseHeader& setStatus(int status, const std::string& reason)
	{
		status_ = status;
		reason_ = reason;
		return *this;
	}
	int                                                     getStatusCode() const { return status_; }
	std::string                                             getReasonPhrase() const { return reason_; }
	std::string                                             getHTTPVersion() const { return version_; }
	const std::vector<std::pair<std::string, std::string>>& getHeaders() const { return headers_; }
	const std::vector<HTTPCookie>&                          getCookies() const { return cookies_; }

  private:
	std::string                                      version_;
	int                                              status_;
	std::string                                      reason_;
	std::vector<std::pair<std::string, std::string>> headers_;
	std::vector<HTTPCookie>                          cookies_;
};
}  // namespace cgicc

#endif
