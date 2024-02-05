#include "otsdaq/XmlUtilities/ConvertFromXML.h"

#include <xercesc/util/XMLString.hpp>

#include <cstdio>

using namespace ots;

//==============================================================================
ConvertFromXML::ConvertFromXML(const XMLCh* const toTranscode) { xmlTranscoded_ = xercesc::XMLString::transcode(toTranscode); }

//==============================================================================
ConvertFromXML::~ConvertFromXML() { xercesc::XMLString::release(&xmlTranscoded_); }

//==============================================================================
const char* ConvertFromXML::toConstChar() const { return xmlTranscoded_; }

//==============================================================================
char* ConvertFromXML::toChar() const { return xmlTranscoded_; }
//==============================================================================
std::string ConvertFromXML::toString() const { return xmlTranscoded_; }
