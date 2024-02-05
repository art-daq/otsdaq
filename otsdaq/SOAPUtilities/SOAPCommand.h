#ifndef _ots_SOAPCommand_h
#define _ots_SOAPCommand_h

#include "otsdaq/SOAPUtilities/SOAPParameter.h"
#include "otsdaq/SOAPUtilities/SOAPParameters.h"

#include <xoap/MessageReference.h>

#include <ostream>
#include <string>

namespace ots
{
class SOAPCommand
{
  public:
	SOAPCommand(void);
	SOAPCommand(const xoap::MessageReference& message);
	SOAPCommand(std::string command);
	SOAPCommand(std::string command, SOAPParameters parameters);
	SOAPCommand(std::string command, SOAPParameter parameter);
	~SOAPCommand(void);

	// Getters
	// FIXMEconst xoap::MessageReference translate         (void) const;
	const std::string&    getCommand(void) const;
	const SOAPParameters& getParameters(void) const;
	SOAPParameters&       getParametersRef(void);
	std::string           getParameterValue(std::string parameterName) const;
	unsigned int          getParametersSize(void) const;

	// Setters
	void translate(const xoap::MessageReference& message);
	void setCommand(const std::string command);
	void setParameters(const SOAPParameters& parameters);
	void setParameter(const std::string parameterName, const std::string parameterValue);
	void setParameter(const SOAPParameter parameter);

	bool                 hasParameters(void) const;
	bool                 findParameter(std::string parameterName) const;
	friend std::ostream& operator<<(std::ostream& os, const SOAPCommand& command);

  private:
	std::string    command_;
	SOAPParameters parameters_;
};

}  // namespace ots
#endif
