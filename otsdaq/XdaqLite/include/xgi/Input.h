// OTS_LITE stand-in for xdaq's xgi/Input.h: an HTTP request as seen by a handler.
// In real xdaq, xgi::Input is a cgicc::CgiInput, so handlers do `cgicc::Cgicc cgi(in)`.
// The lite front end fills the environment, query string, and post data, and Cgicc parses them.
#ifndef _xgi_Input_h_
#define _xgi_Input_h_

#include <map>
#include <string>

#include "cgicc/CgiEnvironment.h"
#include "cgicc/CgiInput.h"

namespace xgi
{
class Input : public cgicc::CgiInput
{
  public:
	Input() {}
	Input(const cgicc::CgiEnvironment& env, const std::string& queryString, const std::string& postData) : env_(env), query_(queryString), body_(postData) {}

	const cgicc::CgiEnvironment& getEnvironment() const override { return env_; }
	std::string                  getQueryString() const override { return query_; }
	std::string                  getPostData() const override { return body_; }

	// convenience accessors used directly by some otsdaq handlers
	std::string getRequestMethod() const { return env_.getRequestMethod(); }
	std::string getURL() const { return env_.getScriptName() + env_.getPathInfo(); }
	std::string getContent() const { return body_; }

  private:
	cgicc::CgiEnvironment env_;
	std::string           query_, body_;
};
}  // namespace xgi

#endif
