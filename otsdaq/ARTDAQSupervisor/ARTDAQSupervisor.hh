#ifndef _ots_ARTDAQSupervisor_h
#define _ots_ARTDAQSupervisor_h

#if __cplusplus > 201402L
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wregister"
#include <Python.h>
#pragma GCC diagnostic pop
#else
#include <Python.h>
#endif

#include <mutex>
#include <set>
#include <thread>

#include "artdaq/ExternalComms/CommanderInterface.hh"
#include "otsdaq/CoreSupervisors/CoreSupervisorBase.h"
#include "otsdaq/TablePlugins/ARTDAQTableBase/ARTDAQTableBase.h"

namespace ots
{
/// ARTDAQSupervisor
///	This class provides the otsdaq Supervisor interface to a single artdaq Data Logger.
class ARTDAQSupervisor : public CoreSupervisorBase
{
  public:
	XDAQ_INSTANTIATOR();

	struct DAQInterfaceProcessInfo
	{
		std::string label;
		std::string host;
		int         port;
		int         subsystem;
		int         rank;
		std::string state;
	};

	ARTDAQSupervisor(xdaq::ApplicationStub* s);
	virtual ~ARTDAQSupervisor(void);

	void init(void);
	void destroy(void);

	virtual void transitionConfiguring(toolbox::Event::Reference event) override;
	virtual void transitionHalting(toolbox::Event::Reference event) override;
	virtual void transitionInitializing(toolbox::Event::Reference event) override;
	virtual void transitionPausing(toolbox::Event::Reference event) override;
	virtual void transitionResuming(toolbox::Event::Reference event) override;
	virtual void transitionStarting(toolbox::Event::Reference event) override;
	virtual void transitionStopping(toolbox::Event::Reference event) override;
	virtual void enteringError(toolbox::Event::Reference event) override;

	virtual std::vector<SupervisorInfo::SubappInfo> getSubappInfo(void) override;
	virtual std::string                             getStatusProgressDetail(void) override
	{
		if(!theStateMachine_.isInTransition() &&
		   (theStateMachine_.getCurrentStateName() ==
		        RunControlStateMachine::HALTED_STATE_NAME ||
		    theStateMachine_.getCurrentStateName() ==
		        RunControlStateMachine::INITIAL_STATE_NAME))
			return CoreSupervisorBase::getStatusProgressDetail();

		std::lock_guard<std::mutex> lk(thread_mutex_);
		__COUTVS__(20, thread_progress_message_);
		return thread_progress_message_;
	}  //end getStatusProgressDetail()

	std::list<
	    std::pair<DAQInterfaceProcessInfo, std::unique_ptr<artdaq::CommanderInterface>>>
	makeCommandersFromProcessInfo();

	// Hostnames of all enabled artdaq processes from the active configuration.
	// Reflects the configuration's intended deployment (works even when
	// DAQInterface is not running). Note: config does NOT carry the runtime
	// xmlrpc commander ports -- this is for host discovery only.
	std::set<std::string> getConfiguredArtdaqHosts(void);

	static std::list<std::string> tokenize_(std::string const& input);

  private:
	void configuringThread(void);
	void startingThread(void);

	/// RAII wrapper for Python objects to ensure cleanup even on exception
	struct PyObjectGuard
	{
		PyObject* obj;
		explicit PyObjectGuard(PyObject* o)
		    : obj(o) {}
		~PyObjectGuard()
		{
			if(obj)
				Py_DECREF(obj);
		}
		PyObjectGuard(const PyObjectGuard&)            = delete;
		PyObjectGuard& operator=(const PyObjectGuard&) = delete;
		PyObject*      get() const { return obj; }
	};

	PyObject *daqinterface_ptr_, *stringIO_out_,
	    *stringIO_err_;  //stringIO_err_ not needed with new Tee Buffer solution
	std::recursive_mutex         daqinterface_pythonMutex_;
	std::mutex                   daqinterface_statusMutex_;
	std::string                  daqinterface_status_;
	int                          partition_;
	std::string                  daqinterface_state_;
	std::unique_ptr<std::thread> runner_thread_;
	std::atomic<bool>            runner_running_;

	std::mutex                         thread_mutex_;
	ProgressBar                        thread_progress_bar_;
	std::string                        thread_progress_message_;
	std::string                        thread_error_message_;
	int                                last_thread_progress_read_;
	time_t                             last_thread_progress_update_;
	std::map<std::string, std::string> label_to_proc_type_map_;

	std::string capturePyErr(std::string label = "");
	bool        checkPythonError(
	           PyObject* result);  // Check if Python call failed (returns true on error)
	std::string                        captureStderrAndStdout_(std::string label = "");
	void                               getDAQState_(void);
	std::string                        getProcessInfo_(void);
	std::string                        artdaqStateToOtsState(std::string state);
	std::string                        labelToProcType_(std::string label);
	std::list<DAQInterfaceProcessInfo> getAndParseProcessInfo_(void);
	void                               daqinterfaceRunner_(void);
	void                               stop_runner_(void);
	void                               start_runner_(void);
	void                               set_thread_message_(std::string msg)
	{
		std::lock_guard<std::mutex> lk(thread_mutex_);
		thread_progress_message_ = msg;
	}
};

}  // namespace ots

#endif
