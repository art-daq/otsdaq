#include "otsdaq-core/Supervisor/Iterator.h"
#include "otsdaq-core/MessageFacility/MessageFacility.h"
#include "otsdaq-core/Macros/CoutHeaderMacros.h"
#include "otsdaq-core/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq-core/Supervisor/Supervisor.h"

#include <iostream>
#include <thread>       //for std::thread

#undef 	__MF_SUBJECT__
#define __MF_SUBJECT__ "SupervisorIterator"

using namespace ots;

//========================================================================================================================
Iterator::Iterator(Supervisor* supervisor)
: workloopRunning_				(false)
, activePlanIsRunning_			(false)
, commandPlay_(false), commandPause_(false), commandHalt_(false)
, activePlanName_				("")
, theSupervisor_				(supervisor)
{
	__MOUT__ << "Iterator constructed." << std::endl;
	__COUT__ << "Iterator constructed." << std::endl;

}

//========================================================================================================================
Iterator::~Iterator(void)
{
}

//========================================================================================================================
void Iterator::IteratorWorkLoop(Iterator *iterator)
try
{
	__MOUT__ << "Iterator work loop starting..." << std::endl;
	__COUT__ << "Iterator work loop starting..." << std::endl;

	ConfigurationManager cfgMgr;
	unsigned int commandIndex = (unsigned int)-1;
	std::string activePlan = "";
	bool running = false;

	while(1)
	{
		//Process:
		//	- always "listen" for commands
		//		- play: if no plan running, activePlanIsRunning_ = true,
		//			and start or continue plan based on name/commandIndex
		//		- pause: if plan playing, pause it, activePlanIsRunning_ = false
		//			and do not clear commandIndex or name
		//		- halt: if plan playing or not, activePlanIsRunning_ = false
		//			and clear commandIndex or name
		//	- when running
		//		- go through each command and execute them

		//start command handling
		//define mutex scope
		{
			//lockout the messages array for the remainder of the scope
			//this guarantees the reading thread can safely access the messages
			std::lock_guard<std::mutex> lock(iterator->accessMutex_);

			if(!iterator->commandPlay_)
			{
				iterator->commandPlay_ = false; //clear

				if(!iterator->activePlanIsRunning_)
				{
					//valid PLAY command!

					iterator->activePlanIsRunning_ = true;

					if(activePlan != iterator->activePlanName_)
					{
						__COUT__ << "New plan name encountered old=" << activePlan <<
								" vs new=" << iterator->activePlanName_ << std::endl;
						commandIndex = -1; //reset for new plan
					}

					activePlan = iterator->activePlanName_;

					if(commandIndex == (unsigned int)-1)
						__MOUT__ << "Starting plan '" << activePlan << ".'" << std::endl;
					else
						__MOUT__ << "Continuing plan '" << activePlan << "' at command index " <<
							commandIndex << ". " << std::endl;
				}
			}
			else if(!iterator->commandPause_)
			{
				iterator->commandPause_ = false; //clear

				if(iterator->activePlanIsRunning_)
				{
					//valid PAUSE command!

					iterator->activePlanIsRunning_ = false;

					__MOUT__ << "Paused plan '" << activePlan << "' at command index " <<
						commandIndex << ". " << std::endl;
				}
			}
			else if(!iterator->commandHalt_)
			{
				iterator->commandHalt_ = false; //clear

				//valid HALT command!

				iterator->activePlanIsRunning_ = false;

				__MOUT__ << "Halted plan '" << activePlan << "' at command index " <<
					commandIndex << ". " << std::endl;

				activePlan = ""; //clear
				commandIndex = -1; //clear
			}

			running = iterator->activePlanIsRunning_;
		} //end command handling


		////////////////
		////////////////
		//	handle running
		if(running)
		{
			if(commandIndex == (unsigned int)-1)
			{
				__COUT__ << "Get commands" << std::endl;

			}

		} 	//end running
		////////////////
		////////////////


		sleep(1); //do everything on steps of 1 second
	}

	iterator->workloopRunning_ = false; //if we ever exit
}
catch(...)
{
	iterator->workloopRunning_ = false; //if we ever exit
}

//========================================================================================================================
void Iterator::playIterationPlan(HttpXmlDocument& xmldoc, const std::string& planName)
{
	__MOUT__ << "Attempting to playing iteration plan '" << planName << ".'" << std::endl;
	__COUT__ << "Attempting to playing iteration plan '" << planName << ".'" << std::endl;

	if(!workloopRunning_)
	{
		workloopRunning_ = true;

		//must start thread first
		std::thread([](Iterator *iterator){ Iterator::IteratorWorkLoop(iterator); },this).detach();
	}

	//setup "play" command

	//lockout the messages array for the remainder of the scope
	//this guarantees the reading thread can safely access the messages
	std::lock_guard<std::mutex> lock(accessMutex_);
	if(workloopRunning_ && !activePlanIsRunning_ && !commandPlay_)
	{
		activePlanName_ = planName;
		commandPlay_ = true;
	}
	else
	{
		__MOUT__ << "Invalid play command attempted." << std::endl;
		__COUT__ << "Invalid play command attempted. " <<
				commandPlay_ << " " <<
				activePlanName_ << std::endl;
	}
}

//========================================================================================================================
void Iterator::pauseIterationPlan(HttpXmlDocument& xmldoc)
{
	__MOUT__ << "Attempting to pause iteration plan '" << activePlanName_ << ".'" << std::endl;
	__COUT__ << "Attempting to pause iteration plan '" << activePlanName_ << ".'" << std::endl;

	//setup "pause" command

	//lockout the messages array for the remainder of the scope
	//this guarantees the reading thread can safely access the messages
	std::lock_guard<std::mutex> lock(accessMutex_);
	if(workloopRunning_ && activePlanIsRunning_ && !commandPause_)
	{
		commandPause_ = true;
	}
	else
	{
		__MOUT__ << "Invalid pause command attempted." << std::endl;
		__COUT__ << "Invalid pause command attempted. " <<
				workloopRunning_ << " " <<
				activePlanIsRunning_ << " " <<
				commandPause_ << " " <<
				activePlanName_ << std::endl;
	}
}

//========================================================================================================================
void Iterator::haltIterationPlan(HttpXmlDocument& xmldoc)
{
	__MOUT__ << "Attempting to halt iteration plan '" << activePlanName_ << ".'" << std::endl;
	__COUT__ << "Attempting to halt iteration plan '" << activePlanName_ << ".'" << std::endl;

	//setup "halt" command

	//lockout the messages array for the remainder of the scope
	//this guarantees the reading thread can safely access the messages
	std::lock_guard<std::mutex> lock(accessMutex_);
	if(workloopRunning_ && activePlanIsRunning_ && !commandHalt_)
	{
		commandHalt_ = true;
	}
	else
	{
		__MOUT__ << "Invalid halt command attempted." << std::endl;
		__COUT__ << "Invalid halt command attempted. " <<
				workloopRunning_ << " " <<
				activePlanIsRunning_ << " " <<
				commandHalt_ << " " <<
				activePlanName_ << std::endl;
	}
}

//========================================================================================================================
void Iterator::getIterationPlanStatus(HttpXmlDocument& xmldoc)
{
//	xmldoc.addTextElementToData("current_state", theStateMachine_.getCurrentStateName());
//	xmldoc.addTextElementToData("in_transition", theStateMachine_.isInTransition() ? "1" : "0");
//	if (theStateMachine_.isInTransition())
//		xmldoc.addTextElementToData("transition_progress", theProgressBar_.readPercentageString());
//	else
//		xmldoc.addTextElementToData("transition_progress", "100");
//
//
//	char tmp[20];
//	sprintf(tmp,"%lu",theStateMachine_.getTimeInState());
//	xmldoc.addTextElementToData("time_in_state", tmp);
//
//
//
//	//__COUT__ << "current state: " << theStateMachine_.getCurrentStateName() << std::endl;
//
//
//	//// ======================== get run name based on fsm name ====
//	std::string fsmName = CgiDataUtilities::getData(cgi, "fsmName");
//	//		__COUT__ << "fsmName = " << fsmName << std::endl;
//	//		__COUT__ << "activeStateMachineName_ = " << activeStateMachineName_ << std::endl;
//	//		__COUT__ << "theStateMachine_.getProvenanceStateName() = " <<
//	//				theStateMachine_.getProvenanceStateName() << std::endl;
//	//		__COUT__ << "theStateMachine_.getCurrentStateName() = " <<
//	//				theStateMachine_.getCurrentStateName() << std::endl;
//
//	if(!theStateMachine_.isInTransition())
//	{
//		std::string stateMachineRunAlias = "Run"; //default to "Run"
//
//		// get stateMachineAliasFilter if possible
//		ConfigurationTree configLinkNode = theConfigurationManager_->getSupervisorConfigurationNode(
//				supervisorContextUID_, supervisorApplicationUID_);
//
//		if(!configLinkNode.isDisconnected())
//		{
//			try //for backwards compatibility
//			{
//				ConfigurationTree fsmLinkNode = configLinkNode.getNode("LinkToStateMachineConfiguration");
//				if(!fsmLinkNode.isDisconnected())
//					stateMachineRunAlias =
//							fsmLinkNode.getNode(fsmName + "/RunDisplayAlias").getValue<std::string>();
//				//else
//				//	__COUT_INFO__ << "FSM Link disconnected." << std::endl;
//			}
//			catch(std::runtime_error &e) { __COUT_INFO__ << e.what() << std::endl; }
//			catch(...) { __COUT_ERR__ << "Unknown error. Should never happen." << std::endl; }
//		}
//		//else
//		//	__COUT_INFO__ << "FSM Link disconnected." << std::endl;
//
//		//__COUT__ << "stateMachineRunAlias  = " << stateMachineRunAlias	<< std::endl;
//
//		xmldoc.addTextElementToData("stateMachineRunAlias", stateMachineRunAlias);
//		//// ======================== get run name based on fsm name ====
//
//
//
//		if(theStateMachine_.getCurrentStateName() == "Running" ||
//				theStateMachine_.getCurrentStateName() == "Paused")
//			sprintf(tmp,"Current %s Number: %u",stateMachineRunAlias.c_str(),getNextRunNumber(activeStateMachineName_)-1);
//		else
//			sprintf(tmp,"Next %s Number: %u",stateMachineRunAlias.c_str(),getNextRunNumber(fsmName));
//		xmldoc.addTextElementToData("run_number", tmp);
//	}
}

