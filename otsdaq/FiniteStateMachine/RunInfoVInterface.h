#ifndef _ots_RunInfoVInterface_h_
#define _ots_RunInfoVInterface_h_

#include <string>
#include "otsdaq/Configurable/Configurable.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/StringMacros.h"

namespace ots
{

class RunInfoVInterface  // : public Configurable
{
  public:
	enum class RunStopType
	{
		HALT,
		STOP,
		ERROR,
		PAUSE,
		RESUME
	};

	// NOTE: Memory access violations were happening when we tried to pass  const ConfigurationTree& theXDAQContextConfigTree
	//	If needed in future, possibly passing a copy of ConfigureTree would make everything happy.. but for now, it is not needed.
	RunInfoVInterface(const std::string& interfaceUID)
	    :  //, const ConfigurationTree& theXDAQContextConfigTree, const std::string& configurationPath) :
	       // Configurable(theXDAQContextConfigTree, configurationPath)
	       //,
	    mfSubject_(interfaceUID)
	// , theXDAQContextConfigTree_(theXDAQContextConfigTree)
	// , configurationPath_(configurationPath)
	{
		;
	}
	virtual ~RunInfoVInterface(void) { ; }

	virtual unsigned int claimNextRunNumber(const std::string& runInfoConditions = "")                     = 0;
	virtual void         updateRunInfo(unsigned int runNumber, RunInfoVInterface::RunStopType runStopType) = 0;

  private:
	const std::string mfSubject_;
	// ConfigurationTree 		theXDAQContextConfigTree_;
	// std::string 			configurationPath_;
};

}  // namespace ots

#endif
