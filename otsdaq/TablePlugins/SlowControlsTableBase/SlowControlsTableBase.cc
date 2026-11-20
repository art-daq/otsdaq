#include "otsdaq/TablePlugins/SlowControlsTableBase/SlowControlsTableBase.h"
#include "otsdaq/TablePlugins/XDAQContextTable/XDAQContextTable.h"

#include <fstream>  // std::fstream
#include <sstream>

using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "SlowControlsTableBase"

//==============================================================================
/// TableBase
///	If a valid string pointer is passed in accumulatedExceptions
///	then allowIllegalColumns is set for InfoReader
///	If accumulatedExceptions pointer = 0, then illegal columns throw std::runtime_error
/// exception
SlowControlsTableBase::SlowControlsTableBase(std::string  tableName,
                                             std::string* accumulatedExceptions /* =0 */)
    : TableBase(tableName, accumulatedExceptions)
{
	// December 2021 started seeing an issue where traceTID is found to be cleared to 0
	//	which crashes TRACE if __COUT__ is used in a Table plugin constructor
	//	This check and re-initialization seems to cover up the issue for now.
	//	Why it is cleared to 0 after the constructor sets it to -1 is still unknown.
	//		Note: it seems to only happen on the first alphabetially ARTDAQ Configure Table plugin.
#ifndef OTS_MAXLITE  // TRACE workaround; maxlite has no TRACE
	if(traceTID == 0)
	{
		std::cout << "SlowControlsTableBase Before traceTID=" << traceTID << __E__;
		char buf[40];
		traceInit(trace_name(TRACE_NAME, __TRACE_FILE__, buf, sizeof(buf)), 0);
		std::cout << "SlowControlsTableBase After traceTID=" << traceTID << __E__;
		__COUT__ << "SlowControlsTableBase TRACE reinit and Constructed." << __E__;
	}
#endif
}  // end constuctor()

//==============================================================================
/// SlowControlsTableBase
///	Default constructor should never be used because table type is lost
SlowControlsTableBase::SlowControlsTableBase(void) : TableBase("SlowControlsTableBase")
{
	__SS__ << "Should not call void constructor, table type is lost!" << __E__;
	__SS_THROW__;
}  // end illegal default constructor()

//==============================================================================
SlowControlsTableBase::~SlowControlsTableBase(void) {}  // end destructor()

//==============================================================================
void SlowControlsTableBase::getSlowControlsChannelList(
    std::vector<std::pair<std::string /*channelName*/, std::vector<std::string>>>&
        channelList) const
{
	outputEpicsPVFile(lastConfigManager_, &channelList);
}  // end getSlowControlsChannelList()

//==============================================================================
bool SlowControlsTableBase::slowControlsChannelListHasChanged(void) const
{
	__COUT__ << "channelListHasChanged()" << __E__;

	if(lastConfigManager_ == nullptr)
	{
		__SS__ << "Illegal call to get status of channel list, no config manager has "
		          "been initialized!"
		       << __E__;
		__SS_THROW__;
	}

	// if here, lastConfigManager_ pointer is defined
	bool changed = outputEpicsPVFile(lastConfigManager_);
	__COUT__ << "slowControlsChannelListHasChanged(): return " << std::boolalpha
	         << std::to_string(changed) << __E__;
	return changed;
}  // end slowControlsChannelListHasChanged()

//==============================================================================
unsigned int SlowControlsTableBase::slowControlsHandler(
    std::stringstream& out,
    std::string&       tabStr,
    std::string&       commentStr,
    std::string&       subsystem,
    std::string&       location,
    ConfigurationTree  slowControlsLink,
    std::vector<std::pair<std::string /*channelName*/, std::vector<std::string>>>*
        channelList /*= 0*/
) const
try
{
	unsigned int numberOfChannels = 0;
	__COUTS__(2) << "slowControlsHandler \n" << StringMacros::stackTrace() << __E__;

	if(!slowControlsLink.isDisconnected())
	// if(1)
	{
		std::vector<std::pair<std::string, ConfigurationTree>> channelChildren =
		    slowControlsLink.getChildren();

		__COUTVS__(2, channelChildren.size());
		// first do single bit binary fields
		bool first = true;
		for(auto& channel : channelChildren)
		{
			if(channel.second.getNode(channelColNames_.colChannelDataType_)
			       .getValue<std::string>() != "1b")
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

			std::string pvName = channel.first;
			std::string comment =
			    channel.second.getNode(TableViewColumnInfo::COL_NAME_COMMENT)
			        .getValue<std::string>();

			// output channel
			OUT << "{ \"" << subsystem << "\", \"" << location << "\", \"" << pvName
			    << "\", \""
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
			__COUTVS__(2, channel.first);
			if(channel.second.getNode(channelColNames_.colChannelDataType_)
			       .getValue<std::string>() == "1b")
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

			std::string experimentName = "";
			try
			{
				experimentName = __ENV__("OTS_EPICS_OWNER");
			}
			catch(...)
			{
				experimentName = __ENV__("OTS_OWNER");
			}
			__COUT__ << "experimentName has metricParameterValue: " << experimentName
			         << __E__;

			std::string pvName = channel.first;
			std::string comment =
			    channel.second.getNode(TableViewColumnInfo::COL_NAME_COMMENT)
			        .getValue<std::string>();
			std::string precision = "0";
			std::string units     = channel.second.getNode(channelColNames_.colUnits_)
			                        .getValue<std::string>();
			// channel.second.getNode(channelColNames_.colChannelDataType_)
			//     .getValue<std::string>();
			std::string low_alarm_lmt =
			    channel.second.getNode(channelColNames_.colLowLowThreshold_)
			        .getValueWithDefault<std::string>("-1000");
			std::string low_warn_lmt =
			    channel.second.getNode(channelColNames_.colLowThreshold_)
			        .getValueWithDefault<std::string>("-100");
			std::string high_warn_lmt =
			    channel.second.getNode(channelColNames_.colHighThreshold_)
			        .getValueWithDefault<std::string>("100");
			std::string high_alarm_lmt =
			    channel.second.getNode(channelColNames_.colHighHighThreshold_)
			        .getValueWithDefault<std::string>("1000");
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
				channelList->push_back(std::make_pair(
				    experimentName + ":" + subsystem + ":" + location + ":" + pvName,
				    pvSettings));
			}

			// output channel
			OUT << "{ \"" << subsystem << "\", \"" << location << "\", \"" << pvName
			    << "\", \"" << precision  // PREC
			    << "\", \"" << units << "\", \"" << low_alarm_lmt << "\", \""
			    << low_warn_lmt << "\", \"" << high_warn_lmt << "\", \"" << high_alarm_lmt
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
		__COUT__ << "Disconnected EventBuilder Slow Controls metric channels link, so "
		            "assuming "
		            "no slow controls channels."
		         << __E__;

	return numberOfChannels;
}  // end localSlowControlsHandler
catch(const std::runtime_error& e)
{
	__COUTV__(e.what());
	throw;
}

//==============================================================================
/// return channel list if pointer passed
bool SlowControlsTableBase::outputEpicsPVFile(
    ConfigurationManager* configManager,
    std::vector<std::pair<std::string /*channelName*/, std::vector<std::string>>>*
        channelList /*= 0*/) const
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
			std::fread(
			    &previousConfigFileContents[0], 1, previousConfigFileContents.size(), fp);
			std::fclose(fp);
		}
		else
			__COUT_WARN__ << "Could not open EPICS IOC config file at " << filename
			              << __E__;

	}  // done reading

	/////////////////////////
	// generate xdaq run parameter file

	std::stringstream out;
	unsigned int      numberOfParameters =
	    slowControlsHandlerConfig(out, configManager, channelList);
	__COUTVS__(2, numberOfParameters);

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
				__COUT_WARN__ << "Could not open dirty file: " << EPICS_DIRTY_FILE_PATH
				              << __E__;
		}

		// Indicate that PV list has changed
		//	if otsdaq_epics plugin is listening, then write PV data to archive db:	SQL insert or modify of ROW for PV
		__COUT__ << "outputEpicsPVFile() return true" << __E__;
		return true;
	}  // end handling of previous contents
	__COUT__ << "outputEpicsPVFile() return false" << __E__;
	return false;
}  // end outputEpicsPVFile()

// clang-format off
#define				FCL_COMMENT_POSITION	65
#define				TABSZ					4

/// OUTCF:   (X)string + (C)comment, with tree-path + (F)field
#define				OUTCF(X,C,F)			{ std::stringstream outSs; outSs << X; addCommentWhitespace(outSs, tabStr.size()*TABSZ + commentStr.size() + outSs.str().size()); outSs << (C) << (std::string(C).size()?" - ":"") << "from config-tree: " << parentPath << (std::string(F).size()?(std::string("/") + std::string(F)):std::string("")) << "\n"; OUT << outSs.str();}
/// OUTC:    (X)string + (C)comment, with tree-path
#define				OUTC(X,C)				OUTCF(X,C,"")
/// OUTCLF:  (X)string + (C)comment, with local tree-path + (F)field
#define				OUTCLF(X,C,F)			{ std::stringstream outSs; outSs << X; addCommentWhitespace(outSs, tabStr.size()*TABSZ + commentStr.size() + outSs.str().size()); outSs << (C) << (std::string(C).size()?" - ":"") << "from config-tree: " << localParentPath << std::string(std::string(F).size()?("/" + std::string(F)):std::string("")) << "\n"; OUT << outSs.str();}
/// OUTCL:   (X)string + (C)comment, with local tree-path
#define				OUTCL(X,C)				OUTCLF(X,C,"")
/// OUTCL2F: (X)string + (C)comment, with local2 tree-path + (F)field
#define				OUTCL2F(X,C,F)			{ std::stringstream outSs; outSs << X; addCommentWhitespace(outSs, tabStr.size()*TABSZ + commentStr.size() + outSs.str().size()); outSs << (C) << (std::string(C).size()?" - ":"") << "from config-tree: " << localParentPath2 << (std::string(F).size()?(std::string("/") + std::string(F)):std::string("")) << "\n"; OUT << outSs.str();}
/// OUTCL2:  (X)string + (C)comment, with local2 tree-path
#define				OUTCL2(X,C)				OUTCL2F(X,C,"")
// clang-format on

//==============================================================================
/// add whitespace so comments lineup in fcl for nicer presentation
void SlowControlsTableBase::addCommentWhitespace(std::ostream& os, size_t lineLength)
{
	for(size_t i = 0; true; i += 20)
	{
		if(lineLength < FCL_COMMENT_POSITION + i)  // pad to FCL_COMMENT_POSITION + i
		{
			os << std::string(FCL_COMMENT_POSITION + i - lineLength, ' ');
			break;
		}
	}
	os << " // ";
}  //end addCommentWhitespace()

//==============================================================================
/// insertMetricsBlock
///	Writes the fcl metrics block of the node's daqMetricsLink (used by the FE Supervisor
///	metric manager and by the artdaq table plugins).
void SlowControlsTableBase::insertMetricsBlock(std::ostream&      out,
                                               std::string&       tabStr,
                                               std::string&       commentStr,
                                               const std::string& parentPath,
                                               ConfigurationTree  daqNode)
{
	auto metricsGroup = daqNode.getNode("daqMetricsLink");

	out << "\n";
	OUTCF("metrics: {", "", metricsGroup.getParentLinkColumnName());
	PUSHTAB;
	if(!metricsGroup.isDisconnected())
	{
		auto metrics = metricsGroup.getChildren();
		bool sendSystemMetrics(false), sendProcessMetrics(false);
		for(auto& metric : metrics)
		{
			if(!metric.second.status())
				PUSHCOMMENT;

			__COUTT__ << "Inserting metric... " << parentPath << __E__;
			std::string localParentPath =
			    parentPath + "/" + metricsGroup.getParentLinkColumnName() + ":" +
			    metric.second.getTableName() + ":" + metricsGroup.getParentLinkIndex() +
			    ":" + metricsGroup.getParentLinkID() + "/" + metric.second.getValue();
			__COUTT__ << "Inserting metric... " << localParentPath << __E__;

			OUTCL(metric.second.getNode("metricKey").getValue() << ": {",
			      metric.second.hasComment() ? metric.second.getComment() : "");
			PUSHTAB;

			if(metric.second.getNode("sendSystemMetrics").getValue<bool>())
			{
				sendSystemMetrics = true;
			}
			if(metric.second.getNode("sendProcessMetrics").getValue<bool>())
			{
				sendProcessMetrics = true;
			}

			OUTCLF("metricPluginType: "
			           << metric.second.getNode("metricPluginType").getValue(),
			       "" /* comment */,
			       "metricPluginType");
			OUTCLF(
			    "level_string: " << metric.second.getNode("metricLevelString").getValue(),
			    "" /* comment */,
			    "metricLevelString");

			auto metricParametersGroup = metric.second.getNode("metricParametersLink");
			if(!metricParametersGroup.isDisconnected())
			{
				auto metricParameters = metricParametersGroup.getChildren();
				for(auto& metricParameter : metricParameters)
				{
					if(!metricParameter.second.status())
						PUSHCOMMENT;

					__COUTT__ << "Inserting metric... " << localParentPath << __E__;
					std::string localParentPath2 =
					    localParentPath + "/" +
					    metricParametersGroup.getParentLinkColumnName() + ":" +
					    metricParameter.second.getTableName() + ":" +
					    metricParametersGroup.getParentLinkIndex() + ":" +
					    metricParametersGroup.getParentLinkID() + "/" +
					    metricParameter.second.getValue();
					__COUTT__ << "Inserting metric... " << localParentPath2 << __E__;
					OUTCL2(metricParameter.second.getNode("metricParameterKey").getValue()
					           << ": "
					           << metricParameter.second.getNode("metricParameterValue")
					                  .getValue(),
					       metricParameter.second.hasComment()
					           ? metricParameter.second.getComment()
					           : "");

					if(!metricParameter.second.status())
						POPCOMMENT;
				}
			}
			POPTAB;
			OUT << "} # end " << metric.second.getNode("metricKey").getValue()
			    << "\n\n";  // end metric

			if(!metric.second.status())
				POPCOMMENT;
		}  //end metricsGroup children loop

		__COUTT__ << "Inserting metric send... " << parentPath << __E__;
		std::string localParentPath =
		    parentPath + "/" + metricsGroup.getParentLinkColumnName() + ":" +
		    metricsGroup.getTableName() + ":" + metricsGroup.getParentLinkIndex() + ":" +
		    metricsGroup.getParentLinkID();
		if(sendSystemMetrics)
		{
			__COUTT__ << "Inserting send_system_metrics... " << localParentPath << __E__;
			OUTCLF("send_system_metrics: true ",
			       "true, if any children are true",
			       "*/sendSystemMetrics");
		}
		else
			OUTCLF("# send_system_metrics: false ",
			       "true, if any children are true",
			       "*/sendSystemMetrics");

		if(sendProcessMetrics)
		{
			__COUTT__ << "Inserting send_process_metrics... " << localParentPath << __E__;
			OUTCLF("send_process_metrics: true ",
			       "true, if any children are true",
			       "*/sendProcessMetrics");
		}
		else
			OUTCLF("# send_process_metrics: false ",
			       "true, if any children are true",
			       "*/sendProcessMetrics");
	}  //end connected daq metrics link handling
	else
	{
		__COUTS__(3) << "No metrics found" << __E__;
		std::string localParentPath =
		    parentPath + "/" + metricsGroup.getParentLinkColumnName();
		OUTCL("# no metrics found", "" /* comment*/);
	}

	POPTAB;
	OUT << "} # end metrics\n\n";  // end metrics
}  // end insertMetricsBlock()
