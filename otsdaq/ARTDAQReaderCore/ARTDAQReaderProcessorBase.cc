#include "otsdaq/ARTDAQReaderCore/ARTDAQReaderProcessorBase.h"
#include "artdaq/Application/Commandable.hh"
#include "otsdaq/DataManager/DataManager.h"
#include "otsdaq/DataManager/DataManagerSingleton.h"
#include "otsdaq/Macros/CoutMacros.h"
// #include "otsdaq/Macros/ProcessorPluginMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <set>

using namespace ots;

#define ARTDAQ_FCL_PATH std::string(__ENV__("USER_DATA")) + "/" + "ARTDAQConfigurations/"
#define ARTDAQ_FILE_PREAMBLE "boardReader"

//==============================================================================
ARTDAQReaderProcessorBase::ARTDAQReaderProcessorBase(std::string /*supervisorApplicationUID*/,
                                                     std::string /*bufferUID*/,
                                                     std::string              processorUID,
                                                     const ConfigurationTree& theXDAQContextConfigTree,
                                                     const std::string&       configurationPath)
    // : WorkLoop(processorUID)
    // , DataProducer(supervisorApplicationUID, bufferUID, processorUID)
    // theXDAQContextConfigTree.getNode(configurationPath).getNode("BufferSize").getValue<unsigned
    // int>())
    : Configurable(theXDAQContextConfigTree, configurationPath), name_("BoardReader_" + processorUID)
{
	INIT_MF("." /*directory used is USER_DATA/LOG/.*/);

	__CFG_COUT__ << "Constructing..." << __E__;
	//__CFG_COUT__ << "Configuration string:-" <<
	// theXDAQContextConfigTree.getNode(configurationPath).getNode("ConfigurationString").getValue<std::string>()
	//<< "-" << __E__;

	std::string filename = ARTDAQ_FCL_PATH + ARTDAQ_FILE_PREAMBLE + "-";
	std::string uid      = theXDAQContextConfigTree.getNode(configurationPath).getValue();

	__CFG_COUT__ << "uid: " << uid << __E__;
	for(unsigned int i = 0; i < uid.size(); ++i)
		if((uid[i] >= 'a' && uid[i] <= 'z') || (uid[i] >= 'A' && uid[i] <= 'Z') || (uid[i] >= '0' && uid[i] <= '9'))  // only allow alpha numeric in file name
			filename += uid[i];
	filename += ".fcl";

	__CFG_COUT__ << __E__;
	__CFG_COUT__ << __E__;
	__CFG_COUT__ << "filename: " << filename << __E__;

	std::string fileFclString;
	{
		std::ifstream in(filename, std::ios::in | std::ios::binary);
		if(in)
		{
			std::string contents;
			in.seekg(0, std::ios::end);
			fileFclString.resize(in.tellg());
			in.seekg(0, std::ios::beg);
			in.read(&fileFclString[0], fileFclString.size());
			in.close();
		}
	}
	//__CFG_COUT__ << fileFclString << __E__;

	// find fragment_receiver {
	// and insert e.g.,
	//	SupervisorApplicationUID:"ARTDataManager0"
	//	BufferUID:"ART_S0_DM0_DataBuffer0"
	//	ProcessorUID:"ART_S0_DM0_DB0_ARTConsumer0"
	size_t fcli = fileFclString.find("fragment_receiver: {") + +strlen("fragment_receiver: {");
	if(fcli == std::string::npos)
	{
		__SS__ << "Could not find 'fragment_receiver: {' in Board Reader fcl string!" << __E__;
		__CFG_COUT__ << "\n" << ss.str();
		__SS_THROW__;
	}

	// get the parent IDs from configurationPath
	__CFG_COUT__ << "configurationPath " << configurationPath << __E__;

	std::string  consumerID, bufferID, appID;
	unsigned int backSteps;  // at 2, 4, and 7 are the important parent IDs
	size_t       backi = -1, backj;
	backSteps          = 7;
	for(unsigned int i = 0; i < backSteps; i++)
	{
		//__CFG_COUT__ << "backsteps: " << i+1 << __E__;

		backj = backi;
		backi = configurationPath.rfind('/', backi - 1);

		//__CFG_COUT__ << "backi:" << backi << " backj:" << backj << __E__;
		//__CFG_COUT__ << "substr: " << configurationPath.substr(backi+1,backj-backi-1) <<
		// __E__;

		if(i + 1 == 2)
			consumerID = configurationPath.substr(backi + 1, backj - backi - 1);
		else if(i + 1 == 4)
			bufferID = configurationPath.substr(backi + 1, backj - backi - 1);
		else if(i + 1 == 7)
			appID = configurationPath.substr(backi + 1, backj - backi - 1);
	}

	// insert parent IDs into fcl string
	fileFclString = fileFclString.substr(0, fcli) + "\n\t\t" + "SupervisorApplicationUID: \"" + appID + "\"\n\t\t" + "BufferUID: \"" + bufferID + "\"\n\t\t" +
	                "ProcessorUID: \"" + consumerID + "\"\n" + fileFclString.substr(fcli);

	__CFG_COUT__ << fileFclString << __E__;

	try
	{
		fhiclConfiguration_ = fhicl::ParameterSet::make(fileFclString);
	}
	catch(const cet::coded_exception<fhicl::error, &fhicl::detail::translate>& e)
	{
		__CFG_SS__ << "Error was caught while configuring: " << e.what() << __E__;
		__CFG_SS_THROW__;
	}
	catch(const cet::exception& e)
	{
		__CFG_SS__ << "Error was caught while configuring: " << e.explain_self() << __E__;
		__CFG_SS_THROW__;
	}
	catch(...)
	{
		__CFG_SS__ << "Unknown error was caught while configuring." << __E__;
		__CFG_SS_THROW__;
	}

	// fhicl::make_ParameterSet(theXDAQContextConfigTree.getNode(configurationPath).getNode("ConfigurationString").getValue<std::string>(),
	// fhiclConfiguration_);

}  // end constructor()

//==============================================================================
// ARTDAQReaderProcessorBase::ARTDAQReaderProcessorBase(std::string interfaceID, MPI_Comm
// local_group_comm, std::string name) :FEVInterface     (feId, 0)
// ,local_group_comm_(local_group_comm)
//,name_            (name)
//{}

//==============================================================================
ARTDAQReaderProcessorBase::~ARTDAQReaderProcessorBase(void)
{
	halt();
	__CFG_COUT__ << "DONE DELETING!" << __E__;
}

//==============================================================================
void ARTDAQReaderProcessorBase::initLocalGroup(int rank) { configure(rank); }

#define ARTDAQ_FCL_PATH std::string(__ENV__("USER_DATA")) + "/" + "ARTDAQConfigurations/"
#define ARTDAQ_FILE_PREAMBLE "boardReader"

//==============================================================================
void ARTDAQReaderProcessorBase::configure(int rank)
{
	__CFG_COUT__ << "Configuring..." << __E__;

	// in the following block, we first destroy the existing BoardReader
	// instance, then create a new one.  Doing it in one step does not
	// produce the desired result since that creates a new instance and
	// then deletes the old one, and we need the opposite order.
	fragment_receiver_ptr_.reset(nullptr);
	__CFG_COUT__ << "New core" << __E__;
	my_rank  = rank;
	app_name = name_;
	fragment_receiver_ptr_.reset(new artdaq::BoardReaderApp());

	// FIXME These are passed as parameters
	//	should they come from the Configuration Tree?
	uint64_t timeout   = 45;
	uint64_t timestamp = 184467440737095516;
	__CFG_COUT__ << "Initializing '" << name_ << "'" << __E__;  //<< fhiclConfiguration_.to_string() << __E__;

	if(!fragment_receiver_ptr_->initialize(fhiclConfiguration_, timeout, timestamp))
	{
		__CFG_SS__ << "Error initializing '" << name_ << "' with ParameterSet = \n" << fhiclConfiguration_.to_string() << __E__;
		ss << "Here is the Board Reader report: \n" << fragment_receiver_ptr_->report("" /* or "transition_status" */) << __E__;
		__CFG_SS_THROW__;
	}
	__CFG_COUT__ << "Done Initializing." << __E__;

	// do any other configure steps here

	__CFG_COUT__ << "Configured." << __E__;
}  // end configure()

//==============================================================================
void ARTDAQReaderProcessorBase::halt(void)
{
	__CFG_COUT__ << "Halting..." << __E__;

	// FIXME These are passed as parameters
	//	should they come from the Configuration Tree?
	uint64_t timeout = 45;

	if(!fragment_receiver_ptr_->shutdown(timeout))
	{
		__CFG_SS__ << "Error shutting down '" << name_ << ".'" << __E__;
		__CFG_SS_THROW__;
	}

	__CFG_COUT__ << "Halted." << __E__;
}  // end halt()

//==============================================================================
void ARTDAQReaderProcessorBase::pause(void)
{
	__CFG_COUT__ << "Pausing..." << __E__;

	// FIXME These are passed as parameters
	//	should they come from the Configuration Tree?
	uint64_t timeout   = 45;
	uint64_t timestamp = 184467440737095516;

	if(!fragment_receiver_ptr_->pause(timeout, timestamp))
	{
		__CFG_SS__ << "Error pausing '" << name_ << ".'" << __E__;
		__CFG_SS_THROW__;
	}

	__CFG_COUT__ << "Paused." << __E__;
}  // end pause()

//==============================================================================
void ARTDAQReaderProcessorBase::resume(void)
{
	__CFG_COUT__ << "Resuming..." << __E__;

	// FIXME These are passed as parameters
	//	should they come from the Configuration Tree?
	uint64_t timeout   = 45;
	uint64_t timestamp = 184467440737095516;

	if(!fragment_receiver_ptr_->resume(timeout, timestamp))
	{
		__CFG_SS__ << "Error resuming '" << name_ << ".'" << __E__;
		__CFG_SS_THROW__;
	}

	__CFG_COUT__ << "Resumed." << __E__;
}  // end resume ()

//==============================================================================
void ARTDAQReaderProcessorBase::start(const std::string& runNumber)
{
	__CFG_COUT__ << "Starting..." << __E__;

	art::RunID runId((art::RunNumber_t)boost::lexical_cast<art::RunNumber_t>(runNumber));

	// FIXME These are passed as parameters
	//	should they come from the Configuration Tree?
	uint64_t timeout   = 45;
	uint64_t timestamp = 184467440737095516;

	__CFG_COUT__ << "Start run: " << runId << __E__;
	if(!fragment_receiver_ptr_->start(runId, timeout, timestamp))
	{
		__CFG_SS__ << "Error starting '" << name_ << "' for run number '" << runId << ",'" << __E__;
		__CFG_SS_THROW__;
	}

	__CFG_COUT__ << "Started." << __E__;
}  // end start()

//==============================================================================
void ARTDAQReaderProcessorBase::stop(void)
{
	__CFG_COUT__ << "Stop" << __E__;

	// FIXME These are passed as parameters
	//	should they come from the Configuration Tree?
	uint64_t timeout   = 45;
	uint64_t timestamp = 184467440737095516;

	auto sts = fragment_receiver_ptr_->status();
	if(sts == "Ready")
	{
		__CFG_COUT__ << "Already stopped - never started!" << __E__;
		return;  // Already stopped/never started
	}

	if(!fragment_receiver_ptr_->stop(timeout, timestamp))
	{
		__CFG_SS__ << "Error stopping '" << name_ << ".'" << __E__;
		__CFG_SS_THROW__;
	}

	__CFG_COUT__ << "Stopped." << __E__;
}  // end stop()
