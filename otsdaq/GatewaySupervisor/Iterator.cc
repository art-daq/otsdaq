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


#define ITERATOR_PLAN_HISTORY_FILENAME                                                                                                                   \
	((getenv("SERVICE_DATA_PATH") == NULL) ? (std::string(__ENV__("USER_DATA")) + "/ServiceData") : (std::string(__ENV__("SERVICE_DATA_PATH")))) + \
	    "/IteratorPlanHistory.hist"

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
	__COUT__ << "Filename for iterator history: " << ITERATOR_PLAN_HISTORY_FILENAME << __E__;
	FILE* fp = fopen((ITERATOR_PLAN_HISTORY_FILENAME).c_str(), "r");
	if(fp)  // check for all core table names in file, and force their presence
	{
		char                      line[100];
		int i = 0;
		while(fgets(line, 100, fp))
		{
			if(strlen(line) < 3)
				continue;
			line[strlen(line) - 1] = '\0';                           // remove endline
			__COUTV__(line);
			if(i == 0)
				lastStartedPlanName_ = line;
			else if(i == 1)
			{
				lastFinishedPlanName_ = line;	
				break; //only 2 lines in file (for now)
			}
			++i;
		}
		fclose(fp);
		__COUTV__(lastStartedPlanName_);
		__COUTV__(lastFinishedPlanName_);
	} //end get iterator plan history
} //end constructor()

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

	ConfigurationManagerRW theConfigurationManager(WebUsers::DEFAULT_ITERATOR_USERNAME);  // this is a restricted username
	// theConfigurationManager.init();
	theConfigurationManager.getAllTableInfo(true /* refresh */,  // to prep all info
			0 /* accumulatedWarnings */,
			"" /* errorFilterName */,
			false /* getGroupKeys */,
			false /* getGroupInfo */,
			true /* initializeActiveGroups */);

	IteratorWorkLoopStruct theIteratorStruct(iterator, &theConfigurationManager);

	const IterateTable* itConfig;

	std::vector<IterateTable::Command> commands;

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
						__COUT__ << "New plan name encountered old=" << theIteratorStruct.activePlan_ << " vs new=" << iterator->activePlanName_ << __E__;
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
						__COUT_WARN__ << "Could not open Iterator history file: " << ITERATOR_PLAN_HISTORY_FILENAME << __E__;
					
					if(theIteratorStruct.commandIndex_ == (unsigned int)-1)
					{
						__COUT__ << "Starting plan '" << theIteratorStruct.activePlan_ << ".'" << __E__;
						__MOUT__ << "Starting plan '" << theIteratorStruct.activePlan_ << ".'" << __E__;
					}
					else
					{
						theIteratorStruct.doResumeAction_ = true;
						__COUT__ << "Continuing plan '" << theIteratorStruct.activePlan_ << "' at command index " << theIteratorStruct.commandIndex_ << ". "
						         << __E__;
						__MOUT__ << "Continuing plan '" << theIteratorStruct.activePlan_ << "' at command index " << theIteratorStruct.commandIndex_ << ". "
						         << __E__;
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

			if(iterator->activeCommandIndex_ !=  // update active command status if changed
			   theIteratorStruct.commandIndex_)
			{
				iterator->activeCommandIndex_     = theIteratorStruct.commandIndex_;
				iterator->activeCommandStartTime_ = time(0);  // reset on any change

				if(theIteratorStruct.commandIndex_ < theIteratorStruct.commandIterations_.size())
					iterator->activeCommandIteration_ = theIteratorStruct.commandIterations_[theIteratorStruct.commandIndex_];
				else
					iterator->activeCommandIteration_ = -1;

				iterator->depthIterationStack_.clear();
				for(const auto& depthIteration : theIteratorStruct.stepIndexStack_)
					iterator->depthIterationStack_.push_back(depthIteration);
				// if(theIteratorStruct.stepIndexStack_.size())
				//	iterator->activeLoopIteration_ =
				// theIteratorStruct.stepIndexStack_.back();  else
				//	iterator->activeLoopIteration_ = -1;
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

			__COUT__ << "Paused plan '" << theIteratorStruct.activePlan_ << "' at command index " << theIteratorStruct.commandIndex_ << ". " << __E__;
			__MOUT__ << "Paused plan '" << theIteratorStruct.activePlan_ << "' at command index " << theIteratorStruct.commandIndex_ << ". " << __E__;

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
			//			__MOUT__ << "Halted plan '" << theIteratorStruct.activePlan_ << "'
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
		if(theIteratorStruct.running_ && theIteratorStruct.activePlan_ != "")  // important, because after errors, still "running" until halt
		{
			if(theIteratorStruct.commandIndex_ == (unsigned int)-1)
			{
				// initialize the running plan

				__COUT__ << "Get commands" << __E__;

				theIteratorStruct.commandIndex_ = 0;

				theIteratorStruct.cfgMgr_->init();  // completely reset to re-align with any changes
				itConfig = theIteratorStruct.cfgMgr_->__GET_CONFIG__(IterateTable);

				theIteratorStruct.commands_ = itConfig->getPlanCommands(theIteratorStruct.cfgMgr_, theIteratorStruct.activePlan_);

				// reset commandIteration counts
				theIteratorStruct.commandIterations_.clear();
				for(auto& command : theIteratorStruct.commands_)
				{
					theIteratorStruct.commandIterations_.push_back(0);
					__COUT__ << "command " << command.type_ << __E__;
					__COUT__ << "table " << IterateTable::commandToTableMap_.at(command.type_) << __E__;
					__COUT__ << "param count = " << command.params_.size() << __E__;

					for(auto& param : command.params_)
					{
						__COUT__ << "\t param " << param.first << " : " << param.second << __E__;
					}
				}

				theIteratorStruct.originalTrackChanges_ = ConfigurationInterface::isVersionTrackingEnabled();
				theIteratorStruct.originalConfigGroup_  = theIteratorStruct.cfgMgr_->getActiveGroupName();
				theIteratorStruct.originalConfigKey_    = theIteratorStruct.cfgMgr_->getActiveGroupKey();

				__COUT__ << "originalTrackChanges " << theIteratorStruct.originalTrackChanges_ << __E__;
				__COUT__ << "originalConfigGroup " << theIteratorStruct.originalConfigGroup_ << __E__;
				__COUT__ << "originalConfigKey " << theIteratorStruct.originalConfigKey_ << __E__;

			}  // end initial section

			if(!theIteratorStruct.commandBusy_)
			{
				if(theIteratorStruct.commandIndex_ < theIteratorStruct.commands_.size())
				{
					// execute command
					theIteratorStruct.commandBusy_ = true;

					__COUT__ << "Iterator starting command " << theIteratorStruct.commandIndex_ + 1 << ": "
					         << theIteratorStruct.commands_[theIteratorStruct.commandIndex_].type_ << __E__;
					__MOUT__ << "Iterator starting command " << theIteratorStruct.commandIndex_ + 1 << ": "
					         << theIteratorStruct.commands_[theIteratorStruct.commandIndex_].type_ << __E__;

					iterator->startCommand(&theIteratorStruct);
				}
				else if(theIteratorStruct.commandIndex_ == theIteratorStruct.commands_.size())  // Done!
				{
					__COUT__ << "Finished Iteration Plan '" << theIteratorStruct.activePlan_ << __E__;
					__MOUT__ << "Finished Iteration Plan '" << theIteratorStruct.activePlan_ << __E__;

					__COUT__ << "Reverting track changes." << __E__;
					ConfigurationInterface::setVersionTrackingEnabled(theIteratorStruct.originalTrackChanges_);

					//l
					__COUT__ << "Activating original group..." << __E__;
					try
					{
						theIteratorStruct.cfgMgr_->activateTableGroup(theIteratorStruct.originalConfigGroup_, theIteratorStruct.originalConfigKey_);
					}
					catch(...)
					{
						__COUT_WARN__ << "Original group could not be activated." << __E__;
					}

					// leave FSM halted
					__COUT__ << "Completing Iteration Plan and cleaning up..." << __E__;

					iterator->haltIterator(iterator, &theIteratorStruct, true /* doNotHaltFSM */);
				}
			}
			else if(theIteratorStruct.commandBusy_)
			{
				// check for command completion
				if(iterator->checkCommand(&theIteratorStruct))
				{
					theIteratorStruct.commandBusy_ = false;  // command complete

					++theIteratorStruct.commandIndex_;

					__COUT__ << "Ready for next command. Done with " << theIteratorStruct.commandIndex_ << " of " << theIteratorStruct.commands_.size()
					         << __E__;
					__MOUT__ << "Iterator ready for next command. Done with " << theIteratorStruct.commandIndex_ << " of " << theIteratorStruct.commands_.size()
					         << __E__;
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
	}

	iterator->workloopRunning_ = false;  // if we ever exit
}
catch(const std::runtime_error& e)
{
	__SS__ << "Encountered error in Iterator thread:\n" << e.what() << __E__;
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
	try	{ throw; } //one more try to printout extra info
	catch(const std::exception &e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...){}
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
}  // end IteratorWorkLoop()

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
	std::lock_guard<std::mutex> lock(iteratorStruct->theIterator_->theSupervisor_->stateMachineAccessMutex_);
	if(iteratorStruct->theIterator_->theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Have FSM access" << __E__;

	// for out of range, throw exception - should never happen
	if(iteratorStruct->commandIndex_ >= iteratorStruct->commands_.size())
	{
		__SS__ << "Out of range commandIndex = " << iteratorStruct->commandIndex_ << " in size = " << iteratorStruct->commands_.size() << __E__;
		__SS_THROW__;
	}

	// increment iteration count for command
	++iteratorStruct->commandIterations_[iteratorStruct->commandIndex_];

	std::string type = iteratorStruct->commands_[iteratorStruct->commandIndex_].type_;
	if(type == IterateTable::COMMAND_BEGIN_LABEL)
	{
		return startCommandBeginLabel(iteratorStruct);
	}
	else if(type == IterateTable::COMMAND_CHOOSE_FSM)
	{
		return startCommandChooseFSM(iteratorStruct,
		                             iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandChooseFSMParams_.NameOfFSM_]);
	}
	else if(type == IterateTable::COMMAND_CONFIGURE_ACTIVE_GROUP)
	{
		return startCommandConfigureActive(iteratorStruct);
	}
	else if(type == IterateTable::COMMAND_CONFIGURE_ALIAS)
	{
		return startCommandConfigureAlias(
		    iteratorStruct, iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandConfigureAliasParams_.SystemAlias_]);
	}
	else if(type == IterateTable::COMMAND_CONFIGURE_GROUP)
	{
		return startCommandConfigureGroup(iteratorStruct);
	}
	else if(type == IterateTable::COMMAND_ACTIVATE_ALIAS)
	{
		ConfigurationManagerRW* cfgMgr = iteratorStruct->cfgMgr_;
		std::pair<std::string, TableGroupKey> newActiveGroup = cfgMgr->getTableGroupFromAlias(
			iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandActivateAliasParams_.SystemAlias_]);
		cfgMgr->loadTableGroup(newActiveGroup.first, newActiveGroup.second, true /*activate*/);
	}
	else if(type == IterateTable::COMMAND_ACTIVATE_GROUP)
	{
		ConfigurationManagerRW* cfgMgr = iteratorStruct->cfgMgr_;
		cfgMgr->loadTableGroup( 
			iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandActivateGroupParams_.GroupName_],
			TableGroupKey(iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandActivateGroupParams_.GroupKey_]), 
			true /*activate*/);
	}
	else if(type == IterateTable::COMMAND_EXECUTE_FE_MACRO)
	{
		return startCommandMacro(iteratorStruct, true /*isFEMacro*/);
	}
	else if(type == IterateTable::COMMAND_EXECUTE_MACRO)
	{
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
	else if(type == IterateTable::COMMAND_START)
	{
		return startCommandFSMTransition(iteratorStruct, "Start");
	}
	else if(type == IterateTable::COMMAND_STOP)
	{
		return startCommandFSMTransition(iteratorStruct, "Stop");
	}
	else if(type == IterateTable::COMMAND_PAUSE)
	{
		return startCommandFSMTransition(iteratorStruct, "Pause");
	}
	else if(type == IterateTable::COMMAND_RESUME)
	{
		return startCommandFSMTransition(iteratorStruct, "Resume");
	}
	else if(type == IterateTable::COMMAND_HALT)
	{
		return startCommandFSMTransition(iteratorStruct, "Halt");
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
	ConfigurationInterface::setVersionTrackingEnabled(iteratorStruct->originalTrackChanges_);

	__COUT__ << "Activating original group..." << __E__;
	try
	{
		iteratorStruct->cfgMgr_->activateTableGroup(iteratorStruct->originalConfigGroup_, iteratorStruct->originalConfigKey_);
	}
	catch(...)
	{
		__COUT_WARN__ << "Original group could not be activated." << __E__;
	}
	throw;
}  // end startCommand()

//==============================================================================
// checkCommand
//	when busy for a while, start to sleep
//		use sleep() or nanosleep()
bool Iterator::checkCommand(IteratorWorkLoopStruct* iteratorStruct)
try
{
	// for out of range, return done
	if(iteratorStruct->commandIndex_ >= iteratorStruct->commands_.size())
	{
		__COUT__ << "Out of range commandIndex = " << iteratorStruct->commandIndex_ << " in size = " << iteratorStruct->commands_.size() << __E__;
		return true;
	}

	std::string type = iteratorStruct->commands_[iteratorStruct->commandIndex_].type_;
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
	else if(type == IterateTable::COMMAND_CONFIGURE_ALIAS || type == IterateTable::COMMAND_CONFIGURE_ACTIVE_GROUP ||
	        type == IterateTable::COMMAND_CONFIGURE_GROUP)
	{
		return checkCommandConfigure(iteratorStruct);
	}
	else if(type == IterateTable::COMMAND_ACTIVATE_ALIAS || type == IterateTable::COMMAND_ACTIVATE_GROUP)
	{
		// do nothing
		return true;
	}
	else if(type == IterateTable::COMMAND_EXECUTE_FE_MACRO)
	{
		return checkCommandMacro(iteratorStruct, true /*isFEMacro*/);
	}
	else if(type == IterateTable::COMMAND_EXECUTE_MACRO)
	{
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
	else if(type == IterateTable::COMMAND_START)
	{
		return checkCommandFSMTransition(iteratorStruct, "Running");
	}
	else if(type == IterateTable::COMMAND_STOP)
	{
		return checkCommandFSMTransition(iteratorStruct, "Configured");
	}
	else if(type == IterateTable::COMMAND_PAUSE)
	{
		return checkCommandFSMTransition(iteratorStruct, "Paused");
	}
	else if(type == IterateTable::COMMAND_RESUME)
	{
		return checkCommandFSMTransition(iteratorStruct, "Running");
	}
	else if(type == IterateTable::COMMAND_HALT)
	{
		return checkCommandFSMTransition(iteratorStruct, RunControlStateMachine::HALTED_STATE_NAME);
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
	ConfigurationInterface::setVersionTrackingEnabled(iteratorStruct->originalTrackChanges_);

	__COUT__ << "Activating original group..." << __E__;
	try
	{
		iteratorStruct->cfgMgr_->activateTableGroup(iteratorStruct->originalConfigGroup_, iteratorStruct->originalConfigKey_);
	}
	catch(...)
	{
		__COUT_WARN__ << "Original group could not be activated." << __E__;
	}

	throw;
}  // end checkCommand()

//==============================================================================
void Iterator::startCommandChooseFSM(IteratorWorkLoopStruct* iteratorStruct, const std::string& fsmName)
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
	ConfigurationTree configLinkNode = iteratorStruct->cfgMgr_->getSupervisorTableNode(iteratorStruct->theIterator_->theSupervisor_->getContextUID(),
	                                                                                   iteratorStruct->theIterator_->theSupervisor_->getSupervisorUID());

	if(!configLinkNode.isDisconnected())
	{
		try  // for backwards compatibility
		{
			ConfigurationTree fsmLinkNode = configLinkNode.getNode("LinkToStateMachineTable");
			if(!fsmLinkNode.isDisconnected())
				iteratorStruct->fsmRunAlias_ = fsmLinkNode.getNode(fsmName + "/RunDisplayAlias").getValue<std::string>();
			else
				__COUT_INFO__ << "FSM Link disconnected." << __E__;
		}
		catch(std::runtime_error& e)
		{
			//__COUT_INFO__ << e.what() << __E__;
			__COUT_INFO__ << "No state machine Run alias. Ignoring and assuming alias of '" << iteratorStruct->fsmRunAlias_ << ".'" << __E__;
		}
		catch(...)
		{
			__COUT_ERR__ << "Unknown error. Should never happen." << __E__;

			__COUT_INFO__ << "No state machine Run alias. Ignoring and assuming alias of '" << iteratorStruct->fsmRunAlias_ << ".'" << __E__;
		}
	}
	else
		__COUT_INFO__ << "FSM Link disconnected." << __E__;

	__COUT__ << "fsmRunAlias_  = " << iteratorStruct->fsmRunAlias_ << __E__;

	//// ======================== get run number based on fsm name ====

	iteratorStruct->fsmNextRunNumber_ = iteratorStruct->theIterator_->theSupervisor_->getNextRunNumber(iteratorStruct->fsmName_);

	if(iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.getCurrentStateName() == "Running" ||
	   iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.getCurrentStateName() == "Paused")
		--iteratorStruct->fsmNextRunNumber_;  // current run number is one back

	__COUT__ << "fsmNextRunNumber_  = " << iteratorStruct->fsmNextRunNumber_ << __E__;
}  // end startCommandChooseFSM()

//==============================================================================
// return true if an action was attempted
bool Iterator::haltIterator(Iterator* iterator, IteratorWorkLoopStruct* iteratorStruct /* = 0 */,
	bool doNotHaltFSM /* = false */)

{
	GatewaySupervisor* theSupervisor = iterator->theSupervisor_;
	const std::string& fsmName       = iterator->lastFsmName_;

	std::vector<std::string> fsmCommandParameters;
	std::string              errorStr     = "";
	std::string              currentState = theSupervisor->theStateMachine_.getCurrentStateName();

	bool haltAttempted = true;	
	if(doNotHaltFSM)
	{
		__COUT_INFO__ << "Iterator is leaving FSM in current state: " << currentState << 
			". If this is undesireable, add a Halt command, for example, to the end of your Iteration plan." << __E__;
		haltAttempted = false;
	}
	else if(currentState == "Initialized" || currentState == "Halted")
	{
		__COUT__ << "Do nothing. Already halted." << __E__;
		haltAttempted = false;
	}
	else if(currentState == "Running")
		errorStr = theSupervisor->attemptStateMachineTransition(
		    0, 0, "Abort", fsmName, WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/, WebUsers::DEFAULT_ITERATOR_USERNAME, fsmCommandParameters);
	else
		errorStr = theSupervisor->attemptStateMachineTransition(
		    0, 0, "Halt", fsmName, WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/, WebUsers::DEFAULT_ITERATOR_USERNAME, fsmCommandParameters);

	if(haltAttempted)
	{
		if(errorStr != "")
		{
			__SS__ << "Iterator failed to halt because of the following error: " << errorStr;
			__SS_THROW__;
		}

		// else successfully launched
		__COUT__ << "FSM in transition = " << theSupervisor->theStateMachine_.isInTransition() << __E__;
		__COUT__ << "halting state machine launched." << __E__;
	}

	// finish up cleanup of the iterator
	__COUT__ << "Conducting Iterator cleanup." << __E__;

	if(iteratorStruct)
	{
		__COUT__ << "Reverting track changes." << __E__;
		ConfigurationInterface::setVersionTrackingEnabled(iteratorStruct->originalTrackChanges_);

		if(!doNotHaltFSM)
		{
			__COUT__ << "Activating original group..." << __E__;
			try
			{
				iteratorStruct->cfgMgr_->activateTableGroup(iteratorStruct->originalConfigGroup_, iteratorStruct->originalConfigKey_);
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
		__COUT__ << "Iterator cleanup complete of plan '" << iteratorStruct->activePlan_ << "' at command index " << iteratorStruct->commandIndex_ << ". " << __E__;
		
		iterator->lastFinishedPlanName_ = iteratorStruct->activePlan_;
		FILE* fp = fopen((ITERATOR_PLAN_HISTORY_FILENAME).c_str(), "w");
		if(fp)
		{
			fprintf(fp, "%s\n", iterator->lastStartedPlanName_.c_str());
			fprintf(fp, "%s\n", iterator->lastFinishedPlanName_.c_str());
			fclose(fp);
		}
		else
			__COUT_WARN__ << "Could not open Iterator history file: " << ITERATOR_PLAN_HISTORY_FILENAME << __E__;

		iteratorStruct->activePlan_    = "";  // clear
		iteratorStruct->commandIndex_  = -1;  // clear
	}

	return haltAttempted;
}  // end haltIterator()

//==============================================================================
void Iterator::startCommandBeginLabel(IteratorWorkLoopStruct* iteratorStruct)
{
	__COUT__ << "Entering label '" << iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandBeginLabelParams_.Label_] << "'..."
	         << std::endl;

	// add new step index to stack
	iteratorStruct->stepIndexStack_.push_back(0);
}  // end startCommandBeginLabel()

//==============================================================================
void Iterator::startCommandRepeatLabel(IteratorWorkLoopStruct* iteratorStruct)
{
	// search for first matching label backward and set command to there

	int numOfRepetitions;
	sscanf(iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandRepeatLabelParams_.NumberOfRepetitions_].c_str(),
	       "%d",
	       &numOfRepetitions);
	__COUT__ << "numOfRepetitions remaining = " << numOfRepetitions << __E__;

	char repStr[200];

	if(numOfRepetitions <= 0)
	{
		// write original number of repetitions value back
		sprintf(repStr, "%d", iteratorStruct->stepIndexStack_.back());
		iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandRepeatLabelParams_.NumberOfRepetitions_] =
		    repStr;  // re-store as string

		// remove step index from stack
		iteratorStruct->stepIndexStack_.pop_back();

		return;  // no more repetitions
	}

	--numOfRepetitions;

	// increment step index in stack
	++(iteratorStruct->stepIndexStack_.back());

	unsigned int i;
	for(i = iteratorStruct->commandIndex_; i > 0; --i)  // assume 0 is always the fallback option
		if(iteratorStruct->commands_[i].type_ == IterateTable::COMMAND_BEGIN_LABEL &&
		   iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandRepeatLabelParams_.Label_] ==
		       iteratorStruct->commands_[i].params_[IterateTable::commandBeginLabelParams_.Label_])
			break;

	sprintf(repStr, "%d", numOfRepetitions);
	iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandRepeatLabelParams_.NumberOfRepetitions_] =
	    repStr;  // re-store as string

	iteratorStruct->commandIndex_ = i;
	__COUT__ << "Jumping back to commandIndex " << iteratorStruct->commandIndex_ << __E__;
}  // end startCommandRepeatLabel()

//==============================================================================
void Iterator::startCommandRun(IteratorWorkLoopStruct* iteratorStruct)
{
	iteratorStruct->runIsDone_ = false;
	iteratorStruct->fsmCommandParameters_.clear();

	std::string errorStr     = "";
	std::string currentState = iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.getCurrentStateName();

	// execute first transition (may need two)

	if(currentState == "Configured")
		errorStr = iteratorStruct->theIterator_->theSupervisor_->attemptStateMachineTransition(0,
		                                                                                       0,
		                                                                                       "Start",
		                                                                                       iteratorStruct->fsmName_,
		                                                                                       WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
		                                                                                       WebUsers::DEFAULT_ITERATOR_USERNAME,
		                                                                                       iteratorStruct->fsmCommandParameters_);
	else
		errorStr = "Can only Run from the Configured state. The current state is " + currentState;

	if(errorStr != "")
	{
		__SS__ << "Iterator failed to run because of the following error: " << errorStr;
		__SS_THROW__;
	}

	// save original duration
	sscanf(iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandRunParams_.DurationInSeconds_].c_str(),
	       "%ld",
	       &iteratorStruct->originalDurationInSeconds_);
	__COUTV__(iteratorStruct->originalDurationInSeconds_);

	// else successfully launched
	__COUT__ << "FSM in transition = " << iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.isInTransition() << __E__;
	__COUT__ << "startCommandRun success." << __E__;
}  // end startCommandRun()

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

	std::string   group = iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandConfigureGroupParams_.GroupName_];
	TableGroupKey key   = TableGroupKey(iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandConfigureGroupParams_.GroupKey_]);

	__COUT__ << "group " << group << __E__;
	__COUT__ << "key " << key << __E__;

	// create special alias for this group using : separators

	std::stringstream systemAlias;
	systemAlias << "GROUP:" << group << ":" << key;
	startCommandConfigureAlias(iteratorStruct, systemAlias.str());
}  // end startCommandConfigureGroup()

//==============================================================================
void Iterator::startCommandConfigureAlias(IteratorWorkLoopStruct* iteratorStruct, const std::string& systemAlias)
{
	__COUT__ << "systemAlias " << systemAlias << __E__;

	iteratorStruct->fsmCommandParameters_.clear();
	iteratorStruct->fsmCommandParameters_.push_back(systemAlias);

	std::string errorStr     = "";
	std::string currentState = iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.getCurrentStateName();

	// execute first transition (may need two in conjunction with checkCommandConfigure())

	if(currentState == "Initial")
		errorStr = iteratorStruct->theIterator_->theSupervisor_->attemptStateMachineTransition(0,
		                                                                                       0,
		                                                                                       "Initialize",
		                                                                                       iteratorStruct->fsmName_,
		                                                                                       WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
		                                                                                       WebUsers::DEFAULT_ITERATOR_USERNAME,
		                                                                                       iteratorStruct->fsmCommandParameters_);
	else if(currentState == "Halted")
		errorStr = iteratorStruct->theIterator_->theSupervisor_->attemptStateMachineTransition(0,
		                                                                                       0,
		                                                                                       "Configure",
		                                                                                       iteratorStruct->fsmName_,
		                                                                                       WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
		                                                                                       WebUsers::DEFAULT_ITERATOR_USERNAME,
		                                                                                       iteratorStruct->fsmCommandParameters_);
	else if(currentState == "Configured")
		errorStr = iteratorStruct->theIterator_->theSupervisor_->attemptStateMachineTransition(0,
		                                                                                       0,
		                                                                                       "Halt",
		                                                                                       iteratorStruct->fsmName_,
		                                                                                       WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
		                                                                                       WebUsers::DEFAULT_ITERATOR_USERNAME,
		                                                                                       iteratorStruct->fsmCommandParameters_);
	else
		errorStr = "Can only Configure from the Initial or Halted state. The current state is " + currentState;

	if(errorStr != "")
	{
		__SS__ << "Iterator failed to configure with system alias '"
		       << (iteratorStruct->fsmCommandParameters_.size() ? iteratorStruct->fsmCommandParameters_[0] : "UNKNOWN")
		       << "' because of the following error: " << errorStr;
		__SS_THROW__;
	}

	// else successfully launched
	__COUT__ << "FSM in transition = " << iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.isInTransition() << __E__;
	__COUT__ << "startCommandConfigureAlias success." << __E__;
}  // end startCommandConfigureAlias()

//==============================================================================
void Iterator::startCommandFSMTransition(IteratorWorkLoopStruct* iteratorStruct, const std::string& transitionCommand)
{
	__COUTV__(transitionCommand);

	iteratorStruct->fsmCommandParameters_.clear();

	std::string errorStr     = "";
	std::string currentState = iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.getCurrentStateName();

	// execute first transition (may need two in conjunction with checkCommandConfigure())

	__COUTV__(currentState);

	errorStr = iteratorStruct->theIterator_->theSupervisor_->attemptStateMachineTransition(0,
	                                                                                       0,
	                                                                                       transitionCommand,
	                                                                                       iteratorStruct->fsmName_,
	                                                                                       WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
	                                                                                       WebUsers::DEFAULT_ITERATOR_USERNAME,
	                                                                                       iteratorStruct->fsmCommandParameters_);

	if(errorStr != "")
	{
		__SS__ << "Iterator failed to configure with system alias '"
		       << (iteratorStruct->fsmCommandParameters_.size() ? iteratorStruct->fsmCommandParameters_[0] : "UNKNOWN")
		       << "' because of the following error: " << errorStr;
		__SS_THROW__;
	}

	// else successfully launched
	__COUT__ << "FSM in transition = " << iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.isInTransition() << __E__;
	__COUT__ << "startCommandFSMTransition success." << __E__;
}  // end startCommandFSMTransition()

//==============================================================================
void Iterator::startCommandMacro(IteratorWorkLoopStruct* iteratorStruct, bool isFrontEndMacro)
{
	// Steps:
	//	4 parameters  CommandExecuteFEMacroParams:
	//		//targets
	//		const std::string FEMacroName_ 				= "FEMacroName";
	//		//macro parameters (table/groupID)

	const std::string& macroName = iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandExecuteMacroParams_.MacroName_];
	const std::string& enableSavingOutput =
	    iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandExecuteMacroParams_.EnableSavingOutput_];
	const std::string& outputFilePath =
	    iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandExecuteMacroParams_.OutputFilePath_];
	const std::string& outputFileRadix =
	    iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandExecuteMacroParams_.OutputFileRadix_];
	const std::string& inputArgs =
	    iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandExecuteMacroParams_.MacroArgumentString_];

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
	for(const auto& target : iteratorStruct->commands_[iteratorStruct->commandIndex_].targets_)
	{
		__COUT__ << "target " << target.table_ << ":" << target.UID_ << __E__;

		// for each target, init to not done
		iteratorStruct->targetsDone_.push_back(false);

		xoap::MessageReference message = SOAPUtilities::makeSOAPMessageReference("FECommunication");

		SOAPParameters parameters;
		std::string    type = isFrontEndMacro ? "feMacroMultiDimensionalStart" : "macroMultiDimensionalStart";
		parameters.addParameter("type", type);
		parameters.addParameter("requester", WebUsers::DEFAULT_ITERATOR_USERNAME);
		parameters.addParameter("targetInterfaceID", target.UID_);
		parameters.addParameter(isFrontEndMacro ? "feMacroName" : "macroName", macroName);
		parameters.addParameter("enableSavingOutput", enableSavingOutput);
		parameters.addParameter("outputFilePath", outputFilePath);
		parameters.addParameter("outputFileRadix", outputFileRadix);
		parameters.addParameter("inputArgs", inputArgs);
		SOAPUtilities::addParameters(message, parameters);

		__COUT__ << "Sending FE communication: " << SOAPUtilities::translate(message) << __E__;

		xoap::MessageReference replyMessage = iteratorStruct->theIterator_->theSupervisor_->SOAPMessenger::sendWithSOAPReply(
		    iteratorStruct->theIterator_->theSupervisor_->allSupervisorInfo_.getAllMacroMakerTypeSupervisorInfo().begin()->second.getDescriptor(), message);

		__COUT__ << "Response received: " << SOAPUtilities::translate(replyMessage) << __E__;

		SOAPParameters rxParameters;
		rxParameters.addParameter("Error");
		std::string response = SOAPUtilities::receive(replyMessage, rxParameters);

		std::string error = rxParameters.getValue("Error");

		if(response != type + "Done" || error != "")
		{
			// error occurred!
			__SS__ << "Error transmitting request to target interface '" << target.UID_ << "' from '" << WebUsers::DEFAULT_ITERATOR_USERNAME << ".' Response '"
			       << response << "' with error: " << error << __E__;
			__SS_THROW__;
		}
	}  // end target loop

}  // end startCommandMacro()

//==============================================================================
bool Iterator::checkCommandMacro(IteratorWorkLoopStruct* iteratorStruct, bool isFrontEndMacro)
{
	sleep(1);

	// Steps:
	//	4 parameters  CommandExecuteFEMacroParams:
	//		//targets
	//		const std::string FEMacroName_ 				= "FEMacroName";
	//		//macro parameters (table/groupID)

	const std::string& macroName = iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandExecuteMacroParams_.MacroName_];

	__COUTV__(macroName);

	// send request to MacroMaker to check completion of macro
	// as targets are identified complete, remove targets_ from vector

	for(unsigned int i = 0; i < iteratorStruct->commands_[iteratorStruct->commandIndex_].targets_.size(); ++i)
	{
		ots::IterateTable::CommandTarget& target = iteratorStruct->commands_[iteratorStruct->commandIndex_].targets_[i];

		__COUT__ << "target " << target.table_ << ":" << target.UID_ << __E__;

		xoap::MessageReference message = SOAPUtilities::makeSOAPMessageReference("FECommunication");

		SOAPParameters parameters;
		std::string    type = isFrontEndMacro ? "feMacroMultiDimensionalCheck" : "macroMultiDimensionalCheck";
		parameters.addParameter("type", type);
		parameters.addParameter("requester", WebUsers::DEFAULT_ITERATOR_USERNAME);
		parameters.addParameter("targetInterfaceID", target.UID_);
		parameters.addParameter(isFrontEndMacro ? "feMacroName" : "macroName", macroName);
		SOAPUtilities::addParameters(message, parameters);

		__COUT__ << "Sending FE communication: " << SOAPUtilities::translate(message) << __E__;

		xoap::MessageReference replyMessage = iteratorStruct->theIterator_->theSupervisor_->SOAPMessenger::sendWithSOAPReply(
		    iteratorStruct->theIterator_->theSupervisor_->allSupervisorInfo_.getAllMacroMakerTypeSupervisorInfo().begin()->second.getDescriptor(), message);

		__COUT__ << "Response received: " << SOAPUtilities::translate(replyMessage) << __E__;

		SOAPParameters rxParameters;
		rxParameters.addParameter("Error");
		rxParameters.addParameter("Done");
		std::string response = SOAPUtilities::receive(replyMessage, rxParameters);

		std::string error = rxParameters.getValue("Error");
		bool        done  = rxParameters.getValue("Done") == "1";

		if(response != type + "Done" || error != "")
		{
			// error occurred!
			__SS__ << "Error transmitting request to target interface '" << target.UID_ << "' from '" << WebUsers::DEFAULT_ITERATOR_USERNAME << ".' Response '"
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
	if("True" == iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandModifyActiveParams_.DoTrackGroupChanges_])
		doTrackGroupChanges = true;

	const std::string& startValueStr =
	    iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandModifyActiveParams_.FieldStartValue_];
	const std::string& stepSizeStr =
	    iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandModifyActiveParams_.FieldIterationStepSize_];

	const unsigned int stepIndex = iteratorStruct->stepIndexStack_.back();

	__COUT__ << "doTrackGroupChanges " << (doTrackGroupChanges ? "yes" : "no") << std::endl;
	__COUTV__(startValueStr);
	__COUTV__(stepSizeStr);
	__COUTV__(stepIndex);	

	ConfigurationInterface::setVersionTrackingEnabled(doTrackGroupChanges);

	// two approaches: double or long handling
	// OR TODO -- if step is 0 and startValue is NaN.. then handle as string

	if(((stepSizeStr.size() && stepSizeStr[0] == '0') || !stepSizeStr.size()) && 
		!StringMacros::isNumber(startValueStr)) //if not a number and no step size, then interpret as string
	{
		__COUT__ << "Treating start value as string: " << startValueStr << __E__;
		helpCommandModifyActive(iteratorStruct, startValueStr, doTrackGroupChanges);
	}
	else if(startValueStr.size() && (startValueStr[startValueStr.size() - 1] == 'f' || startValueStr.find('.') != std::string::npos))
	{
		// handle as double
		double startValue = strtod(startValueStr.c_str(), 0);
		double stepSize   = strtod(stepSizeStr.c_str(), 0);

		__COUT__ << "startValue " << startValue << std::endl;
		__COUT__ << "stepSize " << stepSize << std::endl;
		__COUT__ << "currentValue " << startValue + stepSize * stepIndex << std::endl;

		helpCommandModifyActive(iteratorStruct, startValue + stepSize * stepIndex, doTrackGroupChanges);
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

		helpCommandModifyActive(iteratorStruct, startValue + stepSize * stepIndex, doTrackGroupChanges);
	}

}  // end startCommandModifyActive()

//==============================================================================
// checkCommandRun
//	return true if done
//
//	Either will be done on (priority 1) running threads (for Frontends) ending
//		or (priority 2 and ignored if <= 0) duration timeout
//
//	Note: use command structure strings to maintain duration left
//	Note: watch iterator->doPauseAction and iterator->doHaltAction and respond
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
	std::lock_guard<std::mutex> lock(iteratorStruct->theIterator_->theSupervisor_->stateMachineAccessMutex_);
	if(iteratorStruct->theIterator_->theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Have FSM access" << __E__;

	if(iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.isInTransition())
		return false;

	iteratorStruct->fsmCommandParameters_.clear();

	std::string errorStr     = "";
	std::string currentState = iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.getCurrentStateName();

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
			errorStr = iteratorStruct->theIterator_->theSupervisor_->attemptStateMachineTransition(0,
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
			errorStr = "Expected to be in Paused. Unexpectedly, the current state is " + currentState +
			           ". Last State Machine error message was as follows: " + iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.getErrorMessage();

		if(errorStr != "")
		{
			__SS__ << "Iterator failed to pause because of the following error: " << errorStr;
			__SS_THROW__;
		}
		return false;
	}
	else if(iteratorStruct->doHaltAction_)
	{
		// transition to halted state
		__COUT__ << "Transitioning FSM to Halted..." << __E__;

		if(currentState == "Halted")
		{
			// done with early halt exit!
			__COUT__ << "Transition to Halted complete." << __E__;
			return true;
		}
		else if(currentState == "Running" ||  // launch transition to halt
		        currentState == "Paused")
			errorStr = iteratorStruct->theIterator_->theSupervisor_->attemptStateMachineTransition(0,
			                                                                                       0,
			                                                                                       "Abort",
			                                                                                       iteratorStruct->fsmName_,
			                                                                                       WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
			                                                                                       WebUsers::DEFAULT_ITERATOR_USERNAME,
			                                                                                       iteratorStruct->fsmCommandParameters_);
		else if(currentState == "Configured")  // launch transition to halt
			errorStr = iteratorStruct->theIterator_->theSupervisor_->attemptStateMachineTransition(0,
			                                                                                       0,
			                                                                                       "Halt",
			                                                                                       iteratorStruct->fsmName_,
			                                                                                       WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
			                                                                                       WebUsers::DEFAULT_ITERATOR_USERNAME,
			                                                                                       iteratorStruct->fsmCommandParameters_);
		else
			errorStr = "Expected to be in Halted. Unexpectedly, the current state is " + currentState +
			           ". Last State Machine error message was as follows: " + iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.getErrorMessage();

		if(errorStr != "")
		{
			__SS__ << "Iterator failed to halt because of the following error: " << errorStr;
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
			errorStr = iteratorStruct->theIterator_->theSupervisor_->attemptStateMachineTransition(0,
			                                                                                       0,
			                                                                                       "Resume",
			                                                                                       iteratorStruct->fsmName_,
			                                                                                       WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
			                                                                                       WebUsers::DEFAULT_ITERATOR_USERNAME,
			                                                                                       iteratorStruct->fsmCommandParameters_);

		if(errorStr != "")
		{
			__SS__ << "Iterator failed to run because of the following error: " << errorStr;
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
			__COUT__ << "Reached end of run " << iteratorStruct->fsmNextRunNumber_ << __E__;
			return true;
		}

		errorStr = "Expected to be in Running. Unexpectedly, the current state is " + currentState +
		           ". Last State Machine error message was as follows: " + iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.getErrorMessage();
	}
	else  // else in Running state! Check for end of run
	{
		bool waitOnRunningThreads = false;
		if("True" == iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandRunParams_.WaitOnRunningThreads_])
			waitOnRunningThreads = true;

		time_t remainingDurationInSeconds;
		sscanf(iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandRunParams_.DurationInSeconds_].c_str(),
		       "%ld",
		       &remainingDurationInSeconds);

		__COUT__ << "waitOnRunningThreads " << waitOnRunningThreads << __E__;
		__COUT__ << "remainingDurationInSeconds " << remainingDurationInSeconds << __E__;

		///////////////////
		// priority 1 is waiting on running threads
		if(waitOnRunningThreads)
		{
			//	get status of all running FE workloops
			GatewaySupervisor* theSupervisor = iteratorStruct->theIterator_->theSupervisor_;

			bool allFrontEndsAreDone = true;
			for(auto& it : theSupervisor->allSupervisorInfo_.getAllFETypeSupervisorInfo())
			{
				try
				{
					std::string status = theSupervisor->send(it.second.getDescriptor(), "WorkLoopStatusRequest");

					__COUT__ << "FESupervisor instance " << it.first << " has status = " << status << std::endl;

					if(status != CoreSupervisorBase::WORK_LOOP_DONE)
					{
						allFrontEndsAreDone = false;
						break;
					}
				}
				catch(xdaq::exception::Exception& e)
				{
					__SS__ << "Could not retrieve status from FESupervisor instance " << it.first << "." << std::endl;
					__COUT_WARN__ << ss.str();
					errorStr = ss.str();

					if(errorStr != "")
					{
						__SS__ << "Iterator failed to run because of the following error: " << errorStr;
						__SS_THROW__;
					}
				}
			}

			if(allFrontEndsAreDone)
			{
				// need to end run!
				__COUT__ << "FE workloops all complete! Stopping run..." << __E__;

				errorStr = iteratorStruct->theIterator_->theSupervisor_->attemptStateMachineTransition(0,
				                                                                                       0,
				                                                                                       "Stop",
				                                                                                       iteratorStruct->fsmName_,
				                                                                                       WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
				                                                                                       WebUsers::DEFAULT_ITERATOR_USERNAME,
				                                                                                       iteratorStruct->fsmCommandParameters_);

				if(errorStr != "")
				{
					__SS__ << "Iterator failed to stop run because of the following error: " << errorStr;
					__SS_THROW__;
				}

				// write indication of run done into duration
				iteratorStruct->runIsDone_ = true;

				return false;
			}
		}

		///////////////////
		// priority 2 is duration, if <= 0 it is ignored
		if(remainingDurationInSeconds > 1)
		{
			--remainingDurationInSeconds;

			// write back to string
			char str[200];
			sprintf(str, "%ld", remainingDurationInSeconds);
			iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandRunParams_.DurationInSeconds_] = str;  // re-store as string
		}
		else if(remainingDurationInSeconds == 1)
		{
			// need to end run!
			__COUT__ << "Time duration reached! Stopping run..." << __E__;

			errorStr = iteratorStruct->theIterator_->theSupervisor_->attemptStateMachineTransition(0,
			                                                                                       0,
			                                                                                       "Stop",
			                                                                                       iteratorStruct->fsmName_,
			                                                                                       WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
			                                                                                       WebUsers::DEFAULT_ITERATOR_USERNAME,
			                                                                                       iteratorStruct->fsmCommandParameters_);

			if(errorStr != "")
			{
				__SS__ << "Iterator failed to stop run because of the following error: " << errorStr;
				__SS_THROW__;
			}

			// write indication of run done
			iteratorStruct->runIsDone_ = true;

			// write original duration back to string
			char str[200];
			sprintf(str, "%ld", iteratorStruct->originalDurationInSeconds_);
			iteratorStruct->commands_[iteratorStruct->commandIndex_].params_[IterateTable::commandRunParams_.DurationInSeconds_] = str;  // re-store as string
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
// return true if done
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
	std::lock_guard<std::mutex> lock(iteratorStruct->theIterator_->theSupervisor_->stateMachineAccessMutex_);
	if(iteratorStruct->theIterator_->theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Have FSM access" << __E__;

	if(iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.isInTransition())
		return false;

	std::string errorStr     = "";
	std::string currentState = iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.getCurrentStateName();

	if(currentState == "Halted")
		errorStr = iteratorStruct->theIterator_->theSupervisor_->attemptStateMachineTransition(0,
		                                                                                       0,
		                                                                                       "Configure",
		                                                                                       iteratorStruct->fsmName_,
		                                                                                       WebUsers::DEFAULT_ITERATOR_USERNAME /*fsmWindowName*/,
		                                                                                       WebUsers::DEFAULT_ITERATOR_USERNAME,
		                                                                                       iteratorStruct->fsmCommandParameters_);
	else if(currentState != "Configured")
		errorStr = "Expected to be in Configure. Unexpectedly, the current state is " + currentState + "." +
		           ". Last State Machine error message was as follows: " + iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.getErrorMessage();
	else  // else successfully done (in Configured state!)
	{
		__COUT__ << "checkCommandConfigureAlias complete." << __E__;

		// also activate the Iterator config manager to mimic active config
		std::pair<std::string, TableGroupKey> newActiveGroup = iteratorStruct->cfgMgr_->getTableGroupFromAlias(iteratorStruct->fsmCommandParameters_[0]);
		iteratorStruct->cfgMgr_->loadTableGroup(newActiveGroup.first, newActiveGroup.second, true /*activate*/);

		__COUT__ << "originalTrackChanges " << iteratorStruct->originalTrackChanges_ << __E__;
		__COUT__ << "originalConfigGroup " << iteratorStruct->originalConfigGroup_ << __E__;
		__COUT__ << "originalConfigKey " << iteratorStruct->originalConfigKey_ << __E__;

		__COUT__ << "currentTrackChanges " << ConfigurationInterface::isVersionTrackingEnabled() << __E__;
		__COUT__ << "originalConfigGroup " << iteratorStruct->cfgMgr_->getActiveGroupName() << __E__;
		__COUT__ << "originalConfigKey " << iteratorStruct->cfgMgr_->getActiveGroupKey() << __E__;

		return true;
	}

	if(errorStr != "")
	{
		__SS__ << "Iterator failed to configure with system alias '"
		       << (iteratorStruct->fsmCommandParameters_.size() ? iteratorStruct->fsmCommandParameters_[0] : "UNKNOWN")
		       << "' because of the following error: " << errorStr;
		__SS_THROW__;
	}
	return false;
}  // end checkCommandConfigure()

//==============================================================================
// return true if done
bool Iterator::checkCommandFSMTransition(IteratorWorkLoopStruct* iteratorStruct, const std::string& finalState)
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
	std::lock_guard<std::mutex> lock(iteratorStruct->theIterator_->theSupervisor_->stateMachineAccessMutex_);
	if(iteratorStruct->theIterator_->theSupervisor_->VERBOSE_MUTEX)
		__COUT__ << "Have FSM access" << __E__;

	if(iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.isInTransition())
		return false;

	std::string errorStr     = "";
	std::string currentState = iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.getCurrentStateName();

	if(currentState != finalState)
		errorStr = "Expected to be in " + finalState + ". Unexpectedly, the current state is " + currentState + "." +
		           ". Last State Machine error message was as follows: " + iteratorStruct->theIterator_->theSupervisor_->theStateMachine_.getErrorMessage();
	else  // else successfully done (in Configured state!)
	{
		__COUT__ << "checkCommandFSMTransition complete." << __E__;

		return true;
	}

	if(errorStr != "")
	{
		__SS__ << "Iterator failed to reach final state '" << finalState << "' because of the following error: " << errorStr;
		__SS_THROW__;
	}
	return false;
}  // end checkCommandConfigure()

//==============================================================================
bool Iterator::handleCommandRequest(HttpXmlDocument& xmldoc, const std::string& command, const std::string& parameter)
{
	//__COUTV__(command);
	if(command == "iteratePlay")
	{
		playIterationPlan(xmldoc, parameter);
		return true;
	}
	else if(command == "iteratePause")
	{
		pauseIterationPlan(xmldoc);
		return true;
	}
	else if(command == "iterateHalt")
	{
		haltIterationPlan(xmldoc);
		return true;
	}
	else if(command == "getIterationPlanStatus")
	{
		if(activePlanName_ == "" && parameter != "") //take parameter to set active plan name from GUI manipulations
		{
			activePlanName_ = parameter;
			__COUTV__(activePlanName_);		
		}
		getIterationPlanStatus(xmldoc);
		return true;
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
			__MOUT_ERR__ << "\n" << ss.str();

			xmldoc.addTextElementToData("state_tranisition_attempted",
			                            "0");  // indicate to GUI transition NOT attempted
			xmldoc.addTextElementToData("state_tranisition_attempted_err",
			                            ss.str());  // indicate to GUI transition NOT attempted

			return true;  // to block other commands
		}
	}
	return false;
}

//==============================================================================
void Iterator::playIterationPlan(HttpXmlDocument& xmldoc, const std::string& planName)
{
	__MOUT__ << "Attempting to play iteration plan '" << planName << ".'" << __E__;
	__COUT__ << "Attempting to play iteration plan '" << planName << ".'" << __E__;

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
			std::thread([](Iterator* iterator) { Iterator::IteratorWorkLoop(iterator); }, this).detach();
		}

		activePlanName_ = planName;
		commandPlay_    = true;
	}
	else
	{
		__SS__ << "Invalid play command attempted. Can only play when the Iterator is "
		          "inactive or paused."
		       << " If you would like to restart an iteration plan, first try halting." << __E__;
		__MOUT__ << ss.str();

		xmldoc.addTextElementToData("error_message", ss.str());

		__COUT__ << "Invalid play command attempted. " << commandPlay_ << " " << activePlanName_ << __E__;
	}
}

//==============================================================================
void Iterator::pauseIterationPlan(HttpXmlDocument& xmldoc)
{
	__MOUT__ << "Attempting to pause iteration plan '" << activePlanName_ << ".'" << __E__;
	__COUT__ << "Attempting to pause iteration plan '" << activePlanName_ << ".'" << __E__;

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
		__SS__ << "Invalid pause command attempted. Can only pause when running." << __E__;
		__MOUT__ << ss.str();

		xmldoc.addTextElementToData("error_message", ss.str());

		__COUT__ << "Invalid pause command attempted. " << workloopRunning_ << " " << activePlanIsRunning_ << " " << commandPause_ << " " << activePlanName_
		         << __E__;
	}
}

//==============================================================================
void Iterator::haltIterationPlan(HttpXmlDocument& /*xmldoc*/)
{
	__MOUT__ << "Attempting to halt iteration plan '" << activePlanName_ << ".'" << __E__;
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
		__COUT__ << "No thread, so conducting halt." << __E__;
		Iterator::haltIterator(this);
	}
}

//==============================================================================
//	return state machine and iterator status
void Iterator::getIterationPlanStatus(HttpXmlDocument& xmldoc)
{
	xmldoc.addTextElementToData("current_state",
	                            theSupervisor_->theStateMachine_.isInTransition()
	                                ? theSupervisor_->theStateMachine_.getCurrentTransitionName(theSupervisor_->stateMachineLastCommandInput_)
	                                : theSupervisor_->theStateMachine_.getCurrentStateName());

	// xmldoc.addTextElementToData("in_transition",
	// theSupervisor_->theStateMachine_.isInTransition() ? "1" : "0");
	if(theSupervisor_->theStateMachine_.isInTransition())
		xmldoc.addTextElementToData("transition_progress", theSupervisor_->theProgressBar_.readPercentageString());
	else
		xmldoc.addTextElementToData("transition_progress", "100");

	char tmp[20];
	sprintf(tmp, "%lu", theSupervisor_->theStateMachine_.getTimeInState());
	xmldoc.addTextElementToData("time_in_state", tmp);

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

	sprintf(tmp, "%u", activeCommandIndex_);
	xmldoc.addTextElementToData("current_command_index", tmp);
	sprintf(tmp, "%ld", time(0) - activeCommandStartTime_);
	xmldoc.addTextElementToData("current_command_duration", tmp);
	sprintf(tmp, "%u", activeCommandIteration_);
	xmldoc.addTextElementToData("current_command_iteration", tmp);

	for(const auto& depthIteration : depthIterationStack_)
	{
		sprintf(tmp, "%u", depthIteration);
		xmldoc.addTextElementToData("depth_iteration", tmp);
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
}
