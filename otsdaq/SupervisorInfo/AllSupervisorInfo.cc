#include "otsdaq/SupervisorInfo/AllSupervisorInfo.h"

#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/TablePlugins/XDAQContextTable/XDAQContextTable.h"

#include <iostream>

using namespace ots;

const bool AllSupervisorInfo::MACROMAKER_MODE = ((getenv("MACROMAKER_MODE") == NULL)  // check Macro Maker mode environment variable in a safe way
                                                     ? (false)
                                                     : ((std::string(__ENV__("MACROMAKER_MODE")) == "1") ? true : false));

//==============================================================================
AllSupervisorInfo::AllSupervisorInfo(void) : theSupervisorInfo_(nullptr), theWizardInfo_(nullptr), theARTDAQSupervisorInfo_(nullptr) {}

//==============================================================================
AllSupervisorInfo::AllSupervisorInfo(xdaq::ApplicationContext* applicationContext) : AllSupervisorInfo() { init(applicationContext); }

//==============================================================================
AllSupervisorInfo::~AllSupervisorInfo(void) { destroy(); }

//==============================================================================
void AllSupervisorInfo::destroy(void)
{
	allSupervisorInfo_.clear();
	allFETypeSupervisorInfo_.clear();
	allDMTypeSupervisorInfo_.clear();

	theSupervisorInfo_       = nullptr;
	theWizardInfo_           = nullptr;
	theARTDAQSupervisorInfo_ = nullptr;

	SupervisorDescriptorInfoBase::destroy();

}  // end destroy()

//==============================================================================
void AllSupervisorInfo::init(xdaq::ApplicationContext* applicationContext)
{
	__COUT__ << "Initializing info based on XDAQ context..." << __E__;

	AllSupervisorInfo::destroy();
	SupervisorDescriptorInfoBase::init(applicationContext);

	auto allDescriptors = SupervisorDescriptorInfoBase::getAllDescriptors();
	// ready.. loop through all descriptors, and organize

	//	for(const auto& descriptor:allDescriptors)
	//	{
	//		SupervisorInfo tempSupervisorInfo(
	//						descriptor.second /* descriptor */,
	//						"" /* config app name */,"" /* config parent context name */
	////skip  configuration info
	//						);
	//
	//		__COUT__ << "id " << descriptor.second->getLocalId() << " url " <<
	// descriptor.second->getContextDescriptor()->getURL() << __E__;
	//
	//	}
	//	__COUTV__(XDAQContextTable::GATEWAY_SUPERVISOR_CLASS);

	// Steps:
	//	1. first pass, identify Wiz mode or not
	//	2. second pass, organize supervisors

	bool isWizardMode        = false;
	theSupervisorInfo_       = nullptr;  // reset
	theWizardInfo_           = nullptr;  // reset
	theARTDAQSupervisorInfo_ = nullptr;  // reset

	// first pass, identify Wiz mode or not
	//	accept first encountered (wizard or gateway) as the mode
	for(const auto& descriptor : allDescriptors)
	{
		SupervisorInfo tempSupervisorInfo(
		    descriptor.second /* descriptor */, "" /* config app name */, "" /* config parent context name */  // skip configuration info
		);

		// check for gateway supervisor
		if(tempSupervisorInfo.isGatewaySupervisor())
		{
			// found normal mode, done with first pass
			isWizardMode = false;
			break;
		}
		else if(tempSupervisorInfo.isWizardSupervisor())
		{
			// found wiz mode, done with first pass
			isWizardMode = true;
			break;
		}
	}

	if(AllSupervisorInfo::MACROMAKER_MODE)
		__COUT__ << "Initializing info for Macro Maker mode XDAQ context..." << __E__;
	else if(isWizardMode)
		__COUT__ << "Initializing info for Wiz mode XDAQ context..." << __E__;
	else
		__COUT__ << "Initializing info for Normal mode XDAQ context..." << __E__;
	std::unique_ptr<ConfigurationManager> cfgMgr((isWizardMode || AllSupervisorInfo::MACROMAKER_MODE) ? 0 : new ConfigurationManager());
	const XDAQContextTable* contextConfig = (isWizardMode || AllSupervisorInfo::MACROMAKER_MODE) ? nullptr : cfgMgr->__GET_CONFIG__(XDAQContextTable);
	//__COUTV__(contextConfig);

	// For TRACE controllers in normal mode, temporarily make a map by hostname of supervisors
	// std::map<std::string /*hostname*/,std::pair<unsigned int, SupervisorInfo>> TRACEAppMap;

	std::string name, contextName;
	// do not involve the Configuration Manager
	//	as it adds no valid information to the supervisors
	//	present in wiz mode
	for(const auto& descriptor : allDescriptors)
	{
		name        = contextConfig ? contextConfig->getApplicationUID(descriptor.second->getContextDescriptor()->getURL(), descriptor.second->getLocalId())
		                            : "" /* config app name */;
		contextName = contextConfig ? contextConfig->getContextUID(descriptor.second->getContextDescriptor()->getURL()) : "" /* config parent context name */;

		auto /*<iterator,bool>*/ emplacePair =
		    allSupervisorInfo_.emplace(std::pair<unsigned int, SupervisorInfo>(descriptor.second->getLocalId(),  // descriptor.first,
		                                                                       SupervisorInfo(descriptor.second /* descriptor */, name, contextName)));
		if(!emplacePair.second)
		{
			__SS__ << "Error! Duplicate Application IDs are not allowed. ID =" << descriptor.second->getLocalId() << __E__;
			__SS_THROW__;
		}

		//__COUTV__(emplacePair.first->second.getName());
		//__COUTV__(emplacePair.first->second.getContextName());

		// emplace in allTraceControllerSupervisorInfo_ to find app per host to control trace
		// NOTE: it will fail if there is already an entry for this hostname (which is what we want - keep one app per host)
		if(emplacePair.first->second.isTypeARTDAQSupervisor())
		{
			// make sure artdaq Supervisor represents its host
			if(theARTDAQSupervisorInfo_)
			{
				__SS__ << "Error! Multiple ARTDAQ Supervisors of class " << XDAQContextTable::ARTDAQ_SUPERVISOR_CLASS
				       << " found. There can only be one. ID =" << descriptor.second->getLocalId() << __E__;
				__SS_THROW__;
			}

			theARTDAQSupervisorInfo_ = &(emplacePair.first->second);
		}

		/////////////////////////////////////////////
		// now organize new descriptor by class...

		// check for gateway supervisor
		// note: necessarily exclusive to other Supervisor types
		if(emplacePair.first->second.isGatewaySupervisor())
		{
			if(theSupervisorInfo_)
			{
				__SS__ << "Error! Multiple Gateway Supervisors of class " << XDAQContextTable::GATEWAY_SUPERVISOR_CLASS
				       << " found. There can only be one. ID =" << descriptor.second->getLocalId() << __E__;
				__SS_THROW__;
			}
			theSupervisorInfo_ = &(emplacePair.first->second);
			continue;
		}

		// check for wizard supervisor
		// note: necessarily exclusive to other Supervisor types
		if(emplacePair.first->second.isWizardSupervisor())
		{
			if(theWizardInfo_)
			{
				__SS__ << "Error! Multiple Wizard Supervisors of class " << XDAQContextTable::WIZARD_SUPERVISOR_CLASS
				       << " found. There can only be one. ID =" << descriptor.second->getLocalId() << __E__;
				__SS_THROW__;
			}
			theWizardInfo_ = &(emplacePair.first->second);

			continue;
		}

		// check for FE type, then add to FE group
		// note: not necessarily exclusive to other Supervisor types
		if(emplacePair.first->second.isTypeFESupervisor())
		{
			allFETypeSupervisorInfo_.emplace(std::pair<unsigned int, const SupervisorInfo&>(emplacePair.first->second.getId(), emplacePair.first->second));
		}

		// check for DM type, then add to DM group
		// note: not necessarily exclusive to other Supervisor types
		if(emplacePair.first->second.isTypeDMSupervisor())
		{
			allDMTypeSupervisorInfo_.emplace(std::pair<unsigned int, const SupervisorInfo&>(emplacePair.first->second.getId(), emplacePair.first->second));
		}

		// check for Logbook type, then add to Logbook group
		// note: not necessarily exclusive to other Supervisor types
		if(emplacePair.first->second.isTypeLogbookSupervisor())
		{
			allLogbookTypeSupervisorInfo_.emplace(std::pair<unsigned int, const SupervisorInfo&>(emplacePair.first->second.getId(), emplacePair.first->second));
		}

		// check for MacroMaker type, then add to MacroMaker group
		// note: not necessarily exclusive to other Supervisor types
		if(emplacePair.first->second.isTypeMacroMakerSupervisor())
		{
			allMacroMakerTypeSupervisorInfo_.emplace(
			    std::pair<unsigned int, const SupervisorInfo&>(emplacePair.first->second.getId(), emplacePair.first->second));
		}

	}  // end main extraction loop

	if(AllSupervisorInfo::MACROMAKER_MODE)
	{
		if(theWizardInfo_ || theSupervisorInfo_)
		{
			__SS__ << "Error! For MacroMaker mode, must not have one " << XDAQContextTable::GATEWAY_SUPERVISOR_CLASS << " OR one "
			       << XDAQContextTable::WIZARD_SUPERVISOR_CLASS << " as part of the context configuration! "
			       << "One was found." << __E__;
			__SS_THROW__;
		}
		if(allMacroMakerTypeSupervisorInfo_.size() != 1)
		{
			__SS__ << "Error! For MacroMaker mode, must have one and only one " << *(XDAQContextTable::MacroMakerTypeClassNames_.begin())
			       << " as part of the context configuration! " << allMacroMakerTypeSupervisorInfo_.size() << " were found." << __E__;
			__SS_THROW__;
		}
	}
	else if((!theWizardInfo_ && !theSupervisorInfo_) || (theWizardInfo_ && theSupervisorInfo_))
	{
		__SS__ << "Error! Must have one " << XDAQContextTable::GATEWAY_SUPERVISOR_CLASS << " OR one " << XDAQContextTable::WIZARD_SUPERVISOR_CLASS
		       << " as part of the context configuration! "
		       << "Neither (or both) were found." << __E__;
		__SS_THROW__;
	}

	// wrap up TRACE controller map handling
	{
		// now create the list of TRACE Controller apps (one per host, but priority to artdaq supervisor)
		allTraceControllerSupervisorInfo_.clear();

		if(theARTDAQSupervisorInfo_)  // priority to artdaq supervisor
		{
			__COUT__ << "The ARTDAQ TRACE-controller app for hostname '" << theARTDAQSupervisorInfo_->getHostname()
			         << "' is CLASS:LID = " << theARTDAQSupervisorInfo_->getClass() << ":" << theARTDAQSupervisorInfo_->getId() << __E__;

			// DO NOT USE make_pair here.. it somehow infers types that break the map
			allTraceControllerSupervisorInfo_.emplace(
			    std::pair<std::string, const SupervisorInfo&>(theARTDAQSupervisorInfo_->getHostname(), *theARTDAQSupervisorInfo_));
		}

		// the use emplace, because it will fail insert on collisions! (and we want to keep artdaq selection above)
		for(auto& TRACEApp : allSupervisorInfo_)
		{
			// NOTE!! need to copy const SupervisorInfo& from the persistent copy in the allSupervisorInfo
			if(allTraceControllerSupervisorInfo_
			       .emplace(std::pair<std::string, const SupervisorInfo&>(TRACEApp.second.getHostname(), allSupervisorInfo_.at(TRACEApp.first)))
			       .second)
			{
				__COUT__ << "The TRACE-controller app for hostname '" << TRACEApp.second.getHostname() << "' is CLASS:LID = " << TRACEApp.second.getClass()
				         << ":" << TRACEApp.second.getId() << " name = " << TRACEApp.second.getName() << __E__;
			}
			//			allTraceControllerSupervisorInfo_.emplace(
			//					std::pair<unsigned int, const SupervisorInfo&>(
			//							TRACEApp.second.second.getId(),
			//							allSupervisorInfo_.at(TRACEApp.second.second.getId())));
		}
		__COUT__ << "TRACE-controller app count = " << allTraceControllerSupervisorInfo_.size() << __E__;

		// for(auto& TRACEApp : allTraceControllerSupervisorInfo_)
		// {
		// 	__COUT__ << "The TRACE-controller for hostname = " << TRACEApp.first << "/" << TRACEApp.second.getId() << " is ..."
		// 	         << " name = " << TRACEApp.second.getName() << " class = " << TRACEApp.second.getClass() << " hostname = " << TRACEApp.second.getHostname()
		// 	         << __E__;
		// }
	}

	SupervisorDescriptorInfoBase::destroy();

	__COUT__ << "Supervisor Info initialization complete!" << __E__;

	// for debugging
	// getOrderedSupervisorDescriptors("Configure");
}  // end init()

//==============================================================================
const SupervisorInfo& AllSupervisorInfo::getSupervisorInfo(xdaq::Application* app) const
{
	auto it = allSupervisorInfo_.find(app->getApplicationDescriptor()->getLocalId());
	if(it == allSupervisorInfo_.end())
	{
		__SS__ << "Could not find: " << app->getApplicationDescriptor()->getLocalId() << std::endl;
		__SS_THROW__;
	}
	return it->second;
}

//==============================================================================
void AllSupervisorInfo::setSupervisorStatus(xdaq::Application*                      app,
                                            const std::string&                      status,
                                            const unsigned int                      progress,
                                            const std::string&                      detail,
                                            std::vector<SupervisorInfo::SubappInfo> subapps )
{
	setSupervisorStatus(app->getApplicationDescriptor()->getLocalId(), status, progress, detail, subapps);
}
//==============================================================================
void AllSupervisorInfo::setSupervisorStatus(const SupervisorInfo&                   appInfo,
                                            const std::string&                      status,
                                            const unsigned int                      progress,
                                            const std::string&                      detail,
                                            std::vector<SupervisorInfo::SubappInfo> subapps)
{
	setSupervisorStatus(appInfo.getId(), status, progress, detail, subapps);
}
//==============================================================================
void AllSupervisorInfo::setSupervisorStatus(
    const unsigned int& id, const std::string& status, const unsigned int progress, const std::string& detail, std::vector<SupervisorInfo::SubappInfo> subapps)
{
	auto it = allSupervisorInfo_.find(id);
	if(it == allSupervisorInfo_.end())
	{
		__SS__ << "Could not find: " << id << __E__;
		__SS_THROW__;
	}
	// non-blocking here, it's ok if the status is not updated (it is probably blocked on purpose, for exampled by the GatewaySupervisor broadcast threads)
	if(allSupervisorInfoMutex_[id].try_lock())
	{
		it->second.setStatus(status, progress, detail);
		//if subapps size mismatch, then clear to eliminate renamed subapps
		if(it->second.getSubappInfo().size() != subapps.size())
			it->second.clearSubapps(); 
		for (auto& subapp : subapps) {
			it->second.copySubappStatus(subapp);
		}
		allSupervisorInfoMutex_[id].unlock();
	}
}  // end setSupervisorStatus()

//==============================================================================
void AllSupervisorInfo::clearSupervisorSubappsStatus(const SupervisorInfo&                   appInfo)
{
	auto it = allSupervisorInfo_.find(appInfo.getId());
	if(it == allSupervisorInfo_.end())
	{
		__SS__ << "Could not find: " << appInfo.getId() << __E__;
		__SS_THROW__;
	}
	// non-blocking here, it's ok if the status is not updated (it is probably blocked on purpose, for exampled by the GatewaySupervisor broadcast threads)
	if(allSupervisorInfoMutex_[appInfo.getId()].try_lock())
	{
		it->second.clearSubapps();
		allSupervisorInfoMutex_[appInfo.getId()].unlock();
	}
} //end clearSupervisorSubappsStatus()

//==============================================================================
const SupervisorInfo& AllSupervisorInfo::getGatewayInfo(void) const
{
	if(!theSupervisorInfo_)
	{
		__SS__ << "AllSupervisorInfo was not initialized or no Application of type " << XDAQContextTable::GATEWAY_SUPERVISOR_CLASS << " found!" << __E__
		       << StringMacros::stackTrace() << __E__;
		__SS_THROW__;
	}
	return *theSupervisorInfo_;
}
//==============================================================================
XDAQ_CONST_CALL xdaq::ApplicationDescriptor* AllSupervisorInfo::getGatewayDescriptor(void) const { return getGatewayInfo().getDescriptor(); }

//==============================================================================
const SupervisorInfo& AllSupervisorInfo::getWizardInfo(void) const
{
	if(!theWizardInfo_)
	{
		__SS__ << "AllSupervisorInfo was not initialized or no Application of type " << XDAQContextTable::WIZARD_SUPERVISOR_CLASS << "  found!" << __E__;
		__SS_THROW__;
	}
	return *theWizardInfo_;
}
//==============================================================================
XDAQ_CONST_CALL xdaq::ApplicationDescriptor* AllSupervisorInfo::getWizardDescriptor(void) const { return getWizardInfo().getDescriptor(); }

//==============================================================================
const SupervisorInfo& AllSupervisorInfo::getArtdaqSupervisorInfo(void) const
{
	if(!theARTDAQSupervisorInfo_)
	{
		__SS__ << "AllSupervisorInfo was not initialized or no Application of type " << XDAQContextTable::ARTDAQ_SUPERVISOR_CLASS << "  found!" << __E__;
		__SS_THROW__;
	}
	return *theARTDAQSupervisorInfo_;
}  // end getArtdaqSupervisorInfo()

//==============================================================================
std::vector<std::vector<const SupervisorInfo*>> AllSupervisorInfo::getOrderedSupervisorDescriptors(const std::string& stateMachineCommand,
                                                                                                   bool               onlyGatewayContextSupervisors) const
{
	__COUT__ << "getOrderedSupervisorDescriptors for command " << stateMachineCommand << __E__;

	std::map<uint64_t /*priority*/, std::vector<unsigned int /*appId*/>> orderedByPriority;

	try
	{
		ConfigurationManager                              cfgMgr;
		const std::vector<XDAQContextTable::XDAQContext>& contexts = cfgMgr.__GET_CONFIG__(XDAQContextTable)->getContexts();

		for(const auto& context : contexts)
			if(context.status_)
				for(const auto& app : context.applications_)
				{
					if(!app.status_)
						continue;  // skip disabled apps

					auto it = app.stateMachineCommandPriority_.find(stateMachineCommand);
					if(it == app.stateMachineCommandPriority_.end())
						orderedByPriority[XDAQContextTable::XDAQApplication::DEFAULT_PRIORITY].push_back(
						    app.id_);  // if no priority, then default to
						               // XDAQContextTable::XDAQApplication::DEFAULT_PRIORITY
					else               // take value, and do not allow DEFAULT value of 0 -> force to
					                   // XDAQContextTable::XDAQApplication::DEFAULT_PRIORITY
						orderedByPriority[it->second ? it->second : XDAQContextTable::XDAQApplication::DEFAULT_PRIORITY].push_back(app.id_);

					//__COUT__ << "app.id_ " << app.id_ << __E__;
				}
	}
	catch(...)
	{
		__COUT_ERR__ << "SupervisorDescriptorInfoBase could not access the XDAQ Context "
		                "and Application configuration through the Context Table "
		                "Group."
		             << __E__;
		throw;
	}

	__COUT__ << "Here is the order supervisors will be " << stateMachineCommand << "'d:" << __E__;

	// return ordered set of supervisor infos
	//	skip over Gateway Supervisor,
	//	and other supervisors that do not need state transitions.
	std::vector<std::vector<const SupervisorInfo*>> retVec;
	bool                                            createContainer;
	const std::string                               whitespace = "                                   ";
	for(const auto& priorityAppVector : orderedByPriority)
	{
		createContainer = true;

		for(const auto& priorityApp : priorityAppVector.second)
		{
			auto it = allSupervisorInfo_.find(priorityApp);
			if(it == allSupervisorInfo_.end())
			{
				__SS__ << "Error! Was AllSupervisorInfo properly initialized? The app.id_ " << priorityApp << " priority "
				       << (unsigned int)priorityAppVector.first
				       << " could not be found in AllSupervisorInfo. Was the Context changed? Rebooting ots may fix this issue." << __E__;
				__SS_THROW__;
			}

			//__COUT__ << it->second.getName() << " [" << it->second.getId() << "]: " << "
			// priority? " << 				(unsigned int)priorityAppVector.first <<
			//__E__;

			if(onlyGatewayContextSupervisors && it->second.getContextName() != theSupervisorInfo_->getContextName())
				continue;  // for shutdown and startup only broadcast to apps that are local to the Gateway supervisor

			if(it->second.isGatewaySupervisor())
				continue;  // skip gateway supervisor
			if(it->second.isTypeLogbookSupervisor())
				continue;  // skip logbook supervisor(s)
			if(it->second.isTypeMacroMakerSupervisor())
				continue;  // skip macromaker supervisor(s)
			if(it->second.isTypeConfigurationGUISupervisor())
				continue;  // skip configurationGUI supervisor(s)
			if(it->second.isTypeChatSupervisor())
				continue;  // skip chat supervisor(s)
			if(it->second.isTypeConsoleSupervisor())
				continue;  // skip console supervisor(s)
			if(it->second.isTypeCodeEditorSupervisor())
				continue;  // skip code editor supervisor(s)

			if(createContainer)  // create container first time
			{
				retVec.push_back(std::vector<const SupervisorInfo*>());

				// if default priority, create a new vector container for each entry
				//	so they happen in sequence by default
				// if(priorityAppVector.first !=
				//		XDAQContextTable::XDAQApplication::DEFAULT_PRIORITY)
				// createContainer = false;

				createContainer = false;
			}
			retVec[retVec.size() - 1].push_back(&(it->second));

			__COUT__ << "\t" << it->second.getName() << " [LID=" << it->second.getId()
			         << "]: " << (it->second.getName().size() > whitespace.size() ? "" : whitespace.substr(0, whitespace.size() - it->second.getName().size()))
			         << " priority " << (unsigned int)priorityAppVector.first << " count " << retVec[retVec.size() - 1].size() << __E__;
		}
	}  // end equal priority loop
	return retVec;
}
