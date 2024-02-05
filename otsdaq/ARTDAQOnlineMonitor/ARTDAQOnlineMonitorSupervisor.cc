#include "otsdaq/ARTDAQOnlineMonitor/ARTDAQOnlineMonitorSupervisor.h"

#include "otsdaq/TablePlugins/ARTDAQTableBase/ARTDAQTableBase.h"

#include "artdaq-core/Utilities/ExceptionHandler.hh"
#include "artdaq-core/Utilities/TimeUtils.hh"

#include <signal.h>
#include <sys/wait.h>
#include <boost/filesystem.hpp>
#include <boost/thread.hpp>

XDAQ_INSTANTIATOR_IMPL(ots::ARTDAQOnlineMonitorSupervisor)

#define FAKE_CONFIG_NAME "ots_config"

//==============================================================================
ots::ARTDAQOnlineMonitorSupervisor::ARTDAQOnlineMonitorSupervisor(xdaq::ApplicationStub* stub)
    : CoreSupervisorBase(stub), partition_(getSupervisorProperty("partition", 0))
{
	__SUP_COUT__ << "Constructor." << __E__;

	INIT_MF("." /*directory used is USER_DATA/LOG/.*/);

	__SUP_COUT__ << "Constructed." << __E__;
}  // end constructor()

//==============================================================================
ots::ARTDAQOnlineMonitorSupervisor::~ARTDAQOnlineMonitorSupervisor(void)
{
	__SUP_COUT__ << "Destructor." << __E__;
	destroy();
	__SUP_COUT__ << "Destructed." << __E__;
}  // end destructor()

//==============================================================================

void ots::ARTDAQOnlineMonitorSupervisor::destroy(void)
{
	__SUP_COUT__ << "Destroying..." << __E__;

	__SUP_COUT__ << "Destroyed." << __E__;
}  // end destroy()

//==============================================================================
void ots::ARTDAQOnlineMonitorSupervisor::init(void)
{
	__SUP_COUT__ << "Initializing..." << __E__;

	__SUP_COUT__ << "Initialized." << __E__;
}  // end init()

//==============================================================================
void ots::ARTDAQOnlineMonitorSupervisor::transitionInitializing(toolbox::Event::Reference /*event*/)
try
{
	__SUP_COUT__ << "Initializing..." << __E__;
	init();
	__SUP_COUT__ << "Initialized." << __E__;
}  // end transitionInitializing()
catch(const std::runtime_error& e)
{
	__SS__ << "Error was caught while Initializing: " << e.what() << __E__;
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "Unknown error was caught while Initializing. Please checked the logs." << __E__;
	artdaq::ExceptionHandler(artdaq::ExceptionHandlerRethrow::no, ss.str());
	__SS_THROW__;
}  // end transitionInitializing() error handling

//==============================================================================
void ots::ARTDAQOnlineMonitorSupervisor::transitionConfiguring(toolbox::Event::Reference /*e*/)
{
	std::pair<std::string /*group name*/, TableGroupKey> theGroup(
		SOAPUtilities::translate(theStateMachine_.getCurrentMessage()).getParameters().getValue("ConfigurationTableGroupName"),
		TableGroupKey(SOAPUtilities::translate(theStateMachine_.getCurrentMessage()).getParameters().getValue("ConfigurationTableGroupKey")));

	__SUP_COUT__ << "Configuration table group name: " << theGroup.first << " key: " << theGroup.second << std::endl;

	try
	{		
		//disable version tracking to accept untracked versions to be selected by the FSM transition source
		theConfigurationManager_->loadTableGroup(theGroup.first, theGroup.second, true /*doActivate*/,
			0,0,0,0,0,0,false,0,0,ConfigurationManager::LoadGroupType::ALL_TYPES,
			true /*ignoreVersionTracking*/);
	}
	catch(const std::runtime_error& e)
	{
		__SS__ << "Error loading table group '" << theGroup.first << "(" << theGroup.second << ")! \n" << e.what() << __E__;
		__SUP_COUT_ERR__ << ss.str();
		// ExceptionHandler(ExceptionHandlerRethrow::no, ss.str());

		//__SS_THROW_ONLY__;
		theStateMachine_.setErrorMessage(ss.str());
		throw toolbox::fsm::exception::Exception(
			"Transition Error" /*name*/, ss.str() /* message*/, "ARTDAQOnlineMonitorSupervisor::transitionConfiguring" /*module*/, __LINE__ /*line*/, __FUNCTION__ /*function*/
		);
	}
	catch(...)
	{
		__SS__ << "Unknown error loading table group '" << theGroup.first << "(" << theGroup.second << ")!" << __E__;
		__SUP_COUT_ERR__ << ss.str();
		// ExceptionHandler(ExceptionHandlerRethrow::no, ss.str());

		//__SS_THROW_ONLY__;
		theStateMachine_.setErrorMessage(ss.str());
		throw toolbox::fsm::exception::Exception(
			"Transition Error" /*name*/, ss.str() /* message*/, "ARTDAQOnlineMonitorSupervisor::transitionConfiguring" /*module*/, __LINE__ /*line*/, __FUNCTION__ /*function*/
		);
	}


	try
	{
		ConfigurationTree theSupervisorNode = getSupervisorTableNode();
		om_rank_                            = theSupervisorNode.getNode("MonitorID").getValue<int>();

		__SUP_COUT__ << "Building configuration directory" << __E__;

		boost::system::error_code ignored;
		//		boost::filesystem::remove_all(ARTDAQTableBase::ARTDAQ_FCL_PATH + FAKE_CONFIG_NAME, ignored);

		// Make directory for art process logfiles
		boost::filesystem::create_directory(std::string(__ENV__("OTSDAQ_LOG_ROOT")) + "/" + theSupervisorNode.getValue(), ignored);
		mkdir((ARTDAQTableBase::ARTDAQ_FCL_PATH + FAKE_CONFIG_NAME).c_str(), 0755);

		// Generate Online Monitor FHICL
		ARTDAQTableBase::outputOnlineMonitorFHICL(theSupervisorNode);
		ARTDAQTableBase::flattenFHICL(ARTDAQTableBase::ARTDAQAppType::Monitor, theSupervisorNode.getValue());

		symlink(ARTDAQTableBase::getFlatFHICLFilename(ARTDAQTableBase::ARTDAQAppType::Monitor, theSupervisorNode.getValue()).c_str(),
				(ARTDAQTableBase::ARTDAQ_FCL_PATH + FAKE_CONFIG_NAME + "/" + theSupervisorNode.getValue() + ".fcl").c_str());
		config_file_name_ = ARTDAQTableBase::ARTDAQ_FCL_PATH + FAKE_CONFIG_NAME + "/" + theSupervisorNode.getValue() + ".fcl";
	}
	catch(const std::runtime_error& e)
	{
		__SS__ << "Error configuring the ARTDAQOnlineMonitorSupervisor! \n" << e.what() << __E__;
		__SUP_COUT_ERR__ << ss.str();
		// ExceptionHandler(ExceptionHandlerRethrow::no, ss.str());

		//__SS_THROW_ONLY__;
		theStateMachine_.setErrorMessage(ss.str());
		throw toolbox::fsm::exception::Exception(
			"Transition Error" /*name*/, ss.str() /* message*/, "ARTDAQOnlineMonitorSupervisor::transitionConfiguring" /*module*/, __LINE__ /*line*/, __FUNCTION__ /*function*/
		);
	}
} //end transitionConfiguring()

//==============================================================================
void ots::ARTDAQOnlineMonitorSupervisor::transitionStarting(toolbox::Event::Reference /*event*/)
try
{
	__SUP_COUT__ << "Starting..." << __E__;

	StartArtProcess(config_file_name_);

	__SUP_COUT__ << "Started." << __E__;
}  // end transitionStarting()
catch(const std::runtime_error& e)
{
	__SS__ << "Error was caught while Starting: " << e.what() << __E__;
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "Unknown error was caught while Starting. Please checked the logs." << __E__;
	artdaq::ExceptionHandler(artdaq::ExceptionHandlerRethrow::no, ss.str());
	__SS_THROW__;
}  // end transitionStarting() error handling

//==============================================================================
void ots::ARTDAQOnlineMonitorSupervisor::transitionStopping(toolbox::Event::Reference /*event*/)
try
{
	__SUP_COUT__ << "Stopping..." << __E__;

	ShutdownArtProcess();

	__SUP_COUT__ << "Stopped." << __E__;
}  // end transitionStopping()
catch(const std::runtime_error& e)
{
	__SS__ << "Error was caught while Stopping: " << e.what() << __E__;
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "Unknown error was caught while Stopping. Please checked the logs." << __E__;
	artdaq::ExceptionHandler(artdaq::ExceptionHandlerRethrow::no, ss.str());
	__SS_THROW__;
}  // end transitionStopping() error handling

//==============================================================================
void ots::ARTDAQOnlineMonitorSupervisor::transitionPausing(toolbox::Event::Reference /*event*/)
try
{
	__SUP_COUT__ << "Pausing..." << __E__;

	ShutdownArtProcess();

	__SUP_COUT__ << "Paused." << __E__;
}  // end transitionPausing()
catch(const std::runtime_error& e)
{
	__SS__ << "Error was caught while Pausing: " << e.what() << __E__;
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "Unknown error was caught while Pausing. Please checked the logs." << __E__;
	artdaq::ExceptionHandler(artdaq::ExceptionHandlerRethrow::no, ss.str());
	__SS_THROW__;
}  // end transitionPausing() error handling

//==============================================================================
void ots::ARTDAQOnlineMonitorSupervisor::transitionResuming(toolbox::Event::Reference /*event*/)
try
{
	__SUP_COUT__ << "Resuming..." << __E__;

	StartArtProcess(config_file_name_);

	__SUP_COUT__ << "Resumed." << __E__;
}  // end transitionResuming()
catch(const std::runtime_error& e)
{
	__SS__ << "Error was caught while Resuming: " << e.what() << __E__;
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "Unknown error was caught while Resuming. Please checked the logs." << __E__;
	artdaq::ExceptionHandler(artdaq::ExceptionHandlerRethrow::no, ss.str());
	__SS_THROW__;
}  // end transitionResuming() error handling

//==============================================================================
void ots::ARTDAQOnlineMonitorSupervisor::transitionHalting(toolbox::Event::Reference /*event*/)
try
{
	__SUP_COUT__ << "Halting..." << __E__;

	ShutdownArtProcess();

	__SUP_COUT__ << "Halted." << __E__;
}  // end transitionHalting()
catch(const std::runtime_error& e)
{
	const std::string transitionName = "Halting";
	// if halting from Failed state, then ignore errors
	if(theStateMachine_.getProvenanceStateName() == RunControlStateMachine::FAILED_STATE_NAME ||
	   theStateMachine_.getProvenanceStateName() == RunControlStateMachine::HALTED_STATE_NAME)
	{
		__SUP_COUT_INFO__ << "Error was caught while halting (but ignoring because "
		                     "previous state was '"
		                  << RunControlStateMachine::FAILED_STATE_NAME << "'): " << e.what() << __E__;
	}
	else  // if not previously in Failed state, then fail
	{
		__SUP_SS__ << "Error was caught while " << transitionName << ": " << e.what() << __E__;
		__SUP_COUT_ERR__ << "\n" << ss.str();
		theStateMachine_.setErrorMessage(ss.str());
		throw toolbox::fsm::exception::Exception("Transition Error" /*name*/,
		                                         ss.str() /* message*/,
		                                         "ots::ARTDAQOnlineMonitorSupervisorBase::transition" + transitionName /*module*/,
		                                         __LINE__ /*line*/,
		                                         __FUNCTION__ /*function*/
		);
	}
}  // end transitionHalting() std::runtime_error exception handling
catch(...)
{
	const std::string transitionName = "Halting";
	// if halting from Failed state, then ignore errors
	if(theStateMachine_.getProvenanceStateName() == RunControlStateMachine::FAILED_STATE_NAME ||
	   theStateMachine_.getProvenanceStateName() == RunControlStateMachine::HALTED_STATE_NAME)
	{
		__SUP_COUT_INFO__ << "Unknown error was caught while halting (but ignoring "
		                     "because previous state was '"
		                  << RunControlStateMachine::FAILED_STATE_NAME << "')." << __E__;
	}
	else  // if not previously in Failed state, then fail
	{
		__SUP_SS__ << "Unknown error was caught while " << transitionName << ". Please checked the logs." << __E__;
		__SUP_COUT_ERR__ << "\n" << ss.str();
		theStateMachine_.setErrorMessage(ss.str());

		artdaq::ExceptionHandler(artdaq::ExceptionHandlerRethrow::no, ss.str());

		throw toolbox::fsm::exception::Exception("Transition Error" /*name*/,
		                                         ss.str() /* message*/,
		                                         "ots::ARTDAQOnlineMonitorSupervisor::transition" + transitionName /*module*/,
		                                         __LINE__ /*line*/,
		                                         __FUNCTION__ /*function*/
		);
	}
}  // end transitionHalting() exception handling

//==============================================================================
void ots::ARTDAQOnlineMonitorSupervisor::enteringError(toolbox::Event::Reference /*event*/)
{
	__SUP_COUT__ << "Entering error recovery state" << __E__;

	ShutdownArtProcess();

	__SUP_COUT__ << "EnteringError DONE." << __E__;

}  // end enteringError()

//==============================================================================

void ots::ARTDAQOnlineMonitorSupervisor::RunArt(const std::string& config_file, const std::shared_ptr<std::atomic<pid_t>>& pid_out)
{
	do
	{
		auto start_time = std::chrono::steady_clock::now();
		TLOG(TLVL_INFO) << "Starting art process with config file " << config_file;

		pid_t pid = 0;

		char* filename = new char[config_file.length() + 1];
		memcpy(filename, config_file.c_str(), config_file.length());
		filename[config_file.length()] = '\0';  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

#ifdef DEBUG_ART
		std::string debugArgS = "--config-out=" + app_name + "_art.out";
		char*       debugArg  = new char[debugArgS.length() + 1];
		memcpy(debugArg, debugArgS.c_str(), debugArgS.length());
		debugArg[debugArgS.length()] = '\0';

		std::vector<char*> args{const_cast<char*>("art"), const_cast<char*>("-c"), filename, debugArg, NULL};  // NOLINT(cppcoreguidelines-pro-type-const-cast)
#else
		std::vector<char*> args{const_cast<char*>("art"), const_cast<char*>("-c"), filename, nullptr};  // NOLINT(cppcoreguidelines-pro-type-const-cast)
#endif

		pid = fork();
		if(pid == 0)
		{ /* child */

			// Do any child environment setup here
			std::string envVarKey   = "ARTDAQ_PARTITION_NUMBER";
			std::string envVarValue = std::to_string(partition_);
			if(setenv(envVarKey.c_str(), envVarValue.c_str(), 1) != 0)
			{
				TLOG(TLVL_ERROR) << "Error setting environment variable \"" << envVarKey << "\" in the environment of a child art process. "
				                 << "This may result in incorrect TCP port number "
				                 << "assignments or other issues, and data may "
				                 << "not flow through the system correctly.";
			}
			envVarKey   = "ARTDAQ_APPLICATION_NAME";
			envVarValue = getSupervisorTableNode().getValue();
			if(setenv(envVarKey.c_str(), envVarValue.c_str(), 1) != 0)
			{
				TLOG(TLVL_DEBUG) << "Error setting environment variable \"" << envVarKey << "\" in the environment of a child art process. ";
			}
			envVarKey   = "ARTDAQ_RANK";
			envVarValue = std::to_string(om_rank_);
			if(setenv(envVarKey.c_str(), envVarValue.c_str(), 1) != 0)
			{
				TLOG(TLVL_DEBUG) << "Error setting environment variable \"" << envVarKey << "\" in the environment of a child art process. ";
			}

			execvp("art", &args[0]);
			delete[] filename;
			exit(1);
		}
		delete[] filename;

		*pid_out = pid;

		TLOG(TLVL_INFO) << "PID of new art process is " << pid;
		siginfo_t status;
		auto      sts = waitid(P_PID, pid, &status, WEXITED);
		TLOG(TLVL_INFO) << "Removing PID " << pid << " from process list";
		if(sts < 0)
		{
			TLOG(TLVL_WARNING) << "Error occurred in waitid for art process " << pid << ": " << errno << " (" << strerror(errno) << ").";
		}
		else if(status.si_code == CLD_EXITED && status.si_status == 0)
		{
			TLOG(TLVL_INFO) << "art process " << pid << " exited normally, " << (restart_art_ ? "restarting" : "not restarting");
		}
		else
		{
			auto art_lifetime = artdaq::TimeUtils::GetElapsedTime(start_time);
			if(art_lifetime < minimum_art_lifetime_s_)
			{
				restart_art_ = false;
			}

			auto exit_type = "exited with status code";
			switch(status.si_code)
			{
			case CLD_DUMPED:
			case CLD_KILLED:
				exit_type = "was killed with signal";
				break;
			case CLD_EXITED:
			default:
				break;
			}

			TLOG((restart_art_ ? TLVL_WARNING : TLVL_ERROR))
			    << "art process " << pid << " " << exit_type << " " << status.si_status << (status.si_code == CLD_DUMPED ? " (core dumped)" : "")
			    << " after running for " << std::setprecision(2) << std::fixed << art_lifetime << " seconds, "
			    << (restart_art_ ? "restarting" : "not restarting");
		}
	} while(restart_art_);
}

//==============================================================================

void ots::ARTDAQOnlineMonitorSupervisor::StartArtProcess(const std::string& config_file)
{
	static std::mutex            start_art_mutex;
	std::unique_lock<std::mutex> lk(start_art_mutex);
	// TraceLock lk(start_art_mutex, 15, "StartArtLock");
	restart_art_   = should_restart_art_;
	auto startTime = std::chrono::steady_clock::now();

	art_pid_ = std::shared_ptr<std::atomic<pid_t>>(new std::atomic<pid_t>(-1));
	boost::thread thread([this, config_file] { RunArt(config_file, art_pid_); });
	thread.detach();

	while((*art_pid_ <= 0) && (artdaq::TimeUtils::GetElapsedTime(startTime) < 5))
	{
		usleep(10000);
	}
	if(*art_pid_ <= 0)
	{
		TLOG(TLVL_WARNING) << "art process has not started after 5s. Check art configuration!"
		                   << " (pid=" << *art_pid_ << ")";
		__SS__ << "art process has not started after 5s. Check art configuration!"
		       << " (pid=" << *art_pid_ << ")" << __E__;
		__SUP_SS_THROW__;
	}

	TLOG(TLVL_INFO) << std::setw(4) << std::fixed << "art initialization took " << artdaq::TimeUtils::GetElapsedTime(startTime) << " seconds.";
}

//==============================================================================

void ots::ARTDAQOnlineMonitorSupervisor::ShutdownArtProcess()
{
	restart_art_ = false;
	// current_art_config_file_ = nullptr;
	// current_art_pset_ = fhicl::ParameterSet();

	auto check_pid = [&]() {
		if(art_pid_ == nullptr || *art_pid_ <= 0)
		{
			return false;
		}
		else if(kill(*art_pid_, 0) < 0)
		{
			return false;
		}
		return true;
	};

	if(!check_pid())
	{
		TLOG(14) << "art process already exited, nothing to do.";
		usleep(1000);
		return;
	}

	auto shutdown_start = std::chrono::steady_clock::now();

	int graceful_wait_ms = 1000 * 10;
	int gentle_wait_ms   = 1000 * 2;
	int int_wait_ms      = 1000;

	TLOG(TLVL_TRACE) << "Waiting up to " << graceful_wait_ms << " ms for art process to exit gracefully";
	for(int ii = 0; ii < graceful_wait_ms; ++ii)
	{
		usleep(1000);

		if(!check_pid())
		{
			TLOG(TLVL_INFO) << "art process exited after " << artdaq::TimeUtils::GetElapsedTimeMilliseconds(shutdown_start) << " ms.";
			return;
		}
	}

	{
		TLOG(TLVL_TRACE) << "Gently informing art process that it is time to shut down";

		TLOG(TLVL_TRACE) << "Sending SIGQUIT to pid " << *art_pid_;
		kill(*art_pid_, SIGQUIT);
	}

	TLOG(TLVL_TRACE) << "Waiting up to " << gentle_wait_ms << " ms for art process to exit from SIGQUIT";
	for(int ii = 0; ii < gentle_wait_ms; ++ii)
	{
		usleep(1000);

		if(!check_pid())
		{
			TLOG(TLVL_INFO) << "art process exited after " << artdaq::TimeUtils::GetElapsedTimeMilliseconds(shutdown_start) << " ms (SIGQUIT).";
			return;
		}
	}

	{
		TLOG(TLVL_TRACE) << "Insisting that the art process shut down";
		kill(*art_pid_, SIGINT);
	}

	TLOG(TLVL_TRACE) << "Waiting up to " << int_wait_ms << " ms for art process to exit from SIGINT";
	for(int ii = 0; ii < int_wait_ms; ++ii)
	{
		usleep(1000);

		if(!check_pid())
		{
			TLOG(TLVL_INFO) << "art process exited after " << artdaq::TimeUtils::GetElapsedTimeMilliseconds(shutdown_start) << " ms (SIGINT).";
			return;
		}
	}

	TLOG(TLVL_TRACE) << "Killing art process with extreme prejudice";
	while(check_pid())
	{
		{
			kill(*art_pid_, SIGKILL);
			usleep(1000);
		}
	}
	TLOG(TLVL_INFO) << "art process exited after " << artdaq::TimeUtils::GetElapsedTimeMilliseconds(shutdown_start) << " ms (SIGKILL).";
}

//==============================================================================
