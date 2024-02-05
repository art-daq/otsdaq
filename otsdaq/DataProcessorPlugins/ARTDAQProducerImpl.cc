
// #include "artdaq/Application/Commandable.hh"
// #include "fhiclcpp/make_ParameterSet.h"
// #include "otsdaq/DataManager/DataManager.h"
// #include "otsdaq/DataManager/DataManagerSingleton.h"
#include "otsdaq/DataProcessorPlugins/ARTDAQProducer.h"
// #include "otsdaq/Macros/CoutMacros.h"
// #include "otsdaq/MessageFacility/MessageFacility.h"
//
// #include <cstdint>
// #include <fstream>
// #include <iostream>
// #include <set>

using namespace ots;
//
// #define ARTDAQ_FCL_PATH std::string(__ENV__("USER_DATA")) + "/" +
//"ARTDAQConfigurations/" #define ARTDAQ_FILE_PREAMBLE "boardReader"

//==============================================================================
ARTDAQProducer::ARTDAQProducer(std::string              supervisorApplicationUID,
                               std::string              bufferUID,
                               std::string              processorUID,
                               const ConfigurationTree& theXDAQContextConfigTree,
                               const std::string&       configurationPath)
    : WorkLoop(processorUID)
    , DataProducer(supervisorApplicationUID, bufferUID, processorUID)
    , ARTDAQReaderProcessorBase(supervisorApplicationUID, bufferUID, processorUID, theXDAQContextConfigTree, configurationPath)
//    : WorkLoop(processorUID)
//    , DataProducer(supervisorApplicationUID, bufferUID, processorUID)
//    //
//    theXDAQContextConfigTree.getNode(configurationPath).getNode("BufferSize").getValue<unsigned
//    // int>())
//    , Configurable(theXDAQContextConfigTree, configurationPath)
{
	// Tell me why this printout does not happen
	__COUT__ << "ARTDAQ Producer constructed." << __E__;
	//	//__COUT__ << "Configuration string:-" <<
	//	//
	// theXDAQContextConfigTree.getNode(configurationPath).getNode("ConfigurationString").getValue<std::string>()
	//	//<< "-" << __E__;
	//
	//	std::string filename = ARTDAQ_FCL_PATH + ARTDAQ_FILE_PREAMBLE + "-";
	//	std::string uid      =
	// theXDAQContextConfigTree.getNode(configurationPath).getValue();
	//
	//	__COUT__ << "uid: " << uid << __E__;
	//	for(unsigned int i = 0; i < uid.size(); ++i)
	//		if((uid[i] >= 'a' && uid[i] <= 'z') || (uid[i] >= 'A' && uid[i] <= 'Z') ||
	//		   (uid[i] >= '0' && uid[i] <= '9'))  // only allow alpha numeric in file name
	//			filename += uid[i];
	//	filename += ".fcl";
	//
	//	__COUT__ << __E__;
	//	__COUT__ << __E__;
	//	__COUT__ << "filename: " << filename << __E__;
	//
	//	std::string fileFclString;
	//	{
	//		std::ifstream in(filename, std::ios::in | std::ios::binary);
	//		if(in)
	//		{
	//			std::string contents;
	//			in.seekg(0, std::ios::end);
	//			fileFclString.resize(in.tellg());
	//			in.seekg(0, std::ios::beg);
	//			in.read(&fileFclString[0], fileFclString.size());
	//			in.close();
	//		}
	//	}
	//	//__COUT__ << fileFclString << __E__;
	//
	//	// find fragment_receiver {
	//	// and insert e.g.,
	//	//	SupervisorApplicationUID:"ARTDataManager0"
	//	//	BufferUID:"ART_S0_DM0_DataBuffer0"
	//	//	ProcessorUID:"ART_S0_DM0_DB0_ARTConsumer0"
	//	size_t fcli =
	//	    fileFclString.find("fragment_receiver: {") + +strlen("fragment_receiver: {");
	//	if(fcli == std::string::npos)
	//	{
	//		__SS__ << "Could not find 'fragment_receiver: {' in Board Reader fcl string!"
	//		       << __E__;
	//		__COUT__ << "\n" << ss.str();
	//		__SS_THROW__;
	//	}
	//
	//	// get the parent IDs from configurationPath
	//	__COUT__ << "configurationPath " << configurationPath << __E__;
	//
	//	std::string  consumerID, bufferID, appID;
	//	unsigned int backSteps;  // at 2, 4, and 7 are the important parent IDs
	//	size_t       backi = -1, backj;
	//	backSteps          = 7;
	//	for(unsigned int i = 0; i < backSteps; i++)
	//	{
	//		//__COUT__ << "backsteps: " << i+1 << __E__;
	//
	//		backj = backi;
	//		backi = configurationPath.rfind('/', backi - 1);
	//
	//		//__COUT__ << "backi:" << backi << " backj:" << backj << __E__;
	//		//__COUT__ << "substr: " << configurationPath.substr(backi+1,backj-backi-1) <<
	//		// __E__;
	//
	//		if(i + 1 == 2)
	//			consumerID = configurationPath.substr(backi + 1, backj - backi - 1);
	//		else if(i + 1 == 4)
	//			bufferID = configurationPath.substr(backi + 1, backj - backi - 1);
	//		else if(i + 1 == 7)
	//			appID = configurationPath.substr(backi + 1, backj - backi - 1);
	//	}
	//
	//	// insert parent IDs into fcl string
	//	fileFclString = fileFclString.substr(0, fcli) + "\n\t\t" +
	//	                "SupervisorApplicationUID: \"" + appID + "\"\n\t\t" +
	//	                "BufferUID: \"" + bufferID + "\"\n\t\t" + "ProcessorUID: \"" +
	//	                consumerID + "\"\n" + fileFclString.substr(fcli);
	//
	//	__COUT__ << fileFclString << __E__;
	//
	//	fhicl::make_ParameterSet(fileFclString, fhiclConfiguration_);
	//
	//	//
	// fhicl::make_ParameterSet(theXDAQContextConfigTree.getNode(configurationPath).getNode("ConfigurationString").getValue<std::string>(),
	//	// fhiclConfiguration_);
}

//==============================================================================
// ARTDAQProducer::ARTDAQProducer(std::string interfaceID, MPI_Comm local_group_comm,
// std::string name) :FEVInterface     (feId, 0) ,local_group_comm_(local_group_comm)
//,name_            (name)
//{}

//==============================================================================
ARTDAQProducer::~ARTDAQProducer(void)
{
	halt();
	__COUT__ << "Destructor." << __E__;
}
//
////==============================================================================
// void ARTDAQProducer::initLocalGroup(int rank)
//{
//	name_ = "BoardReader_" + DataProducer::processorUID_;
//	configure(rank);
//}
//
// #define ARTDAQ_FCL_PATH std::string(__ENV__("USER_DATA")) + "/" +
//"ARTDAQConfigurations/" #define ARTDAQ_FILE_PREAMBLE "boardReader"
//
////==============================================================================
// void ARTDAQProducer::configure(int rank)
//{
//	__COUT__ << "\tConfigure" << __E__;
//
//	report_string_           = "";
//	external_request_status_ = true;
//
//	// in the following block, we first destroy the existing BoardReader
//	// instance, then create a new one.  Doing it in one step does not
//	// produce the desired result since that creates a new instance and
//	// then deletes the old one, and we need the opposite order.
//	fragment_receiver_ptr_.reset(nullptr);
//	__COUT__ << "\tNew core" << __E__;
//	my_rank  = rank;
//	app_name = name_;
//	fragment_receiver_ptr_.reset(new artdaq::BoardReaderApp());
//	// FIXME These are passed as parameters
//	uint64_t timeout = 45;
//	// uint64_t timestamp = 184467440737095516;
//	uint64_t timestamp = 184467440737095516;
//	__COUT__ << "\tInitialize: "
//	          << __E__;  //<< fhiclConfiguration_.to_string() << __E__;
//	external_request_status_ =
//	    fragment_receiver_ptr_->initialize(fhiclConfiguration_, timeout, timestamp);
//	__COUT__ << "\tDone Initialize" << __E__;
//	if(!external_request_status_)
//	{
//		report_string_ = "Error initializing ";
//		report_string_.append(name_ + " ");
//		report_string_.append("with ParameterSet = \"" + fhiclConfiguration_.to_string() +
//		                      "\".");
//	}
//	__COUT__ << "\tDone Configure" << __E__;
//}
//
////==============================================================================
// void ARTDAQProducer::halt(void)
//{
//	__COUT__ << "\tHalt" << __E__;
//	// FIXME These are passed as parameters
//	uint64_t timeout = 45;
//	// uint64_t timestamp = 184467440737095516;
//	report_string_           = "";
//	external_request_status_ = fragment_receiver_ptr_->shutdown(timeout);
//	if(!external_request_status_)
//	{
//		report_string_ = "Error shutting down ";
//		report_string_.append(name_ + ".");
//	}
//}
//
//==============================================================================
void ARTDAQProducer::pauseProcessingData(void) { ARTDAQReaderProcessorBase::pause(); }
//	__COUT__ << "\tPause" << __E__;
//	// FIXME These are passed as parameters
//	uint64_t timeout         = 45;
//	uint64_t timestamp       = 184467440737095516;
//	report_string_           = "";
//	external_request_status_ = fragment_receiver_ptr_->pause(timeout, timestamp);
//	if(!external_request_status_)
//	{
//		report_string_ = "Error pausing ";
//		report_string_.append(name_ + ".");
//	}
//}
//
//==============================================================================
void ARTDAQProducer::resumeProcessingData(void) { ARTDAQReaderProcessorBase::resume(); }
//	__COUT__ << "\tResume" << __E__;
//	// FIXME These are passed as parameters
//	uint64_t timeout         = 45;
//	uint64_t timestamp       = 184467440737095516;
//	report_string_           = "";
//	external_request_status_ = fragment_receiver_ptr_->resume(timeout, timestamp);
//	if(!external_request_status_)
//	{
//		report_string_ = "Error resuming ";
//		report_string_.append(name_ + ".");
//	}
//}
//
//==============================================================================
void ARTDAQProducer::startProcessingData(std::string runNumber) { ARTDAQReaderProcessorBase::start(runNumber); }
//	__COUT__ << "\tStart" << __E__;
//
//	art::RunID runId((art::RunNumber_t)boost::lexical_cast<art::RunNumber_t>(runNumber));
//
//	// FIXME These are passed as parameters
//	uint64_t timeout   = 45;
//	uint64_t timestamp = 184467440737095516;
//
//	report_string_ = "";
//	__COUT__ << "\tStart run: " << runId << __E__;
//	external_request_status_ = fragment_receiver_ptr_->start(runId, timeout, timestamp);
//	__COUT__ << "\tStart already crashed " << __E__;
//	if(!external_request_status_)
//	{
//		report_string_ = "Error starting ";
//		report_string_.append(name_ + " ");
//		report_string_.append("for run number ");
//		report_string_.append(boost::lexical_cast<std::string>(runId.run()));
//		report_string_.append(", timeout ");
//		report_string_.append(boost::lexical_cast<std::string>(timeout));
//		report_string_.append(", timestamp ");
//		report_string_.append(boost::lexical_cast<std::string>(timestamp));
//		report_string_.append(".");
//	}
//}
//
//==============================================================================
void ARTDAQProducer::stopProcessingData(void) { ARTDAQReaderProcessorBase::stop(); }
//	__COUT__ << "\tStop" << __E__;
//	// FIXME These are passed as parameters
//	uint64_t timeout   = 45;
//	uint64_t timestamp = 184467440737095516;
//	report_string_     = "";
//
//	auto sts = fragment_receiver_ptr_->status();
//	if(sts == "Ready")
//		return;  // Already stopped/never started
//
//	external_request_status_ = fragment_receiver_ptr_->stop(timeout, timestamp);
//	if(!external_request_status_)
//	{
//		report_string_ = "Error stopping ";
//		report_string_.append(name_ + ".");
//		// return false;
//	}
//}
//