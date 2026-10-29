// OTS_LITE stand-in for artdaq/Application/LoadParameterSet.hh (same logic as artdaq's):
// try the argument as literal FHiCL, else treat it as a file name looked up on FHICL_FILE_PATH.
#ifndef artdaq_Application_LoadParameterSet_hh
#define artdaq_Application_LoadParameterSet_hh

#include <cstdlib>
#include <string>

#include "cetlib/filepath_maker.h"
#include "fhiclcpp/ParameterSet.h"
#include "fhiclcpp/exception.h"

inline fhicl::ParameterSet LoadParameterSet(std::string const& psetOrFile)
{
	try
	{
		return fhicl::ParameterSet::make(psetOrFile);
	}
	catch(const fhicl::exception&)
	{
		if(getenv("FHICL_FILE_PATH") == nullptr)
			setenv("FHICL_FILE_PATH", ".", 0);
		cet::filepath_lookup_after1 lookup("FHICL_FILE_PATH");
		return fhicl::ParameterSet::make(psetOrFile, lookup);
	}
}

#endif
