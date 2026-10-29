// OTS_LITE: implementations for the artdaq stand-in headers in include/artdaq*/.
#include "artdaq-core/Utilities/ExceptionHandler.hh"
#include "artdaq-core/Utilities/configureMessageFacility.hh"
#include "artdaq/DAQdata/Globals.hh"

#include "fhiclcpp/ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include <unistd.h>
#include <exception>
#include <filesystem>
#include <iostream>
#include <sstream>

std::unique_ptr<artdaq::MetricManager> artdaq::Globals::metricMan_;
std::string                            artdaq::Globals::app_name_;

void artdaq::ExceptionHandler(ExceptionHandlerRethrow decision, const std::string& optional_message)
{
	try
	{
		throw;
	}
	catch(const std::exception& e)
	{
		std::cerr << "ExceptionHandler: " << optional_message << " std::exception: " << e.what() << std::endl;
	}
	catch(...)
	{
		std::cerr << "ExceptionHandler: " << optional_message << " unknown exception" << std::endl;
	}
	if(decision == ExceptionHandlerRethrow::yes)
		throw;
}

std::string artdaq::generateMessageFacilityConfiguration(char const* progname, bool useConsole, bool printDebug, char const* fileExtraName)
{
	const char*       logRoot = getenv("ARTDAQ_LOG_ROOT");
	const std::string level   = printDebug ? "DEBUG" : "INFO";
	std::ostringstream ss;
	ss << "destinations : { ";
	if(useConsole)
		ss << "console : { type : \"cout\" threshold : \"" << level << "\" } ";
	if(logRoot != nullptr)
	{
		std::filesystem::path dir = std::filesystem::path(logRoot) / progname;
		std::error_code       ec;
		std::filesystem::create_directories(dir, ec);
		ss << "file : { type : \"file\" threshold : \"DEBUG\" filename : \"" << (dir / (std::string(progname) + fileExtraName + "-%p.log")).string() << "\" append : false } ";
	}
	ss << "} ";
	return ss.str();
}

void artdaq::configureTRACE(fhicl::ParameterSet&) {}

void artdaq::configureMessageFacility(char const* progname, bool useConsole, bool printDebug)
{
	const std::string config = generateMessageFacilityConfiguration(progname, useConsole, printDebug);
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
