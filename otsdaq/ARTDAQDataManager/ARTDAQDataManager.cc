#include "../ARTDAQDataManager/ARTDAQDataManager.h"

#include "otsdaq/DataProcessorPlugins/ARTDAQConsumer.h"
#include "otsdaq/DataProcessorPlugins/ARTDAQProducer.h"

#include "artdaq/BuildInfo/GetPackageBuildInfo.hh"

#include <cassert>
#include <iostream>

using namespace ots;

//==============================================================================
ARTDAQDataManager::ARTDAQDataManager(const ConfigurationTree& theXDAQContextConfigTree, const std::string& supervisorConfigurationPath)
    : DataManager(theXDAQContextConfigTree, supervisorConfigurationPath)
{
	INIT_MF("." /*directory used is USER_DATA/LOG/.*/);

	__CFG_COUT__ << "Constructor." << __E__;

	std::string name = "BoardReader";

	__CFG_MCOUT__("artdaq version " << artdaq::GetPackageBuildInfo::getPackageBuildInfo().getPackageVersion() << ", built "
	                                << artdaq::GetPackageBuildInfo::getPackageBuildInfo().getBuildTimestamp());

	// artdaq::configureMessageFacility("boardreader");
	// artdaq::configureMessageFacility(name.c_str());

	__CFG_COUT__ << "MF initialized" << __E__;

	rank_ = Configurable::getApplicationLID();

	__CFG_COUTV__(rank_);

	// unsigned short port = 5100;

	// artdaq::setMsgFacAppName(name, port);

	// create the BoardReaderApp
	//	  artdaq::BoardReaderApp br_app(local_group_comm, name);
	__CFG_COUT__ << "END" << __E__;

	__CFG_COUT__ << "Constructed." << __E__;
}

//==============================================================================
ARTDAQDataManager::~ARTDAQDataManager(void) {}

//==============================================================================
void ARTDAQDataManager::configure(void)
{
	__CFG_COUT__ << "ARTDAQDataManager configuring..." << __E__;

	DataManager::configure();

	// find the ARTDAQ processor (can only be 1)
	//	and initialize with rank info

	__CFG_COUT__ << "ARTDAQDataManager DataManager configured now pass the MPI stuff" << __E__;
	for(auto it = DataManager::buffers_.begin(); it != DataManager::buffers_.end(); it++)
		for(auto& consumer : it->second.consumers_)
			if(dynamic_cast<ARTDAQConsumer*>(consumer))
			{
				__CFG_COUT__ << "Found an ARTDAQ Consumer: " << consumer->getProcessorID() << __E__;

				dynamic_cast<ARTDAQConsumer*>(consumer)->initLocalGroup(rank_);
				return;  // There can only be 1 ARTDAQConsumer for each
				         // ARTDAQDataManager!!!!!!!
			}

	__CFG_SS__ << "There was no ARTDAQ Consumer found on a buffer!" << __E__;
	__CFG_COUT__ << ss.str();

	__CFG_COUT__ << "Looking for an ARTDAQ Producer..." << __E__;

	for(auto it = DataManager::buffers_.begin(); it != DataManager::buffers_.end(); it++)
		for(auto& producer : it->second.producers_)
			if(dynamic_cast<ARTDAQProducer*>(producer))
			{
				__CFG_COUT__ << "Found an ARTDAQ Producer: " << producer->getProcessorID() << __E__;

				dynamic_cast<ARTDAQProducer*>(producer)->initLocalGroup(rank_);
				return;  // There can only be 1 ARTDAQProducer for each
				         // ARTDAQDataManager!!!!!!!
			}

	ss << "No ARTDAQ Producers found either... so error!" << __E__;
	__CFG_COUT__ << ss.str();

	{
		__CFG_SS__;
		DataManager::dumpStatus((std::ostream*)&ss);
		__COUT__ << ss.str() << __E__;
	}

	__CFG_COUT_ERR__ << ss.str();
	__CFG_SS_THROW__;
}  // end configure()

//==============================================================================
void ARTDAQDataManager::stop(void) { DataManager::stop(); }  // end stop()
