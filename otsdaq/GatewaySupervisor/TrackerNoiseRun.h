#pragma once

// Transport-independent NoiseRun scan logic. Only the Iterator adapter accesses OTS.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <locale>
#include <map>
#include <regex>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace ots
{
namespace tracker_noise_run
{
inline void require(bool value, const std::string& message)
{
	if(!value)
		throw std::runtime_error(message);
}
struct Target
{
	std::string              uid;
	unsigned int             supervisor;
	std::vector<int>         links;
	std::vector<std::string> inputs, outputs, publisherInputs, publisherOutputs;
};
using Outputs = std::map<std::string, std::string>;
inline std::vector<std::string> persistedIdentity(const std::string& path)
{
	std::ifstream f(path);
	require(bool(f), "Cannot read persisted ActiveTableGroups.cfg");
	std::vector<std::string> lines;
	std::string              line;
	while(std::getline(f, line))
	{
		if(!line.empty() && line.back() == '\r')
			line.pop_back();
		lines.push_back(line);
	}
	require(lines.size() == 8 && !lines[0].empty() && !lines[6].empty(),
	        "Malformed persisted active groups");
	for(auto i : {1, 7})
		require(std::regex_match(lines[i], std::regex("[0-9]+")),
		        "Invalid persisted group key");
	return {lines[0],
	        lines[1],
	        lines[6],
	        lines[7]};  // Context and Configuration, not editor state.
}
inline unsigned long long scalar(const std::string& text)
{
	std::smatch m;
	require(
	    std::regex_match(
	        text, m, std::regex("\\s*([0-9]+)(?:\\s+\\(0[xX]([0-9a-fA-F]+)\\))?\\s*")),
	    "Malformed numeric macro output");
	auto n = std::stoull(m[1]);
	if(m[2].matched)
		require(n == std::stoull(m[2], nullptr, 16),
		        "Mismatched decimal/hex macro output");
	return n;
}
inline std::vector<int> numbers(const std::string& text)
{
	if(text.find('[') == std::string::npos)
	{
		auto n = scalar(text);
		require(n <= 65535, "Macro number outside range");
		return {static_cast<int>(n)};
	}
	require(std::regex_match(
	            text, std::regex("\\s*\\[\\s*[0-9]+(?:\\s*,\\s*[0-9]+)*\\s*\\]\\s*")),
	        "Malformed macro output list");
	std::vector<int> result;
	const std::regex digit("[0-9]+");
	for(auto i = std::sregex_iterator(text.begin(), text.end(), digit);
	    i != std::sregex_iterator();
	    ++i)
	{
		auto n = std::stoul(i->str());
		require(n <= 65535, "Macro number outside range");
		result.push_back(n);
	}
	return result;
}
inline void validateTargets(const std::vector<Target>& targets)
{
	require(!targets.empty() && targets.size() <= 36, "Invalid NoiseRun DTC coverage");
	std::set<std::string> seen;
	for(const auto& t : targets)
	{
		require(std::regex_match(t.uid, std::regex("Trk(0[1-9]|1[0-8])_DTC[01]")) &&
		            seen.insert(t.uid).second,
		        "Invalid/duplicate NoiseRun target");
		const std::set<int> links(t.links.begin(), t.links.end());
		require(!links.empty() && links.size() == t.links.size() && *links.begin() >= 0 &&
		            *links.rbegin() <= 5,
		        "Invalid/duplicate ROC links");
	}
}
inline double totalHz(const Target& target, const Outputs& out)
{
	auto links = numbers(out.at("Target ROC")), codes = numbers(out.at("Return Code"));
	require(links.size() == target.links.size() &&
	            std::set<int>(links.begin(), links.end()) ==
	                std::set<int>(target.links.begin(), target.links.end()),
	        "Incomplete ROC coverage: " + target.uid);
	require(codes.size() == links.size() &&
	            std::all_of(codes.begin(), codes.end(), [](int n) { return n == 0; }),
	        "Rate/mask restoration failed: " + target.uid);
	const auto&      masks = out.at("Saved hardware masks");
	const std::regex mask(
	    "ROC\\s+([0-5])\\s*:\\s*CAL\\s+(0x[0-9a-fA-F]+)\\s+(0x[0-9a-fA-F]+)\\s+(0x[0-9a-"
	    "fA-F]+)\\s*;\\s*HV\\s+(0x[0-9a-fA-F]+)\\s+(0x[0-9a-fA-F]+)\\s+(0x[0-9a-fA-F]+)");
	std::set<int> maskLinks;
	for(auto i = std::sregex_iterator(masks.begin(), masks.end(), mask);
	    i != std::sregex_iterator();
	    ++i)
	{
		require(maskLinks.insert(std::stoi((*i)[1])).second, "Duplicate saved mask");
		for(int k = 2; k <= 7; ++k)
			require(std::stoul((*i)[k], nullptr, 16) <= 65535, "Invalid saved mask word");
	}
	require(maskLinks == std::set<int>(links.begin(), links.end()),
	        "Missing saved mask coverage: " + target.uid);
	std::istringstream input(out.at("Rates"));
	input.imbue(std::locale::classic());
	std::string      line;
	int              channels = 0, tables = 0;
	double           sum = 0;
	const std::regex row("\\s*([0-9]+)\\s+(\\S+)\\s+(\\S+)\\s+(\\S+)\\s*");
	while(std::getline(input, line))
	{
		std::smatch m;
		if(!std::regex_match(line, m, row))
			continue;
		int ch = std::stoi(m[1]);
		if(ch == 0)
		{
			require(channels == 0 || channels == 96, "Incomplete rate table");
			channels = 0;
			++tables;
		}
		require(tables > 0 && ch == channels && ch < 96, "Nonsequential rate channels");
		++channels;
		for(int k = 2; k <= 4; ++k)
		{
			size_t n = 0;
			double v = std::stod(m[k], &n);
			require(n == m[k].str().size() && std::isfinite(v) && v >= 0,
			        "Invalid rate value");
			if(k == 4)
				sum += v * 1000;  // kHz Total/coincidence column -> Hz, all 96 channels.
		}
	}
	require(
	    tables == static_cast<int>(links.size()) && channels == 96 && std::isfinite(sum),
	    "Incomplete rate tables");
	return sum;
}
struct Summary
{
	std::vector<double> rates;
	double              total = 0, seconds = 0;
	unsigned int        channels = 0, rocs = 0;
	std::string         json(const std::vector<Target>& targets) const
	{
		std::ostringstream s;
		s.imbue(std::locale::classic());
		s << std::setprecision(17);
		s << "{\"schema_version\":1,\"total_hz\":" << total
		  << ",\"channels\":" << channels << ",\"scan_seconds\":" << seconds
		  << ",\"dtcs\":[";
		for(size_t i = 0; i < targets.size(); ++i)
			s << (i ? "," : "") << "{\"uid\":\"" << targets[i].uid
			  << "\",\"total_hz\":" << rates.at(i)
			  << ",\"channels\":" << 96 * targets[i].links.size()
			  << ",\"rocs\":" << targets[i].links.size() << "}";
		return s.str() + "]}";
	}
};
// Always settle every submitted operation before reporting any failure. Never retry
// a measurement or publish whose outcome might be uncertain.
inline Summary scan(const std::vector<Target>&                   targets,
                    const std::function<Outputs(const Target&)>& measure,
                    const std::function<void()>&                 guard,
                    const std::function<void(const Summary&)>&   publish)
{
	validateTargets(targets);
	guard();
	const auto                        start = std::chrono::steady_clock::now();
	std::vector<std::future<Outputs>> jobs;
	std::string                       errors;
	try
	{
		for(const auto& t : targets)
			jobs.push_back(std::async(std::launch::async, [&, t] { return measure(t); }));
	}
	catch(const std::exception& e)
	{
		errors = e.what();
	}
	Summary summary;
	for(size_t i = 0; i < jobs.size(); ++i)
	{
		try
		{
			double v = totalHz(targets[i], jobs[i].get());
			summary.rates.push_back(v);
			summary.total += v;
			summary.rocs += targets[i].links.size();
		}
		catch(const std::exception& e)
		{
			errors += "; " + targets[i].uid + ": " + e.what();
		}
		catch(...)
		{
			errors += "; " + targets[i].uid + ": unknown scan failure";
		}
	}
	require(errors.empty(),
	        "NoiseRun scan failed; all submitted requests settled" + errors);
	require(summary.rates.size() == targets.size() && std::isfinite(summary.total),
	        "Incomplete/invalid scan total");
	summary.channels = 96 * summary.rocs;
	summary.seconds =
	    std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	guard();
	publish(summary);
	return summary;
}
inline std::chrono::steady_clock::time_point nextScan(
    std::chrono::steady_clock::time_point start,
    std::chrono::steady_clock::time_point end)
{
	return std::max(start + std::chrono::seconds(3), end);
}
}  // namespace tracker_noise_run
}  // namespace ots
