#include "otsdaq/ARTDAQSupervisor/ARTDAQSupervisorTRACEController.h"

#include <cstdint>
#include <ctime>
#include <set>
#include <sstream>
#include <string>

ots::ARTDAQSupervisorTRACEController::ARTDAQSupervisorTRACEController() {}

//==============================================================================
// parseOtsTraceLevels (file-local helper)
static size_t parseOtsTraceLevels(const std::string&                        otsOutput,
                                  ots::ITRACEController::HostTraceLevelMap& outMap)
{
	static const std::string hostTag = "#OTSTRACE-HOST ";
	std::set<std::string>    hostsSeen;
	std::istringstream       stream(otsOutput);
	std::string              line;
	std::string              curKey;

	while(std::getline(stream, line))
	{
		if(line.rfind(hostTag, 0) == 0)
		{
			std::string host = line.substr(hostTag.size());
			while(!host.empty() &&
			      (host.back() == '\r' || host.back() == ' ' || host.back() == '\t'))
				host.pop_back();
			// Normalize: strip -data/-ipmi network suffixes so hosts merge
			// with addTraceLevelsForThisHost() which uses the plain hostname.
			auto pos = host.find("-data");
			if(pos != std::string::npos)
				host.erase(pos, 5);
			pos = host.find("-ipmi");
			if(pos != std::string::npos)
				host.erase(pos, 5);
			curKey = host;
			if(!curKey.empty())
				hostsSeen.insert(curKey);
			continue;
		}
		if(line.rfind("#OTSTRACE", 0) == 0)
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
			continue;
		try
		{
			uint64_t M             = std::stoull(sM, nullptr, 0);
			uint64_t S             = std::stoull(sS, nullptr, 0);
			uint64_t T             = std::stoull(sT, nullptr, 0);
			outMap[curKey][name].M = M;
			outMap[curKey][name].S = S;
			outMap[curKey][name].T = T;
		}
		catch(...)
		{
			continue;
		}
	}
	return hostsSeen.size();
}  // end parseOtsTraceLevels()

const ots::ITRACEController::HostTraceLevelMap&
ots::ARTDAQSupervisorTRACEController::getTraceLevels()
{
	__COUT__ << "getTraceLevels() BEGIN" << __E__;

	traceLevelsMap_.clear();

	ots::ITRACEController::addTraceLevelsForThisHost();

	// If setTraceLevelMask() just ran and cached the updated host's levels
	// (within the last 5 seconds), use that cache instead of a full ots -tt
	// readback. The set leaf already includes a level dump, so this avoids
	// a redundant SSH fan-out to all hosts.
	time_t now = time(nullptr);
	if(!lastSetLevels_.empty() && (now - lastSetTime_) < 5)
	{
		__COUT__ << "Using cached set-response levels (" << lastSetLevels_.size()
		         << " host key(s), age " << (now - lastSetTime_) << "s)." << __E__;
		for(const auto& hostEntry : lastSetLevels_)
			traceLevelsMap_[hostEntry.first] = hostEntry.second;
		lastSetLevels_.clear();
	}
	else
	{
		lastSetLevels_.clear();

		std::string cmd = "ots -tt";
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

		size_t artdaqHostCount = parseOtsTraceLevels(out, traceLevelsMap_);
		__COUT__ << "'ots -tt' populated " << artdaqHostCount << " artdaq host(s)."
		         << __E__;
	}

	// Merge duplicate host keys that differ only by domain suffix (e.g. "mu2e-calo-01"
	// from gethostname() vs "mu2e-calo-01.fnal.gov" from ots -tt). Merge the longer
	// key's labels into the shorter key, then remove the longer key. Only merges when
	// the short name matches exactly up to a '.' boundary — different base names or
	// domains are never merged.
	{
		std::vector<std::string> keysToRemove;
		for(auto& entry : traceLevelsMap_)
		{
			const std::string& key    = entry.first;
			auto               dotPos = key.find('.');
			if(dotPos == std::string::npos)
				continue;  // no domain — can't be the long form
			std::string shortKey = key.substr(0, dotPos);
			auto        it       = traceLevelsMap_.find(shortKey);
			if(it != traceLevelsMap_.end() && it->first != key)
			{
				// Merge: copy labels from FQDN key into short key (short key wins on collision)
				for(const auto& label : entry.second)
					it->second.emplace(label.first, label.second);
				keysToRemove.push_back(key);
			}
		}
		for(const auto& k : keysToRemove)
			traceLevelsMap_.erase(k);
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

	// Determine if the target host is local by comparing stripped short hostnames.
	auto stripHost = [](const std::string& h) -> std::string {
		std::string s   = h;
		auto        pos = s.find("-data");
		if(pos != std::string::npos)
			s.erase(pos, 5);
		pos = s.find("-ipmi");
		if(pos != std::string::npos)
			s.erase(pos, 5);
		pos = s.find('.');
		if(pos != std::string::npos)
			s = s.substr(0, pos);
		return s;
	};

	std::string localShort  = stripHost(getHostnameString());
	std::string targetShort = stripHost(host);
	bool        isLocal     = (host == "localhost" || targetShort == localShort);

	if(isLocal)
	{
		ots::ITRACEController::setTraceLevelsForThisHost(label, lvl, mode);
		return;
	}

	// Remote host: use ots -ttlvl* (parse format) via SSH
	std::string cmd;
	if(allMode)
		cmd = "ots -ttlvlmsk '" + host + "' '" + label + "' " + std::to_string(lvl.M) +
		      " " + std::to_string(lvl.S) + " " + std::to_string(lvl.T);
	else if(mode == "FAST")
		cmd = "ots -ttlvlM '" + host + "' '" + label + "' " + std::to_string(lvl.M);
	else if(mode == "SLOW")
		cmd = "ots -ttlvlS '" + host + "' '" + label + "' " + std::to_string(lvl.S);
	else if(mode == "TRIGGER")
		cmd = "ots -ttlvlT '" + host + "' '" + label + "' " + std::to_string(lvl.T);

	if(!cmd.empty())
	{
		__COUT__ << "Remote TRACE set: " << cmd << __E__;
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
		{
			lastSetLevels_.clear();
			parseOtsTraceLevels(out, lastSetLevels_);
			lastSetTime_ = time(nullptr);
			__COUT__ << "Set confirmed; cached " << lastSetLevels_.size()
			         << " host(s) from response." << __E__;
			return;
		}
		__COUT_ERR__ << "'ots' TRACE set did not confirm (#OTSTRACE-OK missing)."
		             << __E__;
	}
}  // end setTraceLevelMask()
