#include "otsdaq/GatewaySupervisor/Iterator.h"
#include "otsdaq/CoreSupervisors/CoreSupervisorBase.h"
#include "otsdaq/GatewaySupervisor/GatewaySupervisor.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"
#include "otsdaq/WebUsersUtilities/WebUsers.h"

#include <iostream>
#include <thread>  //for std::thread

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "Iterator"

using namespace ots;

#define ITERATOR_PLAN_HISTORY_FILENAME                          \
	((getenv("SERVICE_DATA_PATH") == NULL)                      \
	     ? (std::string(__ENV__("USER_DATA")) + "/ServiceData") \
	     : (std::string(__ENV__("SERVICE_DATA_PATH")))) +       \
	    "/IteratorPlanHistory.hist"

const std::string Iterator::RESERVED_GEN_PLAN_NAME = "---GENERATED_PLAN---";

//==============================================================================
Iterator::Iterator(GatewaySupervisor* supervisor)
    : workloopRunning_(false)
    , activePlanIsRunning_(false)
    , iteratorBusy_(false)
    , commandPlay_(false)
    , commandPause_(false)
    , commandHalt_(false)
    , activePlanName_("")
    , activeCommandIndex_(-1)
    , activeCommandStartTime_(0)
    , theSupervisor_(supervisor)
{
	__COUT__ << "Iterator constructed." << __E__;

	//restore lastStartedPlanName_ and lastFinishedPlanName_ from file
	__COUT__ << "Filename for iterator history: " << ITERATOR_PLAN_HISTORY_FILENAME
	         << __E__;
	FILE* fp = fopen((ITERATOR_PLAN_HISTORY_FILENAME).c_str(), "r");
	if(fp)  // check for all core table names in file, and force their presence
	{
		char line[100];
		int  i = 0;
		while(fgets(line, 100, fp))
		{
			if(strlen(line) < 3)
				continue;
			line[strlen(line) - 1] = '\0';  // remove endline
			__COUTV__(line);
			if(i == 0)
				lastStartedPlanName_ = line;
			else if(i == 1)
			{
				lastFinishedPlanName_ = line;
				break;  //only 2 lines in file (for now)
			}
			++i;
		}
		fclose(fp);
		__COUTV__(lastStartedPlanName_);
		__COUTV__(lastFinishedPlanName_);
	}  //end get iterator plan history
}  //end constructor()

//==============================================================================
Iterator::~Iterator(void) {}

//==============================================================================
void Iterator::IteratorWorkLoop(Iterator* iterator)
try
{
	__COUT__ << "Iterator work loop starting..." << __E__;

	// mutex init scope
	{
		// lockout the messages array for the remainder of the scope
		// this guarantees the reading thread can safely access the messages
		if(iterator->theSupervisor_->VERBOSE_MUTEX)
			__COUT__ << "Waiting for iterator access" << __E__;
		std::lock_guard<std::mutex> lock(iterator->accessMutex_);
		if(iterator->theSupervisor_->VERBOSE_MUTEX)
			__COUT__ << "Have iterator access" << __E__;

		iterator->errorMessage_ = "";  // clear error message
	}

	ConfigurationManagerRW theConfigurationManager(
	    WebUsers::DEFAULT_ITERATOR_USERNAME);  // this is a restricted username
	// theConfigurationManager.init();
	theConfigurationManager.getAllTableInfo(true /* refresh */,  // to prep all info
	                                        0 /* accumulatedWarnings */,
	                                        "" /* errorFilterName */,
	                                        false /* getGroupKeys */,
	                                        false /* getGroupInfo */,
	                                        true /* initializeActiveGroups */);

	__COUT__ << "Iterator work loop starting..." << __E__;
	IteratorWorkLoopStruct theIteratorStruct(iterator, &theConfigurationManager);
	__COUT__ << "Iterator work loop starting..." << __E__;

	const IterateTable* itConfig;

	std::vector<IterateTable::Command> commands;

	try
	{
		while(1)
		{
			// Process:
			//	- always "listen" for commands
			//		- play: if no plan running, activePlanIsRunning_ = true,
			//			and start or continue plan based on name/commandIndex
			//		- pause: if plan playing, pause it, activePlanIsRunning_ = false
			//			and do not clear commandIndex or name, iteratorBusy_ = true
			//		- halt: if plan playing or not, activePlanIsRunning_ = false
			//			and clear commandIndex or name, iteratorBusy_ = false
			//	- when running
			//		- go through each command
			//			- start the command, commandBusy = true
			//			- check for complete, then commandBusy = false

			// start command handling
			// define mutex scope
			{
				// lockout the messages array for the remainder of the scope
				// this guarantees the reading thread can safely access the messages
				if(iterator->theSupervisor_->VERBOSE_MUTEX)
					__COUT__ << "Waiting for iterator access" << __E__;
				std::lock_guard<std::mutex> lock(iterator->accessMutex_);
				if(iterator->theSupervisor_->VERBOSE_MUTEX)
					__COUT__ << "Have iterator access" << __E__;

				if(iterator->commandPlay_)
				{
					iterator->commandPlay_ = false;  // clear

					if(!iterator->activePlanIsRunning_)
					{
						// valid PLAY command!

						iterator->activePlanIsRunning_ = true;
						iterator->iteratorBusy_        = true;

						if(theIteratorStruct.activePlan_ != iterator->activePlanName_)
						{
							__COUT__ << "New plan name encountered old="
							         << theIteratorStruct.activePlan_
							         << " vs new=" << iterator->activePlanName_ << __E__;
							theIteratorStruct.commandIndex_ = -1;  // reset for new plan
						}

						theIteratorStruct.activePlan_  = iterator->activePlanName_;
						iterator->lastStartedPlanName_ = iterator->activePlanName_;
						FILE* fp = fopen((ITERATOR_PLAN_HISTORY_FILENAME).c_str(), "w");
						if(fp)
						{
							fprintf(fp, "%s\n", iterator->lastStartedPlanName_.c_str());
							fprintf(fp, "%s\n", iterator->lastFinishedPlanName_.c_str());
							fclose(fp);
						}
						else
							__COUT_WARN__ << "Could not open Iterator history file: "
							              << ITERATOR_PLAN_HISTORY_FILENAME << __E__;

						if(theIteratorStruct.commandIndex_ == (unsigned int)-1)
						{
							__COUT__ << "Starting plan '" << theIteratorStruct.activePlan_
							         << ".'" << __E__;
							__COUT__ << "Starting plan '" << theIteratorStruct.activePlan_
							         << ".'" << __E__;
						}
						else
						{
							theIteratorStruct.doResumeAction_ = true;
							__COUT__ << "Continuing plan '"
							         << theIteratorStruct.activePlan_
							         << "' at command index "
							         << theIteratorStruct.commandIndex_ << ". " << __E__;
							__COUT__ << "Continuing plan '"
							         << theIteratorStruct.activePlan_
							         << "' at command index "
							         << theIteratorStruct.commandIndex_ << ". " << __E__;
						}
					}
				}
				else if(iterator->commandPause_ && !theIteratorStruct.doPauseAction_)
				{
					theIteratorStruct.doPauseAction_ = true;
					iterator->commandPause_          = false;  // clear
				}
				else if(iterator->commandHalt_ && !theIteratorStruct.doHaltAction_)
				{
					theIteratorStruct.doHaltAction_ = true;
					iterator->commandHalt_          = false;  // clear
				}

				theIteratorStruct.running_ = iterator->activePlanIsRunning_;

				if(iterator
				       ->activeCommandIndex_ !=  // update active command status if changed
				   theIteratorStruct.commandIndex_)
				{
					iterator->activeCommandIndex_ = theIteratorStruct.commandIndex_;
					if(theIteratorStruct.commandIndex_ <
					   theIteratorStruct.commands_.size())
						iterator->activeCommandType_ =
						    theIteratorStruct.commands_[theIteratorStruct.commandIndex_]
						        .type_;
					else
						iterator->activeCommandType_ = "";
					iterator->activeCommandStartTime_ = time(0);  // reset on any change

					if(theIteratorStruct.commandIndex_ <
					   theIteratorStruct.commandIterations_.size())
						iterator->activeCommandIteration_ =
						    theIteratorStruct
						        .commandIterations_[theIteratorStruct.commandIndex_];
					else
						iterator->activeCommandIteration_ = -1;

					iterator->depthIterationStack_.clear();
					for(const auto& depthIteration : theIteratorStruct.stepIndexStack_)
						iterator->depthIterationStack_.push_back(depthIteration);
					// if(theIteratorStruct.stepIndexStack_.size())
					//	iterator->activeLoopIteration_ =
					// theIteratorStruct.stepIndexStack_.back();  else
					//	iterator->activeLoopIteration_ = -1;

					iterator->activeNumberOfCommands_ =
					    theIteratorStruct.commands_.size();
				}

			}  // end command handling and iterator mutex

			////////////////
			////////////////
			// do halt or pause action outside of iterator mutex

			if(theIteratorStruct.doPauseAction_)
			{
				// valid PAUSE-iterator command!

				// safely pause plan!
				//	i.e. check that command is complete

				__COUT__ << "Waiting to pause..." << __E__;
				while(!iterator->checkCommand(&theIteratorStruct))
					__COUT__ << "Waiting to pause..." << __E__;

				__COUT__ << "Completing pause..." << __E__;

				theIteratorStruct.doPauseAction_ = false;  // clear

				// lockout the messages array for the remainder of the scope
				// this guarantees the reading thread can safely access the messages
				if(iterator->theSupervisor_->VERBOSE_MUTEX)
					__COUT__ << "Waiting for iterator access" << __E__;
				std::lock_guard<std::mutex> lock(iterator->accessMutex_);
				if(iterator->theSupervisor_->VERBOSE_MUTEX)
					__COUT__ << "Have iterator access" << __E__;

				iterator->activePlanIsRunning_ = false;

				__COUT__ << "Paused plan '" << theIteratorStruct.activePlan_
				         << "' at command index " << theIteratorStruct.commandIndex_
				         << ". " << __E__;
				__COUT__ << "Paused plan '" << theIteratorStruct.activePlan_
				         << "' at command index " << theIteratorStruct.commandIndex_
				         << ". " << __E__;

				continue;  // resume workloop
			}
			else if(theIteratorStruct.doHaltAction_)
			{
				// valid HALT-iterator command!

				// safely end plan!
				//	i.e. check that command is complete

				__COUT__ << "Waiting to halt..." << __E__;
				while(!iterator->checkCommand(&theIteratorStruct))
					__COUT__ << "Waiting to halt..." << __E__;

				__COUT__ << "Completing halt..." << __E__;

				theIteratorStruct.doHaltAction_ = false;  // clear

				iterator->haltIterator(iterator, &theIteratorStruct);

				//			//last ditch effort to make sure FSM is halted
				//			iterator->haltIterator(
				//					iterator->theSupervisor_,
				//					theIteratorStruct.fsmName_);
				//
				//			//lockout the messages array for the remainder of the scope
				//			//this guarantees the reading thread can safely access the
				// messages
				//			if(iterator->theSupervisor_->VERBOSE_MUTEX) __COUT__ << "Waiting
				// for  iterator access" << __E__; 			std::lock_guard<std::mutex>
				// lock(iterator->accessMutex_);
				//			if(iterator->theSupervisor_->VERBOSE_MUTEX) __COUT__ << "Have
				// iterator  access" << __E__;
				//
				//			iterator->activePlanIsRunning_ = false;
				//			iterator->iteratorBusy_ = false;
				//
				//			__COUT__ << "Halted plan '" << theIteratorStruct.activePlan_ << "'
				// at  command index " << 					theIteratorStruct.commandIndex_ <<
				//". " << __E__;
				//			__COUT__ << "Halted plan '" << theIteratorStruct.activePlan_ << "'
				// at  command index " << 					theIteratorStruct.commandIndex_ <<
				//". " << __E__;
				//
				//			theIteratorStruct.activePlan_ = ""; //clear
				//			theIteratorStruct.commandIndex_ = -1; //clear

				continue;  // resume workloop
			}

			////////////////
			////////////////
			//	handle running
			//		__COUT__ << "thinking.." << theIteratorStruct.running_ << " " <<
			//				theIteratorStruct.activePlan_ << " cmd=" <<
			//				theIteratorStruct.commandIndex_ << __E__;
			if(theIteratorStruct.running_ &&
			   theIteratorStruct.activePlan_ !=
			       "")  // important, because after errors, still "running" until halt
			{
				if(theIteratorStruct.commandIndex_ == (unsigned int)-1)
				{
					// initialize the running plan

					__COUT__ << "Get commands" << __E__;

					theIteratorStruct.commandIndex_ = 0;

					theIteratorStruct.cfgMgr_
					    ->init();  // completely reset to re-align with any changes

					if(theIteratorStruct.activePlan_ == Iterator::RESERVED_GEN_PLAN_NAME)
					{
						__COUT__ << "Using generated plan..." << __E__;
						theIteratorStruct.onlyConfigIfNotConfigured_ =
						    iterator->genKeepConfiguration_;
						theIteratorStruct.commands_ =
						    generateIterationPlan(iterator->genFsmName_,
						                          iterator->genConfigAlias_,
						                          iterator->genPlanDurationSeconds_,
						                          iterator->genPlanNumberOfRuns_);
					}
					else
					{
						__COUT__ << "Getting iterator table..." << __E__;
						itConfig =
						    theIteratorStruct.cfgMgr_->__GET_CONFIG__(IterateTable);
						theIteratorStruct.onlyConfigIfNotConfigured_ = false;
						theIteratorStruct.commands_ = itConfig->getPlanCommands(
						    theIteratorStruct.cfgMgr_, theIteratorStruct.activePlan_);
					}

					// reset commandIteration counts and any label stacks left by a
					//	plan halted mid-loop
					theIteratorStruct.commandIterations_.clear();
					theIteratorStruct.stepIndexStack_.clear();
					theIteratorStruct.stepLabelStack_.clear();
					for(auto& command : theIteratorStruct.commands_)
					{
						theIteratorStruct.commandIterations_.push_back(0);
						__COUT__ << "command " << command.type_ << __E__;
						__COUT__ << "table "
						         << IterateTable::commandToTableMap_.at(command.type_)
						         << __E__;
						__COUT__ << "param count = " << command.params_.size() << __E__;

						for(auto& param : command.params_)
						{
							__COUT__ << "\t param " << param.first << " : "
							         << param.second << __E__;
						}
					}

					theIteratorStruct.originalTrackChanges_ =
					    ConfigurationInterface::isVersionTrackingEnabled();
					theIteratorStruct.originalConfigGroup_ =
					    theIteratorStruct.cfgMgr_->getActiveGroupName();
					theIteratorStruct.originalConfigKey_ =
					    theIteratorStruct.cfgMgr_->getActiveGroupKey();

					__COUT__ << "originalTrackChanges "
					         << theIteratorStruct.originalTrackChanges_ << __E__;
					__COUT__ << "originalConfigGroup "
					         << theIteratorStruct.originalConfigGroup_ << __E__;
					__COUT__ << "originalConfigKey "
					         << theIteratorStruct.originalConfigKey_ << __E__;

				}  // end initial section

				if(!theIteratorStruct.commandBusy_)
				{
					if(theIteratorStruct.commandIndex_ <
					   theIteratorStruct.commands_.size())
					{
						// execute command
						theIteratorStruct.commandBusy_ = true;

						__COUT__ << "Iterator starting command "
						         << theIteratorStruct.commandIndex_ + 1 << ": "
						         << theIteratorStruct
						                .commands_[theIteratorStruct.commandIndex_]
						                .type_
						         << __E__;
						__COUT__ << "Iterator starting command "
						         << theIteratorStruct.commandIndex_ + 1 << ": "
						         << theIteratorStruct
						                .commands_[theIteratorStruct.commandIndex_]
						                .type_
						         << __E__;

						iterator->startCommand(&theIteratorStruct);
					}
					else if(theIteratorStruct.commandIndex_ ==
					        theIteratorStruct.commands_.size())  // Done!
					{
						__COUT__ << "Finished Iteration Plan '"
						         << theIteratorStruct.activePlan_ << __E__;
						__COUT__ << "Finished Iteration Plan '"
						         << theIteratorStruct.activePlan_ << __E__;

						__COUT__ << "Reverting track changes." << __E__;
						ConfigurationInterface::setVersionTrackingEnabled(
						    theIteratorStruct.originalTrackChanges_);

						//l
						__COUT__ << "Activating original group..." << __E__;
						try
						{
							theIteratorStruct.cfgMgr_->activateTableGroup(
							    theIteratorStruct.originalConfigGroup_,
							    theIteratorStruct.originalConfigKey_);
						}
						catch(...)
						{
							__COUT_WARN__ << "Original group could not be activated."
							              << __E__;
						}

						// leave FSM halted
						__COUT__ << "Completing Iteration Plan and cleaning up..."
						         << __E__;

						iterator->haltIterator(
						    iterator, &theIteratorStruct, true /* doNotHaltFSM */);
					}
				}
				else if(theIteratorStruct.commandBusy_)
				{
					// check for command completion
					if(iterator->checkCommand(&theIteratorStruct))
					{
						theIteratorStruct.commandBusy_ = false;  // command complete

						++theIteratorStruct.commandIndex_;

						__COUT__ << "Ready for next command. Done with "
						         << theIteratorStruct.commandIndex_ << " of "
						         << theIteratorStruct.commands_.size() << __E__;
						__COUT__ << "Iterator ready for next command. Done with "
						         << theIteratorStruct.commandIndex_ << " of "
						         << theIteratorStruct.commands_.size() << __E__;
					}

					// Note: check command gets one shot to resume
					if(theIteratorStruct.doResumeAction_)  // end resume action
						theIteratorStruct.doResumeAction_ = false;
				}

			}  // end running
			else
				sleep(1);  // when inactive sleep a lot

			////////////////
			////////////////

			//		__COUT__ << "end loop.." << theIteratorStruct.running_ << " " <<
			//				theIteratorStruct.activePlan_ << " cmd=" <<
			//				theIteratorStruct.commandIndex_ << __E__;
		}                               /* code */
	}                                   //end try/catch
	catch(const std::runtime_error& e)  //insert info about the state of the iterator
	{
		if(theIteratorStruct.activePlan_ != "")
		{
			__SS__ << "The active Iterator plan name is '"
			       << theIteratorStruct.activePlan_
			       << "'... Here was the error: " << e.what() << __E__;
			__SS_THROW__;
		}
		else
			throw;
	}

	iterator->workloopRunning_ = false;  // if we ever exit
}  //end IteratorWorkLoop()
catch(const std::runtime_error& e)
{
	__SS__ << "Encountered error in Iterator thread. Here is the error:\n"
	       << e.what() << __E__;
	__COUT_ERR__ << ss.str();

	// lockout the messages array for the remainder of the scope
	// this guarantees the reading thread can safely access the messages
	if(iterator->theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Waiting for iterator access" << __E__;
	std::lock_guard<std::mutex> lock(iterator->accessMutex_);
	if(iterator->theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Have iterator access" << __E__;

	iterator->workloopRunning_ = false;  // if we ever exit
	iterator->errorMessage_    = ss.str();
}
catch(...)
{
	__SS__ << "Encountered unknown error in Iterator thread." << __E__;
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
	__COUT_ERR__ << ss.str();

	// lockout the messages array for the remainder of the scope
	// this guarantees the reading thread can safely access the messages
	if(iterator->theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Waiting for iterator access" << __E__;
	std::lock_guard<std::mutex> lock(iterator->accessMutex_);
	if(iterator->theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Have iterator access" << __E__;

	iterator->workloopRunning_ = false;  // if we ever exit
	iterator->errorMessage_    = ss.str();
}  // end IteratorWorkLoop() exception handling

//==============================================================================
void Iterator::startCommand(IteratorWorkLoopStruct* iteratorStruct)
try
{
	{
		int i = 0;
		for(const auto& depthIteration : iteratorStruct->stepIndexStack_)
		{
			__COUT__ << i++ << ":" << depthIteration << __E__;
		}
	}

	// should be mutually exclusive with GatewaySupervisor main thread state machine
	// accesses  lockout the messages array for the remainder of the scope  this
	// guarantees the reading thread can safely access the messages
	if(iteratorStruct->theIterator_->theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Waiting for FSM access" << __E__;
	std::lock_guard<std::mutex> lock(
	    iteratorStruct->theIterator_->theSupervisor_->stateMachineAccessMutex_);
	if(iteratorStruct->theIterator_->theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Have FSM access" << __E__;

	// for out of range, throw exception - should never happen
	if(iteratorStruct->commandIndex_ >= iteratorStruct->commands_.size())
	{
		__SS__ << "Out of range commandIndex = " << iteratorStruct->commandIndex_
		       << " in size = " << iteratorStruct->commands_.size() << __E__;
		__SS_THROW__;
	}

	// increment iteration count for command
	++iteratorStruct->commandIterations_[iteratorStruct->commandIndex_];

	std::string type = iteratorStruct->commands_[iteratorStruct->commandIndex_].type_;
	const std::string& targetSubsystem =
	    iteratorStruct->commands_[iteratorStruct->commandIndex_].targetSubsystem_;

	// fail loudly rather than silently acting on the local system
	if(targetSubsystem.size() &&
	   (type == IterateTable::COMMAND_ACTIVATE_ALIAS ||
	    type == IterateTable::COMMAND_ACTIVATE_GROUP ||
	    type == IterateTable::COMMAND_MODIFY_ACTIVE_GROUP ||
	    type == IterateTable::COMMAND_RUN || type == IterateTable::COMMAND_WAIT ||
	    type == IterateTable::COMMAND_BEGIN_LABEL ||
	    type == IterateTable::COMMAND_REPEAT_LABEL ||
	    type == IterateTable::COMMAND_CHOOSE_FSM))
	{
		__SS__ << "Command type '" << type << "' can not target a remote subsystem ('"
		       << targetSubsystem << "'). Clear the TargetSubsystem for this command."
		       << __E__;
		__SS_THROW__;
	}

	if(type == IterateTable::COMMAND_BEGIN_LABEL)
	{
		return startCommandBeginLabel(iteratorStruct);
	}
	else if(type == IterateTable::COMMAND_CHOOSE_FSM)
	{
		return startCommandChooseFSM(
		    iteratorStruct,
		    iteratorStruct->commands_[iteratorStruct->commandIndex_]
		        .params_[IterateTable::commandChooseFSMParams_.NameOfFSM_]);
	}
	else if(type == IterateTable::COMMAND_CONFIGURE_ACTIVE_GROUP)
	{
		if(targetSubsystem.size())
		{
			// use the remote subsystem's own active config group, not the top-level's
			std::string groupAlias;
			{
				std::lock_guard<std::mutex> lock(
				    iteratorStruct->theIterator_->theSupervisor_
				        ->remoteGatewayAppsMutex_);
				const auto& remoteApp = iteratorStruct->theIterator_->theSupervisor_
				                            ->remoteGatewayApps_[findRemoteGatewayApp(
				                                iteratorStruct, targetSubsystem)];
				if(remoteApp.activeConfigGroupName == "" ||
				   remoteApp.activeConfigGroupKey.isInvalid())
				{
					__SS__ << "Remote subsystem '" << targetSubsystem
					       << "' has not reported an active configuration group yet, "
					          "so CONFIGURE_ACTIVE_GROUP can not be applied to it."
					       << __E__;
					__SS_THROW__;
				}
				groupAlias = "GROUP:" + remoteApp.activeConfigGroupName + ":" +
				             remoteApp.activeConfigGroupKey.toString();
			}
			return startRemoteCommandConfigure(
			    iteratorStruct, groupAlias, targetSubsystem);
		}
		return startCommandConfigureActive(iteratorStruct);
	}
	else if(type == IterateTable::COMMAND_CONFIGURE_ALIAS)
	{
		const std::string& systemAlias =
		    iteratorStruct->commands_[iteratorStruct->commandIndex_]
		        .params_[IterateTable::commandConfigureAliasParams_.SystemAlias_];
		if(targetSubsystem.size())
			return startRemoteCommandConfigure(
			    iteratorStruct, systemAlias, targetSubsystem);
		return startCommandConfigureAlias(iteratorStruct, systemAlias);
	}
	else if(type == IterateTable::COMMAND_CONFIGURE_GROUP)
	{
		if(targetSubsystem.size())
		{
			auto& params =
			    iteratorStruct->commands_[iteratorStruct->commandIndex_].params_;
			std::string groupAlias =
			    "GROUP:" + params[IterateTable::commandConfigureGroupParams_.GroupName_] +
			    ":" + params[IterateTable::commandConfigureGroupParams_.GroupKey_];
			return startRemoteCommandConfigure(
			    iteratorStruct, groupAlias, targetSubsystem);
		}
		return startCommandConfigureGroup(iteratorStruct);
	}
	else if(type == IterateTable::COMMAND_ACTIVATE_ALIAS)
	{
		ConfigurationManagerRW*               cfgMgr = iteratorStruct->cfgMgr_;
		std::pair<std::string, TableGroupKey> newActiveGroup =
		    cfgMgr->getTableGroupFromAlias(
		        iteratorStruct->commands_[iteratorStruct->commandIndex_]
		            .params_[IterateTable::commandActivateAliasParams_.SystemAlias_]);
		cfgMgr->loadTableGroup(
		    newActiveGroup.first, newActiveGroup.second, true /*activate*/);
	}
	else if(type == IterateTable::COMMAND_ACTIVATE_GROUP)
	{
		ConfigurationManagerRW* cfgMgr = iteratorStruct->cfgMgr_;
		cfgMgr->loadTableGroup(
		    iteratorStruct->commands_[iteratorStruct->commandIndex_]
		        .params_[IterateTable::commandActivateGroupParams_.GroupName_],
		    TableGroupKey(
		        iteratorStruct->commands_[iteratorStruct->commandIndex_]
		            .params_[IterateTable::commandActivateGroupParams_.GroupKey_]),
		    true /*activate*/);
	}
	else if(type == IterateTable::COMMAND_EXECUTE_FE_MACRO)
	{
		if(targetSubsystem.size())
			return startRemoteCommandMacro(iteratorStruct, true /*isFEMacro*/);
		return startCommandMacro(iteratorStruct, true /*isFEMacro*/);
	}
	else if(type == IterateTable::COMMAND_EXECUTE_MACRO)
	{
		if(targetSubsystem.size())
			return startRemoteCommandMacro(iteratorStruct, false /*isFEMacro*/);
		return startCommandMacro(iteratorStruct, false /*isFEMacro*/);
	}
	else if(type == IterateTable::COMMAND_MODIFY_ACTIVE_GROUP)
	{
		return startCommandModifyActive(iteratorStruct);
	}
	else if(type == IterateTable::COMMAND_REPEAT_LABEL)
	{
		return startCommandRepeatLabel(iteratorStruct);
	}
	else if(type == IterateTable::COMMAND_RUN)
	{
		return startCommandRun(iteratorStruct);
	}
	else if(type == IterateTable::COMMAND_WAIT)
	{
		return startCommandWait(iteratorStruct);
	}
	else if(type == IterateTable::COMMAND_START)
	{
		if(targetSubsystem.size())
			return startRemoteCommandFSMTransition(
			    iteratorStruct,
			    RunControlStateMachine::START_TRANSITION_NAME,
			    targetSubsystem);
		return startCommandFSMTransition(iteratorStruct,
		                                 RunControlStateMachine::START_TRANSITION_NAME);
	}
	else if(type == IterateTable::COMMAND_STOP)
	{
		if(targetSubsystem.size())
			return startRemoteCommandFSMTransition(
			    iteratorStruct,
			    RunControlStateMachine::STOP_TRANSITION_NAME,
			    targetSubsystem);
		return startCommandFSMTransition(iteratorStruct,
		                                 RunControlStateMachine::STOP_TRANSITION_NAME);
	}
	else if(type == IterateTable::COMMAND_PAUSE)
	{
		if(targetSubsystem.size())
			return startRemoteCommandFSMTransition(
			    iteratorStruct,
			    RunControlStateMachine::PAUSE_TRANSITION_NAME,
			    targetSubsystem);
		return startCommandFSMTransition(iteratorStruct,
		                                 RunControlStateMachine::PAUSE_TRANSITION_NAME);
	}
	else if(type == IterateTable::COMMAND_RESUME)
	{
		if(targetSubsystem.size())
			return startRemoteCommandFSMTransition(
			    iteratorStruct,
			    RunControlStateMachine::RESUME_TRANSITION_NAME,
			    targetSubsystem);
		return startCommandFSMTransition(iteratorStruct,
		                                 RunControlStateMachine::RESUME_TRANSITION_NAME);
	}
	else if(type == IterateTable::COMMAND_HALT)
	{
		if(targetSubsystem.size())
			return startRemoteCommandFSMTransition(
			    iteratorStruct,
			    RunControlStateMachine::HALT_TRANSITION_NAME,
			    targetSubsystem);
		return startCommandFSMTransition(iteratorStruct,
		                                 RunControlStateMachine::HALT_TRANSITION_NAME);
	}
	else
	{
		__SS__ << "Failed attempt to start unrecognized command type = " << type << __E__;
		__COUT_ERR__ << ss.str();
		__SS_THROW__;
	}
}
catch(...)
{
	__COUT__ << "Error caught. Reverting track changes." << __E__;
	ConfigurationInterface::setVersionTrackingEnabled(
	    iteratorStruct->originalTrackChanges_);

	__COUT__ << "Activating original group..." << __E__;
	try
	{
		iteratorStruct->cfgMgr_->activateTableGroup(iteratorStruct->originalConfigGroup_,
		                                            iteratorStruct->originalConfigKey_);
	}
	catch(...)
	{
		__COUT_WARN__ << "Original group could not be activated." << __E__;
	}
	throw;
}  // end startCommand()

//==============================================================================
/// checkCommand
///	when busy for a while, start to sleep
///		use sleep() or nanosleep()
bool Iterator::checkCommand(IteratorWorkLoopStruct* iteratorStruct)
try
{
	// for out of range, return done
	if(iteratorStruct->commandIndex_ >= iteratorStruct->commands_.size())
	{
		__COUT__ << "Out of range commandIndex = " << iteratorStruct->commandIndex_
		         << " in size = " << iteratorStruct->commands_.size() << __E__;
		return true;
	}

	std::string type = iteratorStruct->commands_[iteratorStruct->commandIndex_].type_;
	const std::string& targetSubsystem =
	    iteratorStruct->commands_[iteratorStruct->commandIndex_].targetSubsystem_;

	if(type == IterateTable::COMMAND_BEGIN_LABEL)
	{
		// do nothing
		return true;
	}
	else if(type == IterateTable::COMMAND_CHOOSE_FSM)
	{
		// do nothing
		return true;
	}
	else if(type == IterateTable::COMMAND_CONFIGURE_ALIAS ||
	        type == IterateTable::COMMAND_CONFIGURE_ACTIVE_GROUP ||
	        type == IterateTable::COMMAND_CONFIGURE_GROUP)
	{
		if(targetSubsystem.size())
			return checkRemoteCommandConfigure(iteratorStruct, targetSubsystem);
		return checkCommandConfigure(iteratorStruct);
	}
	else if(type == IterateTable::COMMAND_ACTIVATE_ALIAS ||
	        type == IterateTable::COMMAND_ACTIVATE_GROUP)
	{
		// do nothing
		return true;
	}
	else if(type == IterateTable::COMMAND_EXECUTE_FE_MACRO)
	{
		if(targetSubsystem.size())
			return checkRemoteCommandMacro(iteratorStruct, true /*isFEMacro*/);
		return checkCommandMacro(iteratorStruct, true /*isFEMacro*/);
	}
	else if(type == IterateTable::COMMAND_EXECUTE_MACRO)
	{
		if(targetSubsystem.size())
			return checkRemoteCommandMacro(iteratorStruct, false /*isFEMacro*/);
		return checkCommandMacro(iteratorStruct, false /*isFEMacro*/);
	}
	else if(type == IterateTable::COMMAND_MODIFY_ACTIVE_GROUP)
	{
		// do nothing
		return true;
	}
	else if(type == IterateTable::COMMAND_REPEAT_LABEL)
	{
		// do nothing
		return true;
	}
	else if(type == IterateTable::COMMAND_RUN)
	{
		return checkCommandRun(iteratorStruct);
	}
	else if(type == IterateTable::COMMAND_WAIT)
	{
		return checkCommandWait(iteratorStruct);
	}
	else if(type == IterateTable::COMMAND_START)
	{
		if(targetSubsystem.size())
			return checkRemoteCommandFSMTransition(
			    iteratorStruct, "Running", targetSubsystem);
		return checkCommandFSMTransition(iteratorStruct, "Running");
	}
	else if(type == IterateTable::COMMAND_STOP)
	{
		if(targetSubsystem.size())
			return checkRemoteCommandFSMTransition(
			    iteratorStruct, "Configured", targetSubsystem);
		return checkCommandFSMTransition(iteratorStruct, "Configured");
	}
	else if(type == IterateTable::COMMAND_PAUSE)
	{
		if(targetSubsystem.size())
			return checkRemoteCommandFSMTransition(
			    iteratorStruct, "Paused", targetSubsystem);
		return checkCommandFSMTransition(iteratorStruct, "Paused");
	}
	else if(type == IterateTable::COMMAND_RESUME)
	{
		if(targetSubsystem.size())
			return checkRemoteCommandFSMTransition(
			    iteratorStruct, "Running", targetSubsystem);
		return checkCommandFSMTransition(iteratorStruct, "Running");
	}
	else if(type == IterateTable::COMMAND_HALT)
	{
		if(targetSubsystem.size())
			return checkRemoteCommandFSMTransition(
			    iteratorStruct,
			    RunControlStateMachine::HALTED_STATE_NAME,
			    targetSubsystem);
		return checkCommandFSMTransition(iteratorStruct,
		                                 RunControlStateMachine::HALTED_STATE_NAME);
	}
	else
	{
		__SS__ << "Attempt to check unrecognized command type = " << type << __E__;
		__COUT_ERR__ << ss.str();
		__SS_THROW__;
	}
}
catch(...)
{
	__COUT__ << "Error caught. Reverting track changes." << __E__;
	ConfigurationInterface::setVersionTrackingEnabled(
	    iteratorStruct->originalTrackChanges_);

	__COUT__ << "Activating original group..." << __E__;
	try
	{
		iteratorStruct->cfgMgr_->activateTableGroup(iteratorStruct->originalConfigGroup_,
		                                            iteratorStruct->originalConfigKey_);
	}
	catch(...)
	{
		__COUT_WARN__ << "Original group could not be activated." << __E__;
	}

	throw;
}  // end checkCommand()

//==============================================================================
void Iterator::startCommandChooseFSM(IteratorWorkLoopStruct* iteratorStruct,
                                     const std::string&      fsmName)
{
	__COUT__ << "fsmName " << fsmName << __E__;

	iteratorStruct->fsmName_                   = fsmName;
	iteratorStruct->theIterator_->lastFsmName_ = fsmName;

	// Translate fsmName
	//	to gives us run alias (fsmRunAlias_) and next run number (fsmNextRunNumber_)

	// CAREFUL?? Threads

	//// ======================== get run alias based on fsm name ====

	iteratorStruct->fsmRunAlias_ = "Run";  // default to "Run"

	// get stateMachineAliasFilter if possible
	ConfigurationTree configLinkNode = iteratorStruct->cfgMgr_->getSupervisorTableNode(
	    iteratorStruct->theIterator_->theSupervisor_->getContextUID(),
	    iteratorStruct->theIterator_->theSupervisor_->getSupervisorUID());

	if(!configLinkNode.isDisconnected())
	{
		try  // for backwards compatibility
		{
			ConfigurationTree fsmLinkNode =
			    configLinkNode.getNode("LinkToStateMachineTable");
			if(!fsmLinkNode.isDisconnected())
				iteratorStruct->fsmRunAlias_ =
				    fsmLinkNode.getNode(fsmName + "/RunDisplayAlias")
				        .getValue<std::string>();
			else
				__COUT_INFO__ << "FSM Link disconnected." << __E__;
		}
		catch(std::runtime_error& e)
		{
			//__COUT_INFO__ << e.what() << __E__;
			__COUT_INFO__
			    << "No state machine Run alias. Ignoring and assuming alias of '"
			    << iteratorStruct->fsmRunAlias_ << ".'" << __E__;
		}
		catch(...)
		{
			__COUT_ERR__ << "Unknown error. Should never happen." << __E__;

			__COUT_INFO__
			    << "No state machine Run alias. Ignoring and assuming alias of '"
			    << iteratorStruct->fsmRunAlias_ << ".'" << __E__;
		}
	}
	else
		__COUT_INFO__ << "FSM Link disconnected." << __E__;

	__COUT__ << "fsmRunAlias_  = " << iteratorStruct->fsmRunAlias_ << __E__;

	//// ======================== get run number based on fsm name ====

	iteratorStruct->fsmNextRunNumber_ =
	    iteratorStruct->theIterator_->theSupervisor_->getNextRunNumber(
	        iteratorStruct->fsmName_);

	if(iteratorStruct->theIterator_->theSupervisor_->theStateMachine_
	           .getCurrentStateName() == "Running" ||
	   iteratorStruct->theIterator_->theSupervisor_->theStateMachine_
	           .getCurrentStateName() == "Paused")
		--iteratorStruct->fsmNextRunNumber_;  // current run number is one back

	__COUT__ << "fsmNextRunNumber_  = " << iteratorStruct->fsmNextRunNumber_ << __E__;
}  // end startCommandChooseFSM()

//==============================================================================
/// return true if an action was attempted
bool Iterator::haltIterator(Iterator*               iterator,
                            IteratorWorkLoopStruct* iteratorStruct /* = 0 */,
                            bool                    doNotHaltFSM /* = false */)

{
	GatewaySupervisor* theSupervisor = iterator->theSupervisor_;
	const std::string& fsmName       = iterator->lastFsmName_;

	std::vector<std::string> fsmCommandParameters;
	std::string              errorStr = "";
	std::string currentState = theSupervisor->theStateMachine_.getCurrentStateName();

	__COUTV__(currentState);

	bool haltAttempted = true;
	if(doNotHaltFSM)
	{
		__COUT_INFO__ << "Iterator is leaving FSM in current state: " << currentState
		              << ". If this is undesireable, add a Halt command, for example, to "
		                 "the end of your Iteration plan."
		              << __E__;
		haltAttempted = false;
	}
	else if(currentState == RunControlStateMachine::INITIAL_STATE_NAME ||
	        currentState == RunControlStateMachine::HALTED_STATE_NAME)
	{
		__COUT__ << "Do nothing. Already halted." << __E__;
		haltAttempted = false;
	}
	else if(currentState == "Running")
		errorStr = theSupervisor->attemptStateMachineTransition(
		    0,
		    0,
		    RunControlStateMachine::ABORT_TRANSITION_NAME,
		    fsmName,
		    WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
		    WebUsers::DEFAULT_ITERATOR_USERNAME,
		    fsmCommandParameters);
	else
		errorStr = theSupervisor->attemptStateMachineTransition(
		    0,
		    0,
		    RunControlStateMachine::HALT_TRANSITION_NAME,
		    fsmName,
		    WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
		    WebUsers::DEFAULT_ITERATOR_USERNAME,
		    fsmCommandParameters);

	if(haltAttempted)
	{
		if(errorStr != "")
		{
			__SS__ << "Iterator failed to halt because of the following error: "
			       << errorStr;
			__SS_THROW__;
		}

		// else successfully launched
		__COUT__ << "FSM in transition = "
		         << theSupervisor->theStateMachine_.isInTransition() << __E__;
		__COUT__ << "halting state machine launched." << __E__;
	}

	// finish up cleanup of the iterator
	__COUT__ << "Conducting Iterator cleanup." << __E__;

	if(iteratorStruct)
	{
		__COUT__ << "Reverting track changes." << __E__;
		ConfigurationInterface::setVersionTrackingEnabled(
		    iteratorStruct->originalTrackChanges_);

		if(!doNotHaltFSM)
		{
			__COUT__ << "Activating original group..." << __E__;
			try
			{
				iteratorStruct->cfgMgr_->activateTableGroup(
				    iteratorStruct->originalConfigGroup_,
				    iteratorStruct->originalConfigKey_);
			}
			catch(...)
			{
				__COUT_WARN__ << "Original group could not be activated." << __E__;
			}
		}
	}

	// lockout the messages array for the remainder of the scope
	// this guarantees the reading thread can safely access the messages
	if(iterator->theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Waiting for iterator access" << __E__;
	std::lock_guard<std::mutex> lock(iterator->accessMutex_);
	if(iterator->theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Have iterator access" << __E__;

	iterator->activePlanIsRunning_ = false;
	iterator->iteratorBusy_        = false;

	// clear
	iterator->activePlanName_     = "";
	iterator->activeCommandIndex_ = -1;

	if(iteratorStruct)
	{
		__COUT__ << "Iterator cleanup complete of plan '" << iteratorStruct->activePlan_
		         << "' at command index " << iteratorStruct->commandIndex_ << ". "
		         << __E__;

		iterator->lastFinishedPlanName_ = iteratorStruct->activePlan_;
		FILE* fp = fopen((ITERATOR_PLAN_HISTORY_FILENAME).c_str(), "w");
		if(fp)
		{
			fprintf(fp, "%s\n", iterator->lastStartedPlanName_.c_str());
			fprintf(fp, "%s\n", iterator->lastFinishedPlanName_.c_str());
			fclose(fp);
		}
		else
			__COUT_WARN__ << "Could not open Iterator history file: "
			              << ITERATOR_PLAN_HISTORY_FILENAME << __E__;

		iteratorStruct->activePlan_   = "";  // clear
		iteratorStruct->commandIndex_ = -1;  // clear
	}

	return haltAttempted;
}  // end haltIterator()

//==============================================================================
void Iterator::startCommandBeginLabel(IteratorWorkLoopStruct* iteratorStruct)
{
	__COUT__ << "Entering label '"
	         << iteratorStruct->commands_[iteratorStruct->commandIndex_]
	                .params_[IterateTable::commandBeginLabelParams_.Label_]
	         << "'..." << std::endl;

	// add new step index to stack
	iteratorStruct->stepIndexStack_.push_back(0);
	iteratorStruct->stepLabelStack_.push_back(
	    iteratorStruct->commands_[iteratorStruct->commandIndex_]
	        .params_[IterateTable::commandBeginLabelParams_.Label_]);
}  // end startCommandBeginLabel()

//==============================================================================
void Iterator::startCommandRepeatLabel(IteratorWorkLoopStruct* iteratorStruct)
{
	// search for first matching label backward and set command to there

	int numOfRepetitions;
	sscanf(iteratorStruct->commands_[iteratorStruct->commandIndex_]
	           .params_[IterateTable::commandRepeatLabelParams_.NumberOfRepetitions_]
	           .c_str(),
	       "%d",
	       &numOfRepetitions);
	__COUT__ << "numOfRepetitions remaining = " << numOfRepetitions << __E__;

	char repStr[200];

	if(numOfRepetitions <= 0)
	{
		// write original number of repetitions value back
		sprintf(repStr, "%d", iteratorStruct->stepIndexStack_.back());
		iteratorStruct->commands_[iteratorStruct->commandIndex_]
		    .params_[IterateTable::commandRepeatLabelParams_.NumberOfRepetitions_] =
		    repStr;  // re-store as string

		// remove step index from stack
		iteratorStruct->stepIndexStack_.pop_back();
		if(iteratorStruct->stepLabelStack_.size())
			iteratorStruct->stepLabelStack_.pop_back();

		return;  // no more repetitions
	}

	--numOfRepetitions;

	// increment step index in stack
	++(iteratorStruct->stepIndexStack_.back());

	unsigned int i;
	for(i = iteratorStruct->commandIndex_; i > 0;
	    --i)  // assume 0 is always the fallback option
		if(iteratorStruct->commands_[i].type_ == IterateTable::COMMAND_BEGIN_LABEL &&
		   iteratorStruct->commands_[iteratorStruct->commandIndex_]
		           .params_[IterateTable::commandRepeatLabelParams_.Label_] ==
		       iteratorStruct->commands_[i]
		           .params_[IterateTable::commandBeginLabelParams_.Label_])
			break;

	sprintf(repStr, "%d", numOfRepetitions);
	iteratorStruct->commands_[iteratorStruct->commandIndex_]
	    .params_[IterateTable::commandRepeatLabelParams_.NumberOfRepetitions_] =
	    repStr;  // re-store as string

	iteratorStruct->commandIndex_ = i;
	__COUT__ << "Jumping back to commandIndex " << iteratorStruct->commandIndex_ << __E__;
}  // end startCommandRepeatLabel()

//==============================================================================
void Iterator::startCommandRun(IteratorWorkLoopStruct* iteratorStruct)
{
	__COUT__ << "startCommandRun " << __E__;

	iteratorStruct->runIsDone_ = false;
	iteratorStruct->fsmCommandParameters_.clear();

	std::string errorStr     = "";
	std::string currentState = iteratorStruct->theIterator_->theSupervisor_
	                               ->theStateMachine_.getCurrentStateName();

	// execute first transition (may need two)

	__COUTTV__(iteratorStruct->theIterator_->genLogEntry_);

	if(currentState == RunControlStateMachine::CONFIGURED_STATE_NAME)
		errorStr =
		    iteratorStruct->theIterator_->theSupervisor_->attemptStateMachineTransition(
		        0,
		        0,
		        RunControlStateMachine::START_TRANSITION_NAME,
		        iteratorStruct->fsmName_,
		        WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
		        WebUsers::DEFAULT_ITERATOR_USERNAME,
		        iteratorStruct->fsmCommandParameters_,
		        iteratorStruct->activePlan_ == Iterator::RESERVED_GEN_PLAN_NAME
		            ? iteratorStruct->theIterator_->genLogEntry_
		            : "");
	else
		errorStr = "Can only Run from the Configured state. The current state is " +
		           currentState;

	if(errorStr != "")
	{
		__SS__ << "Iterator failed to run because of the following error: " << errorStr;
		__SS_THROW__;
	}

	// save original duration
	sscanf(iteratorStruct->commands_[iteratorStruct->commandIndex_]
	           .params_[IterateTable::commandRunParams_.DurationInSeconds_]
	           .c_str(),
	       "%ld",
	       &iteratorStruct->originalDurationInSeconds_);
	__COUTV__(iteratorStruct->originalDurationInSeconds_);

	// else successfully launched
	__COUT__
	    << "FSM in transition = "
	    << iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.isInTransition()
	    << __E__;
	__COUT__ << "startCommandRun success." << __E__;
}  // end startCommandRun()

//==============================================================================
void Iterator::startCommandWait(IteratorWorkLoopStruct* iteratorStruct)
{
	__COUT__ << "startCommandWait " << __E__;

	iteratorStruct->waitIsDone_ = false;

	// Get original duration
	sscanf(iteratorStruct->commands_[iteratorStruct->commandIndex_]
	           .params_[IterateTable::commandWaitParams_.DurationInSeconds_]
	           .c_str(),
	       "%ld",
	       &iteratorStruct->originalDurationInSeconds_);
	__COUTV__(iteratorStruct->originalDurationInSeconds_);

	__COUT__ << "startCommandWait success." << __E__;
}

//==============================================================================
/// commandSkipsIfConfigured
///	true when the current Configure command should leave an already-Configured FSM
///	alone: either the plan-wide flag (generated plans) or the command's own
///	SkipIfAlreadyConfigured parameter (all three Configure tables share the column name).
bool Iterator::commandSkipsIfConfigured(IteratorWorkLoopStruct* iteratorStruct)
{
	if(iteratorStruct->onlyConfigIfNotConfigured_)
		return true;
	auto& params = iteratorStruct->commands_[iteratorStruct->commandIndex_].params_;
	auto  it =
	    params.find(IterateTable::commandConfigureAliasParams_.SkipIfAlreadyConfigured_);
	return it != params.end() && (it->second == "1" || it->second == "Yes" ||
	                              it->second == "True" || it->second == "On");
}  // end commandSkipsIfConfigured()

//==============================================================================
void Iterator::startCommandConfigureActive(IteratorWorkLoopStruct* iteratorStruct)
{
	__COUT__ << "startCommandConfigureActive " << __E__;

	// steps:
	//	get active config group
	//	transition to configure with parameters describing group

	std::string   group = iteratorStruct->cfgMgr_->getActiveGroupName();
	TableGroupKey key   = iteratorStruct->cfgMgr_->getActiveGroupKey();

	__COUT__ << "group " << group << __E__;
	__COUT__ << "key " << key << __E__;

	// create special alias for this group using : separators

	std::stringstream systemAlias;
	systemAlias << "GROUP:" << group << ":" << key;
	startCommandConfigureAlias(iteratorStruct, systemAlias.str());
}  // end startCommandConfigureActive()

//==============================================================================
void Iterator::startCommandConfigureGroup(IteratorWorkLoopStruct* iteratorStruct)
{
	__COUT__ << "startCommandConfigureGroup " << __E__;

	// steps:
	//	transition to configure with parameters describing group

	std::string group =
	    iteratorStruct->commands_[iteratorStruct->commandIndex_]
	        .params_[IterateTable::commandConfigureGroupParams_.GroupName_];
	TableGroupKey key =
	    TableGroupKey(iteratorStruct->commands_[iteratorStruct->commandIndex_]
	                      .params_[IterateTable::commandConfigureGroupParams_.GroupKey_]);

	__COUT__ << "group " << group << __E__;
	__COUT__ << "key " << key << __E__;

	// create special alias for this group using : separators

	std::stringstream systemAlias;
	systemAlias << "GROUP:" << group << ":" << key;
	startCommandConfigureAlias(iteratorStruct, systemAlias.str());
}  // end startCommandConfigureGroup()

//==============================================================================
void Iterator::startCommandConfigureAlias(IteratorWorkLoopStruct* iteratorStruct,
                                          const std::string&      systemAlias)
{
	__COUT__ << "systemAlias " << systemAlias << __E__;

	iteratorStruct->fsmCommandParameters_.clear();
	iteratorStruct->fsmCommandParameters_.push_back(systemAlias);

	std::string errorStr     = "";
	std::string currentState = iteratorStruct->theIterator_->theSupervisor_
	                               ->theStateMachine_.getCurrentStateName();

	// execute first transition (may need two in conjunction with checkCommandConfigure())

	if(currentState == RunControlStateMachine::INITIAL_STATE_NAME)
		errorStr =
		    iteratorStruct->theIterator_->theSupervisor_->attemptStateMachineTransition(
		        0,
		        0,
		        RunControlStateMachine::INIT_TRANSITION_NAME,
		        iteratorStruct->fsmName_,
		        WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
		        WebUsers::DEFAULT_ITERATOR_USERNAME,
		        iteratorStruct->fsmCommandParameters_);
	else if(currentState == RunControlStateMachine::HALTED_STATE_NAME)
		errorStr =
		    iteratorStruct->theIterator_->theSupervisor_->attemptStateMachineTransition(
		        0,
		        0,
		        RunControlStateMachine::CONFIGURE_TRANSITION_NAME,
		        iteratorStruct->fsmName_,
		        WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
		        WebUsers::DEFAULT_ITERATOR_USERNAME,
		        iteratorStruct->fsmCommandParameters_);
	else if(currentState == RunControlStateMachine::CONFIGURED_STATE_NAME ||
	        currentState == RunControlStateMachine::FAILED_STATE_NAME)
	{
		if(commandSkipsIfConfigured(iteratorStruct) &&
		   currentState != RunControlStateMachine::FAILED_STATE_NAME)
			__COUT_INFO__ << "Already configured and SkipIfAlreadyConfigured is set, so "
			                 "leaving the configuration as-is."
			              << __E__;
		else
			errorStr = iteratorStruct->theIterator_->theSupervisor_
			               ->attemptStateMachineTransition(
			                   0,
			                   0,
			                   RunControlStateMachine::HALT_TRANSITION_NAME,
			                   iteratorStruct->fsmName_,
			                   WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
			                   WebUsers::DEFAULT_ITERATOR_USERNAME,
			                   iteratorStruct->fsmCommandParameters_);
	}
	else
		errorStr =
		    "Can only Configure from the Initial or Halted state. The current state is " +
		    currentState;

	if(errorStr != "")
	{
		__SS__ << "Iterator failed to configure with system alias '"
		       << (iteratorStruct->fsmCommandParameters_.size()
		               ? iteratorStruct->fsmCommandParameters_[0]
		               : "UNKNOWN")
		       << "' because of the following error: " << errorStr;
		__SS_THROW__;
	}

	// else successfully launched
	__COUT__
	    << "FSM in transition = "
	    << iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.isInTransition()
	    << __E__;
	__COUT__ << "startCommandConfigureAlias success." << __E__;
}  // end startCommandConfigureAlias()

//==============================================================================
void Iterator::startCommandFSMTransition(IteratorWorkLoopStruct* iteratorStruct,
                                         const std::string&      transitionCommand)
{
	__COUTV__(transitionCommand);

	iteratorStruct->fsmCommandParameters_.clear();

	std::string errorStr     = "";
	std::string currentState = iteratorStruct->theIterator_->theSupervisor_
	                               ->theStateMachine_.getCurrentStateName();

	// execute first transition (may need two in conjunction with checkCommandConfigure())

	__COUTV__(currentState);

	errorStr =
	    iteratorStruct->theIterator_->theSupervisor_->attemptStateMachineTransition(
	        0,
	        0,
	        transitionCommand,
	        iteratorStruct->fsmName_,
	        WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
	        WebUsers::DEFAULT_ITERATOR_USERNAME,
	        iteratorStruct->fsmCommandParameters_,
	        (transitionCommand == RunControlStateMachine::START_TRANSITION_NAME &&
	         iteratorStruct->activePlan_ == Iterator::RESERVED_GEN_PLAN_NAME)
	            ? iteratorStruct->theIterator_->genLogEntry_
	            : "");

	if(errorStr != "")
	{
		__SS__ << "Iterator failed to " << transitionCommand << " with ";
		if(iteratorStruct->fsmCommandParameters_.size() == 0)
			ss << "no parameters ";
		else
			ss << "parameters '"
			   << StringMacros::vectorToString(iteratorStruct->fsmCommandParameters_)
			   << "' ";
		ss << "' because of the following error: " << errorStr;
		__SS_THROW__;
	}

	// else successfully launched
	__COUT__
	    << "FSM in transition = "
	    << iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.isInTransition()
	    << __E__;
	__COUT__ << "startCommandFSMTransition success." << __E__;
}  // end startCommandFSMTransition()

//==============================================================================
void Iterator::startCommandMacro(IteratorWorkLoopStruct* iteratorStruct,
                                 bool                    isFrontEndMacro)
{
	// Steps:
	//	4 parameters  CommandExecuteFEMacroParams:
	//		//targets
	//		const std::string FEMacroName_ 				= "FEMacroName";
	//		//macro parameters (table/groupID)

	const std::string& macroName =
	    iteratorStruct->commands_[iteratorStruct->commandIndex_]
	        .params_[IterateTable::commandExecuteMacroParams_.MacroName_];
	const std::string& enableSavingOutput =
	    iteratorStruct->commands_[iteratorStruct->commandIndex_]
	        .params_[IterateTable::commandExecuteMacroParams_.EnableSavingOutput_];
	const std::string& outputFilePath =
	    iteratorStruct->commands_[iteratorStruct->commandIndex_]
	        .params_[IterateTable::commandExecuteMacroParams_.OutputFilePath_];
	const std::string& outputFileRadix =
	    iteratorStruct->commands_[iteratorStruct->commandIndex_]
	        .params_[IterateTable::commandExecuteMacroParams_.OutputFileRadix_];
	const std::string inputArgs = applyStepIndexToMacroArgs(
	    iteratorStruct,
	    iteratorStruct->commands_[iteratorStruct->commandIndex_]
	        .params_[IterateTable::commandExecuteMacroParams_.MacroArgumentString_],
	    iteratorStruct->commands_[iteratorStruct->commandIndex_]
	        .params_[IterateTable::commandExecuteMacroParams_.MacroArgumentLabels_]);

	__COUTV__(macroName);
	__COUTV__(enableSavingOutput);
	__COUTV__(outputFilePath);
	__COUTV__(outputFileRadix);
	__COUTV__(inputArgs);

	// send request to MacroMaker a la FEVInterface::runFrontEndMacro
	//	but need to pass iteration information, so that the call is launched by just one
	// send 	to each front end. 	Front-ends must immediately respond that is started
	//		FEVInterfacesManager.. must start a thread for running the macro iterations
	//	Then check for complete.

	iteratorStruct->targetsDone_.clear();  // reset

	__COUTV__(iteratorStruct->commands_[iteratorStruct->commandIndex_].targets_.size());
	for(const auto& target :
	    iteratorStruct->commands_[iteratorStruct->commandIndex_].targets_)
	{
		__COUT__ << "target " << target.table_ << ":" << target.UID_ << __E__;

		// for each target, init to not done
		iteratorStruct->targetsDone_.push_back(false);

		xoap::MessageReference message =
		    SOAPUtilities::makeSOAPMessageReference("FECommunication");

		SOAPParameters parameters;
		std::string    type = isFrontEndMacro ? "feMacroMultiDimensionalStart"
		                                      : "macroMultiDimensionalStart";
		parameters.addParameter("type", type);
		parameters.addParameter("requester", WebUsers::DEFAULT_ITERATOR_USERNAME);
		parameters.addParameter("targetInterfaceID", target.UID_);
		parameters.addParameter(isFrontEndMacro ? "feMacroName" : "macroName", macroName);
		parameters.addParameter("enableSavingOutput", enableSavingOutput);
		parameters.addParameter("outputFilePath", outputFilePath);
		parameters.addParameter("outputFileRadix", outputFileRadix);
		parameters.addParameter("inputArgs", inputArgs);
		SOAPUtilities::addParameters(message, parameters);

		__COUT__ << "Sending FE communication: " << SOAPUtilities::translate(message)
		         << __E__;

		xoap::MessageReference replyMessage =
		    iteratorStruct->theIterator_->theSupervisor_
		        ->SOAPMessenger::sendWithSOAPReply(
		            iteratorStruct->theIterator_->theSupervisor_->allSupervisorInfo_
		                .getAllMacroMakerTypeSupervisorInfo()
		                .begin()
		                ->second.getDescriptor(),
		            message);

		__COUT__ << "Response received: " << SOAPUtilities::translate(replyMessage)
		         << __E__;

		SOAPParameters rxParameters;
		rxParameters.addParameter("Error");
		std::string response = SOAPUtilities::receive(replyMessage, rxParameters);

		std::string error = rxParameters.getValue("Error");

		if(response != type + "Done" || error != "")
		{
			// error occurred!
			__SS__ << "Error transmitting request to target interface '" << target.UID_
			       << "' from '" << WebUsers::DEFAULT_ITERATOR_USERNAME << ".' Response '"
			       << response << "' with error: " << error << __E__;
			__SS_THROW__;
		}
	}  // end target loop

}  // end startCommandMacro()

//==============================================================================
bool Iterator::checkCommandMacro(IteratorWorkLoopStruct* iteratorStruct,
                                 bool                    isFrontEndMacro)
{
	sleep(1);

	// Steps:
	//	4 parameters  CommandExecuteFEMacroParams:
	//		//targets
	//		const std::string FEMacroName_ 				= "FEMacroName";
	//		//macro parameters (table/groupID)

	const std::string& macroName =
	    iteratorStruct->commands_[iteratorStruct->commandIndex_]
	        .params_[IterateTable::commandExecuteMacroParams_.MacroName_];

	__COUTV__(macroName);

	// send request to MacroMaker to check completion of macro
	// as targets are identified complete, remove targets_ from vector

	for(unsigned int i = 0;
	    i < iteratorStruct->commands_[iteratorStruct->commandIndex_].targets_.size();
	    ++i)
	{
		ots::IterateTable::CommandTarget& target =
		    iteratorStruct->commands_[iteratorStruct->commandIndex_].targets_[i];

		__COUT__ << "target " << target.table_ << ":" << target.UID_ << __E__;

		xoap::MessageReference message =
		    SOAPUtilities::makeSOAPMessageReference("FECommunication");

		SOAPParameters parameters;
		std::string    type = isFrontEndMacro ? "feMacroMultiDimensionalCheck"
		                                      : "macroMultiDimensionalCheck";
		parameters.addParameter("type", type);
		parameters.addParameter("requester", WebUsers::DEFAULT_ITERATOR_USERNAME);
		parameters.addParameter("targetInterfaceID", target.UID_);
		parameters.addParameter(isFrontEndMacro ? "feMacroName" : "macroName", macroName);
		SOAPUtilities::addParameters(message, parameters);

		__COUT__ << "Sending FE communication: " << SOAPUtilities::translate(message)
		         << __E__;

		xoap::MessageReference replyMessage =
		    iteratorStruct->theIterator_->theSupervisor_
		        ->SOAPMessenger::sendWithSOAPReply(
		            iteratorStruct->theIterator_->theSupervisor_->allSupervisorInfo_
		                .getAllMacroMakerTypeSupervisorInfo()
		                .begin()
		                ->second.getDescriptor(),
		            message);

		__COUT__ << "Response received: " << SOAPUtilities::translate(replyMessage)
		         << __E__;

		SOAPParameters rxParameters;
		rxParameters.addParameter("Error");
		rxParameters.addParameter("Done");
		std::string response = SOAPUtilities::receive(replyMessage, rxParameters);

		std::string error = rxParameters.getValue("Error");
		bool        done  = rxParameters.getValue("Done") == "1";

		if(response != type + "Done" || error != "")
		{
			// error occurred!
			__SS__ << "Error transmitting request to target interface '" << target.UID_
			       << "' from '" << WebUsers::DEFAULT_ITERATOR_USERNAME << ".' Response '"
			       << response << "' with error: " << error << __E__;
			__SS_THROW__;
		}

		if(!done)  // still more to do so give up checking
			return false;

		// mark target done
		iteratorStruct->targetsDone_[i] = true;

		//		iteratorStruct->commands_[iteratorStruct->commandIndex_].targets_.erase(
		//				targetIt--); //go back after delete

	}  // end target loop

	// if here all targets are done
	return true;
}  // end checkCommandMacro()

//==============================================================================
void Iterator::startCommandModifyActive(IteratorWorkLoopStruct* iteratorStruct)
{
	// Steps:
	//	4 parameters commandModifyActiveParams_:
	//		const std::string DoTrackGroupChanges_  TrueFalse
	//		//targets
	//		const std::string RelativePathToField_ 		= "RelativePathToField";
	//		const std::string FieldStartValue_ 			= "FieldStartValue";
	//		const std::string FieldIterationStepSize_ 	= "FieldIterationStepSize";
	//
	//	if tracking changes,
	//		create a new group
	//		for every enabled FE
	//			set field = start value + stepSize * currentStepIndex_
	//		activate group
	//	else
	//		load scratch group
	//		for every enabled FE
	//			set field = start value + stepSize * stepIndex
	//		activate group

	bool doTrackGroupChanges = false;
	if("True" ==
	   iteratorStruct->commands_[iteratorStruct->commandIndex_]
	       .params_[IterateTable::commandModifyActiveParams_.DoTrackGroupChanges_])
		doTrackGroupChanges = true;

	const std::string& startValueStr =
	    iteratorStruct->commands_[iteratorStruct->commandIndex_]
	        .params_[IterateTable::commandModifyActiveParams_.FieldStartValue_];
	const std::string& stepSizeStr =
	    iteratorStruct->commands_[iteratorStruct->commandIndex_]
	        .params_[IterateTable::commandModifyActiveParams_.FieldIterationStepSize_];

	const unsigned int stepIndex = getStepIndexForLabel(iteratorStruct, "" /*innermost*/);

	__COUT__ << "doTrackGroupChanges " << (doTrackGroupChanges ? "yes" : "no")
	         << std::endl;
	__COUTV__(startValueStr);
	__COUTV__(stepSizeStr);
	__COUTV__(stepIndex);

	ConfigurationInterface::setVersionTrackingEnabled(doTrackGroupChanges);

	// two approaches: double or long handling
	// OR TODO -- if step is 0 and startValue is NaN.. then handle as string

	if(((stepSizeStr.size() && stepSizeStr[0] == '0') || !stepSizeStr.size()) &&
	   !StringMacros::isNumber(
	       startValueStr))  //if not a number and no step size, then interpret as string
	{
		__COUT__ << "Treating start value as string: " << startValueStr << __E__;
		helpCommandModifyActive(iteratorStruct, startValueStr, doTrackGroupChanges);
	}
	else if(startValueStr.size() && (startValueStr[startValueStr.size() - 1] == 'f' ||
	                                 startValueStr.find('.') != std::string::npos))
	{
		// handle as double
		double startValue = strtod(startValueStr.c_str(), 0);
		double stepSize   = strtod(stepSizeStr.c_str(), 0);

		__COUT__ << "startValue " << startValue << std::endl;
		__COUT__ << "stepSize " << stepSize << std::endl;
		__COUT__ << "currentValue " << startValue + stepSize * stepIndex << std::endl;

		helpCommandModifyActive(
		    iteratorStruct, startValue + stepSize * stepIndex, doTrackGroupChanges);
	}
	else  // handle as long
	{
		long int startValue;
		long int stepSize;

		StringMacros::getNumber(startValueStr, startValue);
		StringMacros::getNumber(startValueStr, stepSize);

		__COUT__ << "startValue " << startValue << std::endl;
		__COUT__ << "stepSize " << stepSize << std::endl;
		__COUT__ << "currentValue " << startValue + stepSize * stepIndex << std::endl;

		helpCommandModifyActive(
		    iteratorStruct, startValue + stepSize * stepIndex, doTrackGroupChanges);
	}

}  // end startCommandModifyActive()

//==============================================================================
/// checkCommandRun
///	return true if done
///
///	Either will be done on (priority 1) running threads (for Frontends) ending
///		or (priority 2 and ignored if <= 0) duration timeout
///
///	Note: use command structure strings to maintain duration left
///	Note: watch iterator->doPauseAction and iterator->doHaltAction and respond
bool Iterator::checkCommandRun(IteratorWorkLoopStruct* iteratorStruct)
{
	sleep(1);  // sleep to give FSM time to transition

	// all RunControlStateMachine access commands should be mutually exclusive with
	// GatewaySupervisor main thread state machine accesses  should be mutually exclusive
	// with GatewaySupervisor main thread state machine accesses  lockout the messages
	// array for the remainder of the scope  this guarantees the reading thread can safely
	// access the messages
	if(iteratorStruct->theIterator_->theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Waiting for FSM access" << __E__;
	std::lock_guard<std::mutex> lock(
	    iteratorStruct->theIterator_->theSupervisor_->stateMachineAccessMutex_);
	if(iteratorStruct->theIterator_->theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Have FSM access" << __E__;

	if(iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.isInTransition())
		return false;

	iteratorStruct->fsmCommandParameters_.clear();

	std::string errorStr     = "";
	std::string currentState = iteratorStruct->theIterator_->theSupervisor_
	                               ->theStateMachine_.getCurrentStateName();

	/////////////////////
	// check for imposed actions and forced exits
	if(iteratorStruct->doPauseAction_)
	{
		// transition to pause state
		__COUT__ << "Transitioning FSM to Paused..." << __E__;

		if(currentState == "Paused")
		{
			// done with early pause exit!
			__COUT__ << "Transition to Paused complete." << __E__;
			return true;
		}
		else if(currentState == "Running")  // launch transition to pause
			errorStr = iteratorStruct->theIterator_->theSupervisor_
			               ->attemptStateMachineTransition(
			                   0,
			                   0,
			                   "Pause",
			                   iteratorStruct->fsmName_,
			                   WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
			                   WebUsers::DEFAULT_ITERATOR_USERNAME,
			                   iteratorStruct->fsmCommandParameters_);
		else if(currentState == "Configured")
		{
			// no need to pause state machine, no run going on
			__COUT__ << "In Configured state. No need to transition to Paused." << __E__;
			return true;
		}
		else
			errorStr = "Expected to be in Paused. Unexpectedly, the current state is " +
			           currentState +
			           ". Last State Machine error message was as follows: " +
			           iteratorStruct->theIterator_->theSupervisor_->theStateMachine_
			               .getErrorMessage();

		if(errorStr != "")
		{
			__SS__ << "Iterator failed to pause because of the following error: "
			       << errorStr;
			__SS_THROW__;
		}
		return false;
	}
	else if(iteratorStruct->doHaltAction_)
	{
		// transition to halted state
		__COUT__ << "Transitioning FSM to Halted..." << __E__;

		if(currentState == RunControlStateMachine::HALTED_STATE_NAME)
		{
			// done with early halt exit!
			__COUT__ << "Transition to Halted complete." << __E__;
			return true;
		}
		else if(currentState == RunControlStateMachine::
		                            RUNNING_STATE_NAME ||  // launch transition to halt
		        currentState == RunControlStateMachine::PAUSED_STATE_NAME)
			errorStr = iteratorStruct->theIterator_->theSupervisor_
			               ->attemptStateMachineTransition(
			                   0,
			                   0,
			                   RunControlStateMachine::ABORT_TRANSITION_NAME,
			                   iteratorStruct->fsmName_,
			                   WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
			                   WebUsers::DEFAULT_ITERATOR_USERNAME,
			                   iteratorStruct->fsmCommandParameters_);
		else if(currentState == RunControlStateMachine::
		                            CONFIGURED_STATE_NAME)  // launch transition to halt
			errorStr = iteratorStruct->theIterator_->theSupervisor_
			               ->attemptStateMachineTransition(
			                   0,
			                   0,
			                   RunControlStateMachine::HALT_TRANSITION_NAME,
			                   iteratorStruct->fsmName_,
			                   WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
			                   WebUsers::DEFAULT_ITERATOR_USERNAME,
			                   iteratorStruct->fsmCommandParameters_);
		else
			errorStr = "Expected to be in Halted. Unexpectedly, the current state is " +
			           currentState +
			           ". Last State Machine error message was as follows: " +
			           iteratorStruct->theIterator_->theSupervisor_->theStateMachine_
			               .getErrorMessage();

		if(errorStr != "")
		{
			__SS__ << "Iterator failed to halt because of the following error: "
			       << errorStr;
			__SS_THROW__;
		}
		return false;
	}
	else if(iteratorStruct->doResumeAction_)
	{
		// Note: check command gets one shot to resume

		// transition to running state
		__COUT__ << "Transitioning FSM to Running..." << __E__;

		if(currentState == "Paused")  // launch transition to running
			errorStr = iteratorStruct->theIterator_->theSupervisor_
			               ->attemptStateMachineTransition(
			                   0,
			                   0,
			                   RunControlStateMachine::RESUME_TRANSITION_NAME,
			                   iteratorStruct->fsmName_,
			                   WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
			                   WebUsers::DEFAULT_ITERATOR_USERNAME,
			                   iteratorStruct->fsmCommandParameters_);

		if(errorStr != "")
		{
			__SS__ << "Iterator failed to run because of the following error: "
			       << errorStr;
			__SS_THROW__;
		}
		return false;
	}

	/////////////////////
	// normal running

	if(currentState != "Running")
	{
		if(iteratorStruct->runIsDone_ && currentState == "Configured")
		{
			// indication of done
			__COUT__ << "Reached end of run " << iteratorStruct->fsmNextRunNumber_
			         << __E__;
			return true;
		}

		errorStr = "Expected to be in Running. Unexpectedly, the current state is " +
		           currentState + ". Last State Machine error message was as follows: " +
		           iteratorStruct->theIterator_->theSupervisor_->theStateMachine_
		               .getErrorMessage();
	}
	else  // else in Running state! Check for end of run
	{
		bool waitOnRunningThreads = false;
		if("True" == iteratorStruct->commands_[iteratorStruct->commandIndex_]
		                 .params_[IterateTable::commandRunParams_.WaitOnRunningThreads_])
			waitOnRunningThreads = true;

		time_t remainingDurationInSeconds;
		sscanf(iteratorStruct->commands_[iteratorStruct->commandIndex_]
		           .params_[IterateTable::commandRunParams_.DurationInSeconds_]
		           .c_str(),
		       "%ld",
		       &remainingDurationInSeconds);

		__COUT__ << "waitOnRunningThreads " << waitOnRunningThreads << __E__;
		__COUT__ << "remainingDurationInSeconds " << remainingDurationInSeconds << __E__;

		///////////////////
		// priority 1 is waiting on running threads
		if(waitOnRunningThreads)
		{
			//	get status of all running FE workloops
			GatewaySupervisor* theSupervisor =
			    iteratorStruct->theIterator_->theSupervisor_;

			bool allFrontEndsAreDone = true;
			for(auto& it : theSupervisor->allSupervisorInfo_.getAllFETypeSupervisorInfo())
			{
				try
				{
					std::string status = theSupervisor->send(it.second.getDescriptor(),
					                                         "WorkLoopStatusRequest");

					__COUT__ << "FESupervisor instance " << it.first
					         << " has status = " << status << std::endl;

					if(status != CoreSupervisorBase::WORK_LOOP_DONE)
					{
						allFrontEndsAreDone = false;
						break;
					}
				}
				catch(xdaq::exception::Exception& e)
				{
					__SS__ << "Could not retrieve status from FESupervisor instance "
					       << it.first << "." << std::endl;
					__COUT_WARN__ << ss.str();
					errorStr = ss.str();

					if(errorStr != "")
					{
						__SS__
						    << "Iterator failed to run because of the following error: "
						    << errorStr;
						__SS_THROW__;
					}
				}
			}

			if(allFrontEndsAreDone)
			{
				// need to end run!
				__COUT__ << "FE workloops all complete! Stopping run..." << __E__;

				errorStr = iteratorStruct->theIterator_->theSupervisor_
				               ->attemptStateMachineTransition(
				                   0,
				                   0,
				                   "Stop",
				                   iteratorStruct->fsmName_,
				                   WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
				                   WebUsers::DEFAULT_ITERATOR_USERNAME,
				                   iteratorStruct->fsmCommandParameters_);

				if(errorStr != "")
				{
					__SS__
					    << "Iterator failed to stop run because of the following error: "
					    << errorStr;
					__SS_THROW__;
				}

				// write indication of run done into duration
				iteratorStruct->runIsDone_ = true;

				return false;
			}
		}  //end waitOnRunningThreads

		///////////////////
		// priority 2 is duration, if <= 0 it is ignored
		if(remainingDurationInSeconds > 1)
		{
			//reset command start time (for displays) once running is stable
			if(remainingDurationInSeconds == iteratorStruct->originalDurationInSeconds_)
			{
				iteratorStruct->theIterator_->activeCommandStartTime_ =
				    time(0);  // reset on any change
				__COUT__ << "Starting run duration of " << remainingDurationInSeconds
				         << " [s] at time = "
				         << iteratorStruct->theIterator_->activeCommandStartTime_ << " "
				         << StringMacros::getTimestampString(
				                iteratorStruct->theIterator_->activeCommandStartTime_)
				         << __E__;
			}

			--remainingDurationInSeconds;

			// write back to string
			char str[200];
			sprintf(str, "%ld", remainingDurationInSeconds);
			iteratorStruct->commands_[iteratorStruct->commandIndex_]
			    .params_[IterateTable::commandRunParams_.DurationInSeconds_] =
			    str;  // re-store as string
		}
		else if(remainingDurationInSeconds == 1)
		{
			// need to end run!
			__COUT__ << "Time duration reached! Stopping run..." << __E__;

			errorStr = iteratorStruct->theIterator_->theSupervisor_
			               ->attemptStateMachineTransition(
			                   0,
			                   0,
			                   "Stop",
			                   iteratorStruct->fsmName_,
			                   WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
			                   WebUsers::DEFAULT_ITERATOR_USERNAME,
			                   iteratorStruct->fsmCommandParameters_);

			if(errorStr != "")
			{
				__SS__ << "Iterator failed to stop run because of the following error: "
				       << errorStr;
				__SS_THROW__;
			}

			// write indication of run done
			iteratorStruct->runIsDone_ = true;

			// write original duration back to string
			char str[200];
			sprintf(str, "%ld", iteratorStruct->originalDurationInSeconds_);
			iteratorStruct->commands_[iteratorStruct->commandIndex_]
			    .params_[IterateTable::commandRunParams_.DurationInSeconds_] =
			    str;  // re-store as string
			return false;
		}
	}

	if(errorStr != "")
	{
		__SS__ << "Iterator failed to run because of the following error: " << errorStr;
		__SS_THROW__;
	}
	return false;
}  // end checkCommandRun()

//==============================================================================
bool Iterator::checkCommandWait(IteratorWorkLoopStruct* iteratorStruct)
{
	sleep(1);  // sleep for a second

	// Get the remaining time
	long remainingDurationInSeconds;
	sscanf(iteratorStruct->commands_[iteratorStruct->commandIndex_]
	           .params_[IterateTable::commandWaitParams_.DurationInSeconds_]
	           .c_str(),
	       "%ld",
	       &remainingDurationInSeconds);

	__COUT__ << "Wait remaining time: " << remainingDurationInSeconds << " seconds."
	         << __E__;

	// If this is the first check, reset the command start time for the display
	if(remainingDurationInSeconds == iteratorStruct->originalDurationInSeconds_)
	{
		iteratorStruct->theIterator_->activeCommandStartTime_ = time(0);
		__COUT__ << "Starting wait duration of " << remainingDurationInSeconds
		         << " [s] at time = "
		         << iteratorStruct->theIterator_->activeCommandStartTime_ << " "
		         << StringMacros::getTimestampString(
		                iteratorStruct->theIterator_->activeCommandStartTime_)
		         << __E__;
	}

	// Handle pause and halt actions if requested
	if(iteratorStruct->doPauseAction_ || iteratorStruct->doHaltAction_)
	{
		// Restore the original duration before exiting
		char str[200];
		sprintf(str, "%ld", iteratorStruct->originalDurationInSeconds_);
		iteratorStruct->commands_[iteratorStruct->commandIndex_]
		    .params_[IterateTable::commandWaitParams_.DurationInSeconds_] = str;

		return true;  // Command is done when pause or halt is requested
	}

	// Check if we're done waiting
	if(remainingDurationInSeconds <= 1)
	{
		__COUT__ << "Wait duration complete!" << __E__;

		// Restore the original duration
		char str[200];
		sprintf(str, "%ld", iteratorStruct->originalDurationInSeconds_);
		iteratorStruct->commands_[iteratorStruct->commandIndex_]
		    .params_[IterateTable::commandWaitParams_.DurationInSeconds_] = str;

		iteratorStruct->waitIsDone_ = true;
		return true;  // Command is done
	}
	else
	{
		// Decrement the remaining time
		--remainingDurationInSeconds;

		// Write back to string
		char str[200];
		sprintf(str, "%ld", remainingDurationInSeconds);
		iteratorStruct->commands_[iteratorStruct->commandIndex_]
		    .params_[IterateTable::commandWaitParams_.DurationInSeconds_] = str;
	}

	return false;  // Command still in progress
}

//==============================================================================
/// return true if done
bool Iterator::checkCommandConfigure(IteratorWorkLoopStruct* iteratorStruct)
{
	sleep(1);  // sleep to give FSM time to transition

	// all RunControlStateMachine access commands should be mutually exclusive with
	// GatewaySupervisor main thread state machine accesses  should be mutually exclusive
	// with GatewaySupervisor main thread state machine accesses  lockout the messages
	// array for the remainder of the scope  this guarantees the reading thread can safely
	// access the messages
	if(iteratorStruct->theIterator_->theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Waiting for FSM access" << __E__;
	std::lock_guard<std::mutex> lock(
	    iteratorStruct->theIterator_->theSupervisor_->stateMachineAccessMutex_);
	if(iteratorStruct->theIterator_->theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Have FSM access" << __E__;

	if(iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.isInTransition())
		return false;

	std::string errorStr     = "";
	std::string currentState = iteratorStruct->theIterator_->theSupervisor_
	                               ->theStateMachine_.getCurrentStateName();

	if(currentState == RunControlStateMachine::HALTED_STATE_NAME)
		errorStr =
		    iteratorStruct->theIterator_->theSupervisor_->attemptStateMachineTransition(
		        0,
		        0,
		        "Configure",
		        iteratorStruct->fsmName_,
		        WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
		        WebUsers::DEFAULT_ITERATOR_USERNAME,
		        iteratorStruct->fsmCommandParameters_);
	else if(currentState != RunControlStateMachine::CONFIGURED_STATE_NAME)
		errorStr = "Expected to be in '" + RunControlStateMachine::CONFIGURED_STATE_NAME +
		           "' state. Unexpectedly, the current state is " + currentState + "." +
		           ". Last State Machine error message was as follows: " +
		           iteratorStruct->theIterator_->theSupervisor_->theStateMachine_
		               .getErrorMessage();
	else  // else successfully done (in Configured state!)
	{
		__COUT__ << "checkCommandConfigureAlias complete." << __E__;

		// also activate the Iterator config manager to mimic active config
		std::pair<std::string, TableGroupKey> newActiveGroup =
		    iteratorStruct->cfgMgr_->getTableGroupFromAlias(
		        iteratorStruct->fsmCommandParameters_[0]);
		iteratorStruct->cfgMgr_->loadTableGroup(
		    newActiveGroup.first, newActiveGroup.second, true /*activate*/);

		__COUT__ << "originalTrackChanges " << iteratorStruct->originalTrackChanges_
		         << __E__;
		__COUT__ << "originalConfigGroup " << iteratorStruct->originalConfigGroup_
		         << __E__;
		__COUT__ << "originalConfigKey " << iteratorStruct->originalConfigKey_ << __E__;

		__COUT__ << "currentTrackChanges "
		         << ConfigurationInterface::isVersionTrackingEnabled() << __E__;
		__COUT__ << "originalConfigGroup "
		         << iteratorStruct->cfgMgr_->getActiveGroupName() << __E__;
		__COUT__ << "originalConfigKey " << iteratorStruct->cfgMgr_->getActiveGroupKey()
		         << __E__;

		return true;
	}

	if(errorStr != "")
	{
		__SS__ << "Iterator failed to configure with system alias '"
		       << (iteratorStruct->fsmCommandParameters_.size()
		               ? iteratorStruct->fsmCommandParameters_[0]
		               : "UNKNOWN")
		       << "' because of the following error: " << errorStr;
		__SS_THROW__;
	}
	return false;
}  // end checkCommandConfigure()

//==============================================================================
/// return true if done
bool Iterator::checkCommandFSMTransition(IteratorWorkLoopStruct* iteratorStruct,
                                         const std::string&      finalState)
{
	__COUTV__(finalState);

	sleep(1);  // sleep to give FSM time to transition

	// all RunControlStateMachine access commands should be mutually exclusive with
	// GatewaySupervisor main thread state machine accesses  should be mutually exclusive
	// with GatewaySupervisor main thread state machine accesses  lockout the messages
	// array for the remainder of the scope  this guarantees the reading thread can safely
	// access the messages
	if(iteratorStruct->theIterator_->theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Waiting for FSM access" << __E__;
	std::lock_guard<std::mutex> lock(
	    iteratorStruct->theIterator_->theSupervisor_->stateMachineAccessMutex_);
	if(iteratorStruct->theIterator_->theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Have FSM access" << __E__;

	if(iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.isInTransition())
		return false;

	std::string errorStr     = "";
	std::string currentState = iteratorStruct->theIterator_->theSupervisor_
	                               ->theStateMachine_.getCurrentStateName();

	if(currentState != finalState)
		errorStr = "Expected to be in " + finalState +
		           ". Unexpectedly, the current state is " + currentState + "." +
		           ". Last State Machine error message was as follows: " +
		           iteratorStruct->theIterator_->theSupervisor_->theStateMachine_
		               .getErrorMessage();
	else  // else successfully done (in Configured state!)
	{
		__COUT__ << "checkCommandFSMTransition complete." << __E__;

		return true;
	}

	if(errorStr != "")
	{
		__SS__ << "Iterator failed to reach final state '" << finalState
		       << "' because of the following error: " << errorStr;
		__SS_THROW__;
	}
	return false;
}  // end checkCommandConfigure()

//==============================================================================
/// -1 durationSeconds means open-ended single run
std::vector<IterateTable::Command> Iterator::generateIterationPlan(
    const std::string& fsmName,
    const std::string& configAlias,
    uint64_t           durationSeconds /* = -1 */,
    unsigned int       numberOfRuns /* = 1 */)
{
	std::vector<IterateTable::Command> commands;

	//example commands:
	//  Choose FSM name
	//	Configure if needed to selected system alias
	//	Start run
	//		count seconds
	//	Stop run numberOfRuns
	//	loop

	if(durationSeconds == (uint64_t)-1)
		numberOfRuns = 1;
	__COUTV__(durationSeconds);
	__COUTV__(numberOfRuns);

	// --------  Choose FSM name
	{
		commands.push_back(IterateTable::Command());
		commands.back().type_ = IterateTable::COMMAND_CHOOSE_FSM;

		commands.back().params_.emplace(
		    std::pair<std::string /*param name*/, std::string /*param value*/>(
		        IterateTable::commandChooseFSMParams_.NameOfFSM_, fsmName));
	}

	// -------- Configure if needed to selected system alias
	{
		commands.push_back(IterateTable::Command());
		commands.back().type_ = IterateTable::COMMAND_CONFIGURE_ALIAS;

		commands.back().params_.emplace(
		    std::pair<std::string /*param name*/, std::string /*param value*/>(
		        IterateTable::commandConfigureAliasParams_.SystemAlias_, configAlias));
		// generated plans keep using the plan-wide onlyConfigIfNotConfigured_ flag
		commands.back().params_.emplace(
		    std::pair<std::string /*param name*/, std::string /*param value*/>(
		        IterateTable::commandConfigureAliasParams_.SkipIfAlreadyConfigured_,
		        "0"));
	}

	if(durationSeconds == (uint64_t)-1)
	{
		__COUT__ << "Open ended run..." << __E__;

		// -------- Start FSM transtion
		{
			commands.push_back(IterateTable::Command());
			commands.back().type_ = IterateTable::COMMAND_START;
		}
	}
	else
	{
		__COUT__ << "Finite duration run(s)..." << __E__;

		if(numberOfRuns > 1)
		{
			__COUT__ << "Setting up iterator loop for " << numberOfRuns << " runs."
			         << __E__;

			commands.push_back(IterateTable::Command());
			commands.back().type_ = IterateTable::COMMAND_BEGIN_LABEL;

			commands.back().params_.emplace(
			    std::pair<std::string /*param name*/, std::string /*param value*/>(
			        IterateTable::commandBeginLabelParams_.Label_, "GENERATED_LABEL"));
		}

		// -------- Start managed run
		{
			commands.push_back(IterateTable::Command());
			commands.back().type_ = IterateTable::COMMAND_RUN;

			commands.back().params_.emplace(
			    std::pair<std::string /*param name*/, std::string /*param value*/>(
			        IterateTable::commandRunParams_.DurationInSeconds_,
			        std::to_string(durationSeconds)));
		}

		if(numberOfRuns > 1)
		{
			commands.push_back(IterateTable::Command());
			commands.back().type_ = IterateTable::COMMAND_REPEAT_LABEL;

			commands.back().params_.emplace(
			    std::pair<std::string /*param name*/, std::string /*param value*/>(
			        IterateTable::commandRepeatLabelParams_.Label_, "GENERATED_LABEL"));
			commands.back().params_.emplace(
			    std::pair<std::string /*param name*/, std::string /*param value*/>(
			        IterateTable::commandRepeatLabelParams_.NumberOfRepetitions_,
			        std::to_string(numberOfRuns -
			                       1)  //number of repeats (i.e., 1 repeat, gives 2 runs)
			        ));
		}
	}

	__COUTV__(commands.size());

	return commands;
}  //end generateIterationPlan()

//==============================================================================
bool Iterator::handleCommandRequest(HttpXmlDocument&   xmldoc,
                                    const std::string& command,
                                    const std::string& parameter)
try
{
	bool handledCommand = false;
	__COUTTV__(command);
	if(command == "iteratePlay")
	{
		playIterationPlan(xmldoc, parameter);
		handledCommand = true;
	}
	else if(command == "iteratePlayGenerated")
	{
		playGeneratedIterationPlan(xmldoc, parameter);
		handledCommand = true;
	}
	else if(command == "iteratePause")
	{
		pauseIterationPlan(xmldoc);
		handledCommand = true;
	}
	else if(command == "iterateHalt")
	{
		haltIterationPlan(xmldoc);
		handledCommand = true;
	}
	else if(command == "getIterationPlanStatus")
	{
		if(activePlanName_ == "" &&
		   parameter !=
		       "")  //take parameter to set active plan name from GUI manipulations
		{
			activePlanName_ = parameter;
			__COUTV__(activePlanName_);
		}
		getIterationPlanStatus(xmldoc);
		handledCommand = true;
	}
	else  // return true if iterator has control of state machine
	{
		// lockout the messages array for the remainder of the scope
		// this guarantees the reading thread can safely access the messages
		if(theSupervisor_->VERBOSE_MUTEX)
			__COUT__ << "Waiting for iterator access" << __E__;
		std::lock_guard<std::mutex> lock(accessMutex_);
		if(theSupervisor_->VERBOSE_MUTEX)
			__COUT__ << "Have iterator access" << __E__;

		if(iteratorBusy_)
		{
			__SS__ << "Error - Can not accept request because the Iterator "
			       << "is currently "
			       << "in control of State Machine progress. ";
			__COUT_ERR__ << "\n" << ss.str();
			__COUT_ERR__ << "\n" << ss.str();

			xmldoc.addTextElementToData("state_transition_attempted",
			                            "0");  // indicate to GUI transition NOT attempted
			xmldoc.addTextElementToData(
			    "state_transition_attempted_err",
			    ss.str());  // indicate to GUI transition NOT attempted
			theSupervisor_->theStateMachine_.setErrorMessage(ss.str());
			return true;  // to block other commands
		}
	}

	if(handledCommand)
	{
		xmldoc.addTextElementToData("state_transition_attempted",
		                            "1");  // indicate to GUI iterator attempted
		return true;
	}
	//else not an iterator command

	return false;
}  //end handleCommandRequest()
catch(...)
{
	__SS__ << "Error caught by Iterator command handling!" << __E__;
	try
	{
		throw;
	}
	catch(const std::runtime_error& e)
	{
		ss << "\nHere is the error: " << e.what() << __E__;
	}
	catch(...)
	{
		ss << "Uknown error caught." << __E__;
	}

	__COUT_ERR__ << "\n" << ss.str();

	xmldoc.addTextElementToData("state_transition_attempted",
	                            "0");  // indicate to GUI transition NOT attempted
	xmldoc.addTextElementToData("state_transition_attempted_err",
	                            ss.str());  // indicate to GUI transition NOT attempted
	theSupervisor_->theStateMachine_.setErrorMessage(ss.str());
	return true;
}  // end handleCommandRequest() error handling

//==============================================================================
void Iterator::playIterationPlan(HttpXmlDocument& xmldoc, const std::string& planName)
{
	__COUT__ << "Attempting to play iteration plan '" << planName << ".'" << __E__;

	if(planName == Iterator::RESERVED_GEN_PLAN_NAME)
	{
		__SS__ << "Illegal use of reserved iteration plan name '"
		       << Iterator::RESERVED_GEN_PLAN_NAME << "!' Please select a different name."
		       << __E__;
		__SS_THROW__;
	}

	playIterationPlanPrivate(xmldoc, planName);

}  //end playIterationPlan()

//==============================================================================
void Iterator::playGeneratedIterationPlan(HttpXmlDocument&   xmldoc,
                                          const std::string& parametersCSV)
{
	__COUTV__(parametersCSV);
	std::vector<std::string> parameters =
	    StringMacros::getVectorFromString(parametersCSV, {','});

	if(parameters.size() != 6)
	{
		__SS__ << "Malformed CSV parameters to playGeneratedIterationPlan(), must be 6 "
		          "arguments and there were "
		       << parameters.size() << ": " << parametersCSV << __E__;
		__SS_THROW__;
	}
	// parameters[0] /*fsmName*/,
	// parameters[1] /*configAlias*/,
	// parameters[2] /*durationSeconds*/,
	// parameters[3] /*numberOfRuns*/,
	// parameters[4] /*keepConfiguration*/,
	// parameters[5] /*logEntry*/ double encoded
	parameters[5] = StringMacros::decodeURIComponent(parameters[5]);

	uint64_t durationSeconds;
	sscanf(parameters[2].c_str(), "%lu", &durationSeconds);
	unsigned int numberOfRuns;
	sscanf(parameters[3].c_str(), "%u", &numberOfRuns);
	unsigned int keepConfiguration;
	sscanf(parameters[4].c_str(), "%u", &keepConfiguration);
	playGeneratedIterationPlan(xmldoc,
	                           parameters[0] /*fsmName*/,
	                           parameters[1] /*configAlias*/,
	                           durationSeconds,
	                           numberOfRuns,
	                           keepConfiguration,
	                           parameters[5] /*logEntry*/
	);

}  //end playGeneratedIterationPlan()

//==============================================================================
void Iterator::playGeneratedIterationPlan(HttpXmlDocument&   xmldoc,
                                          const std::string& fsmName,
                                          const std::string& configAlias,
                                          uint64_t           durationSeconds /* = -1 */,
                                          unsigned int       numberOfRuns /* = 1 */,
                                          bool keepConfiguration /* = false */,
                                          const std::string& logEntry)
{
	std::string planName = Iterator::RESERVED_GEN_PLAN_NAME;
	__COUT__ << "Attempting to play iteration plan '" << planName << ".'" << __E__;

	genFsmName_             = fsmName;
	genConfigAlias_         = configAlias;
	genPlanDurationSeconds_ = durationSeconds;
	genPlanNumberOfRuns_    = numberOfRuns;
	genKeepConfiguration_   = keepConfiguration;
	genLogEntry_            = logEntry;

	__COUTV__(genFsmName_);
	__COUTV__(genConfigAlias_);
	__COUTV__(genPlanDurationSeconds_);
	__COUTV__(genPlanNumberOfRuns_);
	__COUTV__(genKeepConfiguration_);
	__COUTV__(genLogEntry_);

	playIterationPlanPrivate(xmldoc, planName);

}  //end playGeneratedIterationPlan()

//==============================================================================
///called by both playIterationPlan and playGeneratedIterationPlan
void Iterator::playIterationPlanPrivate(HttpXmlDocument&   xmldoc,
                                        const std::string& planName)
{
	// setup "play" command

	// lockout the messages array for the remainder of the scope
	// this guarantees the reading thread can safely access the messages
	if(theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Waiting for iterator access" << __E__;
	std::lock_guard<std::mutex> lock(accessMutex_);
	if(theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Have iterator access" << __E__;

	if(!activePlanIsRunning_ && !commandPlay_)
	{
		if(!workloopRunning_)
		{
			// start thread with member variables initialized

			workloopRunning_ = true;

			// must start thread first
			std::thread([](Iterator* iterator) { Iterator::IteratorWorkLoop(iterator); },
			            this)
			    .detach();
		}

		activePlanName_ = planName;
		commandPlay_    = true;
	}
	else
	{
		__SS__ << "Invalid play command attempted. Can only play when the Iterator is "
		          "inactive or paused."
		       << " If you would like to restart an iteration plan, first try halting "
		          "the Iterator."
		       << __E__;
		__COUT__ << ss.str();

		xmldoc.addTextElementToData("error_message", ss.str());

		__COUT__ << "Invalid play command attempted. " << activePlanIsRunning_ << " "
		         << commandPlay_ << " " << activePlanName_ << __E__;
	}
}  //end playIterationPlan()

//==============================================================================
void Iterator::pauseIterationPlan(HttpXmlDocument& xmldoc)
{
	__COUT__ << "Attempting to pause iteration plan '" << activePlanName_ << ".'"
	         << __E__;
	__COUT__ << "Attempting to pause iteration plan '" << activePlanName_ << ".'"
	         << __E__;

	// setup "pause" command

	// lockout the messages array for the remainder of the scope
	// this guarantees the reading thread can safely access the messages
	if(theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Waiting for iterator access" << __E__;
	std::lock_guard<std::mutex> lock(accessMutex_);
	if(theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Have iterator access" << __E__;

	if(workloopRunning_ && activePlanIsRunning_ && !commandPause_)
	{
		commandPause_ = true;
	}
	else
	{
		__SS__ << "Invalid pause command attempted. Can only pause when running."
		       << __E__;
		__COUT__ << ss.str();

		xmldoc.addTextElementToData("error_message", ss.str());

		__COUT__ << "Invalid pause command attempted. " << workloopRunning_ << " "
		         << activePlanIsRunning_ << " " << commandPause_ << " " << activePlanName_
		         << __E__;
	}
}  //end pauseIterationPlan()

//==============================================================================
void Iterator::haltIterationPlan(HttpXmlDocument& /*xmldoc*/)
{
	__COUT__ << "Attempting to halt iteration plan '" << activePlanName_ << ".'" << __E__;
	__COUT__ << "Attempting to halt iteration plan '" << activePlanName_ << ".'" << __E__;

	// setup "halt" command

	if(workloopRunning_)
	{
		// lockout the messages array for the remainder of the scope
		// this guarantees the reading thread can safely access the messages
		if(theSupervisor_->VERBOSE_MUTEX)
			__COUT__ << "Waiting for iterator access" << __E__;
		std::lock_guard<std::mutex> lock(accessMutex_);
		if(theSupervisor_->VERBOSE_MUTEX)
			__COUT__ << "Have iterator access" << __E__;

		__COUT__ << "activePlanIsRunning_: " << activePlanIsRunning_ << __E__;
		__COUT__ << "Passing halt command to iterator thread." << __E__;
		commandHalt_ = true;

		// clear
		activePlanName_     = "";
		activeCommandIndex_ = -1;
	}
	else  // no thread, so halt (and reset Error') without command to thread
	{
		__COUT__ << "No thread, so conducting halt. lastFsmName_ = " << lastFsmName_
		         << __E__;
		lastFsmName_ =
		    theSupervisor_
		        ->activeStateMachineName_;  //force haltIterator to be successful
		__COUTV__(lastFsmName_);
		Iterator::haltIterator(this);
	}
}  //end haltIterationPlan()

//==============================================================================
///	return state machine and iterator status
void Iterator::getIterationPlanStatus(HttpXmlDocument& xmldoc)
{
	xmldoc.addTextElementToData(
	    "current_state",
	    theSupervisor_->theStateMachine_.isInTransition()
	        ? theSupervisor_->theStateMachine_.getCurrentTransitionName(
	              theSupervisor_->stateMachineLastCommandInput_)
	        : theSupervisor_->theStateMachine_.getCurrentStateName());

	// xmldoc.addTextElementToData("in_transition",
	// theSupervisor_->theStateMachine_.isInTransition() ? "1" : "0");
	if(theSupervisor_->theStateMachine_.isInTransition())
		xmldoc.addTextElementToData(
		    "transition_progress",
		    theSupervisor_->theProgressBar_.readPercentageString());
	else
		xmldoc.addTextElementToData("transition_progress", "100");

	xmldoc.addNumberElementToData("time_in_state",
	                              theSupervisor_->theStateMachine_.getTimeInState());

	// lockout the messages array for the remainder of the scope
	// this guarantees the reading thread can safely access the messages
	if(theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Waiting for iterator access" << __E__;
	std::lock_guard<std::mutex> lock(accessMutex_);
	if(theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Have iterator access" << __E__;

	xmldoc.addTextElementToData("active_plan", activePlanName_);
	xmldoc.addTextElementToData("last_started_plan", lastStartedPlanName_);
	xmldoc.addTextElementToData("last_finished_plan", lastFinishedPlanName_);

	xmldoc.addNumberElementToData("current_command_index", activeCommandIndex_);
	xmldoc.addNumberElementToData("current_number_of_commands", activeNumberOfCommands_);
	xmldoc.addTextElementToData("current_command_type", activeCommandType_);
	xmldoc.addNumberElementToData("current_command_duration",
	                              time(0) - activeCommandStartTime_);
	xmldoc.addNumberElementToData("current_command_iteration", activeCommandIteration_);
	for(const auto& depthIteration : depthIterationStack_)
		xmldoc.addNumberElementToData("depth_iteration", depthIteration);

	if(activePlanName_ == Iterator::RESERVED_GEN_PLAN_NAME)
	{
		xmldoc.addNumberElementToData("generated_number_of_runs", genPlanNumberOfRuns_);
		xmldoc.addNumberElementToData("generated_duration_of_runs",
		                              genPlanDurationSeconds_);
	}

	if(activePlanIsRunning_ && iteratorBusy_)
	{
		if(workloopRunning_)
			xmldoc.addTextElementToData("active_plan_status", "Running");
		else
			xmldoc.addTextElementToData("active_plan_status", "Error");
	}
	else if(!activePlanIsRunning_ && iteratorBusy_)
		xmldoc.addTextElementToData("active_plan_status", "Paused");
	else
		xmldoc.addTextElementToData("active_plan_status", "Inactive");

	xmldoc.addTextElementToData("error_message", errorMessage_);
}  //end getIterationPlanStatus()

//==============================================================================
size_t Iterator::findRemoteGatewayApp(IteratorWorkLoopStruct* iteratorStruct,
                                      const std::string&      targetSubsystem)
{
	auto& remoteApps = iteratorStruct->theIterator_->theSupervisor_->remoteGatewayApps_;
	for(size_t i = 0; i < remoteApps.size(); ++i)
		if(remoteApps[i].appInfo.name == targetSubsystem)
			return i;

	__SS__ << "Target subsystem '" << targetSubsystem
	       << "' not found in remote gateway apps." << __E__;
	__SS_THROW__;
}  // end findRemoteGatewayApp()

//==============================================================================
void Iterator::queueRemoteGatewayCommand(IteratorWorkLoopStruct* iteratorStruct,
                                         size_t                  remoteAppIndex,
                                         const std::string&      command,
                                         const std::string&      statusLabel)
{
	auto& remoteApp =
	    iteratorStruct->theIterator_->theSupervisor_->remoteGatewayApps_[remoteAppIndex];

	if(remoteApp.command != "")
	{
		__SS__ << "Remote subsystem '" << remoteApp.appInfo.name
		       << "' already has a pending command: " << remoteApp.command << __E__;
		__SS_THROW__;
	}

	remoteApp.clearError();
	remoteApp.command          = command;
	remoteApp.fsmName          = iteratorStruct->fsmName_;
	remoteApp.appInfo.status   = "Launching " + statusLabel;
	remoteApp.appInfo.progress = 0;
	remoteApp.commandSentTime  = time(0);

	__COUT__ << "Remote command '" << command << "' queued for subsystem '"
	         << remoteApp.appInfo.name << "'" << __E__;
}  // end queueRemoteGatewayCommand()

//==============================================================================
/// Mirrors the Configure command assembled by the commandRemoteSubsystem request
/// handler: alias, then SubsystemCommon lists, then LogEntry (which must be last).
std::string Iterator::buildRemoteConfigureCommand(IteratorWorkLoopStruct* iteratorStruct,
                                                  const std::string&      systemAlias)
{
	GatewaySupervisor* gw = iteratorStruct->theIterator_->theSupervisor_;

	std::string command =
	    RunControlStateMachine::CONFIGURE_TRANSITION_NAME + "," + systemAlias;

	std::string subsystemCommonList =
	    StringMacros::setToString(gw->theConfigurationManager_->getVersionAliases(
	        ConfigurationManager::SUBSYSTEM_COMMON_VERSION_ALIAS));
	if(subsystemCommonList.size())
		command += "," + GatewaySupervisor::COMMAND_PARAM_SUBSYSTEM_COMMON_PREAMBLE +
		           StringMacros::encodeURIComponent(subsystemCommonList);

	std::string subsystemCommonOverrideList =
	    StringMacros::setToString(gw->theConfigurationManager_->getVersionAliases(
	        ConfigurationManager::SUBSYSTEM_COMMON_OVERRIDE_VERSION_ALIAS));
	if(subsystemCommonOverrideList.size())
		command += "," +
		           GatewaySupervisor::COMMAND_PARAM_SUBSYSTEM_COMMON_OVERRIDE_PREAMBLE +
		           StringMacros::encodeURIComponent(subsystemCommonOverrideList);

	std::string logEntry = gw->getLastLogEntry(
	    RunControlStateMachine::CONFIGURE_TRANSITION_NAME, iteratorStruct->fsmName_);
	if(logEntry.size())
		command += "," + GatewaySupervisor::COMMAND_PARAM_LOG_ENTRY_PREAMBLE +
		           StringMacros::encodeURIComponent(logEntry);

	return command;
}  // end buildRemoteConfigureCommand()

//==============================================================================
void Iterator::startRemoteCommandFSMTransition(IteratorWorkLoopStruct* iteratorStruct,
                                               const std::string&      transitionCommand,
                                               const std::string&      targetSubsystem)
{
	__COUT__ << "startRemoteCommandFSMTransition: " << transitionCommand << " targeting '"
	         << targetSubsystem << "'" << __E__;

	std::lock_guard<std::mutex> lock(
	    iteratorStruct->theIterator_->theSupervisor_->remoteGatewayAppsMutex_);

	queueRemoteGatewayCommand(iteratorStruct,
	                          findRemoteGatewayApp(iteratorStruct, targetSubsystem),
	                          transitionCommand,
	                          transitionCommand);
}  // end startRemoteCommandFSMTransition()

//==============================================================================
bool Iterator::checkRemoteCommandFSMTransition(IteratorWorkLoopStruct* iteratorStruct,
                                               const std::string&      finalState,
                                               const std::string&      targetSubsystem)
{
	sleep(1);

	std::lock_guard<std::mutex> lock(
	    iteratorStruct->theIterator_->theSupervisor_->remoteGatewayAppsMutex_);

	const auto& remoteApp =
	    iteratorStruct->theIterator_->theSupervisor_
	        ->remoteGatewayApps_[findRemoteGatewayApp(iteratorStruct, targetSubsystem)];

	if(remoteApp.getError() != "")
	{
		__SS__ << "Remote subsystem '" << targetSubsystem
		       << "' reported error: " << remoteApp.getError() << __E__;
		__SS_THROW__;
	}

	if(remoteApp.command != "")
	{
		__COUT__ << "Waiting for command to be sent to '" << targetSubsystem << "'..."
		         << __E__;
		return false;
	}

	const std::string& remoteStatus = remoteApp.appInfo.status;
	__COUT__ << "Remote subsystem '" << targetSubsystem << "' status: " << remoteStatus
	         << " (waiting for '" << finalState << "')" << __E__;

	// Halt is the recovery path out of Failed, so a (possibly stale) Failed status
	// is not an error while waiting to reach Halted
	if(finalState != RunControlStateMachine::HALTED_STATE_NAME &&
	   remoteStatus.find(RunControlStateMachine::FAILED_STATE_NAME) == 0)
	{
		__SS__ << "Remote subsystem '" << targetSubsystem << "' entered '" << remoteStatus
		       << "' while waiting for '" << finalState << "'" << __E__;
		__SS_THROW__;
	}

	if(remoteStatus == finalState)
	{
		__COUT__ << "checkRemoteCommandFSMTransition complete for '" << targetSubsystem
		         << "'" << __E__;
		return true;
	}
	return false;
}  // end checkRemoteCommandFSMTransition()

//==============================================================================
/// Remote analogue of startCommandConfigureAlias(): Configure directly from
/// Initial/Halted, or Halt first (Configure follows in the check) from Configured/Failed.
void Iterator::startRemoteCommandConfigure(IteratorWorkLoopStruct* iteratorStruct,
                                           const std::string&      systemAlias,
                                           const std::string&      targetSubsystem)
{
	__COUT__ << "startRemoteCommandConfigure: alias '" << systemAlias << "' targeting '"
	         << targetSubsystem << "'" << __E__;

	iteratorStruct->fsmCommandParameters_.clear();
	iteratorStruct->fsmCommandParameters_.push_back(systemAlias);
	iteratorStruct->remoteConfigureQueued_ = false;

	std::lock_guard<std::mutex> lock(
	    iteratorStruct->theIterator_->theSupervisor_->remoteGatewayAppsMutex_);

	size_t      remoteAppIndex = findRemoteGatewayApp(iteratorStruct, targetSubsystem);
	std::string currentState =
	    iteratorStruct->theIterator_->theSupervisor_->remoteGatewayApps_[remoteAppIndex]
	        .appInfo.status;
	__COUTV__(currentState);

	if(currentState == RunControlStateMachine::INITIAL_STATE_NAME ||
	   currentState == RunControlStateMachine::HALTED_STATE_NAME)
	{
		queueRemoteGatewayCommand(
		    iteratorStruct,
		    remoteAppIndex,
		    buildRemoteConfigureCommand(iteratorStruct, systemAlias),
		    RunControlStateMachine::CONFIGURE_TRANSITION_NAME);
	}
	else if(currentState == RunControlStateMachine::CONFIGURED_STATE_NAME ||
	        currentState.find(RunControlStateMachine::FAILED_STATE_NAME) == 0)
	{
		if(commandSkipsIfConfigured(iteratorStruct) &&
		   currentState == RunControlStateMachine::CONFIGURED_STATE_NAME)
		{
			__COUT_INFO__
			    << "Remote subsystem '" << targetSubsystem
			    << "' already configured and SkipIfAlreadyConfigured is set, so "
			       "leaving its configuration as-is."
			    << __E__;
			return;  // check sees Configured with nothing queued and completes
		}

		queueRemoteGatewayCommand(iteratorStruct,
		                          remoteAppIndex,
		                          RunControlStateMachine::HALT_TRANSITION_NAME,
		                          RunControlStateMachine::HALT_TRANSITION_NAME);
		iteratorStruct->remoteConfigureQueued_ = true;
	}
	else
	{
		__SS__ << "Iterator failed to configure remote subsystem '" << targetSubsystem
		       << "' with system alias '" << systemAlias
		       << "': Can only Configure from the Initial or Halted state. The current "
		          "state is "
		       << currentState << __E__;
		__SS_THROW__;
	}

	__COUT__ << "startRemoteCommandConfigure success." << __E__;
}  // end startRemoteCommandConfigure()

//==============================================================================
bool Iterator::checkRemoteCommandConfigure(IteratorWorkLoopStruct* iteratorStruct,
                                           const std::string&      targetSubsystem)
{
	sleep(1);

	std::lock_guard<std::mutex> lock(
	    iteratorStruct->theIterator_->theSupervisor_->remoteGatewayAppsMutex_);

	size_t remoteAppIndex = findRemoteGatewayApp(iteratorStruct, targetSubsystem);
	auto&  remoteApp =
	    iteratorStruct->theIterator_->theSupervisor_->remoteGatewayApps_[remoteAppIndex];

	if(remoteApp.getError() != "")
	{
		__SS__ << "Iterator failed to configure remote subsystem '" << targetSubsystem
		       << "' with system alias '"
		       << (iteratorStruct->fsmCommandParameters_.size()
		               ? iteratorStruct->fsmCommandParameters_[0]
		               : "UNKNOWN")
		       << "' because of the following error: " << remoteApp.getError() << __E__;
		__SS_THROW__;
	}

	if(remoteApp.command != "")
	{
		__COUT__ << "Waiting for command to be sent to '" << targetSubsystem << "'..."
		         << __E__;
		return false;
	}

	const std::string& currentState = remoteApp.appInfo.status;
	__COUT__ << "Remote subsystem '" << targetSubsystem << "' status: " << currentState
	         << __E__;

	if(iteratorStruct->remoteConfigureQueued_)
	{
		// Halt was sent first; once Halted, send the real Configure.
		// A stale 'Failed' status is expected briefly here, so do not treat it as
		// an error; a Halt that actually fails surfaces through getError() above.
		if(currentState == RunControlStateMachine::HALTED_STATE_NAME)
		{
			queueRemoteGatewayCommand(
			    iteratorStruct,
			    remoteAppIndex,
			    buildRemoteConfigureCommand(iteratorStruct,
			                                iteratorStruct->fsmCommandParameters_[0]),
			    RunControlStateMachine::CONFIGURE_TRANSITION_NAME);
			iteratorStruct->remoteConfigureQueued_ = false;
		}
		return false;
	}

	if(currentState.find(RunControlStateMachine::FAILED_STATE_NAME) == 0)
	{
		__SS__ << "Remote subsystem '" << targetSubsystem << "' entered '" << currentState
		       << "' while configuring." << __E__;
		__SS_THROW__;
	}

	if(currentState == RunControlStateMachine::CONFIGURED_STATE_NAME)
	{
		__COUT__ << "checkRemoteCommandConfigure complete for '" << targetSubsystem << "'"
		         << __E__;
		return true;
	}
	return false;
}  // end checkRemoteCommandConfigure()

//==============================================================================
/// parseMacroLoopSpec
///	Parses the Iterator MacroArgumentString without materializing the iterations:
///		- format "nIter,arg:init:step,...;nIter2,arg:init:step,...", dimension 0 outermost
///		- step == DEFAULT/Default  -> constant string argument
///		- init or step containing '.' or ending in 'f' -> double, else long
///		- lower dimension wins on a name clash
///	An empty string yields a single iteration with no arguments.
Iterator::MacroLoopSpec Iterator::parseMacroLoopSpec(const std::string& inputArgs)
{
	MacroLoopSpec spec;

	std::vector<std::string> dimensions;
	StringMacros::getVectorFromString(inputArgs, dimensions, {';'});

	if(dimensions.size() == 0 || (dimensions.size() == 1 && dimensions[0] == ""))
	{
		spec.dimIterations.push_back(1);
		spec.dimArgs.push_back({});
	}
	else
		for(unsigned int d = 0; d < dimensions.size(); ++d)
		{
			std::vector<std::string> args;
			StringMacros::getVectorFromString(dimensions[d], args, {','});
			if(args.size() == 0 || args[0] == "")
			{
				__SS__ << "Invalid dimensional arguments! Need number of iterations at "
				          "dimension "
				       << d << __E__;
				__SS_THROW__;
			}
			unsigned long numOfIterations;
			StringMacros::getNumber(args[0], numOfIterations);
			if(numOfIterations == 0)
			{
				__SS__ << "Illegal number of iterations '" << args[0] << "' at dimension "
				       << d << ". Must be a positive integer!" << __E__;
				__SS_THROW__;
			}
			spec.dimIterations.push_back(numOfIterations);
			spec.dimArgs.push_back({});

			for(unsigned int a = 1; a < args.size(); ++a)
			{
				// name may contain ':' (e.g. "Target Link (Default := -1)"), so split from the right
				std::string name, init, step;
				if(!StringMacros::splitMacroArgTriple(args[a], name, init, step))
				{
					__SS__ << "Invalid argument '" << args[a]
					       << "'! Expected name:initialValue:stepSize." << __E__;
					__SS_THROW__;
				}
				MacroLoopSpec::Arg arg;
				arg.name = name;
				if(step == TableViewColumnInfo::DATATYPE_STRING_DEFAULT ||
				   step == TableViewColumnInfo::DATATYPE_STRING_ALT_DEFAULT)
				{
					arg.type = MacroLoopSpec::Arg::STRING;
					arg.sVal = init;
				}
				else if((init.size() &&
				         (init.back() == 'f' || init.find('.') != std::string::npos)) ||
				        (step.size() && (step.back() == 'f' || step.find('.') != std::string::npos)))
				{
					arg.type  = MacroLoopSpec::Arg::DOUBLE;
					arg.dInit = strtod(init.c_str(), 0);
					arg.dStep = strtod(step.c_str(), 0);
				}
				else
				{
					arg.type = MacroLoopSpec::Arg::LONG;
					StringMacros::getNumber(init, arg.lInit);
					StringMacros::getNumber(step, arg.lStep);
				}
				spec.dimArgs.back().push_back(arg);
			}
		}

	// total = product of dimension counts, guarding overflow
	spec.totalIterations = 1;
	for(unsigned long n : spec.dimIterations)
		if(__builtin_mul_overflow(spec.totalIterations, (uint64_t)n, &spec.totalIterations))
		{
			__SS__ << "Dimensional loop '" << inputArgs
			       << "' has more iterations than can be counted (product overflows)."
			       << __E__;
			__SS_THROW__;
		}

	// emit-order argument names, de-duplicated (lower dimension wins)
	for(const auto& dim : spec.dimArgs)
		for(const auto& arg : dim)
		{
			bool clash = false;
			for(const auto& existing : spec.argNames)
				if(existing == arg.name)
				{
					clash = true;
					break;
				}
			if(!clash)
				spec.argNames.push_back(arg.name);
		}

	return spec;
}  // end parseMacroLoopSpec()

//==============================================================================
/// macroLoopIteration
///	Computes the index-th iteration (0-based) as if the dimensions were nested loops
///	with dimension 0 outermost: value = init + step * (this dimension's counter).
std::vector<std::pair<std::string, std::string>> Iterator::macroLoopIteration(
    const MacroLoopSpec& spec, uint64_t index)
{
	// odometer: innermost (last) dimension turns fastest
	std::vector<uint64_t> counters(spec.dimIterations.size(), 0);
	for(size_t d = spec.dimIterations.size(); d-- > 0;)
	{
		counters[d] = index % spec.dimIterations[d];
		index /= spec.dimIterations[d];
	}

	std::vector<std::pair<std::string, std::string>> argsIn;
	for(size_t d = 0; d < spec.dimArgs.size(); ++d)
		for(const auto& arg : spec.dimArgs[d])
		{
			bool clash = false;
			for(const auto& existing : argsIn)
				if(existing.first == arg.name)
				{
					clash = true;
					break;
				}
			if(clash)
				continue;  // lower dimension wins
			std::string value =
			    arg.type == MacroLoopSpec::Arg::LONG
			        ? std::to_string(arg.lInit + arg.lStep * (long)counters[d])
			        : arg.type == MacroLoopSpec::Arg::DOUBLE
			              ? std::to_string(arg.dInit + arg.dStep * (double)counters[d])
			              : arg.sVal;
			argsIn.emplace_back(arg.name, value);
		}
	return argsIn;
}  // end macroLoopIteration()

//==============================================================================
unsigned int Iterator::getStepIndexForLabel(IteratorWorkLoopStruct* iteratorStruct,
                                            const std::string&      label)
{
	if(iteratorStruct->stepIndexStack_.empty())
		return 0;

	// blank, or the table's unset-column sentinel, both mean the innermost open label
	if(label == "" || label == TableViewColumnInfo::DATATYPE_STRING_DEFAULT ||
	   label == TableViewColumnInfo::DATATYPE_STRING_ALT_DEFAULT)
		return iteratorStruct->stepIndexStack_.back();

	// stacks are parallel; search from the innermost outward
	for(size_t i = iteratorStruct->stepLabelStack_.size(); i-- > 0;)
		if(iteratorStruct->stepLabelStack_[i] == label &&
		   i < iteratorStruct->stepIndexStack_.size())
			return iteratorStruct->stepIndexStack_[i];

	__COUT_WARN__ << "Step label '" << label
	              << "' is not an open BEGIN_LABEL at this command (open labels: "
	              << StringMacros::vectorToString(iteratorStruct->stepLabelStack_)
	              << "). Using pass index 0." << __E__;
	return 0;
}  // end getStepIndexForLabel()

//==============================================================================
/// applyStepIndexToMacroArgs
///	inputArgs: "nIter,name:init:step,...;nIter2,..." (one ;-block per dimension)
///	labelsStr: ";"-separated StepLabel per dimension (may be shorter/empty)
///	Numeric inits become init + step*passIndex; string args (step DEFAULT) are unchanged.
std::string Iterator::applyStepIndexToMacroArgs(IteratorWorkLoopStruct* iteratorStruct,
                                                const std::string&      inputArgs,
                                                const std::string&      labelsStr)
{
	if(inputArgs == "")
		return inputArgs;

	std::vector<std::string> dimensions, labels;
	StringMacros::getVectorFromString(inputArgs, dimensions, {';'});
	StringMacros::getVectorFromString(labelsStr, labels, {';'});

	std::string out;
	for(size_t d = 0; d < dimensions.size(); ++d)
	{
		std::string  label     = d < labels.size() ? labels[d] : "";
		unsigned int stepIndex = getStepIndexForLabel(iteratorStruct, label);

		std::vector<std::string> args;
		StringMacros::getVectorFromString(dimensions[d], args, {','});

		if(d)
			out += ";";
		for(size_t a = 0; a < args.size(); ++a)
		{
			if(a)
				out += ",";
			if(a == 0)  // iteration count
			{
				out += args[0];
				continue;
			}

			std::vector<std::string> pieces(3);
			if(!StringMacros::splitMacroArgTriple(
			       args[a], pieces[0], pieces[1], pieces[2]))
			{
				out += args[a];  // leave malformed entries for the FE-side error
				continue;
			}

			const std::string& name = pieces[0];
			const std::string& init = pieces[1];
			const std::string& step = pieces[2];

			if(step == TableViewColumnInfo::DATATYPE_STRING_DEFAULT ||
			   step == TableViewColumnInfo::DATATYPE_STRING_ALT_DEFAULT)
			{
				out += args[a];  // constant string argument
				continue;
			}

			std::string newInit;
			if((init.size() &&
			    (init.back() == 'f' || init.find('.') != std::string::npos)) ||
			   (step.size() &&
			    (step.back() == 'f' || step.find('.') != std::string::npos)))
				newInit = std::to_string(strtod(init.c_str(), 0) +
				                         strtod(step.c_str(), 0) * stepIndex);
			else
			{
				long initValue = 0, stepValue = 0;
				StringMacros::getNumber(init, initValue);
				StringMacros::getNumber(step, stepValue);
				newInit = std::to_string(initValue + stepValue * (long)stepIndex);
			}

			__COUT_INFO__ << "Macro arg '" << name << "' pass " << stepIndex
			              << (label.size() ? (" of label '" + label + "'") : "") << ": "
			              << init << " + " << step << "*" << stepIndex << " = " << newInit
			              << __E__;

			out += name + ":" + newInit + ":" + step;
		}
	}
	return out;
}  // end applyStepIndexToMacroArgs()

//==============================================================================
/// feMacroArgBaseName
///	FE macro argument names may carry a mutable "(Default := x)" / "(Note)" suffix;
///	FEVInterfacesManager::runFEMacro ignores everything from the first '(' when
///	matching names, so remote matching does the same. Trailing whitespace is dropped.
std::string Iterator::feMacroArgBaseName(const std::string& argName)
{
	std::string base = argName.substr(0, argName.find('('));
	size_t      end  = base.find_last_not_of(" \t");
	return end == std::string::npos ? std::string() : base.substr(0, end + 1);
}  // end feMacroArgBaseName()

//==============================================================================
/// startRemoteCommandMacro
///	Runs an FE Macro (or MacroMaker Macro) on a remote subsystem by driving that
///	subsystem's MacroMaker UDP interface directly: one blocking RunFrontendMacro call per
///	iteration of the dimensional loop, with all target FEs passed as a CSV in each call.
///	The calls happen in a detached thread; checkRemoteCommandMacro() polls the shared state.
void Iterator::startRemoteCommandMacro(IteratorWorkLoopStruct* iteratorStruct,
                                       bool                    isFEMacro)
{
	auto&             command = iteratorStruct->commands_[iteratorStruct->commandIndex_];
	const std::string targetSubsystem = command.targetSubsystem_;
	const std::string macroName =
	    command.params_[IterateTable::commandExecuteMacroParams_.MacroName_];
	const bool saveOutputs =
	    command.params_[IterateTable::commandExecuteMacroParams_.EnableSavingOutput_] ==
	    "1";
	const std::string inputArgs = applyStepIndexToMacroArgs(
	    iteratorStruct,
	    command.params_[IterateTable::commandExecuteMacroParams_.MacroArgumentString_],
	    command.params_[IterateTable::commandExecuteMacroParams_.MacroArgumentLabels_]);

	__COUT__ << "startRemoteCommandMacro: '" << macroName << "' targeting '"
	         << targetSubsystem << "' isFEMacro=" << isFEMacro << __E__;
	__COUTV__(inputArgs);

	if(command.targets_.size() == 0)
	{
		__SS__ << "No target front-ends defined for remote macro '" << macroName
		       << "' on subsystem '" << targetSubsystem << "'" << __E__;
		__SS_THROW__;
	}

	GatewaySupervisor* gw = iteratorStruct->theIterator_->theSupervisor_;

	// resolve MacroMaker UDP address (throws with env-var / Configured guidance)
	std::string ipPort = gw->getRemoteMacroMakerUDPAddress(targetSubsystem);
	__COUTV__(ipPort);

	// discover live FEs and macros on the remote
	// Iterator is a friend of GatewaySupervisor, so the UDP bind address is reachable
	const std::string localIpAddress = gw->ipAddressForStateChangesOverUDP_;

	GatewaySupervisor::RemoteFEMacroInfo info =
	    GatewaySupervisor::parseFEMacroInfo(GatewaySupervisor::queryRemoteMacroMaker(
	        ipPort, "GetFrontendMacroInfo", 10 /*inactivity s*/, localIpAddress));
	__COUT__ << "Remote subsystem '" << targetSubsystem << "' has " << info.fes.size()
	         << " live front-end(s) and " << info.publicMacros.size()
	         << " public MacroMaker macro(s)." << __E__;

	// validate targets and macro; collect input/output names
	std::vector<std::string> inputNames, outputNames;
	std::string              uidCSV;
	for(size_t t = 0; t < command.targets_.size(); ++t)
	{
		const std::string& uid  = command.targets_[t].UID_;
		auto               feIt = info.fes.find(uid);
		if(feIt == info.fes.end())
		{
			__SS__ << "Front-end '" << uid << "' is not live on remote subsystem '"
			       << targetSubsystem
			       << "'. Is it enabled and is the subsystem Configured? "
			       << "Live front-ends: ";
			for(const auto& fe : info.fes)
				ss << fe.first << " ";
			ss << __E__;
			__SS_THROW__;
		}
		if(isFEMacro)
		{
			auto macroIt = feIt->second.macros.find(macroName);
			if(macroIt == feIt->second.macros.end())
			{
				__SS__ << "FE Macro '" << macroName << "' not found on front-end '" << uid
				       << "' of remote subsystem '" << targetSubsystem
				       << "'. Available: ";
				for(const auto& m : feIt->second.macros)
					ss << m.first << " ";
				ss << __E__;
				__SS_THROW__;
			}
			if(t == 0)
			{
				inputNames  = macroIt->second.inputs;
				outputNames = macroIt->second.outputs;
			}
			else
			{
				// one RunFrontendMacro call carries a single ordered input list for
				//	all target FEs, so every target must declare the same signature
				const auto& otherInputs = macroIt->second.inputs;
				bool        same        = otherInputs.size() == inputNames.size();
				for(size_t k = 0; same && k < inputNames.size(); ++k)
					same = feMacroArgBaseName(otherInputs[k]) ==
					       feMacroArgBaseName(inputNames[k]);
				if(!same)
				{
					__SS__ << "FE Macro '" << macroName << "' on front-end '" << uid
					       << "' of remote subsystem '" << targetSubsystem
					       << "' declares inputs ["
					       << StringMacros::vectorToString(otherInputs)
					       << "] which differ from front-end '"
					       << command.targets_[0].UID_ << "' inputs ["
					       << StringMacros::vectorToString(inputNames)
					       << "]. All targets of one remote macro command must share "
					          "the same input signature."
					       << __E__;
					__SS_THROW__;
				}
			}
		}
		if(t)
			uidCSV += ",";
		uidCSV += uid;
	}
	if(!isFEMacro)
	{
		auto macroIt = info.publicMacros.find(macroName);
		if(macroIt == info.publicMacros.end())
		{
			__SS__ << "MacroMaker macro '" << macroName
			       << "' not found among the PUBLIC macros of remote subsystem '"
			       << targetSubsystem
			       << "' (only public macros can be run remotely; make a private macro "
			          "public in MacroMaker first). Available: ";
			for(const auto& m : info.publicMacros)
				ss << m.first << " ";
			ss << __E__;
			__SS_THROW__;
		}
		inputNames  = macroIt->second.inputs;
		outputNames = macroIt->second.outputs;
	}

	// Parse the dimensional loop (iterations are computed one at a time in the worker,
	//	so a large product never has to fit in memory) and map the remote macro's
	//	declared inputs, in its order, onto the loop's argument names once.
	//	The remote (FEVInterfacesManager::runFEMacro) validates inputs positionally
	//	and ignores any "(Default/Note)" suffix, so match on the base name here too;
	//	this keeps saved plans working when a macro's default/note text changes.
	const MacroLoopSpec spec = parseMacroLoopSpec(inputArgs);
	std::vector<size_t> inputToArgIndex;  // inputNames[k] takes spec.argNames[inputToArgIndex[k]]
	{
		std::vector<bool> used(spec.argNames.size(), false);
		for(const auto& inputName : inputNames)
		{
			const std::string inputBase = feMacroArgBaseName(inputName);
			bool              bound     = false;
			for(size_t a = 0; a < spec.argNames.size(); ++a)
				if(!used[a] && feMacroArgBaseName(spec.argNames[a]) == inputBase)
				{
					inputToArgIndex.push_back(a);
					used[a] = true;
					bound   = true;
					break;
				}
			if(!bound)
			{
				__SS__ << "ArgIn '" << inputName
				       << "' was not assigned a value by any dimensional loop parameter "
				          "sets. This is illegal. Macro '"
				       << macroName << "' requires '" << inputName
				       << "' as an input argument. Either remove the input argument from "
				          "the macro, or define a value as a dimensional loop parameter."
				       << __E__;
				__SS_THROW__;
			}
		}
		for(size_t a = 0; a < spec.argNames.size(); ++a)
			if(!used[a])
				__COUT_WARN__ << "Dimensional loop parameter '" << spec.argNames[a]
				              << "' is not an input of macro '" << macroName
				              << "' on remote subsystem '" << targetSubsystem
				              << "'; it will not be sent." << __E__;
	}

	{
		std::stringstream hdr;
		hdr << "Remote macro '" << macroName << "' on subsystem '" << targetSubsystem
		    << "' targets [" << uidCSV << "]: " << spec.totalIterations
		    << " iteration(s) from loop spec '" << inputArgs << "'";
		if(saveOutputs)
			hdr << ". Output saving is enabled: the remote MacroMaker writes "
			       "macroOutput_<time>.txt under its OTSDAQ_DATA (OutputFilePath/Radix "
			       "are "
			       "not applied to remote targets)";
		__COUT_INFO__ << hdr.str() << __E__;
	}

	std::string outputCSV;
	for(size_t i = 0; i < outputNames.size(); ++i)
		outputCSV += (i ? "," : "") + StringMacros::encodeURIComponent(outputNames[i]);

	auto run             = std::make_shared<IteratorWorkLoopStruct::RemoteMacroRun>();
	run->iterationsTotal = spec.totalIterations;
	iteratorStruct->remoteMacroRun_ = run;

	// The thread captures only value copies and the shared run state: no Iterator,
	//	IteratorWorkLoopStruct or GatewaySupervisor pointer, so it can safely outlive
	//	all of them (it is detached and may block on the remote for up to the
	//	inactivity timeout). run->abort is checked inside every UDP receive poll.
	std::thread([run,
	             localIpAddress,
	             ipPort,
	             uidCSV,
	             macroName,
	             macroType = std::string(isFEMacro ? "fe" : "public"),
	             spec,
	             inputNames,
	             inputToArgIndex,
	             outputCSV,
	             saveOutputs,
	             targetSubsystem]() {
		try
		{
			const uint64_t total = spec.totalIterations;
			for(uint64_t i = 0; i < total; ++i)
			{
				if(run->abort)
				{
					__COUT_INFO__ << "Remote macro '" << macroName
					              << "' aborted before iteration " << i + 1 << " of " << total
					              << __E__;
					break;
				}

				// compute this iteration's values and emit them in the remote macro's
				//	declared input order, under the remote's current input names
				const auto  values = macroLoopIteration(spec, i);
				std::string inputStr;
				for(size_t k = 0; k < inputNames.size(); ++k)
					inputStr += (k ? ";" : "") + StringMacros::encodeURIComponent(inputNames[k]) +
					            "," +
					            StringMacros::encodeURIComponent(values[inputToArgIndex[k]].second);

				// RunFrontendMacro;feClass;feUIDs;macroType;macroName;inputArgs;outputArgs;saveOutputs
				std::string cmd = "RunFrontendMacro;*;" + uidCSV + ";" + macroType + ";" +
				                  StringMacros::encodeURIComponent(macroName) + ";" +
				                  StringMacros::encodeURIComponent(inputStr) + ";" +
				                  StringMacros::encodeURIComponent(outputCSV) + ";" +
				                  (saveOutputs ? "1" : "0");

				__COUT_INFO__ << "Remote macro '" << macroName << "' iteration " << i + 1
				              << " of " << total << " inputs: " << inputStr << __E__;

				std::string response = GatewaySupervisor::queryRemoteMacroMaker(
				    ipPort,
				    cmd,
				    30 /*inactivity s*/,
				    localIpAddress,
				    [&](int pct) {
					    std::lock_guard<std::mutex> lock(run->mutex);
					    run->progress = pct;
				    },
				    &run->abort);

				if(response.find("Error") == 0)
				{
					std::lock_guard<std::mutex> lock(run->mutex);
					run->error = "iteration " + std::to_string(i + 1) + " of " +
					             std::to_string(total) + ": " + response;
					break;
				}

				// log per-FE outputs
				size_t      after = 0;
				std::string feUid;
				while((feUid = StringMacros::extractXmlField(
				           response, "fe_uid", 0, after, &after)) != "")
				{
					after += strlen("fe_uid");
					size_t      argAfter = after;
					std::string outName, outValue, outputs;
					// outputArgs_name/value pairs follow fe_* fields within this feMacroExec
					size_t nextFe = response.find("<fe_uid", after);
					while((outName = StringMacros::extractXmlField(
					           response, "outputArgs_name", 0, argAfter, &argAfter)) !=
					          "" &&
					      (nextFe == std::string::npos || argAfter < nextFe))
					{
						argAfter += strlen("outputArgs_name");
						outValue = StringMacros::extractXmlField(
						    response, "outputArgs_value", 0, argAfter, &argAfter);
						argAfter += strlen("outputArgs_value");
						outputs += " " + outName + "=" +
						           StringMacros::decodeURIComponent(outValue);
					}
					__COUT_INFO__ << "Remote macro '" << macroName << "' iteration "
					              << i + 1 << " FE '" << feUid << "' outputs:" << outputs
					              << __E__;
				}

				std::lock_guard<std::mutex> lock(run->mutex);
				++run->iterationsDone;
				run->progress = 0;
			}
		}
		catch(const std::runtime_error& e)
		{
			if(run->abort)  // a Halt interrupted the in-flight UDP call: not an error
				__COUT_INFO__ << "Remote macro '" << macroName
				              << "' aborted mid-iteration: " << e.what() << __E__;
			else
			{
				std::lock_guard<std::mutex> lock(run->mutex);
				run->error = e.what();
			}
		}
		catch(...)
		{
			std::lock_guard<std::mutex> lock(run->mutex);
			run->error = "unknown error";
		}
		run->done = true;
	}).detach();

	__COUT__ << "startRemoteCommandMacro launched for '" << targetSubsystem << "'"
	         << __E__;
}  // end startRemoteCommandMacro()

//==============================================================================
/// checkRemoteCommandMacro
///	Returns true when the background remote macro thread has finished. Throws if it
///	recorded an error. On Halt, asks the thread to stop: the in-flight UDP wait is
///	interrupted promptly (the remote may still complete that iteration on its own).
bool Iterator::checkRemoteCommandMacro(IteratorWorkLoopStruct* iteratorStruct,
                                       bool /*isFEMacro*/)
{
	sleep(1);

	auto run = iteratorStruct->remoteMacroRun_;
	if(!run)
	{
		__SS__ << "No remote macro run in progress!?" << __E__;
		__SS_THROW__;
	}

	if(iteratorStruct->doHaltAction_ && !run->abort)
	{
		__COUT_INFO__ << "Halt requested: interrupting the remote macro run." << __E__;
		run->abort = true;
	}

	std::string error;
	uint64_t    itDone, itTotal;
	int         progress;
	{
		std::lock_guard<std::mutex> lock(run->mutex);
		error    = run->error;
		itDone   = run->iterationsDone;
		itTotal  = run->iterationsTotal;
		progress = run->progress;
	}

	if(error != "")
	{
		iteratorStruct->remoteMacroRun_.reset();
		auto& command = iteratorStruct->commands_[iteratorStruct->commandIndex_];
		__SS__ << "Remote macro '"
		       << command.params_[IterateTable::commandExecuteMacroParams_.MacroName_]
		       << "' on subsystem '" << command.targetSubsystem_ << "' failed: " << error
		       << __E__;
		__SS_THROW__;
	}

	__COUT__ << "Remote macro progress: " << itDone << " of " << itTotal
	         << " iteration(s) done, current iteration " << progress << "%" << __E__;

	if(run->done)
	{
		iteratorStruct->remoteMacroRun_.reset();
		__COUT__ << "checkRemoteCommandMacro complete." << __E__;
		return true;
	}
	return false;
}  // end checkRemoteCommandMacro()
