// OTS_LITE: out-of-line parts of the xdaq stand-in (see include/ for the header-only parts).
#include "cgicc/Cgicc.h"
#include "toolbox/task/WorkLoopFactory.h"
#include "xdaq/Application.h"
#include "xdaq/ApplicationContext.h"
#include "xdaq/HandlerRegistry.h"
#include "xgi/Input.h"
#include "xgi/Output.h"

#include <netdb.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <sstream>

// ---------------------------------------------------------------------------
// xdaq::Application
// ---------------------------------------------------------------------------
std::vector<xdaq::PendingHTTPHandler>& xdaq::pendingHTTPHandlers()
{
	static std::vector<PendingHTTPHandler> v;
	return v;
}
std::vector<xdaq::PendingSOAPHandler>& xdaq::pendingSOAPHandlers()
{
	static std::vector<PendingSOAPHandler> v;
	return v;
}
std::mutex& xdaq::handlerRegistryMutex()
{
	static std::mutex m;
	return m;
}

void xdaq::Application::claimPendingHandlers()
{
	std::lock_guard<std::mutex> lk(handlerRegistryMutex());
	auto&                       http = pendingHTTPHandlers();
	for(auto it = http.begin(); it != http.end();)
	{
		if(it->resolve() == this)
		{
			httpHandlers_[it->name] = it->fn;
			it                      = http.erase(it);
		}
		else
			++it;
	}
	auto& soap = pendingSOAPHandlers();
	for(auto it = soap.begin(); it != soap.end();)
	{
		if(it->resolve() == this)
		{
			soapHandlers_[it->command] = it->fn;
			it                         = soap.erase(it);
		}
		else
			++it;
	}
}

bool xdaq::Application::dispatchHTTP(const std::string& name,
                                     xgi::Input*        in,
                                     xgi::Output*       out)
{
	claimPendingHandlers();
	auto it = httpHandlers_.find(name.empty() ? "Default" : name);
	if(it == httpHandlers_.end())
		return false;
	it->second(in, out);
	return true;
}

xoap::MessageReference xdaq::Application::dispatchSOAP(xoap::MessageReference msg)
{
	claimPendingHandlers();
	const auto& body = msg->bodyElements();
	if(body.empty())
		XCEPT_RAISE(xdaq::exception::Exception, "SOAP message has no body element");
	const std::string command = body.front().getElementName().getLocalName();
	auto              it      = soapHandlers_.find(command);
	if(it == soapHandlers_.end())
		XCEPT_RAISE(xdaq::exception::Exception,
		            "No SOAP handler bound for command '" + command + "' in " +
		                getApplicationDescriptor()->getClassName());
	return it->second(msg);
}

// ---------------------------------------------------------------------------
// xdaq::ApplicationContext
// ---------------------------------------------------------------------------
const xdaq::ApplicationDescriptor* xdaq::ApplicationContext::addApplication(
    Application*       app,
    const std::string& className,
    unsigned int       lid,
    unsigned int       instance)
{
	descriptors_.emplace_back(
	    new ApplicationDescriptor(className, lid, instance, &context_));
	zone_.group_.descriptors_.insert(descriptors_.back().get());
	localApps_.emplace_back(lid, app);
	return descriptors_.back().get();
}

const xdaq::ApplicationDescriptor* xdaq::ApplicationContext::addRemoteApplication(
    const std::string& className,
    unsigned int       lid,
    unsigned int       instance,
    const std::string& contextURL)
{
	remoteContexts_.emplace_back(new ContextDescriptor(contextURL));
	descriptors_.emplace_back(new ApplicationDescriptor(
	    className, lid, instance, remoteContexts_.back().get()));
	zone_.group_.descriptors_.insert(descriptors_.back().get());
	return descriptors_.back().get();
}

xdaq::Application* xdaq::ApplicationContext::getApplication(unsigned int lid) const
{
	for(const auto& p : localApps_)
		if(p.first == lid)
			return p.second;
	return nullptr;
}

void xdaq::ApplicationContext::setApplication(unsigned int lid, Application* app)
{
	for(auto& p : localApps_)
		if(p.first == lid)
		{
			p.second = app;
			return;
		}
	localApps_.emplace_back(lid, app);
}

namespace
{
// Remote SOAP: a minimal blocking HTTP/1.1 POST of the SOAP envelope to another context's
// /urn:xdaq-application:lid=<N>, which ots_lite_main's serveApp dispatches to the
// application. That server answers with Content-Length and Connection: close, so the
// reply is read to end-of-stream. Returns the response body; raises on failure.
std::string postSOAPToContext(const std::string& contextURL,
                              unsigned int       lid,
                              const std::string& soapBody)
{
	std::string hostPort = contextURL;
	size_t      scheme   = hostPort.find("://");
	if(scheme != std::string::npos)
		hostPort = hostPort.substr(scheme + 3);
	size_t slash = hostPort.find('/');
	if(slash != std::string::npos)
		hostPort = hostPort.substr(0, slash);
	std::string host = hostPort, port = "80";
	size_t      colon = hostPort.rfind(':');
	if(colon != std::string::npos)
	{
		host = hostPort.substr(0, colon);
		port = hostPort.substr(colon + 1);
	}

	struct addrinfo hints;
	std::memset(&hints, 0, sizeof(hints));
	hints.ai_family      = AF_UNSPEC;
	hints.ai_socktype    = SOCK_STREAM;
	struct addrinfo* res = nullptr;
	if(getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0 || !res)
		XCEPT_RAISE(xdaq::exception::Exception,
		            "OTS_LITE: cannot resolve remote context " + contextURL);

	int fd = -1;
	for(struct addrinfo* p = res; p; p = p->ai_next)
	{
		fd = ::socket(p->ai_family, p->ai_socktype, p->ai_protocol);
		if(fd < 0)
			continue;
		if(::connect(fd, p->ai_addr, p->ai_addrlen) == 0)
			break;
		::close(fd);
		fd = -1;
	}
	freeaddrinfo(res);
	if(fd < 0)
		XCEPT_RAISE(xdaq::exception::Exception,
		            "OTS_LITE: cannot connect to remote context " + contextURL +
		                " (is that context running?)");

	// FSM transitions can take a long time; wait generously for the reply
	struct timeval tv;
	tv.tv_sec  = 1200;
	tv.tv_usec = 0;
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

	std::ostringstream req;
	req << "POST /urn:xdaq-application:lid=" << lid << " HTTP/1.1\r\n"
	    << "Host: " << hostPort << "\r\n"
	    << "Content-Type: text/xml; charset=utf-8\r\n"
	    << "SOAPAction: \"\"\r\n"
	    << "Content-Length: " << soapBody.size() << "\r\n"
	    << "Connection: close\r\n\r\n"
	    << soapBody;
	const std::string request = req.str();

	size_t sent = 0;
	while(sent < request.size())
	{
		ssize_t n = ::write(fd, request.data() + sent, request.size() - sent);
		if(n <= 0)
		{
			::close(fd);
			XCEPT_RAISE(xdaq::exception::Exception,
			            "OTS_LITE: failed sending SOAP to remote context " + contextURL);
		}
		sent += n;
	}

	std::string response;
	char        buf[4096];
	while(true)
	{
		ssize_t n = ::read(fd, buf, sizeof(buf));
		if(n <= 0)
			break;
		response.append(buf, n);
	}
	::close(fd);

	size_t hdrEnd = response.find("\r\n\r\n");
	if(hdrEnd == std::string::npos || response.size() < 12)
		XCEPT_RAISE(xdaq::exception::Exception,
		            "OTS_LITE: no valid reply from remote context " + contextURL +
		                " (lid=" + std::to_string(lid) + ")");

	int status = std::atoi(response.c_str() + 9);  // "HTTP/1.1 200 OK"
	if(status != 200)
		XCEPT_RAISE(xdaq::exception::Exception,
		            "OTS_LITE: remote context " + contextURL + " returned HTTP " +
		                std::to_string(status) + " for lid=" + std::to_string(lid) +
		                ": " + response.substr(hdrEnd + 4));

	return response.substr(hdrEnd + 4);
}  // end postSOAPToContext()
}  // namespace

xoap::MessageReference xdaq::ApplicationContext::postSOAP(
    xoap::MessageReference msg,
    const ApplicationDescriptor& /*from*/,
    const ApplicationDescriptor& to)
{
	if(to.getContextDescriptor() == &context_)
	{
		if(Application* local = getApplication(to.getLocalId()))
			return local->dispatchSOAP(msg);
		auto why = unavailable_.find(to.getLocalId());
		if(why != unavailable_.end())
			XCEPT_RAISE(xdaq::exception::Exception, why->second);
		XCEPT_RAISE(xdaq::exception::Exception,
		            "OTS_LITE: SOAP target lid=" + std::to_string(to.getLocalId()) +
		                " (" + to.getClassName() +
		                ") is in this context but not constructed yet");
	}
	// the target is in another context (process): send it over HTTP
	std::string request;
	msg->writeTo(request);
	const std::string reply =
	    postSOAPToContext(to.getContextDescriptor()->getURL(), to.getLocalId(), request);

	xoap::MessageReference replyMsg = xoap::createMessage();
	replyMsg->readFrom(reply);
	return replyMsg;
}

// ---------------------------------------------------------------------------
// xoap::SOAPMessage serialization (SOAP 1.1 envelope; one body element, attributes = params)
// ---------------------------------------------------------------------------
namespace
{
std::string xmlEscape(const std::string& s)
{
	std::string o;
	for(char c : s)
		switch(c)
		{
		case '&':
			o += "&amp;";
			break;
		case '<':
			o += "&lt;";
			break;
		case '>':
			o += "&gt;";
			break;
		case '"':
			o += "&quot;";
			break;
		default:
			o += c;
		}
	return o;
}
}  // namespace

void xoap::SOAPMessage::writeTo(std::string& out) const
{
	std::ostringstream s;
	s << "<SOAP-ENV:Envelope "
	     "xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\" "
	     "xmlns:xdaq=\"urn:xdaq-soap:3.0\"><SOAP-ENV:Header/><SOAP-ENV:Body>";
	for(const auto& e : bodyElements_)
	{
		s << "<xdaq:" << e.getElementName().getLocalName();
		for(const auto& a : e.getAttributes())
			s << " " << a.first << "=\"" << xmlEscape(a.second) << "\"";
		s << "/>";
	}
	s << "</SOAP-ENV:Body></SOAP-ENV:Envelope>";
	out = s.str();
}

// Parses the wire form written by writeTo(): the first element inside <...Body> is the
// command; its attributes are the parameters. Deliberately minimal (no full XML parser):
// enough to round-trip what SOAPUtilities produces.
void xoap::SOAPMessage::readFrom(const std::string& in)
{
	bodyElements_.clear();
	size_t b = in.find("Body>");
	if(b == std::string::npos)
		XCEPT_RAISE(xoap::exception::Exception, "SOAP message has no Body");
	size_t lt = in.find('<', b + 5);
	if(lt == std::string::npos)
		return;
	size_t gt = in.find('>', lt);
	if(gt == std::string::npos)
		XCEPT_RAISE(xoap::exception::Exception, "malformed SOAP body element");
	std::string tag = in.substr(lt + 1, gt - lt - 1);
	if(!tag.empty() && tag.back() == '/')
		tag.pop_back();
	size_t      sp   = tag.find_first_of(" \t\r\n");
	std::string qn   = tag.substr(0, sp);
	size_t      col  = qn.find(':');
	std::string name = col == std::string::npos ? qn : qn.substr(col + 1);
	SOAPElement el   = SOAPPart(&bodyElements_)
	                     .getEnvelope()
	                     .getBody()
	                     .addBodyElement(SOAPName(
	                         name, col == std::string::npos ? "" : qn.substr(0, col)));
	if(sp == std::string::npos)
		return;
	std::string attrs = tag.substr(sp);
	size_t      pos   = 0;
	while(true)
	{
		size_t eq = attrs.find('=', pos);
		if(eq == std::string::npos)
			break;
		size_t ks       = attrs.find_last_of(" \t\r\n", eq);
		ks              = ks == std::string::npos ? 0 : ks + 1;
		std::string key = attrs.substr(ks, eq - ks);
		size_t      q1  = attrs.find('"', eq);
		if(q1 == std::string::npos)
			break;
		size_t q2 = attrs.find('"', q1 + 1);
		if(q2 == std::string::npos)
			break;
		std::string val = attrs.substr(q1 + 1, q2 - q1 - 1);
		for(const auto& rep : std::vector<std::pair<std::string, std::string>>{
		        {"&quot;", "\""}, {"&lt;", "<"}, {"&gt;", ">"}, {"&amp;", "&"}})
			for(size_t f = val.find(rep.first); f != std::string::npos;
			    f        = val.find(rep.first, f + rep.second.size()))
                val.replace(f, rep.first.size(), rep.second);
		if(key.find("xmlns") != 0)
			el.addAttribute(SOAPName(key), val);
		pos = q2 + 1;
	}
}

// ---------------------------------------------------------------------------
// toolbox::task::getWorkLoopFactory
// ---------------------------------------------------------------------------
toolbox::task::WorkLoopFactory* toolbox::task::getWorkLoopFactory()
{
	static WorkLoopFactory factory;
	return &factory;
}

// ---------------------------------------------------------------------------
// cgicc
// ---------------------------------------------------------------------------
std::string cgicc::form_urldecode(const std::string& s)
{
	std::string o;
	for(size_t i = 0; i < s.size(); ++i)
	{
		if(s[i] == '+')
			o += ' ';
		else if(s[i] == '%' && i + 2 < s.size())
		{
			o += static_cast<char>(std::stoi(s.substr(i + 1, 2), nullptr, 16));
			i += 2;
		}
		else
			o += s[i];
	}
	return o;
}

cgicc::Cgicc::Cgicc(CgiInput* input)
{
	if(!input)
		return;
	env_ = input->getEnvironment();
	parse(input->getQueryString());
	parseBody(env_.getContentType(), input->getPostData());
}

cgicc::Cgicc::Cgicc(const CgiEnvironment& env,
                    const std::string&    queryString,
                    const std::string&    postData)
    : env_(env)
{
	parse(queryString);
	parseBody(env.getContentType(), postData);
}

void cgicc::Cgicc::parseBody(const std::string& contentType, const std::string& postData)
{
	size_t b = contentType.find("boundary=");
	if(contentType.find("multipart/form-data") != std::string::npos &&
	   b != std::string::npos)
	{
		std::string boundary = contentType.substr(b + 9);
		size_t      end      = boundary.find(';');
		if(end != std::string::npos)
			boundary.erase(end);
		while(!boundary.empty() && (boundary.back() == ' ' || boundary.back() == '\r' ||
		                            boundary.back() == '\n'))
			boundary.pop_back();
		if(boundary.size() > 1 && boundary.front() == '"' && boundary.back() == '"')
			boundary = boundary.substr(1, boundary.size() - 2);
		parseMultipart(boundary, postData);
	}
	else if(contentType.find("application/x-www-form-urlencoded") != std::string::npos ||
	        contentType.empty())
		parse(postData);
}

// RFC 7578: parts are delimited by "--<boundary>"; each has a header block ending in a
// blank line. A part with a filename is a FormFile, otherwise a FormEntry.
void cgicc::Cgicc::parseMultipart(const std::string& boundary, const std::string& body)
{
	const std::string delim = "--" + boundary;
	size_t            pos   = body.find(delim);
	while(pos != std::string::npos)
	{
		pos += delim.size();
		if(body.compare(pos, 2, "--") == 0)
			break;  // closing delimiter
		size_t hdrStart = body.find_first_not_of("\r\n", pos);
		if(hdrStart == std::string::npos)
			break;
		size_t hdrEnd    = body.find("\r\n\r\n", hdrStart);
		size_t dataStart = hdrEnd + 4;
		if(hdrEnd == std::string::npos)
		{
			hdrEnd = body.find("\n\n", hdrStart);
			if(hdrEnd == std::string::npos)
				break;
			dataStart = hdrEnd + 2;
		}
		size_t next = body.find(delim, dataStart);
		if(next == std::string::npos)
			break;
		size_t dataEnd = next;  // drop the line break that precedes the next delimiter
		if(dataEnd >= dataStart + 2 && body.compare(dataEnd - 2, 2, "\r\n") == 0)
			dataEnd -= 2;
		else if(dataEnd > dataStart && body[dataEnd - 1] == '\n')
			dataEnd -= 1;

		std::string headers = body.substr(hdrStart, hdrEnd - hdrStart);
		auto        attr    = [&headers](const std::string& key) {
            size_t k = headers.find(
                " " + key + "=\"");  // leading space: "name" must not match "filename"
            if(k == std::string::npos)
                return std::string();
            k += key.size() + 3;
            size_t q = headers.find('"', k);
            return headers.substr(k, q == std::string::npos ? std::string::npos : q - k);
		};
		std::string type;
		size_t      ct = headers.find("Content-Type:");
		if(ct != std::string::npos)
		{
			ct += 13;
			size_t ctEnd = headers.find_first_of("\r\n", ct);
			type         = headers.substr(
                ct, ctEnd == std::string::npos ? std::string::npos : ctEnd - ct);
			type.erase(0, type.find_first_not_of(' '));
		}
		std::string data = body.substr(dataStart, dataEnd - dataStart);
		if(headers.find("filename=") != std::string::npos)
			files_.emplace_back(attr("name"), attr("filename"), type, data);
		else
			entries_.emplace_back(attr("name"), data);
		pos = next;
	}
}

void cgicc::Cgicc::parse(const std::string& data)
{
	size_t pos = 0;
	while(pos < data.size())
	{
		size_t      amp = data.find('&', pos);
		std::string pair =
		    data.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
		size_t eq = pair.find('=');
		if(!pair.empty())
			entries_.emplace_back(
			    form_urldecode(pair.substr(0, eq)),
			    eq == std::string::npos ? "" : form_urldecode(pair.substr(eq + 1)));
		if(amp == std::string::npos)
			break;
		pos = amp + 1;
	}
}

cgicc::Cgicc::form_iterator cgicc::Cgicc::getElement(const std::string& name)
{
	for(auto it = entries_.begin(); it != entries_.end(); ++it)
		if(it->getName() == name)
			return it;
	return entries_.end();
}

cgicc::Cgicc::const_form_iterator cgicc::Cgicc::getElement(const std::string& name) const
{
	for(auto it = entries_.begin(); it != entries_.end(); ++it)
		if(it->getName() == name)
			return it;
	return entries_.end();
}

bool cgicc::Cgicc::getElement(const std::string&      name,
                              std::vector<FormEntry>& result) const
{
	result.clear();
	for(const auto& e : entries_)
		if(e.getName() == name)
			result.push_back(e);
	return !result.empty();
}

cgicc::Cgicc::file_iterator cgicc::Cgicc::getFile(const std::string& name)
{
	for(auto it = files_.begin(); it != files_.end(); ++it)
		if(it->getName() == name)
			return it;
	return files_.end();
}

cgicc::Cgicc::const_file_iterator cgicc::Cgicc::getFile(const std::string& name) const
{
	for(auto it = files_.begin(); it != files_.end(); ++it)
		if(it->getName() == name)
			return it;
	return files_.end();
}
