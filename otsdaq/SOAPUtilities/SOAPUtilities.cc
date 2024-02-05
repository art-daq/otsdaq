#include "otsdaq/SOAPUtilities/SOAPUtilities.h"

#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

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
SOAPUtilities::SOAPUtilities(void) {}

//==============================================================================
SOAPUtilities::~SOAPUtilities(void) {}

//==============================================================================
xoap::MessageReference SOAPUtilities::makeSOAPMessageReference(SOAPCommand soapCommand)
{
	if(soapCommand.hasParameters())
		return makeSOAPMessageReference(soapCommand.getCommand(), soapCommand.getParameters());
	else
		return makeSOAPMessageReference(soapCommand.getCommand());
}

//==============================================================================
xoap::MessageReference SOAPUtilities::makeSOAPMessageReference(std::string command)
{
	xoap::MessageReference message  = xoap::createMessage();
	xoap::SOAPEnvelope     envelope = message->getSOAPPart().getEnvelope();
	xoap::SOAPName         name     = envelope.createName(command, "xdaq", XDAQ_NS_URI);
	xoap::SOAPBody         body     = envelope.getBody();
	body.addBodyElement(name);
	return message;
}

//==============================================================================
xoap::MessageReference SOAPUtilities::makeSOAPMessageReference(std::string command, SOAPParameters parameters)
{
	//__COUT__ << "Command: " << command << " par size: " << parameters.size() <<
	// std::endl;
	if(parameters.size() == 0)
		return makeSOAPMessageReference(command);
	xoap::MessageReference message       = xoap::createMessage();
	xoap::SOAPEnvelope     envelope      = message->getSOAPPart().getEnvelope();
	xoap::SOAPName         name          = envelope.createName(command, "xdaq", XDAQ_NS_URI);
	xoap::SOAPBody         body          = envelope.getBody();
	xoap::SOAPElement      bodyCommand   = body.addBodyElement(name);
	xoap::SOAPName         parameterName = envelope.createName("Null");
	for(SOAPParameters::iterator it = parameters.begin(); it != parameters.end(); it++)
	{
		parameterName = envelope.createName(it->first);
		bodyCommand.addAttribute(parameterName, it->second);
	}

	return message;
}

//==============================================================================
xoap::MessageReference SOAPUtilities::makeSOAPMessageReference(std::string command, std::string fileName)
{
	__COUT__ << "SOAP XML file path : " << fileName << std::endl;
	xoap::MessageReference message  = xoap::createMessage();
	xoap::SOAPPart         soap     = message->getSOAPPart();
	xoap::SOAPEnvelope     envelope = soap.getEnvelope();
	xoap::AttachmentPart*  attachment;
	attachment = message->createAttachmentPart();
	attachment->setContent(fileName);
	attachment->setContentId("SOAPTEST1");
	attachment->addMimeHeader("Content-Description", "This is a SOAP message with attachments");
	message->addAttachmentPart(attachment);
	xoap::SOAPName name = envelope.createName(command, "xdaq", XDAQ_NS_URI);
	xoap::SOAPBody body = envelope.getBody();
	body.addBodyElement(name);
	return message;
}

//==============================================================================
void SOAPUtilities::addParameters(xoap::MessageReference& message, SOAPParameters parameters)
{
	//__COUT__ << "adding parameters!!!!!!" << std::endl;
	if(parameters.size() == 0)
		return;
	xoap::SOAPEnvelope envelope = message->getSOAPPart().getEnvelope();
	xoap::SOAPBody     body     = envelope.getBody();
	xoap::SOAPName     name(translate(message).getCommand(), "xdaq", XDAQ_NS_URI);

	std::vector<xoap::SOAPElement> bodyList = body.getChildElements();
	for(std::vector<xoap::SOAPElement>::iterator it = bodyList.begin(); it != bodyList.end(); it++)
	{
		if((*it).getElementName() == name)
		{
			for(SOAPParameters::iterator itPar = parameters.begin(); itPar != parameters.end(); itPar++)
			{
				xoap::SOAPName parameterName = envelope.createName(itPar->first);
				(*it).addAttribute(parameterName, itPar->second);
			}
		}
	}
}

//==============================================================================
SOAPCommand SOAPUtilities::translate(const xoap::MessageReference& message)
{
	SOAPCommand                           soapCommand;
	const std::vector<xoap::SOAPElement>& bodyList = message->getSOAPPart().getEnvelope().getBody().getChildElements();
	for(std::vector<xoap::SOAPElement>::const_iterator it = bodyList.begin(); it != bodyList.end(); it++)
	{
		xoap::SOAPElement element = *it;
		soapCommand.setCommand(element.getElementName().getLocalName());
		DOMNamedNodeMap* parameters = element.getDOM()->getAttributes();
		for(unsigned int i = 0; i < parameters->getLength(); i++)
			soapCommand.setParameter(xoap::XMLCh2String(parameters->item(i)->getNodeName()), xoap::XMLCh2String(parameters->item(i)->getNodeValue()));
	}
	return soapCommand;
}

//==============================================================================
std::string SOAPUtilities::receive(const xoap::MessageReference& message, SOAPCommand& soapCommand) { return receive(message, soapCommand.getParametersRef()); }

//==============================================================================
std::string SOAPUtilities::receive(const xoap::MessageReference& message)
{
	// NOTE it is assumed that there is only 1 command for each message (that's why we use
	// begin)
	return (message->getSOAPPart().getEnvelope().getBody().getChildElements()).begin()->getElementName().getLocalName();
}

//==============================================================================
std::string SOAPUtilities::receive(const xoap::MessageReference& message, SOAPParameters& parameters)
{
	xoap::SOAPEnvelope             envelope    = message->getSOAPPart().getEnvelope();
	std::vector<xoap::SOAPElement> bodyList    = envelope.getBody().getChildElements();
	xoap::SOAPElement              command     = bodyList[0];
	std::string                    commandName = command.getElementName().getLocalName();
	xoap::SOAPName                 name        = envelope.createName("Key");

	for(SOAPParameters::iterator it = parameters.begin(); it != parameters.end(); it++)
	{
		name = envelope.createName(it->first);

		try
		{
			it->second = command.getAttributeValue(name);
			// if( parameters.getParameter(it->first).isEmpty() )
			//{
			//    __COUT__ << "Complaint from " <<
			//    (theApplication_->getApplicationDescriptor()->getClassName()) <<
			//    std::endl;
			//    __COUT__ << " : Parameter "<< it->first
			//    << " does not exist in the list of incoming parameters!" << std::endl;
			//    __COUT__ << "It could also be because you passed an empty std::string"
			//    << std::endl;
			//    //assert(0);
			//};
		}
		catch(xoap::exception::Exception& e)
		{
			__COUT__ << "Parameter " << it->first << " does not exist in the list of incoming parameters!" << std::endl;
			XCEPT_RETHROW(xoap::exception::Exception, "Looking for parameter that does not exist!", e);
		}
	}

	return commandName;
}
