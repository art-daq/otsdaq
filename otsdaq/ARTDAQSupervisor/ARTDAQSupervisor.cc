

#define TRACEMF_USE_VERBATIM 1  // for trace longer path filenames
#include "otsdaq/ARTDAQSupervisor/ARTDAQSupervisor.hh"

#include "artdaq-core/Utilities/configureMessageFacility.hh"
#include "artdaq/BuildInfo/GetPackageBuildInfo.hh"
#include "artdaq/DAQdata/Globals.hh"
#include "artdaq/ExternalComms/MakeCommanderPlugin.hh"
#include "cetlib_except/exception.h"
#include "fhiclcpp/make_ParameterSet.h"
#include "otsdaq/ARTDAQSupervisor/ARTDAQSupervisorTRACEController.h"

#include "artdaq-core/Utilities/ExceptionHandler.hh" /*for artdaq::ExceptionHandler*/

#include <boost/exception/all.hpp>
#include <boost/filesystem.hpp>

#include <signal.h>
#include <regex>

#define OUT_ON_ERR_SIZE 2000  //tail size of output to include on error

using namespace ots;

XDAQ_INSTANTIATOR_IMPL(ARTDAQSupervisor)

#define FAKE_CONFIG_NAME "ots_config"
#define DAQINTERFACE_PORT                    \
	std::atoi(__ENV__("ARTDAQ_BASE_PORT")) + \
	    (partition_ * std::atoi(__ENV__("ARTDAQ_PORTS_PER_PARTITION")))

static ARTDAQSupervisor*                         instance = nullptr;
static std::unordered_map<int, struct sigaction> old_actions =
    std::unordered_map<int, struct sigaction>();
static bool sighandler_init = false;
static void signal_handler(int signum)
{
	// Messagefacility may already be gone at this point, TRACE ONLY!
#if TRACE_REVNUM < 1459
	TRACE_STREAMER(TLVL_ERROR, &("ARTDAQsupervisor")[0], 0, 0, 0)
#else
	TRACE_STREAMER(TLVL_ERROR, TLOG2("ARTDAQsupervisor", 0), 0)
#endif
	    << "A signal of type " << signum
	    << " was caught by ARTDAQSupervisor. Shutting down DAQInterface, "
	       "then proceeding with default handlers!";

	if(instance)
		instance->destroy();

	sigset_t set;
	pthread_sigmask(SIG_UNBLOCK, NULL, &set);
	pthread_sigmask(SIG_UNBLOCK, &set, NULL);

#if TRACE_REVNUM < 1459
	TRACE_STREAMER(TLVL_ERROR, &("ARTDAQsupervisor")[0], 0, 0, 0)
#else
	TRACE_STREAMER(TLVL_ERROR, TLOG2("ARTDAQsupervisor", 0), 0)
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

static void init_sighandler(ARTDAQSupervisor* inst)
{
	static std::mutex            sighandler_mutex;
	std::unique_lock<std::mutex> lk(sighandler_mutex);

	if(!sighandler_init)
	{
		sighandler_init          = true;
		instance                 = inst;
		std::vector<int> signals = {
		    SIGINT,
		    SIGILL,
		    SIGABRT,
		    SIGFPE,
		    SIGSEGV,
		    SIGPIPE,
		    SIGALRM,
		    SIGTERM,
		    SIGUSR2,
		    SIGHUP};  // SIGQUIT is used by art in normal operation
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

//==============================================================================
ARTDAQSupervisor::ARTDAQSupervisor(xdaq::ApplicationStub* stub)
    : CoreSupervisorBase(stub)
    , daqinterface_ptr_(NULL)
    , partition_(getSupervisorProperty("partition", 0))
    , daqinterface_state_("notrunning")
    , runner_thread_(nullptr)
{
	__SUP_COUT__ << "Constructor." << __E__;

	INIT_MF("." /*directory used is USER_DATA/LOG/.*/);
	init_sighandler(this);

	// Only use system Python
	// unsetenv("PYTHONPATH");
	// unsetenv("PYTHONHOME");

	// Write out settings file
	auto          settings_file = __ENV__("DAQINTERFACE_SETTINGS");
	std::ofstream o(settings_file, std::ios::trunc);

	setenv("DAQINTERFACE_PARTITION_NUMBER", std::to_string(partition_).c_str(), 1);
	auto logfileName = std::string(__ENV__("OTSDAQ_LOG_DIR")) +
	                   "/DAQInteface/DAQInterface_partition" +
	                   std::to_string(partition_) + ".log";
	setenv("DAQINTERFACE_LOGFILE", logfileName.c_str(), 1);

	o << "log_directory: "
	  << getSupervisorProperty("log_directory", std::string(__ENV__("OTSDAQ_LOG_DIR")))
	  << std::endl;

	{
		const std::string record_directory = getSupervisorProperty(
		    "record_directory", ARTDAQTableBase::ARTDAQ_FCL_PATH + "/run_records/");
		mkdir(record_directory.c_str(), 0755);
		o << "record_directory: " << record_directory << std::endl;
	}

	o << "package_hashes_to_save: "
	  << getSupervisorProperty("package_hashes_to_save", "[artdaq]") << std::endl;

	o << "spack_root_for_bash_scripts: "
	  << getSupervisorProperty("spack_root_for_bash_scripts",
	                           std::string(__ENV__("SPACK_ROOT")))
	  << std::endl;
	o << "boardreader timeout: " << getSupervisorProperty("boardreader_timeout", 30)
	  << std::endl;
	o << "eventbuilder timeout: " << getSupervisorProperty("eventbuilder_timeout", 30)
	  << std::endl;
	o << "datalogger timeout: " << getSupervisorProperty("datalogger_timeout", 30)
	  << std::endl;
	o << "dispatcher timeout: " << getSupervisorProperty("dispatcher_timeout", 30)
	  << std::endl;
	// Only put max_fragment_size_bytes into DAQInterface settings file if advanced_memory_usage is disabled
	if(!getSupervisorProperty("advanced_memory_usage", false))
	{
		o << "max_fragment_size_bytes: "
		  << getSupervisorProperty("max_fragment_size_bytes", 1048576) << std::endl;
	}
	o << "transfer_plugin_to_use: "
	  << getSupervisorProperty("transfer_plugin_to_use", "TCPSocket") << std::endl;
	if(getSupervisorProperty("transfer_plugin_from_brs", "") != "")
	{
		o << "transfer_plugin_from_brs: "
		  << getSupervisorProperty("transfer_plugin_from_brs", "") << std::endl;
	}
	if(getSupervisorProperty("transfer_plugin_from_ebs", "") != "")
	{
		o << "transfer_plugin_from_ebs: "
		  << getSupervisorProperty("transfer_plugin_from_ebs", "") << std::endl;
	}
	if(getSupervisorProperty("transfer_plugin_from_dls", "") != "")
	{
		o << "transfer_plugin_from_dls: "
		  << getSupervisorProperty("transfer_plugin_from_dls", "") << std::endl;
	}
	o << "all_events_to_all_dispatchers: " << std::boolalpha
	  << getSupervisorProperty("all_events_to_all_dispatchers", true) << std::endl;
	if(getSupervisorProperty("data_directory_override", "") != "")
	{
		o << "data_directory_override: "
		  << getSupervisorProperty("data_directory_override", "") << std::endl;
	}
	o << "max_configurations_to_list: "
	  << getSupervisorProperty("max_configurations_to_list", 10) << std::endl;
	o << "disable_unique_rootfile_labels: "
	  << getSupervisorProperty("disable_unique_rootfile_labels", false) << std::endl;
	o << "use_messageviewer: " << std::boolalpha
	  << getSupervisorProperty("use_messageviewer", false) << std::endl;
	o << "use_messagefacility: " << std::boolalpha
	  << getSupervisorProperty("use_messagefacility", true) << std::endl;
	o << "fake_messagefacility: " << std::boolalpha
	  << getSupervisorProperty("fake_messagefacility", false) << std::endl;
	o << "kill_existing_processes: " << std::boolalpha
	  << getSupervisorProperty("kill_existing_processes", true) << std::endl;
	o << "advanced_memory_usage: " << std::boolalpha
	  << getSupervisorProperty("advanced_memory_usage", false) << std::endl;
	o << "strict_fragment_id_mode: " << std::boolalpha
	  << getSupervisorProperty("strict_fragment_id_mode", false) << std::endl;
	o << "disable_private_network_bookkeeping: " << std::boolalpha
	  << getSupervisorProperty("disable_private_network_bookkeeping", false) << std::endl;
	o << "allowed_processors: " << getSupervisorProperty("allowed_processors", "0-255")
	  << std::
	         endl;  // Note this sets a taskset for ALL processes, on all nodes (ex. "1,2,5-7")

	o.close();

	// destroy current TRACEController and instantiate ARTDAQSupervisorTRACEController
	if(CorePropertySupervisorBase::theTRACEController_)
	{
		__SUP_COUT__ << "Destroying TRACE Controller..." << __E__;
		delete CorePropertySupervisorBase::
		    theTRACEController_;  // destruct current TRACEController
		CorePropertySupervisorBase::theTRACEController_ = nullptr;
	}
	CorePropertySupervisorBase::theTRACEController_ =
	    new ARTDAQSupervisorTRACEController();
	((ARTDAQSupervisorTRACEController*)CorePropertySupervisorBase::theTRACEController_)
	    ->setSupervisorPtr(this);

	__SUP_COUT__ << "Constructed." << __E__;
}  // end constructor()

//==============================================================================
ARTDAQSupervisor::~ARTDAQSupervisor(void)
{
	__SUP_COUT__ << "Destructor." << __E__;
	destroy();
	__SUP_COUT__ << "Destructed." << __E__;
}  // end destructor()

//==============================================================================
void ARTDAQSupervisor::destroy(void)
{
	__SUP_COUT__ << "Destroying..." << __E__;

	if(daqinterface_ptr_ != NULL)
	{
		__SUP_COUT__ << "Calling recover transition" << __E__;
		std::lock_guard<std::recursive_mutex> lk(daqinterface_pythonMutex_);

		PyObjectGuard  pName(PyUnicode_FromString("do_recover"));
		PyObjectGuard res(PyObject_CallMethodObjArgs(
		    daqinterface_ptr_, pName.get(), NULL));

		__SUP_COUT__ << "Making sure that correct state has been reached" << __E__;
		getDAQState_();
		while(daqinterface_state_ != "stopped")
		{
			getDAQState_();
			__SUP_COUT__ << "State is " << daqinterface_state_
			             << ", waiting 1s and retrying..." << __E__;
			usleep(1000000);
		}

		// Cleanup
		Py_XDECREF(daqinterface_ptr_);
		daqinterface_ptr_ = NULL;
	}

	__SUP_COUT__ << "Flusing printouts" << __E__;

	//make sure to flush printouts
	PyRun_SimpleString(R"(
import sys
sys.stdout = sys.__stdout__
sys.stderr = sys.__stderr__
)");
	Py_XDECREF(stringIO_out_);
	Py_XDECREF(stringIO_err_);

	__SUP_COUT__ << "Thread and garbage cleanup" << __E__;
	//force python thread cleanup:
	PyRun_SimpleString(
	    "import threading; [t.join() for t in threading.enumerate() if t is not "
	    "threading.main_thread()]");
	PyRun_SimpleString("import gc; gc.collect()");
	Py_Finalize();

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
void ARTDAQSupervisor::init(void)
{
	stop_runner_();

	__SUP_COUT__ << "Initializing..." << __E__;
	{
		std::lock_guard<std::recursive_mutex> lk(daqinterface_pythonMutex_);

		// allSupervisorInfo_.init(getApplicationContext());
		artdaq::configureMessageFacility("ARTDAQSupervisor");
		__SUP_COUT__ << "artdaq MF configured." << __E__;

		// initialization
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

			//setup Python output to tee output to stdout/err and to StringIO buffer "tee_buffer"
			PyRun_SimpleString(
			    "import sys\n"
			    "from io import StringIO\n"
			    "\n"
			    "class TeeOut:\n"
			    "    def __init__(self, real, buf):\n"
			    "        self.real = real\n"
			    "        self.buf = buf\n"
			    "    def write(self, data):\n"
			    "        self.real.write(data)\n"
			    "        self.buf.write(data)\n"
			    "    def flush(self):\n"
			    "        self.real.flush()\n"
			    "        self.buf.flush()\n"
			    "\n"
			    "tee_buffer = StringIO()\n"
			    "sys.stdout = TeeOut(sys.stdout, tee_buffer)\n"
			    "sys.stderr = TeeOut(sys.stderr, tee_buffer)\n");

			__SUP_COUT__ << "Adding DAQInterface directory to PYTHON_PATH" << __E__;
			PyObject* sysPath     = PySys_GetObject((char*)"path"); //do NOT DECREF borrowed objects through GetObject!
			PyObjectGuard programName(PyUnicode_FromString(daqinterface_dir));
			PyList_Append(sysPath, programName.get());

			__SUP_COUT__ << "Creating Module name" << __E__;
			PyObjectGuard pName(PyUnicode_FromString("rc.control.daqinterface"));
			/* Error checking of pName left out */

			__SUP_COUT__ << "Importing module" << __E__;
			PyObjectGuard pModule(PyImport_Import(pName.get()));

			if(pModule.get() == NULL)
			{
				PyErr_Print();
				__SS__ << "Failed to load rc.control.daqinterface" << __E__;
				__SUP_SS_THROW__;
			}
			else
			{
				__SUP_COUT__ << "Loading python module dictionary" << __E__;
				PyObject* pDict = PyModule_GetDict(pModule.get()); //do NOT DECREF borrowed objects through GetDict!
				if(pDict == NULL)
				{
					PyErr_Print();
					__SS__ << "Unable to load module dictionary" << __E__;
					__SUP_SS_THROW__;
				}
				else
				{
					__SUP_COUT__ << "Getting DAQInterface object pointer" << __E__;
					PyObjectGuard di_obj_ptr(PyDict_GetItemString(pDict, "DAQInterface"));

					__SUP_COUT__ << "Filling out DAQInterface args struct" << __E__;
					PyObjectGuard pArgs(PyTuple_New(0));

					PyObjectGuard kwargs(Py_BuildValue("{s:s, s:s, s:i, s:i, s:s, s:s}",
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
					                                 "localhost"));

					__SUP_COUT__ << "Calling DAQInterface Object Constructor" << __E__;

					// Get sys and io
					PyObjectGuard sys(PyImport_ImportModule("sys"));
					PyObjectGuard io(PyImport_ImportModule("io"));

					if(0)
					{
						//------------- redirect stdout to string

						// Create StringIO objects for stdout and stderr
						stringIO_out_ = PyObject_CallMethod(io.get(), "StringIO", NULL);
						stringIO_err_ = PyObject_CallMethod(io.get(), "StringIO", NULL);

						// Save originals (not needed, since just keep the redirection until daqinterface_ptr_ is destructed)
						// PyObject* sys_stdout = PyObject_GetAttrString(sys, "stdout");
						// PyObject* sys_stderr = PyObject_GetAttrString(sys, "stderr");

						// Redirect
						PyObject_SetAttrString(sys.get(), "stdout", stringIO_out_);
						PyObject_SetAttrString(sys.get(), "stderr", stringIO_err_);
						//------------- end redirect stdout to string
					}
					else  //capture tee buffer instead so output to console continues
					{
						PyObject* mainmod =
						    PyImport_AddModule("__main__");             // borrowed ref
						PyObject* globals = PyModule_GetDict(mainmod);  // borrowed ref

						stringIO_out_ =
						    PyDict_GetItemString(globals, "tee_buffer");  // borrowed ref

						// Do not Py_DECREF borrowed references.
					}

					daqinterface_ptr_ = PyObject_Call(di_obj_ptr.get(), pArgs.get(), kwargs.get());

					if(0)  //example printout handling
					{
						// Force an error
						PyObjectGuard bad(PyObject_CallMethod(sys.get(), "does_not_exist", NULL));
						if(!bad.get())
							PyErr_Print();  // <-- this writes into stringIO_err_, not the terminal

						// Grab stderr contents
						PyObjectGuard err_text(
						    PyObject_CallMethod(stringIO_err_, "getvalue", NULL));
						if(err_text.get())
							__COUT__ << "Captured stderr:\n"
							         << PyUnicode_AsUTF8(err_text.get()) << "\n";
						else
							__COUT__ << "Capture of stderr failed.";
					}  //end example printout handling

				}
			}
		}

		getDAQState_();

		// { //attempt to cleanup old artdaq processes DOES NOT WORK because artdaq interface knows it hasn't started
		// 	__SUP_COUT__ << "Attempting artdaq stale cleanup..." << __E__;
		// 	std::lock_guard<std::recursive_mutex> lk(daqinterface_pythonMutex_);
		// 	getDAQState_();
		// 	__SUP_COUT__ << "Status before cleanup: " << daqinterface_state_ << __E__;

		// 	PyObjectGuard pName(PyUnicode_FromString("do_recover"));
		// 	PyObjectGuard res(PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, NULL));
		// __COUT_MULTI_LBL__(0,captureStderrAndStdout_("do_recover"),"do_recover");

		// 	if(res == NULL)
		// 	{
		// 		std::string err = capturePyErr("do_recover");
		// 		__SS__ << "Error with clean up calling do_recover: " << err << __E__;
		// 		__SUP_SS_THROW__;
		// 	}
		// 	getDAQState_();
		// 	__SUP_COUT__ << "Status after cleanup: " << daqinterface_state_ << __E__;
		// 	__SUP_COUT__ << "cleanup DONE." << __E__;
		// }
	}
	start_runner_();
	__SUP_COUT__ << "Initialized." << __E__;
}  // end init()

//==============================================================================
void ARTDAQSupervisor::transitionConfiguring(toolbox::Event::Reference /*event*/)
{
	__SUP_COUT__ << "transitionConfiguring" << __E__;

	// activate the configuration tree (the first iteration)
	if(RunControlStateMachine::getIterationIndex() == 0 &&
	   RunControlStateMachine::getSubIterationIndex() == 0)
	{
		thread_error_message_ = "";
		thread_progress_bar_.resetProgressBar(0);
		last_thread_progress_update_ = time(0);  // initialize timeout timer

		CoreSupervisorBase::configureInit();

		// start configuring thread
		std::thread(&ARTDAQSupervisor::configuringThread, this).detach();

		__SUP_COUT__ << "Configuring thread started." << __E__;

		RunControlStateMachine::
		    indicateIterationWork();  // use Iteration to allow other steps to complete in the system
	}
	else  // not first time
	{
		std::string errorMessage;
		{
			std::lock_guard<std::mutex> lock(
			    thread_mutex_);                    // lock out for remainder of scope
			errorMessage = thread_error_message_;  // theStateMachine_.getErrorMessage();
		}
		int progress = thread_progress_bar_.read();
		__SUP_COUTVS__(2, errorMessage);
		__SUP_COUTVS__(2, progress);
		__SUP_COUTVS__(2, thread_progress_bar_.isComplete());

		// check for done and error messages
		if(errorMessage == "" &&  // if no update in 600 seconds, give up
		   time(0) - last_thread_progress_update_ > 600)
		{
			__SUP_SS__ << "There has been no update from the configuration thread for "
			           << (time(0) - last_thread_progress_update_)
			           << " seconds, assuming something is wrong and giving up! "
			           << "Last progress received was " << progress << __E__;
			errorMessage = ss.str();
		}

		if(errorMessage != "")
		{
			__SUP_SS__ << "Error was caught in configuring thread: " << errorMessage
			           << __E__;
			__SUP_COUT_ERR__ << "\n" << ss.str();

			theStateMachine_.setErrorMessage(ss.str());
			throw toolbox::fsm::exception::Exception(
			    "Transition Error" /*name*/,
			    ss.str() /* message*/,
			    "CoreSupervisorBase::transitionConfiguring" /*module*/,
			    __LINE__ /*line*/,
			    __FUNCTION__ /*function*/
			);
		}

		if(!thread_progress_bar_.isComplete())
		{
			__SUP_COUT__ << "Not done yet..." << __E__;
			//attempt to get live view of python output
			// __COUT_MULTI_LBL__(0, captureStderrAndStdout_("statuscheck"), "statuscheck");

			RunControlStateMachine::
			    indicateIterationWork();  // use Iteration to allow other steps to complete in the system

			if(last_thread_progress_read_ != progress)
			{
				last_thread_progress_read_   = progress;
				last_thread_progress_update_ = time(0);
			}

			sleep(1 /*seconds*/);
		}
		else
		{
			__SUP_COUT_INFO__ << "Complete configuring transition!" << __E__;
			__SUP_COUTV__(getProcessInfo_());
		}
	}

	return;
}  // end transitionConfiguring()

//==============================================================================
void ARTDAQSupervisor::configuringThread()
try
{
	std::string uid = theConfigurationManager_
	                      ->getNode(ConfigurationManager::XDAQ_APPLICATION_TABLE_NAME +
	                                "/" + CorePropertySupervisorBase::getSupervisorUID() +
	                                "/" + "LinkToSupervisorTable")
	                      .getValueAsString();

	__COUT__ << "Supervisor uid is " << uid << ", getting supervisor table node" << __E__;

	const std::string mfSubject_ = supervisorClassNoNamespace_ + "-" + uid;

	ConfigurationTree theSupervisorNode = getSupervisorTableNode();

	thread_progress_bar_.step();

	set_thread_message_("ConfigGen");

	auto info = ARTDAQTableBase::extractARTDAQInfo(
	    theSupervisorNode,
	    false /*getStatusFalseNodes*/,
	    true /*doWriteFHiCL*/,
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

	int debugLevel = theSupervisorNode.getNode("DAQInterfaceDebugLevel").getValue<int>();
	std::string setupScript = theSupervisorNode.getNode("DAQSetupScript").getValue();

	// Generate boot file content using helper function
	std::string bootContent =
	    ARTDAQTableBase::getBootFileContentFromInfo(info, setupScript, debugLevel);

	// Populate label_to_proc_type_map_ (still needed for later)
	for(auto& builder : info.processes[ARTDAQTableBase::ARTDAQAppType::EventBuilder])
		label_to_proc_type_map_[builder.label] = "EventBuilder";
	for(auto& logger : info.processes[ARTDAQTableBase::ARTDAQAppType::DataLogger])
		label_to_proc_type_map_[logger.label] = "DataLogger";
	for(auto& dispatcher : info.processes[ARTDAQTableBase::ARTDAQAppType::Dispatcher])
		label_to_proc_type_map_[dispatcher.label] = "Dispatcher";
	for(auto& rmanager : info.processes[ARTDAQTableBase::ARTDAQAppType::RoutingManager])
		label_to_proc_type_map_[rmanager.label] = "RoutingManager";

	// Write boot.txt file
	std::ofstream o(ARTDAQTableBase::ARTDAQ_FCL_PATH + "/boot.txt", std::ios::trunc);
	o << bootContent;
	o.close();

	// TODO: To save to runlog, store bootContent in metadata/configuration archive
	// Example (add when implementing runlog integration):
	// saveToRunlog("boot.txt", bootContent, run_number);

	thread_progress_bar_.step();
	set_thread_message_("Writing Fhicl Files");

	__GEN_COUT__ << "Building configuration directory" << __E__;

	boost::system::error_code ignored;
	boost::filesystem::remove_all(ARTDAQTableBase::ARTDAQ_FCL_PATH + FAKE_CONFIG_NAME,
	                              ignored);
	mkdir((ARTDAQTableBase::ARTDAQ_FCL_PATH + FAKE_CONFIG_NAME).c_str(), 0755);

	for(auto& reader : info.processes[ARTDAQTableBase::ARTDAQAppType::BoardReader])
	{
		symlink(ARTDAQTableBase::getFlatFHICLFilename(
		            ARTDAQTableBase::ARTDAQAppType::BoardReader, reader.label)
		            .c_str(),
		        (ARTDAQTableBase::ARTDAQ_FCL_PATH + FAKE_CONFIG_NAME + "/" +
		         reader.label + ".fcl")
		            .c_str());
	}
	for(auto& builder : info.processes[ARTDAQTableBase::ARTDAQAppType::EventBuilder])
	{
		symlink(ARTDAQTableBase::getFlatFHICLFilename(
		            ARTDAQTableBase::ARTDAQAppType::EventBuilder, builder.label)
		            .c_str(),
		        (ARTDAQTableBase::ARTDAQ_FCL_PATH + FAKE_CONFIG_NAME + "/" +
		         builder.label + ".fcl")
		            .c_str());
	}
	for(auto& logger : info.processes[ARTDAQTableBase::ARTDAQAppType::DataLogger])
	{
		symlink(ARTDAQTableBase::getFlatFHICLFilename(
		            ARTDAQTableBase::ARTDAQAppType::DataLogger, logger.label)
		            .c_str(),
		        (ARTDAQTableBase::ARTDAQ_FCL_PATH + FAKE_CONFIG_NAME + "/" +
		         logger.label + ".fcl")
		            .c_str());
	}
	for(auto& dispatcher : info.processes[ARTDAQTableBase::ARTDAQAppType::Dispatcher])
	{
		symlink(ARTDAQTableBase::getFlatFHICLFilename(
		            ARTDAQTableBase::ARTDAQAppType::Dispatcher, dispatcher.label)
		            .c_str(),
		        (ARTDAQTableBase::ARTDAQ_FCL_PATH + FAKE_CONFIG_NAME + "/" +
		         dispatcher.label + ".fcl")
		            .c_str());
	}
	for(auto& rmanager : info.processes[ARTDAQTableBase::ARTDAQAppType::RoutingManager])
	{
		symlink(ARTDAQTableBase::getFlatFHICLFilename(
		            ARTDAQTableBase::ARTDAQAppType::RoutingManager, rmanager.label)
		            .c_str(),
		        (ARTDAQTableBase::ARTDAQ_FCL_PATH + FAKE_CONFIG_NAME + "/" +
		         rmanager.label + ".fcl")
		            .c_str());
	}

	thread_progress_bar_.step();

	std::lock_guard<std::recursive_mutex> lk(daqinterface_pythonMutex_);
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
	PyObjectGuard pName1(PyUnicode_FromString("setdaqcomps"));

	PyObjectGuard readerDict(PyDict_New());
	for(auto& reader : info.processes[ARTDAQTableBase::ARTDAQAppType::BoardReader])
	{
		//Note: transfer reference to readerDict, so no PyObjectGuard on fill objects

		label_to_proc_type_map_[reader.label] = "BoardReader";
		PyObject* readerName = PyUnicode_FromString(reader.label.c_str());

		int list_size = reader.allowed_processors != "" ? 4 : 3;

		PyObject* readerData = PyList_New(list_size);
		PyObject* readerHost = PyUnicode_FromString(reader.hostname.c_str());
		PyObject* readerPort = PyUnicode_FromString("-1");
		PyObject* readerSubsystem =
		    PyUnicode_FromString(std::to_string(reader.subsystem).c_str());
		PyList_SetItem(readerData, 0, readerHost);
		PyList_SetItem(readerData, 1, readerPort);
		PyList_SetItem(readerData, 2, readerSubsystem);
		if(reader.allowed_processors != "")
		{
			PyObject* readerAllowedProcessors =
			    PyUnicode_FromString(reader.allowed_processors.c_str());
			PyList_SetItem(readerData, 3, readerAllowedProcessors);
		}
		PyDict_SetItem(readerDict.get(), readerName, readerData);
	}
	PyObjectGuard res1(
	    PyObject_CallMethodObjArgs(daqinterface_ptr_, pName1.get(), readerDict.get(), NULL));
	__COUT_MULTI_LBL__(0, captureStderrAndStdout_("setdaqcomps"), "setdaqcomps");

	if(checkPythonError(res1.get()))
	{
		std::string err_msg = capturePyErr("setdaqcomps");
		__GEN_SS__ << "Error calling setdaqcomps: " << err_msg << __E__;
		__GEN_SS_THROW__;
	}

	getDAQState_();
	__GEN_COUT__ << "Status after setdaqcomps: " << daqinterface_state_ << __E__;

	thread_progress_bar_.step();
	set_thread_message_("Calling do_boot");
	__GEN_COUT_INFO__ << "Calling do_boot" << __E__;
	__GEN_COUT__ << "Status before boot: " << daqinterface_state_ << __E__;

	// 1. Create Python Strings (Must DECREF later)
	PyObjectGuard pNameBoot(PyUnicode_FromString("do_boot"));
	PyObjectGuard pBootArgs(
	    PyUnicode_FromString((ARTDAQTableBase::ARTDAQ_FCL_PATH + "/boot.txt").c_str()));

	// 2. First Attempt: Call do_boot
	PyObjectGuard resBoot1(
	    PyObject_CallMethodObjArgs(daqinterface_ptr_, pNameBoot.get(), pBootArgs.get(), NULL));

	std::string doBootOutput = captureStderrAndStdout_("do_boot");
	__COUT_MULTI_LBL__(0, doBootOutput, "do_boot");

	if(checkPythonError(resBoot1.get()))
	{
		// --- FAILURE PATH ---

		std::string err1 = capturePyErr("do_boot");

		__GEN_COUT_INFO__ << "Error on first boot attempt: " << err1
		                  << ". Recovering and retrying..." << __E__;

		// B. Attempt 'do_recover'
		PyObjectGuard pNameRecover(PyUnicode_FromString("do_recover"));
		PyObjectGuard resRecover(
		    PyObject_CallMethodObjArgs(daqinterface_ptr_, pNameRecover.get(), NULL));
		__COUT_MULTI_LBL__(0, captureStderrAndStdout_("do_recover"), "do_recover");

		if(checkPythonError(resRecover.get()))
		{
			// Recover failed - Critical Error
			std::string errRec = capturePyErr("do_recover");

			std::stringstream oss;
			oss << "Error calling recover transition!!!! " << errRec;
			if(doBootOutput.size() > OUT_ON_ERR_SIZE)
				oss << "... last " << OUT_ON_ERR_SIZE
				    << " chars: " << doBootOutput.substr(doBootOutput.size() - 1000);
			else
				oss << doBootOutput;

			// Clean up original args before throwing
			__GEN_SS__ << oss.str() << __E__;
			__GEN_SS_THROW__;
		}

		// C. Retry 'do_boot'
		thread_progress_bar_.step();
		set_thread_message_("Calling do_boot (retry)");
		__GEN_COUT_INFO__ << "Calling do_boot again" << __E__;

		// Reuse pNameBoot and pBootArgs (valid until we DECREF them at the very end)
		PyObjectGuard resBoot2(
		    PyObject_CallMethodObjArgs(daqinterface_ptr_, pNameBoot.get(), pBootArgs.get(), NULL));

		doBootOutput = captureStderrAndStdout_("do_boot (retry)");
		__COUT_MULTI_LBL__(0, doBootOutput, "do_boot (retry)");

		if(checkPythonError(resBoot2.get()))
		{
			// Second boot failed
			std::string err2 = capturePyErr("do_boot retry");

			std::stringstream oss;
			oss << "Error calling boot transition (2nd try): " << err2;
			if(doBootOutput.size() > OUT_ON_ERR_SIZE)
				oss << "... last " << OUT_ON_ERR_SIZE
				    << " chars: " << doBootOutput.substr(doBootOutput.size() - 1000);
			else
				oss << doBootOutput;

			__GEN_SS__ << oss.str() << __E__;
			__GEN_SS_THROW__;
		}
	}

	getDAQState_();
	if(daqinterface_state_ != "booted")
	{
		std::cout << "Do boot output on error: \n" << doBootOutput << __E__;
		__GEN_SS__ << "DAQInterface boot transition failed! "
		           << "Status after boot attempt: " << daqinterface_state_ << __E__;

		if(doBootOutput.size() > OUT_ON_ERR_SIZE)  //last OUT_ON_ERR_SIZE chars only
			ss << "... last " << OUT_ON_ERR_SIZE
			   << " characters: " << doBootOutput.substr(doBootOutput.size() - 1000);
		else
			ss << doBootOutput;
		__GEN_SS_THROW__;
	}
	__GEN_COUT__ << "Status after boot: " << daqinterface_state_ << __E__;

	thread_progress_bar_.step();
	set_thread_message_("Calling do_config");
	__GEN_COUT_INFO__ << "Calling do_config" << __E__;
	__GEN_COUT__ << "Status before config: " << daqinterface_state_ << __E__;
	std::string doConfigOutput = "";
	{  //do_config call
		// RAII wrapper for Python objects to ensure cleanup even on exception

		PyObjectGuard pName3(PyUnicode_FromString("do_config"));
		// 2. Create the argument - list containing config name: ["my_config"]
		PyObjectGuard pArg(Py_BuildValue("[s]", FAKE_CONFIG_NAME));

		// 3. Call the method
		PyObjectGuard res3(
		    PyObject_CallMethodObjArgs(daqinterface_ptr_, pName3.get(), pArg.get(), NULL));

		// 4. Check for errors FIRST before capturing output (which might clear error state)
		if(checkPythonError(res3.get()))
		{
			// Get the error message before doing anything else
			std::string err = capturePyErr("do_config");

			// Now capture output for diagnostics
			doConfigOutput = captureStderrAndStdout_("do_config");

			__GEN_SS__ << "Error calling config transition: " << err << __E__;
			__GEN_SS_THROW__;
		}

		// 5. Success path - capture output
		doConfigOutput = captureStderrAndStdout_("do_config");
		__COUT_MULTI_LBL__(0, doConfigOutput, "do_config");

		// 6. Success Handling (Safe conversion to string)
		// We use PyObject_Str to safely convert any return type (None, Int, String) to text
		PyObjectGuard  strRes(PyObject_Str(res3.get()));
		const char* res_cstr = "";
		if(strRes.get())
		{
			res_cstr = PyUnicode_AsUTF8(strRes.get());
		}

		__SUP_COUTT__ << "do_config result=" << (res_cstr ? res_cstr : "N/A") << __E__;
	}  //end do_config call

	getDAQState_();
	if(daqinterface_state_ != "ready")
	{
		__GEN_SS__ << "DAQInterface config transition failed!" << __E__
		           << "Supervisor state: \"" << daqinterface_state_ << "\" != \"ready\" "
		           << __E__;
		auto doConfigOutput_recover_i =
		    doConfigOutput.find("RECOVER transition underway");
		if(doConfigOutput_recover_i == std::string::npos)
			ss << doConfigOutput;
		else if(doConfigOutput_recover_i >
		        OUT_ON_ERR_SIZE)  //last OUT_ON_ERR_SIZE chars only
			ss << "... tail of " << OUT_ON_ERR_SIZE << " characters before recovery: "
			   << doConfigOutput.substr(
			          doConfigOutput_recover_i - OUT_ON_ERR_SIZE +
			              std::string("RECOVER transition underway").size(),
			          OUT_ON_ERR_SIZE);
		else
			ss << doConfigOutput.substr(
			    0,
			    doConfigOutput_recover_i +
			        std::string("RECOVER transition underway").size());
		__GEN_SS_THROW__;
	}
	__GEN_COUT__ << "Status after config: " << daqinterface_state_ << __E__;
	thread_progress_bar_.complete();
	set_thread_message_("Configured");
	__GEN_COUT_INFO__ << "Configured." << __E__;

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
	__SS__ << "Unknown error was caught while configuring. Please checked the logs."
	       << __E__;
	__COUT_ERR__ << "\n" << ss.str();

	artdaq::ExceptionHandler(artdaq::ExceptionHandlerRethrow::no, ss.str());

	std::lock_guard<std::mutex> lock(thread_mutex_);  // lock out for remainder of scope
	thread_error_message_ = ss.str();
}  // end configuringThread() error handling

//==============================================================================
void ARTDAQSupervisor::transitionHalting(toolbox::Event::Reference /*event*/)
try
{
	set_thread_message_("Halting");
	__SUP_COUT__ << "Halting..." << __E__;

	int tries = 0;
	while(tries++ < 5)
	{
		std::unique_lock<std::recursive_mutex> lk(daqinterface_pythonMutex_,
		                                          std::try_to_lock);
		if(!lk.owns_lock())  //if lock not availabe, just report last status
		{
			__COUTS__(50) << "Do not have python lock for halt. tries=" << tries << __E__;
			sleep(1);
			continue;
		}
		__COUTS__(50) << "Have python lock!" << __E__;

		// std::lock_guard<std::recursive_mutex> lk(daqinterface_pythonMutex_);
		getDAQState_();
		__SUP_COUT__ << "Status before halt: " << daqinterface_state_ << __E__;

		if(daqinterface_state_ == "running")
		{
			// First stop before halting
			PyObjectGuard pName(PyUnicode_FromString("do_stop_running"));
			PyObjectGuard res(PyObject_CallMethodObjArgs(daqinterface_ptr_, pName.get(), NULL));
			__COUT_MULTI_LBL__(
			    0, captureStderrAndStdout_("do_stop_running"), "do_stop_running");

			if(res.get() == NULL)
			{
				std::string err = capturePyErr();
				__SS__ << "Error calling  DAQ Interface stop transition: " << err
				       << __E__;
				__SUP_SS_THROW__;
			}
		}

		PyObjectGuard pName(PyUnicode_FromString("do_command"));
		PyObjectGuard pArg(PyUnicode_FromString("Shutdown"));
		PyObjectGuard res(PyObject_CallMethodObjArgs(daqinterface_ptr_, pName.get(), pArg.get(), NULL));
		__COUT_MULTI_LBL__(
		    0, captureStderrAndStdout_("do_command Shutdown"), "do_command Shutdown");

		if(checkPythonError(res.get()))
		{
			std::string err = capturePyErr("do_command Shutdown");
			__SS__ << "Error calling DAQ Interface halt transition: " << err << __E__;
			__SUP_SS_THROW__;
		}

		getDAQState_();
		__SUP_COUT__ << "Status after halt: " << daqinterface_state_ << __E__;
		break;
	}  //end retry loop

	__SUP_COUT__ << "Halted." << __E__;
	set_thread_message_("Halted");
}  // end transitionHalting()
catch(const std::runtime_error& e)
{
	const std::string transitionName = "Halting";
	// if halting from Failed state, then ignore errors
	if(theStateMachine_.getProvenanceStateName() ==
	       RunControlStateMachine::FAILED_STATE_NAME ||
	   theStateMachine_.getProvenanceStateName() ==
	       RunControlStateMachine::HALTED_STATE_NAME)
	{
		__SUP_COUT_INFO__ << "Error was caught while halting (but ignoring because "
		                     "previous state was '"
		                  << RunControlStateMachine::FAILED_STATE_NAME
		                  << "'): " << e.what() << __E__;
	}
	else  // if not previously in Failed state, then fail
	{
		__SUP_SS__ << "Error was caught while " << transitionName << ": " << e.what()
		           << __E__;
		__SUP_COUT_ERR__ << "\n" << ss.str();
		theStateMachine_.setErrorMessage(ss.str());
		throw toolbox::fsm::exception::Exception(
		    "Transition Error" /*name*/,
		    ss.str() /* message*/,
		    "ARTDAQSupervisorBase::transition" + transitionName /*module*/,
		    __LINE__ /*line*/,
		    __FUNCTION__ /*function*/
		);
	}
}  // end transitionHalting() std::runtime_error exception handling
catch(...)
{
	const std::string transitionName = "Halting";
	// if halting from Failed state, then ignore errors
	if(theStateMachine_.getProvenanceStateName() ==
	       RunControlStateMachine::FAILED_STATE_NAME ||
	   theStateMachine_.getProvenanceStateName() ==
	       RunControlStateMachine::HALTED_STATE_NAME)
	{
		__SUP_COUT_INFO__ << "Unknown error was caught while halting (but ignoring "
		                     "because previous state was '"
		                  << RunControlStateMachine::FAILED_STATE_NAME << "')." << __E__;
	}
	else  // if not previously in Failed state, then fail
	{
		__SUP_SS__ << "Unknown error was caught while " << transitionName
		           << ". Please checked the logs." << __E__;
		__SUP_COUT_ERR__ << "\n" << ss.str();
		theStateMachine_.setErrorMessage(ss.str());

		artdaq::ExceptionHandler(artdaq::ExceptionHandlerRethrow::no, ss.str());

		throw toolbox::fsm::exception::Exception(
		    "Transition Error" /*name*/,
		    ss.str() /* message*/,
		    "ARTDAQSupervisorBase::transition" + transitionName /*module*/,
		    __LINE__ /*line*/,
		    __FUNCTION__ /*function*/
		);
	}
}  // end transitionHalting() exception handling

//==============================================================================
void ARTDAQSupervisor::transitionInitializing(toolbox::Event::Reference /*event*/)
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
	__SS__ << "Unknown error was caught while Initializing. Please checked the logs."
	       << __E__;
	artdaq::ExceptionHandler(artdaq::ExceptionHandlerRethrow::no, ss.str());
	__SS_THROW__;
}  // end transitionInitializing() error handling

//==============================================================================
void ARTDAQSupervisor::transitionPausing(toolbox::Event::Reference /*event*/)
try
{
	set_thread_message_("Pausing");
	__SUP_COUT__ << "Pausing..." << __E__;
	std::lock_guard<std::recursive_mutex> lk(daqinterface_pythonMutex_);

	getDAQState_();
	__SUP_COUT__ << "Status before pause: " << daqinterface_state_ << __E__;

	PyObjectGuard pName(PyUnicode_FromString("do_command"));
	PyObjectGuard pArg(PyUnicode_FromString("Pause"));
	PyObjectGuard res(PyObject_CallMethodObjArgs(daqinterface_ptr_, pName.get(), pArg.get(), NULL));
	__COUT_MULTI_LBL__(
	    0, captureStderrAndStdout_("do_command Pause"), "do_command Pause");

	if(checkPythonError(res.get()))
	{
		std::string err = capturePyErr("do_command Pause");
		__SS__ << "Error calling DAQ Interface Pause transition: " << err << __E__;
		__SUP_SS_THROW__;
	}

	getDAQState_();
	__SUP_COUT__ << "Status after pause: " << daqinterface_state_ << __E__;

	__SUP_COUT__ << "Paused." << __E__;
	set_thread_message_("Paused");
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
void ARTDAQSupervisor::transitionResuming(toolbox::Event::Reference /*event*/)
try
{
	set_thread_message_("Resuming");
	__SUP_COUT__ << "Resuming..." << __E__;
	std::lock_guard<std::recursive_mutex> lk(daqinterface_pythonMutex_);

	getDAQState_();
	__SUP_COUT__ << "Status before resume: " << daqinterface_state_ << __E__;
	PyObjectGuard pName(PyUnicode_FromString("do_command"));
	PyObjectGuard pArg(PyUnicode_FromString("Resume"));
	PyObjectGuard res(PyObject_CallMethodObjArgs(daqinterface_ptr_, pName.get(), pArg.get(), NULL));
	__COUT_MULTI_LBL__(
	    0, captureStderrAndStdout_("do_command Resume"), "do_command Resume");

	if(checkPythonError(res.get()))
	{
		std::string err = capturePyErr("do_command Resume");
		__SS__ << "Error calling DAQ Interface Resume transition: " << err << __E__;
		__SUP_SS_THROW__;
	}

	getDAQState_();
	__SUP_COUT__ << "Status after resume: " << daqinterface_state_ << __E__;
	__SUP_COUT__ << "Resumed." << __E__;
	set_thread_message_("Resumed");
}  // end transitionResuming()
catch(const std::runtime_error& e)
{
	__SS__ << "Error was caught while Resuming: " << e.what() << __E__;
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "Unknown error was caught while Resuming. Please checked the logs."
	       << __E__;
	artdaq::ExceptionHandler(artdaq::ExceptionHandlerRethrow::no, ss.str());
	__SS_THROW__;
}  // end transitionResuming() error handling

//==============================================================================
void ARTDAQSupervisor::transitionStarting(toolbox::Event::Reference /*event*/)
try
{
	__SUP_COUT__ << "transitionStarting" << __E__;

	// first time launch thread because artdaq Supervisor may take a while
	if(RunControlStateMachine::getIterationIndex() == 0 &&
	   RunControlStateMachine::getSubIterationIndex() == 0)
	{
		thread_error_message_ = "";
		thread_progress_bar_.resetProgressBar(0);
		last_thread_progress_update_ = time(0);  // initialize timeout timer

		// start configuring thread
		std::thread(&ARTDAQSupervisor::startingThread, this).detach();

		__SUP_COUT_INFO__ << "Starting thread started." << __E__;

		RunControlStateMachine::
		    indicateIterationWork();  // use Iteration to allow other steps to complete in the system
	}
	else  // not first time
	{
		std::string errorMessage;
		{
			std::lock_guard<std::mutex> lock(
			    thread_mutex_);                    // lock out for remainder of scope
			errorMessage = thread_error_message_;  // theStateMachine_.getErrorMessage();
		}
		int progress = thread_progress_bar_.read();
		__SUP_COUTV__(errorMessage);
		__SUP_COUTV__(progress);
		__SUP_COUTV__(thread_progress_bar_.isComplete());

		// check for done and error messages
		if(errorMessage == "" &&  // if no update in 600 seconds, give up
		   time(0) - last_thread_progress_update_ > 600)
		{
			__SUP_SS__ << "There has been no update from the start thread for "
			           << (time(0) - last_thread_progress_update_)
			           << " seconds, assuming something is wrong and giving up! "
			           << "Last progress received was " << progress << __E__;
			errorMessage = ss.str();
		}

		if(errorMessage != "")
		{
			__SUP_SS__ << "Error was caught in starting thread: " << errorMessage
			           << __E__;
			__SUP_COUT_ERR__ << "\n" << ss.str();

			theStateMachine_.setErrorMessage(ss.str());
			throw toolbox::fsm::exception::Exception(
			    "Transition Error" /*name*/,
			    ss.str() /* message*/,
			    "CoreSupervisorBase::transitionStarting" /*module*/,
			    __LINE__ /*line*/,
			    __FUNCTION__ /*function*/
			);
		}

		if(!thread_progress_bar_.isComplete())
		{
			__SUP_COUT__ << "Not done yet..." << __E__;
			//attempt to get live view of python output (not working and not needed with new Tee Buffer solution)
			// __COUT_MULTI_LBL__(0, captureStderrAndStdout_("statuscheck"), "statuscheck");

			RunControlStateMachine::
			    indicateIterationWork();  // use Iteration to allow other steps to complete in the system

			if(last_thread_progress_read_ != progress)
			{
				last_thread_progress_read_   = progress;
				last_thread_progress_update_ = time(0);
			}

			sleep(1 /*seconds*/);
		}
		else
		{
			__SUP_COUT_INFO__ << "Starting transition completed!" << __E__;
			__SUP_COUTV__(getProcessInfo_());
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
	__SS__ << "Unknown error was caught while Starting. Please checked the logs."
	       << __E__;
	artdaq::ExceptionHandler(artdaq::ExceptionHandlerRethrow::no, ss.str());
	__SS_THROW__;
}  // end transitionStarting() error handling

//==============================================================================
void ARTDAQSupervisor::startingThread()
try
{
	std::string uid = theConfigurationManager_
	                      ->getNode(ConfigurationManager::XDAQ_APPLICATION_TABLE_NAME +
	                                "/" + CorePropertySupervisorBase::getSupervisorUID() +
	                                "/" + "LinkToSupervisorTable")
	                      .getValueAsString();

	__COUT__ << "Supervisor uid is " << uid << ", getting supervisor table node" << __E__;
	const std::string mfSubject_ = supervisorClassNoNamespace_ + "-" + uid;
	__GEN_COUT__ << "Starting..." << __E__;
	set_thread_message_("Starting");

	thread_progress_bar_.step();
	stop_runner_();
	{
		std::lock_guard<std::recursive_mutex> lk(daqinterface_pythonMutex_);
		getDAQState_();
		__GEN_COUT__ << "Status before start: " << daqinterface_state_ << __E__;
		auto runNumber = SOAPUtilities::translate(theStateMachine_.getCurrentMessage())
		                     .getParameters()
		                     .getValue("RunNumber");

		thread_progress_bar_.step();

		__GEN_COUT_INFO__ << "Calling do_start_running" << __E__;
		PyObjectGuard pName(PyUnicode_FromString("do_start_running"));
		int           run_number = std::stoi(runNumber);
		PyObjectGuard pStateArgs(PyLong_FromLong(run_number));
		PyObjectGuard res(
		    PyObject_CallMethodObjArgs(daqinterface_ptr_, pName.get(), pStateArgs.get(), NULL));
		__COUT_MULTI_LBL__(
		    0, captureStderrAndStdout_("do_start_running"), "do_start_running");

		thread_progress_bar_.step();

		if(res.get() == NULL)
		{
			std::string err = capturePyErr();
			__SS__ << "Error calling start transition: " << err << __E__;
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

	__GEN_COUT_INFO__ << "Started." << __E__;
	thread_progress_bar_.complete();

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
	__SS__ << "Unknown error was caught while Starting. Please checked the logs."
	       << __E__;
	__COUT_ERR__ << "\n" << ss.str();

	artdaq::ExceptionHandler(artdaq::ExceptionHandlerRethrow::no, ss.str());

	std::lock_guard<std::mutex> lock(thread_mutex_);  // lock out for remainder of scope
	thread_error_message_ = ss.str();
}  // end startingThread() error handling

//==============================================================================
void ARTDAQSupervisor::transitionStopping(toolbox::Event::Reference /*event*/)
try
{
	__SUP_COUT__ << "Stopping..." << __E__;
	set_thread_message_("Stopping");
	std::lock_guard<std::recursive_mutex> lk(daqinterface_pythonMutex_);
	getDAQState_();
	__SUP_COUT__ << "Status before stop: " << daqinterface_state_ << __E__;
	PyObjectGuard pName(PyUnicode_FromString("do_stop_running"));
	PyObjectGuard res(PyObject_CallMethodObjArgs(daqinterface_ptr_, pName.get(), NULL));
	__COUT_MULTI_LBL__(0, captureStderrAndStdout_("do_stop_running"), "do_stop_running");

	if(checkPythonError(res.get()))
	{
		std::string err = capturePyErr("do_stop_running");
		__SS__ << "Error calling DAQ Interface  stop transition: " << err << __E__;
		__SUP_SS_THROW__;
	}
	getDAQState_();
	__SUP_COUT__ << "Status after stop: " << daqinterface_state_ << __E__;
	__SUP_COUT__ << "Stopped." << __E__;
	set_thread_message_("Stopped");
}  // end transitionStopping()
catch(const std::runtime_error& e)
{
	__SS__ << "Error was caught while Stopping: " << e.what() << __E__;
	__SS_THROW__;
}
catch(...)
{
	__SS__ << "Unknown error was caught while Stopping. Please checked the logs."
	       << __E__;
	artdaq::ExceptionHandler(artdaq::ExceptionHandlerRethrow::no, ss.str());
	__SS_THROW__;
}  // end transitionStopping() error handling

//==============================================================================
void ots::ARTDAQSupervisor::enteringError(toolbox::Event::Reference /*event*/)
{
	__SUP_COUT__ << "Entering error recovery state" << __E__;
	std::lock_guard<std::recursive_mutex> lk(daqinterface_pythonMutex_);
	getDAQState_();
	__SUP_COUT__ << "Status before error: " << daqinterface_state_ << __E__;

	PyObjectGuard pName(PyUnicode_FromString("do_recover"));
	PyObjectGuard res(PyObject_CallMethodObjArgs(daqinterface_ptr_, pName.get(), NULL));
	__COUT_MULTI_LBL__(0, captureStderrAndStdout_("do_recover"), "do_recover");

	if(checkPythonError(res.get()))
	{
		std::string err = capturePyErr("do_recover");
		//do not throw exception when entering error, because failing DAQ interface could be the reason for error in first place
		__SUP_COUT_WARN__ << "Error calling DAQ Interface recover transition: " << err
		                  << __E__;
		return;
	}

	getDAQState_();
	__SUP_COUT__ << "Status after error: " << daqinterface_state_ << __E__;
	__SUP_COUT__ << "EnteringError DONE." << __E__;

}  // end enteringError()

std::vector<SupervisorInfo::SubappInfo> ots::ARTDAQSupervisor::getSubappInfo(void)
{
	auto apps = getAndParseProcessInfo_();

	std::map<int, SupervisorInfo::SubappInfo> subapp_infos;
	for(auto& app : apps)
	{
		SupervisorInfo::SubappInfo info;

		info.name   = app.label;
		info.detail = "Rank " + std::to_string(app.rank) + ", subsystem " +
		              std::to_string(app.subsystem);
		info.lastStatusTime = time(0);
		info.progress       = 100;
		info.status         = artdaqStateToOtsState(app.state);
		info.url        = "http://" + app.host + ":" + std::to_string(app.port) + "/RPC2";
		info.class_name = "ARTDAQ " + labelToProcType_(app.label);

		subapp_infos[app.rank] = info;
	}

	std::vector<SupervisorInfo::SubappInfo> output;
	for(auto& [rank, info] : subapp_infos)
	{
		output.push_back(info);
	}
	return output;
} //end getSubappInfo()

//==============================================================================
// Helper function to check if a Python call failed
// Returns true if there was an error (result is NULL or PyErr_Occurred)
// NOTE: Does NOT clear the Python error state - caller should call capturePyErr() to fetch it
bool ots::ARTDAQSupervisor::checkPythonError(PyObject* result)
{
	if(result == NULL || PyErr_Occurred())
	{
		// Assume result is cleaned up by its PyObjectGuard if needed
		
		// Note: We keep the Python error state so caller can extract the message with capturePyErr()
		return true;  // Error occurred
	}
	return false;  // No error
} //end checkPythonError()

//==============================================================================
std::string ots::ARTDAQSupervisor::capturePyErr(std::string label /* = "" */)
{
	std::string err_msg = "Unknown Python Error";
	PyObject *  pType, *pValue, *pTraceback;
	PyErr_Fetch(&pType, &pValue, &pTraceback);
	PyErr_NormalizeException(&pType, &pValue, &pTraceback);

	if(pType != NULL)
	{
		// Format the full traceback like Python does
		PyObjectGuard traceback_module(PyImport_ImportModule("traceback"));
		if(traceback_module.get() != NULL)
		{
			PyObjectGuard format_exception(
			    PyObject_GetAttrString(traceback_module.get(), "format_exception"));
			if(format_exception.get() != NULL)
			{
				PyObjectGuard formatted(
				    PyObject_CallFunctionObjArgs(format_exception.get(),
				                                 pType,
				                                 pValue ? pValue : Py_None,
				                                 pTraceback ? pTraceback : Py_None,
				                                 NULL));
				if(formatted.get() != NULL)
				{
					// formatted is a list of strings, join them
					PyObjectGuard empty_string(PyUnicode_FromString(""));
					PyObjectGuard joined(PyUnicode_Join(empty_string.get(), formatted.get()));
					if(joined.get() != NULL)
					{
						const char* traceback_cstr = PyUnicode_AsUTF8(joined.get());
						if(traceback_cstr)
							err_msg = traceback_cstr;
					}
				}
			}
		}

		// Fallback to simple message if traceback formatting failed
		if(err_msg == "Unknown Python Error" && pValue != NULL)
		{
			PyObjectGuard pStr(PyObject_Str(pValue));
			if(pStr.get() != NULL)
			{
				const char* error_cstr = PyUnicode_AsUTF8(pStr.get());
				if(error_cstr)
					err_msg = error_cstr;
			}
		}
	}

	Py_XDECREF(pType);
	Py_XDECREF(pValue);
	Py_XDECREF(pTraceback);

	// Add label prefix if provided
	if(!label.empty())
		err_msg = label + ":\n" + err_msg;

	return err_msg;
}  //end capturePyErr()

//==============================================================================
std::string ots::ARTDAQSupervisor::captureStderrAndStdout_(std::string label /* = "" */)
{
	if(!stringIO_out_)
		return "";  // Not defined
	if(label.size())
		label += ' ';  //for nice printing

	std::string outString = "";
	PyObjectGuard  out(PyObject_CallMethod(stringIO_out_, "getvalue", NULL));

	if(checkPythonError(out.get()))
	{
		// Error getting output - clear the error and return empty string
		capturePyErr("captureStderrAndStdout getvalue");
		return "";
	}

	const char* text = PyUnicode_AsUTF8(out.get());

	return text ? text : "";
}  //end captureStderrAndStdout_()

void ots::ARTDAQSupervisor::getDAQState_()
{
	__SUP_COUTS__(50) << "Getting DAQInterface python lock" << __E__;
	std::lock_guard<std::recursive_mutex> lk(daqinterface_pythonMutex_);
	__SUP_COUTS__(50) << "Have DAQInterface python lock" << __E__;

	if(daqinterface_ptr_ == nullptr)
	{
		daqinterface_state_ = "";
		__SUP_COUT_WARN__ << "daqinterface_ptr_ is not initialized!" << __E__;
		return;
	}

	// Prepare Python Strings ONCE (Move outside loop to prevent 5x memory leak)
	PyObjectGuard pName(PyUnicode_FromString("state"));
	PyObjectGuard pArg(PyUnicode_FromString("DAQInterface"));

	// WARNING: Verify your Python 'state' method accepts an argument.
	// If 'def state(self):' is the signature, passing pArg will fail.
	// If so, call: PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, NULL);

	int tries = 0;
	while(tries < 5)
	{
		// Call the method
		PyObjectGuard res(PyObject_CallMethodObjArgs(daqinterface_ptr_, pName.get(), pArg.get(), NULL));

		if(checkPythonError(res.get()))
		{
			tries++;

			// Get the error message
			std::string err_msg = capturePyErr("state");

			std::ostringstream ss;
			ss << "Attempt n " << tries
			   << ". Error calling 'state'. Python Exception: " << err_msg;

			if(tries >= 5)
			{
				__COUT_ERR__ << ss.str() << __E__;  // Log error on final fail
				daqinterface_state_ = "ERROR";      // distinct from empty
			}
			else
			{
				__COUT__ << ss.str() << __E__;  // Log warning
				usleep(100000);                 // 100ms
			}
			continue;
		}

		// --- SUCCESS CASE ---

		// Safely convert result to string (res might not be a string!)
		PyObjectGuard strRes(PyObject_Str(res.get()));  // Force conversion to string
		if(strRes.get())
		{
			daqinterface_state_ = std::string(PyUnicode_AsUTF8(strRes.get()));
		}
		else
		{
			// Rare case: object couldn't be converted to string
			daqinterface_state_ = "UNKNOWN";
		}

		__SUP_COUTS__(20) << "getDAQState_ state=" << daqinterface_state_ << __E__;
		break;
	}

	// Cleanup the string objects we created
}  //end getDAQState_()

//==============================================================================
std::string ots::ARTDAQSupervisor::getProcessInfo_(void)
{
	__SUP_COUTS__(50) << "Getting DAQInterface state lock" << __E__;
	std::lock_guard<std::recursive_mutex> lk(daqinterface_pythonMutex_);
	__SUP_COUTS__(50) << "Have DAQInterface state lock" << __E__;

	if(daqinterface_ptr_ == nullptr)
	{
		return "";
	}

	PyObjectGuard pName(PyUnicode_FromString("artdaq_process_info"));
	PyObjectGuard pArg(PyUnicode_FromString("DAQInterface"));
	PyObjectGuard pArg2(PyBool_FromLong(true));
	PyObjectGuard res(
	    PyObject_CallMethodObjArgs(daqinterface_ptr_, pName.get(), pArg.get(), pArg2.get(), NULL));

	if(checkPythonError(res.get()))
	{
		std::string err = capturePyErr("artdaq_process_info");
		__SS__ << "Error calling artdaq_process_info function: " << err << __E__;
		__SUP_SS_THROW__;
		return "";
	}
	//cache status as latest
	std::lock_guard<std::mutex> lock(daqinterface_statusMutex_);
	daqinterface_status_ = std::string(PyUnicode_AsUTF8(res.get()));
	return daqinterface_status_;
}  // end getProcessInfo_()

std::string ots::ARTDAQSupervisor::artdaqStateToOtsState(std::string state)
{
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
}

std::string ots::ARTDAQSupervisor::labelToProcType_(std::string label)
{
	if(label_to_proc_type_map_.count(label))
	{
		return label_to_proc_type_map_[label];
	}
	return "UNKNOWN";
}

//==============================================================================
/// Called by status check
std::list<ots::ARTDAQSupervisor::DAQInterfaceProcessInfo>
ots::ARTDAQSupervisor::getAndParseProcessInfo_()
{
	std::list<ots::ARTDAQSupervisor::DAQInterfaceProcessInfo> output;
	// full acquire from getProcessInfo_ creates mutex locking up!
	// auto                                                      info  = getProcessInfo_();
	std::string info;

	std::unique_lock<std::recursive_mutex> lk(daqinterface_pythonMutex_,
	                                          std::try_to_lock);
	if(!lk.owns_lock())  //if lock not availabe, just report last status
	{
		__COUTS__(50) << "Do not have python lock." << __E__;
		std::lock_guard<std::mutex> lock(daqinterface_statusMutex_);
		info = daqinterface_status_;
	}
	else  //have lock! so retrieve Python Interface status
	{
		__COUTS__(50) << "Have python lock!" << __E__;
		info = getProcessInfo_();
	}
	__COUTVS__(20, info);

	auto procs = tokenize_(info);

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
std::list<std::pair<ots::ARTDAQSupervisor::DAQInterfaceProcessInfo,
                    std::unique_ptr<artdaq::CommanderInterface>>>
ots::ARTDAQSupervisor::makeCommandersFromProcessInfo()
{
	std::list<
	    std::pair<DAQInterfaceProcessInfo, std::unique_ptr<artdaq::CommanderInterface>>>
	     output;
	auto infos = getAndParseProcessInfo_();

	for(auto& info : infos)
	{
		artdaq::Commandable cm;
		fhicl::ParameterSet ps;

		ps.put<std::string>("commanderPluginType", "xmlrpc");
		ps.put<int>("id", info.port);
		ps.put<std::string>("server_url", info.host);

		output.emplace_back(std::make_pair<DAQInterfaceProcessInfo,
		                                   std::unique_ptr<artdaq::CommanderInterface>>(
		    std::move(info), artdaq::MakeCommanderPlugin(ps, cm)));
	}

	return output;
}  // end makeCommandersFromProcessInfo()

//==============================================================================
std::list<std::string> ots::ARTDAQSupervisor::tokenize_(std::string const& input)
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
void ots::ARTDAQSupervisor::daqinterfaceRunner_()
{
	TLOG(TLVL_TRACE) << "Runner thread starting";
	runner_running_ = true;
	while(runner_running_)
	{
		if(daqinterface_ptr_ != NULL)
		{
			std::unique_lock<std::recursive_mutex> lk(daqinterface_pythonMutex_);
			getDAQState_();
			std::string state_before = daqinterface_state_;

			__SUP_COUTS__(2) << "Runner state_before=" << state_before
			                 << " state now=" << daqinterface_state_
			                 << " ?= running, ready, or booted" << __E__;

			if(daqinterface_state_ == "running" || daqinterface_state_ == "ready" ||
			   daqinterface_state_ == "booted")
			{
				try
				{
					TLOG(TLVL_TRACE) << "Calling DAQInterface::check_proc_heartbeats";
					PyObjectGuard pName(PyUnicode_FromString("check_proc_heartbeats"));
					PyObjectGuard res(
					    PyObject_CallMethodObjArgs(daqinterface_ptr_, pName.get(), NULL));
					__COUT_MULTI_LBL__(1,
					                   captureStderrAndStdout_("check_proc_heartbeats"),
					                   "check_proc_heartbeats");
					TLOG(TLVL_TRACE)
					    << "Done with DAQInterface::check_proc_heartbeats call";

					if(res.get() == NULL)
					{
						runner_running_ = false;
						std::string err = capturePyErr("check_proc_heartbeats");
						__SS__ << "Error calling check_proc_heartbeats function: " << err
						       << __E__;
						__SUP_SS_THROW__;
						break;
					}
				}
				catch(cet::exception& ex)
				{
					runner_running_ = false;
					std::string err = capturePyErr("check_proc_heartbeats");
					__SS__ << "An cet::exception occurred while calling "
					          "check_proc_heartbeats function "
					       << ex.explain_self() << ": " << err << __E__;
					__SUP_SS_THROW__;
					break;
				}
				catch(std::exception& ex)
				{
					runner_running_ = false;
					std::string err = capturePyErr("check_proc_heartbeats");
					__SS__ << "An std::exception occurred while calling "
					          "check_proc_heartbeats function: "
					       << ex.what() << "\n\n"
					       << err << __E__;
					__SUP_SS_THROW__;
					break;
				}
				catch(...)
				{
					runner_running_ = false;
					std::string err = capturePyErr("check_proc_heartbeats");
					__SS__ << "An unknown Error occurred while calling "
					          "check_proc_heartbeats function: "
					       << err << __E__;
					__SUP_SS_THROW__;
					break;
				}

				lk.unlock();
				getDAQState_();
				if(daqinterface_state_ != state_before)
				{
					runner_running_ = false;
					lk.unlock();
					__SS__ << "DAQInterface state unexpectedly changed from "
					       << state_before << " to " << daqinterface_state_
					       << ". Check supervisor log file for more info!" << __E__;
					__SUP_SS_THROW__;
					break;
				}
			}
		}
		else
		{
			__SUP_COUT__ << "daqinterface_ptr_ is null" << __E__;
			break;
		}
		usleep(1000000);
	}
	runner_running_ = false;
	TLOG(TLVL_TRACE) << "Runner thread complete";
}  // end daqinterfaceRunner_()

//==============================================================================
void ots::ARTDAQSupervisor::stop_runner_()
{
	runner_running_ = false;
	if(runner_thread_ && runner_thread_->joinable())
	{
		runner_thread_->join();
		runner_thread_.reset(nullptr);
	}
}  // end stop_runner_()

//==============================================================================
void ots::ARTDAQSupervisor::start_runner_()
{
	stop_runner_();
	runner_thread_ =
	    std::make_unique<std::thread>(&ots::ARTDAQSupervisor::daqinterfaceRunner_, this);
}  // end start_runner_()
