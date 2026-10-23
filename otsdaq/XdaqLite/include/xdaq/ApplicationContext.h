// OTS_LITE stand-in for xdaq's xdaq/ApplicationContext.h: the registry of applications in this
// context plus the SOAP transport between applications. Zone/group are faked as a single "daq" group.
#ifndef _xdaq_ApplicationContext_h_
#define _xdaq_ApplicationContext_h_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "xdaq/ApplicationDescriptor.h"
#include "xdaq/ContextDescriptor.h"
#include "xoap/MessageReference.h"

namespace xdaq
{
class Application;

class ApplicationGroup
{
  public:
	std::set<const ApplicationDescriptor*> getApplicationDescriptors() const { return descriptors_; }
	std::set<const ApplicationDescriptor*> getApplicationDescriptors(const std::string& className) const
	{
		std::set<const ApplicationDescriptor*> r;
		for(auto d : descriptors_)
			if(d->getClassName() == className)
				r.insert(d);
		return r;
	}
	std::set<const ApplicationDescriptor*> descriptors_;
};

class Zone
{
  public:
	const ApplicationGroup* getApplicationGroup(const std::string& /*name*/) const { return &group_; }
	ApplicationGroup        group_;
};

class NetGroup
{
};

class ApplicationContext
{
  public:
	explicit ApplicationContext(const std::string& url) : context_(url) {}

	const ContextDescriptor* getContextDescriptor() const { return &context_; }
	const Zone*              getDefaultZone() const { return &zone_; }
	const NetGroup*          getNetGroup() const { return &netGroup_; }

	// Registers an application living in THIS context; returns its descriptor.
	const ApplicationDescriptor* addApplication(Application* app, const std::string& className, unsigned int lid, unsigned int instance);
	// Registers a descriptor for an application in ANOTHER context (known from the XDAQ context table).
	const ApplicationDescriptor* addRemoteApplication(const std::string& className, unsigned int lid, unsigned int instance, const std::string& contextURL);

	Application* getApplication(unsigned int lid) const;
	// Binds the instantiated application to a descriptor registered with addApplication(nullptr, ...).
	void setApplication(unsigned int lid, Application* app);

	// SOAP transport. Local targets are dispatched in-process; remote targets go over HTTP
	// (not yet implemented in lite: throws xdaq::exception::Exception).
	xoap::MessageReference postSOAP(xoap::MessageReference msg, const ApplicationDescriptor& from, const ApplicationDescriptor& to);

  private:
	ContextDescriptor                                       context_;
	Zone                                                    zone_;
	NetGroup                                                netGroup_;
	std::vector<std::unique_ptr<ApplicationDescriptor>>     descriptors_;
	std::vector<std::unique_ptr<ContextDescriptor>>         remoteContexts_;
	std::vector<std::pair<unsigned int, Application*>>      localApps_;
};
}  // namespace xdaq

#endif
