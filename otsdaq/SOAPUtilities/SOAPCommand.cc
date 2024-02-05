#include "otsdaq/SOAPUtilities/SOAPCommand.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <xoap/SOAPElement.h>
#include <xoap/SOAPEnvelope.h>
#include <xoap/domutils.h>

#include <iostream>

using namespace ots;

//==============================================================================
SOAPCommand::SOAPCommand(void) {}

//==============================================================================
SOAPCommand::SOAPCommand(const xoap::MessageReference& message) { translate(message); }

//==============================================================================
SOAPCommand::SOAPCommand(std::string command) : command_(command) {}
//==============================================================================
SOAPCommand::SOAPCommand(std::string command, SOAPParameters parameters) : command_(command), parameters_(parameters) {}

//==============================================================================
SOAPCommand::SOAPCommand(std::string command, SOAPParameter parameter) : command_(command), parameters_(parameter) {}

//==============================================================================
SOAPCommand::~SOAPCommand(void) {}

// Getters
//==============================================================================
// const xoap::MessageReference SOAPCommand::translate(void)
//{
// FIXME
//}

//==============================================================================
const std::string& SOAPCommand::getCommand(void) const { return command_; }

//==============================================================================
const SOAPParameters& SOAPCommand::getParameters(void) const { return parameters_; }

SOAPParameters& SOAPCommand::getParametersRef() { return parameters_; }

//==============================================================================
std::string SOAPCommand::getParameterValue(std::string parameterName) const
{
	SOAPParameters::const_iterator it;
	if((it = parameters_.find(parameterName)) != parameters_.end())
		return it->second;
	return "";
}

//==============================================================================
unsigned int SOAPCommand::getParametersSize(void) const { return parameters_.size(); }
// Setters
//==============================================================================
void SOAPCommand::translate(const xoap::MessageReference& message)
{
	// A SOAP message can ONLY have 1 command so we get the .begin() of the vector of
	// commands
	xoap::SOAPElement messageCommand = *(message->getSOAPPart().getEnvelope().getBody().getChildElements().begin());
	command_                         = messageCommand.getElementName().getLocalName();
	DOMNamedNodeMap* parameters      = messageCommand.getDOM()->getAttributes();
	for(unsigned int i = 0; i < parameters->getLength(); i++)
	{
		parameters_.addParameter(xoap::XMLCh2String(parameters->item(i)->getNodeName()), xoap::XMLCh2String(parameters->item(i)->getNodeValue()));
	}
}

//==============================================================================
void SOAPCommand::setCommand(std::string command) { command_ = command; }

//==============================================================================
void SOAPCommand::setParameters(const SOAPParameters& parameters) { parameters_ = parameters; }

//==============================================================================
void SOAPCommand::setParameter(std::string parameterName, std::string parameterValue) { parameters_.addParameter(parameterName, parameterValue); }

//==============================================================================
void SOAPCommand::setParameter(const SOAPParameter parameter) { parameters_.addParameter(parameter.getName(), parameter.getValue()); }

//==============================================================================
bool SOAPCommand::hasParameters(void) const { return (parameters_.size() != 0); }

//==============================================================================
bool SOAPCommand::findParameter(std::string parameterName) const { return (parameters_.find(parameterName) != parameters_.end()); }

//==============================================================================
namespace ots
{
std::ostream& operator<<(std::ostream& os, const SOAPCommand& c)
{
	os << "Command: " << c.getCommand();
	unsigned int p = 0;
	for(SOAPParameters::const_iterator it = (c.getParameters()).begin(); it != (c.getParameters()).end(); it++)
		os << " Par " << p << " Name: " << it->first << " Value: " << it->second << ",";
	return os;
}
}  // namespace ots
