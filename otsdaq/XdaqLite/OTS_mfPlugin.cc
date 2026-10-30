// OTS_LITE stand-in for the "OTS" MessageFacility destination from artdaq_mfextensions
// (mfextensions/Destinations/OTS_mfPlugin.cc), so the MessageFacility*.fcl files otsdaq
// ships work unchanged. Formats each message with the fcl's format_string
// (e.g. "|%L:%N: %f:%u |\t%m") and writes it to stdout, i.e. the ots quiet-run log.
// Same format specifiers as the original: %A app, %a host address, %d module, %f file
// (after filename_delimit), %h host, %L severity, %m message, %N category, %P pid,
// %r iteration, %s severity initial (lower case), %T timestamp, %u line, %% '%'.
#include "messagefacility/plugins/mfPlugin.h"

#include "fhiclcpp/types/AllowedConfigurationMacro.h"
#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/types/TableFragment.h"
#include "messagefacility/MessageLogger/MessageLogger.h"
#include "messagefacility/MessageService/ELdestination.h"
#include "messagefacility/Utilities/ErrorObj.h"

#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>

namespace ots
{
class ELOTSLite : public mf::service::ELdestination
{
  public:
	struct Config
	{
		fhicl::TableFragment<mf::service::ELdestination::Config> elDestConfig;
		fhicl::Atom<std::string> format_string{fhicl::Name{"format_string"}, fhicl::Comment{"Format specifier for printing to console. %% => '%' ..."}, "%L:%N:%f [%u]\t%m"};
		fhicl::Atom<std::string> filename_delimit{fhicl::Name{"filename_delimit"}, fhicl::Comment{"Grab path after this. \"/srcs/\" /x/srcs/y/z.cc => y/z.cc"}, "/"};
	};
	using Parameters = fhicl::WrappedTable<Config>;

	explicit ELOTSLite(Parameters const& pset)
	    : ELdestination(pset().elDestConfig())
	    , format_string_(pset().format_string())
	    , filename_delimit_(pset().filename_delimit())
	    , pid_(getpid())
	{
		char buf[256];
		if(gethostname(buf, sizeof(buf)) == 0)
		{
			hostname_ = buf;
			if(hostent* he = gethostbyname(buf))
				if(he->h_addr_list[0])
					hostaddr_ = inet_ntoa(*reinterpret_cast<in_addr*>(he->h_addr_list[0]));
		}
	}

  protected:
	void fillPrefix(std::ostringstream& oss, const mf::ErrorObj& msg) override
	{
		const auto& xid        = msg.xid();
		bool        msgPrinted = false;
		for(const char* cp = format_string_.c_str(); *cp; ++cp)
		{
			if(*cp != '%')
			{
				oss << *cp;
				continue;
			}
			if(*++cp == '\0')
			{
				oss << '%';
				break;
			}
			switch(*cp)
			{
			case 'A': oss << mf::GetApplicationName(); break;
			case 'a': oss << hostaddr_; break;
			case 'd': oss << xid.module(); break;
			case 'f': oss << shortFilename(msg.filename()); break;
			case 'h': oss << hostname_; break;
			case 'L': oss << xid.severity().getInputStr(); break;
			case 'm':
				oss << messageText(msg);
				msgPrinted = true;
				break;
			case 'N': oss << xid.id(); break;
			case 'P': oss << pid_; break;
			case 'r': oss << mf::GetIteration(); break;
			case 's': oss << static_cast<char>(xid.severity().getInputStr()[0] | 0x20); break;
			case 'T': oss << format_.timestamp(msg.timestamp()); break;
			case 'u': oss << msg.lineNumber(); break;
			case '%': oss << '%'; break;
			default: oss << '%' << *cp; break;
			}
		}
		if(!msgPrinted)
			oss << messageText(msg);
	}
	void fillUsrMsg(std::ostringstream&, const mf::ErrorObj&) override {}
	void fillSuffix(std::ostringstream&, const mf::ErrorObj&) override {}

	void routePayload(const std::ostringstream& oss, const mf::ErrorObj&) override
	{
		std::lock_guard<std::mutex> lk(mutex_);
		std::cout << oss.str() << '\n';
		std::cout.flush();
	}

  private:
	std::string shortFilename(const std::string& file) const
	{
		if(filename_delimit_.empty())
			return file;
		if(filename_delimit_.size() == 1)
		{
			size_t p = file.rfind(filename_delimit_[0]);
			return p == std::string::npos ? file : file.substr(p + 1);
		}
		size_t p = file.find(filename_delimit_);
		if(p == std::string::npos)
			return file;
		// skip the delimiter and the directory right after it ("/srcs/otsdaq/..." -> "otsdaq/...")
		p += filename_delimit_.size() - 1;
		size_t q = file.find('/', p + 1);
		return q == std::string::npos ? file.substr(p + 1) : file.substr(q + 1);
	}
	static std::string messageText(const mf::ErrorObj& msg)
	{
		std::string s;
		for(const auto& item : msg.items())
			s += item;
		if(!s.empty() && s.front() == '\n')
			s.erase(0, 1);
		if(!s.empty() && s.back() == '\n')
			s.pop_back();
		return s;
	}

	std::string format_string_, filename_delimit_, hostname_, hostaddr_;
	long        pid_;
	std::mutex  mutex_;
};
}  // namespace ots

MAKE_MFPLUGIN_START(, pset)
{
	return std::make_unique<ots::ELOTSLite>(pset);
}
FHICL_PROVIDE_ALLOWED_CONFIGURATION(ots::ELOTSLite)
MAKE_PLUGIN_END
