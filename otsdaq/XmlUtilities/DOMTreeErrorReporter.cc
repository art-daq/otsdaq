#include "otsdaq/XmlUtilities/DOMTreeErrorReporter.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <iostream>
#include <sstream>
#include <xercesc/util/XMLString.hpp>

using namespace ots;

#undef __COUT_HDR__
#define __COUT_HDR__ "DOMTreeErrorReporter"

//==============================================================================
DOMTreeErrorReporter::DOMTreeErrorReporter() {}

//==============================================================================
DOMTreeErrorReporter::~DOMTreeErrorReporter() {}

//==============================================================================
void DOMTreeErrorReporter::warning(const xercesc::SAXParseException& ex)
{
	__COUT__ << "Warning!" << std::endl;
	__THROW__(reportParseException(ex));
}

//==============================================================================
void DOMTreeErrorReporter::error(const xercesc::SAXParseException& ex)
{
	__COUT__ << "Error!" << std::endl;
	__THROW__(reportParseException(ex));
}

//==============================================================================
void DOMTreeErrorReporter::fatalError(const xercesc::SAXParseException& ex)
{
	__COUT__ << "Fatal Error!" << std::endl;
	__THROW__(reportParseException(ex));
}

//==============================================================================
void DOMTreeErrorReporter::resetErrors() {}

//==============================================================================
std::string DOMTreeErrorReporter::reportParseException(const xercesc::SAXParseException& exception)
{
	__SS__ << "\n"
	       << "\tIn file \"" << xercesc::XMLString::transcode(exception.getSystemId()) << "\", line " << exception.getLineNumber() << ", column "
	       << exception.getColumnNumber() << std::endl
	       << "\tMessage: "
	       << xercesc::XMLString::transcode(exception.getMessage())
	       //<< " (check against xsd file)" //RAR commented, has no meaning to me or users..
	       << "\n\n";
	__COUT__ << "\n" << ss.str() << std::endl;
	return ss.str();
} //end reportParseException()
