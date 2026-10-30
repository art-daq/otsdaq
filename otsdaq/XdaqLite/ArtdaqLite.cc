// OTS_LITE: implementations for the artdaq stand-in headers in include/artdaq*/.
#include "artdaq-core/Utilities/ExceptionHandler.hh"
#include "artdaq-core/Utilities/configureMessageFacility.hh"
#include "artdaq/DAQdata/Globals.hh"

#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include <unistd.h>
#include <exception>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

std::unique_ptr<artdaq::MetricManager> artdaq::Globals::metricMan_;
std::string                            artdaq::Globals::app_name_;

void artdaq::ExceptionHandler(ExceptionHandlerRethrow decision,
                              const std::string&      optional_message)
{
	try
	{
		throw;
	}
	catch(const std::exception& e)
	{
		std::cerr << "ExceptionHandler: " << optional_message
		          << " std::exception: " << e.what() << std::endl;
	}
	catch(...)
	{
		std::cerr << "ExceptionHandler: " << optional_message << " unknown exception"
		          << std::endl;
	}
	if(decision == ExceptionHandlerRethrow::yes)
		throw;
}

// Follows artdaq-core: the destinations come from the fcl file named by ARTDAQ_LOG_FHICL
// (otsdaq's MessageFacility*.fcl, which use the "OTS" and "UDP" destination types; lite
// builds its own versions of those two plugins alongside this library). With no fcl, a
// plain cout destination is used so messages still reach the ots quiet-run log.
// No per-application log file is generated: otsdaq's fcl files all end with `file: ""`
// to turn that off, and a generated `file` table placed before it was taken instead of
// the fcl's value, silently diverting every message into "<ARTDAQ_LOG_ROOT>/./.-%p.log".
std::string artdaq::generateMessageFacilityConfiguration(char const* /*progname*/,
                                                         bool        useConsole,
                                                         bool        printDebug,
                                                         char const* /*fileExtraName*/)
{
	const char*        logFhicl = getenv("ARTDAQ_LOG_FHICL");
	const std::string  level    = printDebug ? "DEBUG" : "INFO";
	std::ostringstream ss;
	ss << "destinations : { ";
	if(useConsole || logFhicl == nullptr)
		ss << "console : { type : \"cout\" threshold : \"" << level << "\" } ";
	if(logFhicl != nullptr)
	{
		std::ifstream in(logFhicl);
		if(!in)
			throw std::runtime_error(std::string("Unable to open requested fhicl file ARTDAQ_LOG_FHICL=\"") + logFhicl + "\".");
		ss << in.rdbuf();
	}
	ss << "} ";
	return ss.str();
}

void artdaq::configureTRACE(fhicl::ParameterSet&) {}

void artdaq::configureMessageFacility(char const* progname,
                                      bool        useConsole,
                                      bool        printDebug)
{
	const std::string config =
	    generateMessageFacilityConfiguration(progname, useConsole, printDebug);
	mf::StartMessageFacility(fhicl::ParameterSet::make(config), progname);
}

std::string artdaq::setMsgFacAppName(const std::string& appType, unsigned short port)
{
	std::string appName(appType);
	char        hostname[256];
	if(gethostname(hostname, sizeof(hostname)) == 0)
	{
		std::string host(hostname);
		size_t      dot = host.find('.');
		if(dot != std::string::npos && dot > 2)
			host = host.substr(0, dot);
		appName += "-" + host;
	}
	appName += "-" + std::to_string(port);
	mf::SetApplicationName(appName);
	return appName;
}
