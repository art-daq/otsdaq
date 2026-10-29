// OTS_LITE stand-in for artdaq/DAQdata/Globals.hh. There is no metric manager in lite;
// the MetricManager stub exists so code paths that only test `metricMan` compile.
#ifndef artdaq_DAQdata_Globals_hh
#define artdaq_DAQdata_Globals_hh

#include <memory>
#include <string>

#include "fhiclcpp/ParameterSet.h"

namespace artdaq
{
enum class MetricMode
{
	LastPoint,
	Accumulate,
	Rate,
	Average,
	Minimum,
	Maximum
};

class MetricManager
{
  public:
	void initialize(fhicl::ParameterSet const&, std::string const&) {}
	bool Initialized() const { return false; }
	bool Running() const { return false; }
	void do_start() {}
	void do_stop() {}
	void shutdown() {}
	template<typename T>
	void sendMetric(std::string const&, T const&, std::string const&, int, MetricMode, std::string const& = "")
	{
	}
};

class Globals
{
  public:
	static std::unique_ptr<MetricManager> metricMan_;
	static std::string                    app_name_;
	static void                           CleanUpGlobals() { metricMan_.reset(); }
};
}  // namespace artdaq

#define metricMan artdaq::Globals::metricMan_
#define app_name  artdaq::Globals::app_name_

#endif
