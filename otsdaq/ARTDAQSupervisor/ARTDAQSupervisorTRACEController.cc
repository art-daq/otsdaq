#include "otsdaq/ARTDAQSupervisor/ARTDAQSupervisorTRACEController.h"

#include <cstdint>
#include <set>
#include <sstream>
#include <string>

ots::ARTDAQSupervisorTRACEController::ARTDAQSupervisorTRACEController() {}

//==============================================================================
// parseOtsTraceLevels (file-local helper)
//	Parse `ots -tt` machine output into the host trace level map. Output is a
//	sequence of blocks (other lines, e.g. Fast_ots_setup.sh banners, are ignored):
//		#OTSTRACE-HOST <hostname>
//		<name> <maskM> <maskS> <maskT>      (masks in hex, e.g. 0x1ff)
//		...
//		#OTSTRACE-END
//	Each host's labels are stored under the "artdaq.." + hostname key (so artdaq
//	TRACE is managed independently of any normal ots TRACE on the same host).
//	Returns the number of distinct artdaq host keys added.
static size_t parseOtsTraceLevels(const std::string&                        otsOutput,
                                  ots::ITRACEController::HostTraceLevelMap& outMap)
{
	static const std::string hostTag = "#OTSTRACE-HOST ";
	std::set<std::string>     hostsSeen;
	std::istringstream        stream(otsOutput);
	std::string               line;
	std::string               curKey;  // "artdaq.." + host, or empty when outside a block

	while(std::getline(stream, line))
	{
		if(line.rfind(hostTag, 0) == 0)
		{
			std::string host = line.substr(hostTag.size());
			while(!host.empty() &&
			      (host.back() == '\r' || host.back() == ' ' || host.back() == '\t'))
				host.pop_back();
			curKey = host.empty() ? "" : ("artdaq.." + host);
			if(!curKey.empty())
				hostsSeen.insert(curKey);
			continue;
		}
		if(line.rfind("#OTSTRACE", 0) == 0)  // -END, -OK, or any other marker
		{
			if(line.rfind("#OTSTRACE-END", 0) == 0)
				curKey = "";
			continue;
		}
		if(curKey.empty())
			continue;

		std::istringstream iss(line);
		std::string        name, sM, sS, sT;
		if(!(iss >> name >> sM >> sS >> sT))
			continue;  // not a 4-token data row
		try
		{
			uint64_t M = std::stoull(sM, nullptr, 0);  // base 0 => handles hex (0x..)
			uint64_t S = std::stoull(sS, nullptr, 0);
			uint64_t T = std::stoull(sT, nullptr, 0);
			outMap[curKey][name].M = M;
			outMap[curKey][name].S = S;
			outMap[curKey][name].T = T;
		}
		catch(...)
		{
			continue;  // skip non-numeric (banner) lines
		}
	}
	return hostsSeen.size();
}  // end parseOtsTraceLevels()

const ots::ITRACEController::HostTraceLevelMap&
ots::ARTDAQSupervisorTRACEController::getTraceLevels()
{
	__COUT__ << "getTraceLevels() BEGIN -- theSupervisor_ is "
	         << (theSupervisor_ ? "SET" : "NULL") << __E__;

	traceLevelsMap_.clear();  // reset

	ots::ITRACEController::addTraceLevelsForThisHost();  // local supervisor host
	if(theSupervisor_)
	{
		// PRIMARY PATH: enumerate artdaq hosts from the configuration (reliable even
		// when DAQInterface is not running -- the original failure mode) and query
		// their TRACE levels via `ots -tt`, which uses ssh + trace_cntl and needs
		// only hostnames (no runtime commander ports, which the config lacks).
		std::set<std::string> hosts          = theSupervisor_->getConfiguredArtdaqHosts();
		size_t                artdaqHostCount = 0;
		if(!hosts.empty())
		{
			std::string csv;
			for(const auto& h : hosts)
			{
				if(!csv.empty())
					csv += ",";
				csv += h;
			}
			std::string cmd = "ots -tt '" + csv + "' '*'";
			__COUT__ << "Primary TRACE path: " << cmd << __E__;

			std::string out;
			try
			{
				out = StringMacros::exec(cmd.c_str());
			}
			catch(const std::exception& e)
			{
				__COUT_ERR__ << "'ots -tt' failed: " << e.what() << __E__;
			}
			catch(...)
			{
				__COUT_ERR__ << "'ots -tt' failed (unknown exception)." << __E__;
			}

			artdaqHostCount = parseOtsTraceLevels(out, traceLevelsMap_);
			__COUT__ << "'ots -tt' populated " << artdaqHostCount << " artdaq host(s)."
			         << __E__;
		}
		else
			__COUT__ << "No configured artdaq hosts returned from configuration." << __E__;

		// FALLBACK PATH: if the config/ots path yielded no artdaq hosts, use the live
		// DAQInterface commanders (kept as an alternative per design).
		if(artdaqHostCount == 0)
		{
			__COUT__ << "No artdaq hosts via 'ots -tt'; falling back to live DAQInterface "
			            "commanders."
			         << __E__;
			auto commanders = theSupervisor_->makeCommandersFromProcessInfo();
			__COUT__ << "makeCommandersFromProcessInfo() returned " << commanders.size()
			         << " live artdaq commander(s)." << __E__;

			for(auto& comm : commanders)
			{
				std::string lvlstring;
				try
				{
					lvlstring = comm.second->send_trace_get("ALL");
				}
				catch(const std::exception& e)
				{
					__COUT_ERR__ << "send_trace_get(\"ALL\") FAILED for host '"
					             << comm.first.host << "' port " << comm.first.port
					             << " -- skipping. Exception: " << e.what() << __E__;
					continue;
				}
				catch(...)
				{
					__COUT_ERR__ << "send_trace_get(\"ALL\") FAILED for host '"
					             << comm.first.host << "' port " << comm.first.port
					             << " -- skipping (unknown exception)." << __E__;
					continue;
				}

				auto lvls = ARTDAQSupervisor::tokenize_(lvlstring);
				for(auto& lvl : lvls)
				{
					std::istringstream iss(lvl);
					std::string        name;
					uint64_t           lvlM, lvlS, lvlT;
					iss >> name >> lvlM >> lvlS >> lvlT;

					// PREPEND special artdaq signature, so that normal TRACE on those hosts can be modified independently of artdaq processes that are handled
					// by ARTDAQ supervisor.
					traceLevelsMap_["artdaq.." + comm.first.host][name].M = lvlM;
					traceLevelsMap_["artdaq.." + comm.first.host][name].S = lvlS;
					traceLevelsMap_["artdaq.." + comm.first.host][name].T = lvlT;
				}
			}
		}
	}

	__COUT__ << "getTraceLevels() END -- traceLevelsMap_ has " << traceLevelsMap_.size()
	         << " host key(s):" << __E__;
	for(const auto& host : traceLevelsMap_)
		__COUT__ << "    host key '" << host.first << "' with " << host.second.size()
		         << " label(s)." << __E__;

	return traceLevelsMap_;
}  // end getTraceLevels()

void ots::ARTDAQSupervisorTRACEController::setTraceLevelMask(
    const std::string& label,
    TraceMasks const&  lvl,
    const std::string& host /*=localhost*/,
    std::string const& mode /*= "ALL"*/)
{
	bool allMode = mode == "ALL";

	// artdaq hosts carry the "artdaq.." prefix (see getTraceLevels()).
	static const std::string artdaqPrefix = "artdaq..";
	if(theSupervisor_ && host.rfind(artdaqPrefix, 0) == 0)
	{
		const std::string realHost = host.substr(artdaqPrefix.size());

		// PRIMARY PATH: `ots -tlvl*` whole-mask write via ssh + trace_cntl. Only the
		// column(s) selected by 'mode' are written (the other TraceMasks fields are not
		// meaningful when mode != ALL), so use the per-column commands for FAST/SLOW/TRIGGER.
		std::string cmd;
		if(allMode)
			cmd = "ots -tlvlmsk '" + realHost + "' '" + label + "' " +
			      std::to_string(lvl.M) + " " + std::to_string(lvl.S) + " " +
			      std::to_string(lvl.T);
		else if(mode == "FAST")
			cmd = "ots -tlvlM '" + realHost + "' '" + label + "' " + std::to_string(lvl.M);
		else if(mode == "SLOW")
			cmd = "ots -tlvlS '" + realHost + "' '" + label + "' " + std::to_string(lvl.S);
		else if(mode == "TRIGGER")
			cmd = "ots -tlvlT '" + realHost + "' '" + label + "' " + std::to_string(lvl.T);

		if(!cmd.empty())
		{
			__COUT__ << "Primary TRACE set: " << cmd << __E__;
			std::string out;
			try
			{
				out = StringMacros::exec(cmd.c_str());
			}
			catch(...)
			{
				out = "";
			}
			if(out.find("#OTSTRACE-OK") != std::string::npos)
				return;  // success
			__COUT_ERR__ << "'ots' TRACE set did not confirm (#OTSTRACE-OK missing); "
			                "falling back to live commander."
			             << __E__;
		}

		// FALLBACK PATH: live DAQInterface commander.
		auto commanders = theSupervisor_->makeCommandersFromProcessInfo();
		for(auto& comm : commanders)
		{
			if(artdaqPrefix + comm.first.host == host)
			{
				if(allMode || mode == "FAST")
					comm.second->send_trace_set(label, "M", std::to_string(lvl.M));
				if(allMode || mode == "SLOW")
					comm.second->send_trace_set(label, "S", std::to_string(lvl.S));
				if(allMode || mode == "TRIGGER")
					comm.second->send_trace_set(label, "T", std::to_string(lvl.T));
				return;
			}
		}
		return;
	}

	ots::ITRACEController::setTraceLevelsForThisHost(label, lvl, mode);
}  // end setTraceLevelMask()
