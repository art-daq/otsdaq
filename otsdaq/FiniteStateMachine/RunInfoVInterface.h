#ifndef _ots_RunInfoVInterface_h_
#define _ots_RunInfoVInterface_h_

#include <string>
#include "otsdaq/Configurable/Configurable.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/StringMacros.h"

namespace ots
{

// clang-format off
class RunInfoVInterface  ///< : public Configurable
{
  public:
	enum class RunStopType
	{
		HALT,
		STOP,
		ERROR,
		PAUSE,
		RESUME,
		START
	};

	/// NOTE: Memory access violations were happening when we tried to pass  const ConfigurationTree& theXDAQContextConfigTree
	///	If needed in future, possibly passing a copy of ConfigureTree would make everything happy.. but for now, it is not needed.
	RunInfoVInterface							(const std::string& interfaceUID)
	    :
	    mfSubject_(interfaceUID)
	{;}
	virtual ~RunInfoVInterface						(void) { ; }

	virtual unsigned int insertLocalConfigureBlob	(const std::string& /*blob*/) { __COUT__ << "Not implemented!!"; return -1;};

	virtual unsigned int insertRunCondition(const std::string& runInfoConditions = "",
	                                        const std::string& configTypeName = "") = 0;

	virtual unsigned int claimNextRunNumber(unsigned int       conditionID,
											const std::string& runInfoConditions = "",
											const std::string& comment = "") = 0;
	virtual void updateRunInfo(unsigned int                   runNumber,
	                           RunInfoVInterface::RunStopType runStopType)   = 0;

	//start queryFilter with 'AND' to fiter more the selection
	virtual std::vector<std::vector<std::string>> getRunRecords(
	    unsigned int       startTime,
	    unsigned int       endTime,
	    const std::string& queryFilter = "") = 0;

	virtual std::vector<std::vector<std::string>> getRunConditionByID(
	    uint64_t conditionID) = 0;

	virtual std::vector<std::vector<std::string>> getRunConfigSubsystemInfo(
		uint64_t configID) = 0;

  private:
	const std::string mfSubject_;
	// ConfigurationTree 	theXDAQContextConfigTree_;
	// std::string 			configurationPath_;
};
// clang-format on

}  // namespace ots

#endif
