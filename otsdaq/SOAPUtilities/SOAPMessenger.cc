#include "otsdaq/SOAPUtilities/SOAPMessenger.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"
#include "otsdaq/SOAPUtilities/SOAPCommand.h"
#include "otsdaq/SOAPUtilities/SOAPUtilities.h"

#include <xdaq/NamespaceURI.h>
#include <xoap/MessageReference.h>
#include <xoap/Method.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnon-virtual-dtor"
#include <xoap/MessageFactory.h>
#pragma GCC diagnostic pop
#include <xoap/AttachmentPart.h>
#include <xoap/SOAPBody.h>
#include <xoap/SOAPEnvelope.h>
#include <xoap/SOAPPart.h>
#include <xoap/domutils.h>

using namespace ots;

//==============================================================================
SOAPMessenger::SOAPMessenger(xdaq::Application* application) : theApplication_(application) {}

//==============================================================================
SOAPMessenger::SOAPMessenger(const SOAPMessenger& aSOAPMessenger) : theApplication_(aSOAPMessenger.theApplication_) {}

//==============================================================================
// in xdaq
//    xdaq::ApplicationDescriptor* sourceptr;
// void getURN()
//
std::string SOAPMessenger::send(XDAQ_CONST_CALL xdaq::ApplicationDescriptor* ind, xoap::MessageReference message)

{
	return SOAPUtilities::receive(sendWithSOAPReply(ind, message));
}

//==============================================================================
std::string SOAPMessenger::send(XDAQ_CONST_CALL xdaq::ApplicationDescriptor* d, SOAPCommand soapCommand)

{
	if(soapCommand.hasParameters())
		return send(d, soapCommand.getCommand(), soapCommand.getParameters());
	else
		return send(d, soapCommand.getCommand());
}

//==============================================================================
std::string SOAPMessenger::send(XDAQ_CONST_CALL xdaq::ApplicationDescriptor* d, std::string command)

{
	xoap::MessageReference message = SOAPUtilities::makeSOAPMessageReference(command);
	std::string            msgStr;
	try
	{
		msgStr = send(d, message);
	}
	catch(...)
	{
		__COUT__ << "send failed?!" << __E__;
		__COUT__ << SOAPUtilities::translate(message) << __E__;
		throw;
	}

	return msgStr;
}

//==============================================================================
std::string SOAPMessenger::send(XDAQ_CONST_CALL xdaq::ApplicationDescriptor* ind, std::string cmd, SOAPParameters parameters)

{
	return SOAPUtilities::receive(sendWithSOAPReply(ind, cmd, parameters));
}

//==============================================================================
xoap::MessageReference SOAPMessenger::sendWithSOAPReply(XDAQ_CONST_CALL xdaq::ApplicationDescriptor* ind, std::string cmd)

{
	return sendWithSOAPReply(ind, SOAPUtilities::makeSOAPMessageReference(cmd));
}

//==============================================================================
xoap::MessageReference SOAPMessenger::sendWithSOAPReply(XDAQ_CONST_CALL xdaq::ApplicationDescriptor* ind, xoap::MessageReference message)

{
	// const_cast away the const
	//	so that this line is compatible with slf6 and slf7 versions of xdaq
	//	where they changed to XDAQ_CONST_CALL xdaq::ApplicationDescriptor* in slf7
	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* d = const_cast<xdaq::ApplicationDescriptor*>(ind);
	try
	{
		message->getMimeHeaders()->setHeader("Content-Location", d->getURN());
		message->getMimeHeaders()->setHeader("xdaq-pthttp-connection-timeout", "30" /*seconds*/);

		//__COUT__ << d->getURN() << __E__;
		//__COUT__ << SOAPUtilities::translate(message) << __E__;
		// std::string mystring;
		// message->writeTo(mystring);
		//__COUT__<< mystring << std::endl;

		xoap::MessageReference reply = theApplication_->getApplicationContext()->postSOAP(message, *(theApplication_->getApplicationDescriptor()), *d);
		return reply;
	}
	catch(xdaq::exception::Exception& e)
	{
		//__COUT_ERR__ << "This application failed to send a SOAP message to " << d->getClassName() << " instance " << d->getInstance()
		//         << " re-throwing exception = " << xcept::stdformat_exception_history(e);
		// std::string mystring;
		// message->writeTo(mystring);
		//__COUT_ERR__ << mystring << std::endl;
		XCEPT_RETHROW(xdaq::exception::Exception, "Failed to send SOAP command.", e);
	}
}

//==============================================================================
xoap::MessageReference SOAPMessenger::sendWithSOAPReply(XDAQ_CONST_CALL xdaq::ApplicationDescriptor* ind, std::string cmd, SOAPParameters parameters)

{
	return sendWithSOAPReply(ind, SOAPUtilities::makeSOAPMessageReference(cmd, parameters));
}

//==============================================================================
std::string SOAPMessenger::sendStatus(XDAQ_CONST_CALL xdaq::ApplicationDescriptor* ind, std::string message)

{
	// const_cast away the const
	//	so that this line is compatible with slf6 and slf7 versions of xdaq
	//	where they changed to XDAQ_CONST_CALL xdaq::ApplicationDescriptor* in slf7
	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* d = const_cast<xdaq::ApplicationDescriptor*>(ind);

	std::string cmd = "StatusNotification";
	try
	{
		timeval tv;  // keep track of when the message comes
		gettimeofday(&tv, NULL);

		std::stringstream ss;
		SOAPParameters    parameters;
		parameters.addParameter("Description", message);
		ss.str("");
		ss << tv.tv_sec;
		parameters.addParameter("Time", ss.str());
		ss.str("");
		ss << tv.tv_usec;
		parameters.addParameter("usec", ss.str());
		return send(d, cmd, parameters);
	}
	catch(xdaq::exception::Exception& e)
	{
		__COUT__ << "This application failed to send a SOAP error message to " << d->getClassName() << " instance " << d->getInstance()
		         << " with command = " << cmd << " re-throwing exception = " << xcept::stdformat_exception_history(e) << std::endl;
		XCEPT_RETHROW(xdaq::exception::Exception, "Failed to send SOAP command.", e);
	}
}
