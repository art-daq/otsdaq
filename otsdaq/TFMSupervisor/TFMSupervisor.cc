

#define TRACEMF_USE_VERBATIM 1  // for trace longer path filenames
#include "otsdaq/TFMSupervisor/TFMSupervisor.hh"

#include "artdaq-core/Utilities/configureMessageFacility.hh"
#include "artdaq/BuildInfo/GetPackageBuildInfo.hh"
#include "artdaq/DAQdata/Globals.hh"
#include "artdaq/ExternalComms/MakeCommanderPlugin.hh"
#include "cetlib_except/exception.h"
#include "fhiclcpp/make_ParameterSet.h"
#include "otsdaq/TFMSupervisor/TFMSupervisorTRACEController.h"

#include "artdaq-core/Utilities/ExceptionHandler.hh" /*for artdaq::ExceptionHandler*/

#include <boost/exception/all.hpp>
#include <boost/filesystem.hpp>

#include <signal.h>
#include <regex>
#include <pwd.h>
#include <filesystem>

using namespace ots;
namespace fs = std::filesystem;

XDAQ_INSTANTIATOR_IMPL(TFMSupervisor)

#define FAKE_CONFIG_NAME "ots_config"
#define DAQINTERFACE_PORT std::atoi(__ENV__("ARTDAQ_BASE_PORT")) + (partition_ * std::atoi(__ENV__("ARTDAQ_PORTS_PER_PARTITION")))

static TFMSupervisor*                         instance        = nullptr;
static std::unordered_map<int, struct sigaction> old_actions     = std::unordered_map<int, struct sigaction>();
static bool                                      sighandler_init = false;
static void                                      signal_handler(int signum)
{
	// Messagefacility may already be gone at this point, TRACE ONLY!
#if TRACE_REVNUM < 1459
	TRACE_STREAMER(TLVL_ERROR, &("TFMsupervisor")[0], 0, 0, 0)
#else
	TRACE_STREAMER(TLVL_ERROR, TLOG2("TFMsupervisor", 0), 0)
#endif
	    << "A signal of type " << signum
	    << " was caught by TFMSupervisor. Shutting down DAQInterface, "
	       "then proceeding with default handlers!";

	if(instance)
		instance->destroy();

	sigset_t set;
	pthread_sigmask(SIG_UNBLOCK, NULL, &set);
	pthread_sigmask(SIG_UNBLOCK, &set, NULL);

#if TRACE_REVNUM < 1459
	TRACE_STREAMER(TLVL_ERROR, &("TFMsupervisor")[0], 0, 0, 0)
#else
	TRACE_STREAMER(TLVL_ERROR, TLOG2("TFMsupervisor", 0), 0)
#endif
	    << "Calling default signal handler";
	if(signum != SIGUSR2)
	{
		sigaction(signum, &old_actions[signum], NULL);
		kill(getpid(), signum);  // Only send signal to self
	}
	else
	{
		// Send Interrupt signal if parsing SIGUSR2 (i.e. user-defined exception that
		// should tear down ARTDAQ)
		sigaction(SIGINT, &old_actions[SIGINT], NULL);
		kill(getpid(), SIGINT);  // Only send signal to self
	}
}

static void init_sighandler(TFMSupervisor* inst)
{
	static std::mutex            sighandler_mutex;
	std::unique_lock<std::mutex> lk(sighandler_mutex);

	if(!sighandler_init)
	{
		sighandler_init          = true;
		instance                 = inst;
		std::vector<int> signals = {
		    SIGINT, SIGILL, SIGABRT, SIGFPE, SIGSEGV, SIGPIPE, SIGALRM, SIGTERM, SIGUSR2, SIGHUP};  // SIGQUIT is used by art in normal operation
		for(auto signal : signals)
		{
			struct sigaction old_action;
			sigaction(signal, NULL, &old_action);

			// If the old handler wasn't SIG_IGN (it's a handler that just
			// "ignore" the signal)
			if(old_action.sa_handler != SIG_IGN)
			{
				struct sigaction action;
				action.sa_handler = signal_handler;
				sigemptyset(&action.sa_mask);
				for(auto sigblk : signals)
				{
					sigaddset(&action.sa_mask, sigblk);
				}
				action.sa_flags = 0;

				// Replace the signal handler of SIGINT with the one described by
				// new_action
				sigaction(signal, &action, NULL);
				old_actions[signal] = old_action;
			}
		}
	}
}

//int TFMSupervisor::xmlrpcClient_abort_ = 0; // static for xmlrpc client

//==============================================================================
TFMSupervisor::TFMSupervisor(xdaq::ApplicationStub* stub)
    : CoreSupervisorBase(stub)
    //, daqinterface_ptr_(NULL)
    , partition_(0)
    //, daqinterface_state_("notrunning")
    , tfm_connected_(0)
    , tfm_state_("notrunning")
    , runner_thread_(nullptr)
    , tfm_(0)
    , commanders_()
    , daq_repoers_enabled_(false)
{
	__SUP_COUT__ << "Constructor." << __E__;

	INIT_MF("." /*directory used is USER_DATA/LOG/.*/);
	init_sighandler(this);

    char* envValue = getenv("ARTDAQ_PARTITION");
    if (envValue != nullptr) partition_ = std::atoi(envValue);

    //snprintf(xmlrpcUrl_, 100, "http://localhost:%i/RPC2", 10000+1000*partition_);
    snprintf(xmlrpcUrl_, 100, "http://mu2edaq08.fnal.gov:%i/RPC2", 10000+1000*partition_);
    //xmlrpc_setup();

    __SUP_COUT__ << "Killing all running farm managers: " <<
                     killAllRunningFarmManagers() << __E__;

	// destroy current TRACEController and instantiate ARTDAQSupervisorTRACEController
	if(CorePropertySupervisorBase::theTRACEController_)
	{
		__SUP_COUT__ << "Destroying TRACE Controller..." << __E__;
		delete CorePropertySupervisorBase::theTRACEController_;  // destruct current TRACEController
		CorePropertySupervisorBase::theTRACEController_ = nullptr;
	}
	CorePropertySupervisorBase::theTRACEController_ = new TFMSupervisorTRACEController();
	((TFMSupervisorTRACEController*)CorePropertySupervisorBase::theTRACEController_)->setSupervisorPtr(this);

	__SUP_COUT__ << "Constructed." << __E__;
}  // end constructor()

//==============================================================================
TFMSupervisor::~TFMSupervisor(void)
{
	__SUP_COUT__ << "Destructor." << __E__;
	destroy();
    xmlrpc_cleanup();
	__SUP_COUT__ << "Destructed." << __E__;
}  // end destructor()

//==============================================================================
void TFMSupervisor::destroy(void)
{
	__SUP_COUT__ << "Destroying..." << __E__;

    TLOG(TLVL_DEBUG) << "tfm pid:" << tfm_ << ", if >0 send SIGKILL" << __E__;
    if(tfm_ > 0) {
        kill(tfm_, SIGKILL);
    }

	// CorePropertySupervisorBase would destroy, but since it was created here, attempt to destroy
	if(CorePropertySupervisorBase::theTRACEController_)
	{
		__SUP_COUT__ << "Destroying TRACE Controller..." << __E__;
		delete CorePropertySupervisorBase::theTRACEController_;
		CorePropertySupervisorBase::theTRACEController_ = nullptr;
	}

	__SUP_COUT__ << "Destroyed." << __E__;
}  // end destroy()

//==============================================================================
void TFMSupervisor::init(void)
{
	stop_runner_();

	__SUP_COUT__ << "Initializing..." << __E__;
	{
        
		//std::lock_guard<std::recursive_mutex> lk(daqinterface_mutex_);

		// allSupervisorInfo_.init(getApplicationContext());
		artdaq::configureMessageFacility("TFMSupervisor");
		__SUP_COUT__ << "artdaq MF configured." << __E__;

        // 
        char* config_dir = getenv("TFM_CONFIG_DIR");
        if(config_dir == NULL)
		{
			__SS__ << "TFM_CONFIG_DIR environment variable not set! This "
			          "is needed to set up TFM."
			       << __E__;
			__SUP_SS_THROW__;
		}
	}
	__SUP_COUT__ << "Initialized." << __E__;
}  // end init()

//==============================================================================
void TFMSupervisor::transitionConfiguring(toolbox::Event::Reference /*event*/)
{
    const int transitionTimeout = getSupervisorProperty("tfm_transition_timeout_ms", 20000);
    if(RunControlStateMachine::getIterationIndex() == 0 && RunControlStateMachine::getSubIterationIndex() == 0) {
        __SUP_COUT__ << "transitionConfiguring" << __E__;
        std::string configName = getSupervisorProperty("tfm_config", "demo_config");
        bool generateConfig    = getSupervisorProperty("tfm_generate_config", true);
        daq_repoers_enabled_   = getSupervisorProperty("tfm_daq_repoers_enabled_", true);

        setenv("TFM_CONFIG_NAME",configName.c_str(), 1);
        if(generateConfig) {
            __SUP_COUT__ << "(Re-)generate config '" << configName << "'.";
            set_thread_message_("(Re-)generate tfm config '"+configName+"'");
            int sts = writeSettings(configName);
            if(sts != 0) {
                // no !=0 return value is implemented yet
            }
        }
        RunControlStateMachine::indicateIterationWork();
        
    } else if(RunControlStateMachine::getIterationIndex() == 1 && RunControlStateMachine::getSubIterationIndex() == 0) { // first iteration

        std::string configName = getSupervisorProperty("tfm_config", "demo_config");
        setenv("TFM_CONFIG_NAME",configName.c_str(), 1);
        
        __SUP_COUT__ << "Configure TFM for '" << configName << "', partition " << partition_ << "." <<  __E__;

        // start by forking this process 
        set_thread_message_("Fork tfm (farm_manager.py) process");
        pid_t pid = fork();
        if (pid < 0) {
            __SS__ <<  "Failed to fork process" << __E__;
            __SUP_SS_THROW__;
        } else if (pid == 0) {
            char* tfm_dir = getenv("TFM_DIR");
            if(tfm_dir == NULL) {
                __SS__ << "TFM_DIR is not specified" << __E__;
                __SUP_SS_THROW__;
            }
            char* tfm_config_dir = getenv("TFM_CONFIG_DIR");
            if(tfm_config_dir == NULL) {
                __SS__ << "TFM_CONFIG_DIR is not specified" << __E__;
                __SUP_SS_THROW__;
            }
            std::string prog = std::string(tfm_dir)+"/rc/control/farm_manager.py";
            std::string arg = "--config-dir="+std::string(tfm_config_dir)+"/"+configName;
            __SUP_COUT__ << "/bin/sh -c '"+prog+" "+arg+"'" << __E__;
            execl("/bin/sh", "sh", "-c", (prog+" "+arg).c_str(), nullptr);

            // should never reach
            __SS__ << "Failed to run farm_manager.py" << __E__;
            __SUP_SS_THROW__;
            exit(1);
        } else { 
            // we are in the parent process
            tfm_ = pid;
            __SUP_COUT__ << "DEBUG Parent process" << __E__;
            xmlrpc_setup();
            
            std::lock_guard<std::recursive_mutex> lk(daqinterface_mutex_);
            setTfmState_("booting");
            //current_transition_timeout_ms_ = 3000;
            daqinterface_mutex_.unlock();
            usleep(1000000); // 1s
            start_runner_();
            // start a thread that monitors the daq status every second here
            RunControlStateMachine::indicateIterationWork();
        }
    } 
	else  // not first time
	{
        if(tfm_state_ != "stopped:100") {
            if(checkTransitionTimeout_(transitionTimeout)) {
                __SS__ << "tfm (farm_manager.py) timed out in state '" <<  tfm_state_ << "', tfm connected=" << tfm_connected_ << "." << __E__;
                __SUP_SS_THROW__;
            }
            set_thread_message_(tfm_state_); 
            usleep(500000); // 0.5s, daqinterface_state_ is updated ever 1s
            RunControlStateMachine::indicateIterationWork();
        } else { // all done
            // last step, make commanders
        }
    }

	return;
}  // end transitionConfiguring()

//==============================================================================
void TFMSupervisor::transitionHalting(toolbox::Event::Reference /*event*/)
//try
{
    __SUP_COUT__ << "transitionHalting" << __E__;
    commanders_.clear(); // stop updating daq reports
    //xmlrpc_setup();
    std::string msg;
    int rc = xmlrpc("shutdown","(s)","daqint", msg);
    if(rc != 0) {
        __SUP_COUT__ << "rpc result: " << __E__;
        TLOG(TLVL_WARNING) << "xmlrpc failed, TFM might already be halted." << __E__; 
    } else {
        TLOG(TLVL_DEBUG) << "xmlrpc result: " << msg << __E__;
    }

    stop_runner_();
    xmlrpc_cleanup();
    return;
}  // end transitionHalting() exception handling

//==============================================================================
void TFMSupervisor::transitionInitializing(toolbox::Event::Reference /*event*/)
try
{
	set_thread_message_("Initializing");
	__SUP_COUT__ << "Initializing..." << __E__;
	init();
	__SUP_COUT__ << "Initialized." << __E__;
	set_thread_message_("Initialized");
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
void TFMSupervisor::transitionPausing(toolbox::Event::Reference /*event*/)
try
{
    const int transitionTimeout = getSupervisorProperty("tfm_transition_timeout_ms", 20000);
    if(RunControlStateMachine::getIterationIndex() == 0 && RunControlStateMachine::getSubIterationIndex() == 0) {
        __SUP_COUT__ << "transitionPausing" << __E__;
        auto resultP = xmlrpc_client_call(&xmlrpcEnv_, xmlrpcUrl_, 
                            "state_change","(ss{s:i})","daqint","pausing","ignored_variable", 999);
        if (resultP == NULL) {
            __SS__ << "XML-RPC call failed: " << xmlrpcEnv_.fault_string << __E__;
            __SUP_SS_THROW__;
        }
        resetTfmStateTime_(); // reset timeout timer
        RunControlStateMachine::indicateIterationWork();
    } else {
        if(tfm_state_ != "stopped:100") {
            if(checkTransitionTimeout_(transitionTimeout)) {
                __SS__ << "tfm (farm_manager.py) timed out in state '" <<  tfm_state_ << "', tfm connected=" << tfm_connected_ << "." << __E__;
                __SUP_SS_THROW__;
            }
            set_thread_message_(tfm_state_); 
            usleep(500000); // 0.5s, daqinterface_state_ is updated ever 1s
            RunControlStateMachine::indicateIterationWork();
        } else {
            set_thread_message_(tfm_state_);
            // nothing to do, all done
        }
    }
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
void TFMSupervisor::transitionResuming(toolbox::Event::Reference /*event*/)
try
{
    const int transitionTimeout = getSupervisorProperty("tfm_transition_timeout_ms", 20000);
    if(RunControlStateMachine::getIterationIndex() == 0 && RunControlStateMachine::getSubIterationIndex() == 0) {
        __SUP_COUT__ << "transitionResuming" << __E__;
        auto resultP = xmlrpc_client_call(&xmlrpcEnv_, xmlrpcUrl_, 
                            "state_change","(ss{s:i})","daqint","resuming","ignored_variable", 999);
        if (resultP == NULL) {
            __SS__ << "XML-RPC call failed: " << xmlrpcEnv_.fault_string << __E__;
            __SUP_SS_THROW__;
        }
        resetTfmStateTime_(); // reset timeout timer
        RunControlStateMachine::indicateIterationWork();
    } else {
        if(tfm_state_ != "stopped:100") {
            if(checkTransitionTimeout_(transitionTimeout)) {
                __SS__ << "tfm (farm_manager.py) timed out in state '" <<  tfm_state_ << "', tfm connected=" << tfm_connected_ << "." << __E__;
                __SUP_SS_THROW__;
            }
            set_thread_message_(tfm_state_); 
            usleep(500000); // 0.5s, daqinterface_state_ is updated ever 1s
            RunControlStateMachine::indicateIterationWork();
        } else {
            set_thread_message_(tfm_state_);
            // nothing to do, all done
        }
    }
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
void TFMSupervisor::transitionStarting(toolbox::Event::Reference /*event*/)
try
{
    const int runNumber = atoi(SOAPUtilities::translate(theStateMachine_.getCurrentMessage()).getParameters().getValue("RunNumber").c_str());
    const int transitionTimeout = getSupervisorProperty("tfm_transition_timeout_ms", 20000);
    __SUP_COUT__ << "transitionStarting getIterationIndex" << RunControlStateMachine::getIterationIndex() << " " << RunControlStateMachine::getSubIterationIndex() << __E__;
    // first call, configure the tfm
    if(RunControlStateMachine::getIterationIndex() == 0 && RunControlStateMachine::getSubIterationIndex() == 0) {
        __SUP_COUT__ << "transitionStarting" << __E__;
        auto resultP = xmlrpc_client_call(&xmlrpcEnv_, xmlrpcUrl_, 
                            "state_change","(ss{s:i})","daqint","configuring","run_number", runNumber);
        if (resultP == NULL) {
            __SS__ << "XML-RPC call failed: " << xmlrpcEnv_.fault_string << __E__;
            __SUP_SS_THROW__;
        }
        resetTfmStateTime_(); // reset timeout timer
        RunControlStateMachine::indicateIterationWork();
    } else if(RunControlStateMachine::getIterationIndex() == 1) {
        // wait until the tfm is configured
        if(tfm_state_ != "configured:100") {
            if(checkTransitionTimeout_(transitionTimeout*2)) {
                __SS__ << "tfm (farm_manager.py) timed out in state '" <<  tfm_state_ << "', tfm connected=" << tfm_connected_ << "." << __E__;
                __SUP_SS_THROW__;
            }
            set_thread_message_(tfm_state_); 
            usleep(500000); // 0.5s, daqinterface_state_ is updated ever 1s
            RunControlStateMachine::indicateSubIterationWork();
        } else { // all done, move on to the next tfm configuration step
            auto resultP = xmlrpc_client_call(&xmlrpcEnv_, xmlrpcUrl_, 
                            "state_change","(ss{s:i})","daqint","starting","ignored_variable", runNumber);
            if (resultP == NULL) {
                __SS__ << "XML-RPC call failed: " << xmlrpcEnv_.fault_string << __E__;
                __SUP_SS_THROW__;
            }
            RunControlStateMachine::indicateIterationWork();
        }
    } else {
        if(tfm_state_ != "running:100") {
            if(checkTransitionTimeout_(transitionTimeout)) {
                __SS__ << "tfm (farm_manager.py) timed out in state '" <<  tfm_state_ << "', tfm connected=" << tfm_connected_ << "." << __E__;
                __SUP_SS_THROW__;
            }
            set_thread_message_(tfm_state_); 
            usleep(500000); // 0.5s, daqinterface_state_ is updated ever 1s
            //RunControlStateMachine::indicateSubIterationWork();
            RunControlStateMachine::indicateIterationWork();
        } else {
            set_thread_message_(tfm_state_);
            commanders_ = makeCommandersFromProcessInfo();
        }
    }
	return;

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
void TFMSupervisor::transitionStopping(toolbox::Event::Reference /*event*/)
try
{
    const int transitionTimeout = getSupervisorProperty("tfm_transition_timeout_ms", 20000);
    if(RunControlStateMachine::getIterationIndex() == 0 && RunControlStateMachine::getSubIterationIndex() == 0) {
        __SUP_COUT__ << "transitionStopping" << __E__;
        auto resultP = xmlrpc_client_call(&xmlrpcEnv_, xmlrpcUrl_, 
                            "state_change","(ss{s:i})","daqint","stopping","ignored_variable", 999);
        if (resultP == NULL) {
            __SS__ << "XML-RPC call failed: " << xmlrpcEnv_.fault_string << __E__;
            __SUP_SS_THROW__;
        }
        resetTfmStateTime_(); // reset timeout timer
        RunControlStateMachine::indicateIterationWork();
    } else {
        if(tfm_state_ != "stopped:100") {
            if(checkTransitionTimeout_(transitionTimeout)) {
                __SS__ << "tfm (farm_manager.py) timed out in state '" <<  tfm_state_ << "', tfm connected=" << tfm_connected_ << "." << __E__;
                __SUP_SS_THROW__;
            }
            set_thread_message_(tfm_state_); 
            usleep(500000); // 0.5s, daqinterface_state_ is updated ever 1s
            RunControlStateMachine::indicateIterationWork();
        } else {
            set_thread_message_(tfm_state_);
            // nothing to do, all done
        }
    }
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
void ots::TFMSupervisor::enteringError(toolbox::Event::Reference /*event*/)
{
}  // end enteringError()

std::vector<SupervisorInfo::SubappInfo> ots::TFMSupervisor::getSubappInfo(void)
{   
    if(tfm_connected_) {
        try {
            auto                                    apps = getAndParseProcessInfo_();
            std::vector<SupervisorInfo::SubappInfo> output;
            for(auto& app : apps)
            {
                SupervisorInfo::SubappInfo info;

                info.name           = app.label;
                info.detail         = "Rank " + std::to_string(app.rank) + ", subsystem " + std::to_string(app.subsystem);
                info.lastStatusTime = time(0);
                info.progress       = 100;
                info.status         = artdaqStateToOtsState(app.state);
                info.url            = "http://" + app.host + ":" + std::to_string(app.port) + "/RPC2";
                info.class_name     = "ARTDAQ " + labelToProcType_(app.label);

                output.push_back(info);
            }
            return output;
        } catch(...) {
            // catch and return empty list back below
        }
    }
    return std::vector<SupervisorInfo::SubappInfo> ();
}

//==============================================================================
void ots::TFMSupervisor::getDAQState_()
{   
    std::lock_guard<std::recursive_mutex> lk(daqinterface_mutex_);

    std::string state;
    if(xmlrpc("get_state","(s)","daqint", state) == 0) {
        setTfmState_(state);
        tfm_connected_ = 1;
    } else {
        tfm_connected_ = 0;
    }
 }  // end getDAQState_()

void ots::TFMSupervisor::getDAQReports_() {
    //auto commanders = makeCommandersFromProcessInfo(); // future, let's do this at the config step
    TLOG(TLVL_DEBUG) << "commanders_.size() = " << commanders_.size() << std::endl;
    for(auto& comm : commanders_) {
        TLOG(TLVL_DEBUG) << comm.first.label << ": state " << comm.first.state << __E__;
        //daq_reports_[comm.first.label] = comm.first.label+":"+comm.first.state;
        if(comm.first.state == "Running") {
            //daq_reports_[comm.first.label] += ";"+comm.second->send_report("stats");
            daq_reports_[comm.first.label] = comm.second->send_report("stats");
            TLOG(TLVL_DEBUG) << daq_reports_[comm.first.label] << __E__;
        }
        daq_reports_update_time_[comm.first.label] = std::chrono::high_resolution_clock::now();
    }
}  // end getDAQReports_()

//==============================================================================
std::string ots::TFMSupervisor::getProcessInfo_(void)
{
    return xmlrpc("artdaq_process_info","(s)","daqint");
   
}  // end getProcessInfo_()

std::string ots::TFMSupervisor::artdaqStateToOtsState(std::string state)
{
	if(state == "nonexistent")
		return RunControlStateMachine::INITIAL_STATE_NAME;
	if(state == "Ready")
		return "Configured";
	if(state == "Running")
		return RunControlStateMachine::RUNNING_STATE_NAME;
	if(state == "Paused")
		return RunControlStateMachine::PAUSED_STATE_NAME;
	if(state == "Stopped")
		return RunControlStateMachine::HALTED_STATE_NAME;

	TLOG(TLVL_WARNING) << "Unrecognized state name " << state;
	return state; //RunControlStateMachine::FAILED_STATE_NAME;
}

std::string ots::TFMSupervisor::labelToProcType_(std::string label)
{
	if(label_to_proc_type_map_.count(label))
	{
		return label_to_proc_type_map_[label];
	}
    return label;
}

//==============================================================================
std::list<ots::TFMSupervisor::DAQInterfaceProcessInfo> ots::TFMSupervisor::getAndParseProcessInfo_()
{
	std::list<ots::TFMSupervisor::DAQInterfaceProcessInfo> output;
	auto                                                      info  = getProcessInfo_();
	auto                                                      procs = tokenize_(info);

	// 0: Whole string
	// 1: Process Label
	// 2: Process host
	// 3: Process port
	// 4: Process subsystem
	// 5: Process Rank
	// 6: Process state
	std::regex re("(.*?) at ([^:]*):(\\d+) \\(subsystem (\\d+), rank (\\d+)\\): (.*)");

	for(auto& proc : procs)
	{
		std::smatch match;
		if(std::regex_match(proc, match, re))
		{
			DAQInterfaceProcessInfo info;

			info.label     = match[1];
			info.host      = match[2];
			info.port      = std::stoi(match[3]);
			info.subsystem = std::stoi(match[4]);
			info.rank      = std::stoi(match[5]);
			info.state     = match[6];

			output.push_back(info);
		}
	}
	return output;
}  // end getAndParseProcessInfo_()

//==============================================================================
std::list<std::pair<ots::TFMSupervisor::DAQInterfaceProcessInfo, std::unique_ptr<artdaq::CommanderInterface>>>
ots::TFMSupervisor::makeCommandersFromProcessInfo()
{
	std::list<std::pair<DAQInterfaceProcessInfo, std::unique_ptr<artdaq::CommanderInterface>>> output;
	auto                                                                                       infos = getAndParseProcessInfo_();

	for(auto& info : infos)
	{
		artdaq::Commandable cm;
		fhicl::ParameterSet ps;

		ps.put<std::string>("commanderPluginType", "xmlrpc");
		ps.put<int>("id", info.port);
		ps.put<std::string>("server_url", info.host);

		output.emplace_back(
		    std::make_pair<DAQInterfaceProcessInfo, std::unique_ptr<artdaq::CommanderInterface>>(std::move(info), artdaq::MakeCommanderPlugin(ps, cm)));
	}

	return output;
}  // end makeCommandersFromProcessInfo()

//==============================================================================
std::list<std::string> ots::TFMSupervisor::tokenize_(std::string const& input)
{
	size_t                 pos = 0;
	std::list<std::string> output;

	while(pos != std::string::npos && pos < input.size())
	{
		auto newpos = input.find('\n', pos);
		if(newpos != std::string::npos)
		{
			output.emplace_back(input, pos, newpos - pos);
			// TLOG(TLVL_TRACE) << "tokenize_: " << output.back();
			pos = newpos + 1;
		}
		else
		{
			output.emplace_back(input, pos);
			// TLOG(TLVL_TRACE) << "tokenize_: " << output.back();
			pos = newpos;
		}
	}
	return output;
}  // end tokenize_()

//==============================================================================
void ots::TFMSupervisor::daqinterfaceRunner_()
{
	TLOG(TLVL_TRACE) << "Runner thread starting";
	runner_running_ = true;
    unsigned int cnt = 0;
	while(runner_running_)
	{   
        getDAQState_();
        TLOG(TLVL_INFO) << "daq_repoers_enabled_: " << daq_repoers_enabled_ << " && (" << cnt << " % 5 == 0 :" << ((cnt) % 5 == 0) << __E__; 
        if(daq_repoers_enabled_ && ((++cnt) % 5 == 0)) {
            getDAQReports_();
            TLOG(TLVL_INFO) << getDAQReport() << __E__;
        }
		usleep(1000000); // 1s
	}
	runner_running_ = false;
	TLOG(TLVL_TRACE) << "Runner thread complete";
    
}  // end daqinterfaceRunner_()

//==============================================================================
void ots::TFMSupervisor::stop_runner_()
{
	runner_running_ = false;
	if(runner_thread_ && runner_thread_->joinable())
	{
		runner_thread_->join();
		runner_thread_.reset(nullptr);
	}
}  // end stop_runner_()

//==============================================================================
void ots::TFMSupervisor::start_runner_()
{
	stop_runner_();
	runner_thread_ = std::make_unique<std::thread>(&ots::TFMSupervisor::daqinterfaceRunner_, this);
}  // end start_runner_()

void TFMSupervisor::xmlrpc_setup() {
    // global client
    xmlrpc_env_init(&xmlrpcEnv_);
    xmlrpc_client_init2(&xmlrpcEnv_, XMLRPC_CLIENT_NO_FLAGS, "TFMSupervisor", "1.0", NULL, 0);
}

void TFMSupervisor::xmlrpc_cleanup() {
    xmlrpc_env_clean(&xmlrpcEnv_);
    xmlrpc_client_cleanup();
}


std::string TFMSupervisor::xmlrpc(const char* cmd, const char* format, const char* args) {
    xmlrpc_value * resultP;
    size_t length;
    const char* value;
    //xmlrpcClient_abort_ = 0;

    TLOG(TLVL_DEBUG) << "xmlrpc " << xmlrpcUrl_ << " " << cmd << " " << format << " " << args << __E__;
    //xmlrpc_client_call2f(&xmlrpcEnv_, xmlrpcClient_, xmlrpcUrl_, 
    //                     cmd, &resultP, format, args);
    //xmlrpc_env_init(&xmlrpcEnv_);
    resultP = xmlrpc_client_call(&xmlrpcEnv_, xmlrpcUrl_, cmd, format, args);
    TLOG(TLVL_DEBUG) << "xmlrpc done" << __E__;
    if (resultP == NULL) {
        __SS__ << "XML-RPC call failed: " << xmlrpcEnv_.fault_string << __E__;
        __SUP_SS_THROW__;
    }
    //TLOG(TLVL_DEBUG) << "xmlrpc done fault_occurred:" << xmlrpcEnv_.fault_occurred << ", fault_code:" << xmlrpcEnv_.fault_code << ", fault_string:" << xmlrpcEnv_.fault_string << __E__;
    //return "test";
    if(xmlrpcEnv_.fault_occurred) {
        __SS__ << "XML-RPC rc=" << xmlrpcEnv_.fault_code << " " << xmlrpcEnv_.fault_string << __E__;
		__SUP_SS_THROW__;
    }
    //TLOG(TLVL_DEBUG) << "xmlrpc done fault_occurred:" << xmlrpcEnv_.fault_occurred << ", fault_code:" << xmlrpcEnv_.fault_code << ", fault_string:" << xmlrpcEnv_.fault_string << __E__;

    xmlrpc_read_string_lp(&xmlrpcEnv_, resultP, &length, &value);
    std::string result = value;
    xmlrpc_DECREF(resultP);
    TLOG(TLVL_DEBUG) << "xmlrpc return: " << result << __E__;
    //xmlrpc_cv_.notify_one();

    //xmlrpc_env_clean(&xmlrpcEnv_);
    //xmlrpc_client_destroy(xmlrpcClient_);
    //xmlrpc_client_teardown_global_const();
    return result;
}

int TFMSupervisor::xmlrpc(const char* cmd, const char* format, const char* args, std::string& value) {
    try {
        value = xmlrpc(cmd, format, args);
        return EXIT_SUCCESS;
    } catch(const std::exception& e) {
        value = e.what();
        return -EXIT_FAILURE;
    }
}

/*
int TFMSupervisor::xmlrpc_timeout(const char* cmd, const char* format, const char* args, 
                          std::string& result, int timeout_ms) {
    //std::packaged_task<std::string(const char*, const char*, const char*)> task(xmlrpc);
    auto task = std::packaged_task<std::string(const char*, const char*, const char*)>(
        std::bind(&TFMSupervisor::xmlrpc, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3)
    );
    std::future<std::string> future = task.get_future();
    std::thread xmlrpcThread(std::move(task), cmd, format, args);
    //xmlrpc_client_set_interrupt(xmlrpcClient_, &xmlrpcClient_abort_);
    //xmlrpcClient_abort_ = 0;

    std::unique_lock<std::mutex> lock(xmlrpc_mtx_);
    auto status = xmlrpc_cv_.wait_for(lock, std::chrono::milliseconds(timeout_ms));

    int retval = EXIT_SUCCESS;
    if(status == std::cv_status::timeout) {
        //xmlrpcClient_abort_ = true; // abort the xmlrpc client here to avoid blocking
        result = "xmlrpc timeout";
        retval = -ETIMEDOUT;
    } else { // std::cv_status::no_timeout
        try {
                result = future.get();
            } catch (const std::exception& e) {
                TLOG(TLVL_DEBUG) << "exception caught in xmlrpc: " << e.what() << __E__;
                xmlrpcThread.join();
                retval = -EXIT_FAILURE;
            }
            xmlrpcThread.join();
            retval = EXIT_SUCCESS;
    }

    //xmlrpc_client_set_interrupt(xmlrpcClient_, NULL);
    return retval;
}*/

int TFMSupervisor::killAllRunningFarmManagers() {
    TLOG(TLVL_DEBUG) << "killAllRunningFarmManagers" << __E__;
    std::string command = "ps -aux | grep farm_manager.py | grep -v grep";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        return -1;
    }

    char buffer[1024];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }
    pclose(pipe);
    TLOG(TLVL_DEBUG) << "killAllRunningFarmManagers, buffer=" << buffer << __E__;


    // Parse the output to find the PID
    std::stringstream ss(result);
    std::string user, pidStr;
    ss >> std::ws >> user >> pidStr;
    struct passwd* pw = getpwuid(getuid());
    TLOG(TLVL_DEBUG) << "killAllRunningFarmManagers, check if same user: " << std::string(pw->pw_name) << " ?= " << user << __E__;
    if((pw != nullptr) & (std::string(pw->pw_name) == user)) {
        TLOG(TLVL_DEBUG) << "killAllRunningFarmManagers, send SIGKILL to " << pidStr << " (" << std::stoi(pidStr) << ")" << __E__;
        return kill(std::stoi(pidStr), SIGKILL);
    }
    return -1;
}

int TFMSupervisor::writeSettings(std::string configName) {
    bool generateFcls = true;
    TLOG(TLVL_DEBUG) << "writeSettings for '" << configName << "' and " << (generateFcls ? "" : "DONT") << " generate FCL files." << __E__;
    auto start = std::chrono::high_resolution_clock::now();

    char* tfm_config_dir = getenv("TFM_CONFIG_DIR");
    fs::path tfm_config_path = std::string(tfm_config_dir)+"/"+configName;
    if (!fs::exists(tfm_config_path)) {
        try {
            fs::create_directory(tfm_config_path);
            TLOG(TLVL_DEBUG) << "Folder '" << tfm_config_path << "' created." << __E__;
        } catch (const fs::filesystem_error& e) {
            __SS__ << "Error creating folder '" << tfm_config_path << "': " << e.what() << __E__;
            __SUP_SS_THROW__;
        }
    }

	std::pair<std::string , TableGroupKey> theGroup(
	    SOAPUtilities::translate(theStateMachine_.getCurrentMessage()).getParameters().getValue("ConfigurationTableGroupName"),
		    TableGroupKey(SOAPUtilities::translate(theStateMachine_.getCurrentMessage()).getParameters().getValue("ConfigurationTableGroupKey")));

	TLOG(TLVL_DEBUG) << "Configuration table group name: " << theGroup.first << " key: " << theGroup.second << __E__;

    {
	    // disable version tracking to accept untracked versions to be selected by the FSM transition source
		theConfigurationManager_->loadTableGroup(theGroup.first,
			                                     theGroup.second,
			                                     true ,
			                                     0,
			                                     0,
			                                     0,
			                                     0,
			                                     0,
			                                     0,
		                                         false,
		                                         0,
		                                         0,
			                                     ConfigurationManager::LoadGroupType::ALL_TYPES,
			                                     true );
    }

    // the FCL are generated in ARTDAQConfigurations, keep that for the moment and copy only the flattened files to the config folder
    ConfigurationTree theSupervisorNode = getSupervisorTableNode();
    ProgressBar thread_progress_bar; // not used
	auto info = ARTDAQTableBase::extractARTDAQInfo(theSupervisorNode,
	                                               false , /*getStatusFalseNodes*/
	                                               generateFcls ,  /*doWriteFHiCL*/
	                                               getSupervisorProperty("max_fragment_size_bytes", 8888),
	                                               getSupervisorProperty("routing_timeout_ms", 1999),
	                                               getSupervisorProperty("routing_retry_count", 12),
	                                               &thread_progress_bar);

	int         debugLevel  = theSupervisorNode.getNode("DAQInterfaceDebugLevel").getValue<int>();
	std::string setupScript = theSupervisorNode.getNode("DAQSetupScript").getValue();

	std::ofstream o((tfm_config_path.string()+"/settings").c_str(), std::ios::trunc);
    o << "#-------------------------------------------------------------------------------------------------" << std::endl;
    o << "# generated by OTSDAQ TFMSupervisor from " << theGroup.first << "(" << theGroup.second << ") on ";
    std::time_t now_c = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    o << std::put_time(std::localtime(&now_c), "%Y-%m-%d %H:%M:%S") << "." << std::endl;
    o << "#-------------------------------------------------------------------------------------------------" << std::endl;
    const int fw = 30; // fixed width
    o << std::left;
	o << std::setw(fw) << "debug_level"             << ": " << debugLevel;
    o << std::setw(fw)<< "# debug_level ranges from 1 to 4 in increasing level of verbosity" << std::endl;
    o << std::setw(fw) << "daq_setup_script"        << ": "  << setupScript << std::endl;
    auto top_output_dir = getSupervisorProperty("top_output_dir", std::string(__ENV__("OTS_SCRATCH")));
    o << std::setw(fw) << "top_output_dir"          << ": "  << top_output_dir << std::endl;
    o << std::setw(fw) << "log_directory"           << ": "  << top_output_dir << "/Logs" << std::endl;
    o << std::setw(fw) << "data_directory_override" << ": "  << top_output_dir << "/OutputData" << std::endl;
    o << std::setw(fw) << "record_directory"        << ": "  << top_output_dir << "/RunRecords" << std::endl;
    
	// Note that productsdir_for_bash_scripts is REQUIRED!
	__SUP_COUT__ << "Use spack is " << getSupervisorProperty("use_spack", false) << ", spack_root is "
	             << getSupervisorProperty("spack_root_for_bash_scripts", "NOT SET") << ", productsdir is "
	             << getSupervisorProperty("productsdir_for_bash_scripts", "NOT SET") << __E__;
	if(getSupervisorProperty("use_spack", false)) {
		o << std::setw(fw) << "spack_root_for_bash_scripts: " << getSupervisorProperty("spack_root_for_bash_scripts", std::string(__ENV__("SPACK_ROOT"))) << std::endl;
	}
	else {
		o << std::setw(fw) << "productsdir_for_bash_scripts: " << getSupervisorProperty("productsdir_for_bash_scripts", std::string(__ENV__("OTS_PRODUCTS"))) << std::endl;
	}

    o << std::endl;

    o << "#-------------------------------------------------------------------------------------------------" << std::endl; 
    o << "# For more information on these variables and what they mean, see the" << std::endl;                                  
    o << "# relevant section of the DAQInterface wiki," << std::endl;                           
    o << "# https://cdcvs.fnal.gov/redmine/projects/artdaq-utilities/wiki/The_settings_file_reference " << std::endl;
    o << "#-------------------------------------------------------------------------------------------------" << std::endl;


	o << std::setw(fw) << "package_hashes_to_save"         << ": " << getSupervisorProperty("package_hashes_to_save", "[]") << std::endl;
	o << std::setw(fw) << "boardreader timeout"            << ": " << getSupervisorProperty("boardreader_timeout", 30) << std::endl;
	o << std::setw(fw) << "eventbuilder timeout"           << ": " << getSupervisorProperty("eventbuilder_timeout", 30) << std::endl;
	o << std::setw(fw) << "datalogger timeout"             << ": " << getSupervisorProperty("datalogger_timeout", 30) << std::endl;
	o << std::setw(fw) << "dispatcher timeout"             << ": " << getSupervisorProperty("dispatcher_timeout", 30) << std::endl;
	o << std::setw(fw) << "routing_manager timeout"        << ": " << getSupervisorProperty("routing_manager_timeout", 30) << std::endl;
    o << std::setw(fw) << "aggregator timeout"             << ": " << getSupervisorProperty("aggregator_timeout", 30) << std::endl;
    o << std::endl;

    o << std::setw(fw) << "advanced_memory_usage: "        << ": " << std::boolalpha << getSupervisorProperty("advanced_memory_usage", false) << " # default \"false\"" << std::endl;
    o << "# If set to \"true\", max_fragment_size_bytes must not be set as both settings deal with the same thing " << std::endl;
    o << "#in mutually exclusive ways: the size of fragments and events which can pass through the artdaq system. " << std::endl;
    o << "#advanced_memory_usage allows for more sophisticated fine-tuning of these sizes, and warrants its own section. " << std::endl;
    o << "#Info is provided in the memory management details section." << std::endl;
	// Only put max_fragment_size_bytes into DAQInterface settings file if advanced_memory_usage is disabled
	//if(!getSupervisorProperty("advanced_memory_usage", false)) {
	//	o << std::setw(fw) << "max_fragment_size_bytes"    << ": " << getSupervisorProperty("max_fragment_size_bytes", 1048576) << std::endl;
	//} else {
    //    o << std::setw(fw) << "#max_fragment_size_bytes"   << ": " << getSupervisorProperty("max_fragment_size_bytes", 1048576) << std::endl;
    //}
    // but tfm complains if max_fragment_size_byte is not present, for now, add it
    o << std::setw(fw) << "max_fragment_size_bytes"    << ": " << getSupervisorProperty("max_fragment_size_bytes", 1048576) << std::endl;
    auto transfer_plugin_to_use = getSupervisorProperty("transfer_plugin_to_use", "Autodetect");
    if(transfer_plugin_to_use != "Autodetect") {
	    o << std::setw(fw) << "transfer_plugin_to_use"     << ": " <<  transfer_plugin_to_use << std::endl;
    } else {
        o << std::setw(fw) << "#transfer_plugin_to_use"    << ": " <<  transfer_plugin_to_use << std::endl;
    }
	auto disable_unique_rootfile_labels = getSupervisorProperty("disable_unique_rootfile_labels", false);
    if(disable_unique_rootfile_labels != true) {
        o << std::setw(fw) << "disable_unique_rootfile_labels" << ": " <<  disable_unique_rootfile_labels << std::endl;
    } else {
        o << std::setw(fw) << "#disable_unique_rootfile_labels" << ": " <<  disable_unique_rootfile_labels << std::endl;
    }
    o << std::setw(fw) << "use_messageviewer"              << ": " <<  std::boolalpha << getSupervisorProperty("use_messageviewer", false) << std::endl;
	auto use_messagefacility = getSupervisorProperty("use_messagefacility", true);
    if(use_messagefacility != true) {
        o << std::setw(fw) << "use_messagefacility"       << ": " <<  std::boolalpha << use_messagefacility << std::endl;
    } else {
        o << std::setw(fw) << "#use_messagefacility"      << ": " <<  std::boolalpha << use_messagefacility << std::endl;
    }
    auto fake_messagefacility = getSupervisorProperty("fake_messagefacility", false);
    if(fake_messagefacility != false) {
	    o << std::setw(fw) << "fake_messagefacility"      << ": " <<  fake_messagefacility << std::endl;
    } else {
        o << std::setw(fw) << "#fake_messagefacility"     << ": " <<  fake_messagefacility << std::endl;
    }
    o << std::setw(fw) << "validate_setup_script"         << ": " <<  0 << "# defaults=1" <<std::endl;
	
    // setting I don't know the defaults yet
    o << std::setw(fw) << "# settings I don't yet know the default yet" << std::endl;
    o << std::setw(fw) << "kill_existing_processes"       << ": " <<  std::boolalpha << getSupervisorProperty("kill_existing_processes", true) << std::endl;
    o << std::setw(fw) << "all_events_to_all_dispatchers" << ": " <<  std::boolalpha << getSupervisorProperty("all_events_to_all_dispatchers", true) << std::endl;
	//o << "max_configurations_to_list" << getSupervisorProperty("max_configurations_to_list", 10) << std::endl;
	o << std::setw(fw) << "strict_fragment_id_mode"       << ": " <<  std::boolalpha << getSupervisorProperty("strict_fragment_id_mode", false) << std::endl;
	o << std::setw(fw) << "disable_private_network_bookkeeping" << ": " << std::boolalpha << getSupervisorProperty("disable_private_network_bookkeeping", false) << std::endl;
	o << std::setw(fw) << "allowed_processors"            << ": " <<  getSupervisorProperty("allowed_processors", "0-255") << std::endl;
    
	o << std::endl;  // Note this sets a taskset for ALL processes, on all nodes (ex. "1,2,5-7")

    o << std::endl;

    o << "#-------------------------------------------------------------------------------------------------" << std::endl;
    o << "# subsystems   id    source     destination  fragmentMode " << std::endl;
    o << "#-------------------------------------------------------------------------------------------------" << std::endl;
    
	if(info.subsystems.size() > 1) {
		for(auto& ss : info.subsystems) {
			if(ss.first == 0)
				continue;
			o << "Subsystem : " << std::right << std::setw(5) << ss.first; // << std::endl;
			std::string sources = "";
            if(ss.second.sources.size() > 0) {
                for(auto& sss : ss.second.sources) {
                    sources = sources+std::to_string(sss)+",";
                    //o << "Subsystem source: " << sss << std::endl;
                }
            } else {
                sources = "none,";
            }
            o << std::right << std::setw(10) << sources.substr(0, sources.length() - 1);

			if(ss.second.destination != 0) {
				o << std::right << std::setw(16) << ss.second.destination;
			} else {
                o << std::right << std::setw(16) << "none";
            }

			if(ss.second.eventMode) {
				o << std::right << std::setw(14) << ss.second.eventMode;
			} else {
                o << std::right << std::setw(14) << "none";
            }
			o << std::endl;
		}
	}

    o << "#-------------------------------------------------------------------------------------------------" << std::endl;
    o << "#                          label                 host  port subsystem   allowed   prepend    target" << std::endl;
    o << "#                                                                      processors                 " << std::endl;
    o << "#-------------------------------------------------------------------------------------------------" << std::endl;

    std::vector<std::pair<std::string,ARTDAQTableBase::ARTDAQAppType>> appTypes {
        {"BoardReader",ARTDAQTableBase::ARTDAQAppType::BoardReader},
        {"EventBuilder",ARTDAQTableBase::ARTDAQAppType::EventBuilder},
        {"DataLogger",ARTDAQTableBase::ARTDAQAppType::DataLogger},
        {"Dispatcher",ARTDAQTableBase::ARTDAQAppType::Dispatcher},
        {"RoutingManager",ARTDAQTableBase::ARTDAQAppType::RoutingManager}
    };
    for(auto& appType : appTypes) {
        for(auto& process : info.processes[appType.second])
        {
            o << std::setw(12) << std::left << appType.first << " : " << std::right << std::setw(17) << process.label;
            o  << std::right << std::setw(21) << process.hostname;
            o  << std::right << std::setw(6) << process.port;
            o  << std::right << std::setw(10) << process.subsystem;
            if(process.allowed_processors != "") {
                o << std::right << std::setw(10) << process.allowed_processors;
            } else {
                o << std::right << std::setw(10) << "-1";
            }
            //if(process.prepend != "") {
            //    o << std::left << std::setw(10) << process.prepend;
            //} else {
                o << std::right << std::setw(10) << "none";
            //}
            //if(process.target != "") {
            //    o << std::left << std::setw(10) << process.target;
            //} else {
                o << std::right << std::setw(10) << "none";
            //}
            o << std::endl;
        }
        o << "#" << std::endl;
    }
    o << "#-------------------------------------------------------------------------------------------------" << std::endl;

    o.close();

    // copy the generated fcl files
    if(generateFcls) {
        // remove all exisitng fcl files 
        TLOG(TLVL_DEBUG) << "Remove all FCL files in '" << tfm_config_path.string() << ".";
        for (const auto& entry : fs::directory_iterator(tfm_config_path)) {
            if (entry.is_regular_file() && 
                entry.path().extension().string() == ".fcl") {
                    fs::remove(entry.path());
            }
        }
        TLOG(TLVL_DEBUG) << "Coping FCL files to '" << tfm_config_path.string() << "': ";
        for(auto& appType : appTypes) {
            for(auto& process : info.processes[appType.second]) {
                fs::copy(ARTDAQTableBase::getFlatFHICLFilename(appType.second, process.label), // source
                         tfm_config_path.string()+"/"+process.label+".fcl");
                 TLOG(TLVL_DEBUG) << process.label+".fcl, ";
            }
        }
        TLOG(TLVL_DEBUG) << __E__;
    }

    auto end_setting = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_setting - start).count();
    TLOG(TLVL_DEBUG) << "settings generation: " << duration << "ms." << __E__;

    return 0;
}

std::string TFMSupervisor::getDAQReport() {
    std::stringstream ss;
    ss << tfm_state_ << std::endl;
    for(const auto& report : daq_reports_) {
        ss << report.second << std::endl;
    }
    return ss.str();
}

void TFMSupervisor::setSupervisorPropertyDefaults()
{
	CorePropertySupervisorBase::setSupervisorProperty(
	    CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.CheckUserLockRequestTypes, "*");

} // end setSupervisorPropertyDefaults()

//==============================================================================
// forceSupervisorPropertyValues
//		override to force supervisor property values (and ignore user settings)
void TFMSupervisor::forceSupervisorPropertyValues()
{
	CorePropertySupervisorBase::setSupervisorProperty(
	    CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.AutomatedRequestTypes, "getDAQReport | getDAQState");

	//{
    //    CorePropertySupervisorBase::setSupervisorProperty(
    //        CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.UserPermissionsThreshold,
    //        "*=0 | getDAQReport=1");  // block users from writing if no write access
	//}
} //end forceSupervisorPropertyValues()

void TFMSupervisor::request(const std::string&               requestType,
                             cgicc::Cgicc&                    /*cgiIn*/,
                             HttpXmlDocument&                 xmlOut,
                             const WebUsers::RequestUserInfo& /*userInfo*/)
try
{
	if(requestType == "getDAQReport") {
        //xmlOut.addTextElementToData("DAQReport", getDAQReport());
        xmlOut.addTextElementToData("connected", (tfm_connected_ ? "true": "false"));
        xmlOut.addTextElementToData("state", tfm_state_);
        for(const auto& report : daq_reports_) {
            xmlOut.addTextElementToData(report.first, report.second);
        }
    } else if(requestType == "getDAQState") {
        xmlOut.addTextElementToData("connected", (tfm_connected_ ? "true": "false"));
        xmlOut.addTextElementToData("state", tfm_state_);
        for(const auto& app : getAndParseProcessInfo_()) {
            xmlOut.addTextElementToData(app.label, '{host: "'+app.host+'", subsystem: '+std::to_string(app.subsystem)+', port: '+std::to_string(app.port)+', state:"'+app.state+'", rank:'+std::to_string(app.rank)+'}');
        }
    } else {
		__SUP_SS__ << "requestType '" << requestType << "' request not recognized."
		           << __E__;
		__SUP_COUT__ << "\n" << ss.str();
		xmlOut.addTextElementToData("Error", ss.str());
	}
}  // end ::request()
catch(const std::runtime_error& e) {
	__SS__ << "A fatal error occurred while handling the request '" << requestType
	       << ".' Error: " << e.what() << __E__;
	__COUT_ERR__ << "\n" << ss.str();
	xmlOut.addTextElementToData("Error", ss.str());
	try {
		// always add version tracking bool
		xmlOut.addTextElementToData(
		    "versionTracking",
		    ConfigurationInterface::isVersionTrackingEnabled() ? "ON" : "OFF");
	} catch(...) {
		__COUT_ERR__ << "Error getting version tracking status!" << __E__;
	}
} // end ::request() catch
catch(...) {
	__SS__ << "An unknown fatal error occurred while handling the request '"
	       << requestType << ".'" << __E__;
	try	{ throw; } //one more try to printout extra info
	catch(const std::exception &e) {
		ss << "Exception message: " << e.what();
	}
	catch(...){}
	__COUT_ERR__ << "\n" << ss.str();
	xmlOut.addTextElementToData("Error", ss.str());
	try {
		// always add version tracking bool
		xmlOut.addTextElementToData(
		    "versionTracking",
		    ConfigurationInterface::isVersionTrackingEnabled() ? "ON" : "OFF");
	}
	catch(...) {
		__COUT_ERR__ << "Error getting version tracking status!" << __E__;
	}
} // end ::request() catch