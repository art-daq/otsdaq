#ifndef _ots_SOAPMessenger_h
#define _ots_SOAPMessenger_h

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#if __GNUC__ >= 8
#pragma GCC diagnostic ignored "-Wcatch-value"
#endif
#if __GNUC__ >= 9
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#endif
#include <xdaq/Application.h>
#pragma GCC diagnostic pop
#include <xcept/tools.h>
#include "otsdaq/Macros/XDAQApplicationMacros.h"

#include "otsdaq/SOAPUtilities/SOAPUtilities.h" /* for SOAPCommand & SOAPParameters */

#include <string>
#include "otsdaq/Macros/CoutMacros.h" /* for XDAQ_CONST_CALL */

namespace ots
{
class SOAPMessenger : public virtual toolbox::lang::Class
{
  public:
	SOAPMessenger(xdaq::Application* application);
	SOAPMessenger(const SOAPMessenger& aSOAPMessenger);

	std::string send(XDAQ_CONST_CALL xdaq::ApplicationDescriptor* d, xoap::MessageReference message);
	std::string send(XDAQ_CONST_CALL xdaq::ApplicationDescriptor* d, SOAPCommand soapCommand);
	std::string send(XDAQ_CONST_CALL xdaq::ApplicationDescriptor* d, std::string command);
	std::string send(XDAQ_CONST_CALL xdaq::ApplicationDescriptor* d, std::string command, SOAPParameters parameters);
	std::string sendStatus(XDAQ_CONST_CALL xdaq::ApplicationDescriptor* d, std::string message);

	xoap::MessageReference sendWithSOAPReply(XDAQ_CONST_CALL xdaq::ApplicationDescriptor* d, xoap::MessageReference message);
	xoap::MessageReference sendWithSOAPReply(XDAQ_CONST_CALL xdaq::ApplicationDescriptor* d, SOAPCommand soapCommand);
	xoap::MessageReference sendWithSOAPReply(XDAQ_CONST_CALL xdaq::ApplicationDescriptor* d, std::string command);
	xoap::MessageReference sendWithSOAPReply(XDAQ_CONST_CALL xdaq::ApplicationDescriptor* d, std::string command, SOAPParameters parameters);

  protected:
	xdaq::Application* theApplication_;
};
}  // namespace ots
#endif
