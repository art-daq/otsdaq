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
#include <thread>

#include "artdaq/ExternalComms/CommanderInterface.hh"
#include "otsdaq/CoreSupervisors/CoreSupervisorBase.h"
#include "otsdaq/TablePlugins/ARTDAQTableBase/ARTDAQTableBase.h"

namespace ots
{
// ARTDAQSupervisor
//	This class provides the otsdaq Supervisor interface to a single artdaq Data Logger.
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

	virtual void                                    transitionConfiguring(toolbox::Event::Reference event) override;
	virtual void                                    transitionHalting(toolbox::Event::Reference event) override;
	virtual void                                    transitionInitializing(toolbox::Event::Reference event) override;
	virtual void                                    transitionPausing(toolbox::Event::Reference event) override;
	virtual void                                    transitionResuming(toolbox::Event::Reference event) override;
	virtual void                                    transitionStarting(toolbox::Event::Reference event) override;
	virtual void                                    transitionStopping(toolbox::Event::Reference event) override;
	virtual void                                    enteringError(toolbox::Event::Reference event) override;
	virtual std::vector<SupervisorInfo::SubappInfo> getSubappInfo(void) override;
	virtual std::string                             getStatusProgressDetail(void) override
	{
		if(!theStateMachine_.isInTransition() && (theStateMachine_.getCurrentStateName() == RunControlStateMachine::HALTED_STATE_NAME ||
		                                          theStateMachine_.getCurrentStateName() == RunControlStateMachine::INITIAL_STATE_NAME))
			return CoreSupervisorBase::getStatusProgressDetail();

		std::lock_guard<std::mutex> lk(thread_mutex_);
		// __COUTV__(thread_progress_message_);
		return thread_progress_message_;
	}

	std::list<std::pair<DAQInterfaceProcessInfo, std::unique_ptr<artdaq::CommanderInterface>>> makeCommandersFromProcessInfo();

	static std::list<std::string> tokenize_(std::string const& input);

  private:
	void configuringThread(void);
	void startingThread(void);

	PyObject*                    daqinterface_ptr_;
	std::recursive_mutex         daqinterface_mutex_;
	int                          partition_;
	std::string                  daqinterface_state_;
	std::unique_ptr<std::thread> runner_thread_;
	std::atomic<bool>            runner_running_;

	std::mutex  thread_mutex_;
	ProgressBar thread_progress_bar_;
	std::string thread_progress_message_;
	std::string thread_error_message_;
	int         last_thread_progress_read_;
	time_t      last_thread_progress_update_;

	void                               getDAQState_(void);
	std::string                        getProcessInfo_(void);
	std::string                        artdaqStateToOtsState(std::string state);
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
