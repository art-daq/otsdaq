#include "otsdaq/TablePlugins/SlowControlsTableBase/SlowControlsTableBase.h"
#include "otsdaq/TablePlugins/XDAQContextTable/XDAQContextTable.h"

#include <fstream>  // std::fstream

using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "SlowControlsTableBase"

//==============================================================================
// TableBase
//	If a valid string pointer is passed in accumulatedExceptions
//	then allowIllegalColumns is set for InfoReader
//	If accumulatedExceptions pointer = 0, then illegal columns throw std::runtime_error
// exception
SlowControlsTableBase::SlowControlsTableBase(std::string tableName, std::string* accumulatedExceptions /* =0 */) : TableBase(tableName, accumulatedExceptions)
{
	// December 2021 started seeing an issue where traceTID is found to be cleared to 0
	//	which crashes TRACE if __COUT__ is used in a Table plugin constructor
	//	This check and re-initialization seems to cover up the issue for now.
	//	Why it is cleared to 0 after the constructor sets it to -1 is still unknown.
	//		Note: it seems to only happen on the first alphabetially ARTDAQ Configure Table plugin.
	if(traceTID == 0)
	{
		std::cout << "SlowControlsTableBase Before traceTID=" << traceTID << __E__;
		char buf[40];
		traceInit(trace_name(TRACE_NAME, __TRACE_FILE__, buf, sizeof(buf)), 0);
		std::cout << "SlowControlsTableBase After traceTID=" << traceTID << __E__;
		__COUT__ << "SlowControlsTableBase TRACE reinit and Constructed." << __E__;
	}
}  // end constuctor()

//==============================================================================
// SlowControlsTableBase
//	Default constructor should never be used because table type is lost
SlowControlsTableBase::SlowControlsTableBase(void) : TableBase("SlowControlsTableBase")
{
	__SS__ << "Should not call void constructor, table type is lost!" << __E__;
	__SS_THROW__;
}  // end illegal default constructor()

//==============================================================================
SlowControlsTableBase::~SlowControlsTableBase(void) {}  // end destructor()

//==============================================================================
void SlowControlsTableBase::getSlowControlsChannelList(std::vector<std::pair<std::string /*channelName*/, std::vector<std::string>>>& channelList) const
{
	outputEpicsPVFile(lastConfigManager_, &channelList);
}  // end getSlowControlsChannelList()

//==============================================================================
bool SlowControlsTableBase::slowControlsChannelListHasChanged(void) const
{
	__COUT__ << "channelListHasChanged()" << __E__;
	if(isFirstAppInContext_)
		return channelListHasChanged_;

	if(lastConfigManager_ == nullptr)
	{
		__SS__ << "Illegal call to get status of channel list, no config manager has been initialized!" << __E__;
		__SS_THROW__;
	}

	// if here, lastConfigManager_ pointer is defined
	bool changed = outputEpicsPVFile(lastConfigManager_);
	__COUT__ << "slowControlsChannelListHasChanged(): return " << std::boolalpha << std::to_string(changed) << __E__;
	return changed;
}  // end slowControlsChannelListHasChanged()

//==============================================================================
unsigned int SlowControlsTableBase::slowControlsHandler(std::stringstream&                                                             out,
                                                        std::string&                                                                   tabStr,
                                                        std::string&                                                                   commentStr,
                                                        std::string&                                                                   subsystem,
                                                        std::string&                                                                   location,
                                                        ConfigurationTree                                                              slowControlsLink,
                                                        std::vector<std::pair<std::string /*channelName*/, std::vector<std::string>>>* channelList /*= 0*/
) const
{
	unsigned int numberOfChannels = 0;
	__COUT__ << "slowControlsHandler" << __E__;

	if(!slowControlsLink.isDisconnected())
	// if(1)
	{
		std::vector<std::pair<std::string, ConfigurationTree>> channelChildren = slowControlsLink.getChildren();

		// first do single bit binary fields
		bool first = true;
		for(auto& channel : channelChildren)
		{
			if(channel.second.getNode(channelColNames_.colChannelDataType_).getValue<std::string>() != "1b")
				continue;  // skip non-binary fields

			if(first)  // if first, output header
			{
				first = false;
				OUT << "file \"dbt/soft_bi.dbt\" {" << __E__;
				PUSHTAB;
				OUT << "pattern  { Subsystem, loc, pvar, ZNAM, ONAM, ZSV, OSV, "
				       "COSV, DESC  }"
				    << __E__;
				PUSHTAB;
			}

			++numberOfChannels;

			std::string pvName  = channel.first;
			std::string comment = channel.second.getNode(TableViewColumnInfo::COL_NAME_COMMENT).getValue<std::string>();

			// output channel
			OUT << "{ \"" << subsystem << "\", \"" << location << "\", \"" << pvName << "\", \""
			    << "NOT_OK"
			    << "\", \""
			    << "OK"
			    << "\", \""
			    << "MAJOR"
			    << "\", \""
			    << "NO_ALARM"
			    << "\", \""
			    << ""
			    << "\", \"" << comment << "\"  }" << __E__;

		}           // end binary channel loop
		if(!first)  // if there was data, then pop tabs
		{
			POPTAB;
			POPTAB;
			out << "}" << __E__;
		}

		// then do 'analog' fields
		first = true;
		for(auto& channel : channelChildren)
		{
			if(channel.second.getNode(channelColNames_.colChannelDataType_).getValue<std::string>() == "1b")
				continue;  // skip non-binary fields

			if(first)  // if first, output header
			{
				first = false;
				OUT << "file \"dbt/subst_ai.dbt\" {" << __E__;
				PUSHTAB;
				OUT << "pattern  { Subsystem, loc, pvar, PREC, EGU, LOLO, LOW, "
				       "HIGH, HIHI, MDEL, ADEL, INP, SCAN, DTYP, DESC }"
				    << __E__;
				PUSHTAB;
			}

			++numberOfChannels;

			std::string pvName    = channel.first;
			std::string comment   = channel.second.getNode(TableViewColumnInfo::COL_NAME_COMMENT).getValue<std::string>();
			std::string precision = "0";
			std::string units     = channel.second.getNode(channelColNames_.colUnits_).getValue<std::string>();
			// channel.second.getNode(channelColNames_.colChannelDataType_)
			//     .getValue<std::string>();
			std::string low_alarm_lmt  = channel.second.getNode(channelColNames_.colLowLowThreshold_).getValueWithDefault<std::string>("-1000");
			std::string low_warn_lmt   = channel.second.getNode(channelColNames_.colLowThreshold_).getValueWithDefault<std::string>("-100");
			std::string high_warn_lmt  = channel.second.getNode(channelColNames_.colHighThreshold_).getValueWithDefault<std::string>("100");
			std::string high_alarm_lmt = channel.second.getNode(channelColNames_.colHighHighThreshold_).getValueWithDefault<std::string>("1000");
			if(channelList != nullptr)
			{
				std::vector<std::string> pvSettings;
				pvSettings.push_back(comment);
				pvSettings.push_back(low_warn_lmt);
				pvSettings.push_back(high_warn_lmt);
				pvSettings.push_back(low_alarm_lmt);
				pvSettings.push_back(high_alarm_lmt);
				pvSettings.push_back(precision);
				pvSettings.push_back(units);
				channelList->push_back(std::make_pair("Mu2e:" + subsystem + ":" + location + ":" + pvName, pvSettings));
			}

			// output channel
			OUT << "{ \"" << subsystem << "\", \"" << location << "\", \"" << pvName << "\", \"" << precision  // PREC
			    << "\", \"" << units << "\", \"" << low_alarm_lmt << "\", \"" << low_warn_lmt << "\", \"" << high_warn_lmt << "\", \"" << high_alarm_lmt
			    << "\", \""
			    << ""
			    << "\", \"" <<  // MDEL
			    ""
			    << "\", \"" <<  // ADEL
			    ""
			    << "\", \"" <<  // INP
			    ""
			    << "\", \"" <<  // SCAN
			    ""
			    << "\", \"" <<  // DTYP
			    comment << "\"  }" << __E__;

		}           // end binary channel loop
		if(!first)  // if there was data, then pop tabs
		{
			POPTAB;
			POPTAB;
			out << "}" << __E__;
		}
	}
	else
		__COUT__ << "Disconnected EventBuilder Slow Controls metric channels link, so assuming "
		            "no slow controls channels."
		         << __E__;

	return numberOfChannels;
}  // end localSlowControlsHandler

//==============================================================================
// return channel list if pointer passed
bool SlowControlsTableBase::outputEpicsPVFile(ConfigurationManager*                                                          configManager,
                                              std::vector<std::pair<std::string /*channelName*/, std::vector<std::string>>>* channelList /*= 0*/) const
{
	/*
	    the file will look something like this:

	        file name.template {
	          pattern { var1, var2, var3, ... }
	              { sub1_for_set1, sub2_for_set1, sub3_for_set1, ... }
	              { sub1_for_set2, sub2_for_set2, sub3_for_set2, ... }
	              { sub1_for_set3, sub2_for_set3, sub3_for_set3, ... }

	              ...
	          }

	          # for comment lines

	    file "soft_ai.dbt"  -- for floating point ("analog") data

	    file "soft_bi.dbt"  -- for binary values (on/off, good/bad, etc)

	    file "soft_stringin.dbt" -- for string values (e.g. "states")

	    Subsystem names:
	   https://docs.google.com/spreadsheets/d/1SO8R3O5Xm37X0JdaBiVmbg9p9aXy1Gk13uqiWFCchBo/edit#gid=1775059019
	    DTC maps to: CRV, Tracker, EMCal, STM, TEM

	    Example lines:

	    file "soft_ai.dbt" {
	        pattern  { Subsystem, loc, var, PREC, EGU, LOLO, LOW, HIGH, HIHI, MDEL, ADEL,
	   DESC } { "TDAQ", "DataLogger", "RunNumber", "0", "", "-1e23", "-1e23", "1e23",
	   "1e23", "", "", "DataLogger run number" } { "TDAQ", "DataLogger", "AvgEvtSize",
	   "0", "MB/evt", "-1e23", "-1e23", "1e23", "1e23", "", "", "Datalogger avg event
	   size" }
	    }

	    file "soft_bi.dbt" {
	        pattern  { Subsystem, loc, pvar, ZNAM, ONAM, ZSV, OSV, COSV, DESC  }
	             { "Computer", "daq01", "voltages_ok", "NOT_OK", "OK", "MAJOR",
	   "NO_ALARM", "", "voltages_ok daq01"  } { "Computer", "daq02", "voltages_ok",
	   "NOT_OK", "OK", "MAJOR", "NO_ALARM", "", "voltages_ok daq02"  }
	    }
	*/

	std::string filename = setFilePath();

	__COUT__ << "EPICS PV file: " << filename << __E__;

	std::string previousConfigFileContents;
	{
		std::FILE* fp = std::fopen(filename.c_str(), "rb");
		if(fp)
		{
			std::fseek(fp, 0, SEEK_END);
			previousConfigFileContents.resize(std::ftell(fp));
			std::rewind(fp);
			std::fread(&previousConfigFileContents[0], 1, previousConfigFileContents.size(), fp);
			std::fclose(fp);
		}
		else
			__COUT_WARN__ << "Could not open EPICS IOC config file at " << filename << __E__;

	}  // done reading

	/////////////////////////
	// generate xdaq run parameter file

	std::stringstream out;
	unsigned int      numberOfParameters = slowControlsHandlerConfig(out, configManager, channelList);
	__COUTV__(numberOfParameters);

	// check if need to restart EPICS ioc
	//	if dbg string has changed, then mark ioc configuration dirty
	if(previousConfigFileContents != out.str())
	{
		__COUT__ << "Configuration has changed! Marking dirty flag..." << __E__;

		// only write files if first app in context AND channelList is not passed, i.e. init() is only time we write!
		// if(isFirstAppInContext_ && channelList == nullptr)
		if(channelList == nullptr)
		{
			std::fstream fout;
			fout.open(filename, std::fstream::out | std::fstream::trunc);
			if(fout.fail())
			{
				__SS__ << "Failed to open EPICS PV file: " << filename << __E__;
				__SS_THROW__;
			}

			fout << out.str();
			fout.close();

			std::string csvFilename = filename.substr(0, filename.length() - 3) + "csv";
			fout.open(csvFilename, std::fstream::out | std::fstream::trunc);
			if(fout.fail())
			{
				__SS__ << "Failed to open CSV EPICS PV file: " << filename << __E__;
				__SS_THROW__;
			}

			std::string csvOut = out.str();
			// if (csvOut.find("file \"dbt/subst_ai.dbt\" {\n") != std::string::npos) csvOut = csvOut.replace(csvOut.find("file \"dbt/subst_ai.dbt\" {\n"), 26,
			// "");
			csvOut.erase(0, csvOut.find("\n") + 1);
			if(csvOut.find("pattern  {") != std::string::npos)
				csvOut = csvOut.replace(csvOut.find("pattern  {"), 10, "");
			while(csvOut.find("{") != std::string::npos)
				csvOut = csvOut.replace(csvOut.find("{"), 1, "");
			while(csvOut.find("}") != std::string::npos)
				csvOut = csvOut.replace(csvOut.find("}"), 1, "");
			while(csvOut.find("\"") != std::string::npos)
				csvOut = csvOut.replace(csvOut.find("\""), 1, "");
			while(csvOut.find(" ") != std::string::npos)
				csvOut = csvOut.replace(csvOut.find(" "), 1, "");
			while(csvOut.find("\t") != std::string::npos)
				csvOut = csvOut.replace(csvOut.find("\t"), 1, "");

			fout << csvOut.substr(0, csvOut.length() - 1);
			fout.close();

			std::FILE* fp = fopen(EPICS_DIRTY_FILE_PATH.c_str(), "w");
			if(fp)
			{
				fprintf(fp, "1");  // set dirty flag
				fclose(fp);
			}
			else
				__COUT_WARN__ << "Could not open dirty file: " << EPICS_DIRTY_FILE_PATH << __E__;
		}

		// Indicate that PV list has changed
		//	if otsdaq_epics plugin is listening, then write PV data to archive db:	SQL insert or modify of ROW for PV
		__COUT__ << "outputEpicsPVFile() return true" << __E__;
		return true;
	}  // end handling of previous contents
	__COUT__ << "outputEpicsPVFile() return false" << __E__;
	return false;
}  // end outputEpicsPVFile()