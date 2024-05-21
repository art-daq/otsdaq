

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

using namespace ots;

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

	// Only use system Python
	// unsetenv("PYTHONPATH");
	// unsetenv("PYTHONHOME");

	// Write out settings file
    /*
    
	auto          settings_file = __ENV__("DAQINTERFACE_SETTINGS");
	std::ofstream o(settings_file, std::ios::trunc);

	setenv("DAQINTERFACE_PARTITION_NUMBER", std::to_string(partition_).c_str(), 1);
	auto logfileName = std::string(__ENV__("OTSDAQ_LOG_DIR")) + "/DAQInteface/DAQInterface_partition" + std::to_string(partition_) + ".log";
	setenv("DAQINTERFACE_LOGFILE", logfileName.c_str(), 1);

	o << "log_directory: " << getSupervisorProperty("log_directory", std::string(__ENV__("OTSDAQ_LOG_DIR"))) << std::endl;

	{
		const std::string record_directory = getSupervisorProperty("record_directory", ARTDAQTableBase::ARTDAQ_FCL_PATH + "/run_records/");
		mkdir(record_directory.c_str(), 0755);
		o << "record_directory: " << record_directory << std::endl;
	}

	o << "package_hashes_to_save: " << getSupervisorProperty("package_hashes_to_save", "[artdaq]") << std::endl;
	// Note that productsdir_for_bash_scripts is REQUIRED!
	__SUP_COUT__ << "Use spack is " << getSupervisorProperty("use_spack", false) << ", spack_root is "
	             << getSupervisorProperty("spack_root_for_bash_scripts", "NOT SET") << ", productsdir is "
	             << getSupervisorProperty("productsdir_for_bash_scripts", "NOT SET") << __E__;
	if(getSupervisorProperty("use_spack", false))
	{
		o << "spack_root_for_bash_scripts: " << getSupervisorProperty("spack_root_for_bash_scripts", std::string(__ENV__("SPACK_ROOT"))) << std::endl;
	}
	else
	{
		o << "productsdir_for_bash_scripts: " << getSupervisorProperty("productsdir_for_bash_scripts", std::string(__ENV__("OTS_PRODUCTS"))) << std::endl;
	}
	o << "boardreader timeout: " << getSupervisorProperty("boardreader_timeout", 30) << std::endl;
	o << "eventbuilder timeout: " << getSupervisorProperty("eventbuilder_timeout", 30) << std::endl;
	o << "datalogger timeout: " << getSupervisorProperty("datalogger_timeout", 30) << std::endl;
	o << "dispatcher timeout: " << getSupervisorProperty("dispatcher_timeout", 30) << std::endl;
	// Only put max_fragment_size_bytes into DAQInterface settings file if advanced_memory_usage is disabled
	if(!getSupervisorProperty("advanced_memory_usage", false))
	{
		o << "max_fragment_size_bytes: " << getSupervisorProperty("max_fragment_size_bytes", 1048576) << std::endl;
	}
	o << "transfer_plugin_to_use: " << getSupervisorProperty("transfer_plugin_to_use", "Autodetect") << std::endl;
	o << "all_events_to_all_dispatchers: " << std::boolalpha << getSupervisorProperty("all_events_to_all_dispatchers", true) << std::endl;
	o << "data_directory_override: " << getSupervisorProperty("data_directory_override", std::string(__ENV__("ARTDAQ_OUTPUT_DIR"))) << std::endl;
	o << "max_configurations_to_list: " << getSupervisorProperty("max_configurations_to_list", 10) << std::endl;
	o << "disable_unique_rootfile_labels: " << getSupervisorProperty("disable_unique_rootfile_labels", false) << std::endl;
	o << "use_messageviewer: " << std::boolalpha << getSupervisorProperty("use_messageviewer", false) << std::endl;
	o << "use_messagefacility: " << std::boolalpha << getSupervisorProperty("use_messagefacility", true) << std::endl;
	o << "fake_messagefacility: " << std::boolalpha << getSupervisorProperty("fake_messagefacility", false) << std::endl;
	o << "kill_existing_processes: " << std::boolalpha << getSupervisorProperty("kill_existing_processes", true) << std::endl;
	o << "advanced_memory_usage: " << std::boolalpha << getSupervisorProperty("advanced_memory_usage", false) << std::endl;
	o << "strict_fragment_id_mode: " << std::boolalpha << getSupervisorProperty("strict_fragment_id_mode", false) << std::endl;
	o << "disable_private_network_bookkeeping: " << std::boolalpha << getSupervisorProperty("disable_private_network_bookkeeping", false) << std::endl;
	o << "allowed_processors: " << getSupervisorProperty("allowed_processors", "0-255")
	  << std::endl;  // Note this sets a taskset for ALL processes, on all nodes (ex. "1,2,5-7")

	o.close();
    */

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
    //xmlrpc_env_clean(&xmlrpcEnv_);
    //xmlrpc_client_destroy(xmlrpcClient_);
    //xmlrpc_client_teardown_global_const();
    xmlrpc_cleanup();
	__SUP_COUT__ << "Destructed." << __E__;
}  // end destructor()

//==============================================================================
void TFMSupervisor::destroy(void)
{
	__SUP_COUT__ << "Destroying..." << __E__;
    //xmlrpc_setup();
    //auto res = xmlrpc("shutdown","(s)","daqint");
    //xmlrpc_cleanup();
    //__SUP_COUT__ << "rpc result: " << res << __E__;

     TLOG(TLVL_DEBUG) << "tfm pid:" << tfm_ << ", if >0 send SIGKILL" << __E__;
    if(tfm_ > 0) {
        kill(tfm_, SIGKILL);
    }

    //xmlrpc_env_clean(&xmlrpcEnv_);
    //xmlrpc_client_cleanup();

    /*
	if(daqinterface_ptr_ != NULL)
	{
		__SUP_COUT__ << "Calling recover transition" << __E__;
		std::lock_guard<std::recursive_mutex> lk(daqinterface_mutex_);
		PyObject*                             pName = PyUnicode_FromString("do_recover");
        */
		///*PyObject*                             res   =*/PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, NULL);
        /*

		__SUP_COUT__ << "Making sure that correct state has been reached" << __E__;
		getDAQState_();
		while(daqinterface_state_ != "stopped")
		{
			getDAQState_();
			__SUP_COUT__ << "State is " << daqinterface_state_ << ", waiting 1s and retrying..." << __E__;
			usleep(1000000);
		}

		Py_XDECREF(daqinterface_ptr_);
		daqinterface_ptr_ = NULL;
	}

	Py_Finalize();
    */

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

        //xmlrpc_setup();

        //xmlrpc_env_init(&xmlrpcEnv_);
        //xmlrpc_client_setup_global_const(&xmlrpcEnv_);
        //xmlrpc_client_create(&xmlrpcEnv_, XMLRPC_CLIENT_NO_FLAGS, "TFMSupervisor", "1.0", NULL, 0,
        //                     &xmlrpcClient_);
        //xmlrpc_client_set_interrupt(xmlrpcClient_, NULL);

        //xmlrpc_client_init2(&xmlrpcEnv_, XMLRPC_CLIENT_NO_FLAGS, "TFMSupervisor", "v1_0", NULL, 0);
        //xmlrpc_env_init(&xmlrpcEnv_);

		// initialization
        /*
		char* daqinterface_dir = getenv("ARTDAQ_DAQINTERFACE_DIR");
		if(daqinterface_dir == NULL)
		{
			__SS__ << "ARTDAQ_DAQINTERFACE_DIR environment variable not set! This "
			          "means that DAQInterface has not been setup!"
			       << __E__;
			__SUP_SS_THROW__;
		}
		else
		{
			__SUP_COUT__ << "Initializing Python" << __E__;
			Py_Initialize();

			__SUP_COUT__ << "Adding DAQInterface directory to PYTHON_PATH" << __E__;
			PyObject* sysPath     = PySys_GetObject((char*)"path");
			PyObject* programName = PyUnicode_FromString(daqinterface_dir);
			PyList_Append(sysPath, programName);
			Py_DECREF(programName);

			__SUP_COUT__ << "Creating Module name" << __E__;
			PyObject* pName = PyUnicode_FromString("rc.control.daqinterface");
            */
			/* Error checking of pName left out */
            /*
			__SUP_COUT__ << "Importing module" << __E__;
			PyObject* pModule = PyImport_Import(pName);
			Py_DECREF(pName);

			if(pModule == NULL)
			{
				PyErr_Print();
				__SS__ << "Failed to load rc.control.daqinterface" << __E__;
				__SUP_SS_THROW__;
			}
			else
			{
				__SUP_COUT__ << "Loading python module dictionary" << __E__;
				PyObject* pDict = PyModule_GetDict(pModule);
				if(pDict == NULL)
				{
					PyErr_Print();
					__SS__ << "Unable to load module dictionary" << __E__;
					__SUP_SS_THROW__;
				}
				else
				{
					Py_DECREF(pModule);

					__SUP_COUT__ << "Getting DAQInterface object pointer" << __E__;
					PyObject* di_obj_ptr = PyDict_GetItemString(pDict, "DAQInterface");

					__SUP_COUT__ << "Filling out DAQInterface args struct" << __E__;
					PyObject* pArgs = PyTuple_New(0);

					PyObject* kwargs = Py_BuildValue("{s:s, s:s, s:i, s:i, s:s, s:s}",
					                                 "logpath",
					                                 ".daqint.log",
					                                 "name",
					                                 "DAQInterface",
					                                 "partition_number",
					                                 partition_,
					                                 "rpc_port",
					                                 DAQINTERFACE_PORT,
					                                 "rpc_host",
					                                 "localhost",
					                                 "control_host",
					                                 "localhost");

					__SUP_COUT__ << "Calling DAQInterface Object Constructor" << __E__;
					daqinterface_ptr_ = PyObject_Call(di_obj_ptr, pArgs, kwargs);

					Py_DECREF(di_obj_ptr);
				}
			}
		}

		getDAQState_();

		// { //attempt to cleanup old artdaq processes DOES NOT WORK because artdaq interface knows it hasn't started
		// 	__SUP_COUT__ << "Attempting artdaq stale cleanup..." << __E__;
		// 	std::lock_guard<std::recursive_mutex> lk(daqinterface_mutex_);
		// 	getDAQState_();
		// 	__SUP_COUT__ << "Status before cleanup: " << daqinterface_state_ << __E__;

		// 	PyObject* pName = PyUnicode_FromString("do_recover");
		// 	PyObject* res   = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, NULL);

		// 	if(res == NULL)
		// 	{
		// 		PyErr_Print();
		// 		__SS__ << "Error with clean up calling do_recover" << __E__;
		// 		__SUP_SS_THROW__;
		// 	}
		// 	getDAQState_();
		// 	__SUP_COUT__ << "Status after cleanup: " << daqinterface_state_ << __E__;
		// 	__SUP_COUT__ << "cleanup DONE." << __E__;
		// }
        */
	}
    start_runner_();
	__SUP_COUT__ << "Initialized." << __E__;
}  // end init()

//==============================================================================
void TFMSupervisor::transitionConfiguring(toolbox::Event::Reference /*event*/)
{
    if(RunControlStateMachine::getIterationIndex() == 0 && RunControlStateMachine::getSubIterationIndex() == 0) { // first iteration
        __SUP_COUT__ << "transitionConfiguring" << __E__;
        std::string configName = "demo_sc";

        setenv("TFM_CONFIG_NAME",configName.c_str(), 1);
        //setenv("TFM_TOP_OUTPUT_DIR", $USER_DATA/Logs

        //std::string config_cmd = "source tfm_configure "+configName+" "+std::to_string(partition_)+"; && env";

        //TFM_SETUP_FHICLCPP = change?
        //TFM_TOP_OUTPUT_DIR = 


        /*FILE* pipe = popen(config_cmd.c_str(), "r");
        if (!pipe) {
            __SS__ << "Failed to source  tfm_configure " << configName << " " << partition_ << __E__;
            __SUP_SS_THROW__;
        }
        
        // now translate the output
        char buffer[128];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            std::string line(buffer);
            if (line[0] == '*') continue; // ignore lines starting with a * from the script
            size_t pos = line.find('=');
            if (pos != std::string::npos) {
                std::string varName = line.substr(0, pos);
                std::string varValue = line.substr(pos + 1);
                setenv(varName.c_str(), varValue.c_str(), 1);
            }
        }*/

        //set_thread_message_(config_cmd); 
        //int rst = std::system(config_cmd.c_str());
        //if(rst != 0) {
        //    __SS__ << "Failed to source tfm_configure " << configName << " " << partition_ << __E__;
        //    __SUP_SS_THROW__;
        //}
        
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
            tfm_state_ = "booting";
            daqinterface_mutex_.unlock();
            usleep(1000000); // 1s
            start_runner_();
            // start a thread that monitors the daq status every second here
            RunControlStateMachine::indicateIterationWork();
        }
    } 
	else  // not first time
	{
        if(tfm_state_ != "stopped:100") { // todo, add timeout! 
            set_thread_message_(tfm_state_); 
            usleep(500000); // 0.5s, daqinterface_state_ is updated ever 1s
            RunControlStateMachine::indicateIterationWork();
        } else { // all done
            // nothing to do, will return below
        }
    }

    //xmlrpc_setup();
    
    //__SUP_COUT__ << "DEBUG get_state:" << xmlrpc("get_state","(s)","daqint") << __E__;
    //usleep(1000000);
    //std::string res;
    //__SUP_COUT__ << "Try an xml read:" << __E__;
    //int sts = xmlrpc_timeout("get_state","(s)","daqint",res,1000);
    //__SUP_COUT__ << "xmlrpc: sts " << sts << ", string: " << res << __E__;
    //usleep(1000000);
    //sts = xmlrpc_timeout("get_state","(s)","daqint",res,1000);
    //__SUP_COUT__ << "xmlrpc: sts " << sts << ", string: " << res << __E__;


    /*

	// activate the configuration tree (the first iteration)
	if(RunControlStateMachine::getIterationIndex() == 0 && RunControlStateMachine::getSubIterationIndex() == 0)
	{
		thread_error_message_ = "";
		thread_progress_bar_.resetProgressBar(0);
		last_thread_progress_update_ = time(0);  // initialize timeout timer

		std::pair<std::string , TableGroupKey> theGroup(
		    SOAPUtilities::translate(theStateMachine_.getCurrentMessage()).getParameters().getValue("ConfigurationTableGroupName"),
		    TableGroupKey(SOAPUtilities::translate(theStateMachine_.getCurrentMessage()).getParameters().getValue("ConfigurationTableGroupKey")));

		__SUP_COUT__ << "Configuration table group name: " << theGroup.first << " key: " << theGroup.second << __E__;

		try
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
		catch(const std::runtime_error& e)
		{
			__SS__ << "Error loading table group '" << theGroup.first << "(" << theGroup.second << ")! \n" << e.what() << __E__;
			__SUP_COUT_ERR__ << ss.str();
			// ExceptionHandler(ExceptionHandlerRethrow::no, ss.str());

			//__SS_THROW_ONLY__;
			theStateMachine_.setErrorMessage(ss.str());
			throw toolbox::fsm::exception::Exception("Transition Error" ,
			                                         ss.str() ,
			                                         "TFMSupervisor::transitionConfiguring" ,
			                                         __LINE__ ,
			                                         __FUNCTION__ 
			);
		}
		catch(...)
		{
			__SS__ << "Unknown error loading table group '" << theGroup.first << "(" << theGroup.second << ")!" << __E__;
			__SUP_COUT_ERR__ << ss.str();
			// ExceptionHandler(ExceptionHandlerRethrow::no, ss.str());

			//__SS_THROW_ONLY__;
			theStateMachine_.setErrorMessage(ss.str());
			throw toolbox::fsm::exception::Exception("Transition Error" ,
			                                         ss.str() ,
			                                         "TFMSupervisor::transitionConfiguring" 
			                                         __LINE__ ,
			                                         __FUNCTION__ 
			);
		}

		// start configuring thread
		std::thread(&TFMSupervisor::configuringThread, this).detach();

		__SUP_COUT__ << "Configuring thread started." << __E__;

		RunControlStateMachine::indicateIterationWork();  // use Iteration to allow other steps to complete in the system
	}
	else  // not first time
	{
		std::string errorMessage;
		{
			std::lock_guard<std::mutex> lock(thread_mutex_);  // lock out for remainder of scope
			errorMessage = thread_error_message_;             // theStateMachine_.getErrorMessage();
		}
		int progress = thread_progress_bar_.read();
		__SUP_COUTV__(errorMessage);
		__SUP_COUTV__(progress);
		__SUP_COUTV__(thread_progress_bar_.isComplete());

		// check for done and error messages
		if(errorMessage == "" &&  // if no update in 600 seconds, give up
		   time(0) - last_thread_progress_update_ > 600)
		{
			__SUP_SS__ << "There has been no update from the configuration thread for " << (time(0) - last_thread_progress_update_)
			           << " seconds, assuming something is wrong and giving up! "
			           << "Last progress received was " << progress << __E__;
			errorMessage = ss.str();
		}

		if(errorMessage != "")
		{
			__SUP_SS__ << "Error was caught in configuring thread: " << errorMessage << __E__;
			__SUP_COUT_ERR__ << "\n" << ss.str();

			theStateMachine_.setErrorMessage(ss.str());
			throw toolbox::fsm::exception::Exception("Transition Error" 
			                                         ss.str() ,
			                                         "CoreSupervisorBase::transitionConfiguring" 
			                                         __LINE__ ,
			                                         __FUNCTION__ 
			);
		}

		if(!thread_progress_bar_.isComplete())
		{
			RunControlStateMachine::indicateIterationWork();  // use Iteration to allow other steps to complete in the system

			if(last_thread_progress_read_ != progress)
			{
				last_thread_progress_read_   = progress;
				last_thread_progress_update_ = time(0);
			}

			sleep(1 );
		}
		else
		{
			__SUP_COUT_INFO__ << "Complete configuring transition!" << __E__;
			__SUP_COUTV__(getProcessInfo_());
		}
	}

    */
	return;
}  // end transitionConfiguring()

//==============================================================================
void TFMSupervisor::configuringThread()
try
{
        std::lock_guard<std::recursive_mutex> lk(daqinterface_mutex_);
        tfm_state_ = "booting";
        daqinterface_mutex_.unlock();
        usleep(1000000); // 1s
        start_runner_();
        // start a thread that monitors the daq status every second here?

        while(tfm_state_ != "stopped:100") { // add timeout?
            try {
                //daqinterface_state_ = xmlrpc("get_state","(s)","daqint");
                thread_progress_bar_.step();
                set_thread_message_(tfm_state_); 
                //__SUP_COUT__ << "DEBUG tfm_connected_=" << tfm_connected_ << ", tfm_state_=" << tfm_state_ << __E__;
            } catch(...) {
                //
            }
            usleep(500000); // 0.5s, daqinterface_state_ is updated ever 1s
        }
        //thread_progress_bar_.complete();
    /*
	std::string uid =
	    theConfigurationManager_
	        ->getNode(ConfigurationManager::XDAQ_APPLICATION_TABLE_NAME + "/" + CorePropertySupervisorBase::getSupervisorUID() + "/" + "LinkToSupervisorTable")
	        .getValueAsString();

	__COUT__ << "Supervisor uid is " << uid << ", getting supervisor table node" << __E__;

	const std::string mfSubject_ = supervisorClassNoNamespace_ + "-" + uid;

	ConfigurationTree theSupervisorNode = getSupervisorTableNode();

	thread_progress_bar_.step();

	set_thread_message_("ConfigGen");

	auto info = ARTDAQTableBase::extractARTDAQInfo(theSupervisorNode,
	                                               false ,
	                                               true ,
	                                               getSupervisorProperty("max_fragment_size_bytes", 8888),
	                                               getSupervisorProperty("routing_timeout_ms", 1999),
	                                               getSupervisorProperty("routing_retry_count", 12),
	                                               &thread_progress_bar_);

	// Check lists
	if(info.processes.count(ARTDAQTableBase::ARTDAQAppType::BoardReader) == 0)
	{
		__GEN_SS__ << "There must be at least one enabled BoardReader!" << __E__;
		__GEN_SS_THROW__;
		return;
	}
	if(info.processes.count(ARTDAQTableBase::ARTDAQAppType::EventBuilder) == 0)
	{
		__GEN_SS__ << "There must be at least one enabled EventBuilder!" << __E__;
		__GEN_SS_THROW__;
		return;
	}

	thread_progress_bar_.step();
	set_thread_message_("Writing boot.txt");

	__GEN_COUT__ << "Writing boot.txt" << __E__;

	int         debugLevel  = theSupervisorNode.getNode("DAQInterfaceDebugLevel").getValue<int>();
	std::string setupScript = theSupervisorNode.getNode("DAQSetupScript").getValue();

	std::ofstream o(ARTDAQTableBase::ARTDAQ_FCL_PATH + "/boot.txt", std::ios::trunc);
	o << "DAQ setup script: " << setupScript << std::endl;
	o << "debug level: " << debugLevel << std::endl;
	o << std::endl;

	if(info.subsystems.size() > 1)
	{
		for(auto& ss : info.subsystems)
		{
			if(ss.first == 0)
				continue;
			o << "Subsystem id: " << ss.first << std::endl;
			if(ss.second.destination != 0)
			{
				o << "Subsystem destination: " << ss.second.destination << std::endl;
			}
			for(auto& sss : ss.second.sources)
			{
				o << "Subsystem source: " << sss << std::endl;
			}
			if(ss.second.eventMode)
			{
				o << "Subsystem fragmentMode: False" << std::endl;
			}
			o << std::endl;
		}
	}

	for(auto& builder : info.processes[ARTDAQTableBase::ARTDAQAppType::EventBuilder])
	{
		o << "EventBuilder host: " << builder.hostname << std::endl;
		o << "EventBuilder label: " << builder.label << std::endl;
		label_to_proc_type_map_[builder.label] = "EventBuilder";
		if(builder.subsystem != 1)
		{
			o << "EventBuilder subsystem: " << builder.subsystem << std::endl;
		}
		if(builder.allowed_processors != "")
		{
			o << "EventBuilder allowed_processors" << builder.allowed_processors << std::endl;
		}
		o << std::endl;
	}
	for(auto& logger : info.processes[ARTDAQTableBase::ARTDAQAppType::DataLogger])
	{
		o << "DataLogger host: " << logger.hostname << std::endl;
		o << "DataLogger label: " << logger.label << std::endl;
		label_to_proc_type_map_[logger.label] = "DataLogger";
		if(logger.subsystem != 1)
		{
			o << "DataLogger subsystem: " << logger.subsystem << std::endl;
		}
		if(logger.allowed_processors != "")
		{
			o << "DataLogger allowed_processors" << logger.allowed_processors << std::endl;
		}
		o << std::endl;
	}
	for(auto& dispatcher : info.processes[ARTDAQTableBase::ARTDAQAppType::Dispatcher])
	{
		o << "Dispatcher host: " << dispatcher.hostname << std::endl;
		o << "Dispatcher label: " << dispatcher.label << std::endl;
		o << "Dispatcher port: " << dispatcher.port << std::endl;
		label_to_proc_type_map_[dispatcher.label] = "Dispatcher";
		if(dispatcher.subsystem != 1)
		{
			o << "Dispatcher subsystem: " << dispatcher.subsystem << std::endl;
		}
		if(dispatcher.allowed_processors != "")
		{
			o << "Dispatcher allowed_processors" << dispatcher.allowed_processors << std::endl;
		}
		o << std::endl;
	}
	for(auto& rmanager : info.processes[ARTDAQTableBase::ARTDAQAppType::RoutingManager])
	{
		o << "RoutingManager host: " << rmanager.hostname << std::endl;
		o << "RoutingManager label: " << rmanager.label << std::endl;
		label_to_proc_type_map_[rmanager.label] = "RoutingManager";
		if(rmanager.subsystem != 1)
		{
			o << "RoutingManager subsystem: " << rmanager.subsystem << std::endl;
		}
		if(rmanager.allowed_processors != "")
		{
			o << "RoutingManager allowed_processors" << rmanager.allowed_processors << std::endl;
		}
		o << std::endl;
	}
	o.close();

	thread_progress_bar_.step();
	set_thread_message_("Writing Fhicl Files");

	__GEN_COUT__ << "Building configuration directory" << __E__;

	boost::system::error_code ignored;
	boost::filesystem::remove_all(ARTDAQTableBase::ARTDAQ_FCL_PATH + FAKE_CONFIG_NAME, ignored);
	mkdir((ARTDAQTableBase::ARTDAQ_FCL_PATH + FAKE_CONFIG_NAME).c_str(), 0755);

	for(auto& reader : info.processes[ARTDAQTableBase::ARTDAQAppType::BoardReader])
	{
		symlink(ARTDAQTableBase::getFlatFHICLFilename(ARTDAQTableBase::ARTDAQAppType::BoardReader, reader.label).c_str(),
		        (ARTDAQTableBase::ARTDAQ_FCL_PATH + FAKE_CONFIG_NAME + "/" + reader.label + ".fcl").c_str());
	}
	for(auto& builder : info.processes[ARTDAQTableBase::ARTDAQAppType::EventBuilder])
	{
		symlink(ARTDAQTableBase::getFlatFHICLFilename(ARTDAQTableBase::ARTDAQAppType::EventBuilder, builder.label).c_str(),
		        (ARTDAQTableBase::ARTDAQ_FCL_PATH + FAKE_CONFIG_NAME + "/" + builder.label + ".fcl").c_str());
	}
	for(auto& logger : info.processes[ARTDAQTableBase::ARTDAQAppType::DataLogger])
	{
		symlink(ARTDAQTableBase::getFlatFHICLFilename(ARTDAQTableBase::ARTDAQAppType::DataLogger, logger.label).c_str(),
		        (ARTDAQTableBase::ARTDAQ_FCL_PATH + FAKE_CONFIG_NAME + "/" + logger.label + ".fcl").c_str());
	}
	for(auto& dispatcher : info.processes[ARTDAQTableBase::ARTDAQAppType::Dispatcher])
	{
		symlink(ARTDAQTableBase::getFlatFHICLFilename(ARTDAQTableBase::ARTDAQAppType::Dispatcher, dispatcher.label).c_str(),
		        (ARTDAQTableBase::ARTDAQ_FCL_PATH + FAKE_CONFIG_NAME + "/" + dispatcher.label + ".fcl").c_str());
	}
	for(auto& rmanager : info.processes[ARTDAQTableBase::ARTDAQAppType::RoutingManager])
	{
		symlink(ARTDAQTableBase::getFlatFHICLFilename(ARTDAQTableBase::ARTDAQAppType::RoutingManager, rmanager.label).c_str(),
		        (ARTDAQTableBase::ARTDAQ_FCL_PATH + FAKE_CONFIG_NAME + "/" + rmanager.label + ".fcl").c_str());
	}

	thread_progress_bar_.step();

	std::lock_guard<std::recursive_mutex> lk(daqinterface_mutex_);
	getDAQState_();
	if(daqinterface_state_ != "stopped" && daqinterface_state_ != "")
	{
		__GEN_SS__ << "Cannot configure DAQInterface because it is in the wrong state"
		           << " (" << daqinterface_state_ << " != stopped)!" << __E__;
		__GEN_SS_THROW__
	}

	set_thread_message_("Calling setdaqcomps");
	__GEN_COUT__ << "Calling setdaqcomps" << __E__;
	__GEN_COUT__ << "Status before setdaqcomps: " << daqinterface_state_ << __E__;
	PyObject* pName1 = PyUnicode_FromString("setdaqcomps");

	PyObject* readerDict = PyDict_New();
	for(auto& reader : info.processes[ARTDAQTableBase::ARTDAQAppType::BoardReader])
	{
		label_to_proc_type_map_[reader.label] = "BoardReader";
		PyObject* readerName = PyUnicode_FromString(reader.label.c_str());

		int list_size = reader.allowed_processors != "" ? 4 : 3;

		PyObject* readerData      = PyList_New(list_size);
		PyObject* readerHost      = PyUnicode_FromString(reader.hostname.c_str());
		PyObject* readerPort      = PyUnicode_FromString("-1");
		PyObject* readerSubsystem = PyUnicode_FromString(std::to_string(reader.subsystem).c_str());
		PyList_SetItem(readerData, 0, readerHost);
		PyList_SetItem(readerData, 1, readerPort);
		PyList_SetItem(readerData, 2, readerSubsystem);
		if(reader.allowed_processors != "")
		{
			PyObject* readerAllowedProcessors = PyUnicode_FromString(reader.allowed_processors.c_str());
			PyList_SetItem(readerData, 3, readerAllowedProcessors);
		}
		PyDict_SetItem(readerDict, readerName, readerData);
	}
	PyObject* res1 = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName1, readerDict, NULL);
	Py_DECREF(readerDict);

	if(res1 == NULL)
	{
		PyErr_Print();
		__GEN_SS__ << "Error calling setdaqcomps transition" << __E__;
		__GEN_SS_THROW__;
	}
	getDAQState_();
	__GEN_COUT__ << "Status after setdaqcomps: " << daqinterface_state_ << __E__;

	thread_progress_bar_.step();
	set_thread_message_("Calling do_boot");
	__GEN_COUT__ << "Calling do_boot" << __E__;
	__GEN_COUT__ << "Status before boot: " << daqinterface_state_ << __E__;
	PyObject* pName2      = PyUnicode_FromString("do_boot");
	PyObject* pStateArgs1 = PyUnicode_FromString((ARTDAQTableBase::ARTDAQ_FCL_PATH + "/boot.txt").c_str());
	PyObject* res2        = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName2, pStateArgs1, NULL);

	if(res2 == NULL)
	{
		PyErr_Print();
		__GEN_COUT__ << "Error on first boost attempt, recovering and retrying" << __E__;

		PyObject* pName = PyUnicode_FromString("do_recover");
		PyObject* res   = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, NULL);

		if(res == NULL)
		{
			PyErr_Print();
			__GEN_SS__ << "Error calling recover transition!!!!" << __E__;
			__GEN_SS_THROW__;
		}

		thread_progress_bar_.step();
		set_thread_message_("Calling do_boot (retry)");
		__GEN_COUT__ << "Calling do_boot again" << __E__;
		__GEN_COUT__ << "Status before boot: " << daqinterface_state_ << __E__;
		PyObject* res3 = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName2, pStateArgs1, NULL);

		if(res3 == NULL)
		{
			PyErr_Print();
			__GEN_SS__ << "Error calling boot transition (2nd try)" << __E__;
			__GEN_SS_THROW__;
		}
	}

	getDAQState_();
	if(daqinterface_state_ != "booted")
	{
		__GEN_SS__ << "DAQInterface boot transition failed! "
		           << "Status after boot attempt: " << daqinterface_state_ << __E__;
		__GEN_SS_THROW__;
	}
	__GEN_COUT__ << "Status after boot: " << daqinterface_state_ << __E__;

	thread_progress_bar_.step();
	set_thread_message_("Calling do_config");
	__GEN_COUT__ << "Calling do_config" << __E__;
	__GEN_COUT__ << "Status before config: " << daqinterface_state_ << __E__;
	PyObject* pName3      = PyUnicode_FromString("do_config");
	PyObject* pStateArgs2 = Py_BuildValue("[s]", FAKE_CONFIG_NAME);
	PyObject* res3        = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName3, pStateArgs2, NULL);

	if(res3 == NULL)
	{
		PyErr_Print();
		__GEN_SS__ << "Error calling config transition" << __E__;
		__GEN_SS_THROW__;
	}
	getDAQState_();
	if(daqinterface_state_ != "ready")
	{
		__GEN_SS__ << "DAQInterface config transition failed!" << __E__ << "Supervisor state: \"" << daqinterface_state_ << "\" != \"ready\" " << __E__;
		__GEN_SS_THROW__;
	}
	__GEN_COUT__ << "Status after config: " << daqinterface_state_ << __E__;
	thread_progress_bar_.complete();
	set_thread_message_("Configured");
	__GEN_COUT__ << "Configured." << __E__;
    */
   thread_progress_bar_.complete();
   set_thread_message_("Configured");
}  // end configuringThread()
catch(const std::runtime_error& e)
{
	set_thread_message_("ERROR");
	__SS__ << "Error was caught while configuring: " << e.what() << __E__;
	__COUT_ERR__ << "\n" << ss.str();
	std::lock_guard<std::mutex> lock(thread_mutex_);  // lock out for remainder of scope
	thread_error_message_ = ss.str();
}
catch(...)
{
	set_thread_message_("ERROR");
	__SS__ << "Unknown error was caught while configuring. Please checked the logs." << __E__;
	__COUT_ERR__ << "\n" << ss.str();

	artdaq::ExceptionHandler(artdaq::ExceptionHandlerRethrow::no, ss.str());

	std::lock_guard<std::mutex> lock(thread_mutex_);  // lock out for remainder of scope
	thread_error_message_ = ss.str();
}  // end configuringThread() error handling

//==============================================================================
void TFMSupervisor::transitionHalting(toolbox::Event::Reference /*event*/)
//try
{
    __SUP_COUT__ << "transitionHalting" << __E__;
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
}

    //usleep(100000);

    //if(tfm_ > 0) {
    //    __SUP_COUT__ << "Sending SIGTERM to pid " << tfm_ << __E__;
    //    kill(tfm_, SIGINT);
    //}

    /*
	set_thread_message_("Halting");
	__SUP_COUT__ << "Halting..." << __E__;
	std::lock_guard<std::recursive_mutex> lk(daqinterface_mutex_);
	getDAQState_();
	__SUP_COUT__ << "Status before halt: " << daqinterface_state_ << __E__;

	if(daqinterface_state_ == "running")
	{
		// First stop before halting
		PyObject* pName = PyUnicode_FromString("do_stop_running");
		PyObject* res   = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, NULL);

		if(res == NULL)
		{
			PyErr_Print();
			__SS__ << "Error calling  DAQ Interface stop transition." << __E__;
			__SUP_SS_THROW__;
		}
	}

	PyObject* pName = PyUnicode_FromString("do_command");
	PyObject* pArg  = PyUnicode_FromString("Shutdown");
	PyObject* res   = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, pArg, NULL);

	if(res == NULL)
	{
		PyErr_Print();
		__SS__ << "Error calling DAQ Interface halt transition." << __E__;
		__SUP_SS_THROW__;
	}

	getDAQState_();
	__SUP_COUT__ << "Status after halt: " << daqinterface_state_ << __E__;
	__SUP_COUT__ << "Halted." << __E__;
	set_thread_message_("Halted");
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
		throw toolbox::fsm::exception::Exception("Transition Error" ,
		                                         ss.str() ,
		                                         "TFMSupervisorBase::transition" + transitionName 
		                                         __LINE__ 
		                                         __FUNCTION__ 
		);
	}
    */
   /*
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

		throw toolbox::fsm::exception::Exception("Transition Error" ,
		                                         ss.str() ,
		                                         "ARTDAQSupervisorBase::transition" + transitionName ,
		                                         __LINE__ ,
		                                         __FUNCTION__ 
		);
	}*/
//}  // end transitionHalting() exception handling

//==============================================================================
void TFMSupervisor::transitionInitializing(toolbox::Event::Reference /*event*/)
try
{
    /*
	set_thread_message_("Initializing");
	__SUP_COUT__ << "Initializing..." << __E__;
	init();
	__SUP_COUT__ << "Initialized." << __E__;
	set_thread_message_("Initialized");
    */
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
    /*
	set_thread_message_("Pausing");
	__SUP_COUT__ << "Pausing..." << __E__;
	std::lock_guard<std::recursive_mutex> lk(daqinterface_mutex_);

	getDAQState_();
	__SUP_COUT__ << "Status before pause: " << daqinterface_state_ << __E__;

	PyObject* pName = PyUnicode_FromString("do_command");
	PyObject* pArg  = PyUnicode_FromString("Pause");
	PyObject* res   = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, pArg, NULL);

	if(res == NULL)
	{
		PyErr_Print();
		__SS__ << "Error calling DAQ Interface Pause transition." << __E__;
		__SUP_SS_THROW__;
	}

	getDAQState_();
	__SUP_COUT__ << "Status after pause: " << daqinterface_state_ << __E__;

	__SUP_COUT__ << "Paused." << __E__;
	set_thread_message_("Paused");
    */
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
    /*
	set_thread_message_("Resuming");
	__SUP_COUT__ << "Resuming..." << __E__;
	std::lock_guard<std::recursive_mutex> lk(daqinterface_mutex_);

	getDAQState_();
	__SUP_COUT__ << "Status before resume: " << daqinterface_state_ << __E__;
	PyObject* pName = PyUnicode_FromString("do_command");
	PyObject* pArg  = PyUnicode_FromString("Resume");
	PyObject* res   = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, pArg, NULL);

	if(res == NULL)
	{
		PyErr_Print();
		__SS__ << "Error calling DAQ Interface Resume transition." << __E__;
		__SUP_SS_THROW__;
	}
	getDAQState_();
	__SUP_COUT__ << "Status after resume: " << daqinterface_state_ << __E__;
	__SUP_COUT__ << "Resumed." << __E__;
	set_thread_message_("Resumed");
    */
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
    //auto runNumber = SOAPUtilities::translate(theStateMachine_.getCurrentMessage()).getParameters().getValue("RunNumber");
    int runNumber = 10;
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
        RunControlStateMachine::indicateIterationWork();
    } else if(RunControlStateMachine::getIterationIndex() == 1) {
        // wait until the tfm is configured
        if(tfm_state_ != "configured:100") { // todo, add timeout! 
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
            set_thread_message_(tfm_state_); 
            usleep(500000); // 0.5s, daqinterface_state_ is updated ever 1s
            //RunControlStateMachine::indicateSubIterationWork();
            RunControlStateMachine::indicateIterationWork();
        } else {
            set_thread_message_(tfm_state_);
            // nothing to do, all done
        }
    }



    /*
	// first time launch thread because artdaq Supervisor may take a while
	if(RunControlStateMachine::getIterationIndex() == 0 && RunControlStateMachine::getSubIterationIndex() == 0)
	{
		thread_error_message_ = "";
		thread_progress_bar_.resetProgressBar(0);
		last_thread_progress_update_ = time(0);  // initialize timeout timer

		// start configuring thread
		std::thread(&TFMSupervisor::startingThread, this).detach();

		__SUP_COUT__ << "Starting thread started." << __E__;

		RunControlStateMachine::indicateIterationWork();  // use Iteration to allow other steps to complete in the system
	}
	else  // not first time
	{
		std::string errorMessage;
		{
			std::lock_guard<std::mutex> lock(thread_mutex_);  // lock out for remainder of scope
			errorMessage = thread_error_message_;             // theStateMachine_.getErrorMessage();
		}
		int progress = thread_progress_bar_.read();
		__SUP_COUTV__(errorMessage);
		__SUP_COUTV__(progress);
		__SUP_COUTV__(thread_progress_bar_.isComplete());

		// check for done and error messages
		if(errorMessage == "" &&  // if no update in 600 seconds, give up
		   time(0) - last_thread_progress_update_ > 600)
		{
			__SUP_SS__ << "There has been no update from the start thread for " << (time(0) - last_thread_progress_update_)
			           << " seconds, assuming something is wrong and giving up! "
			           << "Last progress received was " << progress << __E__;
			errorMessage = ss.str();
		}

		if(errorMessage != "")
		{
			__SUP_SS__ << "Error was caught in starting thread: " << errorMessage << __E__;
			__SUP_COUT_ERR__ << "\n" << ss.str();

			theStateMachine_.setErrorMessage(ss.str());
			throw toolbox::fsm::exception::Exception("Transition Error" ,
			                                         ss.str() ,
			                                         "CoreSupervisorBase::transitionStarting" ,
			                                         __LINE__ ,
			                                         __FUNCTION__ 
			);
		}

		if(!thread_progress_bar_.isComplete())
		{
			RunControlStateMachine::indicateIterationWork();  // use Iteration to allow other steps to complete in the system

			if(last_thread_progress_read_ != progress)
			{
				last_thread_progress_read_   = progress;
				last_thread_progress_update_ = time(0);
			}

			sleep(1 );
		}
		else
		{
			__SUP_COUT_INFO__ << "Complete starting transition!" << __E__;
			__SUP_COUTV__(getProcessInfo_());
		}
	}
    */
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
void TFMSupervisor::startingThread()
try
{
    /*
	std::string uid =
	    theConfigurationManager_
	        ->getNode(ConfigurationManager::XDAQ_APPLICATION_TABLE_NAME + "/" + CorePropertySupervisorBase::getSupervisorUID() + "/" + "LinkToSupervisorTable")
	        .getValueAsString();

	__COUT__ << "Supervisor uid is " << uid << ", getting supervisor table node" << __E__;
	const std::string mfSubject_ = supervisorClassNoNamespace_ + "-" + uid;
	__GEN_COUT__ << "Starting..." << __E__;
	set_thread_message_("Starting");

	thread_progress_bar_.step();
	stop_runner_();
	{
		std::lock_guard<std::recursive_mutex> lk(daqinterface_mutex_);
		getDAQState_();
		__GEN_COUT__ << "Status before start: " << daqinterface_state_ << __E__;
		auto runNumber = SOAPUtilities::translate(theStateMachine_.getCurrentMessage()).getParameters().getValue("RunNumber");

		thread_progress_bar_.step();

		PyObject* pName      = PyUnicode_FromString("do_start_running");
		int       run_number = std::stoi(runNumber);
		PyObject* pStateArgs = PyLong_FromLong(run_number);
		PyObject* res        = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, pStateArgs, NULL);

		thread_progress_bar_.step();

		if(res == NULL)
		{
			PyErr_Print();
			__SS__ << "Error calling start transition" << __E__;
			__GEN_SS_THROW__;
		}
		getDAQState_();

		thread_progress_bar_.step();

		__GEN_COUT__ << "Status after start: " << daqinterface_state_ << __E__;
		if(daqinterface_state_ != "running")
		{
			__SS__ << "DAQInterface start transition failed!" << __E__;
			__GEN_SS_THROW__;
		}

		thread_progress_bar_.step();
	}
	start_runner_();
	set_thread_message_("Started");
	thread_progress_bar_.step();

	__GEN_COUT__ << "Started." << __E__;
	thread_progress_bar_.complete();

*/
}  // end startingThread()
catch(const std::runtime_error& e)
{
	__SS__ << "Error was caught while Starting: " << e.what() << __E__;
	__COUT_ERR__ << "\n" << ss.str();
	std::lock_guard<std::mutex> lock(thread_mutex_);  // lock out for remainder of scope
	thread_error_message_ = ss.str();
}
catch(...)
{
	__SS__ << "Unknown error was caught while Starting. Please checked the logs." << __E__;
	__COUT_ERR__ << "\n" << ss.str();

	artdaq::ExceptionHandler(artdaq::ExceptionHandlerRethrow::no, ss.str());

	std::lock_guard<std::mutex> lock(thread_mutex_);  // lock out for remainder of scope
	thread_error_message_ = ss.str();
}  // end startingThread() error handling

//==============================================================================
void TFMSupervisor::transitionStopping(toolbox::Event::Reference /*event*/)
try
{
    if(RunControlStateMachine::getIterationIndex() == 0 && RunControlStateMachine::getSubIterationIndex() == 0) {
        __SUP_COUT__ << "transitionStopping" << __E__;
        auto resultP = xmlrpc_client_call(&xmlrpcEnv_, xmlrpcUrl_, 
                            "state_change","(ss{s:i})","daqint","stopping","ignored_variable", 999);
        if (resultP == NULL) {
            __SS__ << "XML-RPC call failed: " << xmlrpcEnv_.fault_string << __E__;
            __SUP_SS_THROW__;
        }
        RunControlStateMachine::indicateIterationWork();
    } else {
        if(tfm_state_ != "stopped:100") { // todo, add timeout
            set_thread_message_(tfm_state_); 
            usleep(500000); // 0.5s, daqinterface_state_ is updated ever 1s
            RunControlStateMachine::indicateIterationWork();
        } else {
            set_thread_message_(tfm_state_);
            // nothing to do, all done
        }
    }

    /*
	__SUP_COUT__ << "Stopping..." << __E__;
	set_thread_message_("Stopping");
	std::lock_guard<std::recursive_mutex> lk(daqinterface_mutex_);
	getDAQState_();
	__SUP_COUT__ << "Status before stop: " << daqinterface_state_ << __E__;
	PyObject* pName = PyUnicode_FromString("do_stop_running");
	PyObject* res   = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, NULL);

	if(res == NULL)
	{
		PyErr_Print();
		__SS__ << "Error calling DAQ Interface  stop transition." << __E__;
		__SUP_SS_THROW__;
	}
	getDAQState_();
	__SUP_COUT__ << "Status after stop: " << daqinterface_state_ << __E__;
	__SUP_COUT__ << "Stopped." << __E__;
	set_thread_message_("Stopped");
    */
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
    /*
	__SUP_COUT__ << "Entering error recovery state" << __E__;
	std::lock_guard<std::recursive_mutex> lk(daqinterface_mutex_);
	getDAQState_();
	__SUP_COUT__ << "Status before error: " << daqinterface_state_ << __E__;

	PyObject* pName = PyUnicode_FromString("do_recover");
	PyObject* res   = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, NULL);

	if(res == NULL)
	{
		PyErr_Print();
		__SS__ << "Error calling DAQ Interface recover transition." << __E__;
		__SUP_SS_THROW__;
	}
	getDAQState_();
	__SUP_COUT__ << "Status after error: " << daqinterface_state_ << __E__;
	__SUP_COUT__ << "EnteringError DONE." << __E__;

    */
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
        tfm_state_ = state;
        tfm_connected_ = 1;
    } else {
        tfm_connected_ = 0;
    }


    //int rc = xmlrpc("shutdown","(s)","daqint", state);
    //if(rc == 0) {
    //    daqinterface_state_ = state;
    //} else {
    //    __SS__ << "Error calling state function" << __E__;
    //    __SUP_SS_THROW__;
    //    return;
    //}

    /*
	//__SUP_COUT__ << "Getting DAQInterface state" << __E__;
	std::lock_guard<std::recursive_mutex> lk(daqinterface_mutex_);

	if(daqinterface_ptr_ == nullptr)
	{
		daqinterface_state_ = "";
		return;
	}

	PyObject* pName = PyUnicode_FromString("state");
	PyObject* pArg  = PyUnicode_FromString("DAQInterface");
	PyObject* res   = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, pArg, NULL);

	if(res == NULL)
	{
		PyErr_Print();
		__SS__ << "Error calling state function" << __E__;
		__SUP_SS_THROW__;
		return;
	}
	daqinterface_state_ = std::string(PyUnicode_AsUTF8(res));
	//__SUP_COUT__ << "getDAQState_ DONE: state=" << result << __E__;
    */
}  // end getDAQState_()

//==============================================================================
std::string ots::TFMSupervisor::getProcessInfo_(void)
{
    return xmlrpc("artdaq_process_info","(s)","daqint");
    /*
	//__SUP_COUT__ << "Getting DAQInterface state" << __E__;
	std::lock_guard<std::recursive_mutex> lk(daqinterface_mutex_);

	if(daqinterface_ptr_ == nullptr)
	{
		return "";
	}

	PyObject* pName = PyUnicode_FromString("artdaq_process_info");
	PyObject* pArg  = PyUnicode_FromString("DAQInterface");
	PyObject* pArg2 = PyBool_FromLong(true);
	PyObject* res   = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, pArg, pArg2, NULL);

	if(res == NULL)
	{
		PyErr_Print();
		__SS__ << "Error calling artdaq_process_info function" << __E__;
		__SUP_SS_THROW__;
		return "";
	}
	return std::string(PyUnicode_AsUTF8(res));
	//__SUP_COUT__ << "getDAQState_ DONE: state=" << result << __E__;
    */
    //return "";
}  // end getProcessInfo_()

std::string ots::TFMSupervisor::artdaqStateToOtsState(std::string state)
{
    /*
	if(state == "nonexistant")
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
	return RunControlStateMachine::FAILED_STATE_NAME;
    */
   return state;
}

std::string ots::TFMSupervisor::labelToProcType_(std::string label)
{
    /*
	if(label_to_proc_type_map_.count(label))
	{
		return label_to_proc_type_map_[label];
	}
    */
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
	while(runner_running_)
	{   
        getDAQState_();
        /*
		if(daqinterface_ptr_ != NULL)
		{
			std::unique_lock<std::recursive_mutex> lk(daqinterface_mutex_);
			getDAQState_();
			std::string state_before = daqinterface_state_;

			if(daqinterface_state_ == "running" || daqinterface_state_ == "ready" || daqinterface_state_ == "booted")
			{
				try
				{
					TLOG(TLVL_TRACE) << "Calling DAQInterface::check_proc_heartbeats";
					PyObject* pName = PyUnicode_FromString("check_proc_heartbeats");
					PyObject* res   = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, NULL);
					TLOG(TLVL_TRACE) << "Done with DAQInterface::check_proc_heartbeats call";

					if(res == NULL)
					{
						runner_running_ = false;
						PyErr_Print();
						__SS__ << "Error calling check_proc_heartbeats function" << __E__;
						__SUP_SS_THROW__;
						break;
					}
				}
				catch(cet::exception& ex)
				{
					runner_running_ = false;
					PyErr_Print();
					__SS__ << "An cet::exception occurred while calling "
					          "check_proc_heartbeats function: "
					       << ex.explain_self() << __E__;
					__SUP_SS_THROW__;
					break;
				}
				catch(std::exception& ex)
				{
					runner_running_ = false;
					PyErr_Print();
					__SS__ << "An std::exception occurred while calling "
					          "check_proc_heartbeats function: "
					       << ex.what() << __E__;
					__SUP_SS_THROW__;
					break;
				}
				catch(...)
				{
					runner_running_ = false;
					PyErr_Print();
					__SS__ << "An unknown Error occurred while calling runner function" << __E__;
					__SUP_SS_THROW__;
					break;
				}

				lk.unlock();
				getDAQState_();
				if(daqinterface_state_ != state_before)
				{
					runner_running_ = false;
					lk.unlock();
					__SS__ << "DAQInterface state unexpectedly changed from " << state_before << " to " << daqinterface_state_
					       << ". Check supervisor log file for more info!" << __E__;
					__SUP_SS_THROW__;
					break;
				}
			}
		}
		else
		{
			break;
		}
        */
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
    /*xmlrpc_env_init(&xmlrpcEnv_);
    xmlrpc_client_setup_global_const(&xmlrpcEnv_);
    xmlrpc_client_create(&xmlrpcEnv_, XMLRPC_CLIENT_NO_FLAGS, "TFMSupervisor", "1.0", NULL, 0,
                             &xmlrpcClient_);
    */
    // global client
    xmlrpc_env_init(&xmlrpcEnv_);
    xmlrpc_client_init2(&xmlrpcEnv_, XMLRPC_CLIENT_NO_FLAGS, "TFMSupervisor", "1.0", NULL, 0);
}

void TFMSupervisor::xmlrpc_cleanup() {
    xmlrpc_env_clean(&xmlrpcEnv_);
    xmlrpc_client_cleanup();

    //xmlrpc_client_destroy(xmlrpcClient_);
    //xmlrpc_client_teardown_global_const();
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