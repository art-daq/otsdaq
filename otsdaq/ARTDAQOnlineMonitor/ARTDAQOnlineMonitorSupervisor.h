#ifndef _ots_ARTDAQOnlineMonitorSupervisor_h
#define _ots_ARTDAQOnlineMonitorSupervisor_h

#include "otsdaq/CoreSupervisors/CoreSupervisorBase.h"

#include <memory>
#include <set>
#include <string>
#include <thread>

namespace ots
{
class ARTDAQOnlineMonitorSupervisor : public CoreSupervisorBase
{
  public:
	XDAQ_INSTANTIATOR();

	ARTDAQOnlineMonitorSupervisor(xdaq::ApplicationStub* s);
	virtual ~ARTDAQOnlineMonitorSupervisor(void);
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

  private:
	void RunArt(const std::string& config_file, const std::shared_ptr<std::atomic<pid_t>>& pid_out);

	void StartArtProcess(const std::string& config_file);

	void ShutdownArtProcess();

	const std::string                   supervisorContextUID_;
	const std::string                   supervisorApplicationUID_;
	const std::string                   supervisorConfigurationPath_;
	std::string                         config_file_name_;
	std::shared_ptr<std::atomic<pid_t>> art_pid_;
	std::atomic<bool>                   restart_art_;
	bool                                should_restart_art_;
	int                                 minimum_art_lifetime_s_{5};
	int                                 partition_;
	int                                 om_rank_;
};
}  // namespace ots
#endif  // _ots_ARTDAQOnlineMonitorSupervisor_h