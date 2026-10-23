// OTS_LITE: out-of-line parts of the xdaq stand-in (see include/ for the header-only parts).
#include "xdaq/Application.h"
#include "xdaq/ApplicationContext.h"
#include "xgi/Input.h"
#include "xgi/Output.h"
#include "toolbox/task/WorkLoopFactory.h"
#include "cgicc/Cgicc.h"

#include <map>
#include <mutex>
#include <sstream>

// ---------------------------------------------------------------------------
// xdaq::Application
// ---------------------------------------------------------------------------
bool xdaq::Application::dispatchHTTP(const std::string& name, xgi::Input* in, xgi::Output* out)
{
	auto it = httpHandlers_.find(name.empty() ? "Default" : name);
	if(it == httpHandlers_.end())
		return false;
	it->second(in, out);
	return true;
}

xoap::MessageReference xdaq::Application::dispatchSOAP(xoap::MessageReference msg)
{
	const auto& body = msg->bodyElements();
	if(body.empty())
		XCEPT_RAISE(xdaq::exception::Exception, "SOAP message has no body element");
	const std::string command = body.front().getElementName().getLocalName();
	auto              it      = soapHandlers_.find(command);
	if(it == soapHandlers_.end())
		XCEPT_RAISE(xdaq::exception::Exception, "No SOAP handler bound for command '" + command + "' in " + getApplicationDescriptor()->getClassName());
	return it->second(msg);
}

namespace
{
std::map<std::string, xdaq::ApplicationInstantiator>& instantiators()
{
	static std::map<std::string, xdaq::ApplicationInstantiator> m;
	return m;
}
}  // namespace

void xdaq::registerApplicationClass(const std::string& className, ApplicationInstantiator fn) { instantiators()[className] = fn; }

xdaq::ApplicationInstantiator xdaq::findApplicationClass(const std::string& className)
{
	auto it = instantiators().find(className);
	return it == instantiators().end() ? nullptr : it->second;
}

// ---------------------------------------------------------------------------
// xdaq::ApplicationContext
// ---------------------------------------------------------------------------
const xdaq::ApplicationDescriptor* xdaq::ApplicationContext::addApplication(Application* app, const std::string& className, unsigned int lid, unsigned int instance)
{
	descriptors_.emplace_back(new ApplicationDescriptor(className, lid, instance, &context_));
	zone_.group_.descriptors_.insert(descriptors_.back().get());
	localApps_.emplace_back(lid, app);
	return descriptors_.back().get();
}

const xdaq::ApplicationDescriptor* xdaq::ApplicationContext::addRemoteApplication(const std::string& className, unsigned int lid, unsigned int instance, const std::string& contextURL)
{
	remoteContexts_.emplace_back(new ContextDescriptor(contextURL));
	descriptors_.emplace_back(new ApplicationDescriptor(className, lid, instance, remoteContexts_.back().get()));
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

xoap::MessageReference xdaq::ApplicationContext::postSOAP(xoap::MessageReference msg, const ApplicationDescriptor& /*from*/, const ApplicationDescriptor& to)
{
	if(Application* local = getApplication(to.getLocalId()))
		if(to.getContextDescriptor() == &context_)
			return local->dispatchSOAP(msg);
	XCEPT_RAISE(xdaq::exception::Exception,
	            "OTS_LITE: SOAP to a remote context is not implemented yet (target lid=" + std::to_string(to.getLocalId()) + " at " + to.getContextDescriptor()->getURL() + ")");
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
		case '&': o += "&amp;"; break;
		case '<': o += "&lt;"; break;
		case '>': o += "&gt;"; break;
		case '"': o += "&quot;"; break;
		default: o += c;
		}
	return o;
}
}  // namespace

void xoap::SOAPMessage::writeTo(std::string& out) const
{
	std::ostringstream s;
	s << "<SOAP-ENV:Envelope xmlns:SOAP-ENV=\"http://schemas.xmlsoap.org/soap/envelope/\" xmlns:xdaq=\"urn:xdaq-soap:3.0\"><SOAP-ENV:Header/><SOAP-ENV:Body>";
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

void xoap::SOAPMessage::readFrom(const std::string& /*in*/)
{
	// Parsing of the wire form is needed only once SOAP goes over HTTP between contexts.
	XCEPT_RAISE(xoap::exception::Exception, "OTS_LITE: SOAPMessage::readFrom not implemented yet");
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
	if(env_.getContentType().find("application/x-www-form-urlencoded") != std::string::npos || env_.getContentType().empty())
		parse(input->getPostData());
}

cgicc::Cgicc::Cgicc(const CgiEnvironment& env, const std::string& queryString, const std::string& postData) : env_(env)
{
	parse(queryString);
	if(env.getContentType().find("application/x-www-form-urlencoded") != std::string::npos || env.getContentType().empty())
		parse(postData);
}

void cgicc::Cgicc::parse(const std::string& data)
{
	size_t pos = 0;
	while(pos < data.size())
	{
		size_t      amp  = data.find('&', pos);
		std::string pair = data.substr(pos, amp == std::string::npos ? std::string::npos : amp - pos);
		size_t      eq   = pair.find('=');
		if(!pair.empty())
			entries_.emplace_back(form_urldecode(pair.substr(0, eq)), eq == std::string::npos ? "" : form_urldecode(pair.substr(eq + 1)));
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

bool cgicc::Cgicc::getElement(const std::string& name, std::vector<FormEntry>& result) const
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
