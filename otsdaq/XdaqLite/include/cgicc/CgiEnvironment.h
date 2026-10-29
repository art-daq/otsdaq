// OTS_LITE stand-in for GNU cgicc's CgiEnvironment
#ifndef _cgicc_CgiEnvironment_h_
#define _cgicc_CgiEnvironment_h_

#include <string>
#include <vector>

#include "cgicc/HTTPCookie.h"

namespace cgicc
{
class CgiEnvironment
{
  public:
	CgiEnvironment() {}

	std::string                    getRequestMethod() const { return requestMethod_; }
	std::string                    getQueryString() const { return queryString_; }
	std::string                    getPostData() const { return postData_; }
	std::string                    getRemoteAddr() const { return remoteAddr_; }
	std::string                    getRemoteHost() const { return remoteHost_; }
	std::string                    getServerName() const { return serverName_; }
	unsigned long                  getServerPort() const { return serverPort_; }
	std::string                    getScriptName() const { return scriptName_; }
	std::string                    getPathInfo() const { return pathInfo_; }
	std::string                    getContentType() const { return contentType_; }
	unsigned long                  getContentLength() const { return postData_.size(); }
	std::string                    getUserAgent() const { return userAgent_; }
	std::string                    getReferrer() const { return referrer_; }
	std::string                    getCookies() const { return cookies_; }
	const std::vector<HTTPCookie>& getCookieList() const { return cookieList_; }

	// setters used by the lite HTTP front end
	std::string             requestMethod_, queryString_, postData_, remoteAddr_, remoteHost_, serverName_, scriptName_, pathInfo_, contentType_, userAgent_, referrer_, cookies_;
	unsigned long           serverPort_ = 0;
	std::vector<HTTPCookie> cookieList_;
};
}  // namespace cgicc

#endif
