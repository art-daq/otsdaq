#ifndef _ots_TFMSupervisor_h
#define _ots_TFMSupervisor_h

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
#include <future>

#include <xmlrpc-c/base.h>
#include <xmlrpc-c/client.h>
#include "xmlrpc-c/config.h"  /* information about this build environment, recomended after base and client */

#include "artdaq/ExternalComms/CommanderInterface.hh"
#include "otsdaq/CoreSupervisors/CoreSupervisorBase.h"
#include "otsdaq/TablePlugins/ARTDAQTableBase/ARTDAQTableBase.h"

namespace ots
{
// TFMSupervisor
//	This class provides the otsdaq Supervisor interface to a single artdaq Data Logger.
class TFMSupervisor : public CoreSupervisorBase
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

	TFMSupervisor(xdaq::ApplicationStub* s);
	virtual ~TFMSupervisor(void);

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
    std::string                                     getDAQReport();
    virtual void                                    setSupervisorPropertyDefaults(void) override;
	virtual void                                    forceSupervisorPropertyValues(void) override;

    virtual void                                    request (const std::string& requestType,cgicc::Cgicc& cgiIn, HttpXmlDocument& xmlOut, const WebUsers::RequestUserInfo& userInfo) override;

	std::list<std::pair<DAQInterfaceProcessInfo, std::unique_ptr<artdaq::CommanderInterface>>> makeCommandersFromProcessInfo();

	static std::list<std::string> tokenize_(std::string const& input);

  private:
    std::string xmlrpc(const char* cmd, const char* format, const char* args);
    int xmlrpc(const char* cmd, const char* format, const char* args, std::string& result);
    //int xmlrpc_timeout(const char* cmd, const char* format, const char* args, std::string& result, int timeout_ms = 100);
    void xmlrpc_setup();
    void xmlrpc_cleanup();

    int killAllRunningFarmManagers();
    int writeSettings(std::string configName);

    void setTfmState_(std::string state) {
        if(state != tfm_state_) resetTfmStateTime_();
        tfm_state_ = state;
    }
    void resetTfmStateTime_() {tfm_last_state_change_ = std::chrono::high_resolution_clock::now();}

    bool checkTransitionTimeout_(int timeout_ms) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - tfm_last_state_change_
                //).count() > current_transition_timeout_ms_;
                ).count() > timeout_ms;
    }

	//PyObject*                    daqinterface_ptr_;
	std::recursive_mutex         daqinterface_mutex_;
	int                          partition_;
	//std::string                  daqinterface_state_;
    int                          tfm_connected_;
    std::string                  tfm_state_;
    std::chrono::time_point<std::chrono::high_resolution_clock> tfm_last_state_change_;
	std::unique_ptr<std::thread> runner_thread_;
	std::atomic<bool>            runner_running_;
    //std::future<void>            tfm_;
    pid_t                        tfm_;
    std::list<std::pair<DAQInterfaceProcessInfo, std::unique_ptr<artdaq::CommanderInterface>>> commanders_;
    bool                         daq_repoers_enabled_;
    std::map<std::string, std::string> daq_reports_;
    std::map<std::string, std::chrono::time_point<std::chrono::high_resolution_clock>> daq_reports_update_time_;

    // xmlrpc variables
    char                         xmlrpcUrl_[100];
    xmlrpc_env                   xmlrpcEnv_;
    //xmlrpc_client*               xmlrpcClient_;
    //std::mutex                   xmlrpc_mtx_;
    //std::condition_variable      xmlrpc_cv_;
    //static int                   xmlrpcClient_abort_;


	std::mutex  thread_mutex_;
	//ProgressBar thread_progress_bar_;
	std::string thread_progress_message_;
	//std::string thread_error_message_;
	//int         last_thread_progress_read_;
	//time_t      last_thread_progress_update_;
    //time_t::rep   current_transition_timeout_ms_;
	std::map<std::string, std::string> label_to_proc_type_map_;

	void                               getDAQState_(void);
    void                               getDAQReports_(void);
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
