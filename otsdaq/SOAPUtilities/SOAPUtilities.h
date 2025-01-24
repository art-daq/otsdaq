#ifndef _ots_SOAPUtilities_h
#define _ots_SOAPUtilities_h

#include <xoap/MessageReference.h>

#include "otsdaq/SOAPUtilities/SOAPCommand.h"

#include <string>

namespace ots
{
class SOAPUtilities
{
  private:  // private constructor because all static members, should never instantiate
	        // this class
	SOAPUtilities(void);
	~SOAPUtilities(void);

  public:
	static xoap::MessageReference makeSOAPMessageReference(SOAPCommand soapCommand);
	static xoap::MessageReference makeSOAPMessageReference(std::string command);
	static xoap::MessageReference makeSOAPMessageReference(std::string    command,
	                                                       SOAPParameters parameters);
	static xoap::MessageReference makeSOAPMessageReference(std::string command,
	                                                       std::string fileName);

	static void addParameters(xoap::MessageReference& message, SOAPParameters parameters);

	static SOAPCommand translate(const xoap::MessageReference& message);

	static std::string receive(const xoap::MessageReference& message);
	static std::string receive(const xoap::MessageReference& message,
	                           SOAPCommand&                  soapCommand);
	static std::string receive(const xoap::MessageReference& message,
	                           SOAPParameters&               parameters);
};
}  // namespace ots
#endif
