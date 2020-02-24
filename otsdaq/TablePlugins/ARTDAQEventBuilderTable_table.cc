

#include "otsdaq/Macros/TablePluginMacros.h"
#include "otsdaq/TablePlugins/ARTDAQEventBuilderTable.h"
#include "otsdaq/TablePlugins/XDAQContextTable.h"

#include <stdio.h>
#include <sys/stat.h>  //for mkdir
#include <fstream>     // std::fstream
#include <iostream>

using namespace ots;

//==============================================================================
ARTDAQEventBuilderTable::ARTDAQEventBuilderTable(void) : ARTDAQTableBase("ARTDAQEventBuilderTable")
{
	//////////////////////////////////////////////////////////////////////
	// WARNING: the names used in C++ MUST match the Table INFO  //
	//////////////////////////////////////////////////////////////////////
}

//==============================================================================
ARTDAQEventBuilderTable::~ARTDAQEventBuilderTable(void) {}

//==============================================================================
void ARTDAQEventBuilderTable::init(ConfigurationManager* configManager)
{
	// use isFirstAppInContext to only run once per context, for example to avoid
	//	generating files on local disk multiple times.
	bool isFirstAppInContext = configManager->isOwnerFirstAppInContext();

	//__COUTV__(isFirstAppInContext);
	if(!isFirstAppInContext)
		return;

	// make directory just in case
	mkdir((ARTDAQTableBase::ARTDAQ_FCL_PATH).c_str(), 0755);

	//	__COUT__ << "*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*" << __E__;
	//	__COUT__ << configManager->__SELF_NODE__ << __E__;

	// handle fcl file generation, wherever the level of this table

	auto buiders = configManager->__SELF_NODE__.getChildren(
	    /*default filterMap*/ std::map<std::string /*relative-path*/, std::string /*value*/>(),
	    /*default byPriority*/ false,
	    /*TRUE! onlyStatusTrue*/ true);

	for(auto& builder : buiders)
	{
		ARTDAQTableBase::outputDataReceiverFHICL(builder.second, ARTDAQTableBase::ARTDAQAppType::EventBuilder);
		ARTDAQTableBase::flattenFHICL(ARTDAQAppType::EventBuilder, builder.second.getValue());
	}

}  // end init()

DEFINE_OTS_TABLE(ARTDAQEventBuilderTable)