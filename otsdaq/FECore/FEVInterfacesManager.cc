#include "otsdaq/FECore/FEVInterfacesManager.h"
#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/FECore/MakeInterface.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include "artdaq-core/Utilities/configureMessageFacility.hh"
#include "artdaq/BuildInfo/GetPackageBuildInfo.hh"
#include "fhiclcpp/make_ParameterSet.h"
#include "messagefacility/MessageLogger/MessageLogger.h"

#include <iostream>
#include <sstream>
#include <thread>  //for std::thread

using namespace ots;

//==============================================================================
FEVInterfacesManager::FEVInterfacesManager(const ConfigurationTree& theXDAQContextConfigTree, const std::string& supervisorConfigurationPath)
    : Configurable(theXDAQContextConfigTree, supervisorConfigurationPath), VStateMachine(Configurable::theConfigurationRecordName_)
{
	init();
	__CFG_COUT__ << "Constructed." << __E__;
}

//==============================================================================
FEVInterfacesManager::~FEVInterfacesManager(void)
{
	destroy();
	__CFG_COUT__ << "Destructed." << __E__;
}

//==============================================================================
void FEVInterfacesManager::init(void) {}

//==============================================================================
void FEVInterfacesManager::destroy(void)
{
	for(auto& it : theFEInterfaces_)
		it.second.reset();

	theFEInterfaces_.clear();
	theFENamesByPriority_.clear();
}

//==============================================================================
void FEVInterfacesManager::createInterfaces(void)
{
	const std::string COL_NAME_feGroupLink = "LinkToFEInterfaceTable";
	const std::string COL_NAME_feTypeLink  = "LinkToFETypeTable";
	const std::string COL_NAME_fePlugin    = "FEInterfacePluginName";

	__CFG_COUT__ << "Path: " << theConfigurationPath_ + "/" + COL_NAME_feGroupLink << __E__;

	destroy();

	{  // could access application node like so, ever needed?
		ConfigurationTree appNode = theXDAQContextConfigTree_.getBackNode(theConfigurationPath_, 1);
		__CFG_COUTV__(appNode.getValueAsString());

		auto fes = appNode.getNode("LinkToSupervisorTable").getNode("LinkToFEInterfaceTable").getChildrenNames(true /*byPriority*/, true /*onlyStatusTrue*/);
		__CFG_COUTV__(StringMacros::vectorToString(fes));
	}

	ConfigurationTree feGroupLinkNode = Configurable::getSelfNode().getNode(COL_NAME_feGroupLink);

	std::vector<std::pair<std::string, ConfigurationTree>> feChildren = feGroupLinkNode.getChildren();

	// acquire names by priority
	theFENamesByPriority_ = feGroupLinkNode.getChildrenNames(true /*byPriority*/, true /*onlyStatusTrue*/);
	__CFG_COUTV__(StringMacros::vectorToString(theFENamesByPriority_));

	for(const auto& interface : feChildren)
	{
		try
		{
			if(!interface.second.getNode(TableViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
				continue;
		}
		catch(...)  // if Status column not there ignore (for backwards compatibility)
		{
			__CFG_COUT_INFO__ << "Ignoring FE Status since Status column is missing!" << __E__;
		}

		__CFG_COUT__ << "Interface Plugin Name: " << interface.second.getNode(COL_NAME_fePlugin).getValue<std::string>() << __E__;
		__CFG_COUT__ << "Interface Name: " << interface.first << __E__;
		__CFG_COUT__ << "XDAQContext Node: " << theXDAQContextConfigTree_ << __E__;
		__CFG_COUT__ << "Path to configuration: " << (theConfigurationPath_ + "/" + COL_NAME_feGroupLink + "/" + interface.first + "/" + COL_NAME_feTypeLink)
		             << __E__;

		try
		{
			theFEInterfaces_[interface.first] =
			    makeInterface(interface.second.getNode(COL_NAME_fePlugin).getValue<std::string>(),
			                  interface.first,
			                  theXDAQContextConfigTree_,
			                  (theConfigurationPath_ + "/" + COL_NAME_feGroupLink + "/" + interface.first + "/" + COL_NAME_feTypeLink));

			// setup parent supervisor and interface manager
			//	of FEVinterface (for backwards compatibility, left out of constructor)
			theFEInterfaces_[interface.first]->VStateMachine::parentSupervisor_ = VStateMachine::parentSupervisor_;
			theFEInterfaces_[interface.first]->parentInterfaceManager_          = this;
		}
		catch(const cet::exception& e)
		{
			__CFG_SS__ << "Failed to instantiate plugin named '" << interface.first << "' of type '"
			           << interface.second.getNode(COL_NAME_fePlugin).getValue<std::string>() << "' due to the following error: \n"
			           << e.what() << __E__;
			__MOUT_ERR__ << ss.str();
			__CFG_SS_THROW__;
		}
		catch(const std::runtime_error& e)
		{
			__CFG_SS__ << "Failed to instantiate plugin named '" << interface.first << "' of type '"
			           << interface.second.getNode(COL_NAME_fePlugin).getValue<std::string>() << "' due to the following error: \n"
			           << e.what() << __E__;
			__MOUT_ERR__ << ss.str();
			__CFG_SS_THROW__;
		}
		catch(...)
		{
			__CFG_SS__ << "Failed to instantiate plugin named '" << interface.first << "' of type '"
			           << interface.second.getNode(COL_NAME_fePlugin).getValue<std::string>() << "' due to an unknown error." << __E__;
			try	{ throw; } //one more try to printout extra info
			catch(const std::exception &e)
			{
				ss << "Exception message: " << e.what();
			}
			catch(...){}
			__MOUT_ERR__ << ss.str();
			throw;  // if we do not throw, it is hard to tell what is happening..
			        //__CFG_SS_THROW__;
		}
	}
	__CFG_COUT__ << "Done creating interfaces" << __E__;
}  // end createInterfaces()

//==============================================================================
// virtual progress string that can be overridden with more info
//	e.g. steps and substeps
//	however integer 0-100 should be first number, then separated by : colons
//	e.g. 94:FE0:1:2
std::string FEVInterfacesManager::getStatusProgressDetail(void)
{
	std::string  progress = "";
	unsigned int cnt      = 0;
	//__CFG_COUTV__(theFENamesByPriority_.size());
	//__CFG_COUTV__(VStateMachine::getTransitionName());
	for(unsigned int i = 0; i < theFENamesByPriority_.size(); ++i)
		try
		{
			const std::string& name       = theFENamesByPriority_[i];
			FEVInterface*      fe         = getFEInterfaceP(name);
			std::string        feProgress = fe->getStatusProgressDetail();
			if(feProgress.size())
				progress += ((cnt++) ? "," : "") + StringMacros::encodeURIComponent(feProgress);
		}
		catch(...)
		{
		}  // ignore errors

	// if(progress.size())
	//	__CFG_COUTV__(progress);

	return progress;
}  // end getStatusProgressString()

//==============================================================================
void FEVInterfacesManager::configure(void)
{
	const std::string transitionName = "Configuring";

	__CFG_COUT__ << transitionName << " FEVInterfacesManager " << __E__;

	// create interfaces (the first iteration)
	if(VStateMachine::getIterationIndex() == 0 && VStateMachine::getSubIterationIndex() == 0)
		createInterfaces();  // by priority

	FEVInterface* fe;

	preStateMachineExecutionLoop();
	for(unsigned int i = 0; i < theFENamesByPriority_.size(); ++i)
	{
		// if one state machine is doing a sub-iteration, then target that one
		if(subIterationWorkStateMachineIndex_ != (unsigned int)-1 && i != subIterationWorkStateMachineIndex_)
			continue;  // skip those not in the sub-iteration

		const std::string& name = theFENamesByPriority_[i];

		// test for front-end existence
		fe = getFEInterfaceP(name);

		if(stateMachinesIterationDone_[name])
			continue;  // skip state machines already done

		__CFG_COUT__ << transitionName << " interface " << name << __E__;
		__CFG_COUT__ << transitionName << " interface " << name << __E__;
		__CFG_COUT__ << transitionName << " interface " << name << __E__;

		preStateMachineExecution(i, transitionName);
		fe->configure();
		postStateMachineExecution(i);

		// when done with fe configure, configure slow controls
		if(!fe->VStateMachine::getSubIterationWork() && !fe->VStateMachine::getIterationWork())
		{
			// configure slow controls and start slow controls workloop
			//	slow controls workloop stays alive through start/stop.. and dies on halt
			fe->configureSlowControls();
			fe->startSlowControlsWorkLoop();

			__CFG_COUT__ << "Done " << transitionName << " interface " << name << __E__;
			__CFG_COUT__ << "Done " << transitionName << " interface " << name << __E__;
			__CFG_COUT__ << "Done " << transitionName << " interface " << name << __E__;
		}
	}
	postStateMachineExecutionLoop();

	__CFG_COUT__ << "Done " << transitionName << " all interfaces." << __E__;
}  // end configure()

//==============================================================================
void FEVInterfacesManager::halt(void)
{
	const std::string transitionName = "Halting";
	FEVInterface*     fe;

	preStateMachineExecutionLoop();
	for(unsigned int i = 0; i < theFENamesByPriority_.size(); ++i)
	{
		// if one state machine is doing a sub-iteration, then target that one
		if(subIterationWorkStateMachineIndex_ != (unsigned int)-1 && i != subIterationWorkStateMachineIndex_)
			continue;  // skip those not in the sub-iteration

		const std::string& name = theFENamesByPriority_[i];

		fe = getFEInterfaceP(name);

		if(stateMachinesIterationDone_[name])
			continue;  // skip state machines already done

		__CFG_COUT__ << transitionName << " interface " << name << __E__;
		__CFG_COUT__ << transitionName << " interface " << name << __E__;
		__CFG_COUT__ << transitionName << " interface " << name << __E__;

		preStateMachineExecution(i, transitionName);

		// since halting also occurs on errors, ignore more errors
		try
		{
			fe->stopWorkLoop();
		}
		catch(...)
		{
			__CFG_COUT_WARN__ << "An error occurred while halting the front-end workloop for '" << name << ",' ignoring." << __E__;
		}

		// since halting also occurs on errors, ignore more errors
		try
		{
			fe->stopSlowControlsWorkLoop();
		}
		catch(...)
		{
			__CFG_COUT_WARN__ << "An error occurred while halting the Slow Controls "
			                     "front-end workloop for '"
			                  << name << ",' ignoring." << __E__;
		}

		// since halting also occurs on errors, ignore more errors
		try
		{
			fe->halt();
		}
		catch(...)
		{
			__CFG_COUT_WARN__ << "An error occurred while halting the front-end '" << name << ",' ignoring." << __E__;
		}

		postStateMachineExecution(i);

		__CFG_COUT__ << "Done " << transitionName << " interface " << name << __E__;
		__CFG_COUT__ << "Done " << transitionName << " interface " << name << __E__;
		__CFG_COUT__ << "Done " << transitionName << " interface " << name << __E__;
	}
	postStateMachineExecutionLoop();

	if(!VStateMachine::getSubIterationWork() && !VStateMachine::getIterationWork())
		destroy();  // destroy all FE interfaces on halt, must be configured for FE interfaces to exist

	__CFG_COUT__ << "Done " << transitionName << " all interfaces." << __E__;
}  // end halt()

//==============================================================================
void FEVInterfacesManager::pause(void)
{
	const std::string transitionName = "Pausing";
	FEVInterface*     fe;

	preStateMachineExecutionLoop();
	for(unsigned int i = 0; i < theFENamesByPriority_.size(); ++i)
	{
		// if one state machine is doing a sub-iteration, then target that one
		if(subIterationWorkStateMachineIndex_ != (unsigned int)-1 && i != subIterationWorkStateMachineIndex_)
			continue;  // skip those not in the sub-iteration

		const std::string& name = theFENamesByPriority_[i];

		fe = getFEInterfaceP(name);

		if(stateMachinesIterationDone_[name])
			continue;  // skip state machines already done

		__CFG_COUT__ << transitionName << " interface " << name << __E__;
		__CFG_COUT__ << transitionName << " interface " << name << __E__;
		__CFG_COUT__ << transitionName << " interface " << name << __E__;

		preStateMachineExecution(i, transitionName);
		fe->stopWorkLoop();
		fe->pause();
		postStateMachineExecution(i);

		__CFG_COUT__ << "Done " << transitionName << " interface " << name << __E__;
		__CFG_COUT__ << "Done " << transitionName << " interface " << name << __E__;
		__CFG_COUT__ << "Done " << transitionName << " interface " << name << __E__;
	}
	postStateMachineExecutionLoop();

	__CFG_COUT__ << "Done " << transitionName << " all interfaces." << __E__;
}  // end pause()

//==============================================================================
void FEVInterfacesManager::resume(void)
{
	const std::string transitionName = "Resuming";
	// FEVInterface*     fe;

	preStateMachineExecutionLoop();
	for(unsigned int i = 0; i < theFENamesByPriority_.size(); ++i)
	{
		// if one state machine is doing a sub-iteration, then target that one
		if(subIterationWorkStateMachineIndex_ != (unsigned int)-1 && i != subIterationWorkStateMachineIndex_)
			continue;  // skip those not in the sub-iteration

		const std::string& name = theFENamesByPriority_[i];

		FEVInterface* fe = getFEInterfaceP(name);

		if(stateMachinesIterationDone_[name])
			continue;  // skip state machines already done

		__CFG_COUT__ << transitionName << " interface " << name << __E__;
		__CFG_COUT__ << transitionName << " interface " << name << __E__;
		__CFG_COUT__ << transitionName << " interface " << name << __E__;

		preStateMachineExecution(i, transitionName);
		fe->resume();
		// only start workloop once transition is done
		if(postStateMachineExecution(i))
			fe->startWorkLoop();

		__CFG_COUT__ << "Done " << transitionName << " interface " << name << __E__;
		__CFG_COUT__ << "Done " << transitionName << " interface " << name << __E__;
		__CFG_COUT__ << "Done " << transitionName << " interface " << name << __E__;
	}
	postStateMachineExecutionLoop();

	__CFG_COUT__ << "Done " << transitionName << " all interfaces." << __E__;

}  // end resume()

//==============================================================================
void FEVInterfacesManager::start(std::string runNumber)
{
	const std::string transitionName = "Starting";
	FEVInterface*     fe;

	preStateMachineExecutionLoop();
	for(unsigned int i = 0; i < theFENamesByPriority_.size(); ++i)
	{
		// if one state machine is doing a sub-iteration, then target that one
		if(subIterationWorkStateMachineIndex_ != (unsigned int)-1 && i != subIterationWorkStateMachineIndex_)
			continue;  // skip those not in the sub-iteration

		const std::string& name = theFENamesByPriority_[i];

		fe = getFEInterfaceP(name);

		if(stateMachinesIterationDone_[name])
			continue;  // skip state machines already done

		__CFG_COUT__ << transitionName << " interface " << name << __E__;
		__CFG_COUT__ << transitionName << " interface " << name << __E__;
		__CFG_COUT__ << transitionName << " interface " << name << __E__;

		preStateMachineExecution(i, transitionName);
		fe->start(runNumber);
		// only start workloop once transition is done
		if(postStateMachineExecution(i))
			fe->startWorkLoop();

		__CFG_COUT__ << "Done " << transitionName << " interface " << name << __E__;
		__CFG_COUT__ << "Done " << transitionName << " interface " << name << __E__;
		__CFG_COUT__ << "Done " << transitionName << " interface " << name << __E__;
	}
	postStateMachineExecutionLoop();

	__CFG_COUT__ << "Done " << transitionName << " all interfaces." << __E__;

}  // end start()

//==============================================================================
void FEVInterfacesManager::stop(void)
{
	const std::string transitionName = "Starting";
	FEVInterface*     fe;

	preStateMachineExecutionLoop();
	for(unsigned int i = 0; i < theFENamesByPriority_.size(); ++i)
	{
		// if one state machine is doing a sub-iteration, then target that one
		if(subIterationWorkStateMachineIndex_ != (unsigned int)-1 && i != subIterationWorkStateMachineIndex_)
			continue;  // skip those not in the sub-iteration

		const std::string& name = theFENamesByPriority_[i];

		fe = getFEInterfaceP(name);

		if(stateMachinesIterationDone_[name])
			continue;  // skip state machines already done

		__CFG_COUT__ << transitionName << " interface " << name << __E__;
		__CFG_COUT__ << transitionName << " interface " << name << __E__;
		__CFG_COUT__ << transitionName << " interface " << name << __E__;

		preStateMachineExecution(i, transitionName);
		fe->stopWorkLoop();
		fe->stop();
		postStateMachineExecution(i);

		__CFG_COUT__ << "Done " << transitionName << " interface " << name << __E__;
		__CFG_COUT__ << "Done " << transitionName << " interface " << name << __E__;
		__CFG_COUT__ << "Done " << transitionName << " interface " << name << __E__;
	}
	postStateMachineExecutionLoop();

	__CFG_COUT__ << "Done " << transitionName << " all interfaces." << __E__;

}  // end stop()

//==============================================================================
// getFEInterfaceP
FEVInterface* FEVInterfacesManager::getFEInterfaceP(const std::string& interfaceID)
{
	try
	{
		return theFEInterfaces_.at(interfaceID).get();
	}
	catch(...)
	{
		__CFG_SS__ << "Interface ID '" << interfaceID << "' not found in configured interfaces." << __E__;
		__CFG_SS_THROW__;
	}
}  // end getFEInterfaceP()

//==============================================================================
// getFEInterface
const FEVInterface& FEVInterfacesManager::getFEInterface(const std::string& interfaceID) const
{
	try
	{
		return *(theFEInterfaces_.at(interfaceID));
	}
	catch(...)
	{
		__CFG_SS__ << "Interface ID '" << interfaceID << "' not found in configured interfaces." << __E__;
		__CFG_SS_THROW__;
	}
}  // end getFEInterface()

//==============================================================================
// universalRead
//	used by MacroMaker
//	throw std::runtime_error on error/timeout
void FEVInterfacesManager::universalRead(const std::string& interfaceID, char* address, char* returnValue)
{
	getFEInterfaceP(interfaceID)->universalRead(address, returnValue);
}  // end universalRead()

//==============================================================================
// getInterfaceUniversalAddressSize
//	used by MacroMaker
unsigned int FEVInterfacesManager::getInterfaceUniversalAddressSize(const std::string& interfaceID)
{
	return getFEInterfaceP(interfaceID)->getUniversalAddressSize();
}  // end getInterfaceUniversalAddressSize()

//==============================================================================
// getInterfaceUniversalDataSize
//	used by MacroMaker
unsigned int FEVInterfacesManager::getInterfaceUniversalDataSize(const std::string& interfaceID)
{
	return getFEInterfaceP(interfaceID)->getUniversalDataSize();
}  // end getInterfaceUniversalDataSize()

//==============================================================================
// universalWrite
//	used by MacroMaker
void FEVInterfacesManager::universalWrite(const std::string& interfaceID, char* address, char* writeValue)
{
	getFEInterfaceP(interfaceID)->universalWrite(address, writeValue);
}  // end universalWrite()

//==============================================================================
// getFEListString
//	returns string with each new line for each FE
//	each line:
//		<interface type>:<parent supervisor lid>:<interface UID>
std::string FEVInterfacesManager::getFEListString(const std::string& supervisorLid)
{
	std::string retList = "";

	for(const auto& it : theFEInterfaces_)
	{
		__CFG_COUT__ << "FE name = " << it.first << __E__;

		retList += it.second->getInterfaceType() + ":" + supervisorLid + ":" + it.second->getInterfaceUID() + "\n";
	}
	return retList;
}  // end getFEListString()

//==============================================================================
// startMacroMultiDimensional
//	Launches a thread that manages the multi-dimensional loop
//		running the Macro on the specified FE interface.
//	Called by iterator (for now).
//
//	Note: no output arguments are returned, but outputs are
//		optionally saved to file.
//
//
//	inputs:
//		- inputArgs: dimensional semi-colon-separated,
//			comma separated: dimension iterations and arguments (colon-separated
// name/value/stepsize sets)
//
//	outputs:
//		- throws exception on failure
void FEVInterfacesManager::startMacroMultiDimensional(const std::string& requester,
                                                      const std::string& interfaceID,
                                                      const std::string& macroName,
                                                      const std::string& macroString,
                                                      const bool         enableSavingOutput,
                                                      const std::string& outputFilePath,
                                                      const std::string& outputFileRadix,
                                                      const std::string& inputArgs)
{
	if(requester != "iterator")
	{
		__CFG_SS__ << "Invalid requester '" << requester << "'" << __E__;
		__CFG_SS_THROW__;
	}

	__CFG_COUT__ << "Starting multi-dimensional Macro '" << macroName << "' for interface '" << interfaceID << ".'" << __E__;

	__CFG_COUTV__(macroString);

	__CFG_COUTV__(inputArgs);

	// mark active(only one Macro per interface active at any time, for now)
	{  // lock mutex scope
		std::lock_guard<std::mutex> lock(macroMultiDimensionalDoneMutex_);
		// mark active
		if(macroMultiDimensionalStatusMap_.find(interfaceID) != macroMultiDimensionalStatusMap_.end())
		{
			__SS__ << "Failed to start multi-dimensional Macro '" << macroName << "' for interface '" << interfaceID
			       << "' - this interface already has an active Macro launch!" << __E__;
			__SS_THROW__;
		}
		macroMultiDimensionalStatusMap_.emplace(std::make_pair(interfaceID, "Active"));
	}  // unlock mutex scope

	// start thread
	std::thread(
	    [](FEVInterfacesManager* feMgr,
	       const std::string     interfaceID,
	       const std::string     macroName,
	       const std::string     macroString,
	       const bool            enableSavingOutput,
	       const std::string     outputFilePath,
	       const std::string     outputFileRadix,
	       const std::string     inputArgsStr) {
		    // create local message facility subject
		    std::string mfSubject_ = "threadMultiD-" + macroName;
		    __GEN_COUT__ << "Thread started." << __E__;

		    std::string statusResult = "Done";

		    try
		    {
			    //-------------------------------
			    // check for interfaceID
			    FEVInterface* fe = feMgr->getFEInterfaceP(interfaceID);

			    //-------------------------------
			    // extract macro object
			    FEVInterface::macroStruct_t macro(macroString);

			    //-------------------------------
			    // create output file pointer
			    FILE* outputFilePointer = 0;
			    if(enableSavingOutput)
			    {
				    std::string filename = outputFilePath + "/" + outputFileRadix + macroName + "_" + std::to_string(time(0)) + ".txt";
				    __GEN_COUT__ << "Opening file... " << filename << __E__;

				    outputFilePointer = fopen(filename.c_str(), "w");
				    if(!outputFilePointer)
				    {
					    __GEN_SS__ << "Failed to open output file: " << filename << __E__;
					    __GEN_SS_THROW__;
				    }
			    }  // at this point output file pointer is valid or null

			    //-------------------------------
			    // setup Macro arguments
			    __GEN_COUTV__(inputArgsStr);

			    //	inputs:
			    //		- inputArgs: dimensional semi-colon-separated,
			    //			comma separated: dimension iterations and arguments
			    //(colon-separated name/value/stepsize sets)

			    // need to extract input arguments
			    //	by dimension (by priority)
			    //
			    //	two vector by dimension <map of params>
			    //		one vector for long and for double
			    //
			    //	map of params :=
			    //		name => {
			    //			<long/double current value>
			    //			<long/double init value>
			    //			<long/double step size>
			    //		}

			    std::vector<unsigned long /*dimension iterations*/> dimensionIterations, dimensionIterationCnt;

			    using longParamMap_t =
			        std::map<std::string /*name*/, std::pair<long /*current value*/, std::pair<long /*initial value*/, long /*step value*/>>>;

			    std::vector<longParamMap_t> longDimensionParameters;
			    // Note: double parameters not allowed for Macro (allowed in FE Macros)

			    // instead of strict inputs and outputs, make a map
			    //	and populate with all input and output names
			    std::map<std::string /*name*/, uint64_t /*value*/> variableMap;

			    // std::vector<FEVInterface::frontEndMacroArg_t> argsIn;
			    // std::vector<FEVInterface::frontEndMacroArg_t> argsOut;

			    for(const auto& inputArgName : macro.namesOfInputArguments_)
				    variableMap.emplace(  // do not care about input arg value
				        std::pair<std::string /*name*/, uint64_t /*value*/>(inputArgName, 0));
			    for(const auto& outputArgName : macro.namesOfOutputArguments_)
				    variableMap.emplace(  // do not care about output arg value
				        std::pair<std::string /*name*/, uint64_t /*value*/>(outputArgName, 0));

			    if(0)  // example
			    {
				    // inputArgsStr = "2,fisrD:3:2,fD2:4:1;4,myOtherArg:5:2,sD:10f:1.3";

				    dimensionIterations.push_back(2);
				    dimensionIterations.push_back(4);

				    longDimensionParameters.push_back(longParamMap_t());
				    longDimensionParameters.push_back(longParamMap_t());

				    longDimensionParameters.back().emplace(
				        std::make_pair("myOtherArg", std::make_pair(3 /*current value*/, std::make_pair(3 /*initial value*/, 4 /*step value*/))));
			    }  // end example

			    std::vector<std::string> dimensionArgs;
			    StringMacros::getVectorFromString(inputArgsStr, dimensionArgs, {';'} /*delimeter set*/);

			    __GEN_COUTV__(dimensionArgs.size());
			    //__GEN_COUTV__(StringMacros::vectorToString(dimensionArgs));

			    if(dimensionArgs.size() == 0)
			    {
				    // just call Macro once!
				    //	create dimension with 1 iteration
				    //		and no arguments
				    dimensionIterations.push_back(1);
				    longDimensionParameters.push_back(longParamMap_t());
			    }
			    else
				    for(unsigned int d = 0; d < dimensionArgs.size(); ++d)
				    {
					    // for each dimension
					    //	get argument and classify as long or double

					    std::vector<std::string> args;
					    StringMacros::getVectorFromString(dimensionArgs[d], args, {','} /*delimeter set*/);

					    //__GEN_COUTV__(args.size());
					    //__GEN_COUTV__(StringMacros::vectorToString(args));

					    // require first value for number of iterations
					    if(args.size() == 0)
					    {
						    __GEN_SS__ << "Invalid dimensional arguments! "
						               << "Need number of iterations at dimension " << d << __E__;
						    __GEN_SS_THROW__;
					    }

					    unsigned long numOfIterations;
					    StringMacros::getNumber(args[0], numOfIterations);
					    __GEN_COUT__ << "Dimension " << d << " numOfIterations=" << numOfIterations << __E__;

					    // create dimension!
					    {
						    dimensionIterations.push_back(numOfIterations);
						    longDimensionParameters.push_back(longParamMap_t());
					    }

					    // skip iteration value, start at index 1
					    for(unsigned int a = 1; a < args.size(); ++a)
					    {
						    std::vector<std::string> argPieces;
						    StringMacros::getVectorFromString(args[a], argPieces, {':'} /*delimeter set*/);

						    __GEN_COUTV__(StringMacros::vectorToString(argPieces));

						    // check pieces and determine if arg is long or double
						    // 3 pieces := name, init value, step value
						    if(argPieces.size() != 3)
						    {
							    __GEN_SS__ << "Invalid argument pieces! Should be size "
							                  "3, but is "
							               << argPieces.size() << __E__;
							    ss << StringMacros::vectorToString(argPieces);
							    __GEN_SS_THROW__;
						    }

						    // check piece 1 and 2 for double hint
						    //	a la Iterator::startCommandModifyActive()
						    if((argPieces[1].size() && (argPieces[1][argPieces[1].size() - 1] == 'f' || argPieces[1].find('.') != std::string::npos)) ||
						       (argPieces[2].size() && (argPieces[2][argPieces[2].size() - 1] == 'f' || argPieces[2].find('.') != std::string::npos)))
						    {
							    // handle as double

							    double startValue = strtod(argPieces[1].c_str(), 0);
							    double stepSize   = strtod(argPieces[2].c_str(), 0);

							    __GEN_COUTV__(startValue);
							    __GEN_COUTV__(stepSize);

							    __GEN_SS__ << "Error! Only integer aruments allowed for Macros. "
							               << "Double style arugment found: " << argPieces[0] << "' := " << startValue << ", " << stepSize << __E__;
							    __GEN_SS_THROW__;
							    //							doubleDimensionParameters.back().emplace(
							    //									std::make_pair(argPieces[0],
							    //											std::make_pair(
							    // startValue
							    ///*current value*/,
							    //													std::make_pair(
							    // startValue
							    ///*initial value*/,
							    //															stepSize
							    ///*step  value*/))));
						    }
						    else
						    {
							    // handle as long
							    //__GEN_COUT__ << "Long found" << __E__;

							    long int startValue;
							    long int stepSize;

							    StringMacros::getNumber(argPieces[1], startValue);
							    StringMacros::getNumber(argPieces[2], stepSize);

							    __GEN_COUTV__(startValue);
							    __GEN_COUTV__(stepSize);

							    __GEN_COUT__ << "Creating long argument '" << argPieces[0] << "' := " << startValue << ", " << stepSize << __E__;

							    longDimensionParameters.back().emplace(std::make_pair(
							        argPieces[0],
							        std::make_pair(startValue /*current value*/, std::make_pair(startValue /*initial value*/, stepSize /*step value*/))));
						    }

					    }  // end dimensional argument loop

				    }  // end dimensions loop

			    if(dimensionIterations.size() != longDimensionParameters.size())
			    {
				    __GEN_SS__ << "Impossible vector size mismatch! " << dimensionIterations.size() << " - " << longDimensionParameters.size() << __E__;
				    __GEN_SS_THROW__;
			    }

			    // output header
			    {
				    std::stringstream outSS;
				    {
					    outSS << "\n==========================\n" << __E__;
					    outSS << "Macro '" << macro.macroName_ << "' multi-dimensional scan..." << __E__;
					    outSS << "\t" << StringMacros::getTimestampString() << __E__;
					    outSS << "\t" << dimensionIterations.size() << " dimensions defined." << __E__;
					    for(unsigned int i = 0; i < dimensionIterations.size(); ++i)
					    {
						    outSS << "\t\t"
						          << "dimension[" << i << "] has " << dimensionIterations[i] << " iterations and " << (longDimensionParameters[i].size())
						          << " arguments." << __E__;

						    for(auto& param : longDimensionParameters[i])
							    outSS << "\t\t\t"
							          << "'" << param.first << "' of type long with "
							          << "initial value and step value [decimal] = "
							          << "\t" << param.second.second.first << " & " << param.second.second.second << __E__;
					    }

					    outSS << "\nInput argument names:" << __E__;
					    for(const auto& inputArgName : macro.namesOfInputArguments_)
						    outSS << "\t" << inputArgName << __E__;
					    outSS << "\nOutput argument names:" << __E__;
					    for(const auto& outputArgName : macro.namesOfOutputArguments_)
						    outSS << "\t" << outputArgName << __E__;

					    outSS << "\n==========================\n" << __E__;
				    }  // end outputs stringstream results

				    // if enabled to save to file, do it.
				    __GEN_COUT__ << "\n" << outSS.str();
				    if(outputFilePointer)
					    fprintf(outputFilePointer, "%s", outSS.str().c_str());
			    }  // end output header

			    unsigned int iterationCount = 0;

			    // Use lambda recursive function to do arbitrary dimensions
			    //
			    // Note: can not do lambda recursive function if using auto to declare the
			    // function, 	and must capture reference to the function. Also, must
			    // capture specialFolders 	reference for use internally (const values
			    // already are captured).
			    std::function<void(const unsigned int /*dimension*/
			                       )>
			        localRecurse = [&dimensionIterations,
			                        &dimensionIterationCnt,
			                        &iterationCount,
			                        &longDimensionParameters,
			                        &fe,
			                        &macro,
			                        &variableMap,
			                        &outputFilePointer,
			                        &localRecurse](const unsigned int dimension) {
				        // create local message facility subject
				        std::string mfSubject_ = "multiD-" + std::to_string(dimension) + "-" + macro.macroName_;
				        __GEN_COUTV__(dimension);

				        if(dimension >= dimensionIterations.size())
				        {
					        __GEN_COUT__ << "Iteration count: " << iterationCount++ << __E__;
					        __GEN_COUT__ << "Launching Macro '" << macro.macroName_ << "' ..." << __E__;

					        // set argsIn to current value
					        {
						        // scan all dimension parameter objects
						        //	note: Although conflicts should not be allowed
						        //		at this point, lower dimensions will have priority
						        // over higher dimension 		with same name argument..
						        // and longs will have priority over doubles

						        for(unsigned int j = 0; j < dimensionIterations.size(); ++j)
						        {
							        for(auto& longParam : longDimensionParameters[j])
							        {
								        __GEN_COUT__ << "Assigning argIn '" << longParam.first << "' to current long value '" << longParam.second.first
								                     << "' from dimension " << j << " parameter." << __E__;
								        variableMap.at(longParam.first) = longParam.second.first;
							        }
						        }  // end long loop

					        }  // done building argsIn

					        // output inputs
					        {
						        std::stringstream outSS;
						        {
							        outSS << "\n---------------\n" << __E__;
							        outSS << "Macro '" << macro.macroName_ << "' execution..." << __E__;
							        outSS << "\t"
							              << "iteration " << iterationCount << __E__;
							        for(unsigned int i = 0; i < dimensionIterationCnt.size(); ++i)
								        outSS << "\t"
								              << "dimension[" << i << "] index := " << dimensionIterationCnt[i] << __E__;

							        outSS << "\n"
							              << "\t"
							              << "Input arguments (count: " << macro.namesOfInputArguments_.size() << "):" << __E__;
							        for(auto& argIn : macro.namesOfInputArguments_)
								        outSS << "\t\t" << argIn << " = " << variableMap.at(argIn) << __E__;

						        }  // end outputs stringstream results

						        // if enabled to save to file, do it.
						        __GEN_COUT__ << "\n" << outSS.str();
						        if(outputFilePointer)
							        fprintf(outputFilePointer, "%s", outSS.str().c_str());
					        }  // end output inputs

					        // have FE and Macro structure, so run it
					        fe->runMacro(macro, variableMap);

					        __GEN_COUT__ << "Macro complete!" << __E__;

					        // output results
					        {
						        std::stringstream outSS;
						        {
							        outSS << "\n"
							              << "\t"
							              << "Output arguments (count: " << macro.namesOfOutputArguments_.size() << "):" << __E__;
							        for(auto& argOut : macro.namesOfOutputArguments_)
								        outSS << "\t\t" << argOut << " = " << variableMap.at(argOut) << __E__;
						        }  // end outputs stringstream results

						        // if enabled to save to file, do it.
						        __GEN_COUT__ << "\n" << outSS.str();
						        if(outputFilePointer)
							        fprintf(outputFilePointer, "%s", outSS.str().c_str());
					        }  // end output results

					        return;
				        }

				        // init dimension index
				        if(dimension >= dimensionIterationCnt.size())
					        dimensionIterationCnt.push_back(0);

				        // if enabled to save to file, do it.
				        __GEN_COUT__ << "\n"
				                     << "======================================" << __E__ << "dimension[" << dimension
				                     << "] number of iterations := " << dimensionIterations[dimension] << __E__;

				        // update current value to initial value for this dimension's
				        // parameters
				        {
					        for(auto& longPair : longDimensionParameters[dimension])
					        {
						        longPair.second.first =  // reset to initial value
						            longPair.second.second.first;
						        __GEN_COUT__ << "arg '" << longPair.first << "' current value: " << longPair.second.first << __E__;
					        }  // end long loop

				        }  // end update current value to initial value for all
				           // dimensional parameters

				        for(dimensionIterationCnt[dimension] = 0;  // reset each time through dimension loop
				            dimensionIterationCnt[dimension] < dimensionIterations[dimension];
				            ++dimensionIterationCnt[dimension])
				        {
					        __GEN_COUT__ << "dimension[" << dimension << "] index := " << dimensionIterationCnt[dimension] << __E__;

					        localRecurse(dimension + 1);

					        // update current value to next value for this dimension's
					        // parameters
					        {
						        for(auto& longPair : longDimensionParameters[dimension])
						        {
							        longPair.second.first +=  // add step value
							            longPair.second.second.second;
							        __GEN_COUT__ << "arg '" << longPair.first << "' current value: " << longPair.second.first << __E__;
						        }  // end long loop

					        }  // end update current value to next value for all
					           // dimensional parameters
				        }
				        __GEN_COUT__ << "Completed dimension[" << dimension << "] number of iterations := " << dimensionIterationCnt[dimension] << " of "
				                     << dimensionIterations[dimension] << __E__;
			        };  // end local lambda recursive function

			    // launch multi-dimensional recursion
			    localRecurse(0);

			    // close output file
			    if(outputFilePointer)
				    fclose(outputFilePointer);
		    }
		    catch(const std::runtime_error& e)
		    {
			    __SS__ << "Error executing multi-dimensional Macro: " << e.what() << __E__;
			    statusResult = ss.str();
		    }
		    catch(...)
		    {
			    __SS__ << "Unknown error executing multi-dimensional Macro. " << __E__;
				try	{ throw; } //one more try to printout extra info
				catch(const std::exception &e)
				{
					ss << "Exception message: " << e.what();
				}
				catch(...){}
			    statusResult = ss.str();
		    }

		    __COUTV__(statusResult);

		    {  // lock mutex scope
			    std::lock_guard<std::mutex> lock(feMgr->macroMultiDimensionalDoneMutex_);
			    // change status at completion
			    feMgr->macroMultiDimensionalStatusMap_[interfaceID] = statusResult;
		    }
	    },  // end thread()
	    this,
	    interfaceID,
	    macroName,
	    macroString,
	    enableSavingOutput,
	    outputFilePath,
	    outputFileRadix,
	    inputArgs)
	    .detach();

	__CFG_COUT__ << "Started multi-dimensional Macro '" << macroName << "' for interface '" << interfaceID << ".'" << __E__;

}  // end startMacroMultiDimensional()

//==============================================================================
// startFEMacroMultiDimensional
//	Launches a thread that manages the multi-dimensional loop
//		running the FE Macro in the specified FE interface.
//	Called by iterator (for now).
//
//	Note: no output arguments are returned, but outputs are
//		optionally saved to file.
//
//
//	inputs:
//		- inputArgs: dimensional semi-colon-separated,
//			comma separated: dimension iterations and arguments (colon-separated
// name/value/stepsize sets)
//
//	outputs:
//		- throws exception on failure
void FEVInterfacesManager::startFEMacroMultiDimensional(const std::string& requester,
                                                        const std::string& interfaceID,
                                                        const std::string& feMacroName,
                                                        const bool         enableSavingOutput,
                                                        const std::string& outputFilePath,
                                                        const std::string& outputFileRadix,
                                                        const std::string& inputArgs)
{
	if(requester != "iterator")
	{
		__CFG_SS__ << "Invalid requester '" << requester << "'" << __E__;
		__CFG_SS_THROW__;
	}

	__CFG_COUT__ << "Starting multi-dimensional FE Macro '" << feMacroName << "' for interface '" << interfaceID << ".'" << __E__;
	__CFG_COUTV__(inputArgs);

	// mark active(only one FE Macro per interface active at any time, for now)
	{  // lock mutex scope
		std::lock_guard<std::mutex> lock(macroMultiDimensionalDoneMutex_);
		// mark active
		if(macroMultiDimensionalStatusMap_.find(interfaceID) != macroMultiDimensionalStatusMap_.end())
		{
			__SS__ << "Failed to start multi-dimensional FE Macro '" << feMacroName << "' for interface '" << interfaceID
			       << "' - this interface already has an active FE Macro launch!" << __E__;
			__SS_THROW__;
		}
		macroMultiDimensionalStatusMap_.emplace(std::make_pair(interfaceID, "Active"));
	}

	// start thread
	std::thread(
	    [](FEVInterfacesManager* feMgr,
	       const std::string     interfaceID,
	       const std::string     feMacroName,
	       const bool            enableSavingOutput,
	       const std::string     outputFilePath,
	       const std::string     outputFileRadix,
	       const std::string     inputArgsStr) {
		    // create local message facility subject
		    std::string mfSubject_ = "threadMultiD-" + feMacroName;
		    __GEN_COUT__ << "Thread started." << __E__;

		    std::string statusResult = "Done";

		    try
		    {
			    //-------------------------------
			    // check for interfaceID and macro
			    FEVInterface* fe = feMgr->getFEInterfaceP(interfaceID);

			    // have pointer to virtual FEInterface, find Macro structure
			    auto FEMacroIt = fe->getMapOfFEMacroFunctions().find(feMacroName);
			    if(FEMacroIt == fe->getMapOfFEMacroFunctions().end())
			    {
				    __GEN_SS__ << "FE Macro '" << feMacroName << "' of interfaceID '" << interfaceID << "' was not found!" << __E__;
				    __GEN_SS_THROW__;
			    }
			    const FEVInterface::frontEndMacroStruct_t& feMacro = FEMacroIt->second;

			    //-------------------------------
			    // create output file pointer
			    FILE* outputFilePointer = 0;
			    if(enableSavingOutput)
			    {
				    std::string filename = outputFilePath + "/" + outputFileRadix + feMacroName + "_" + std::to_string(time(0)) + ".txt";
				    __GEN_COUT__ << "Opening file... " << filename << __E__;

				    outputFilePointer = fopen(filename.c_str(), "w");
				    if(!outputFilePointer)
				    {
					    __GEN_SS__ << "Failed to open output file: " << filename << __E__;
					    __GEN_SS_THROW__;
				    }
			    }  // at this point output file pointer is valid or null

			    //-------------------------------
			    // setup FE macro aruments
			    __GEN_COUTV__(inputArgsStr);

			    //	inputs:
			    //		- inputArgs: dimensional semi-colon-separated,
			    //			comma separated: dimension iterations and arguments
			    //(colon-separated name/value/stepsize sets)

			    // need to extract input arguments
			    //	by dimension (by priority)
			    //
			    //	two vector by dimension <map of params>
			    //		one vector for long and for double
			    //
			    //	map of params :=
			    //		name => {
			    //			<long/double current value>
			    //			<long/double init value>
			    //			<long/double step size>
			    //		}

			    std::vector<unsigned long /*dimension iterations*/> dimensionIterations, dimensionIterationCnt;

			    using longParamMap_t =
			        std::map<std::string /*name*/, std::pair<long /*current value*/, std::pair<long /*initial value*/, long /*step value*/>>>;
			    using doubleParamMap_t =
			        std::map<std::string /*name*/, std::pair<double /*current value*/, std::pair<double /*initial value*/, double /*step value*/>>>;
			    using stringParamMap_t = std::map<std::string /*name*/, std::string /* value*/>;

			    std::vector<longParamMap_t>   longDimensionParameters;
			    std::vector<doubleParamMap_t> doubleDimensionParameters;
			    std::vector<stringParamMap_t> stringDimensionParameters;

			    std::vector<FEVInterface::frontEndMacroArg_t> argsIn;
			    std::vector<FEVInterface::frontEndMacroArg_t> argsOut;

			    for(unsigned int i = 0; i < feMacro.namesOfInputArguments_.size(); ++i)
				    argsIn.push_back(std::make_pair(  // do not care about input arg value
				        feMacro.namesOfInputArguments_[i],
				        ""));
			    for(unsigned int i = 0; i < feMacro.namesOfOutputArguments_.size(); ++i)
				    argsOut.push_back(std::make_pair(  // do not care about output arg value
				        feMacro.namesOfOutputArguments_[i],
				        ""));

			    if(0)  // example
			    {
				    // inputArgsStr = "2,fisrD:3:2,fD2:4:1;4,myOtherArg:5:2,sD:10f:1.3";

				    argsIn.push_back(std::make_pair("myOtherArg", "3"));

				    dimensionIterations.push_back(2);
				    dimensionIterations.push_back(4);

				    longDimensionParameters.push_back(longParamMap_t());
				    longDimensionParameters.push_back(longParamMap_t());

				    doubleDimensionParameters.push_back(doubleParamMap_t());
				    doubleDimensionParameters.push_back(doubleParamMap_t());

				    longDimensionParameters.back().emplace(
				        std::make_pair("myOtherArg", std::make_pair(3 /*current value*/, std::make_pair(3 /*initial value*/, 4 /*step value*/))));
			    }  // end example

			    std::vector<std::string> dimensionArgs;
			    StringMacros::getVectorFromString(inputArgsStr, dimensionArgs, {';'} /*delimeter set*/);

			    __GEN_COUTV__(dimensionArgs.size());
			    //__GEN_COUTV__(StringMacros::vectorToString(dimensionArgs));

			    if(dimensionArgs.size() == 0)
			    {
				    // just call FE Macro once!
				    //	create dimension with 1 iteration
				    //		and no arguments
				    dimensionIterations.push_back(1);
				    longDimensionParameters.push_back(longParamMap_t());
				    doubleDimensionParameters.push_back(doubleParamMap_t());
				    stringDimensionParameters.push_back(stringParamMap_t());
			    }
			    else
				    for(unsigned int d = 0; d < dimensionArgs.size(); ++d)
				    {
					    // for each dimension
					    //	get argument and classify as long or double

					    std::vector<std::string> args;
					    StringMacros::getVectorFromString(dimensionArgs[d], args, {','} /*delimeter set*/);

					    //__GEN_COUTV__(args.size());
					    //__GEN_COUTV__(StringMacros::vectorToString(args));

					    // require first value for number of iterations
					    if(args.size() == 0)
					    {
						    __GEN_SS__ << "Invalid dimensional arguments! "
						               << "Need number of iterations at dimension " << d << __E__;
						    __GEN_SS_THROW__;
					    }

					    unsigned long numOfIterations;
					    StringMacros::getNumber(args[0], numOfIterations);
					    __GEN_COUT__ << "Dimension " << d << " numOfIterations=" << numOfIterations << __E__;

					    // create dimension!
					    {
						    dimensionIterations.push_back(numOfIterations);
						    longDimensionParameters.push_back(longParamMap_t());
						    doubleDimensionParameters.push_back(doubleParamMap_t());
						    stringDimensionParameters.push_back(stringParamMap_t());
					    }

					    // skip iteration value, start at index 1
					    for(unsigned int a = 1; a < args.size(); ++a)
					    {
						    std::vector<std::string> argPieces;
						    StringMacros::getVectorFromString(args[a], argPieces, {':'} /*delimeter set*/);

						    __GEN_COUTV__(StringMacros::vectorToString(argPieces));

						    // check pieces and determine if arg is long or double
						    // 3 pieces := name, init value, step value
						    if(argPieces.size() != 3)
						    {
							    __GEN_SS__ << "Invalid argument pieces! Should be size "
							                  "3, but is "
							               << argPieces.size() << __E__;
							    ss << StringMacros::vectorToString(argPieces);
							    __GEN_SS_THROW__;
						    }

						    // check piece 1 and 2 for double hint
						    //	a la Iterator::startCommandModifyActive()
						    if(argPieces[2] == TableViewColumnInfo::DATATYPE_STRING_DEFAULT)
						    {
							    // if step size is default, considering value an unchanging string

							    __GEN_COUT__ << "Creating string argument '" << argPieces[0] << "' := " << argPieces[1] << __E__;

							    stringDimensionParameters.back().emplace(std::make_pair(argPieces[0], argPieces[1]));
						    }
						    else if((argPieces[1].size() && (argPieces[1][argPieces[1].size() - 1] == 'f' || argPieces[1].find('.') != std::string::npos)) ||
						            (argPieces[2].size() && (argPieces[2][argPieces[2].size() - 1] == 'f' || argPieces[2].find('.') != std::string::npos)))
						    {
							    // handle as double
							    //__GEN_COUT__ << "Double found" << __E__;

							    double startValue = strtod(argPieces[1].c_str(), 0);
							    double stepSize   = strtod(argPieces[2].c_str(), 0);

							    __GEN_COUTV__(startValue);
							    __GEN_COUTV__(stepSize);

							    __GEN_COUT__ << "Creating double argument '" << argPieces[0] << "' := " << startValue << ", " << stepSize << __E__;

							    doubleDimensionParameters.back().emplace(std::make_pair(
							        argPieces[0],
							        std::make_pair(startValue /*current value*/, std::make_pair(startValue /*initial value*/, stepSize /*step value*/))));
						    }
						    else
						    {
							    // handle as long
							    //__GEN_COUT__ << "Long found" << __E__;

							    long int startValue;
							    long int stepSize;

							    StringMacros::getNumber(argPieces[1], startValue);
							    StringMacros::getNumber(argPieces[2], stepSize);

							    __GEN_COUTV__(startValue);
							    __GEN_COUTV__(stepSize);

							    __GEN_COUT__ << "Creating long argument '" << argPieces[0] << "' := " << startValue << ", " << stepSize << __E__;

							    longDimensionParameters.back().emplace(std::make_pair(
							        argPieces[0],
							        std::make_pair(startValue /*current value*/, std::make_pair(startValue /*initial value*/, stepSize /*step value*/))));
						    }

					    }  // end dimensional argument loop

				    }  // end dimensions loop

			    if(dimensionIterations.size() != longDimensionParameters.size() || dimensionIterations.size() != doubleDimensionParameters.size() ||
			       dimensionIterations.size() != stringDimensionParameters.size())
			    {
				    __GEN_SS__ << "Impossible vector size mismatch! " << dimensionIterations.size() << " - " << longDimensionParameters.size() << " - "
				               << doubleDimensionParameters.size() << " - " << stringDimensionParameters.size() << __E__;
				    __GEN_SS_THROW__;
			    }

			    // output header
			    {
				    std::stringstream outSS;
				    {
					    outSS << "\n==========================\n" << __E__;
					    outSS << "FEMacro '" << feMacro.feMacroName_ << "' multi-dimensional scan..." << __E__;
					    outSS << "\t" << StringMacros::getTimestampString() << __E__;
					    outSS << "\t" << dimensionIterations.size() << " dimensions defined." << __E__;
					    for(unsigned int i = 0; i < dimensionIterations.size(); ++i)
					    {
						    outSS << "\t\t"
						          << "dimension[" << i << "] has " << dimensionIterations[i] << " iterations and "
						          << (longDimensionParameters[i].size() + doubleDimensionParameters[i].size() + stringDimensionParameters[i].size())
						          << " arguments." << __E__;

						    for(auto& param : longDimensionParameters[i])
							    outSS << "\t\t\t"
							          << "'" << param.first << "' of type long with "
							          << "initial value and step value [decimal] = "
							          << "\t" << param.second.second.first << " & " << param.second.second.second << __E__;

						    for(auto& param : doubleDimensionParameters[i])
							    outSS << "\t\t\t"
							          << "'" << param.first << "' of type double with "
							          << "initial value and step value = "
							          << "\t" << param.second.second.first << " & " << param.second.second.second << __E__;

						    for(auto& param : stringDimensionParameters[i])
							    outSS << "\t\t\t"
							          << "'" << param.first << "' of type string with "
							          << "value = "
							          << "\t" << param.second << __E__;
					    }

					    outSS << "\nHere are the identified input arguments:" << __E__;
					    for(unsigned int i = 0; i < feMacro.namesOfInputArguments_.size(); ++i)
						    outSS << "\t" << feMacro.namesOfInputArguments_[i] << __E__;
					    outSS << "\nHere are the identified input arguments:" << __E__;
					    for(unsigned int i = 0; i < feMacro.namesOfOutputArguments_.size(); ++i)
						    outSS << "\t" << feMacro.namesOfOutputArguments_[i] << __E__;

					    outSS << "\n==========================\n" << __E__;
				    }  // end outputs stringstream results

				    // if enabled to save to file, do it.
				    __GEN_COUT__ << "\n" << outSS.str();
				    if(outputFilePointer)
					    fprintf(outputFilePointer, "%s", outSS.str().c_str());
			    }  // end output header

			    unsigned int iterationCount = 0;

			    // Use lambda recursive function to do arbitrary dimensions
			    //
			    // Note: can not do lambda recursive function if using auto to declare the
			    // function, 	and must capture reference to the function. Also, must
			    // capture specialFolders 	reference for use internally (const values
			    // already are captured).
			    std::function<void(const unsigned int /*dimension*/
			                       )>
			        localRecurse = [&dimensionIterations,
			                        &dimensionIterationCnt,
			                        &iterationCount,
			                        &longDimensionParameters,
			                        &doubleDimensionParameters,
			                        &stringDimensionParameters,
			                        &fe,
			                        &feMacro,
			                        &outputFilePointer,
			                        &argsIn,
			                        &argsOut,
			                        &localRecurse](const unsigned int dimension) {
				        // create local message facility subject
				        std::string mfSubject_ = "multiD-" + std::to_string(dimension) + "-" + feMacro.feMacroName_;
				        __GEN_COUTV__(dimension);

				        if(dimension >= dimensionIterations.size())
				        {
					        __GEN_COUT__ << "Iteration count: " << iterationCount++ << __E__;
					        __GEN_COUT__ << "Launching FE Macro '" << feMacro.feMacroName_ << "' ..." << __E__;

					        // set argsIn to current value

					        // scan all dimension parameter objects
					        //	note: Although conflicts should not be allowed
					        //		at this point, lower dimensions will have priority
					        // over  higher dimension 		with same name argument.. and
					        // longs will have priority  over doubles

					        bool foundAsLong;
					        for(unsigned int i = 0; i < argsIn.size(); ++i)
					        {
						        foundAsLong = false;
						        for(unsigned int j = 0; j < dimensionIterations.size(); ++j)
						        {
							        auto longIt = longDimensionParameters[j].find(argsIn[i].first);
							        if(longIt == longDimensionParameters[j].end())
								        continue;

							        // else found long!
							        __GEN_COUT__ << "Assigning argIn '" << argsIn[i].first << "' to current long value '" << longIt->second.first
							                     << "' from dimension " << j << " parameter." << __E__;
							        argsIn[i].second = std::to_string(longIt->second.first);
							        foundAsLong      = true;
							        break;
						        }  // end long loop

						        if(foundAsLong)
						        {
							        continue;  // skip double check
						        }

						        for(unsigned int j = 0; j < dimensionIterations.size(); ++j)
						        {
							        auto doubleIt = doubleDimensionParameters[j].find(argsIn[i].first);
							        if(doubleIt == doubleDimensionParameters[j].end())
								        continue;

							        // else found long!
							        __GEN_COUT__ << "Assigning argIn '" << argsIn[i].first << "' to current double value '" << doubleIt->second.first
							                     << "' from dimension " << j << " parameter." << __E__;
							        argsIn[i].second = std::to_string(doubleIt->second.first);
							        foundAsLong      = true;
							        break;
						        }  // end double loop

						        if(foundAsLong)
						        {
							        continue;  // skip double check
						        }

						        for(unsigned int j = 0; j < dimensionIterations.size(); ++j)
						        {
							        auto stringIt = stringDimensionParameters[j].find(argsIn[i].first);
							        if(stringIt == stringDimensionParameters[j].end())
								        continue;

							        // else found long!
							        __GEN_COUT__ << "Assigning argIn '" << argsIn[i].first << "' to current string value '" << stringIt->second
							                     << "' from dimension " << j << " parameter." << __E__;
							        argsIn[i].second = stringIt->second;
							        foundAsLong      = true;
							        break;
						        }  // end double loop

						        if(foundAsLong)
						        {
							        continue;  // skip double check
						        }

						        __GEN_SS__ << "ArgIn '" << argsIn[i].first << "' was not assigned a value "
						                   << "by any dimensional loop parameter sets. "
						                      "This is illegal. FEMacro '"
						                   << feMacro.feMacroName_ << "' requires '" << argsIn[i].first
						                   << "' as an input argument. Either remove the "
						                      "input argument from this FEMacro, "
						                   << "or define a value as a dimensional loop "
						                      "parameter."
						                   << __E__;
						        __GEN_SS_THROW__;
					        }  // done building argsIn

					        // have pointer to Macro structure, so run it
					        (fe->*(feMacro.macroFunction_))(feMacro, argsIn, argsOut);

					        __GEN_COUT__ << "FE Macro complete!" << __E__;

					        // output results
					        {
						        std::stringstream outSS;
						        {
							        outSS << "\n---------------\n" << __E__;
							        outSS << "FEMacro '" << feMacro.feMacroName_ << "' execution..." << __E__;
							        outSS << "\t"
							              << "iteration " << iterationCount << __E__;
							        for(unsigned int i = 0; i < dimensionIterationCnt.size(); ++i)
								        outSS << "\t"
								              << "dimension[" << i << "] index := " << dimensionIterationCnt[i] << __E__;

							        outSS << "\n"
							              << "\t"
							              << "Input arguments (count: " << argsIn.size() << "):" << __E__;
							        for(auto& argIn : argsIn)
								        outSS << "\t\t" << argIn.first << " = " << argIn.second << __E__;

							        outSS << "\n"
							              << "\t"
							              << "Output arguments (count: " << argsOut.size() << "):" << __E__;
							        for(auto& argOut : argsOut)
								        outSS << "\t\t" << argOut.first << " = " << argOut.second << __E__;
						        }  // end outputs stringstream results

						        // if enabled to save to file, do it.
						        __GEN_COUT__ << "\n" << outSS.str();
						        if(outputFilePointer)
							        fprintf(outputFilePointer, "%s", outSS.str().c_str());
					        }  // end output results

					        return;
				        }

				        // init dimension index
				        if(dimension >= dimensionIterationCnt.size())
					        dimensionIterationCnt.push_back(0);

				        // if enabled to save to file, do it.
				        __GEN_COUT__ << "\n"
				                     << "======================================" << __E__ << "dimension[" << dimension
				                     << "] number of iterations := " << dimensionIterations[dimension] << __E__;

				        // update current value to initial value for this dimension's
				        // parameters
				        {
					        for(auto& longPair : longDimensionParameters[dimension])
					        {
						        longPair.second.first =  // reset to initial value
						            longPair.second.second.first;
						        __GEN_COUT__ << "arg '" << longPair.first << "' current value: " << longPair.second.first << __E__;
					        }  // end long loop

					        for(auto& doublePair : doubleDimensionParameters[dimension])
					        {
						        doublePair.second.first =  // reset to initial value
						            doublePair.second.second.first;
						        __GEN_COUT__ << "arg '" << doublePair.first << "' current value: " << doublePair.second.first << __E__;
					        }  // end double loop
				        }      // end update current value to initial value for all
				               // dimensional parameters

				        for(dimensionIterationCnt[dimension] = 0;  // reset each time through dimension loop
				            dimensionIterationCnt[dimension] < dimensionIterations[dimension];
				            ++dimensionIterationCnt[dimension])
				        {
					        __GEN_COUT__ << "dimension[" << dimension << "] index := " << dimensionIterationCnt[dimension] << __E__;

					        localRecurse(dimension + 1);

					        // update current value to next value for this dimension's
					        // parameters
					        {
						        for(auto& longPair : longDimensionParameters[dimension])
						        {
							        longPair.second.first +=  // add step value
							            longPair.second.second.second;
							        __GEN_COUT__ << "arg '" << longPair.first << "' current value: " << longPair.second.first << __E__;
						        }  // end long loop

						        for(auto& doublePair : doubleDimensionParameters[dimension])
						        {
							        doublePair.second.first +=  // add step value
							            doublePair.second.second.second;

							        __GEN_COUT__ << "arg '" << doublePair.first << "' current value: " << doublePair.second.first << __E__;
						        }  // end double loop
					        }      // end update current value to next value for all
					           // dimensional parameters
				        }
				        __GEN_COUT__ << "Completed dimension[" << dimension << "] number of iterations := " << dimensionIterationCnt[dimension] << " of "
				                     << dimensionIterations[dimension] << __E__;
			        };  // end local lambda recursive function

			    // launch multi-dimensional recursion
			    localRecurse(0);

			    // close output file
			    if(outputFilePointer)
				    fclose(outputFilePointer);
		    }
		    catch(const std::runtime_error& e)
		    {
			    __SS__ << "Error executing multi-dimensional FE Macro: " << e.what() << __E__;
			    statusResult = ss.str();
		    }
		    catch(...)
		    {
			    __SS__ << "Unknown error executing multi-dimensional FE Macro. " << __E__;
				try	{ throw; } //one more try to printout extra info
				catch(const std::exception &e)
				{
					ss << "Exception message: " << e.what();
				}
				catch(...){}
			    statusResult = ss.str();
		    }

		    __COUTV__(statusResult);

		    {  // lock mutex scope
			    std::lock_guard<std::mutex> lock(feMgr->macroMultiDimensionalDoneMutex_);
			    // change status at completion
			    feMgr->macroMultiDimensionalStatusMap_[interfaceID] = statusResult;
		    }
	    },  // end thread()
	    this,
	    interfaceID,
	    feMacroName,
	    enableSavingOutput,
	    outputFilePath,
	    outputFileRadix,
	    inputArgs)
	    .detach();

	__CFG_COUT__ << "Started multi-dimensional FE Macro '" << feMacroName << "' for interface '" << interfaceID << ".'" << __E__;

}  // end startFEMacroMultiDimensional()

//==============================================================================
// checkFEMacroMultiDimensional
//	Checks for the completion of the thread that manages the multi-dimensional loop
//		running the FE Macro or MacroMaker Macro in the specified FE interface.
//	Called by iterator (for now).
//
//	Returns true if multi-dimensional launch is done
bool FEVInterfacesManager::checkMacroMultiDimensional(const std::string& interfaceID, const std::string& macroName)
{
	// check active(only one FE Macro per interface active at any time, for now)
	// lock mutex scope
	std::lock_guard<std::mutex> lock(macroMultiDimensionalDoneMutex_);
	// check status
	auto statusIt = macroMultiDimensionalStatusMap_.find(interfaceID);
	if(statusIt == macroMultiDimensionalStatusMap_.end())
	{
		__CFG_SS__ << "Status missing for multi-dimensional launch of Macro '" << macroName << "' for interface '" << interfaceID << ".'" << __E__;
		__CFG_SS_THROW__;
	}
	else if(statusIt->second == "Done")
	{
		__CFG_COUT__ << "Completed multi-dimensional launch of Macro '" << macroName << "' for interface '" << interfaceID << ".'" << __E__;

		// erase from map
		macroMultiDimensionalStatusMap_.erase(statusIt);
		return true;
	}
	else if(statusIt->second == "Active")
	{
		__CFG_COUT__ << "Still running multi-dimensional launch of Macro '" << macroName << "' for interface '" << interfaceID << ".'" << __E__;
		return false;
	}
	// else //assume error

	__CFG_SS__ << "Error occured during multi-dimensional launch of Macro '" << macroName << "' for interface '" << interfaceID << "':" << statusIt->second
	           << __E__;
	__CFG_SS_THROW__;

}  // end checkMacroMultiDimensional()

//==============================================================================
// runFEMacroByFE
//	Runs the FE Macro in the specified FE interface. Called by another FE.
//
//	inputs:
//		- inputArgs: colon-separated name/value pairs, and then comma-separated
//		- outputArgs: comma-separated (Note: resolved for FE, allowing FE to not know
// output  arguments)
//
//	outputs:
//		- throws exception on failure
//		- outputArgs: colon-separate name/value pairs, and then comma-separated
void FEVInterfacesManager::runFEMacroByFE(const std::string& callingInterfaceID,
                                          const std::string& interfaceID,
                                          const std::string& feMacroName,
                                          const std::string& inputArgs,
                                          std::string&       outputArgs)
{
	__CFG_COUTV__(callingInterfaceID);

	// check for interfaceID
	FEVInterface* fe = getFEInterfaceP(interfaceID);

	// have pointer to virtual FEInterface, find Macro structure
	auto FEMacroIt = fe->getMapOfFEMacroFunctions().find(feMacroName);
	if(FEMacroIt == fe->getMapOfFEMacroFunctions().end())
	{
		__CFG_SS__ << "FE Macro '" << feMacroName << "' of interfaceID '" << interfaceID << "' was not found!" << __E__;
		__CFG_COUT_ERR__ << "\n" << ss.str();
		__CFG_SS_THROW__;
	}

	const FEVInterface::frontEndMacroStruct_t& feMacro = FEMacroIt->second;

	std::set<std::string> allowedFEsSet;
	StringMacros::getSetFromString(feMacro.allowedCallingFrontEnds_, allowedFEsSet);

	// check if calling interface is allowed to call macro
	if(!StringMacros::inWildCardSet(callingInterfaceID, allowedFEsSet))
	{
		__CFG_SS__ << "FE Macro '" << feMacroName << "' of interfaceID '" << interfaceID << "' does not allow access to calling interfaceID '"
		           << callingInterfaceID << "!' Did the interface add the calling interfaceID "
		           << "to the access list when registering the front-end macro." << __E__;
		__CFG_COUT_ERR__ << "\n" << ss.str();
		__CFG_SS_THROW__;
	}

	// if here, then access allowed
	// build output args list

	outputArgs = "";

	for(unsigned int i = 0; i < feMacro.namesOfOutputArguments_.size(); ++i)
		outputArgs += (i ? "," : "") + feMacro.namesOfOutputArguments_[i];

	__CFG_COUTV__(outputArgs);

	runFEMacro(interfaceID, feMacro, inputArgs, outputArgs);

	__CFG_COUTV__(outputArgs);

}  // end runFEMacroByFE()

//==============================================================================
// runMacro
//	Runs the MacroMaker Macro in the specified FE interface.
//
//	inputs:
//		- inputArgs: colon-separated name/value pairs, and then comma-separated
//		- outputArgs: comma-separated
//
//	outputs:
//		- throws exception on failure
//		- outputArgs: colon-separate name/value pairs, and then comma-separated
void FEVInterfacesManager::runMacro(const std::string& interfaceID, const std::string& macroObjectString, const std::string& inputArgs, std::string& outputArgs)
{
	//-------------------------------
	// extract macro object
	FEVInterface::macroStruct_t macro(macroObjectString);

	// check for interfaceID
	FEVInterface* fe = getFEInterfaceP(interfaceID);

	// build input arguments
	//	parse args, semicolon-separated pairs, and then comma-separated
	std::vector<FEVInterface::frontEndMacroArg_t> argsIn;
	{
		std::istringstream inputStream(inputArgs);
		std::string        splitVal, argName, argValue;
		while(getline(inputStream, splitVal, ';'))
		{
			std::istringstream pairInputStream(splitVal);
			getline(pairInputStream, argName, ',');
			getline(pairInputStream, argValue, ',');
			argsIn.push_back(std::make_pair(argName, argValue));
		}
	}

	// check namesOfInputArguments_
	if(macro.namesOfInputArguments_.size() != argsIn.size())
	{
		__CFG_SS__ << "MacroMaker Macro '" << macro.macroName_ << "' was attempted on interfaceID '" << interfaceID << "' with a mismatch in"
		           << " number of input arguments. " << argsIn.size() << " were given. " << macro.namesOfInputArguments_.size() << " expected." << __E__;
		__CFG_SS_THROW__;
	}
	for(unsigned int i = 0; i < argsIn.size(); ++i)
		if(macro.namesOfInputArguments_.find(argsIn[i].first) == macro.namesOfInputArguments_.end())
		{
			__CFG_SS__ << "MacroMaker Macro '" << macro.macroName_ << "' was attempted on interfaceID '" << interfaceID << "' with a mismatch in"
			           << " a name of an input argument. " << argsIn[i].first
			           << " was given. Expected: " << StringMacros::setToString(macro.namesOfInputArguments_) << __E__;

			__CFG_SS_THROW__;
		}

	// build output arguments
	std::vector<std::string>                      returnStrings;
	std::vector<FEVInterface::frontEndMacroArg_t> argsOut;

	{
		std::istringstream inputStream(outputArgs);
		std::string        argName;
		while(getline(inputStream, argName, ','))
		{
			__CFG_COUT__ << "argName " << argName << __E__;

			returnStrings.push_back("DEFAULT");  // std::string());
			argsOut.push_back(FEVInterface::frontEndMacroArg_t(argName, returnStrings[returnStrings.size() - 1]));
			//
			//			__CFG_COUT__ << argsOut[argsOut.size()-1].first << __E__;
			//__CFG_COUT__ << (uint64_t) & (returnStrings[returnStrings.size() - 1])
			//	                                		 << __E__;
		}
	}

	// check namesOfOutputArguments_
	if(macro.namesOfOutputArguments_.size() != argsOut.size())
	{
		__CFG_SS__ << "MacroMaker Macro '" << macro.macroName_ << "' was attempted on interfaceID '" << interfaceID << "' with a mismatch in"
		           << " number of output arguments. " << argsOut.size() << " were given. " << macro.namesOfOutputArguments_.size() << " expected." << __E__;

		__CFG_SS_THROW__;
	}
	for(unsigned int i = 0; i < argsOut.size(); ++i)
		if(macro.namesOfOutputArguments_.find(argsOut[i].first) == macro.namesOfOutputArguments_.end())
		{
			__CFG_SS__ << "MacroMaker Macro '" << macro.macroName_ << "' was attempted on interfaceID '" << interfaceID << "' with a mismatch in"
			           << " a name of an output argument. " << argsOut[i].first
			           << " were given. Expected: " << StringMacros::setToString(macro.namesOfOutputArguments_) << __E__;

			__CFG_SS_THROW__;
		}

	__CFG_COUT__ << "# of input args = " << argsIn.size() << __E__;

	std::map<std::string /*name*/, uint64_t /*value*/> variableMap;
	// fill variable map
	for(const auto& outputArgName : macro.namesOfOutputArguments_)
		variableMap.emplace(  // do not care about output arg value
		    std::pair<std::string /*name*/, uint64_t /*value*/>(outputArgName, 0));
	for(const auto& inputArgName : macro.namesOfInputArguments_)
		variableMap.emplace(  // do not care about input arg value
		    std::pair<std::string /*name*/, uint64_t /*value*/>(inputArgName, 0));

	for(auto& argIn : argsIn)  // set map values
	{
		__CFG_COUT__ << argIn.first << ": " << argIn.second << __E__;
		StringMacros::getNumber(argIn.second, variableMap.at(argIn.first));
	}

	fe->runMacro(macro, variableMap);

	__CFG_COUT__ << "MacroMaker Macro complete!" << __E__;

	__CFG_COUT__ << "# of output args = " << argsOut.size() << __E__;
	for(auto& arg : argsOut)
	{
		std::stringstream numberSs;
		numberSs << std::dec << variableMap.at(arg.first) << " (0x" << std::hex << variableMap.at(arg.first) << ")" << std::dec;
		arg.second = numberSs.str();
		__CFG_COUT__ << arg.first << ": " << arg.second << __E__;
	}

	// Success! at this point so return the output string
	outputArgs = "";
	for(unsigned int i = 0; i < argsOut.size(); ++i)
	{
		if(i)
			outputArgs += ";";
		outputArgs += argsOut[i].first + "," + argsOut[i].second;
	}

	__CFG_COUT__ << "outputArgs = " << outputArgs << __E__;

}  // end runMacro()

//==============================================================================
// runFEMacro
//	Runs the FE Macro in the specified FE interface.
//
//	inputs:
//		- inputArgs: colon-separated name/value pairs, and then comma-separated
//		- outputArgs: comma-separated
//
//	outputs:
//		- throws exception on failure
//		- outputArgs: colon-separate name/value pairs, and then comma-separated
void FEVInterfacesManager::runFEMacro(const std::string& interfaceID, const std::string& feMacroName, const std::string& inputArgs, std::string& outputArgs)
{
	// check for interfaceID
	FEVInterface* fe = getFEInterfaceP(interfaceID);

	// have pointer to virtual FEInterface, find Macro structure
	auto FEMacroIt = fe->getMapOfFEMacroFunctions().find(feMacroName);
	if(FEMacroIt == fe->getMapOfFEMacroFunctions().end())
	{
		__CFG_SS__ << "FE Macro '" << feMacroName << "' of interfaceID '" << interfaceID << "' was not found!" << __E__;
		__CFG_COUT_ERR__ << "\n" << ss.str();
		__CFG_SS_THROW__;
	}

	runFEMacro(interfaceID, FEMacroIt->second, inputArgs, outputArgs);

}  // end runFEMacro()

//==============================================================================
// runFEMacro
//	Runs the FE Macro in the specified FE interface.
//
//	inputs:
//		- inputArgs: semicolon-separated name/value pairs, and then comma-separated
//		- outputArgs: comma-separated
//
//	outputs:
//		- throws exception on failure
//		- outputArgs: colon-separate name/value pairs, and then comma-separated
void FEVInterfacesManager::runFEMacro(const std::string&                         interfaceID,
                                      const FEVInterface::frontEndMacroStruct_t& feMacro,
                                      const std::string&                         inputArgs,
                                      std::string&                               outputArgs)
{
	// build input arguments
	//	parse args, semicolon-separated pairs, and then comma-separated
	std::vector<FEVInterface::frontEndMacroArg_t> argsIn;
	{
		std::istringstream inputStream(inputArgs);
		std::string        splitVal, argName, argValue;
		while(getline(inputStream, splitVal, ';'))
		{
			std::istringstream pairInputStream(splitVal);
			getline(pairInputStream, argName, ',');
			getline(pairInputStream, argValue, ',');
			argsIn.push_back(std::make_pair(StringMacros::decodeURIComponent(argName), StringMacros::decodeURIComponent(argValue)));
		}
	}

	// check namesOfInputArguments_
	if(feMacro.namesOfInputArguments_.size() != argsIn.size())
	{
		__CFG_SS__ << "FE Macro '" << feMacro.feMacroName_ << "' of interfaceID '" << interfaceID << "' was attempted with a mismatch in"
		           << " number of input arguments. " << argsIn.size() << " were given. " << feMacro.namesOfInputArguments_.size() << " expected." << __E__;
		__CFG_COUT_ERR__ << "\n" << ss.str();
		__CFG_SS_THROW__;
	}
	for(unsigned int i = 0; i < argsIn.size(); ++i)
		if(argsIn[i].first != feMacro.namesOfInputArguments_[i])
		{
			__CFG_SS__ << "FE Macro '" << feMacro.feMacroName_ << "' of interfaceID '" << interfaceID << "' was attempted with a mismatch in"
			           << " a name of an input argument. " << argsIn[i].first << " was given. " << feMacro.namesOfInputArguments_[i] << " expected." << __E__;
			__CFG_COUT_ERR__ << "\n" << ss.str();
			__CFG_SS_THROW__;
		}

	// build output arguments
	std::vector<std::string>                      returnStrings;
	std::vector<FEVInterface::frontEndMacroArg_t> argsOut;

	{
		std::istringstream inputStream(outputArgs);
		std::string        argName;
		while(getline(inputStream, argName, ','))
		{
			__CFG_COUT__ << "argName " << argName << __E__;

			returnStrings.push_back("DEFAULT");  // std::string());
			argsOut.push_back(FEVInterface::frontEndMacroArg_t(StringMacros::decodeURIComponent(argName), returnStrings[returnStrings.size() - 1]));
			//
			//			__CFG_COUT__ << argsOut[argsOut.size()-1].first << __E__;
			__CFG_COUT__ << (uint64_t) & (returnStrings[returnStrings.size() - 1]) << __E__;
		}
	}

	// check namesOfOutputArguments_
	if(feMacro.namesOfOutputArguments_.size() != argsOut.size())
	{
		__CFG_SS__ << "FE Macro '" << feMacro.feMacroName_ << "' of interfaceID '" << interfaceID << "' was attempted with a mismatch in"
		           << " number of output arguments. " << argsOut.size() << " were given. " << feMacro.namesOfOutputArguments_.size() << " expected." << __E__;
		__CFG_COUT_ERR__ << "\n" << ss.str();
		__CFG_SS_THROW__;
	}
	for(unsigned int i = 0; i < argsOut.size(); ++i)
		if(argsOut[i].first != feMacro.namesOfOutputArguments_[i])
		{
			__CFG_SS__ << "FE Macro '" << feMacro.feMacroName_ << "' of interfaceID '" << interfaceID << "' was attempted with a mismatch in"
			           << " a name of an output argument. " << argsOut[i].first << " were given. " << feMacro.namesOfOutputArguments_[i] << " expected."
			           << __E__;
			__CFG_COUT_ERR__ << "\n" << ss.str();
			__CFG_SS_THROW__;
		}

	__CFG_COUT__ << "# of input args = " << argsIn.size() << __E__;
	for(auto& argIn : argsIn)
		__CFG_COUT__ << argIn.first << ": " << argIn.second << __E__;

	//	__CFG_COUT__ << "# of output args = " << argsOut.size() << __E__;
	//	for(unsigned int i=0;i<argsOut.size();++i)
	//		__CFG_COUT__ << i << ": " << argsOut[i].first << __E__;
	//	for(unsigned int i=0;i<returnStrings.size();++i)
	//		__CFG_COUT__ << i << ": " << returnStrings[i] << __E__;

	__MOUT__ << "Launching FE Macro '" << feMacro.feMacroName_ << "' ..." << __E__;
	__CFG_COUT__ << "Launching FE Macro '" << feMacro.feMacroName_ << "' ..." << __E__;

	// have pointer to Macro structure, so run it
	(getFEInterfaceP(interfaceID)->*(feMacro.macroFunction_))(feMacro, argsIn, argsOut);

	__CFG_COUT__ << "FE Macro complete!" << __E__;

	__CFG_COUT__ << "# of output args = " << argsOut.size() << __E__;
	for(const auto& arg : argsOut)
		__CFG_COUT__ << arg.first << ": " << arg.second << __E__;

	// check namesOfOutputArguments_ size
	if(feMacro.namesOfOutputArguments_.size() != argsOut.size())
	{
		__CFG_SS__ << "FE Macro '" << feMacro.feMacroName_ << "' of interfaceID '" << interfaceID
		           << "' was attempted but the FE macro "
		              "manipulated the output arguments vector. It is illegal "
		              "to add or remove output vector name/value pairs."
		           << __E__;
		__CFG_COUT_ERR__ << "\n" << ss.str();
		__CFG_SS_THROW__;
	}

	// Success! at this point so return the output string
	outputArgs = "";
	for(unsigned int i = 0; i < argsOut.size(); ++i)
	{
		if(i)
			outputArgs += ";";

		// attempt to get number, and output hex version
		// otherwise just output result
		try
		{
			uint64_t tmpNumber;
			if(StringMacros::getNumber(argsOut[i].second, tmpNumber))
			{
				std::stringstream outNumberSs;
				outNumberSs << std::dec << tmpNumber << " (0x" << std::hex << tmpNumber << ")" << std::dec;
				outputArgs += argsOut[i].first + "," + outNumberSs.str();
				continue;
			}
		}
		catch(...)
		{  // ignore error, assume not a number
		}

		outputArgs += argsOut[i].first + "," + StringMacros::encodeURIComponent(argsOut[i].second);
	}

	__CFG_COUT_TYPE__(TLVL_DEBUG+11) << __COUT_HDR__ << "outputArgs = " << outputArgs << __E__;

}  // end runFEMacro()

//==============================================================================
// getFEMacrosString
//	returns string with each new line indicating the macros for a FE
//	each line:
//		<parent supervisor name>;<parent supervisor lid>;<interface type>;<interface UID>
//		;<macro name>;<macro permissions req>;<macro num of inputs>;...<input names ;
// separated>...
//		;<macro num of outputs>;...<output names ; separated>...
//	do not use :-separator because of the : in user permissions strings
std::string FEVInterfacesManager::getFEMacrosString(const std::string& supervisorName, const std::string& supervisorLid)
{
	std::string retList = "";

	__CFG_COUTV__(theFEInterfaces_.size());

	for(const auto& it : theFEInterfaces_)
	{
		__CFG_COUT__ << "FE interface UID = " << it.first << __E__;

		retList += supervisorName + ";" + supervisorLid + ";" + it.second->getInterfaceType() + ";" + it.second->getInterfaceUID();

		for(const auto& macroPair : it.second->getMapOfFEMacroFunctions())
		{
			__CFG_COUT__ << "FE Macro name = " << macroPair.first << __E__;
			retList += ";" + macroPair.first + ";" + macroPair.second.requiredUserPermissions_;
			retList += ";" + StringMacros::encodeURIComponent(macroPair.second.feMacroTooltip_);

			retList += ";" + std::to_string(macroPair.second.namesOfInputArguments_.size());
			for(const auto& name : macroPair.second.namesOfInputArguments_)
				retList += ";" + StringMacros::encodeURIComponent(name);

			retList += ";" + std::to_string(macroPair.second.namesOfOutputArguments_.size());
			for(const auto& name : macroPair.second.namesOfOutputArguments_)
				retList += ";" + StringMacros::encodeURIComponent(name);
		}

		retList += "\n";
	}
	return retList;
}

//==============================================================================
bool FEVInterfacesManager::allFEWorkloopsAreDone(void)
{
	bool allFEWorkloopsAreDone = true;
	bool isActive;

	for(const auto& FEInterface : theFEInterfaces_)
	{
		isActive = FEInterface.second->WorkLoop::isActive();

		__CFG_COUT__ << FEInterface.second->getInterfaceUID() << " of type " << FEInterface.second->getInterfaceType() << ": \t"
		             << "workLoop_->isActive() " << (isActive ? "yes" : "no") << __E__;

		if(isActive)  // then not done
		{
			allFEWorkloopsAreDone = false;
			break;
		}
	}

	return allFEWorkloopsAreDone;
}  // end allFEWorkloopsAreDone()

//==============================================================================
void FEVInterfacesManager::preStateMachineExecutionLoop(void)
{
	VStateMachine::clearIterationWork();
	VStateMachine::clearSubIterationWork();

	stateMachinesIterationWorkCount_ = 0;

	__CFG_COUT__ << "Number of front ends to transition: " << theFENamesByPriority_.size() << __E__;

	if(VStateMachine::getIterationIndex() == 0 && VStateMachine::getSubIterationIndex() == 0)
	{
		// reset map for iterations done on first iteration

		subIterationWorkStateMachineIndex_ = -1;  // clear sub iteration work index

		stateMachinesIterationDone_.clear();
		for(const auto& FEPair : theFEInterfaces_)
			stateMachinesIterationDone_[FEPair.first] = false;  // init to not done
	}
	else
		__CFG_COUT__ << "Iteration " << VStateMachine::getIterationIndex() << "." << VStateMachine::getSubIterationIndex() << "("
		             << (int)subIterationWorkStateMachineIndex_ << ")" << __E__;
}  // end preStateMachineExecutionLoop()

//==============================================================================
void FEVInterfacesManager::preStateMachineExecution(unsigned int i, const std::string& transitionName)
{
	if(i >= theFENamesByPriority_.size())
	{
		__CFG_SS__ << "FE Interface " << i << " not found!" << __E__;
		__CFG_SS_THROW__;
	}

	const std::string& name = theFENamesByPriority_[i];

	FEVInterface* fe = getFEInterfaceP(name);

	fe->VStateMachine::setTransitionName(transitionName);
	fe->VStateMachine::setIterationIndex(VStateMachine::getIterationIndex());
	fe->VStateMachine::setSubIterationIndex(VStateMachine::getSubIterationIndex());

	fe->VStateMachine::clearIterationWork();
	fe->VStateMachine::clearSubIterationWork();

	__CFG_COUT__ << "theStateMachineImplementation Iteration " << fe->VStateMachine::getIterationIndex() << "." << fe->VStateMachine::getSubIterationIndex()
	             << __E__;
}  // end preStateMachineExecution()

//==============================================================================
// postStateMachineExecution
//	return false to indicate state machine is NOT done with transition
bool FEVInterfacesManager::postStateMachineExecution(unsigned int i)
{
	if(i >= theFENamesByPriority_.size())
	{
		__CFG_SS__ << "FE Interface index " << i << " not found!" << __E__;
		__CFG_SS_THROW__;
	}

	const std::string& name = theFENamesByPriority_[i];

	FEVInterface* fe = getFEInterfaceP(name);

	// sub-iteration has priority
	if(fe->VStateMachine::getSubIterationWork())
	{
		subIterationWorkStateMachineIndex_ = i;
		VStateMachine::indicateSubIterationWork();

		__CFG_COUT__ << "FE Interface '" << name << "' is flagged for another sub-iteration..." << __E__;
		return false;  // to indicate state machine is NOT done with transition
	}
	else
	{
		subIterationWorkStateMachineIndex_ = -1;  // clear sub iteration work index

		bool& stateMachineDone = stateMachinesIterationDone_[name];
		stateMachineDone       = !fe->VStateMachine::getIterationWork();

		if(!stateMachineDone)
		{
			__CFG_COUT__ << "FE Interface '" << name << "' is flagged for another iteration..." << __E__;
			VStateMachine::indicateIterationWork();  // mark not done at
			                                         // FEVInterfacesManager level
			++stateMachinesIterationWorkCount_;      // increment still working count
			return false;                            // to indicate state machine is NOT done with transition
		}
	}

	fe->VStateMachine::setTransitionName("");  // clear transition
	return true;                               // to indicate state machine is done with transition
}  // end postStateMachineExecution()

//==============================================================================
void FEVInterfacesManager::postStateMachineExecutionLoop(void)
{
	if(VStateMachine::getSubIterationWork())
		__CFG_COUT__ << "FE Interface state machine implementation " << subIterationWorkStateMachineIndex_ << " is flagged for another sub-iteration..."
		             << __E__;
	else if(VStateMachine::getIterationWork())
		__CFG_COUT__ << stateMachinesIterationWorkCount_
		             << " FE Interface state machine implementation(s) flagged for "
		                "another iteration..."
		             << __E__;
	else
		__CFG_COUT__ << "Done transitioning all state machine implementations..." << __E__;
}  // end postStateMachineExecutionLoop()
