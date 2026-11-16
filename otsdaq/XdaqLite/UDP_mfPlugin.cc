// OTS_LITE stand-in for the "UDP" MessageFacility destination from artdaq_mfextensions.
// Accepts the same fcl keys so otsdaq's MessageFacility*.fcl files load unchanged, and sends
// each message as one UDP datagram to host:port, in the line format otsConsoleFwd and the
// Console Supervisor parse (the same line ots maxlite's OtsLog sends, see Macros/OtsLog.cc):
//   UDPMFMESSAGE|time|seq|host|hostaddr|severity|category|application|pid|iteration|module|file|line|message
// Before this it discarded every message, so the Console web app received nothing in lite: its
// receive loop gave up after five heartbeats and the page kept showing its first-load overlay.
#include "messagefacility/plugins/mfPlugin.h"

#include "fhiclcpp/types/AllowedConfigurationMacro.h"
#include "fhiclcpp/types/Atom.h"
#include "fhiclcpp/types/TableFragment.h"
#include "messagefacility/MessageLogger/MessageLogger.h"
#include "messagefacility/MessageService/ELdestination.h"

#include <arpa/inet.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cstring>
#include <ctime>
#include <memory>
#include <mutex>
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
		fhicl::Atom<int> error_turnoff_threshold{fhicl::Name{"error_turnoff_threshold"},
		                                         0};
		fhicl::Atom<int> error_report_backoff_factor{
		    fhicl::Name{"error_report_backoff_factor"}, 0};
		fhicl::Atom<std::string> host{fhicl::Name{"host"}, "127.0.0.1"};
		fhicl::Atom<int>         port{fhicl::Name{"port"}, 0};
		fhicl::Atom<bool> multicast_enabled{fhicl::Name{"multicast_enabled"}, false};
		fhicl::Atom<std::string> multicast_interface_ip{
		    fhicl::Name{"multicast_interface_ip"}, ""};
		fhicl::Atom<std::string> filename_delimit{fhicl::Name{"filename_delimit"}, "/"};
	};
	using Parameters = fhicl::WrappedTable<Config>;

	explicit ELUDPLite(Parameters const& pset)
	    : ELdestination(pset().elDestConfig())
	    , filename_delimit_(pset().filename_delimit())
	    , pid_(std::to_string(getpid()))
	{
		const int port = pset().port();
		if(port <= 0)
			return;  // no destination configured: stay silent, as before

		struct addrinfo hints, *res = nullptr;
		memset(&hints, 0, sizeof(hints));
		hints.ai_family   = AF_INET;
		hints.ai_socktype = SOCK_DGRAM;
		if(getaddrinfo(pset().host().c_str(), nullptr, &hints, &res) != 0 || !res)
			return;
		memcpy(&destination_, res->ai_addr, sizeof(destination_));
		freeaddrinfo(res);
		destination_.sin_port = htons(static_cast<uint16_t>(port));

		fd_ = socket(AF_INET, SOCK_DGRAM, 0);
		if(fd_ < 0)
			return;
		fcntl(fd_, F_SETFD, FD_CLOEXEC);  // not inherited by helpers ots_lite_main forks
		int bufferSize = 4 * 1024 * 1024;  // absorb bursts (e.g. during Configure)
		setsockopt(fd_, SOL_SOCKET, SO_SNDBUF, &bufferSize, sizeof(bufferSize));

		char hostBuf[256] = "";
		gethostname(hostBuf, sizeof(hostBuf) - 1);
		hostname_    = hostBuf;
		hostAddress_ = "127.0.0.1";
		if(getaddrinfo(hostBuf, nullptr, &hints, &res) == 0 && res)
		{
			char addrBuf[INET_ADDRSTRLEN] = "";
			inet_ntop(AF_INET,
			          &reinterpret_cast<struct sockaddr_in*>(res->ai_addr)->sin_addr,
			          addrBuf,
			          sizeof(addrBuf));
			hostAddress_ = addrBuf;
			freeaddrinfo(res);
		}
	}

	~ELUDPLite() override
	{
		if(fd_ >= 0)
			close(fd_);
	}

  protected:
	// the whole line is built in routePayload; ELdestination's own prefix/suffix are not used
	void fillPrefix(std::ostringstream&, const mf::ErrorObj&) override {}
	void fillUsrMsg(std::ostringstream&, const mf::ErrorObj&) override {}
	void fillSuffix(std::ostringstream&, const mf::ErrorObj&) override {}

	void routePayload(const std::ostringstream&, const mf::ErrorObj& msg) override
	{
		if(fd_ < 0)
			return;

		const auto& xid = msg.xid();
		char        stamp[64];
		timeval     tv  = msg.timestamp();
		time_t      sec = tv.tv_sec;
		struct tm   tmv;
		localtime_r(&sec, &tmv);
		strftime(stamp, sizeof(stamp), "%d-%b-%Y %H:%M:%S", &tmv);

		std::string text;
		for(const auto& item : msg.items())
			text += item;
		if(!text.empty() && text.front() == '\n')
			text.erase(0, 1);
		if(!text.empty() && text.back() == '\n')
			text.pop_back();

		std::string line;
		line.reserve(text.size() + 200);
		line += "UDPMFMESSAGE|";
		line += stamp;
		line += '|';
		const size_t sequencePosition = line.size();  // filled under the lock
		line += '|';
		line += hostname_;
		line += '|';
		line += hostAddress_;
		line += '|';
		line += xid.severity().getName();  // Error, Warning, Info, Debug
		line += '|';
		line += xid.id();  // category (__MF_SUBJECT__)
		line += '|';
		line += mf::GetApplicationName();
		line += '|';
		line += pid_;
		line += '|';
		line += mf::GetIteration();
		line += '|';
		line += xid.module();
		line += '|';
		line += shortFilename(msg.filename());
		line += '|';
		line += std::to_string(msg.lineNumber());
		line += '|';
		line += text;

		// the Console checks that each process's sequence numbers arrive in order
		std::lock_guard<std::mutex> lock(sendMutex_);
		line.insert(sequencePosition, std::to_string(++sequence_));
		sendto(fd_,
		       line.data(),
		       line.size(),
		       0,
		       reinterpret_cast<const struct sockaddr*>(&destination_),
		       sizeof(destination_));  // best effort: a lost line is not an error
	}

  private:
	std::string shortFilename(const std::string& file) const
	{
		if(filename_delimit_.empty())
			return file;
		const auto p = file.rfind(filename_delimit_);
		return p == std::string::npos ? file : file.substr(p + filename_delimit_.size());
	}

	int                fd_ = -1;
	struct sockaddr_in destination_;
	std::string        filename_delimit_, hostname_, hostAddress_, pid_;
	uint32_t           sequence_ = 0;
	std::mutex         sendMutex_;
};
}  // namespace ots

MAKE_MFPLUGIN_START(, pset) { return std::make_unique<ots::ELUDPLite>(pset); }
FHICL_PROVIDE_ALLOWED_CONFIGURATION(ots::ELUDPLite)
MAKE_PLUGIN_END
