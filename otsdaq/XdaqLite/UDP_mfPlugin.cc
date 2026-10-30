// OTS_LITE stand-in for the "UDP" MessageFacility destination from artdaq_mfextensions.
// Accepts the same fcl keys so otsdaq's MessageFacility*.fcl files load unchanged, but
// discards every message: the Console web app (its receiver) gets nothing in lite.
#include "messagefacility/plugins/mfPlugin.h"

#include "fhiclcpp/types/AllowedConfigurationMacro.h"
#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/types/TableFragment.h"
#include "messagefacility/MessageService/ELdestination.h"

#include <memory>
#include <sstream>
#include <string>

namespace ots
{
class ELUDPLite : public mf::service::ELdestination
{
  public:
	struct Config
	{
		fhicl::TableFragment<mf::service::ELdestination::Config> elDestConfig;
		fhicl::Atom<int>         error_turnoff_threshold{fhicl::Name{"error_turnoff_threshold"}, 0};
		fhicl::Atom<int>         error_report_backoff_factor{fhicl::Name{"error_report_backoff_factor"}, 0};
		fhicl::Atom<std::string> host{fhicl::Name{"host"}, "127.0.0.1"};
		fhicl::Atom<int>         port{fhicl::Name{"port"}, 0};
		fhicl::Atom<bool>        multicast_enabled{fhicl::Name{"multicast_enabled"}, false};
		fhicl::Atom<std::string> multicast_interface_ip{fhicl::Name{"multicast_interface_ip"}, ""};
		fhicl::Atom<std::string> filename_delimit{fhicl::Name{"filename_delimit"}, "/"};
	};
	using Parameters = fhicl::WrappedTable<Config>;

	explicit ELUDPLite(Parameters const& pset) : ELdestination(pset().elDestConfig()) {}

  protected:
	void routePayload(const std::ostringstream&, const mf::ErrorObj&) override {}
};
}  // namespace ots

MAKE_MFPLUGIN_START(, pset)
{
	return std::make_unique<ots::ELUDPLite>(pset);
}
FHICL_PROVIDE_ALLOWED_CONFIGURATION(ots::ELUDPLite)
MAKE_PLUGIN_END
