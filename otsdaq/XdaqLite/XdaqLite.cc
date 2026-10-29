// OTS_LITE: out-of-line parts of the xdaq stand-in (see include/ for the header-only parts).
#include "cgicc/Cgicc.h"
#include "toolbox/task/WorkLoopFactory.h"
#include "xdaq/Application.h"
#include "xdaq/ApplicationContext.h"
#include "xdaq/HandlerRegistry.h"
#include "xgi/Input.h"
#include "xgi/Output.h"

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

xoap::MessageReference xdaq::ApplicationContext::postSOAP(
    xoap::MessageReference msg,
    const ApplicationDescriptor& /*from*/,
    const ApplicationDescriptor& to)
{
	if(to.getContextDescriptor() == &context_)
	{
		if(Application* local = getApplication(to.getLocalId()))
			return local->dispatchSOAP(msg);
		XCEPT_RAISE(xdaq::exception::Exception,
		            "OTS_LITE: SOAP target lid=" + std::to_string(to.getLocalId()) +
		                " (" + to.getClassName() +
		                ") is in this context but not constructed yet");
	}
	XCEPT_RAISE(xdaq::exception::Exception,
	            "OTS_LITE: SOAP to a remote context is not implemented yet (target lid=" +
	                std::to_string(to.getLocalId()) + " at " +
	                to.getContextDescriptor()->getURL() + ")");
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
	if(env_.getContentType().find("application/x-www-form-urlencoded") !=
	       std::string::npos ||
	   env_.getContentType().empty())
		parse(input->getPostData());
}

cgicc::Cgicc::Cgicc(const CgiEnvironment& env,
                    const std::string&    queryString,
                    const std::string&    postData)
    : env_(env)
{
	parse(queryString);
	if(env.getContentType().find("application/x-www-form-urlencoded") !=
	       std::string::npos ||
	   env.getContentType().empty())
		parse(postData);
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
