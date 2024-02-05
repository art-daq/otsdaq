#ifndef _ots_SlowControlsTableBase_h_
#define _ots_SlowControlsTableBase_h_

#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/TableCore/TableBase.h"

// helpers
#define OUT out << tabStr << commentStr
#define PUSHTAB tabStr += "\t"
#define POPTAB tabStr.resize(tabStr.size() - 1)
#define PUSHCOMMENT commentStr += "# "
#define POPCOMMENT commentStr.resize(commentStr.size() - 2)

namespace ots
{
// clang-format off
class SlowControlsTableBase : virtual public TableBase //virtual so future plugins can inherit from multiple table base classes
{
  public:
	SlowControlsTableBase(void);
	SlowControlsTableBase(std::string tableName, std::string* accumulatedExceptions = 0);

	virtual ~SlowControlsTableBase(void);

	// Getters
	virtual bool	slowControlsChannelListHasChanged 	(void) const;
	virtual void	getSlowControlsChannelList			(std::vector<std::pair<std::string /*channelName*/, std::vector<std::string>>>& channelList) const;

	//boardReader{
	//	build vector .. based on table 1,2,3,4,5.. 
	//	pass vector outputPV()
	//}
	//DTC {
	//	build vector from config Tree
	//	pass vector to outputPV()
	//}
	
	//use table name to have different file names! (instead of DEFINES like in DTC)
	
	//is channel binary or not?.. then can handle all the same
	virtual bool 			outputEpicsPVFile			(ConfigurationManager* configManager, std::vector<std::pair<std::string /*channelName*/, std::vector<std::string>>>* channelList = 0) const;

	virtual unsigned int	slowControlsHandlerConfig	(
															  std::stringstream& out
															, ConfigurationManager* configManager
															, std::vector<std::pair<std::string /*channelName*/, std::vector<std::string>>>* channelList /*= 0*/
														) const = 0;

	virtual unsigned int	slowControlsHandler			(
															  std::stringstream& out
															, std::string& tabStr
															, std::string& commentStr
															, std::string& subsystem
															, std::string& location
															, ConfigurationTree slowControlsLink
															, std::vector<std::pair<std::string /*channelName*/, std::vector<std::string>>>* channelList /*= 0*/
														) const;
	virtual std::string		setFilePath					()  const = 0;

	// Column names
	struct ColChannel
	{
		std::string const colMetricName_ 			= "MetricName";
		std::string const colStatus_ 				= "Status";
		std::string const colUnits_ 				= "Units";
		std::string const colChannelDataType_		= "ChannelDataType";
		std::string const colLowLowThreshold_		= "LowLowThreshold";
		std::string const colLowThreshold_ 			= "LowThreshold";
		std::string const colHighThreshold_			= "HighThreshold";
		std::string const colHighHighThreshold_ 	= "HighHighThreshold";
	} channelColNames_;

	bool					isFirstAppInContext_ 	= false; //for managing if PV list has changed
	bool					channelListHasChanged_ 	= false; //for managing if PV list has changed
	ConfigurationManager* 	lastConfigManager_		= nullptr;

private:

	#define EPICS_CONFIG_PATH (std::string(__ENV__("USER_DATA")) + "/" + "EPICSConfigurations/")
	#define EPICS_DIRTY_FILE_PATH \
		std::string( \
			getenv("OTSDAQ_EPICS_DATA")? \
				(std::string(getenv("OTSDAQ_EPICS_DATA")) + "/" + "dirtyFlag.txt"): \
				(EPICS_CONFIG_PATH + "/dirtyFlag.txt")  )
};
}  // namespace ots

#endif
