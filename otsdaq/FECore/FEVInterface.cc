#include "otsdaq/FECore/FEVInterface.h"
#include "otsdaq/CoreSupervisors/CoreSupervisorBase.h"
#include "otsdaq/FECore/FEVInterfacesManager.h"
#include "otsdaq/NetworkUtilities/UDPDataStreamerBase.h"
#include "otsdaq/Macros/BinaryStringMacros.h"

#define TRACE_NAME "FEVInterface"
#include <iostream>
#include <sstream>
#include <thread>  //for std::thread

using namespace ots;

//==============================================================================
FEVInterface::FEVInterface(const std::string& interfaceUID, const ConfigurationTree& theXDAQContextConfigTree, const std::string& configurationPath)
    : WorkLoop(interfaceUID)
    , Configurable(theXDAQContextConfigTree, configurationPath)
    , VStateMachine(interfaceUID)
    , slowControlsWorkLoop_(interfaceUID + "-SlowControls", this)
    , interfaceUID_(interfaceUID)
    , mfSubject_(interfaceUID)
{
	// NOTE!! be careful to not decorate with __FE_COUT__ because in the constructor the
	// base class versions of function (e.g. getInterfaceType) are called because the
	// derived class has not been instantiate yet!
	// Instead use __GEN_COUT__ which decorates using mfSubject_
	__GEN_COUT__ << "Constructed." << __E__;
}  // end constructor()

//==============================================================================
FEVInterface::~FEVInterface(void)
{
	// NOTE:: be careful not to call __FE_COUT__ decoration because it might use the tree
	// depending on child class overrides, and it may already be destructed partially.
	// Instead use __GEN_COUT__ which decorates using mfSubject_
	__GEN_COUT__ << "Destructed." << __E__;
}  // end destructor()

//==============================================================================
void FEVInterface::configureSlowControls(void)
try
{
	//do not use __GEN_COUT__ as mfSubject may not be setup
	__COUT__ << "configureSlowControls path=" << theConfigurationPath_ << __E__;

	// Start artdaq metric manager here, if possible
	if(metricMan && !metricMan->Running() && metricMan->Initialized())
	{
		__COUT__ << "Metric manager starting..." << __E__;
		metricMan->do_start();
		__COUT__ << "Metric manager started." << __E__;
	}
	else if(!metricMan || !metricMan->Initialized())
		__COUT__ << "Metric manager could not be started! metricMan: " << metricMan << " Initialized()= " << (metricMan ? metricMan->Initialized() : 0)
		             << __E__;
	else
		__COUT__ << "Metric manager already started." << __E__;

	mapOfSlowControlsChannels_.clear();  // reset

	//allow to possible position for link "LinkToSlowControlsChannelTable"
	//	1. back 1 node (e.g. at generic FEInterface table)
	// 	2. at the config path (e.g. for special FE children cases)

	bool type1 = true;
	std::string errMessage;
	try
	{
		ConfigurationTree testNode = theXDAQContextConfigTree_.getNode(theConfigurationPath_).getNode("LinkToSlowControlsChannelTable");
		__COUTV__(testNode.isDisconnected());
		type1 = false;
	}
	catch(...) { /* ignore */ }
	
	if(type1)
		addSlowControlsChannels(theXDAQContextConfigTree_.getBackNode(theConfigurationPath_).getNode("LinkToSlowControlsChannelTable"),
	                        &mapOfSlowControlsChannels_);
	else
	{
		try
		{
			addSlowControlsChannels(theXDAQContextConfigTree_.getNode(theConfigurationPath_).getNode("LinkToSlowControlsChannelTable"),
	                        &mapOfSlowControlsChannels_);	
		}
		catch(const std::runtime_error& e)
		{
			__SS__ << "Configuring slow controls channels encountered an error: " << e.what();			
			__SS_THROW__;
		}
		
	}

}  // end configureSlowControls()
catch(const std::runtime_error& e)
{
	__SS__ << "Error was caught while configuring slow controls: " << e.what() << __E__;
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "Unknown error was caught while configuring slow controls." << __E__;
	try	{ throw; } //one more try to printout extra info
	catch(const std::exception &e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...){}
	__SS_THROW__;
}

//==============================================================================
// addSlowControlsChannels
//	Usually subInterfaceID = "" and mapOfSlowControlsChannels =
//&mapOfSlowControlsChannels_
void FEVInterface::addSlowControlsChannels(ConfigurationTree                                          slowControlsGroupLink,
                                           std::map<std::string /* ROC UID*/, FESlowControlsChannel>* mapOfSlowControlsChannels)
{
	if(slowControlsGroupLink.isDisconnected())
	{
		__FE_COUT__ << "slowControlsGroupLink is disconnected, so done configuring slow controls." << __E__;
		return;
	}
	__FE_COUT__ << "slowControlsGroupLink is valid! Adding slow controls channels..." << __E__;

	std::vector<std::pair<std::string, ConfigurationTree> > groupLinkChildren = slowControlsGroupLink.getChildren();
	for(auto& groupLinkChild : groupLinkChildren)
	{
		// skip channels that are off
		if(!(groupLinkChild.second.getNode(TableViewColumnInfo::COL_NAME_STATUS).getValue<bool>()))
			continue;

		__FE_COUT__ << "Channel:" << slowControlsGroupLink.getTableName() << "/" << 
						getInterfaceUID() << "/" << groupLinkChild.first
		            << "\t Type:" << groupLinkChild.second.getNode("ChannelDataType") << __E__;

		mapOfSlowControlsChannels->insert(std::pair<std::string, FESlowControlsChannel>(
		    groupLinkChild.first,
		    FESlowControlsChannel(this,
		                          groupLinkChild.first,
		                          groupLinkChild.second.getNode("ChannelDataType").getValue<std::string>(),
		                          groupLinkChild.second.getNode("UniversalInterfaceAddress").getValue<std::string>(),
		                          groupLinkChild.second.getNode("UniversalDataBitOffset").getValue<unsigned int>(),
		                          groupLinkChild.second.getNode("ReadAccess").getValue<bool>(),
		                          groupLinkChild.second.getNode("WriteAccess").getValue<bool>(),
		                          groupLinkChild.second.getNode("MonitoringEnabled").getValue<bool>(),
		                          groupLinkChild.second.getNode("RecordChangesOnly").getValue<bool>(),
		                          groupLinkChild.second.getNode("DelayBetweenSamplesInSeconds").getValue<time_t>(),
		                          groupLinkChild.second.getNode("LocalSavingEnabled").getValue<bool>(),
		                          groupLinkChild.second.getNode("LocalFilePath").getValue<std::string>(),
		                          groupLinkChild.second.getNode("RadixFileName").getValue<std::string>(),
		                          groupLinkChild.second.getNode("SaveBinaryFile").getValue<bool>(),
		                          groupLinkChild.second.getNode("AlarmsEnabled").getValue<bool>(),
		                          groupLinkChild.second.getNode("LatchAlarms").getValue<bool>(),
		                          groupLinkChild.second.getNode("LowLowThreshold").getValue<std::string>(),
		                          groupLinkChild.second.getNode("LowThreshold").getValue<std::string>(),
		                          groupLinkChild.second.getNode("HighThreshold").getValue<std::string>(),
		                          groupLinkChild.second.getNode("HighHighThreshold").getValue<std::string>())));
	}
	__FE_COUT__ << "Added " << mapOfSlowControlsChannels->size() << " slow controls channels." << __E__;

}  // end addSlowControlsChannels()

//==============================================================================
// virtual in case channels are handled in multiple maps, for example
void FEVInterface::resetSlowControlsChannelIterator(void)
{
	slowControlsChannelsIterator_ = mapOfSlowControlsChannels_.begin();
}  // end resetSlowControlsChannelIterator()

//==============================================================================
// virtual in case channels are handled in multiple maps, for example
FESlowControlsChannel* FEVInterface::getNextSlowControlsChannel(void)
{
	if(slowControlsChannelsIterator_ == mapOfSlowControlsChannels_.end())
		return nullptr;

	return &((slowControlsChannelsIterator_++)->second);  // return iterator, then increment
}  // end getNextSlowControlsChannel()

//==============================================================================
// virtual in case channels are handled in multiple maps, for example
unsigned int FEVInterface::getSlowControlsChannelCount(void) { return mapOfSlowControlsChannels_.size(); }  // end getSlowControlsChannelCount()

//==============================================================================
bool FEVInterface::slowControlsRunning(void)
try
{
	__FE_COUT__ << "slowControlsRunning" << __E__;

	if(getSlowControlsChannelCount() == 0)
	{
		__FE_COUT__ << "No slow controls channels to monitor, exiting slow controls workloop." << __E__;
		return false;
	}
	std::string readValInst;
	std::string& readVal = readValInst;
	readVal.resize(universalDataSize_);  // size to data in advance

	FESlowControlsChannel* channel;

	const unsigned int txBufferSz            = 1500;
	const unsigned int txBufferFullThreshold = 750;
	std::string        txBuffer;
	txBuffer.reserve(txBufferSz);

	ConfigurationTree FEInterfaceNode = theXDAQContextConfigTree_.getBackNode(theConfigurationPath_);

	// attempt to make Slow Controls transfer socket
	std::unique_ptr<UDPDataStreamerBase> slowContrlolsTxSocket;
	std::string                          slowControlsSupervisorIPAddress = "", slowControlsSelfIPAddress = "";
	int                                  slowControlsSupervisorPort = 0, slowControlsSelfPort = 0;
	try
	{
		ConfigurationTree slowControlsInterfaceLink = FEInterfaceNode.getNode("LinkToSlowControlsSupervisorTable");

		if(slowControlsInterfaceLink.isDisconnected())
		{
			__FE_SS__ << "slowControlsInterfaceLink is disconnected, so no socket made." << __E__;
			__FE_SS_THROW__;
		}

		slowControlsSelfIPAddress       = FEInterfaceNode.getNode("SlowControlsTxSocketIPAddress").getValue<std::string>();
		slowControlsSelfPort            = FEInterfaceNode.getNode("SlowControlsTxSocketPort").getValue<int>();
		slowControlsSupervisorIPAddress = slowControlsInterfaceLink.getNode("IPAddress").getValue<std::string>();
		slowControlsSupervisorPort      = slowControlsInterfaceLink.getNode("Port").getValue<int>();
	}
	catch(...)
	{
		__FE_COUT__ << "Link to slow controls supervisor is missing, so no socket made." << __E__;
	}

	if(slowControlsSupervisorPort && slowControlsSelfPort && slowControlsSupervisorIPAddress != "" && slowControlsSelfIPAddress != "")
	{
		__FE_COUT__ << "slowControlsInterfaceLink is valid! Create tx socket..." << __E__;
		slowContrlolsTxSocket.reset(
		    new UDPDataStreamerBase(slowControlsSelfIPAddress, slowControlsSelfPort, slowControlsSupervisorIPAddress, slowControlsSupervisorPort));
	}
	else
	{
		__FE_COUT__ << "Invalid Slow Controls socket parameters, so no socket made." << __E__;
	}

	// check if aggregate saving

	FILE* fp                          = 0;
	bool  aggregateFileIsBinaryFormat = false;
	try
	{
		if(FEInterfaceNode.getNode("SlowControlsLocalAggregateSavingEnabled").getValue<bool>())
		{
			aggregateFileIsBinaryFormat = FEInterfaceNode.getNode("SlowControlsSaveBinaryFile").getValue<bool>();

			__FE_COUT_INFO__ << "Slow Controls Aggregate Saving turned On BinaryFormat=" << aggregateFileIsBinaryFormat << __E__;

			std::string saveFullFileName = FEInterfaceNode.getNode("SlowControlsLocalFilePath").getValue<std::string>() + "/" +
			                               FEInterfaceNode.getNode("SlowControlsRadixFileName").getValue<std::string>() + "-" +
			                               FESlowControlsChannel::underscoreString(getInterfaceUID()) + "-" + std::to_string(time(0)) +
			                               (aggregateFileIsBinaryFormat ? ".dat" : ".txt");

			fp = fopen(saveFullFileName.c_str(), aggregateFileIsBinaryFormat ? "ab" : "a");
			if(!fp)
			{
				__FE_COUT_ERR__ << "Failed to open slow controls channel file: " << saveFullFileName << __E__;
				// continue on, just nothing will be saved
			}
			else
				__FE_COUT_INFO__ << "Slow controls aggregate file opened: " << saveFullFileName << __E__;
		}
	}
	catch(...)
	{
	}  // do nothing

	if(!aggregateFileIsBinaryFormat)
		__FE_COUT_INFO__ << "Slow Controls Aggregate Saving turned off." << __E__;

	time_t timeCounter = 0;

	unsigned int numOfReadAccessChannels = 0;
	bool         firstTime               = true;

	while(slowControlsWorkLoop_.getContinueWorkLoop())
	{
		// __FE_COUT__ << "..." << __E__;

		sleep(1);  // seconds
		++timeCounter;

		if(txBuffer.size())
			__FE_COUT__ << "txBuffer sz=" << txBuffer.size() << __E__;

		txBuffer.resize(0);  // clear buffer a la txBuffer = "";

		//__FE_COUT__ << "timeCounter=" << timeCounter << __E__;
		//__FE_COUT__ << "txBuffer sz=" << txBuffer.size() << __E__;

		resetSlowControlsChannelIterator();
		while((channel = getNextSlowControlsChannel()) != nullptr)
		{
			// skip if no read access
			if(!channel->readAccess_)
				continue;

			if(firstTime)
				++numOfReadAccessChannels;

			// skip if not a sampling moment in time for channel
			if(timeCounter % channel->delayBetweenSamples_)
				continue;

			__FE_COUT__ << "Reading Channel:" << channel->fullChannelName_ << " at t=" << time(0) << __E__;

			//check if can use buffered value
			resetSlowControlsChannelIterator();
			FESlowControlsChannel* channelToCopy;
			bool usingBufferedValue = false;
			while((channelToCopy = getNextSlowControlsChannel()) != channel)
			{
				__FE_COUT__ << "Looking for buffered value at " << 					
					BinaryStringMacros::binaryNumberToHexString(channelToCopy->universalAddress_,  "0x", " ") << " "  << 
					channelToCopy->getReadSizeBytes() << " " <<
					time(0) - channelToCopy->getLastSampleTime() << __E__;

				if(BinaryStringMacros::binaryNumberToHexString(channelToCopy->universalAddress_,  "0x", " ") == 
					BinaryStringMacros::binaryNumberToHexString(channel->universalAddress_,  "0x", " ") && 
					channelToCopy->getReadSizeBytes() == channel->getReadSizeBytes() && 
					time(0) - channelToCopy->getLastSampleTime() < 2 /* within 2 seconds, then re-use buffer */)
				{
					usingBufferedValue = true;
					//can NOT break;... must take iterator back to channel
					__FE_COUT__ << "Using buffered " << channelToCopy->getReadSizeBytes() << 
						"-byte value at address:" << 
						BinaryStringMacros::binaryNumberToHexString(channelToCopy->universalAddress_,  "0x", " ") << __E__;

					__FE_COUT__ << "Copying: " << BinaryStringMacros::binaryNumberToHexString(channelToCopy->universalReadValue_, "0x", " ") << 
						" at t=" << time(0) << __E__;
					readVal = channelToCopy->universalReadValue_; //copy by reference
					__FE_COUT__ << "Copied: " << BinaryStringMacros::binaryNumberToHexString(readVal, "0x", " ") << " at t=" << time(0) << __E__;
				}
			}

			if(!usingBufferedValue)
				channel->getSample(readVal);

			// have sample
			channel->handleSample(readVal, txBuffer, fp, aggregateFileIsBinaryFormat);
			__FE_COUT__ << "Have: " << BinaryStringMacros::binaryNumberToHexString(channel->universalReadValue_, "0x", " ") << " at t=" << time(0) << __E__;					

			if(txBuffer.size())
				__FE_COUT__ << "txBuffer sz=" << txBuffer.size() << __E__;


			// Use artdaq Metric Manager if available,
			if(channel->monitoringEnabled_ && metricMan && metricMan->Running() && universalAddressSize_ <= 8)
			{
				uint64_t val = 0;  // 64 bits!
				for(size_t ii = 0; ii < universalAddressSize_; ++ii)
					val += (uint8_t)readVal[ii] << (ii * 8);

				__FE_COUT__ << "Sending sample to Metric Manager..." << __E__;
				metricMan->sendMetric(channel->fullChannelName_, val, "", 3, artdaq::MetricMode::LastPoint);
			}
			else
				__FE_COUT__ << "Skipping sample to Metric Manager: "
				            << " channel->monitoringEnabled_=" << channel->monitoringEnabled_ << " metricMan=" << metricMan
				            << " metricMan->Running()=" << (metricMan && metricMan->Running()) << __E__;

			// make sure buffer hasn't exploded somehow
			if(txBuffer.size() > txBufferSz)
			{
				__FE_SS__ << "This should never happen hopefully!" << __E__;
				__FE_SS_THROW__;
			}

			// send early if threshold reached
			if(slowContrlolsTxSocket && txBuffer.size() > txBufferFullThreshold)
			{
				__FE_COUT__ << "Sending now! txBufferFullThreshold=" << txBufferFullThreshold << __E__;
				slowContrlolsTxSocket->send(txBuffer);
				txBuffer.resize(0);  // clear buffer a la txBuffer = "";
			}
		}

		if(txBuffer.size())
			__FE_COUT__ << "txBuffer sz=" << txBuffer.size() << __E__;

		// send anything left
		if(slowContrlolsTxSocket && txBuffer.size())
		{
			__FE_COUT__ << "Sending now!" << __E__;
			slowContrlolsTxSocket->send(txBuffer);
		}

		if(fp)
			fflush(fp);  // flush anything in aggregate file for reading ease

		if(firstTime)
		{
			if(numOfReadAccessChannels == 0)
			{
				__FE_COUT_WARN__ << "There are no slow controls channels with read access!" << __E__;
				break;
			}
			else
				__FE_COUT__ << "There are " << getSlowControlsChannelCount() << " slow controls channels total. " << numOfReadAccessChannels
				         << " with read access enabled." << __E__;
		}
		firstTime = false;
	}  // end main slow controls loop

	if(fp)
		fclose(fp);

	__FE_COUT__ << "Slow controls workloop done." << __E__;

	return false;
}  // end slowControlsRunning()
catch(...)  //
{
	// catch all, then rethrow with local variables needed
	__FE_SS__;

	bool isPauseException = false;
	bool isStopException  = false;

	try
	{
		throw;
	}
	catch(const __OTS_PAUSE_EXCEPTION__& e)
	{
		ss << "PAUSE Exception was caught during slow controls running thread: " << e.what() << std::endl;
		isPauseException = true;
	}
	catch(const __OTS_STOP_EXCEPTION__& e)
	{
		ss << "STOP Exception was caught during slow controls running thread: " << e.what() << std::endl;
		isStopException = true;
	}
	catch(const std::runtime_error& e)
	{
		ss << "Caught an error during slow controls running thread of FE Interface '" << Configurable::theConfigurationRecordName_ << "': " << e.what()
		   << __E__;
	}
	catch(...)
	{
		ss << "Caught an unknown error during slow controls running thread." << __E__;
		try	{ throw; } //one more try to printout extra info
		catch(const std::exception &e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...){}
	}

	// At this point, an asynchronous error has occurred
	//	during front-end running...
	// Send async error to Gateway
	//	Do this as thread so that workloop can end.

	__FE_COUT_ERR__ << ss.str();

	std::thread(
	    [](FEVInterface* fe, const std::string errorMessage, bool isPauseException, bool isStopException) {
		    FEVInterface::sendAsyncExceptionToGateway(fe, errorMessage, isPauseException, isStopException);
	    },
	    // pass the values
	    this /*fe*/,
	    ss.str() /*errorMessage*/,
	    isPauseException,
	    isStopException)
	    .detach();

	return false;
}  // end slowControlsRunning()

//==============================================================================
// SendAsyncErrorToGateway
//	Static -- thread
//	Send async error or soft error to gateway
//	Call this as thread so that parent calling function (workloop) can end.
//
//	Note: be careful not to access fe pointer after HALT
//		has potentially propagated.. because the pointer might be destructed!
void FEVInterface::sendAsyncExceptionToGateway(FEVInterface* fe, const std::string& errorMessage, bool isPauseException, bool isStopException)
try
{
	std::stringstream feHeader;
	feHeader << ":FE:" << fe->getInterfaceType() << ":" << fe->getInterfaceUID() << ":" << fe->theConfigurationRecordName_ << "\t";

	if(isStopException)
	{
		__COUT_ERR__ << feHeader.str() << "Sending FE Async STOP Running Exception... \n" << errorMessage << __E__;
		fe->VStateMachine::parentSupervisor_->setAsyncPauseExceptionMessage(errorMessage);
	}
	else if(isPauseException)
	{
		__COUT_ERR__ << feHeader.str() << "Sending FE Async PAUSE Running Exception... \n" << errorMessage << __E__;
		fe->VStateMachine::parentSupervisor_->setAsyncStopExceptionMessage(errorMessage);
	}
	else
		__COUT_ERR__ << feHeader.str() << "Sending FE Async Running Error... \n" << errorMessage << __E__;

	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* gatewaySupervisor = fe->VStateMachine::parentSupervisor_->allSupervisorInfo_.getGatewayInfo().getDescriptor();

	SOAPParameters parameters;
	parameters.addParameter("ErrorMessage", errorMessage);

	xoap::MessageReference replyMessage = fe->VStateMachine::parentSupervisor_->SOAPMessenger::sendWithSOAPReply(
	    gatewaySupervisor, isPauseException ? "AsyncPauseException" : "AsyncError", parameters);

	std::stringstream replyMessageSStream;
	replyMessageSStream << SOAPUtilities::translate(replyMessage);
	__COUT__ << feHeader.str() << "Received... " << replyMessageSStream.str() << std::endl;

	if(replyMessageSStream.str().find("Fault") != std::string::npos)
	{
		__COUT_ERR__ << feHeader.str() << "Failure to indicate fault to Gateway..." << __E__;
		throw;
	}
}
catch(const xdaq::exception::Exception& e)
{
	if(isPauseException)
		__COUT__ << "SOAP message failure indicating front-end asynchronous running SOFT "
		            "error back to Gateway: "
		         << e.what() << __E__;
	else
		__COUT__ << "SOAP message failure indicating front-end asynchronous running "
		            "error back to Gateway: "
		         << e.what() << __E__;
	throw;  // rethrow and hope error is noticed
}
catch(...)
{
	if(isPauseException)
		__COUT__ << "Unknown error encounter indicating front-end asynchronous running "
		            "SOFT error back to Gateway."
		         << __E__;
	else
		__COUT__ << "Unknown error encounter indicating front-end asynchronous running "
		            "error back to Gateway."
		         << __E__;
	throw;  // rethrow and hope error is noticed
}  // end SendAsyncErrorToGateway()

//==============================================================================
// override WorkLoop::workLoopThread
//	return false to stop the workloop from calling the thread again
bool FEVInterface::workLoopThread(toolbox::task::WorkLoop* /*workLoop*/)
{
	try
	{
		continueWorkLoop_ = running(); /* in case users return false, without using continueWorkLoop_*/
	}
	catch(...)  //
	{
		// catch all, then rethrow with local variables needed
		__FE_SS__;

		bool isPauseException = false;
		bool isStopException  = false;

		try
		{
			throw;
		}
		catch(const __OTS_PAUSE_EXCEPTION__& e)
		{
			ss << "SOFT Exception was caught while running: " << e.what() << std::endl;
			isPauseException = true;
		}
		catch(const __OTS_STOP_EXCEPTION__& e)
		{
			ss << "STOP Exception was caught while running: " << e.what() << std::endl;
			isStopException = true;
		}
		catch(const std::runtime_error& e)
		{
			ss << "Caught an error during running at FE Interface '" << Configurable::theConfigurationRecordName_ << "': " << e.what() << __E__;
		}
		catch(...)
		{
			ss << "Caught an unknown error during running." << __E__;
			try	{ throw; } //one more try to printout extra info
			catch(const std::exception &e)
			{
				ss << "Exception message: " << e.what();
			}
			catch(...){}
		}

		// At this point, an asynchronous error has occurred
		//	during front-end running...
		// Send async error to Gateway
		//	Do this as thread so that workloop can end.

		__FE_COUT_ERR__ << ss.str();

		std::thread(
		    [](FEVInterface* fe, const std::string errorMessage, bool isPauseException, bool isStopException) {
			    FEVInterface::sendAsyncExceptionToGateway(fe, errorMessage, isPauseException, isStopException);
		    },
		    // pass the values
		    this /*fe*/,
		    ss.str() /*errorMessage*/,
		    isPauseException,
		    isStopException)
		    .detach();

		return false;
	}

	return continueWorkLoop_;
}  // end workLoopThread()

//==============================================================================
// registerFEMacroFunction
//	used by user-defined front-end interface implementations of this
//	virtual interface class to register their macro functions.
//
//	Front-end Macro Functions are then made accessible through the ots Control System
//	web interfaces. The menu consisting of all enabled FEs macros is assembled
//	by the FE Supervisor (and its FE Interface Manager).
void FEVInterface::registerFEMacroFunction(const std::string&              feMacroName,
                                           frontEndMacroFunction_t         feMacroFunction,
                                           const std::vector<std::string>& namesOfInputArgs,
                                           const std::vector<std::string>& namesOfOutputArgs,
                                           uint8_t                         requiredUserPermissions,
                                           const std::string&              allowedCallingFEs,
                                           const std::string&              feMacroTooltip)
{
	registerFEMacroFunction(
	    feMacroName, feMacroFunction, namesOfInputArgs, namesOfOutputArgs, std::to_string(requiredUserPermissions), allowedCallingFEs, feMacroTooltip);
}  // end registerFEMacroFunction()
void FEVInterface::registerFEMacroFunction(const std::string&              feMacroName,
                                           frontEndMacroFunction_t         feMacroFunction,
                                           const std::vector<std::string>& namesOfInputArgs,
                                           const std::vector<std::string>& namesOfOutputArgs,
                                           const std::string&              requiredUserPermissions,
                                           const std::string&              allowedCallingFEs,
                                           const std::string&              feMacroTooltip)
{
	if(mapOfFEMacroFunctions_.find(feMacroName) != mapOfFEMacroFunctions_.end())
	{
		__FE_SS__ << "feMacroName '" << feMacroName << "' already exists! Not allowed." << __E__;
		__FE_COUT_ERR__ << "\n" << ss.str();
		__FE_SS_THROW__;
	}

	mapOfFEMacroFunctions_.insert(std::pair<std::string, frontEndMacroStruct_t>(
	    feMacroName,
	    frontEndMacroStruct_t(feMacroName, feMacroFunction, namesOfInputArgs, namesOfOutputArgs, requiredUserPermissions, allowedCallingFEs, feMacroTooltip)));
}  // end registerFEMacroFunction()

//==============================================================================
// getFEMacroConstArgument
//	helper function for getting the value of an argument
//
//	Note: static function
const std::string& FEVInterface::getFEMacroConstArgument(frontEndMacroConstArgs_t& args, const std::string& argName)
{
	for(const frontEndMacroArg_t& pair : args)
	{
		if(pair.first == argName)
		{
			__COUT__ << "argName : " << pair.second << __E__;
			return pair.second;
		}
	}
	__SS__ << "Requested input argument not found with name '" << argName << "'" << __E__;
	__SS_THROW__;
}

//==============================================================================
// getFEMacroConstArgumentValue
//	helper function for getting the copy of the value of an argument
template<>
std::string ots::getFEMacroConstArgumentValue<std::string>(FEVInterface::frontEndMacroConstArgs_t& args, 
														   const std::string& argName,
														   const std::string& defaultValue)
{
	const std::string& data = FEVInterface::getFEMacroConstArgument(args, argName);

	// default value is used only if the user leave "Default"
	if (data == "Default")
		return defaultValue;
	
	return data;
}
//==============================================================================
// getFEMacroArgumentValue
//	helper function for getting the copy of the value of an argument
template<>
std::string ots::getFEMacroArgumentValue<std::string>(FEVInterface::frontEndMacroArgs_t& args, const std::string& argName)
{
	return FEVInterface::getFEMacroArgument(args, argName);
}

//==============================================================================
// getFEMacroOutputArgument
//	helper function for getting the value of an argument
//
//	Note: static function
std::string& FEVInterface::getFEMacroArgument(frontEndMacroArgs_t& args, const std::string& argName)
{
	for(std::pair<const std::string /* output arg name */, std::string /* arg output value */>& pair : args)
	{
		if(pair.first == argName)
			return pair.second;
	}
	__SS__ << "Requested argument not found with name '" << argName << "'" << __E__;
	__SS_THROW__;
}

//==============================================================================
// runSequenceOfCommands
//	runs a sequence of write commands from a linked section of the configuration tree
//		based on these fields:
//			- WriteAddress,  WriteValue, StartingBitPosition, BitFieldSize
void FEVInterface::runSequenceOfCommands(const std::string& treeLinkName)
{
	std::map<uint64_t, uint64_t> writeHistory;
	uint64_t                     writeAddress, writeValue, bitMask;
	uint8_t                      bitPosition;

	std::string writeBuffer;
	std::string readBuffer;
	char        msg[1000];
	bool        ignoreError = true;

	// ignore errors getting sequence of commands through tree (since it is optional)
	try
	{
		ConfigurationTree configSeqLink = theXDAQContextConfigTree_.getNode(theConfigurationPath_).getNode(treeLinkName);

		// but throw errors if problems executing the sequence of commands
		try
		{
			if(configSeqLink.isDisconnected())
				__FE_COUT__ << "Disconnected configure sequence" << __E__;
			else
			{
				__FE_COUT__ << "Handling configure sequence." << __E__;
				auto childrenMap = configSeqLink.getChildrenMap();
				for(const auto& child : childrenMap)
				{
					// WriteAddress and WriteValue fields

					writeAddress = child.second.getNode("WriteAddress").getValue<uint64_t>();
					writeValue   = child.second.getNode("WriteValue").getValue<uint64_t>();
					bitPosition  = child.second.getNode("StartingBitPosition").getValue<uint8_t>();
					bitMask      = (1 << child.second.getNode("BitFieldSize").getValue<uint8_t>()) - 1;

					writeValue &= bitMask;
					writeValue <<= bitPosition;
					bitMask = ~(bitMask << bitPosition);

					// place into write history
					if(writeHistory.find(writeAddress) == writeHistory.end())
						writeHistory[writeAddress] = 0;  // init to 0

					writeHistory[writeAddress] &= bitMask;     // clear incoming bits
					writeHistory[writeAddress] |= writeValue;  // add incoming bits

					sprintf(msg,
					        "\t Writing %s: \t %ld(0x%lX) \t %ld(0x%lX)",
					        child.first.c_str(),
					        writeAddress,
					        writeAddress,
					        writeHistory[writeAddress],
					        writeHistory[writeAddress]);

					__FE_COUT__ << msg << __E__;

					universalWrite((char*)&writeAddress, (char*)&(writeHistory[writeAddress]));
				}
			}
		}
		catch(...)
		{
			ignoreError = false;
			throw;
		}
	}
	catch(...)
	{
		if(!ignoreError)
			throw;
		// else ignoring error
		__FE_COUT__ << "Unable to access sequence of commands through configuration tree. "
		            << "Assuming no sequence. " << __E__;
	}
}  // end runSequenceOfCommands()

//==============================================================================
// runFrontEndMacro
//	Helper function to run this FEInterface's own front-end macro
// and gets the output arguments back.
//
//	Very similar to FEVInterfacesManager::runFEMacro()
//
//	Note: that argsOut are populated for caller, can just pass empty vector.
void FEVInterface::runSelfFrontEndMacro(const std::string& feMacroName,
                                        // not equivalent to __ARGS__
                                        //	left non-const value so caller can modify inputArgs as they are being created
                                        const std::vector<FEVInterface::frontEndMacroArg_t>& argsIn,
                                        std::vector<FEVInterface::frontEndMacroArg_t>&       argsOut)
{
	// have pointer to virtual FEInterface, find Macro structure
	auto FEMacroIt = this->getMapOfFEMacroFunctions().find(feMacroName);
	if(FEMacroIt == this->getMapOfFEMacroFunctions().end())
	{
		__CFG_SS__ << "FE Macro '" << feMacroName << "' of interfaceID '" << getInterfaceUID() << "' was not found!" << __E__;
		__CFG_COUT_ERR__ << "\n" << ss.str();
		__CFG_SS_THROW__;
	}
	const FEVInterface::frontEndMacroStruct_t& feMacro = FEMacroIt->second;

	// check for input arg name match
	for(unsigned int i = 0; i < argsIn.size(); ++i)
		if(argsIn[i].first != feMacro.namesOfInputArguments_[i])
		{
			__CFG_SS__ << "FE Macro '" << feMacro.feMacroName_ << "' of interfaceID '" << getInterfaceUID() << "' was attempted with a mismatch in"
			           << " a name of an input argument. " << argsIn[i].first << " was given. " << feMacro.namesOfInputArguments_[i] << " expected." << __E__;
			__CFG_COUT_ERR__ << "\n" << ss.str();
			__CFG_SS_THROW__;
		}

	// check namesOfInputArguments_
	if(feMacro.namesOfInputArguments_.size() != argsIn.size())
	{
		__CFG_SS__ << "FE Macro '" << feMacro.feMacroName_ << "' of interfaceID '" << getInterfaceUID() << "' was attempted with a mismatch in"
		           << " number of input arguments. " << argsIn.size() << " were given. " << feMacro.namesOfInputArguments_.size() << " expected." << __E__;
		__CFG_COUT_ERR__ << "\n" << ss.str();
		__CFG_SS_THROW__;
	}

	__CFG_COUT__ << "# of input args = " << argsIn.size() << __E__;
	for(auto& argIn : argsIn)
		__CFG_COUT__ << argIn.first << ": " << argIn.second << __E__;

	__CFG_COUT__ << "Launching FE Macro '" << feMacro.feMacroName_ << "' ..." << __E__;

	argsOut.clear();
	for(unsigned int i = 0; i < feMacro.namesOfOutputArguments_.size(); ++i)
		argsOut.push_back(FEVInterface::frontEndMacroArg_t(feMacro.namesOfOutputArguments_[i], "DEFAULT"));

	// run it!
	(this->*(feMacro.macroFunction_))(feMacro, argsIn, argsOut);

	__CFG_COUT__ << "FE Macro complete!" << __E__;

	__CFG_COUT__ << "# of output args = " << argsOut.size() << __E__;
	for(const auto& arg : argsOut)
		__CFG_COUT__ << arg.first << ": " << arg.second << __E__;

}  // end runSelfFrontEndMacro()

//==============================================================================
// runFrontEndMacro
//	run a front-end macro in the target interface plug-in and gets the output arguments
// back
void FEVInterface::runFrontEndMacro(const std::string&                                   targetInterfaceID,
                                    const std::string&                                   feMacroName,
                                    const std::vector<FEVInterface::frontEndMacroArg_t>& inputArgs,
                                    std::vector<FEVInterface::frontEndMacroArg_t>&       outputArgs) const
{
	__FE_COUTV__(targetInterfaceID);
	__FE_COUTV__(VStateMachine::parentSupervisor_);

	std::string inputArgsStr = StringMacros::vectorToString(inputArgs, ";" /*primaryDelimeter*/, "," /*secondaryDelimeter*/);

	__FE_COUTV__(inputArgsStr);

	xoap::MessageReference message = SOAPUtilities::makeSOAPMessageReference("FECommunication");

	SOAPParameters parameters;
	parameters.addParameter("type", "feMacro");
	parameters.addParameter("requester", FEVInterface::interfaceUID_);
	parameters.addParameter("targetInterfaceID", targetInterfaceID);
	parameters.addParameter("feMacroName", feMacroName);
	parameters.addParameter("inputArgs", inputArgsStr);
	SOAPUtilities::addParameters(message, parameters);

	__FE_COUT__ << "Sending FE communication: " << SOAPUtilities::translate(message) << __E__;

	xoap::MessageReference replyMessage = VStateMachine::parentSupervisor_->SOAPMessenger::sendWithSOAPReply(
	    VStateMachine::parentSupervisor_->allSupervisorInfo_.getAllMacroMakerTypeSupervisorInfo().begin()->second.getDescriptor(), message);

	__FE_COUT__ << "Response received: " << SOAPUtilities::translate(replyMessage) << __E__;

	SOAPParameters rxParameters;
	rxParameters.addParameter("Error");
	SOAPUtilities::receive(replyMessage, rxParameters);

	std::string error = rxParameters.getValue("Error");

	if(error != "")
	{
		// error occurred!
		__FE_SS__ << "Error transmitting request to target interface '" << targetInterfaceID << "' from '" << FEVInterface::interfaceUID_ << ".' " << error
		          << __E__;
		__FE_SS_THROW__;
	}

	// extract output args
	SOAPParameters argsOutParameter;
	argsOutParameter.addParameter("outputArgs");
	SOAPUtilities::receive(replyMessage, argsOutParameter);

	std::string    outputArgsStr = argsOutParameter.getValue("outputArgs");
	std::set<char> pairDelimiter({';'}), nameValueDelimiter({','});

	std::map<std::string, std::string> mapToReturn;
	StringMacros::getMapFromString(outputArgsStr, mapToReturn, pairDelimiter, nameValueDelimiter);

	outputArgs.clear();
	for(auto& mapPair : mapToReturn)
		outputArgs.push_back(mapPair);

}  // end runFrontEndMacro()

//==============================================================================
// receiveFromFrontEnd
//	specialized template function for T=std::string
//
//	Note: requester can be a wildcard string as defined in StringMacros
void FEVInterface::receiveFromFrontEnd(const std::string& requester, std::string& retValue, unsigned int timeoutInSeconds) const
{
	__FE_COUTV__(requester);
	__FE_COUTV__(parentSupervisor_);

	std::string data = "0";
	// bool        found = false;
	while(1)
	{
		// mutex scope
		{
			std::lock_guard<std::mutex> lock(parentInterfaceManager_->frontEndCommunicationReceiveMutex_);

			auto receiveBuffersForTargetIt = parentInterfaceManager_->frontEndCommunicationReceiveBuffer_.find(FEVInterface::interfaceUID_);
			if(receiveBuffersForTargetIt != parentInterfaceManager_->frontEndCommunicationReceiveBuffer_.end())
			{
				__FE_COUT__ << "Number of source buffers found for front-end '" << FEVInterface::interfaceUID_
				            << "': " << receiveBuffersForTargetIt->second.size() << __E__;

				for(auto& buffPair : receiveBuffersForTargetIt->second)
					__FE_COUTV__(buffPair.first);

				// match requester to map of buffers
				std::string                        sourceBufferId = "";
				std::queue<std::string /*value*/>& sourceBuffer =
				    StringMacros::getWildCardMatchFromMap(requester, receiveBuffersForTargetIt->second, &sourceBufferId);

				__FE_COUT__ << "Found source buffer '" << sourceBufferId << "' with size " << sourceBuffer.size() << __E__;

				if(sourceBuffer.size())
				{
					__FE_COUT__ << "Found a value in queue of size " << sourceBuffer.size() << __E__;

					// remove from receive buffer
					retValue = sourceBuffer.front();
					sourceBuffer.pop();
					return;
				}
				else
					__FE_COUT__ << "Source buffer empty for '" << requester << "'" << __E__;
			}

			// else, not found...

			// if out of time, throw error
			if(!timeoutInSeconds)
			{
				__FE_SS__ << "Timeout (" << timeoutInSeconds << " s) waiting for front-end communication from " << requester << "." << __E__;
				__FE_SS_THROW__;
			}
			// else, there is still hope

		}  // end mutex scope

		// else try again in a sec
		__FE_COUT__ << "Waiting for front-end communication from " << requester << " for " << timeoutInSeconds << " more seconds..." << __E__;

		--timeoutInSeconds;
		sleep(1);  // wait a sec
	}              // end timeout loop

	// should never get here
}  // end receiveFromFrontEnd()

//==============================================================================
// receiveFromFrontEnd
//	specialized template function for T=std::string
//	Note: if called without template <T> syntax, necessary because types of
// std::basic_string<char> cause compiler problems if no string specific function
std::string FEVInterface::receiveFromFrontEnd(const std::string& requester, unsigned int timeoutInSeconds) const
{
	std::string retValue;
	FEVInterface::receiveFromFrontEnd(requester, retValue, timeoutInSeconds);
	return retValue;
}  // end receiveFromFrontEnd()

//==============================================================================
// macroStruct_t constructor
FEVInterface::macroStruct_t::macroStruct_t(const std::string& macroString)
{
	__COUTV__(macroString);

	// example macro string:
	//	{"name":"testPublic","sequence":"0:w:1001:writeVal,1:r:1001:","time":"Sat Feb 0
	//	9 2019 10:42:03 GMT-0600 (Central Standard Time)","notes":"","LSBF":"false"}@

	std::vector<std::string> extractVec;
	StringMacros::getVectorFromString(macroString, extractVec, {'"'});

	__COUTV__(StringMacros::vectorToString(extractVec, " ||| "));

	enum
	{
		MACRONAME_NAME_INDEX  = 1,
		MACRONAME_VALUE_INDEX = 3,
		SEQUENCE_NAME_INDEX   = 5,
		SEQUENCE_VALUE_INDEX  = 7,
		LSBF_NAME_INDEX       = 17,
		LSFBF_VALUE_INDEX     = 19,
	};

	// verify fields in sequence (for sanity)
	if(MACRONAME_NAME_INDEX >= extractVec.size() || extractVec[MACRONAME_NAME_INDEX] != "name")
	{
		__SS__ << "Invalid sequence, 'name' expected in position " << MACRONAME_NAME_INDEX << __E__;
		__SS_THROW__;
	}
	if(SEQUENCE_NAME_INDEX >= extractVec.size() || extractVec[SEQUENCE_NAME_INDEX] != "sequence")
	{
		__SS__ << "Invalid sequence, 'sequence' expected in position " << SEQUENCE_NAME_INDEX << __E__;
		__SS_THROW__;
	}
	if(LSBF_NAME_INDEX >= extractVec.size() || extractVec[LSBF_NAME_INDEX] != "LSBF")
	{
		__SS__ << "Invalid sequence, 'LSBF' expected in position " << LSBF_NAME_INDEX << __E__;
		__SS_THROW__;
	}
	macroName_ = extractVec[MACRONAME_VALUE_INDEX];
	__COUTV__(macroName_);
	lsbf_ = extractVec[LSFBF_VALUE_INDEX] == "false" ? false : true;
	__COUTV__(lsbf_);
	std::string& sequence = extractVec[SEQUENCE_VALUE_INDEX];
	__COUTV__(sequence);

	std::vector<std::string> sequenceCommands;
	StringMacros::getVectorFromString(sequence, sequenceCommands, {','});

	__COUTV__(StringMacros::vectorToString(sequenceCommands, " ### "));

	for(auto& command : sequenceCommands)
	{
		__COUTV__(command);

		// Note: the only way to distinguish between variable and data
		//	is hex characters or not (lower and upper case allowed for hex)

		std::vector<std::string> commandPieces;
		StringMacros::getVectorFromString(command, commandPieces, {':'});

		__COUTV__(StringMacros::vectorToString(commandPieces, " ### "));

		__COUTV__(commandPieces.size());

		// command format
		//	index | type | address/sleep[ms] | data
		// d is delay (1+2)
		// w is write (1+3)
		// r is read  (1+2/3 with arg)

		// extract input arguments, as the variables in address/data fields
		// extract output arguments, as the variables in read data fields

		if(commandPieces.size() < 3 || commandPieces.size() > 4 || commandPieces[1].size() != 1)
		{
			__SS__ << "Invalid command type specified in command string: " << command << __E__;
			__SS_THROW__;
		}

		//==========================
		// Use lambda to identify variable name in field
		std::function<bool(const std::string& /*field value*/
		                   )>
		    localIsVariable = [/*capture variable*/](const std::string& fieldValue) {
			    // create local message facility subject
			    std::string mfSubject_ = "isVar";
			    __GEN_COUTV__(fieldValue);

			    // return false if all hex characters found
			    for(const auto& c : fieldValue)
				    if(!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F')))
					    return true;  // is variable name!
			    return false;         // else is a valid hex string, so not variable name
		    };                        // end local lambda localIsVariable()

		if(commandPieces[1][0] == 'r' && commandPieces.size() == 4)  // read type
		{
			__COUT__ << "Read type found." << __E__;
			// 2: address or optional variable name
			// 3: optional variable name

			operations_.push_back(std::make_pair(macroStruct_t::OP_TYPE_READ, readOps_.size()));

			readOps_.push_back(macroStruct_t::readOp_t());
			readOps_.back().addressIsVar_ = localIsVariable(commandPieces[2]);
			readOps_.back().dataIsVar_    = localIsVariable(commandPieces[3]);

			if(!readOps_.back().addressIsVar_)
			{
				if(lsbf_)  // flip byte order
				{
					std::string lsbfData = "";

					// always add leading 0 to guarantee do not miss data
					commandPieces[2] = "0" + commandPieces[2];
					for(unsigned int i = 0; i < commandPieces[2].size() / 2; ++i)
					{
						__COUTV__(commandPieces[2].size() - 2 * (i + 1));
						// add one byte at a time, backwards
						lsbfData += commandPieces[2][commandPieces[2].size() - 2 * (i + 1)];
						lsbfData += commandPieces[2][commandPieces[2].size() - 2 * (i + 1) + 1];
						__COUTV__(lsbfData);
					}
					__COUTV__(lsbfData);
					StringMacros::getNumber("0x" + lsbfData, readOps_.back().address_);
				}
				else
					StringMacros::getNumber("0x" + commandPieces[2], readOps_.back().address_);
			}
			else
			{
				readOps_.back().addressVarName_ = commandPieces[2];
				__COUTV__(readOps_.back().addressVarName_);

				namesOfInputArguments_.emplace(readOps_.back().addressVarName_);
			}

			if(readOps_.back().dataIsVar_)
			{
				readOps_.back().dataVarName_ = commandPieces[3];
				__COUTV__(readOps_.back().dataVarName_);

				namesOfOutputArguments_.emplace(readOps_.back().dataVarName_);
			}
		}
		else if(commandPieces[1][0] == 'w' && commandPieces.size() == 4)  // write type
		{
			__COUT__ << "Write type found." << __E__;
			// 2: address or optional variable name
			// 3: data or optional variable name

			operations_.push_back(std::make_pair(macroStruct_t::OP_TYPE_WRITE, writeOps_.size()));

			writeOps_.push_back(macroStruct_t::writeOp_t());
			writeOps_.back().addressIsVar_ = localIsVariable(commandPieces[2]);
			writeOps_.back().dataIsVar_    = localIsVariable(commandPieces[3]);

			if(!writeOps_.back().addressIsVar_)
			{
				if(lsbf_)  // flip byte order
				{
					std::string lsbfData = "";

					// always add leading 0 to guarantee do not miss data
					commandPieces[2] = "0" + commandPieces[2];
					for(unsigned int i = 0; i < commandPieces[2].size() / 2; ++i)
					{
						__COUTV__(commandPieces[2].size() - 2 * (i + 1));
						// add one byte at a time, backwards
						lsbfData += commandPieces[2][commandPieces[2].size() - 2 * (i + 1)];
						lsbfData += commandPieces[2][commandPieces[2].size() - 2 * (i + 1) + 1];
						__COUTV__(lsbfData);
					}
					__COUTV__(lsbfData);
					StringMacros::getNumber("0x" + lsbfData, writeOps_.back().address_);
				}
				else
					StringMacros::getNumber("0x" + commandPieces[2], writeOps_.back().address_);
			}
			else
			{
				writeOps_.back().addressVarName_ = commandPieces[2];
				__COUTV__(writeOps_.back().addressVarName_);

				namesOfInputArguments_.emplace(writeOps_.back().addressVarName_);
			}

			if(!writeOps_.back().dataIsVar_)
			{
				if(lsbf_)  // flip byte order
				{
					std::string lsbfData = "";

					// always add leading 0 to guarantee do not miss data
					commandPieces[2] = "0" + commandPieces[3];
					for(unsigned int i = 0; i < commandPieces[3].size() / 2; ++i)
					{
						__COUTV__(commandPieces[3].size() - 2 * (i + 1));
						// add one byte at a time, backwards
						lsbfData += commandPieces[3][commandPieces[3].size() - 2 * (i + 1)];
						lsbfData += commandPieces[3][commandPieces[3].size() - 2 * (i + 1) + 1];
						__COUTV__(lsbfData);
					}
					__COUTV__(lsbfData);
					StringMacros::getNumber("0x" + lsbfData, writeOps_.back().data_);
				}
				else
					StringMacros::getNumber("0x" + commandPieces[3], writeOps_.back().data_);
			}
			else
			{
				writeOps_.back().dataVarName_ = commandPieces[3];
				__COUTV__(writeOps_.back().dataVarName_);

				namesOfInputArguments_.emplace(writeOps_.back().dataVarName_);
			}
		}
		else if(commandPieces[1][0] == 'd' && commandPieces.size() == 3)  // delay type
		{
			__COUT__ << "Delay type found." << __E__;
			// 2: delay[ms] or optional variable name

			operations_.push_back(std::make_pair(macroStruct_t::OP_TYPE_DELAY, delayOps_.size()));

			delayOps_.push_back(macroStruct_t::delayOp_t());
			delayOps_.back().delayIsVar_ = localIsVariable(commandPieces[2]);

			if(!delayOps_.back().delayIsVar_)
				StringMacros::getNumber("0x" + commandPieces[2], delayOps_.back().delay_);
			else
			{
				delayOps_.back().delayVarName_ = commandPieces[2];
				__COUTV__(delayOps_.back().delayVarName_);

				namesOfInputArguments_.emplace(delayOps_.back().delayVarName_);
			}
		}
		else  // invalid type
		{
			__SS__ << "Invalid command type '" << commandPieces[1][0] << "' specified with " << commandPieces.size() << " components." << __E__;
			__SS_THROW__;
		}

	}  // end sequence commands extraction loop

	__COUT__ << operations_.size() << " operations extracted: \n\t" << readOps_.size() << " reads \n\t" << writeOps_.size() << " writes \n\t"
	         << delayOps_.size() << " delays" << __E__;

	__COUT__ << "Input arguments: " << __E__;
	for(const auto& inputArg : namesOfInputArguments_)
		__COUT__ << "\t" << inputArg << __E__;

	__COUT__ << "Output arguments: " << __E__;
	for(const auto& outputArg : namesOfOutputArguments_)
		__COUT__ << "\t" << outputArg << __E__;

}  // end macroStruct_t constructor

//==============================================================================
// runMacro
void FEVInterface::runMacro(FEVInterface::macroStruct_t& macro, std::map<std::string /*name*/, uint64_t /*value*/>& variableMap)
{
	// Similar to FEVInterface::runSequenceOfCommands()

	__FE_COUT__ << "Running Macro '" << macro.macroName_ << "' of " << macro.operations_.size() << " operations." << __E__;

	for(auto& op : macro.operations_)
	{
		if(op.first == macroStruct_t::OP_TYPE_READ)
		{
			__FE_COUT__ << "Doing read op..." << __E__;
			macroStruct_t::readOp_t& readOp = macro.readOps_[op.second];
			if(readOp.addressIsVar_)
			{
				__FE_COUTV__(readOp.addressVarName_);
				readOp.address_ = variableMap.at(readOp.addressVarName_);
			}

			uint64_t dataValue = 0;

			__FE_COUT__ << std::hex << "Read address: \t 0x" << readOp.address_ << __E__ << std::dec;

			universalRead((char*)&readOp.address_, (char*)&dataValue);

			__FE_COUT__ << std::hex << "Read data: \t 0x" << dataValue << __E__ << std::dec;

			if(readOp.dataIsVar_)
			{
				__FE_COUTV__(readOp.dataVarName_);
				variableMap.at(readOp.dataVarName_) = dataValue;
			}

		}  // end read op
		else if(op.first == macroStruct_t::OP_TYPE_WRITE)
		{
			__FE_COUT__ << "Doing write op..." << __E__;
			macroStruct_t::writeOp_t& writeOp = macro.writeOps_[op.second];
			if(writeOp.addressIsVar_)
			{
				__FE_COUTV__(writeOp.addressVarName_);
				writeOp.address_ = variableMap.at(writeOp.addressVarName_);
			}
			if(writeOp.dataIsVar_)
			{
				__FE_COUTV__(writeOp.dataVarName_);
				writeOp.data_ = variableMap.at(writeOp.dataVarName_);
			}

			__FE_COUT__ << std::hex << "Write address: \t 0x" << writeOp.address_ << __E__ << std::dec;
			__FE_COUT__ << std::hex << "Write data: \t 0x" << writeOp.data_ << __E__ << std::dec;

			universalWrite((char*)&writeOp.address_, (char*)&writeOp.data_);

		}  // end write op
		else if(op.first == macroStruct_t::OP_TYPE_DELAY)
		{
			__FE_COUT__ << "Doing delay op..." << __E__;

			macroStruct_t::delayOp_t& delayOp = macro.delayOps_[op.second];
			if(delayOp.delayIsVar_)
			{
				__FE_COUTV__(delayOp.delayVarName_);
				delayOp.delay_ = variableMap.at(delayOp.delayVarName_);
			}

			__FE_COUT__ << std::dec << "Delay ms: \t " << delayOp.delay_ << __E__;

			usleep(delayOp.delay_ /*ms*/ * 1000);

		}     // end delay op
		else  // invalid type
		{
			__FE_SS__ << "Invalid command type '" << op.first << "!'" << __E__;
			__FE_SS_THROW__;
		}

	}  // end operations loop

}  // end runMacro
