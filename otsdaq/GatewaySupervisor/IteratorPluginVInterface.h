#ifndef _ots_IteratorPluginVInterface_h_
#define _ots_IteratorPluginVInterface_h_

#include <functional>
#include <string>
#include <utility>

#include <xoap/MessageReference.h>
#include "otsdaq/Macros/CoutMacros.h" /* for XDAQ_CONST_CALL */

namespace xdaq
{
class ApplicationDescriptor;
}

namespace ots
{
class GatewaySupervisor;
class ConfigurationManagerRW;
class HttpXmlDocument;
class AllSupervisorInfo;
class FiniteStateMachine;
class TableGroupKey;
class SOAPParameters;

/// What the Iterator hands a plugin on every call. The plugin never sees Iterator
///	internals; everything it may touch is reachable from here.
struct IteratorPluginContext
{
	GatewaySupervisor*      supervisor;
	ConfigurationManagerRW* configurationManager;  ///< the Iterator worker's manager (worker-thread calls only)

	std::function<bool()>                   stopRequested;  ///< true once the operator asked to stop the plugin
	std::function<void(const std::string&)> setError;       ///< records an Iterator error message (locks internally)
	std::function<void(const std::string&)> setActiveCommandType;  ///< status text shown for the active command
};

/// ~~ IteratorPluginVInterface ~~
///	An experiment-specific procedure that takes over the Gateway Iterator for a while:
///	it runs as a reserved Iterator plan, owns the Iterator (normal plans are refused),
///	and may refuse DAQ transitions while it is touching hardware.
///
///	Loaded on demand by name from a request (never at startup), one instance per use:
///		StateMachineXgiHandler?StateMachine=iteratePluginCommand&plugin=<name>&pluginCommand=<cmd>
///	Flow managed by the Iterator:
///		handleCommand("Start")   -> plugin takes ownership by making owns() return true
///		initialize()             -> worker thread, once, before the first step()
///		step()                   -> worker thread, repeatedly while owns(); return promptly on stopRequested()
///		handleCommand("Stop")    -> plugin releases by making owns() return false; "Halt" arrives from iterateHalt
///		finish()                 -> once, after stop/halt; Iterator holds stateMachineAccessMutex_
///		addStatus()              -> appended to getIterationPlanStatus while owns()
///	Any throw from initialize()/step() is reported as the Iterator error and the plugin
///	keeps ownership until the operator sends a stop command it accepts.
class IteratorPluginVInterface
{
  public:
	explicit IteratorPluginVInterface(const std::string& pluginName) : pluginName_(pluginName) {}
	virtual ~IteratorPluginVInterface(void) = default;

	const std::string& getPluginName(void) const { return pluginName_; }

	/// reserved Iterator plan name this plugin runs under (must not collide with saved plans)
	virtual const std::string& getPlanName(void) const = 0;

	/// operator command addressed to this plugin ("Start", "Stop", ...); throw to report an error
	virtual void handleCommand(const std::string& command, HttpXmlDocument& xml, IteratorPluginContext& context) = 0;

	virtual void initialize(IteratorPluginContext& context) = 0;
	virtual void step(IteratorPluginContext& context)       = 0;
	virtual void finish(IteratorPluginContext& context)     = 0;

	virtual void addStatus(HttpXmlDocument& xml, IteratorPluginContext& context) = 0;

	/// true from an accepted Start until finish() has run
	virtual bool owns(void) const = 0;
	/// true while a DAQ transition could collide with hardware access by this plugin
	virtual bool blocksDaqTransitions(void) const = 0;

  protected:
	/// ~~ Gateway access for derived plugins ~~
	///	GatewaySupervisor befriends this base class only, so the members a plugin
	///	legitimately needs are reached through these forwarders (defined in
	///	IteratorPluginVInterface.cc, which sees the full GatewaySupervisor).
	static const AllSupervisorInfo&                     gatewaySupervisorInfo(GatewaySupervisor* supervisor);
	static FiniteStateMachine&                          gatewayStateMachine(GatewaySupervisor* supervisor);  ///< non-const: FiniteStateMachine query methods are not const
	static const std::string&                           gatewayActiveStateMachineName(GatewaySupervisor* supervisor);
	static const std::pair<std::string, TableGroupKey>& gatewayConfigurationTableGroup(GatewaySupervisor* supervisor);
	static xoap::MessageReference                       gatewaySendWithSOAPReply(GatewaySupervisor* supervisor, XDAQ_CONST_CALL xdaq::ApplicationDescriptor* descriptor, const std::string& command);
	static xoap::MessageReference                       gatewaySendWithSOAPReply(GatewaySupervisor* supervisor, XDAQ_CONST_CALL xdaq::ApplicationDescriptor* descriptor, const std::string& command, const SOAPParameters& parameters);

  private:
	const std::string pluginName_;
};

}  // namespace ots

#endif /* _ots_IteratorPluginVInterface_h_ */
