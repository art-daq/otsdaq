#include "otsdaq/CoreSupervisors/FESupervisor.h"
#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/FECore/FEVInterfacesManager.h"
#include "otsdaq/TablePlugins/ARTDAQTableBase/ARTDAQTableBase.h"

#include "artdaq/DAQdata/Globals.hh"  // instantiates artdaq::Globals::metricMan_

#include <unistd.h>  //DIAG: for gettid() in FE macro latency investigation
#include <chrono>    //DIAG: for ms timestamps in FE macro latency investigation
#include <cstring>
#include <iostream>
#include <string>

namespace
{
// DIAG: temporary ms-resolution timestamp helper for FE macro latency investigation
inline uint64_t diagNowMs()
{
	return std::chrono::duration_cast<std::chrono::milliseconds>(
	           std::chrono::system_clock::now().time_since_epoch())
	    .count();
}
}  // namespace

#include "artdaq-core/Utilities/ExceptionHandler.hh" /*for artdaq::ExceptionHandler*/

/// https://cdcvs.fnal.gov/redmine/projects/artdaq/repository/revisions/develop/entry/artdaq/DAQdata/Globals.hh
/// for metric manager include
/// https://cdcvs.fnal.gov/redmine/projects/artdaq/repository/revisions/develop/entry/artdaq/Application/DataReceiverCore.cc
/// for metric manager configuration example
///
/// get pset from Board Reader metric manager table
///  try
///        {
///                metricMan->initialize(metric_pset, app_name);
///        }
///        catch (...)
///        {
///                ExceptionHandler(ExceptionHandlerRethrow::no,
///                                                 "Error loading metrics in
///                                                 DataReceiverCore::initialize()");
///        }
///..
///
/// metricMan->do_start();
/// ..
///  metricMan->shutdown();
///
using namespace ots;

XDAQ_INSTANTIATOR_IMPL(FESupervisor)

//==============================================================================
FESupervisor::FESupervisor(xdaq::ApplicationStub* stub)
    : CoreSupervisorBase(stub)
    , dp_context_{1}  // 1 I/O thread – sufficient for most tests
    , dp_socket_{dp_context_, zmq::socket_type::pub}
{
	__SUP_COUT__ << "Constructing..." << __E__;

	xoap::bind(this,
	           &FESupervisor::macroMakerSupervisorRequest,
	           "MacroMakerSupervisorRequest",
	           XDAQ_NS_URI);

	xoap::bind(
	    this, &FESupervisor::workLoopStatusRequest, "WorkLoopStatusRequest", XDAQ_NS_URI);

	xoap::bind(this,
	           &FESupervisor::frontEndCommunicationRequest,
	           "FECommunication",
	           XDAQ_NS_URI);

	std::string dataPublishingEndpoint;
	try
	{
		dataPublishingEndpoint = getSupervisorProperty(
		    "data_publishing_endpoint");  //like "tcp://0.0.0.0:$((OTS_MAIN_PORT-2))"
	}
	catch(const std::runtime_error& e)
	{
		__SUP_COUT__ << "Data publishing property 'data_publishing_endpoint' not set. "
		                "Defaulting to data publishing disabled."
		             << __E__;

		try
		{
			dataPublishingEndpoint = __ENV__(
			    "DATA_PUBLISHING_ENDPOINT");  //like "tcp://0.0.0.0:$((OTS_MAIN_PORT-2))"
		}
		catch(const std::runtime_error& e)
		{
			__SUP_COUT__
			    << "Data publishing environment variable 'DATA_PUBLISHING_ENDPOINT' "
			       "not set. Defaulting to data publishing disabled."
			    << __E__;
		}
	}
	__SUP_COUT_INFO__ << "dataPublishingEndpoint = " << dataPublishingEndpoint << __E__;

	if(dataPublishingEndpoint.size())
	{
		__SUP_COUT__ << "Initializing data publishing to endpoint '"
		             << dataPublishingEndpoint << "'..." << __E__;

		initDataPublishing(dataPublishingEndpoint,
		                   getSupervisorProperty("data_publishing_topic", ""));
	}

	try
	{
		CoreSupervisorBase::theStateMachineImplementation_.push_back(
		    new FEVInterfacesManager(
		        CorePropertySupervisorBase::getContextTreeNode(),
		        CorePropertySupervisorBase::getSupervisorConfigurationPath()));
	}
	catch(...)
	{
		if(CorePropertySupervisorBase::allSupervisorInfo_.isMacroMakerMode())
		{
			__SUP_COUT_WARN__ << "Error caught constructing FE Interface Manager. In "
			                     "Macro Maker mode, the input fhicl defines the "
			                     "configuration tree, make sure you specified a valid "
			                     "fcl file path."
			                  << __E__;
			try
			{
				throw;
			}
			catch(const std::runtime_error& e)
			{
				__SUP_COUT_WARN__ << e.what() << __E__;
				throw;
			}
		}
		throw;
	}

	extractFEInterfacesManager();

	__SUP_COUT__ << "Constructed." << __E__;

	if(CorePropertySupervisorBase::allSupervisorInfo_.isMacroMakerMode())
	{
		__SUP_COUT_INFO__ << "Macro Maker mode, so configuring at startup!" << __E__;
		if(!theFEInterfacesManager_)
		{
			__SUP_SS__ << "Missing FE Interface manager!" << __E__;
			__SUP_SS_THROW__;
		}

		// copied from CoreSupervisorBase::transitionConfiguring()

		// Now that the configuration manager has all the necessary configurations,
		//	create all objects that depend on the configuration (the first iteration)

		try
		{
			__SUP_COUT__ << "Configuring all state machine implementations..." << __E__;
			__SUP_COUTV__(stateMachinesIterationDone_.size());
			preStateMachineExecutionLoop();
			__SUP_COUTV__(stateMachinesIterationDone_.size());
			for(unsigned int i = 0; i < theStateMachineImplementation_.size(); ++i)
			{
				__SUP_COUT__ << "Configuring state machine i " << i << __E__;

				// if one state machine is doing a sub-iteration, then target that one
				if(subIterationWorkStateMachineIndex_ != (unsigned int)-1 &&
				   i != subIterationWorkStateMachineIndex_)
					continue;  // skip those not in the sub-iteration

				if(stateMachinesIterationDone_[i])
					continue;  // skip state machines already done

				preStateMachineExecution(i);
				theStateMachineImplementation_[i]->parentSupervisor_ =
				    this;  // for backwards compatibility, kept out of configure
				           // parameters
				theStateMachineImplementation_[i]->configure();  // e.g. for FESupervisor,
				// this is configure of
				// FEVInterfacesManager
				postStateMachineExecution(i);
			}
			postStateMachineExecutionLoop();

			//ony indicate alive after configure
			CorePropertySupervisorBase::indicateOtsAlive(0);
		}
		catch(const std::runtime_error& e)
		{
			__SUP_SS__ << "Error was caught while configuring: " << e.what() << __E__;
			__SUP_COUT_ERR__ << "\n" << ss.str();
			throw;
		}
		catch(...)
		{
			__SUP_SS__
			    << "Unknown error was caught while configuring. Please checked the logs."
			    << __E__;
			try
			{
				throw;
			}  //one more try to printout extra info
			catch(const std::exception& e)
			{
				ss << "Exception message: " << e.what();
			}
			catch(...)
			{
			}
			__SUP_COUT_ERR__ << "\n" << ss.str();
			throw;
		}
	}  // end Macro Maker mode initial configure
}  // end constructor

//==============================================================================
FESupervisor::~FESupervisor(void)
{
	__SUP_COUT__ << "Destroying..." << __E__;
	// theStateMachineImplementation_ is reset and the object it points to deleted in
	// ~CoreSupervisorBase()

	// Destructor must never throw, so we swallow any zmq_error.
	try
	{
		closeDataPublishing();
	}
	catch(...)
	{
		// nothing – best‑effort cleanup.
	}

	artdaq::Globals::CleanUpGlobals();  // destruct metricManager (among other things)

	__SUP_COUT__ << "Destructed." << __E__;
}  // end destructor

//==============================================================================
xoap::MessageReference FESupervisor::frontEndCommunicationRequest(
    xoap::MessageReference message)
try
{
	__SUP_COUT_INFO__ << "DIAG ms=" << diagNowMs() << " tid=" << gettid()
	                  << " frontEndCommunicationRequest received: "
	                  << SOAPUtilities::translate(message) << __E__;

	if(!theFEInterfacesManager_)
	{
		__SUP_SS__ << "No FE Interface Manager!" << __E__;
		__SUP_SS_THROW__;
	}
	SOAPParameters typeParameter, rxParameters;  // params for xoap to recv
	typeParameter.addParameter("type");
	SOAPUtilities::receive(message, typeParameter);

	std::string type = typeParameter.getValue("type");

	// types
	//	feSend
	//	feMacro
	//	feMacroMultiDimensionalStart
	//	macroMultiDimensionalStart
	//	feMacroMultiDimensionalCheck
	//	macroMultiDimensionalCheck

	rxParameters.addParameter("requester");
	rxParameters.addParameter("targetInterfaceID");

	if(type == "feSend")
	{
		__SUP_COUTV__(type);

		rxParameters.addParameter("value");
		SOAPUtilities::receive(message, rxParameters);

		std::string requester         = rxParameters.getValue("requester");
		std::string targetInterfaceID = rxParameters.getValue("targetInterfaceID");
		std::string value             = rxParameters.getValue("value");

		__SUP_COUTV__(requester);
		__SUP_COUTV__(targetInterfaceID);
		__SUP_COUTV__(value);

		// test that the interface exists
		theFEInterfacesManager_->getFEInterface(targetInterfaceID);

		// mutex scope
		{
			std::lock_guard<std::mutex> lock(
			    theFEInterfacesManager_->frontEndCommunicationReceiveMutex_);

			theFEInterfacesManager_
			    ->frontEndCommunicationReceiveBuffer_[targetInterfaceID][requester]
			    .emplace(value);

			__SUP_COUT__ << "Number of target interface ID '" << targetInterfaceID
			             << "' buffers: "
			             << theFEInterfacesManager_
			                    ->frontEndCommunicationReceiveBuffer_[targetInterfaceID]
			                    .size()
			             << __E__;
			__SUP_COUT__
			    << "Number of source interface ID '" << requester << "' values received: "
			    << theFEInterfacesManager_
			           ->frontEndCommunicationReceiveBuffer_[targetInterfaceID][requester]
			           .size()
			    << __E__;
		}
		return SOAPUtilities::makeSOAPMessageReference("Received");
	}  // end type feSend
	else if(type == "feMacro")
	{
		__SUP_COUTV__(type);

		rxParameters.addParameter("feMacroName");
		rxParameters.addParameter("inputArgs");

		SOAPUtilities::receive(message, rxParameters);

		std::string requester         = rxParameters.getValue("requester");
		std::string targetInterfaceID = rxParameters.getValue("targetInterfaceID");
		std::string feMacroName       = rxParameters.getValue("feMacroName");
		std::string inputArgs         = rxParameters.getValue("inputArgs");

		__SUP_COUTV__(requester);
		__SUP_COUTV__(targetInterfaceID);
		__SUP_COUTV__(feMacroName);
		__SUP_COUTV__(inputArgs);

		std::string outputArgs;
		try
		{
			__SUP_COUT__ << "DIAG ms=" << diagNowMs() << " runFEMacroByFE starting, macro='"
			             << feMacroName << "' target='" << targetInterfaceID << "'" << __E__;
			theFEInterfacesManager_->runFEMacroByFE(
			    requester, targetInterfaceID, feMacroName, inputArgs, outputArgs);
			__SUP_COUT__ << "DIAG ms=" << diagNowMs() << " runFEMacroByFE done, macro='"
			             << feMacroName << "'" << __E__;
		}
		catch(std::runtime_error& e)
		{
			__SUP_SS__ << "In Supervisor with LID="
			           << getApplicationDescriptor()->getLocalId()
			           << " the FE Macro named '" << feMacroName << "' with target FE '"
			           << targetInterfaceID << "' failed. Here is the error:\n\n"
			           << e.what() << __E__;
			__SUP_SS_THROW__;
		}
		catch(...)
		{
			__SUP_SS__ << "In Supervisor with LID="
			           << getApplicationDescriptor()->getLocalId()
			           << " the FE Macro named '" << feMacroName << "' with target FE '"
			           << targetInterfaceID << "' failed due to an unknown error."
			           << __E__;
			try
			{
				throw;
			}  //one more try to printout extra info
			catch(const std::exception& e)
			{
				ss << "Exception message: " << e.what();
			}
			catch(...)
			{
			}
			__SUP_SS_THROW__;
		}

		__SUP_COUTV__(outputArgs);

		xoap::MessageReference replyMessage =
		    SOAPUtilities::makeSOAPMessageReference("feMacrosResponse");
		SOAPParameters txParameters;
		txParameters.addParameter("requester", requester);
		txParameters.addParameter("targetInterfaceID", targetInterfaceID);
		txParameters.addParameter("feMacroName", feMacroName);
		txParameters.addParameter("outputArgs", outputArgs);
		SOAPUtilities::addParameters(replyMessage, txParameters);

		__SUP_COUT__ << "DIAG ms=" << diagNowMs() << " Sending FE macro result: "
		             << SOAPUtilities::translate(replyMessage) << __E__;

		return replyMessage;
	}                                                  // end type feMacro
	else if(type == "feMacroMultiDimensionalStart" ||  // from iterator
	        type == "macroMultiDimensionalStart")      // from iterator
	{
		__SUP_COUTV__(type);

		if(type[0] == 'm')
		{
			rxParameters.addParameter("macroString");
			rxParameters.addParameter("macroName");
		}
		else
			rxParameters.addParameter("feMacroName");

		rxParameters.addParameter("enableSavingOutput");
		rxParameters.addParameter("outputFilePath");
		rxParameters.addParameter("outputFileRadix");
		rxParameters.addParameter("inputArgs");

		SOAPUtilities::receive(message, rxParameters);

		std::string requester         = rxParameters.getValue("requester");
		std::string targetInterfaceID = rxParameters.getValue("targetInterfaceID");
		std::string macroName, macroString;
		if(type[0] == 'm')
		{
			macroName   = rxParameters.getValue("macroName");
			macroString = rxParameters.getValue("macroString");
			__SUP_COUTV__(macroString);
		}
		else
			macroName = rxParameters.getValue("feMacroName");
		bool enableSavingOutput     = rxParameters.getValue("enableSavingOutput") == "1";
		std::string outputFilePath  = rxParameters.getValue("outputFilePath");
		std::string outputFileRadix = rxParameters.getValue("outputFileRadix");
		std::string inputArgs       = rxParameters.getValue("inputArgs");

		__SUP_COUTV__(requester);
		__SUP_COUTV__(targetInterfaceID);
		__SUP_COUTV__(macroName);
		__SUP_COUTV__(enableSavingOutput);
		__SUP_COUTV__(outputFilePath);
		__SUP_COUTV__(outputFileRadix);
		__SUP_COUTV__(inputArgs);

		if(type[0] == 'm')  // start Macro
		{
			try
			{
				theFEInterfacesManager_->startMacroMultiDimensional(requester,
				                                                    targetInterfaceID,
				                                                    macroName,
				                                                    macroString,
				                                                    enableSavingOutput,
				                                                    outputFilePath,
				                                                    outputFileRadix,
				                                                    inputArgs);
			}
			catch(std::runtime_error& e)
			{
				__SUP_SS__ << "In Supervisor with LID="
				           << getApplicationDescriptor()->getLocalId()
				           << " the Macro named '" << macroName << "' with target FE '"
				           << targetInterfaceID
				           << "' failed to start multi-dimensional launch. "
				           << "Here is the error:\n\n"
				           << e.what() << __E__;
				__SUP_SS_THROW__;
			}
			catch(...)
			{
				__SUP_SS__ << "In Supervisor with LID="
				           << getApplicationDescriptor()->getLocalId()
				           << " the Macro named '" << macroName << "' with target FE '"
				           << targetInterfaceID
				           << "' failed to start multi-dimensional launch "
				           << "due to an unknown error." << __E__;
				try
				{
					throw;
				}  //one more try to printout extra info
				catch(const std::exception& e)
				{
					ss << "Exception message: " << e.what();
				}
				catch(...)
				{
				}
				__SUP_SS_THROW__;
			}
		}
		else  // start FE Macro
		{
			try
			{
				theFEInterfacesManager_->startFEMacroMultiDimensional(requester,
				                                                      targetInterfaceID,
				                                                      macroName,
				                                                      enableSavingOutput,
				                                                      outputFilePath,
				                                                      outputFileRadix,
				                                                      inputArgs);
			}
			catch(std::runtime_error& e)
			{
				__SUP_SS__ << "In Supervisor with LID="
				           << getApplicationDescriptor()->getLocalId()
				           << " the FE Macro named '" << macroName << "' with target FE '"
				           << targetInterfaceID
				           << "' failed to start multi-dimensional launch. "
				           << "Here is the error:\n\n"
				           << e.what() << __E__;
				__SUP_SS_THROW__;
			}
			catch(...)
			{
				__SUP_SS__ << "In Supervisor with LID="
				           << getApplicationDescriptor()->getLocalId()
				           << " the FE Macro named '" << macroName << "' with target FE '"
				           << targetInterfaceID
				           << "' failed to start multi-dimensional launch "
				           << "due to an unknown error." << __E__;
				try
				{
					throw;
				}  //one more try to printout extra info
				catch(const std::exception& e)
				{
					ss << "Exception message: " << e.what();
				}
				catch(...)
				{
				}
				__SUP_SS_THROW__;
			}
		}

		xoap::MessageReference replyMessage =
		    SOAPUtilities::makeSOAPMessageReference(type + "Done");
		SOAPParameters txParameters;
		// txParameters.addParameter("started", "1");
		SOAPUtilities::addParameters(replyMessage, txParameters);

		__SUP_COUT__ << "Sending FE macro result: "
		             << SOAPUtilities::translate(replyMessage) << __E__;

		return replyMessage;
	}  // end type (fe)MacroMultiDimensionalStart
	else if(type == "feMacroMultiDimensionalCheck" ||  // from iterator
	        type == "macroMultiDimensionalCheck")
	{
		// LORE__SUP_COUTV__(type);
		if(type[0] == 'm')
			rxParameters.addParameter("macroName");
		else
			rxParameters.addParameter("feMacroName");
		rxParameters.addParameter("targetInterfaceID");

		SOAPUtilities::receive(message, rxParameters);

		std::string targetInterfaceID = rxParameters.getValue("targetInterfaceID");
		std::string macroName;
		if(type[0] == 'm')
			macroName = rxParameters.getValue("macroName");
		else
			macroName = rxParameters.getValue("feMacroName");

		// LORE__SUP_COUTV__(targetInterfaceID);
		// LORE__SUP_COUTV__(macroName);

		bool done = false;
		try
		{
			done = theFEInterfacesManager_->checkMacroMultiDimensional(targetInterfaceID,
			                                                           macroName);
		}
		catch(std::runtime_error& e)
		{
			__SUP_SS__ << "In Supervisor with LID="
			           << getApplicationDescriptor()->getLocalId()
			           << " the FE Macro named '" << macroName << "' with target FE '"
			           << targetInterfaceID
			           << "' failed to check multi-dimensional launch. "
			           << "Here is the error:\n\n"
			           << e.what() << __E__;
			__SUP_SS_THROW__;
		}
		catch(...)
		{
			__SUP_SS__ << "In Supervisor with LID="
			           << getApplicationDescriptor()->getLocalId()
			           << " the FE Macro named '" << macroName << "' with target FE '"
			           << targetInterfaceID
			           << "' failed to check multi-dimensional launch "
			           << "due to an unknown error." << __E__;
			try
			{
				throw;
			}  //one more try to printout extra info
			catch(const std::exception& e)
			{
				ss << "Exception message: " << e.what();
			}
			catch(...)
			{
			}
			__SUP_SS_THROW__;
		}

		xoap::MessageReference replyMessage =
		    SOAPUtilities::makeSOAPMessageReference(type + "Done");
		SOAPParameters txParameters;
		txParameters.addParameter("Done", done ? "1" : "0");
		SOAPUtilities::addParameters(replyMessage, txParameters);

		// LORE__SUP_COUT__ << "Sending FE macro result: " << SOAPUtilities::translate(replyMessage) << __E__;

		return replyMessage;
	}  // end type (fe)MacroMultiDimensionalCheck
	else
	{
		__SUP_SS__ << "Unrecognized FE Communication type: " << type << __E__;
		__SUP_SS_THROW__;
	}
}
catch(const std::runtime_error& e)
{
	__SUP_SS__ << "Error encountered processing FE communication request: " << e.what()
	           << __E__;
	__SUP_COUT_ERR__ << ss.str();

	SOAPParameters parameters;
	parameters.addParameter("Error", ss.str());
	return SOAPUtilities::makeSOAPMessageReference(
	    supervisorClassNoNamespace_ + "FailFECommunicationRequest", parameters);
}
catch(...)
{
	__SUP_SS__ << "Unknown error encountered processing FE communication request."
	           << __E__;
	try
	{
		throw;
	}  //one more try to printout extra info
	catch(const std::exception& e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...)
	{
	}
	__SUP_COUT_ERR__ << ss.str();

	SOAPParameters parameters;
	parameters.addParameter("Error", ss.str());
	return SOAPUtilities::makeSOAPMessageReference(
	    supervisorClassNoNamespace_ + "FailFECommunicationRequest", parameters);
}  // end frontEndCommunicationRequest()

//==============================================================================
/// macroMakerSupervisorRequest
///	 Handles all MacroMaker Requests:
///		- GetInterfaces (returns interface type and id)
///
///	Note: this code assumes a CoreSupervisorBase has only one
///		FEVInterfacesManager in its vector of state machines
xoap::MessageReference FESupervisor::macroMakerSupervisorRequest(
    xoap::MessageReference message)
{
	__SUP_COUT__ << "$$$$$$$$$$$$$$$$$" << __E__;

	// receive request parameters
	SOAPParameters parameters;
	parameters.addParameter("Request");

	__SUP_COUT__ << "DIAG ms=" << diagNowMs() << " tid=" << gettid()
	             << " Received Macro Maker message: "
	             << SOAPUtilities::translate(message) << __E__;

	SOAPUtilities::receive(message, parameters);
	std::string request = parameters.getValue("Request");

	__SUP_COUT__ << "request: " << request << __E__;

	// request types:
	//	GetInterfaces
	//	UniversalWrite
	//	UniversalRead
	//	GetInterfaceMacros
	//	RunInterfaceMacro
	//	RunMacroMakerMacro

	SOAPParameters retParameters;

	try
	{
		if(request == "GetInterfaces")
		{
			if(theFEInterfacesManager_)
				retParameters.addParameter(
				    "FEList",
				    theFEInterfacesManager_->getFEListString(
				        std::to_string(getApplicationDescriptor()->getLocalId())));
			else  // if no FE interfaces, return empty string
				retParameters.addParameter("FEList", "");

			// if errors in state machine, send also
			if(theStateMachine_.getErrorMessage() != "")
				retParameters.addParameter("frontEndError",
				                           theStateMachine_.getErrorMessage());

			return SOAPUtilities::makeSOAPMessageReference(
			    supervisorClassNoNamespace_ + "Response", retParameters);
		}
		else if(request == "UniversalWrite")
		{
			if(!theFEInterfacesManager_)
			{
				__SUP_SS__ << "No FE Interface Manager! Are you configured?" << __E__;
				__SUP_SS_THROW__;
			}
			// params for running macros
			SOAPParameters requestParameters;
			requestParameters.addParameter("InterfaceID");
			requestParameters.addParameter("Address");
			requestParameters.addParameter("Data");
			SOAPUtilities::receive(message, requestParameters);
			std::string interfaceID = requestParameters.getValue("InterfaceID");
			std::string addressStr  = requestParameters.getValue("Address");
			std::string dataStr     = requestParameters.getValue("Data");

			__SUP_COUT__ << "Address: " << addressStr << " Data: " << dataStr
			             << " InterfaceID: " << interfaceID << __E__;

			// parameters interface index!
			// unsigned int index = stoi(indexStr);	// As long as the supervisor has only
			// one interface, this index will remain 0?

			__SUP_COUT__
			    << "theFEInterfacesManager_->getInterfaceUniversalAddressSize(index) "
			    << theFEInterfacesManager_->getInterfaceUniversalAddressSize(interfaceID)
			    << __E__;
			__SUP_COUT__
			    << "theFEInterfacesManager_->getInterfaceUniversalDataSize(index) "
			    << theFEInterfacesManager_->getInterfaceUniversalDataSize(interfaceID)
			    << __E__;

			// Converting std::string to char*
			// char address

			char tmpHex[3];  // for use converting hex to binary
			tmpHex[2] = '\0';

			__SUP_COUT__ << "Translating address: ";

			std::string addressTmp;
			addressTmp.reserve(
			    theFEInterfacesManager_->getInterfaceUniversalAddressSize(interfaceID));
			char* address = &addressTmp[0];

			if(addressStr.size() % 2)  // if odd, make even
				addressStr = "0" + addressStr;
			unsigned int i = 0;
			for(; i < addressStr.size() &&
			      i / 2 < theFEInterfacesManager_->getInterfaceUniversalAddressSize(
			                  interfaceID);
			    i += 2)
			{
				tmpHex[0] = addressStr[addressStr.size() - 1 - i - 1];
				tmpHex[1] = addressStr[addressStr.size() - 1 - i];
				sscanf(tmpHex, "%hhX", (unsigned char*)&address[i / 2]);
				printf("%2.2X", (unsigned char)address[i / 2]);
			}
			// finish and fill with 0s
			for(; i / 2 <
			      theFEInterfacesManager_->getInterfaceUniversalAddressSize(interfaceID);
			    i += 2)
			{
				address[i / 2] = 0;
				printf("%2.2X", (unsigned char)address[i / 2]);
			}

			std::cout << __E__;

			__SUP_COUT__ << "Translating data: ";

			std::string dataTmp;
			dataTmp.reserve(
			    theFEInterfacesManager_->getInterfaceUniversalDataSize(interfaceID));
			char* data = &dataTmp[0];

			if(dataStr.size() % 2)  // if odd, make even
				dataStr = "0" + dataStr;

			i = 0;
			for(; i < dataStr.size() &&
			      i / 2 <
			          theFEInterfacesManager_->getInterfaceUniversalDataSize(interfaceID);
			    i += 2)
			{
				tmpHex[0] = dataStr[dataStr.size() - 1 - i - 1];
				tmpHex[1] = dataStr[dataStr.size() - 1 - i];
				sscanf(tmpHex, "%hhX", (unsigned char*)&data[i / 2]);
				printf("%2.2X", (unsigned char)data[i / 2]);
			}
			// finish and fill with 0s
			for(; i / 2 <
			      theFEInterfacesManager_->getInterfaceUniversalDataSize(interfaceID);
			    i += 2)
			{
				data[i / 2] = 0;
				printf("%2.2X", (unsigned char)data[i / 2]);
			}

			std::cout << __E__;

			// char* address = new char[addressStr.size() + 1];
			// std::copy(addressStr.begin(), addressStr.end(), address);
			//    	address[addressStr.size()] = '\0';
			//    	char* data = new char[dataStr.size() + 1];
			//		std::copy(dataStr.begin(), dataStr.end(), data);
			//		data[dataStr.size()] = '\0';

			theFEInterfacesManager_->universalWrite(interfaceID, address, data);

			// delete[] address;
			// delete[] data;

			return SOAPUtilities::makeSOAPMessageReference(
			    supervisorClassNoNamespace_ + "DataWritten", retParameters);
		}
		else if(request == "UniversalRead")
		{
			if(!theFEInterfacesManager_)
			{
				__SUP_SS__ << "No FE Interface Manager! Are you configured?" << __E__;
				__SUP_SS_THROW__;
			}

			// params for running macros
			SOAPParameters requestParameters;
			requestParameters.addParameter("InterfaceID");
			requestParameters.addParameter("Address");
			SOAPUtilities::receive(message, requestParameters);
			std::string interfaceID = requestParameters.getValue("InterfaceID");
			std::string addressStr  = requestParameters.getValue("Address");

			__SUP_COUT__ << "Address: " << addressStr << " InterfaceID: " << interfaceID
			             << __E__;

			// parameters interface index!
			// parameter address and data
			// unsigned int index = stoi(indexStr);	// As long as the supervisor has only
			// one interface, this index will remain 0?

			__SUP_COUT__
			    << "theFEInterfacesManager_->getInterfaceUniversalAddressSize(index) "
			    << theFEInterfacesManager_->getInterfaceUniversalAddressSize(interfaceID)
			    << __E__;
			__SUP_COUT__
			    << "theFEInterfacesManager_->getInterfaceUniversalDataSize(index) "
			    << theFEInterfacesManager_->getInterfaceUniversalDataSize(interfaceID)
			    << __E__;

			char tmpHex[3];  // for use converting hex to binary
			tmpHex[2] = '\0';

			__SUP_COUT__ << "Translating address: ";

			std::string addressTmp;
			addressTmp.reserve(
			    theFEInterfacesManager_->getInterfaceUniversalAddressSize(interfaceID));
			char* address = &addressTmp[0];

			if(addressStr.size() % 2)  // if odd, make even
				addressStr = "0" + addressStr;

			unsigned int i = 0;
			for(; i < addressStr.size() &&
			      i / 2 < theFEInterfacesManager_->getInterfaceUniversalAddressSize(
			                  interfaceID);
			    i += 2)
			{
				tmpHex[0] = addressStr[addressStr.size() - 1 - i - 1];
				tmpHex[1] = addressStr[addressStr.size() - 1 - i];
				sscanf(tmpHex, "%hhX", (unsigned char*)&address[i / 2]);
				printf("%2.2X", (unsigned char)address[i / 2]);
			}
			// finish and fill with 0s
			for(; i / 2 <
			      theFEInterfacesManager_->getInterfaceUniversalAddressSize(interfaceID);
			    i += 2)
			{
				address[i / 2] = 0;
				printf("%2.2X", (unsigned char)address[i / 2]);
			}

			std::cout << __E__;

			unsigned int dataSz =
			    theFEInterfacesManager_->getInterfaceUniversalDataSize(interfaceID);
			std::string dataStr;
			dataStr.resize(dataSz);
			char* data = &dataStr[0];

			//    	std::string result =
			//    theFEInterfacesManager_->universalRead(index,address,data);
			//    	__SUP_COUT__<< result << __E__ << __E__;

			try
			{
				theFEInterfacesManager_->universalRead(interfaceID, address, data);
			}
			catch(const std::runtime_error& e)
			{
				// do not allow read exception to crash everything when a macromaker
				// command
				__SUP_COUT_ERR__ << "Exception caught during read: " << e.what() << __E__;
				retParameters.addParameter("dataResult", "Time Out Error");
				return SOAPUtilities::makeSOAPMessageReference(
				    supervisorClassNoNamespace_ + "aa", retParameters);
			}
			catch(...)
			{
				// do not allow read exception to crash everything when a macromaker
				// command
				__SUP_COUT_ERR__ << "Exception caught during read." << __E__;
				retParameters.addParameter("dataResult", "Time Out Error");
				return SOAPUtilities::makeSOAPMessageReference(
				    supervisorClassNoNamespace_ + "aa", retParameters);
			}

			// if dataSz is less than 8 show what the unsigned number would be
			if(dataSz <= 8)
			{
				std::string str8(data);
				str8.resize(8);
				__SUP_COUT__ << "decResult[" << dataSz
				             << " bytes]: " << *((unsigned long long*)(&str8[0]))
				             << __E__;
			}

			std::string hexResultStr;
			hexResultStr.reserve(dataSz * 2 + 1);
			char* hexResult = &hexResultStr[0];
			// go through each byte and convert it to hex value (i.e. 2 0-F chars)
			// go backwards through source data since should be provided in host order
			//	(i.e. a cast to unsigned long long should acquire expected value)
			for(unsigned int i = 0; i < dataSz; ++i)
			{
				sprintf(&hexResult[i * 2], "%2.2X", (unsigned char)data[dataSz - 1 - i]);
			}

			__SUP_COUT__ << "hexResult[" << strlen(hexResult)
			             << " nibbles]: " << std::string(hexResult) << __E__;

			retParameters.addParameter("dataResult", hexResult);
			return SOAPUtilities::makeSOAPMessageReference(
			    supervisorClassNoNamespace_ + "aa", retParameters);
		}
		else if(request == "GetInterfaceMacros")
		{
			if(theFEInterfacesManager_)
			{
				__SUP_COUT__ << "Getting FE Macros from FE Interface Manager..." << __E__;
				retParameters.addParameter(
				    "FEMacros",
				    theFEInterfacesManager_->getFEMacrosString(
				        CorePropertySupervisorBase::getSupervisorUID(),
				        std::to_string(CoreSupervisorBase::getSupervisorLID())));
			}
			else
			{
				__SUP_COUT__ << "No FE Macros because there is no FE Interface Manager..."
				             << __E__;
				retParameters.addParameter("FEMacros", "");
			}

			return SOAPUtilities::makeSOAPMessageReference(
			    supervisorClassNoNamespace_ + "Response", retParameters);
		}
		else if(request == "RunInterfaceMacro")
		{
			if(!theFEInterfacesManager_)
			{
				__SUP_SS__ << "Missing FE Interface Manager! Are you configured?"
				           << __E__;
				__SUP_SS_THROW__;
			}

			// params for running macros
			SOAPParameters requestParameters;
			requestParameters.addParameter("feMacroName");
			requestParameters.addParameter("inputArgs");
			requestParameters.addParameter("outputArgs");
			requestParameters.addParameter("InterfaceID");
			requestParameters.addParameter("userPermissions");
			requestParameters.addParameter("AsyncSupported");
			SOAPUtilities::receive(message, requestParameters);
			std::string interfaceID     = requestParameters.getValue("InterfaceID");
			std::string feMacroName     = requestParameters.getValue("feMacroName");
			std::string inputArgs       = requestParameters.getValue("inputArgs");
			std::string outputArgs      = requestParameters.getValue("outputArgs");
			std::string userPermissions = requestParameters.getValue("userPermissions");
			bool asyncSupported = requestParameters.getValue("AsyncSupported") == "1";

			// check user permission
			//  userPermissions = "allUsers:1";
			//  userPermissions = "allUsers:1 & TDAQ:255";
			__COUTV__(userPermissions);
			std::map<std::string, WebUsers::permissionLevel_t> userPermissionLevelsMap;
			CorePropertySupervisorBase::extractPermissionsMapFromString(
			    userPermissions, userPermissionLevelsMap);
			__COUTV__(StringMacros::mapToString(userPermissionLevelsMap));

			// outputArgs must be filled with the proper argument names
			//	and then the response output values will be returned in the string.

			// check for interfaceID
			FEVInterface* fe = theFEInterfacesManager_->getFEInterfaceP(interfaceID);

			// have pointer to virtual FEInterface, find Macro structure
			auto FEMacroIt = fe->getMapOfFEMacroFunctions().find(feMacroName);
			if(FEMacroIt == fe->getMapOfFEMacroFunctions().end())
			{
				__SUP_SS__ << "FE Macro '" << feMacroName << "' of interfaceID '"
				           << interfaceID << "' was not found!" << __E__;
				__SUP_SS_THROW__;
			}
			const FEVInterface::frontEndMacroStruct_t& FEMacro = FEMacroIt->second;

			__COUTV__(FEMacro.requiredUserPermissions_);
			std::map<std::string, WebUsers::permissionLevel_t>
			    FERequiredUserPermissionsMap;
			CorePropertySupervisorBase::extractPermissionsMapFromString(
			    FEMacro.requiredUserPermissions_, FERequiredUserPermissionsMap);
			__COUTV__(StringMacros::mapToString(FERequiredUserPermissionsMap));

			if(!CorePropertySupervisorBase::doPermissionsGrantAccess(
			       userPermissionLevelsMap, FERequiredUserPermissionsMap))
			{
				__SUP_SS__ << "Invalid user permission for FE Macro '" << feMacroName
				           << "' of interfaceID '" << interfaceID << "'!\n\n"
				           << "Must have access level of at least '"
				           << StringMacros::mapToString(FERequiredUserPermissionsMap)
				           << ".' Users permissions level is only '" << userPermissions
				           << ".'" << __E__;
				__SUP_SS_THROW__;
			};  // skip icon if no access

			if(asyncSupported)
			{
				// Async path: launch macro in a thread, return NotDoneTaskID immediately
				auto task = std::make_shared<AsyncMacroTask>();
				{
					std::lock_guard<std::mutex> lock(asyncMacroMutex_);
					task->taskID = ++asyncMacroTaskIDCounter_;
					if(asyncMacroTaskIDCounter_ == 0)
						task->taskID = ++asyncMacroTaskIDCounter_;
					task->interfaceID = interfaceID;
					task->startTime   = time(0);
					asyncMacroTasks_.push_back(task);
				}

				FEVInterface::frontEndMacroStruct_t localFEMacro = FEMacro;
				std::thread([this,
				             task,
				             interfaceID,
				             localFEMacro,
				             inputArgs,
				             outputArgs]() mutable {
					{
						std::lock_guard<std::mutex> lock(asyncMacroMutex_);
						task->threadID = std::this_thread::get_id();
					}
					// Clear stale progress in case thread ID was reused
					try
					{
						theFEInterfacesManager_->getFEInterfaceP(interfaceID)
						    ->clearFEMacroPercentDone(std::this_thread::get_id());
					}
					catch(...)
					{
					}

					try
					{
						__COUT__ << "DIAG ms=" << diagNowMs()
						         << " async FE Macro thread starting runFEMacro, taskID="
						         << task->taskID << __E__;
						theFEInterfacesManager_->runFEMacro(
						    interfaceID, localFEMacro, inputArgs, outputArgs);
						__COUT__ << "DIAG ms=" << diagNowMs()
						         << " async FE Macro thread finished runFEMacro, taskID="
						         << task->taskID << __E__;
						try
						{
							theFEInterfacesManager_->getFEInterfaceP(interfaceID)
							    ->clearFEMacroPercentDone(std::this_thread::get_id());
						}
						catch(...)
						{
						}
						{
							std::lock_guard<std::mutex> lock(asyncMacroMutex_);
							task->outputArgs = outputArgs;
							task->doneTime   = time(0);
							task->done       = true;
						}
					}
					catch(const std::exception& e)
					{
						try
						{
							theFEInterfacesManager_->getFEInterfaceP(interfaceID)
							    ->clearFEMacroPercentDone(std::this_thread::get_id());
						}
						catch(...)
						{
						}
						std::lock_guard<std::mutex> lock(asyncMacroMutex_);
						task->error =
						    "In Supervisor with LID=" +
						    std::to_string(getApplicationDescriptor()->getLocalId()) +
						    " the FE Macro named '" + interfaceID +
						    "' failed. Here is the error:\n\n" + e.what();
						task->doneTime = time(0);
						task->done     = true;
					}
					catch(...)
					{
						try
						{
							theFEInterfacesManager_->getFEInterfaceP(interfaceID)
							    ->clearFEMacroPercentDone(std::this_thread::get_id());
						}
						catch(...)
						{
						}
						std::lock_guard<std::mutex> lock(asyncMacroMutex_);
						task->error =
						    "In Supervisor with LID=" +
						    std::to_string(getApplicationDescriptor()->getLocalId()) +
						    " the FE Macro named '" + interfaceID +
						    "' failed due to an unknown error.";
						task->doneTime = time(0);
						task->done     = true;
					}
				}).detach();

				__SUP_COUT__ << "DIAG ms=" << diagNowMs() << " Launched async FE Macro '"
				             << feMacroName << "' for interfaceID '" << interfaceID
				             << "' with taskID=" << task->taskID << __E__;

				// Wait briefly to see if macro finishes quickly (avoids unnecessary polling)
				{
					size_t sleepUs = 100;
					for(int i = 0; i < 10; ++i)
					{
						usleep(sleepUs);
						{
							std::lock_guard<std::mutex> lock(asyncMacroMutex_);
							if(task->done)
							{
								if(!task->error.empty())
								{
									std::string errorCopy = task->error;
									for(size_t j = 0; j < asyncMacroTasks_.size(); ++j)
										if(asyncMacroTasks_[j]->taskID == task->taskID)
										{
											asyncMacroTasks_.erase(
											    asyncMacroTasks_.begin() + j);
											break;
										}
									__SUP_SS__ << errorCopy;
									__SUP_SS_THROW__;
								}
								__SUP_COUT__ << "DIAG ms=" << diagNowMs()
								             << " async macro finished within quick-wait,"
								             << " returning synchronously, taskID="
								             << task->taskID << __E__;
								retParameters.addParameter("outputArgs",
								                           task->outputArgs);
								for(size_t j = 0; j < asyncMacroTasks_.size(); ++j)
									if(asyncMacroTasks_[j]->taskID == task->taskID)
									{
										asyncMacroTasks_.erase(asyncMacroTasks_.begin() +
										                       j);
										break;
									}
								return SOAPUtilities::makeSOAPMessageReference(
								    supervisorClassNoNamespace_ + "Response",
								    retParameters);
							}
						}
						sleepUs *= 2;
						if(sleepUs > 50000)
							sleepUs = 50000;
					}
				}

				__SUP_COUT__ << "DIAG ms=" << diagNowMs()
				             << " async macro NOT done within quick-wait,"
				             << " returning NotDoneTaskID=" << task->taskID << __E__;
				retParameters.addParameter("NotDoneTaskID", std::to_string(task->taskID));
				return SOAPUtilities::makeSOAPMessageReference(
				    supervisorClassNoNamespace_ + "Response", retParameters);
			}
			else
			{
				// Synchronous path: run macro and return result (backward compatible)
				try
				{
					theFEInterfacesManager_->runFEMacro(
					    interfaceID, FEMacro, inputArgs, outputArgs);
				}
				catch(std::runtime_error& e)
				{
					__SUP_SS__ << "In Supervisor with LID="
					           << getApplicationDescriptor()->getLocalId()
					           << " the FE Macro named '" << feMacroName
					           << "' with target FE '" << interfaceID
					           << "' failed. Here is the error:\n\n"
					           << e.what() << __E__;
					__SUP_SS_THROW__;
				}
				catch(std::exception& e)
				{
					__SUP_SS__ << "In Supervisor with LID="
					           << getApplicationDescriptor()->getLocalId()
					           << " the FE Macro named '" << feMacroName
					           << "' with target FE '" << interfaceID
					           << "' failed. Here is the error:\n\n"
					           << e.what() << __E__;
					__SUP_SS_THROW__;
				}
				catch(...)
				{
					__SUP_SS__ << "In Supervisor with LID="
					           << getApplicationDescriptor()->getLocalId()
					           << " the FE Macro named '" << feMacroName
					           << "' with target FE '" << interfaceID
					           << "' failed due to an unknown error." << __E__;
					try
					{
						throw;
					}  //one more try to printout extra info
					catch(const std::exception& e)
					{
						ss << "Exception message: " << e.what();
					}
					catch(...)
					{
					}
					__SUP_SS_THROW__;
				}

				retParameters.addParameter("outputArgs", outputArgs);
				return SOAPUtilities::makeSOAPMessageReference(
				    supervisorClassNoNamespace_ + "Response", retParameters);
			}
		}
		else if(request == "RunMacroMakerMacro")
		{
			if(!theFEInterfacesManager_)
			{
				__SUP_SS__ << "Missing FE Interface Manager! Are you configured?"
				           << __E__;
				__SUP_SS_THROW__;
			}

			// params for running macros
			SOAPParameters requestParameters;
			requestParameters.addParameter("macroName");
			requestParameters.addParameter("macroString");
			requestParameters.addParameter("inputArgs");
			requestParameters.addParameter("outputArgs");
			requestParameters.addParameter("InterfaceID");
			requestParameters.addParameter("AsyncSupported");
			SOAPUtilities::receive(message, requestParameters);
			std::string interfaceID = requestParameters.getValue("InterfaceID");
			std::string macroName   = requestParameters.getValue("macroName");
			std::string macroString = requestParameters.getValue("macroString");
			std::string inputArgs   = requestParameters.getValue("inputArgs");
			std::string outputArgs  = requestParameters.getValue("outputArgs");
			bool asyncSupported     = requestParameters.getValue("AsyncSupported") == "1";

			if(asyncSupported)
			{
				// Async path: launch macro in a thread, return NotDoneTaskID immediately
				auto task = std::make_shared<AsyncMacroTask>();
				{
					std::lock_guard<std::mutex> lock(asyncMacroMutex_);
					task->taskID = ++asyncMacroTaskIDCounter_;
					if(asyncMacroTaskIDCounter_ == 0)
						task->taskID = ++asyncMacroTaskIDCounter_;
					task->interfaceID = interfaceID;
					task->startTime   = time(0);
					asyncMacroTasks_.push_back(task);
				}

				std::thread([this,
				             task,
				             interfaceID,
				             macroName,
				             macroString,
				             inputArgs,
				             outputArgs]() mutable {
					{
						std::lock_guard<std::mutex> lock(asyncMacroMutex_);
						task->threadID = std::this_thread::get_id();
					}
					try
					{
						theFEInterfacesManager_->getFEInterfaceP(interfaceID)
						    ->clearFEMacroPercentDone(std::this_thread::get_id());
					}
					catch(...)
					{
					}

					try
					{
						theFEInterfacesManager_->runMacro(
						    interfaceID, macroString, inputArgs, outputArgs);
						try
						{
							theFEInterfacesManager_->getFEInterfaceP(interfaceID)
							    ->clearFEMacroPercentDone(std::this_thread::get_id());
						}
						catch(...)
						{
						}
						{
							std::lock_guard<std::mutex> lock(asyncMacroMutex_);
							task->outputArgs = outputArgs;
							task->doneTime   = time(0);
							task->done       = true;
						}
					}
					catch(const std::exception& e)
					{
						try
						{
							theFEInterfacesManager_->getFEInterfaceP(interfaceID)
							    ->clearFEMacroPercentDone(std::this_thread::get_id());
						}
						catch(...)
						{
						}
						std::lock_guard<std::mutex> lock(asyncMacroMutex_);
						task->error =
						    "In Supervisor with LID=" +
						    std::to_string(getApplicationDescriptor()->getLocalId()) +
						    " the MacroMaker Macro named '" + macroName +
						    "' with target FE '" + interfaceID +
						    "' failed. Here is the error:\n\n" + e.what();
						task->doneTime = time(0);
						task->done     = true;
					}
					catch(...)
					{
						try
						{
							theFEInterfacesManager_->getFEInterfaceP(interfaceID)
							    ->clearFEMacroPercentDone(std::this_thread::get_id());
						}
						catch(...)
						{
						}
						std::lock_guard<std::mutex> lock(asyncMacroMutex_);
						task->error =
						    "In Supervisor with LID=" +
						    std::to_string(getApplicationDescriptor()->getLocalId()) +
						    " the MacroMaker Macro named '" + macroName +
						    "' with target FE '" + interfaceID +
						    "' failed due to an unknown error.";
						task->doneTime = time(0);
						task->done     = true;
					}
				}).detach();

				__SUP_COUT__ << "Launched async MacroMaker Macro '" << macroName
				             << "' for interfaceID '" << interfaceID
				             << "' with taskID=" << task->taskID << __E__;

				// Wait briefly to see if macro finishes quickly (avoids unnecessary polling)
				{
					size_t sleepUs = 100;
					for(int i = 0; i < 10; ++i)
					{
						usleep(sleepUs);
						{
							std::lock_guard<std::mutex> lock(asyncMacroMutex_);
							if(task->done)
							{
								if(!task->error.empty())
								{
									std::string errorCopy = task->error;
									for(size_t j = 0; j < asyncMacroTasks_.size(); ++j)
										if(asyncMacroTasks_[j]->taskID == task->taskID)
										{
											asyncMacroTasks_.erase(
											    asyncMacroTasks_.begin() + j);
											break;
										}
									__SUP_SS__ << errorCopy;
									__SUP_SS_THROW__;
								}
								__SUP_COUT__ << "DIAG ms=" << diagNowMs()
								             << " async macro finished within quick-wait,"
								             << " returning synchronously, taskID="
								             << task->taskID << __E__;
								retParameters.addParameter("outputArgs",
								                           task->outputArgs);
								for(size_t j = 0; j < asyncMacroTasks_.size(); ++j)
									if(asyncMacroTasks_[j]->taskID == task->taskID)
									{
										asyncMacroTasks_.erase(asyncMacroTasks_.begin() +
										                       j);
										break;
									}
								return SOAPUtilities::makeSOAPMessageReference(
								    supervisorClassNoNamespace_ + "Response",
								    retParameters);
							}
						}
						sleepUs *= 2;
						if(sleepUs > 50000)
							sleepUs = 50000;
					}
				}

				__SUP_COUT__ << "DIAG ms=" << diagNowMs()
				             << " async macro NOT done within quick-wait,"
				             << " returning NotDoneTaskID=" << task->taskID << __E__;
				retParameters.addParameter("NotDoneTaskID", std::to_string(task->taskID));
				return SOAPUtilities::makeSOAPMessageReference(
				    supervisorClassNoNamespace_ + "Response", retParameters);
			}
			else
			{
				// Synchronous path: run macro and return result (backward compatible)
				try
				{
					theFEInterfacesManager_->runMacro(
					    interfaceID, macroString, inputArgs, outputArgs);
				}
				catch(std::runtime_error& e)
				{
					__SUP_SS__ << "In Supervisor with LID="
					           << getApplicationDescriptor()->getLocalId()
					           << " the MacroMaker Macro named '" << macroName
					           << "' with target FE '" << interfaceID
					           << "' failed. Here is the error:\n\n"
					           << e.what() << __E__;
					__SUP_SS_THROW__;
				}
				catch(...)
				{
					__SUP_SS__ << "In Supervisor with LID="
					           << getApplicationDescriptor()->getLocalId()
					           << " the MacroMaker Macro named '" << macroName
					           << "' with target FE '" << interfaceID
					           << "' failed due to an unknown error." << __E__;
					try
					{
						throw;
					}  //one more try to printout extra info
					catch(const std::exception& e)
					{
						ss << "Exception message: " << e.what();
					}
					catch(...)
					{
					}
					__SUP_SS_THROW__;
				}

				retParameters.addParameter("outputArgs", outputArgs);
				return SOAPUtilities::makeSOAPMessageReference(
				    supervisorClassNoNamespace_ + "Response", retParameters);
			}
		}
		else if(request == "CheckMacro")
		{
			SOAPParameters requestParameters;
			requestParameters.addParameter("TaskID");
			SOAPUtilities::receive(message, requestParameters);
			uint64_t taskID = std::stoull(requestParameters.getValue("TaskID"));

			std::lock_guard<std::mutex> lock(asyncMacroMutex_);

			// Cleanup old completed tasks (>5 minutes after completion)
			time_t now = time(0);
			for(size_t i = 0; i < asyncMacroTasks_.size();)
			{
				auto& t = asyncMacroTasks_[i];
				if(t->done && t->doneTime > 0 && (now - t->doneTime) > 5 * 60)
					asyncMacroTasks_.erase(asyncMacroTasks_.begin() + i);
				else
					++i;
			}

			// Find the requested task
			std::shared_ptr<AsyncMacroTask> foundTask;
			for(auto& t : asyncMacroTasks_)
			{
				if(t->taskID == taskID)
				{
					foundTask = t;
					break;
				}
			}

			if(!foundTask)
			{
				__SUP_SS__ << "Async macro task with ID=" << taskID
				           << " was not found. Perhaps it completed more than 5 "
				              "minutes ago?"
				           << __E__;
				__SUP_SS_THROW__;
			}

			if(foundTask->done)
			{
				if(!foundTask->error.empty())
				{
					std::string errorCopy = foundTask->error;
					// Remove completed task
					for(size_t i = 0; i < asyncMacroTasks_.size(); ++i)
					{
						if(asyncMacroTasks_[i]->taskID == taskID)
						{
							asyncMacroTasks_.erase(asyncMacroTasks_.begin() + i);
							break;
						}
					}
					__SUP_SS__ << errorCopy;
					__SUP_SS_THROW__;
				}

				__SUP_COUT__ << "DIAG ms=" << diagNowMs()
				             << " CheckMacro found task DONE (doneTime=" << foundTask->doneTime
				             << "), taskID=" << taskID << __E__;
				retParameters.addParameter("outputArgs", foundTask->outputArgs);

				// Remove completed task
				for(size_t i = 0; i < asyncMacroTasks_.size(); ++i)
				{
					if(asyncMacroTasks_[i]->taskID == taskID)
					{
						asyncMacroTasks_.erase(asyncMacroTasks_.begin() + i);
						break;
					}
				}

				return SOAPUtilities::makeSOAPMessageReference(
				    supervisorClassNoNamespace_ + "Response", retParameters);
			}
			else
			{
				// Still running — check for real progress from the FE macro
				if(theFEInterfacesManager_ && foundTask->threadID != std::thread::id())
				{
					try
					{
						int progress = theFEInterfacesManager_
						                   ->getFEInterfaceP(foundTask->interfaceID)
						                   ->getFEMacroPercentDone(foundTask->threadID);
						if(progress >= 0)
							retParameters.addParameter("Progress",
							                           std::to_string(progress));
					}
					catch(...)
					{
					}
				}
				__SUP_COUT__ << "DIAG ms=" << diagNowMs()
				             << " CheckMacro found task still RUNNING, taskID=" << taskID
				             << __E__;
				retParameters.addParameter("NotDoneTaskID", std::to_string(taskID));
				return SOAPUtilities::makeSOAPMessageReference(
				    supervisorClassNoNamespace_ + "Response", retParameters);
			}
		}
		else
		{
			__SUP_SS__ << "Unrecognized request received! '" << request << "'" << __E__;
			__SUP_SS_THROW__;
		}
	}
	catch(const std::runtime_error& e)
	{
		__SUP_SS__ << "Error occurred handling request: " << e.what() << __E__;
		__SUP_COUT_ERR__ << ss.str();
		retParameters.addParameter("Error", ss.str());
	}
	catch(...)
	{
		__SUP_SS__ << "Error occurred handling request." << __E__;
		try
		{
			throw;
		}  //one more try to printout extra info
		catch(const std::exception& e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...)
		{
		}
		__SUP_COUT_ERR__ << ss.str();
		retParameters.addParameter("Error", ss.str());
	}

	return SOAPUtilities::makeSOAPMessageReference(
	    supervisorClassNoNamespace_ + "FailRequest", retParameters);

}  // end macroMakerSupervisorRequest()

//==============================================================================
xoap::MessageReference FESupervisor::workLoopStatusRequest(
    xoap::MessageReference /*message*/)
{
	if(!theFEInterfacesManager_)
	{
		__SUP_SS__ << "Invalid request for front-end workloop status from Supervisor "
		              "without a FEVInterfacesManager."
		           << __E__;
		__SUP_SS_THROW__;
	}

	return SOAPUtilities::makeSOAPMessageReference(
	    (theFEInterfacesManager_->allFEWorkloopsAreDone()
	         ? CoreSupervisorBase::WORK_LOOP_DONE
	         : CoreSupervisorBase::WORK_LOOP_WORKING));
}  // end workLoopStatusRequest()

//==============================================================================
/// extractFEInterfaceManager
///
///	locates theFEInterfacesManager in state machines vector and
///		returns 0 if not found.
///
///	Note: this code assumes a CoreSupervisorBase has only one
///		FEVInterfacesManager in its vector of state machines
FEVInterfacesManager* FESupervisor::extractFEInterfacesManager()
{
	theFEInterfacesManager_ = 0;

	for(unsigned int i = 0; i < theStateMachineImplementation_.size(); ++i)
	{
		try
		{
			theFEInterfacesManager_ =
			    dynamic_cast<FEVInterfacesManager*>(theStateMachineImplementation_[i]);
			if(!theFEInterfacesManager_)
			{
				// dynamic_cast returns null pointer on failure
				__SUP_SS__ << "Dynamic cast failure!" << __E__;
				__SUP_SS_THROW__;
			}
			__SUP_COUT__ << "State Machine " << i << " WAS of type FEVInterfacesManager"
			             << __E__;

			break;
		}
		catch(...)
		{
			__SUP_COUT__ << "State Machine " << i
			             << " was NOT of type FEVInterfacesManager" << __E__;
		}
	}

	__SUP_COUT__ << "theFEInterfacesManager pointer = " << theFEInterfacesManager_
	             << __E__;

	return theFEInterfacesManager_;
}  // end extractFEInterfaceManager()

//==============================================================================
void FESupervisor::transitionConfiguring(toolbox::Event::Reference /*event*/)
{
	__SUP_COUT__ << "transitionConfiguring" << __E__;
	CoreSupervisorBase::configureInit();

	// get pset from Board Reader metric manager table
	try
	{
		__COUTV__(CorePropertySupervisorBase::getSupervisorConfigurationPath());

		ConfigurationTree feSupervisorNode =
		    CorePropertySupervisorBase::getSupervisorTableNode();

		std::string metric_string     = "";
		bool        metricStringSetup = true;
		try
		{
			std::ostringstream oss;
			std::string        tabString      = "";
			std::string        commentsString = "";
			ARTDAQTableBase::insertMetricsBlock(
			    oss, tabString, commentsString, "" /* parentPath*/, feSupervisorNode);
			metric_string = oss.str();
		}
		catch(...)
		{
			metricStringSetup = false;
			metric_string     = "";
		}  // ignore error

		if(!metricMan)
		{
			__SUP_COUT__ << "Metric manager is not instantiated! Attempting to fix."
			             << __E__;
			metricMan = std::make_unique<artdaq::MetricManager>();
		}
		std::string metricNamePreamble =
		    feSupervisorNode.getNode("/SlowControlsMetricManagerChannelNamePreamble")
		        .getValueWithDefault<std::string>("");
		__SUP_COUTV__(metricNamePreamble);

		// std::string         metric_string = "epics: {metricPluginType:epics level:3 channel_name_prefix:Mu2e}";
		fhicl::ParameterSet metric_pset = fhicl::ParameterSet::make(metric_string);

		__SUP_COUTV__(metricNamePreamble);
		try
		{
			metricMan->initialize(metric_pset.get<fhicl::ParameterSet>("metrics"),
			                      metricNamePreamble);
		}
		catch(...)
		{
			if(metricStringSetup)
				throw;
			else
				__SUP_COUT__ << "Ignore metric manager initialize error because metric "
				                "string is not setup."
				             << __E__;
		}
		__SUP_COUT__ << "transitionConfiguring metric manager(" << metricMan
		             << ") initialized = " << metricMan->Initialized() << __E__;
	}
	catch(const std::runtime_error& e)
	{
		__SS__ << "Error loading metrics in FESupervisor::transitionConfiguring(): "
		       << e.what() << __E__;
		__SUP_COUT_ERR__ << ss.str();
		// ExceptionHandler(ExceptionHandlerRethrow::no, ss.str());

		//__SS_THROW_ONLY__;
		theStateMachine_.setErrorMessage(ss.str());
		throw toolbox::fsm::exception::Exception(
		    "Transition Error" /*name*/,
		    ss.str() /* message*/,
		    "FESupervisor::transitionConfiguring" /*module*/,
		    __LINE__ /*line*/,
		    __FUNCTION__ /*function*/
		);
	}
	catch(...)
	{
		__SS__ << "Error loading metrics in FESupervisor::transitionConfiguring()"
		       << __E__;
		try
		{
			throw;
		}  //one more try to printout extra info
		catch(const std::exception& e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...)
		{
		}
		__SUP_COUT_ERR__ << ss.str();
		// ExceptionHandler(ExceptionHandlerRethrow::no, ss.str());

		//__SS_THROW_ONLY__;
		theStateMachine_.setErrorMessage(ss.str());
		throw toolbox::fsm::exception::Exception(
		    "Transition Error" /*name*/,
		    ss.str() /* message*/,
		    "FESupervisor::transitionConfiguring" /*module*/,
		    __LINE__ /*line*/,
		    __FUNCTION__ /*function*/
		);
	}

	CoreSupervisorBase::transitionConfiguringFSMs();

	__SUP_COUT__ << "transitionConfiguring done." << __E__;
}  // end transitionConfiguring()

//==============================================================================
void FESupervisor::transitionHalting(toolbox::Event::Reference event)
{
	__SUP_COUT__ << "transitionHalting" << __E__;
	TLOG_DEBUG(7) << "transitionHalting";

	// Wait for any in-flight async macro tasks before tearing down FE interfaces
	{
		const int    timeoutSeconds = 30;
		const time_t deadline       = time(0) + timeoutSeconds;
		bool         hasOutstanding = true;
		while(hasOutstanding && time(0) < deadline)
		{
			{
				std::lock_guard<std::mutex> lock(asyncMacroMutex_);
				hasOutstanding = false;
				for(const auto& t : asyncMacroTasks_)
				{
					if(!t->done)
					{
						hasOutstanding = true;
						break;
					}
				}
			}
			if(hasOutstanding)
			{
				__SUP_COUT__ << "Waiting for outstanding async macro tasks to complete "
				                "before halting..."
				             << __E__;
				usleep(500 * 1000);
			}
		}
		if(hasOutstanding)
			__SUP_COUT_WARN__ << "Timed out waiting for async macro tasks; "
			                     "proceeding with halt."
			                  << __E__;
		std::lock_guard<std::mutex> lock(asyncMacroMutex_);
		asyncMacroTasks_.clear();
	}

	// shutdown workloops first, then shutdown metric manager
	CoreSupervisorBase::transitionHalting(event);

	try
	{
		if(metricMan && metricMan->Initialized())
		{
			TLOG_DEBUG(7) << "Metric manager(" << metricMan << ") shutting down..."
			              << __E__;
			metricMan
			    ->shutdown();  // will set initilized_ to false with mutex, which should prevent races
			TLOG_DEBUG(7) << "Metric manager shutdown." << __E__;
		}
		else
			__SUP_COUT__ << "Metric manager(" << metricMan << ") already shutdown."
			             << __E__;

		metricMan.reset(nullptr);
	}
	catch(...)
	{
		__SS__ << "Error shutting down metrics in FESupervisor::transitionHalting()"
		       << __E__;
		try
		{
			throw;
		}  //one more try to printout extra info
		catch(const std::exception& e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...)
		{
		}
		__SUP_COUT_ERR__ << ss.str();
		// ExceptionHandler(ExceptionHandlerRethrow::no, ss.str());

		//__SS_THROW_ONLY__;
		theStateMachine_.setErrorMessage(ss.str());
		throw toolbox::fsm::exception::Exception(
		    "Transition Error" /*name*/,
		    ss.str() /* message*/,
		    "FESupervisor::transitionHalting" /*module*/,
		    __LINE__ /*line*/,
		    __FUNCTION__ /*function*/
		);
	}

	__SUP_COUT__ << "transitionHalting done." << __E__;
}  // end transitionHalting()

//==============================================================================
void FESupervisor::initDataPublishing(const std::string& endpoint,
                                      const std::string& topic)
{
	if(dp_isInitialized_)
	{
		// silently re‑initialise – close old socket first.
		closeDataPublishing(false);
	}

	// Store for later use.
	dp_endpoint_ = endpoint;
	dp_topic_    = topic;

	// Bind the PUB socket.
	try
	{
		dp_socket_.bind(dp_endpoint_);
	}
	catch(const zmq::error_t& e)
	{
		__SUP_SS__ << "initDataPublishing() - bind to zmq '" + dp_endpoint_ +
		                  "' failed: " + e.what()
		           << __E__;
		__SUP_SS_THROW__;
	}

	dp_isInitialized_ = true;
}  //end initDataPublishing()

//==============================================================================
void FESupervisor::closeDataPublishing(bool alsoCloseContext /* = true */)
{
	if(dp_isInitialized_)
	{
		try
		{
			dp_socket_.set(zmq::sockopt::linger, 0);  // discard unsent messages on close
			dp_socket_.close();                       // close the PUB socket
		}
		catch(const zmq::error_t&)
		{
			// ignore – we are cleaning up.
		}
		try
		{
			if(alsoCloseContext)
				dp_context_.close();  // close the ZMQ context
		}
		catch(const zmq::error_t&)
		{
			// ignore.
		}
		dp_isInitialized_ = false;
	}
}  //end closeDataPublishing()

//==============================================================================
/// Publish data to the ZeroMQ PUB socket.
///
/// @param dataPtr Pointer to the data to publish. May be nullptr only if dataSize is 0.
/// @param dataSize Size of the data to publish, in bytes. May be 0, in which case an empty frame will be sent.
///
/// Example receiving published data:
/// 	artdaqDriver -c srcs/artdaq-mu2e/tools/fcl/cfo_driver.fcl
void FESupervisor::publishData(const char* dataPtr, size_t dataSize)
{
	if(!dp_isInitialized_)
	{
		__SUP_SS__ << "publishData() called before zmq init()" << __E__;
		__SUP_SS_THROW__;
	}

	// ---- Topic frame (must be sent with sndmore) ----
	// Using zmq::buffer prevents an extra copy – it just points at the data.
	auto rc_topic = dp_socket_.send(zmq::buffer(dp_topic_), zmq::send_flags::sndmore);
	if(!rc_topic)
	{
		__SUP_SS__ << "publishData() - failed to send zmq topic" << __E__;
		__SUP_SS_THROW__;
	}

	// ---- Payload frame (final frame) ----
	// `dataPtr` may be nullptr only if `dataSize == 0` – ZeroMQ accepts an empty frame.
	if(dataPtr == nullptr && dataSize > 0)
	{
		__SUP_SS__ << "FESupervisor::publishData() - dataPtr is nullptr but dataSize is "
		           << dataSize << __E__;
		__SUP_SS_THROW__;
	}
	auto rc_payload =
	    dp_socket_.send(zmq::buffer(dataPtr, dataSize), zmq::send_flags::none);
	if(!rc_payload)
	{
		__SUP_SS__ << "publishData() - failed to send zmq payload" << __E__;
		__SUP_SS_THROW__;
	}
}  //end publishData()
