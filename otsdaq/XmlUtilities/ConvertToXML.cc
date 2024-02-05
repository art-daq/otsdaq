#include "otsdaq/XmlUtilities/ConvertToXML.h"

#include <cstdio>
#include <xercesc/util/XMLString.hpp>

using namespace ots;

//==============================================================================
ConvertToXML::ConvertToXML(const char* const toTranscode) : fUnicodeForm_(xercesc::XMLString::transcode(toTranscode)) {}

//==============================================================================
ConvertToXML::ConvertToXML(const std::string& toTranscode) : fUnicodeForm_(xercesc::XMLString::transcode(toTranscode.c_str())) {}

//==============================================================================
ConvertToXML::ConvertToXML(const int toTranscode)
{
	char str[12];
	sprintf(str, "%d", toTranscode);
	fUnicodeForm_ = xercesc::XMLString::transcode(str);
}

//==============================================================================
ConvertToXML::~ConvertToXML() { xercesc::XMLString::release(&fUnicodeForm_); }

//==============================================================================
const XMLCh* ConvertToXML::unicodeForm() const { return fUnicodeForm_; }
