

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
	o << "data_directory_override: "
	  << getSupervisorProperty("data_directory_override",
	                           std::string(__ENV__("ARTDAQ_OUTPUT_DIR")))
	  << std::endl;
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
		PyObject*                             pName = PyUnicode_FromString("do_recover");
		/*PyObject*                             res   =*/PyObject_CallMethodObjArgs(
		    daqinterface_ptr_, pName, NULL);

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
		// Py_XDECREF(pStateArgs2);
		// Py_XDECREF(out_text);
		// Py_XDECREF(err_text);
		// Py_XDECREF(sys_stdout);
		// Py_XDECREF(sys_stderr);
		// Py_XDECREF(stringIO_out);
		// Py_XDECREF(stringIO_err);
		// Py_XDECREF(io);
		// Py_XDECREF(sys);
		daqinterface_ptr_ = NULL;
	}

	__SUP_COUT__ << "Flusing printouts" << __E__;

	//make sure to flush printouts
	PyRun_SimpleString(R"(
import sys
sys.stdout = sys.__stdout__
sys.stderr = sys.__stderr__
)");
	Py_XDECREF(stringIO_out);
	Py_XDECREF(stringIO_err);

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
			PyObject* sysPath     = PySys_GetObject((char*)"path");
			PyObject* programName = PyUnicode_FromString(daqinterface_dir);
			PyList_Append(sysPath, programName);
			Py_DECREF(programName);

			__SUP_COUT__ << "Creating Module name" << __E__;
			PyObject* pName = PyUnicode_FromString("rc.control.daqinterface");
			/* Error checking of pName left out */

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

					// Get sys and io
					PyObject* sys = PyImport_ImportModule("sys");
					PyObject* io  = PyImport_ImportModule("io");

					if(0)
					{
						//------------- redirect stdout to string

						// Create StringIO objects for stdout and stderr
						stringIO_out = PyObject_CallMethod(io, "StringIO", NULL);
						stringIO_err = PyObject_CallMethod(io, "StringIO", NULL);

						// Save originals (not needed, since just keep the redirection until daqinterface_ptr_ is destructed)
						// PyObject* sys_stdout = PyObject_GetAttrString(sys, "stdout");
						// PyObject* sys_stderr = PyObject_GetAttrString(sys, "stderr");

						// Redirect
						PyObject_SetAttrString(sys, "stdout", stringIO_out);
						PyObject_SetAttrString(sys, "stderr", stringIO_err);
						//------------- end redirect stdout to string
					}
					else  //capture tee buffer instead so output to console continues
					{
						PyObject* mainmod =
						    PyImport_AddModule("__main__");             // borrowed ref
						PyObject* globals = PyModule_GetDict(mainmod);  // borrowed ref

						stringIO_out =
						    PyDict_GetItemString(globals, "tee_buffer");  // borrowed
						// Do not Py_DECREF borrowed references.
					}

					daqinterface_ptr_ = PyObject_Call(di_obj_ptr, pArgs, kwargs);

					if(0)  //example printout handling
					{
						// Force an error
						PyObject* bad = PyObject_CallMethod(sys, "does_not_exist", NULL);
						if(!bad)
							PyErr_Print();  // <-- this writes into stringIO_err, not the terminal

						// Grab stderr contents
						PyObject* err_text =
						    PyObject_CallMethod(stringIO_err, "getvalue", NULL);
						if(err_text)
							__COUT__ << "Captured stderr:\n"
							         << PyUnicode_AsUTF8(err_text) << "\n";
						else
							__COUT__ << "Capture of stderr failed.";
					}  //end example printout handling

					// Cleanup
					Py_DECREF(di_obj_ptr);
					Py_XDECREF(sys);
					Py_XDECREF(io);
				}
			}
		}

		getDAQState_();

		// { //attempt to cleanup old artdaq processes DOES NOT WORK because artdaq interface knows it hasn't started
		// 	__SUP_COUT__ << "Attempting artdaq stale cleanup..." << __E__;
		// 	std::lock_guard<std::recursive_mutex> lk(daqinterface_pythonMutex_);
		// 	getDAQState_();
		// 	__SUP_COUT__ << "Status before cleanup: " << daqinterface_state_ << __E__;

		// 	PyObject* pName = PyUnicode_FromString("do_recover");
		// 	PyObject* res   = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, NULL);
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

		std::pair<std::string /*group name*/, TableGroupKey> theGroup(
		    SOAPUtilities::translate(theStateMachine_.getCurrentMessage())
		        .getParameters()
		        .getValue("ConfigurationTableGroupName"),
		    TableGroupKey(SOAPUtilities::translate(theStateMachine_.getCurrentMessage())
		                      .getParameters()
		                      .getValue("ConfigurationTableGroupKey")));

		__SUP_COUT__ << "Configuration table group name: " << theGroup.first
		             << " key: " << theGroup.second << __E__;

		try
		{
			// disable version tracking to accept untracked versions to be selected by the FSM transition source
			theConfigurationManager_->loadTableGroup(
			    theGroup.first,
			    theGroup.second,
			    true /*doActivate*/,
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
			    true /*ignoreVersionTracking*/);
		}
		catch(const std::runtime_error& e)
		{
			__SS__ << "Error loading table group '" << theGroup.first << "("
			       << theGroup.second << ")! \n"
			       << e.what() << __E__;
			__SUP_COUT_ERR__ << ss.str();
			// ExceptionHandler(ExceptionHandlerRethrow::no, ss.str());

			//__SS_THROW_ONLY__;
			theStateMachine_.setErrorMessage(ss.str());
			throw toolbox::fsm::exception::Exception(
			    "Transition Error" /*name*/,
			    ss.str() /* message*/,
			    "ARTDAQSupervisor::transitionConfiguring" /*module*/,
			    __LINE__ /*line*/,
			    __FUNCTION__ /*function*/
			);
		}
		catch(...)
		{
			__SS__ << "Unknown error loading table group '" << theGroup.first << "("
			       << theGroup.second << ")!" << __E__;
			__SUP_COUT_ERR__ << ss.str();
			// ExceptionHandler(ExceptionHandlerRethrow::no, ss.str());

			//__SS_THROW_ONLY__;
			theStateMachine_.setErrorMessage(ss.str());
			throw toolbox::fsm::exception::Exception(
			    "Transition Error" /*name*/,
			    ss.str() /* message*/,
			    "ARTDAQSupervisor::transitionConfiguring" /*module*/,
			    __LINE__ /*line*/,
			    __FUNCTION__ /*function*/
			);
		}

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
			o << "EventBuilder allowed_processors: " << builder.allowed_processors
			  << std::endl;
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
			o << "DataLogger allowed_processors: " << logger.allowed_processors
			  << std::endl;
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
			o << "Dispatcher allowed_processors: " << dispatcher.allowed_processors
			  << std::endl;
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
			o << "RoutingManager allowed_processors: " << rmanager.allowed_processors
			  << std::endl;
		}
		o << std::endl;
	}
	o.close();

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
	PyObject* pName1 = PyUnicode_FromString("setdaqcomps");

	PyObject* readerDict = PyDict_New();
	for(auto& reader : info.processes[ARTDAQTableBase::ARTDAQAppType::BoardReader])
	{
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
		PyDict_SetItem(readerDict, readerName, readerData);
	}
	PyObject* res1 =
	    PyObject_CallMethodObjArgs(daqinterface_ptr_, pName1, readerDict, NULL);
	__COUT_MULTI_LBL__(0, captureStderrAndStdout_("setdaqcomps"), "setdaqcomps");

	Py_DECREF(readerDict);

	if(res1 == NULL)
	{
		std::string err = capturePyErr("setdaqcomps");
		__GEN_SS__ << "Error calling setdaqcomps transition: " << err << __E__;
		__GEN_SS_THROW__;
	}

	getDAQState_();
	__GEN_COUT__ << "Status after setdaqcomps: " << daqinterface_state_ << __E__;

	thread_progress_bar_.step();
	set_thread_message_("Calling do_boot");
	__GEN_COUT_INFO__ << "Calling do_boot" << __E__;
	__GEN_COUT__ << "Status before boot: " << daqinterface_state_ << __E__;
	PyObject* pName2 = PyUnicode_FromString("do_boot");
	PyObject* pStateArgs1 =
	    PyUnicode_FromString((ARTDAQTableBase::ARTDAQ_FCL_PATH + "/boot.txt").c_str());
	PyObject* res2 =
	    PyObject_CallMethodObjArgs(daqinterface_ptr_, pName2, pStateArgs1, NULL);
	std::string doBootOutput = captureStderrAndStdout_("do_boot");
	__COUT_MULTI_LBL__(0, doBootOutput, "do_boot");

	if(res2 == NULL)
	{
		std::string err = capturePyErr();
		__GEN_COUT_INFO__ << "Error on first boost attempt, recovering and retrying: "
		                  << err << __E__;

		PyObject* pName = PyUnicode_FromString("do_recover");
		PyObject* res   = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, NULL);
		__COUT_MULTI_LBL__(0, captureStderrAndStdout_("do_recover"), "do_recover");

		if(res == NULL)
		{
			std::string err = capturePyErr();
			__GEN_SS__ << "Error calling recover transition!!!! " << err << __E__;
			if(doBootOutput.size() > OUT_ON_ERR_SIZE)  //last OUT_ON_ERR_SIZE chars only
				ss << "... last " << OUT_ON_ERR_SIZE
				   << " characters: " << doBootOutput.substr(doBootOutput.size() - 1000);
			else
				ss << doBootOutput;
			__GEN_SS_THROW__;
		}

		thread_progress_bar_.step();
		set_thread_message_("Calling do_boot (retry)");
		__GEN_COUT_INFO__ << "Calling do_boot again" << __E__;
		__GEN_COUT__ << "Status before boot: " << daqinterface_state_ << __E__;
		PyObject* res3 =
		    PyObject_CallMethodObjArgs(daqinterface_ptr_, pName2, pStateArgs1, NULL);
		doBootOutput = captureStderrAndStdout_("do_boot (retry)");
		__COUT_MULTI_LBL__(0, doBootOutput, "do_boot (retry)");

		if(res3 == NULL)
		{
			std::string err = capturePyErr();
			__GEN_SS__ << "Error calling boot transition (2nd try): " << err << __E__;
			if(doBootOutput.size() > OUT_ON_ERR_SIZE)  //last OUT_ON_ERR_SIZE chars only
				ss << "... last " << OUT_ON_ERR_SIZE
				   << " characters: " << doBootOutput.substr(doBootOutput.size() - 1000);
			else
				ss << doBootOutput;
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
		PyObject* pName3      = PyUnicode_FromString("do_config");
		PyObject* pStateArgs2 = Py_BuildValue("[s]", FAKE_CONFIG_NAME);
		PyObject* res3 =
		    PyObject_CallMethodObjArgs(daqinterface_ptr_, pName3, pStateArgs2, NULL);
		doConfigOutput = captureStderrAndStdout_("do_config");
		__COUT_MULTI_LBL__(0, doConfigOutput, "do_config");
		if(res3 == NULL)
		{
			std::string err = capturePyErr("do_config");
			__GEN_SS__ << "Error calling config transition: " << err << __E__;
			__GEN_SS_THROW__;
		}
		const char* res_cstr = PyUnicode_AsUTF8(res3);
		__SUP_COUTT__ << "do_config result=" << (res_cstr ? res_cstr : "") << __E__;
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
			PyObject* pName = PyUnicode_FromString("do_stop_running");
			PyObject* res   = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, NULL);
			__COUT_MULTI_LBL__(
			    0, captureStderrAndStdout_("do_stop_running"), "do_stop_running");

			if(res == NULL)
			{
				std::string err = capturePyErr();
				__SS__ << "Error calling  DAQ Interface stop transition: " << err
				       << __E__;
				__SUP_SS_THROW__;
			}
		}

		PyObject* pName = PyUnicode_FromString("do_command");
		PyObject* pArg  = PyUnicode_FromString("Shutdown");
		PyObject* res = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, pArg, NULL);
		__COUT_MULTI_LBL__(
		    0, captureStderrAndStdout_("do_command Shutdown"), "do_command Shutdown");

		if(res == NULL)
		{
			std::string err = capturePyErr();
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

	PyObject* pName = PyUnicode_FromString("do_command");
	PyObject* pArg  = PyUnicode_FromString("Pause");
	PyObject* res   = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, pArg, NULL);
	__COUT_MULTI_LBL__(
	    0, captureStderrAndStdout_("do_command Pause"), "do_command Pause");

	if(res == NULL)
	{
		std::string err = capturePyErr();
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
	PyObject* pName = PyUnicode_FromString("do_command");
	PyObject* pArg  = PyUnicode_FromString("Resume");
	PyObject* res   = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, pArg, NULL);
	__COUT_MULTI_LBL__(
	    0, captureStderrAndStdout_("do_command Resume"), "do_command Resume");

	if(res == NULL)
	{
		std::string err = capturePyErr();
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
		PyObject* pName      = PyUnicode_FromString("do_start_running");
		int       run_number = std::stoi(runNumber);
		PyObject* pStateArgs = PyLong_FromLong(run_number);
		PyObject* res =
		    PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, pStateArgs, NULL);
		__COUT_MULTI_LBL__(
		    0, captureStderrAndStdout_("do_start_running"), "do_start_running");

		thread_progress_bar_.step();

		if(res == NULL)
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
	PyObject* pName = PyUnicode_FromString("do_stop_running");
	PyObject* res   = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, NULL);
	__COUT_MULTI_LBL__(0, captureStderrAndStdout_("do_stop_running"), "do_stop_running");

	if(res == NULL)
	{
		std::string err = capturePyErr();
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

	PyObject* pName = PyUnicode_FromString("do_recover");
	PyObject* res   = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, NULL);
	__COUT_MULTI_LBL__(0, captureStderrAndStdout_("do_recover"), "do_recover");

	if(res == NULL)
	{
		std::string err = capturePyErr();
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
	auto                                    apps = getAndParseProcessInfo_();
	std::vector<SupervisorInfo::SubappInfo> output;
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

		output.push_back(info);
	}
	return output;
}

//==============================================================================
std::string ots::ARTDAQSupervisor::capturePyErr(std::string label /* = "" */)
{
	return captureStderrAndStdout_(
	    label);  //new way with Tee output of stdout and stderr to tee_buffer

	PyErr_Print();  // dump the Python exception <-- this writes into stringIO_err, not the terminal
	if(label.size())
		label += ' ';  //for nice printing

	PyObject*   err_text = PyObject_CallMethod(stringIO_err, "getvalue", NULL);
	std::string err      = "";
	if(!err_text)
		err = "Capture of " + label + "PyErr failed.";
	else
		err = "Capture of " + label + "PyErr: " + std::string(PyUnicode_AsUTF8(err_text));

	//clear buffer for reuse
	{
		PyObject* r1 = PyObject_CallMethod(stringIO_err, "seek", "i", 0);
		Py_XDECREF(r1);
		PyObject* r2 = PyObject_CallMethod(stringIO_err, "truncate", NULL);
		Py_XDECREF(r2);
	}
	return err;
}  //end captureStderr()

//==============================================================================
std::string ots::ARTDAQSupervisor::captureStderrAndStdout_(std::string label /* = "" */)
{
	if(!stringIO_out)
		return "";  // Not defined
	if(label.size())
		label += ' ';  //for nice printing

	std::string outString = "";
	PyObject*   out       = PyObject_CallMethod(stringIO_out, "getvalue", NULL);
	if(out == NULL)
		return "";

	const char* text = PyUnicode_AsUTF8(out);
	// use text
	Py_DECREF(out);

	return text;  //new way with Tee output of stdout and stderr to tee_buffer

	try
	{
		//------------- capture stdout and stderr
		PyObject* out_text = PyObject_CallMethod(stringIO_out, "getvalue", NULL);
		if(!out_text)
			PyErr_Print();  // dump the Python exception <-- this writes into stringIO_err, not the terminal
		else
		{
			const char* out_cstr = PyUnicode_AsUTF8(out_text);
			if(out_cstr && strlen(out_cstr))
				outString = "Captured " + label + "stdout:\n" +
				            std::string(out_cstr ? out_cstr : "") + "\n";
			else
				outString = "Captured " + label + "stdout empty.\n";
		}

		std::string errString = "";
		PyObject*   err_text  = PyObject_CallMethod(stringIO_err, "getvalue", NULL);
		if(!err_text)
			__SUP_COUT__ << "Capture of " << label << "stderr failed.";
		else
		{
			const char* err_cstr = PyUnicode_AsUTF8(err_text);
			if(err_cstr && strlen(err_cstr))
				errString = "Captured " + label + "stderr:\n" +
				            std::string(err_cstr ? err_cstr : "") + "\n";
			else
				errString = "Captured " + label + "stderr empty.\n";
		}

		//clear buffers for reuse
		{
			PyObject* r1 = PyObject_CallMethod(stringIO_out, "seek", "i", 0);
			Py_XDECREF(r1);
			PyObject* r2 = PyObject_CallMethod(stringIO_out, "truncate", NULL);
			Py_XDECREF(r2);
		}
		{
			PyObject* r1 = PyObject_CallMethod(stringIO_err, "seek", "i", 0);
			Py_XDECREF(r1);
			PyObject* r2 = PyObject_CallMethod(stringIO_err, "truncate", NULL);
			Py_XDECREF(r2);
		}
		//------------- end capture stdout and stderr
		return errString + outString;
	}
	catch(...)  //make sure to release python thread lock
	{
		__COUT_ERR__ << "Exception caught while capturing python output!" << __E__;
		throw;
	}

}  //end captureStderrAndStdout_()

//==============================================================================
void ots::ARTDAQSupervisor::getDAQState_()
{
	__SUP_COUTS__(50) << "Getting DAQInterface python lock" << __E__;
	std::lock_guard<std::recursive_mutex> lk(daqinterface_pythonMutex_);
	__SUP_COUTS__(50) << "Have DAQInterface python lock" << __E__;

	if(daqinterface_ptr_ == nullptr)
	{
		daqinterface_state_ = "";
		__SUP_COUT_WARN__
		    << "daqinterface_ptr_ is not initialized! Check logs for errors." << __E__;
		return;
	}

	int tries = 0;
	while(tries < 5)
	{
		PyObject* pName = PyUnicode_FromString("state");
		PyObject* pArg  = PyUnicode_FromString("DAQInterface");
		PyObject* res = PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, pArg, NULL);

		if(res == NULL)
		{
			tries++;
			std::string err = capturePyErr("getDAQState_");
			__SS__
			    << "Attempt n " << tries
			    << ". Error calling 'state' function from getDAQState_() - here was the "
			       "value: "
			    << err << "\n\n"
			    << StringMacros::stackTrace() << __E__;
			// __SUP_SS_THROW__;
			//do not throw, just mark state empty
			daqinterface_state_ = "";
			__COUT__ << ss.str();
			if(tries >= 5)
				__COUT_ERR__ << ss.str();
			usleep(100000);
			__SUP_COUTS__(2) << "no getDAQState_ state=" << daqinterface_state_ << __E__;
			continue;
		}
		daqinterface_state_ = std::string(PyUnicode_AsUTF8(res));
		__SUP_COUTS__(20) << "getDAQState_ state=" << daqinterface_state_ << __E__;
		break;
	}

}  // end getDAQState_()

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

	PyObject* pName = PyUnicode_FromString("artdaq_process_info");
	PyObject* pArg  = PyUnicode_FromString("DAQInterface");
	PyObject* pArg2 = PyBool_FromLong(true);
	PyObject* res =
	    PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, pArg, pArg2, NULL);

	if(res == NULL)
	{
		std::string err = capturePyErr();
		__SS__ << "Error calling artdaq_process_info function: " << err << __E__;
		__SUP_SS_THROW__;
		return "";
	}
	//cache status as latest
	std::lock_guard<std::mutex> lock(daqinterface_statusMutex_);
	daqinterface_status_ = std::string(PyUnicode_AsUTF8(res));
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
					PyObject* pName = PyUnicode_FromString("check_proc_heartbeats");
					PyObject* res =
					    PyObject_CallMethodObjArgs(daqinterface_ptr_, pName, NULL);
					__COUT_MULTI_LBL__(1,
					                   captureStderrAndStdout_("check_proc_heartbeats"),
					                   "check_proc_heartbeats");
					TLOG(TLVL_TRACE)
					    << "Done with DAQInterface::check_proc_heartbeats call";

					if(res == NULL)
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
