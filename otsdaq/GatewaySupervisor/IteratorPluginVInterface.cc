#include "otsdaq/GatewaySupervisor/IteratorPluginVInterface.h"
#include "otsdaq/GatewaySupervisor/GatewaySupervisor.h"

using namespace ots;

//==============================================================================
const AllSupervisorInfo& IteratorPluginVInterface::gatewaySupervisorInfo(
    GatewaySupervisor* supervisor)
{
	return supervisor->allSupervisorInfo_;
}

//==============================================================================
FiniteStateMachine& IteratorPluginVInterface::gatewayStateMachine(
    GatewaySupervisor* supervisor)
{
	return supervisor->theStateMachine_;
}

//==============================================================================
const std::string& IteratorPluginVInterface::gatewayActiveStateMachineName(
    GatewaySupervisor* supervisor)
{
	return supervisor->activeStateMachineName_;
}

//==============================================================================
const std::pair<std::string, TableGroupKey>&
IteratorPluginVInterface::gatewayConfigurationTableGroup(GatewaySupervisor* supervisor)
{
	return supervisor->theConfigurationTableGroup_;
}

//==============================================================================
xoap::MessageReference IteratorPluginVInterface::gatewaySendWithSOAPReply(
    GatewaySupervisor*                           supervisor,
    XDAQ_CONST_CALL xdaq::ApplicationDescriptor* descriptor,
    const std::string&                           command)
{
	return supervisor->SOAPMessenger::sendWithSOAPReply(descriptor, command);
}

//==============================================================================
xoap::MessageReference IteratorPluginVInterface::gatewaySendWithSOAPReply(
    GatewaySupervisor*                           supervisor,
    XDAQ_CONST_CALL xdaq::ApplicationDescriptor* descriptor,
    const std::string&                           command,
    const SOAPParameters&                        parameters)
{
	return supervisor->SOAPMessenger::sendWithSOAPReply(descriptor, command, parameters);
}
