#include "otsdaq/TablePlugins/ARTDAQTableBase/ARTDAQTableBase.h"

#include <dirent.h>  //DIR and dirent
#include <fstream>   // for std::ofstream
#include <iostream>  // std::cout
#include <typeinfo>

#include "otsdaq/Macros/CoutMacros.h"
#define TRACE_NAME "ARTDAQTableBase"

#include <fhiclcpp/ParameterSet.h>
#include <fhiclcpp/detail/print_mode.h>
#include <fhiclcpp/intermediate_table.h>
#include <fhiclcpp/parse.h>

#include "otsdaq/ProgressBar/ProgressBar.h"
#include "otsdaq/TablePlugins/XDAQContextTable/XDAQContextTable.h"

using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "ARTDAQTableBase"

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
/// Tree-path rule is, if the last link in the path is a group link with a specified group ID, then include in the last link


const std::string 	ARTDAQTableBase::ARTDAQ_FCL_PATH = std::string(__ENV__("USER_DATA")) + "/" + "ARTDAQConfigurations/";
const std::string 	ARTDAQTableBase::ARTDAQ_CONFIG_LAYOUTS_PATH =
    (((getenv("SERVICE_DATA_PATH") == NULL)
          ? (std::string(getenv("USER_DATA")) + "/ServiceData")
          : std::string(getenv("SERVICE_DATA_PATH")))) +
    "/ConfigurationGUI_artdaqLayouts/";
const bool			ARTDAQTableBase::ARTDAQ_DONOTWRITE_FCL = ((getenv("OTS_FCL_DONOTWRITE") == NULL) ? false : true);

const std::string 	ARTDAQTableBase::ARTDAQ_SUPERVISOR_CLASS = "ots::ARTDAQSupervisor";
const std::string 	ARTDAQTableBase::ARTDAQ_SUPERVISOR_TABLE = "ARTDAQSupervisorTable";

const std::string 	ARTDAQTableBase::ARTDAQ_READER_TABLE = "ARTDAQBoardReaderTable";
const std::string 	ARTDAQTableBase::ARTDAQ_BUILDER_TABLE = "ARTDAQEventBuilderTable";
const std::string 	ARTDAQTableBase::ARTDAQ_LOGGER_TABLE = "ARTDAQDataLoggerTable";
const std::string 	ARTDAQTableBase::ARTDAQ_DISPATCHER_TABLE = "ARTDAQDispatcherTable";
const std::string 	ARTDAQTableBase::ARTDAQ_MONITOR_TABLE = "ARTDAQMonitorTable";
const std::string 	ARTDAQTableBase::ARTDAQ_ROUTER_TABLE = "ARTDAQRoutingManagerTable";

const std::string 	ARTDAQTableBase::ARTDAQ_SUBSYSTEM_TABLE = "ARTDAQSubsystemTable";
const std::string 	ARTDAQTableBase::ARTDAQ_DAQ_TABLE = "ARTDAQDaqTable";
const std::string 	ARTDAQTableBase::ARTDAQ_DAQ_PARAMETER_TABLE = "ARTDAQDaqParameterTable";
const std::string 	ARTDAQTableBase::ARTDAQ_ART_TABLE = "ARTDAQArtTable";

const std::string 	ARTDAQTableBase::ARTDAQ_TYPE_TABLE_HOSTNAME = "ExecutionHostname";
const std::string   ARTDAQTableBase::ARTDAQ_TYPE_TABLE_ALLOWED_PROCESSORS = "AllowedProcessors";
const std::string 	ARTDAQTableBase::ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK = "SubsystemLink";
const std::string 	ARTDAQTableBase::ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK_UID = "SubsystemLinkUID";


const int 			ARTDAQTableBase::NULL_SUBSYSTEM_DESTINATION = 0;
const std::string 	ARTDAQTableBase::NULL_SUBSYSTEM_DESTINATION_LABEL = "nullDestinationSubsystem";

ARTDAQTableBase::ARTDAQInfo 			ARTDAQTableBase::info_;

ARTDAQTableBase::ColARTDAQSupervisor 	ARTDAQTableBase::colARTDAQSupervisor_;
ARTDAQTableBase::ColARTDAQSubsystem 	ARTDAQTableBase::colARTDAQSubsystem_;
ARTDAQTableBase::ColARTDAQReader 		ARTDAQTableBase::colARTDAQReader_;
ARTDAQTableBase::ColARTDAQNotReader 	ARTDAQTableBase::colARTDAQNotReader_;
ARTDAQTableBase::ColARTDAQDaq 			ARTDAQTableBase::colARTDAQDaq_;
ARTDAQTableBase::ColARTDAQDaqParameter 	ARTDAQTableBase::colARTDAQDaqParameter_;
ARTDAQTableBase::ColARTDAQArt			ARTDAQTableBase::colARTDAQArt_;

///Note!!!! processTypes_ must be instantiate after the static artdaq table names (to construct map in constructor in .h)
ARTDAQTableBase::ProcessTypes 			ARTDAQTableBase::processTypes_;

// clang-format on

//==============================================================================
/// TableBase
///	If a valid string pointer is passed in accumulatedExceptions
///	then allowIllegalColumns is set for InfoReader
///	If accumulatedExceptions pointer = 0, then illegal columns throw std::runtime_error
/// exception
ARTDAQTableBase::ARTDAQTableBase(std::string  tableName,
                                 std::string* accumulatedExceptions /* =0 */)
    : TableBase(tableName, accumulatedExceptions)
{
	// make directory just in case
	mkdir((ARTDAQ_FCL_PATH).c_str(), 0755);

	// December 2021 started seeing an issue where traceTID is found to be cleared to 0
	//	which crashes TRACE if __COUT__ is used in a Table plugin constructor
	//	This check and re-initialization seems to cover up the issue for now.
	//	Why it is cleared to 0 after the constructor sets it to -1 is still unknown.
	//		Note: it seems to only happen on the first alphabetially ARTDAQ Configure Table plugin.
	if(traceTID == 0)
	{
		std::cout << "ARTDAQTableBase Before traceTID=" << traceTID << __E__;
		char buf[40];
		traceInit(trace_name(TRACE_NAME, __TRACE_FILE__, buf, sizeof(buf)), 0);
		std::cout << "ARTDAQTableBase After traceTID=" << traceTID << __E__;
		__COUT__ << "ARTDAQTableBase TRACE reinit and Constructed." << __E__;
	}

}  // end constuctor()

//==============================================================================
/// ARTDAQTableBase
///	Default constructor should never be used because table type is lost
ARTDAQTableBase::ARTDAQTableBase(void) : TableBase("ARTDAQTableBase")
{
	__SS__ << "Should not call void constructor, table type is lost!" << __E__;
	__SS_THROW__;
}  // end illegal default constructor()

//==============================================================================
ARTDAQTableBase::~ARTDAQTableBase(void) {}  // end destructor()

//==============================================================================
bool ARTDAQTableBase::doGenFiles(ConfigurationManager* configManager)
{
	// use isFirstAppInContext to only run once per context, for example to avoid
	//	generating files on local disk multiple times.
	isFirstAppInContext_ = configManager->isOwnerFirstAppInContext();

	__COUTVS__(4, isFirstAppInContext_);
	if(!isFirstAppInContext_)
		return false;

	//if artdaq supervisor is disabled, skip fcl handling
	if(!ARTDAQTableBase::isARTDAQEnabled(configManager))
	{
		__COUT_INFO__ << "ARTDAQ Supervisor is disabled, so skipping fcl handling."
		              << __E__;
		return false;
	}

	//allow any table with artdaq prerequisites to init!
	configManager->initPrereqsForARTDAQ();

	// make directory just in case
	mkdir((ARTDAQTableBase::ARTDAQ_FCL_PATH).c_str(), 0755);

	return true;
}  // end doGenFiles()

//==============================================================================
const std::string& ARTDAQTableBase::getTypeString(ARTDAQAppType type)
{
	switch(type)
	{
	case ARTDAQAppType::EventBuilder:
		return processTypes_.BUILDER;
	case ARTDAQAppType::DataLogger:
		return processTypes_.LOGGER;
	case ARTDAQAppType::Dispatcher:
		return processTypes_.DISPATCHER;
	case ARTDAQAppType::BoardReader:
		return processTypes_.READER;
	case ARTDAQAppType::Monitor:
		return processTypes_.MONITOR;
	case ARTDAQAppType::RoutingManager:
		return processTypes_.ROUTER;
	}
	// return "UNKNOWN";
	__SS__ << "Illegal translation attempt for type '" << (unsigned int)type << "'"
	       << __E__;
	__SS_THROW__;
}  // end getTypeString()

//==============================================================================
std::string ARTDAQTableBase::getFHICLFilename(ARTDAQAppType type, const std::string& name)
{
	//__COUT__ << "Type: " << getTypeString(type) << " Name: " << name
	//<< __E__;
	std::string filename = ARTDAQ_FCL_PATH + getTypeString(type) + "-";
	std::string uid      = name;
	for(unsigned int i = 0; i < uid.size(); ++i)
		if((uid[i] >= 'a' && uid[i] <= 'z') || (uid[i] >= 'A' && uid[i] <= 'Z') ||
		   (uid[i] >= '0' && uid[i] <= '9'))  // only allow alpha numeric in file name
			filename += uid[i];

	filename += ".fcl";

	//__COUT__ << "fcl: " << filename << __E__;

	return filename;
}  // end getFHICLFilename()

//==============================================================================
std::string ARTDAQTableBase::getFlatFHICLFilename(ARTDAQAppType      type,
                                                  const std::string& name)
{
	//__COUT__ << "Type: " << getTypeString(type) << " Name: " << name
	//         << __E__;
	std::string filename = ARTDAQ_FCL_PATH + getTypeString(type) + "-";
	std::string uid      = name;
	for(unsigned int i = 0; i < uid.size(); ++i)
		if((uid[i] >= 'a' && uid[i] <= 'z') || (uid[i] >= 'A' && uid[i] <= 'Z') ||
		   (uid[i] >= '0' && uid[i] <= '9'))  // only allow alpha numeric in file name
			filename += uid[i];

	filename += "_flattened.fcl";

	//__COUT__ << "fcl: " << filename << __E__;

	return filename;
}  // end getFlatFHICLFilename()

//==============================================================================
void ARTDAQTableBase::flattenFHICL(ARTDAQAppType      type,
                                   const std::string& name,
                                   std::string*       returnFcl /* = nullptr */)
{
	std::chrono::steady_clock::time_point startClock = std::chrono::steady_clock::now();
	__COUTS__(3) << "flattenFHICL()" << __ENV__("FHICL_FILE_PATH") << __E__;
	__COUTVS__(4, StringMacros::stackTrace());
	//return;

	std::string inFile  = getFHICLFilename(type, name);
	std::string outFile = getFlatFHICLFilename(type, name);

	__COUTVS__(3, inFile);
	__COUTVS__(3, outFile);

	cet::filepath_lookup_nonabsolute policy("FHICL_FILE_PATH");
	fhicl::ParameterSet              pset;

	try
	{
		__COUT_INFO__ << "parsing document: " << inFile;
		// tbl = fhicl::parse_document(inFile, policy);
		// pset = fhicl::ParameterSet::make(tbl);
		pset = fhicl::ParameterSet::make(inFile, policy);
		__COUTT__ << "document: " << inFile << " parsed";
		__COUTT__ << "got pset from table:";

		std::ofstream ofs{outFile};
		if(!ofs)
		{
			__SS__ << "Failed to open fhicl output file '" << outFile << "!'" << __E__;
			__SS_THROW__;
		}
		std::ostringstream out;
		out << pset.to_indented_string(
		    0);  // , fhicl::detail::print_mode::annotated); // Only really useful for debugging
		if(returnFcl)
		{
			*returnFcl = out.str();
			__COUTVS__(21, returnFcl);
		}
		ofs << out.str();
	}
	catch(cet::exception const& e)
	{
		__SS__ << "Failed to parse fhicl into output file '" << outFile
		       << "' - here is the error: " << e.what() << __E__;

		//add additional user helper information, based on error keywords
		if(std::string(e.what()).find("TriggerEpilogs") != std::string::npos)
			ss << "\n\n"
			   << "The Trigger Epilogs are located at "
			      "$USER_DATA/TriggerConfigurations/TriggerEpilogs. "
			   << "Please check that the Trigger Epilogs were properly generated, or "
			      "copy them from a previously working area."
			   << __E__;
		__SS_THROW__;
	}

	__COUTT__ << name
	          << " Flatten Clock time = " << artdaq::TimeUtils::GetElapsedTime(startClock)
	          << __E__;
}  // end flattenFHICL()

//==============================================================================
/// insertParameters
///	Inserts parameters in FHiCL outputs stream.
///
/// 	onlyInsertAtTableParameters allows @table:: parameters only,
///	so that calling code can do two passes (i.e. top of fcl block, @table:: only,
///	and bottom of fcl block, ignore/skip @table:: as default behavior).
void ARTDAQTableBase::insertParameters(std::ostream&      out,
                                       std::string&       tabStr,
                                       std::string&       commentStr,
                                       const std::string& parentPath,
                                       ConfigurationTree  parameterGroupLink,
                                       const std::string& parameterPreamble,
                                       bool onlyInsertAtTableParameters /*=false*/,
                                       bool includeAtTableParameters /*=false*/)
{
	// skip if link is disconnected
	if(!parameterGroupLink.isDisconnected())
	{
		///////////////////////
		auto otherParameters = parameterGroupLink.getChildren();

		std::string key;
		if(TTEST(3))
		{
			__COUTVS__(3, otherParameters.size());
			__COUTVS__(3, onlyInsertAtTableParameters);
			__COUTVS__(3, includeAtTableParameters);
		}
		size_t paramCount = 0;
		for(auto& parameter : otherParameters)
		{
			key = parameter.second.getNode(parameterPreamble + "Key").getValue();

			std::string localParentPath =
			    parentPath + "/" + parameterGroupLink.getParentLinkColumnName() + ":" +
			    parameter.second.getTableName() + ":" +
			    parameterGroupLink.getParentLinkIndex() + ":" +
			    parameterGroupLink.getParentLinkID() + "/" + parameter.second.getValue();

			// handle special keyword @table:: (which imports full tables, usually as
			// defaults)
			if(key.find("@table::") != std::string::npos)
			{
				// include @table::
				if(onlyInsertAtTableParameters || includeAtTableParameters)
				{
					++paramCount;
					if(!parameter.second.status())
						PUSHCOMMENT;

					__COUTT__ << "Inserting parameter... " << localParentPath << __E__;

					// skip connecting : if special keywords found
					OUTCL(key << parameter.second.getNode(parameterPreamble + "Value")
					                 .getValue(),
					      parameter.second.hasComment() ? parameter.second.getComment()
					                                    : "");

					if(!parameter.second.status())
						POPCOMMENT;
				}
				// else skip it

				continue;
			}
			// else NOT @table:: keyword parameter

			if(onlyInsertAtTableParameters)
				continue;  // skip all other types

			++paramCount;
			if(!parameter.second.status())
				PUSHCOMMENT;

			__COUTT__ << "Inserting parameter... " << localParentPath << __E__;

			// skip connecting : if special keywords found
			if(key.size() && key.find("#include") == std::string::npos)
			{
				//normal key / value pair ? or is it a value like @@<table> -> getFclValueForARTDAQ()

				std::string value =
				    parameter.second.getNode(parameterPreamble + "Value").getValue();
				StringMacros::trim(value);  //trim whitespace

				if(value.size() > 2 && value[0] == '@' && value[1] == '@')
				{
					__COUTT__
					    << "Checking for getFclValueForARTDAQ @@ indicator from value = "
					    << value << __E__;
					std::string potentialTable = value.substr(2);
					__COUTTV__(potentialTable);
					try
					{
						auto cfgMgr = parameterGroupLink.getConfigurationManager();
						value       = cfgMgr->getTableByName(potentialTable)
						            ->getFclValueForARTDAQ(cfgMgr, key);
					}
					catch(const std::runtime_error& e)
					{
						__SS__ << "getFclValueForARTDAQ @@ indicator from value = "
						       << value << " corresponds to table '" << potentialTable
						       << "'... however fcl value failed to load: " << e.what();
						__SS_THROW__;
					}

					__COUTT__ << "Value from getFclValueForARTDAQ: value = " << value
					          << __E__;

					std::string localParentPath2 = "/" + potentialTable;
					OUTCL2(key << ": " << value,
					       parameter.second.hasComment() ? parameter.second.getComment()
					                                     : "");
				}
				else  //normal key / value pair
				{
					OUTCL(key << ": "
					          << parameter.second.getNode(parameterPreamble + "Value")
					                 .getValue(),
					      parameter.second.hasComment() ? parameter.second.getComment()
					                                    : "");
				}
			}
			else if(key == "")
			{
				OUTCL(parameter.second.getNode(parameterPreamble + "Value").getValue(),
				      parameter.second.hasComment() ? parameter.second.getComment() : "");
			}
			else  //#include can not have a comment at end of line, so do before!
			{
				OUTCL("# comment for #include below:",
				      parameter.second.hasComment() ? parameter.second.getComment() : "");
				OUT << key
				    << parameter.second.getNode(parameterPreamble + "Value").getValue()
				    << "\n";
			}

			if(!parameter.second.status())
				POPCOMMENT;
		}

		if(!paramCount)
		{
			__COUTS__(3) << "Empty parameter set found onlyInsertAtTableParameters="
			             << onlyInsertAtTableParameters << __E__;
			std::string localParentPath =
			    parentPath + "/" + parameterGroupLink.getParentLinkColumnName();

			if(onlyInsertAtTableParameters)
			{
				OUTCL("# no @table parameters found", "" /* comment*/);
			}
			else
			{
				OUTCL("# empty parameter set found", "" /* comment*/);
			}
		}
	}
	else
	{
		__COUTS__(3) << "No parameters found" << __E__;
		std::string localParentPath =
		    parentPath + "/" + parameterGroupLink.getParentLinkColumnName();
		OUTCL("# no parameters inserted", "" /* comment*/);
	}

}  // end insertParameters()

//==============================================================================
/// insertModuleType
///	Inserts module type field, with consideration for @table::
std::string ARTDAQTableBase::insertModuleType(std::ostream&      out,
                                              std::string&       tabStr,
                                              std::string&       commentStr,
                                              const std::string& parentPath,
                                              ConfigurationTree  moduleTypeNode)
{
	std::string value = moduleTypeNode.getValue();
	__COUTTV__(parentPath);
	OUTCF((value.find("@table::") == std::string::npos ? "module_type: " : "") << value,
	      "" /* comment */,
	      moduleTypeNode.getFieldName());
	return value;
}  // end insertModuleType()

//==============================================================================
/// insertMetricsBlock
void ARTDAQTableBase::insertMetricsBlock(std::ostream&      out,
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

//==============================================================================
void ARTDAQTableBase::outputBoardReaderFHICL(
    const ConfigurationTree& boardReaderNode,
    size_t /*maxFragmentSizeBytes */ /* = DEFAULT_MAX_FRAGMENT_SIZE */,
    size_t routingTimeoutMs /* = DEFAULT_ROUTING_TIMEOUT_MS */,
    size_t routingRetryCount /* = DEFAULT_ROUTING_RETRY_COUNT */)
{
	if(ARTDAQ_DONOTWRITE_FCL)
	{
		__COUT__ << "Skipping fcl generation." << __E__;
		return;
	}

	/*
	    the file will look something like this:

	      daq: {
	          fragment_receiver: {
	            mpi_sync_interval: 50

	            # CommandableFragmentGenerator Table:
	        fragment_ids: []
	        fragment_id: -99 # Please define only one of these

	        sleep_on_stop_us: 0

	        requests_enabled: false # Whether to set up the socket for listening for
	   trigger messages request_mode: "Ignored" # Possible values are: Ignored, Single,
	   Buffer, Window

	        data_buffer_depth_fragments: 1000
	        data_buffer_depth_mb: 1000

	        request_port: 3001
	        request_address: "227.128.12.26" # Multicast request address

	        request_window_offset: 0 # Request message contains tzero. Window will be from
	   tzero - offset to tzero + width request_window_width: 0 stale_request_timeout:
	   "0xFFFFFFFF" # How long to wait before discarding request messages that are outside
	   the available data request_windows_are_unique: true # If request windows are
	   unique, avoids a copy operation, but the same data point cannot be used for two
	   requests. If this is not anticipated, leave set to "true"

	        separate_data_thread: false # MUST be true for triggers to be applied! If
	   triggering is not desired, but a separate readout thread is, set this to true,
	   triggers_enabled to false and trigger_mode to ignored. separate_monitoring_thread:
	   false # Whether a thread should be started which periodically calls checkHWStatus_,
	   a user-defined function which should be used to check hardware status registers and
	   report to MetricMan. poll_hardware_status: false # Whether checkHWStatus_ will be
	   called, either through the thread or at the start of getNext
	        hardware_poll_interval_us: 1000000 # If hardware monitoring thread is enabled,
	   how often should it call checkHWStatus_


	            # Generated Parameters:
	            generator: ToySimulator
	            fragment_type: TOY1
	            fragment_id: 0
	            board_id: 0
	            starting_fragment_id: 0
	            random_seed: 5780
	            sleep_on_stop_us: 500000

	            # Generator-Specific Table:

	        nADCcounts: 40

	        throttle_usecs: 100000

	        distribution_type: 1

	        timestamp_scale_factor: 1


	            destinations: {
	              d2: { transferPluginType: MPI
	                      destination_rank: 2
	                       max_fragment_size_bytes: 2097152
	                       host_map: [
	           {
	              host: "mu2edaq01.fnal.gov"
	              rank: 0
	           },
	           {
	              host: "mu2edaq01.fnal.gov"
	              rank: 1
	           }]
	                       }
	               d3: { transferPluginType: MPI
	                       destination_rank: 3
	                       max_fragment_size_bytes: 2097152
	                       host_map: [
	           {
	              host: "mu2edaq01.fnal.gov"
	              rank: 0
	           },
	           {
	              host: "mu2edaq01.fnal.gov"
	              rank: 1
	           }]
	           }

	            }
	          }

	          metrics: {
	            brFile: {
	              metricPluginType: "file"
	              level: 3
	              fileName: "/tmp/boardreader/br_%UID%_metrics.log"
	              uniquify: true
	            }
	            # ganglia: {
	            #   metricPluginType: "ganglia"
	            #   level: %{ganglia_level}
	            #   reporting_interval: 15.0
	            #
	            #   configFile: "/etc/ganglia/gmond.conf"
	            #   group: "ARTDAQ"
	            # }
	            # msgfac: {
	            #    level: %{mf_level}
	            #    metricPluginType: "msgFacility"
	            #    output_message_application_name: "ARTDAQ Metric"
	            #    output_message_severity: 0
	            # }
	            # graphite: {
	            #   level: %{graphite_level}
	            #   metricPluginType: "graphite"
	            #   host: "localhost"
	            #   port: 20030
	            #   namespace: "artdaq."
	            # }
	          }
	        }

	 */

	std::string filename =
	    getFHICLFilename(ARTDAQAppType::BoardReader, boardReaderNode.getValue());

	/////////////////////////
	// generate xdaq run parameter file
	std::fstream out;

	std::string tabStr     = "";
	std::string commentStr = "";

	__COUTV__(filename);
	out.open(filename, std::fstream::out | std::fstream::trunc);
	if(out.fail())
	{
		__SS__ << "Failed to open ARTDAQ BoardReader fcl file: " << filename << __E__;
		__SS_THROW__;
	}

	try  //catch and give error in fcl file if issue!
	{
		//--------------------------------------
		// header
		OUT << "###########################################################" << __E__;
		OUT << "#" << __E__;
		OUT << "# artdaq " << getTypeString(ARTDAQAppType::BoardReader)
		    << " fcl configuration file produced by otsdaq." << __E__;
		OUT << "# 	Creation time:           \t" << StringMacros::getTimestampString()
		    << __E__;
		OUT << "# 	Original filename:       \t" << filename << __E__;
		OUT << "#	otsdaq-ARTDAQ " << getTypeString(ARTDAQAppType::BoardReader)
		    << " UID:\t" << boardReaderNode.getValue() << __E__;
		OUT << "#" << __E__;
		OUT << "###########################################################" << __E__;
		OUT << "\n\n";

		// no primary link to table tree for reader node!
		try
		{
			if(boardReaderNode.isDisconnected())
			{
				// create empty fcl
				OUT << "{}\n\n";
				out.close();
				return;
			}
		}
		catch(const std::runtime_error&)
		{
			__COUTT__ << "Ignoring error, assume this a valid UID node." << __E__;
			// error is expected here for UIDs.. so just ignore
			// this check is valuable if source node is a unique-Link node, rather than UID
		}

		std::string parentPath =
		    boardReaderNode.getTableName() + "/" + boardReaderNode.getValue();

		OUTC("# start of " << getTypeString(ARTDAQAppType::BoardReader) << " '"
		                   << boardReaderNode.getValue() << "' fcl",
		     "" /* comment */);

		//--------------------------------------
		// handle preamble parameters
		__COUTT__ << "Inserting " << getTypeString(ARTDAQAppType::BoardReader)
		          << " preamble parameters... " << parentPath << __E__;
		out << "\n";
		insertParameters(out,
		                 tabStr,
		                 commentStr,
		                 parentPath,
		                 boardReaderNode.getNode("preambleParametersLink"),
		                 "daqParameter" /*parameterType*/,
		                 false /*onlyInsertAtTableParameters*/,
		                 true /*includeAtTableParameters*/);

		//--------------------------------------
		// handle daq
		__COUTT__ << "Generating daq block..." << __E__;
		out << "\n";
		OUTC("daq: {", "" /* comment */);
		PUSHTAB;

		// fragment_receiver
		out << "\n";
		OUT << "fragment_receiver: {\n";
		PUSHTAB;
		{
			// plugin type and fragment data-type
			OUTCF("generator"
			          << ": "
			          << boardReaderNode.getNode("daqGeneratorPluginType").getValue(),
			      "daq generator plug-in type" /* comment */,
			      "daqGeneratorPluginType" /* field*/);
			OUTCF("fragment_type"
			          << ": "
			          << boardReaderNode.getNode("daqGeneratorFragmentType").getValue(),
			      "generator data fragment type" /* comment */,
			      "daqGeneratorFragmentType" /* field*/);

			__COUTT__ << "Inserting " << getTypeString(ARTDAQAppType::BoardReader)
			          << " DAQ Parameters... " << parentPath << __E__;
			// shared and unique parameters
			insertParameters(out,
			                 tabStr,
			                 commentStr,
			                 parentPath,
			                 boardReaderNode.getNode("daqParametersLink"),
			                 "daqParameter");

			try  //try to get daqFragmentId
			{
				auto        fragmentId = boardReaderNode.getNode("daqFragmentIDs");
				std::string value      = fragmentId.getValue();
				if(value.size() < 2 || value[0] != '[' || value[value.size() - 1] != ']')
				{
					__SS__ << "Invalid 'daqFragmentIDs' - the value must be a valid fcl "
					          "array with starting and ending square brackets: [ ]"
					       << __E__;
					__SS_THROW__;
				}
				__COUTS__(20) << "fragment_ids: " << fragmentId.getValue() << __E__;
				OUTCF("fragment_ids: " << fragmentId.getValue(),
				      "" /* comment */,
				      "daqFragmentIDs");
			}
			catch(...)
			{
				__COUT__
				    << "Ignoring missing fragment_id column associated with Board Reader."
				    << __E__;

				OUTCF("# fragment_ids not specified, but could be", "", "daqFragmentIDs");
			}

			OUT << "\n";  // end daq board reader parameters
		}

		OUT << "destinations: { # empty placeholder, '"
		    << getTypeString(ARTDAQAppType::BoardReader)
		    << "' destinations handled by artdaq interface\n";
		OUT << "}\n\n";  // end destinations

		OUT << "routing_table_config: {\n";
		PUSHTAB;

		auto readerSubsystemID   = 1;
		auto readerSubsystemLink = boardReaderNode.getNode("SubsystemLink");
		if(!readerSubsystemLink.isDisconnected())
		{
			readerSubsystemID = getSubsytemId(readerSubsystemLink);
		}
		if(info_.subsystems[readerSubsystemID].hasRoutingManager)
		{
			std::string localParentPath =
			    parentPath + "/" + readerSubsystemLink.getParentLinkColumnName() + ":" +
			    readerSubsystemLink.getTableName() + "/" + readerSubsystemLink.getValue();
			__COUTT__ << "Inserting routing manager... " << localParentPath << __E__;
			OUTCL("use_routing_manager: true",
			      "auto-generated because subsystem '" +
			          std::to_string(readerSubsystemID) + "' has Routing Manager added");

			OUTCLF("routing_manager_hostname: \""
			           << info_.subsystems[readerSubsystemID].routingManagerHost << "\"",
			       "" /* comment */,
			       ARTDAQTableBase::ARTDAQ_TYPE_TABLE_HOSTNAME);
			OUT << "table_update_port: 0\n";
			OUT << "table_update_address: \"0.0.0.0\"\n";
			OUT << "table_update_multicast_interface: \"0.0.0.0\"\n";
			OUT << "table_acknowledge_port : 0\n";
			OUT << "routing_timeout_ms: " << routingTimeoutMs << "\n";
			OUT << "routing_retry_count: " << routingRetryCount << "\n";
		}
		else
		{
			OUTCF("use_routing_manager: false",
			      "auto-generated if subsystem '" + std::to_string(readerSubsystemID) +
			          "' has Routing Manager added",
			      readerSubsystemLink.getParentLinkColumnName());
		}

		POPTAB;
		OUT << "}\n";  // end routing_table_config

		POPTAB;
		OUT << "} # end fragment_receiver\n";  // end fragment_receiver

		insertMetricsBlock(OUT, tabStr, commentStr, parentPath, boardReaderNode);

		POPTAB;
		OUT << "} # end daq\n\n";  // end daq

		//--------------------------------------
		// handle ALL add-on parameters
		parentPath = boardReaderNode.getTableName() + "/" + boardReaderNode.getValue();
		__COUTT__ << "Inserting " << getTypeString(ARTDAQAppType::BoardReader)
		          << " add-on parameters... " << parentPath << __E__;
		insertParameters(out,
		                 tabStr,
		                 commentStr,
		                 parentPath,
		                 boardReaderNode.getNode("addOnParametersLink"),
		                 "daqParameter" /*parameterType*/,
		                 false /*onlyInsertAtTableParameters*/,
		                 true /*includeAtTableParameters*/);
		out << "\n";
		OUTC("# end of " << getTypeString(ARTDAQAppType::BoardReader) << " '"
		                 << boardReaderNode.getValue() << "' fcl",
		     "" /* comment */);
		__COUTT__ << "outputBoardReaderFHICL DONE" << __E__;
	}
	catch(...)
	{
		__SS__ << "\n\nError while generating FHiCL for "
		       << getTypeString(ARTDAQAppType::BoardReader) << " node at filename '"
		       << filename << "'" << __E__;
		try
		{
			throw;
		}
		catch(const std::runtime_error& e)
		{
			ss << " Here is the error: " << e.what() << __E__;
		}
		catch(const std::exception& e)
		{
			ss << " Here is the error: " << e.what() << __E__;
		}
		out << ss.str();
		out.close();
		__SS_THROW__;
	}

	out.close();
}  // end outputBoardReaderFHICL()

//==============================================================================
/// outputDataReceiverFHICL
///	 Note: currently selfRank and selfPort are unused by artdaq fcl.
///  Could use returnFcl to compare if two nodes have the same fcl.
void ARTDAQTableBase::outputDataReceiverFHICL(
    const ConfigurationTree& receiverNode,
    ARTDAQAppType            appType,
    size_t /*maxFragmentSizeBytes */ /* = DEFAULT_MAX_FRAGMENT_SIZE */,
    size_t       routingTimeoutMs /* = DEFAULT_ROUTING_TIMEOUT_MS */,
    size_t       routingRetryCount /* = DEFAULT_ROUTING_RETRY_COUNT */,
    std::string* returnFcl /* = nullptr */)
{
	if(ARTDAQ_DONOTWRITE_FCL)
	{
		__COUT__ << "Skipping fcl generation." << __E__;
		return;
	}

	std::string filename = getFHICLFilename(appType, receiverNode.getValue());

	/////////////////////////
	// generate xdaq run parameter file
	std::fstream       outf;
	std::ostringstream out;

	std::string tabStr     = "";
	std::string commentStr = "";

	__COUTV__(filename);
	outf.open(filename, std::fstream::out | std::fstream::trunc);
	if(outf.fail())
	{
		__SS__ << "Failed to open ARTDAQ fcl file: " << filename << __E__;
		__SS_THROW__;
	}

	try  //catch and give error in fcl file if issue!
	{
		//--------------------------------------
		// header
		OUT << "###########################################################" << __E__;
		OUT << "#" << __E__;
		OUT << "# artdaq " << getTypeString(appType)
		    << " fcl configuration file produced by otsdaq." << __E__;
		OUT << "# 	Creation time:                  \t"
		    << StringMacros::getTimestampString() << __E__;
		OUT << "# 	Original filename:              \t" << filename << __E__;
		OUT << "#	otsdaq-ARTDAQ " << getTypeString(appType) << " UID:\t"
		    << receiverNode.getValue() << __E__;
		OUT << "#" << __E__;
		OUT << "###########################################################" << __E__;
		OUT << "\n\n";

		// no primary link to table tree for data receiver node!
		try
		{
			if(receiverNode.isDisconnected())
			{
				// create empty fcl
				OUT << "{}\n\n";
				if(returnFcl)
				{
					*returnFcl = out.str();
					__COUTVS__(21, *returnFcl);
				}
				outf << out.str();
				outf.close();
				return;
			}
		}
		catch(const std::runtime_error&)
		{
			__COUTT__ << "Ignoring error, assume this a valid UID node." << __E__;
			// error is expected here for UIDs.. so just ignore
			// this check is valuable if source node is a unique-Link node, rather than UID
		}

		std::string parentPath =
		    receiverNode.getTableName() + "/" + receiverNode.getValue();

		OUTC("# start of " << getTypeString(appType) << " '" << receiverNode.getValue()
		                   << "' fcl",
		     "" /* comment */);

		//--------------------------------------
		// handle preamble parameters
		__COUTT__ << "Inserting " << getTypeString(appType) << " preamble parameters... "
		          << parentPath << __E__;
		out << "\n";
		insertParameters(out,
		                 tabStr,
		                 commentStr,
		                 parentPath,
		                 receiverNode.getNode("preambleParametersLink"),
		                 "daqParameter" /*parameterType*/,
		                 false /*onlyInsertAtTableParameters*/,
		                 true /*includeAtTableParameters*/);

		//--------------------------------------
		// handle daq
		__COUTT__ << "Generating daq block..." << __E__;
		out << "\n";
		auto daq = receiverNode.getNode("daqLink");
		if(!daq.isDisconnected())
		{
			///////////////////////
			OUTCF("daq: {", "" /* comment */, daq.getParentLinkColumnName());

			PUSHTAB;
			if(appType == ARTDAQAppType::EventBuilder)
				OUT << "event_builder: {\n";
			else  // both datalogger and dispatcher use aggregator for now
				OUT << "aggregator: {\n";

			PUSHTAB;

			{  //define datalogger vs dispatcher
				std::stringstream outSs;
				if(appType == ARTDAQAppType::DataLogger)
					outSs << "is_datalogger: true";
				else if(appType == ARTDAQAppType::Dispatcher)
					outSs << "is_dispatcher: true";
				if(outSs.str().size())
				{
					addCommentWhitespace(
					    outSs,
					    tabStr.size() * TABSZ + commentStr.size() + outSs.str().size());
					outSs << "auto-generated based on app type '"
					      << getTypeString(appType) << "'\n";
					OUT << outSs.str();
				}
			}

			//--------------------------------------
			// handle ALL daq parameters
			std::string parentPath = daq.getParentTableName() + "/" +
			                         daq.getParentRecordName() + "/" +
			                         daq.getParentLinkColumnName() + ":" +
			                         daq.getTableName() + "/" + daq.getValue();
			__COUTT__ << "Inserting " << getTypeString(appType) << " DAQ Parameters... "
			          << parentPath << __E__;
			insertParameters(out,
			                 tabStr,
			                 commentStr,
			                 parentPath,
			                 daq.getNode("daqParametersLink"),
			                 "daqParameter" /*parameterType*/,
			                 false /*onlyInsertAtTableParameters*/,
			                 true /*includeAtTableParameters*/);

			if(appType == ARTDAQAppType::EventBuilder)
			{
				out << "\n";
				OUT << "routing_token_config: {\n";
				PUSHTAB;

				auto builderSubsystemID   = 1;
				auto builderSubsystemLink = receiverNode.getNode("SubsystemLink");
				if(!builderSubsystemLink.isDisconnected())
				{
					builderSubsystemID = getSubsytemId(builderSubsystemLink);
				}
				if(info_.subsystems[builderSubsystemID].hasRoutingManager)
				{
					std::string localParentPath =
					    parentPath + "/" +
					    builderSubsystemLink.getParentLinkColumnName() + ":" +
					    builderSubsystemLink.getTableName() + "/" +
					    builderSubsystemLink.getValue();
					__COUTT__ << "Inserting routing manager... " << localParentPath
					          << __E__;
					OUTCL("use_routing_manager: true",
					      "auto-generated because subsystem '" +
					          std::to_string(builderSubsystemID) +
					          "' has Routing Manager added");

					OUTCLF("routing_manager_hostname: \""
					           << info_.subsystems[builderSubsystemID].routingManagerHost
					           << "\"",
					       "" /* comment */,
					       ARTDAQTableBase::ARTDAQ_TYPE_TABLE_HOSTNAME);
					OUT << "routing_token_port: 0\n";
				}
				else
				{
					OUTCF("use_routing_manager: false",
					      "auto-generated if subsystem '" +
					          std::to_string(builderSubsystemID) +
					          "' has Routing Manager added",
					      builderSubsystemLink.getParentLinkColumnName());
				}
				POPTAB;
				OUT << "}\n";  // end routing_token_config
			}

			__COUTT__ << "Adding sources placeholder" << __E__;
			out << "\n";
			OUT << "sources: { # empty placeholder, '" << getTypeString(appType)
			    << "' sources handled by artdaq interface\n";
			OUT << "}\n\n";  // end sources

			POPTAB;

			if(appType == ARTDAQAppType::EventBuilder)
				OUT << "} # end event_builder\n";  // end event builder
			else  // both datalogger and dispatcher use aggregator for now
				OUT << "} # end aggregator\n";  // end aggregator

			insertMetricsBlock(OUT, tabStr, commentStr, parentPath, daq);

			POPTAB;
			OUT << "} # end daq\n\n";  // end daq
		}
		else
		{
			__COUTS__(3) << "No daq found" << __E__;
			std::string localParentPath =
			    parentPath + "/" + daq.getParentLinkColumnName();
			OUTCL("# no daq found", "" /* comment*/);
		}

		//--------------------------------------
		// handle art
		__COUTT__ << "Filling art block..." << __E__;
		out << "\n";
		auto art =
		    receiverNode.getNode(ARTDAQTableBase::colARTDAQNotReader_.colLinkToArt_);
		if(!art.isDisconnected())
		{
			std::string localParentPath = parentPath + "/" +
			                              art.getParentLinkColumnName() + ":" +
			                              art.getTableName() + "/" + art.getValue();
			OUTCF("art: {", "" /* comment */, art.getParentLinkColumnName());

			PUSHTAB;

			insertArtProcessBlock(out,
			                      tabStr,
			                      commentStr,
			                      localParentPath,
			                      art,
			                      receiverNode.getNode("SubsystemLink"),
			                      routingTimeoutMs,
			                      routingRetryCount);

			POPTAB;
			OUT << "} # end art\n\n";  // end art
		}
		else
		{
			__COUTS__(3) << "No art found" << __E__;
			std::string localParentPath =
			    parentPath + "/" + art.getParentLinkColumnName();
			OUTCL("# no art found", "" /* comment*/);
		}

		//--------------------------------------
		// handle ALL add-on parameters
		__COUTT__ << "Inserting " << getTypeString(appType) << " add-on parameters... "
		          << parentPath << __E__;
		insertParameters(out,
		                 tabStr,
		                 commentStr,
		                 parentPath,
		                 receiverNode.getNode("addOnParametersLink"),
		                 "daqParameter" /*parameterType*/,
		                 false /*onlyInsertAtTableParameters*/,
		                 true /*includeAtTableParameters*/);

		out << "\n";
		OUTC("# end of " << getTypeString(appType) << " '" << receiverNode.getValue()
		                 << "' fcl",
		     "" /* comment */);
		__COUTT__ << "outputDataReceiverFHICL DONE" << __E__;
	}
	catch(...)
	{
		__SS__ << "\n\nError while generating FHiCL for " << getTypeString(appType)
		       << " node at filename '" << filename << "'" << __E__;
		try
		{
			throw;
		}
		catch(const std::runtime_error& e)
		{
			ss << " Here is the error: " << e.what() << __E__;
		}
		catch(const std::exception& e)
		{
			ss << " Here is the error: " << e.what() << __E__;
		}
		out << ss.str();
		if(returnFcl)
		{
			*returnFcl = out.str();
			__COUTVS__(21, *returnFcl);
		}
		outf << out.str();
		outf.close();
		__SS_THROW__;
	}

	if(returnFcl)
	{
		*returnFcl = out.str();
		__COUTVS__(21, *returnFcl);
	}
	outf << out.str();
	outf.close();
}  // end outputDataReceiverFHICL()

//==============================================================================
/// outputOnlineMonitorFHICL
///	Note: currently selfRank and selfPort are unused by artdaq fcl
void ARTDAQTableBase::outputOnlineMonitorFHICL(const ConfigurationTree& monitorNode)
{
	if(ARTDAQ_DONOTWRITE_FCL)
	{
		__COUT__ << "Skipping fcl generation." << __E__;
		return;
	}

	std::string filename =
	    getFHICLFilename(ARTDAQAppType::Monitor, monitorNode.getValue());

	/////////////////////////
	// generate xdaq run parameter file
	std::fstream out;

	std::string tabStr     = "";
	std::string commentStr = "";

	__COUTV__(filename);
	out.open(filename, std::fstream::out | std::fstream::trunc);
	if(out.fail())
	{
		__SS__ << "Failed to open ARTDAQ fcl file: " << filename << __E__;
		__SS_THROW__;
	}

	try  //catch and give error in fcl file if issue!
	{
		//--------------------------------------
		// header
		OUT << "###########################################################" << __E__;
		OUT << "#" << __E__;
		OUT << "# artdaq " << getTypeString(ARTDAQAppType::Monitor)
		    << " fcl configuration file produced by otsdaq." << __E__;
		OUT << "# 	Creation time:                  \t"
		    << StringMacros::getTimestampString() << __E__;
		OUT << "# 	Original filename:              \t" << filename << __E__;
		OUT << "#	otsdaq-ARTDAQ " << getTypeString(ARTDAQAppType::Monitor) << " UID:\t"
		    << monitorNode.getValue() << __E__;
		OUT << "#" << __E__;
		OUT << "###########################################################" << __E__;
		OUT << "\n\n";

		// no primary link to table tree for data receiver node!
		try
		{
			if(monitorNode.isDisconnected())
			{
				// create empty fcl
				OUT << "{}\n\n";
				out.close();
				return;
			}
		}
		catch(const std::runtime_error&)
		{
			__COUTT__ << "Ignoring error, assume this a valid UID node." << __E__;
			// error is expected here for UIDs.. so just ignore
			// this check is valuable if source node is a unique-Link node, rather than UID
		}

		//--------------------------------------
		// handle preamble parameters
		std::string parentPath =
		    monitorNode.getParentTableName() + "/" + monitorNode.getParentRecordName() +
		    "/" + monitorNode.getParentLinkColumnName() + ":" +
		    monitorNode.getTableName() + "/" + monitorNode.getValue();
		__COUTT__ << "Inserting " << getTypeString(ARTDAQAppType::Monitor)
		          << " preamble parameters... " << parentPath << __E__;
		insertParameters(out,
		                 tabStr,
		                 commentStr,
		                 parentPath,
		                 monitorNode.getNode("preambleParametersLink"),
		                 "daqParameter" /*parameterType*/,
		                 false /*onlyInsertAtTableParameters*/,
		                 true /*includeAtTableParameters*/);

		//--------------------------------------
		// handle art
		//__COUT__ << "Filling art block..." << __E__;
		auto art =
		    monitorNode.getNode(ARTDAQTableBase::colARTDAQNotReader_.colLinkToArt_);
		if(!art.isDisconnected())
		{
			insertArtProcessBlock(out, tabStr, commentStr, parentPath, art);
			OUT << "services.message: { "
			    << artdaq::generateMessageFacilityConfiguration(
			           mf::GetApplicationName().c_str(), true, false)
			    << "}\n";
			OUT << "services.message.destinations.file: {type: \"GenFile\" threshold: "
			       "\"INFO\" seperator: \"-\""
			    << " pattern: \"" << monitorNode.getValue() << "-%?H%t-%p.log"
			    << "\""
			    << " timestamp_pattern: \"%Y%m%d%H%M%S\""
			    << " directory: \"" << __ENV__("OTSDAQ_LOG_ROOT") << "/"
			    << monitorNode.getValue() << "\""
			    << " append : false }\n";
		}

		auto dispatcherLink = monitorNode.getNode("dispatcherLink");
		if(!dispatcherLink.isDisconnected())
		{
			std::string monitorHost =
			    monitorNode.getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_HOSTNAME)
			        .getValueWithDefault("localhost");
			std::string dispatcherHost =
			    dispatcherLink.getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_HOSTNAME)
			        .getValueWithDefault("localhost");
			OUT << "source.dispatcherHost: \"" << dispatcherHost << "\"\n";
			int dispatcherPort = dispatcherLink.getNode("DispatcherPort").getValue<int>();
			OUT << "source.dispatcherPort: " << dispatcherPort << "\n";
			OUT << "source.commanderPluginType: xmlrpc\n";

			int om_rank = monitorNode.getNode("MonitorID").getValue<int>();
			int om_tcp_listen_port =
			    monitorNode.getNode("MonitorTCPListenPort").getValue<int>();
			int disp_fake_rank =
			    dispatcherLink.getNode("DispatcherID").getValueWithDefault<int>(200);

			size_t max_fragment_size = monitorNode.getNode("max_fragment_size_words")
			                               .getValueWithDefault(0x100000);
			std::string transfer_plugin_type = monitorNode.getNode("transfer_plugin_type")
			                                       .getValueWithDefault("Autodetect");

			OUT << "TransferPluginConfig: {\n";
			PUSHTAB;
			OUT << "transferPluginType: " << transfer_plugin_type << "\n";
			OUT << "host_map: [{ rank: " << disp_fake_rank << " host: \""
			    << dispatcherHost << "\"}, { rank: " << om_rank << " host: \""
			    << monitorHost << "\"}]\n";
			OUT << "max_fragment_size_words: " << max_fragment_size << "\n";
			OUT << "source_rank: " << disp_fake_rank << "\n";
			OUT << "destination_rank: " << om_rank << "\n";
			OUT << "port: " << om_tcp_listen_port << "\n";
			OUT << "unique_label: " << monitorNode.getValue() << "_to_"
			    << dispatcherLink.getValue() << "\n";
			POPTAB;
			OUT << "}\n";
			OUT << "source.transfer_plugin: @local::TransferPluginConfig \n";
			auto dispatcherArt = monitorNode.getNode("dispatcherArtLink");
			if(!dispatcherArt.isDisconnected())
			{
				OUT << "source.dispatcher_config: {\n";

				PUSHTAB;

				OUT << "path: " << monitorNode.getNode("dispatcher_path").getValue()
				    << "\n";
				OUT << "filter_paths: [\n";

				PUSHTAB;

				auto filterPathsLink = monitorNode.getNode("filterPathsLink");
				if(!filterPathsLink.isDisconnected())
				{
					///////////////////////
					auto filterPaths = filterPathsLink.getChildren();
					bool first       = true;

					//__COUTV__(otherParameters.size());
					for(auto& filterPath : filterPaths)
					{
						if(!first)
							OUT << ",";
						OUT << "{ ";

						if(!filterPath.second.status())
							PUSHCOMMENT;

						OUT << "name: " << filterPath.second.getNode("Name").getValue()
						    << " ";
						OUT << "path: " << filterPath.second.getNode("Path").getValue()
						    << " ";

						OUT << "}\n";
						if(!filterPath.second.status())
							POPCOMMENT;
						first = false;
					}
				}

				POPTAB;

				OUT << "]\n";
				OUT << "unique_label: " << monitorNode.getValue() << "\n";
				insertArtProcessBlock(out, tabStr, commentStr, parentPath, dispatcherArt);

				POPTAB;
				OUT << "}\n\n";  // end art
			}
		}

		//--------------------------------------
		// handle ALL add-on parameters
		parentPath = monitorNode.getParentTableName() + "/" +
		             monitorNode.getParentRecordName() + "/" +
		             monitorNode.getParentLinkColumnName() + ":" +
		             monitorNode.getTableName() + "/" + monitorNode.getValue();
		__COUTT__ << "Inserting " << getTypeString(ARTDAQAppType::Monitor)
		          << " add-on parameters... " << parentPath << __E__;
		insertParameters(out,
		                 tabStr,
		                 commentStr,
		                 parentPath,
		                 monitorNode.getNode("addOnParametersLink"),
		                 "daqParameter" /*parameterType*/,
		                 false /*onlyInsertAtTableParameters*/,
		                 true /*includeAtTableParameters*/);

		__COUTT__ << "outputOnlineMonitorFHICL DONE" << __E__;
	}
	catch(...)
	{
		__SS__ << "\n\nError while generating FHiCL for "
		       << getTypeString(ARTDAQAppType::Monitor) << " node at filename '"
		       << filename << "'" << __E__;
		try
		{
			throw;
		}
		catch(const std::runtime_error& e)
		{
			ss << " Here is the error: " << e.what() << __E__;
		}
		catch(const std::exception& e)
		{
			ss << " Here is the error: " << e.what() << __E__;
		}
		out << ss.str();
		out.close();
		__SS_THROW__;
	}

	out.close();
}  // end outputOnlineMonitorFHICL()

//==============================================================================
/// insertArtProcessBlock
///	Note: currently selfRank and selfPort are unused by artdaq fcl
void ARTDAQTableBase::insertArtProcessBlock(std::ostream&      out,
                                            std::string&       tabStr,
                                            std::string&       commentStr,
                                            const std::string& parentPath,
                                            ConfigurationTree  art,
                                            ConfigurationTree  subsystemLink,
                                            size_t             routingTimeoutMs,
                                            size_t             routingRetryCount)
{
	//--------------------------------------
	// handle services
	__COUTT__ << "Filling art.services parentPath =" << parentPath << __E__;
	auto services = art.getNode("servicesLink");
	if(!services.isDisconnected())
	{
		std::string localParentPath =
		    parentPath + "/" + services.getParentLinkColumnName() + ":" +
		    services.getTableName() + "/" +
		    services.getValue();  //unique link so can go further
		__COUTT__ << "Inserting services... " << localParentPath << __E__;
		OUTCL("services: {", services.hasComment() ? services.getComment() : "");
		PUSHTAB;

		//--------------------------------------
		// handle services @table:: parameters
		__COUTT__ << "Inserting services parameters... " << localParentPath << __E__;
		insertParameters(out,
		                 tabStr,
		                 commentStr,
		                 localParentPath,
		                 services.getNode("ServicesParametersLink"),
		                 "daqParameter" /*parameterType*/,
		                 true /*onlyInsertAtTableParameters*/,
		                 false /*includeAtTableParameters*/);

		out << "\n";
		OUT << "ArtdaqSharedMemoryServiceInterface: {\n";
		PUSHTAB;
		OUT << "service_provider: "
		       "ArtdaqSharedMemoryService \n";

		OUTCLF("waiting_time: " << services.getNode("sharedMemoryWaitingTime").getValue(),
		       "" /* comment */,
		       "sharedMemoryWaitingTime");
		OUTCLF("resume_after_timeout: "
		           << (services.getNode("sharedMemoryResumeAfterTimeout").getValue<bool>()
		                   ? "true"
		                   : "false"),
		       "" /* comment */,
		       "sharedMemoryResumeAfterTimeout");
		POPTAB;
		OUT << "} # end ArtdaqSharedMemoryServiceInterface\n\n";

		OUT << "ArtdaqFragmentNamingServiceInterface: {\n";
		PUSHTAB;
		OUT << "service_provider: "
		       "ArtdaqFragmentNamingService \n";
		OUTCLF("helper_plugin: "
		           << services.getNode("fragmentNamingServiceProvider").getValue(),
		       "" /* comment */,
		       "fragmentNamingServiceProvider");
		POPTAB;
		OUT << "} # end ArtdaqFragmentNamingServiceInterface\n\n";

		//--------------------------------------
		// handle services NOT @table:: parameters
		__COUTT__ << "Inserting services parameters... " << localParentPath << __E__;
		insertParameters(out,
		                 tabStr,
		                 commentStr,
		                 localParentPath,
		                 services.getNode("ServicesParametersLink"),
		                 "daqParameter" /*parameterType*/,
		                 false /*onlyInsertAtTableParameters*/,
		                 false /*includeAtTableParameters*/);

		POPTAB;
		OUT << "} # end services\n\n";  // end services
	}                                   //end services
	else
	{
		__COUTS__(3) << "No services found" << __E__;
		std::string localParentPath =
		    parentPath + "/" + services.getParentLinkColumnName();
		OUTCL("# no services found", "" /* comment*/);
	}

	//--------------------------------------
	// handle outputs
	__COUTT__ << "Filling art.outputs parentPath =" << parentPath << __E__;
	auto outputs = art.getNode("outputsLink");
	if(!outputs.isDisconnected())
	{
		std::string localParentPath =
		    parentPath + "/" +
		    outputs.getParentLinkColumnName();  //group link so cannot go further
		__COUTT__ << "Inserting output... " << localParentPath << __E__;
		OUTCL("outputs: {", "" /* comment */);
		PUSHTAB;

		auto outputPlugins = outputs.getChildren();
		for(auto& outputPlugin : outputPlugins)
		{
			if(!outputPlugin.second.status())
				PUSHCOMMENT;

			__COUTT__ << "Inserting output parameters... " << localParentPath << __E__;
			std::string localParentPath2 =
			    localParentPath + ":" + outputPlugin.second.getTableName() + ":" +
			    outputs.getParentLinkIndex() + ":" + outputs.getParentLinkID() + "/" +
			    outputPlugin.second.getValue();
			__COUTT__ << "Inserting output... " << localParentPath2 << __E__;
			OUTCL2F(
			    outputPlugin.second.getNode("outputKey").getValue() << ": {",
			    outputPlugin.second.hasComment() ? outputPlugin.second.getComment() : "",
			    "outputKey");
			PUSHTAB;

			__COUTT__ << "insertModuleType... " << localParentPath2 << __E__;
			std::string moduleType =
			    insertModuleType(out,
			                     tabStr,
			                     commentStr,
			                     localParentPath2,
			                     outputPlugin.second.getNode("outputModuleType"));

			//--------------------------------------
			// handle ALL output parameters
			__COUTT__ << "Inserting output parameters... " << localParentPath << __E__;
			insertParameters(out,
			                 tabStr,
			                 commentStr,
			                 localParentPath,
			                 outputPlugin.second.getNode("outputModuleParameterLink"),
			                 "outputParameter" /*parameterType*/,
			                 false /*onlyInsertAtTableParameters*/,
			                 true /*includeAtTableParameters*/);

			if(outputPlugin.second.getNode("outputModuleType").getValue() ==
			       "BinaryNetOutput" ||
			   outputPlugin.second.getNode("outputModuleType").getValue() ==
			       "RootNetOutput")
			{
				OUT << "destinations: { # empty placeholder, '"
				    << outputPlugin.second.getNode("outputModuleType").getValue()
				    << "' destinations handled by artdaq interface\n";
				OUT << "}\n\n";  // end destinations

				OUT << "routing_table_config: {\n";
				PUSHTAB;

				auto mySubsystemID          = 1;
				auto destinationSubsystemID = 1;
				if(!subsystemLink.isDisconnected())
				{
					mySubsystemID = getSubsytemId(subsystemLink);
				}
				destinationSubsystemID = info_.subsystems[mySubsystemID].destination;
				if(info_.subsystems[destinationSubsystemID].hasRoutingManager)
				{
					std::string localParentPath =
					    parentPath + "/" + subsystemLink.getParentLinkColumnName() + ":" +
					    subsystemLink.getTableName() + "/" + subsystemLink.getValue();
					__COUTT__ << "Inserting routing manager... " << localParentPath
					          << __E__;
					OUTCL("use_routing_manager: true",
					      "auto-generated because subsystem '" +
					          std::to_string(destinationSubsystemID) +
					          "' has Routing Manager added");

					OUTCLF(
					    "routing_manager_hostname: \""
					        << info_.subsystems[destinationSubsystemID].routingManagerHost
					        << "\"",
					    "" /* comment */,
					    ARTDAQTableBase::ARTDAQ_TYPE_TABLE_HOSTNAME);
					OUT << "table_update_port: 0\n";
					OUT << "table_update_address: \"0.0.0.0\"\n";
					OUT << "table_update_multicast_interface: \"0.0.0.0\"\n";
					OUT << "table_acknowledge_port : 0\n";
					OUT << "routing_timeout_ms: " << routingTimeoutMs << "\n";
					OUT << "routing_retry_count: " << routingRetryCount << "\n";
				}
				else
				{
					OUTCF("use_routing_manager: false",
					      "auto-generated if subsystem '" +
					          std::to_string(destinationSubsystemID) +
					          "' has Routing Manager added",
					      subsystemLink.getParentLinkColumnName());
				}

				if(outputPlugin.second.getNode("outputModuleType").getValue() ==
				   "RootNetOutput")
				{
					info_.subsystems[mySubsystemID].eventMode = true;
				}

				POPTAB;
				OUT << "}\n";  // end routing_table_config
			}
			if(outputPlugin.second.getNode("outputModuleType").getValue() ==
			       "TransferOutput" ||
			   outputPlugin.second.getNode("outputModuleType").getValue() ==
			       "TransferOutputReliable")
			{
				OUT << "transfer_plugin: @local::TransferPluginConfig \n";
			}

			POPTAB;
			OUT << "} # end " << outputPlugin.second.getNode("outputKey").getValue()
			    << "\n\n";  // end output module

			if(!outputPlugin.second.status())
				POPCOMMENT;
		}

		POPTAB;
		OUT << "} # end outputs\n\n";  // end outputs
	}                                  //end outputs
	else
	{
		__COUTS__(3) << "No outputs found" << __E__;
		std::string localParentPath =
		    parentPath + "/" + outputs.getParentLinkColumnName();
		OUTCL("# no outputs found", "" /* comment*/);
	}

	//--------------------------------------
	// handle physics
	__COUTT__ << "Filling art.physics parentPath =" << parentPath << __E__;
	auto physics = art.getNode("physicsLink");
	if(!physics.isDisconnected())
	{
		__COUTT__ << "Inserting physics... " << parentPath << __E__;
		std::string localParentPath = parentPath + "/" +
		                              physics.getParentLinkColumnName() + ":" +
		                              physics.getTableName() + "/" +
		                              physics.getValue();  //unique link so can go further

		///////////////////////
		OUTCL("physics: {", physics.hasComment() ? services.getComment() : "");

		PUSHTAB;

		//--------------------------------------
		// handle only @table:: physics parameters
		__COUTT__ << "Inserting physics other parameters... " << localParentPath << __E__;
		insertParameters(out,
		                 tabStr,
		                 commentStr,
		                 localParentPath,
		                 physics.getNode("physicsOtherParametersLink"),
		                 "physicsParameter" /*parameterType*/,
		                 true /*onlyInsertAtTableParameters*/,
		                 false /*includeAtTableParameters*/);

		auto analyzers = physics.getNode("analyzersLink");
		if(!analyzers.isDisconnected())
		{
			__COUTT__ << "Inserting art.physics.analyzers... " << localParentPath
			          << __E__;
			std::string localParentPath2 =
			    localParentPath + "/" + analyzers.getParentLinkColumnName();  //group link
			__COUTT__ << "Inserting art.physics.analyzers... " << localParentPath2
			          << __E__;

			///////////////////////
			out << "\n";
			OUTCL2("analyzers: {", "" /* comment */);
			PUSHTAB;

			bool first   = true;
			auto modules = analyzers.getChildren();
			for(auto& module : modules)
			{
				if(!module.second.status())
					PUSHCOMMENT;

				if(!first)
					out << "\n";
				first = false;

				auto analyzerNodeParameterLink =
				    module.second.getNode("analyzerModuleParameterLink");
				//--------------------------------------
				// handle only @table:: analyzer parameters
				__COUTT__ << "Inserting analyzer @table parameters... "
				          << localParentPath2 << __E__;
				std::string localParentPath3 =
				    localParentPath2 + ":" + module.second.getTableName() + ":" +
				    analyzers.getParentLinkIndex() + ":" + analyzers.getParentLinkID() +
				    "/" + module.second.getValue();
				__COUTT__ << "Inserting analyzer @table parameters... "
				          << localParentPath3 << __E__;
				insertParameters(out,
				                 tabStr,
				                 commentStr,
				                 localParentPath3,
				                 analyzerNodeParameterLink,
				                 "analyzerParameter" /*parameterType*/,
				                 true /*onlyInsertAtTableParameters*/,
				                 false /*includeAtTableParameters*/);

				OUT << module.second.getNode("analyzerKey").getValue() << ": {\n";
				PUSHTAB;
				insertModuleType(out,
				                 tabStr,
				                 commentStr,
				                 localParentPath3,
				                 module.second.getNode("analyzerModuleType"));

				//--------------------------------------
				// handle NOT @table:: producer parameters
				__COUTT__ << "Inserting analayzer not @table parameters... "
				          << localParentPath3 << __E__;
				insertParameters(out,
				                 tabStr,
				                 commentStr,
				                 localParentPath3,
				                 analyzerNodeParameterLink,
				                 "analyzerParameter" /*parameterType*/,
				                 false /*onlyInsertAtTableParameters*/,
				                 false /*includeAtTableParameters*/);

				POPTAB;
				OUT << "}\n";  // end analyzer module

				if(!module.second.status())
					POPCOMMENT;
			}  //end analyzer module loop
			POPTAB;
			OUT << "} # end physics.analyzers\n\n";  // end analyzers
		}
		else
		{
			__COUTS__(3) << "No analyzers found" << __E__;
			std::string localParentPath2 =
			    localParentPath + "/" + analyzers.getParentLinkColumnName();
			OUTCL2("# no analyzers found", "" /* comment*/);
		}

		auto producers = physics.getNode("producersLink");
		if(!producers.isDisconnected())
		{
			__COUTT__ << "Inserting art.physics.producers... " << localParentPath
			          << __E__;
			std::string localParentPath2 =
			    localParentPath + "/" + producers.getParentLinkColumnName();  //group link

			///////////////////////
			out << "\n";
			OUTCL2("producers: {", "" /* comment */);
			PUSHTAB;

			bool first   = true;
			auto modules = producers.getChildren();
			for(auto& module : modules)
			{
				if(!module.second.status())
					PUSHCOMMENT;

				if(!first)
					out << "\n";
				first = false;

				auto producerNodeParameterLink =
				    module.second.getNode("producerModuleParameterLink");
				//--------------------------------------
				// handle only @table:: producer parameters
				__COUTT__ << "Inserting producer @table parameters... "
				          << localParentPath2 << __E__;
				std::string localParentPath3 =
				    localParentPath2 + ":" + module.second.getTableName() + ":" +
				    producers.getParentLinkIndex() + ":" + producers.getParentLinkID() +
				    "/" + module.second.getValue();
				__COUTT__ << "Inserting producer @table parameters... "
				          << localParentPath3 << __E__;
				insertParameters(out,
				                 tabStr,
				                 commentStr,
				                 localParentPath3,
				                 producerNodeParameterLink,
				                 "producerParameter" /*parameterType*/,
				                 true /*onlyInsertAtTableParameters*/,
				                 false /*includeAtTableParameters*/);

				if(module.second.status() &&
				   module.second.getNode("producerModuleType").getValue() == "")
				{
					std::string tmp  = localParentPath2;
					localParentPath2 = localParentPath3;
					OUTCL2F("# skipping '" << module.second.getValue()
					                       << "' with empty module type",
					        "" /* comment */,
					        "producerModuleType");
					localParentPath2 = tmp;
					continue;
				}

				OUT << module.second.getNode("producerKey").getValue() << ": {\n";
				PUSHTAB;

				insertModuleType(out,
				                 tabStr,
				                 commentStr,
				                 localParentPath3,
				                 module.second.getNode("producerModuleType"));

				//--------------------------------------
				// handle NOT @table:: producer parameters
				__COUTT__ << "Inserting producer not @table parameters... "
				          << localParentPath3 << __E__;
				insertParameters(out,
				                 tabStr,
				                 commentStr,
				                 localParentPath3,
				                 producerNodeParameterLink,
				                 "producerParameter" /*parameterType*/,
				                 false /*onlyInsertAtTableParameters*/,
				                 false /*includeAtTableParameters*/);

				POPTAB;
				OUT << "}\n";  // end producer module

				if(!module.second.status())
					POPCOMMENT;
			}  //end producer module loop
			POPTAB;
			OUT << "} # end physics.producers\n\n";  // end producers
		}
		else
		{
			__COUTS__(3) << "No producers found" << __E__;
			std::string localParentPath2 =
			    localParentPath + "/" + producers.getParentLinkColumnName();
			OUTCL2("# no producers found", "" /* comment*/);
		}

		auto filters = physics.getNode("filtersLink");
		if(!filters.isDisconnected())
		{
			__COUTT__ << "Inserting art.physics.filters... " << localParentPath << __E__;
			std::string localParentPath2 =
			    localParentPath + "/" + filters.getParentLinkColumnName();  //group link

			///////////////////////
			out << "\n";
			OUTCL2("filters: {", "" /* comment */);
			PUSHTAB;

			bool first   = true;
			auto modules = filters.getChildren();
			for(auto& module : modules)
			{
				if(!module.second.status())
					PUSHCOMMENT;

				if(!first)
					out << "\n";
				first = false;

				auto filterNodeParameterLink =
				    module.second.getNode("filterModuleParameterLink");
				//--------------------------------------
				// handle only @table:: filter parameters
				__COUTT__ << "Inserting filter @table parameters... " << localParentPath2
				          << __E__;
				std::string localParentPath3 =
				    localParentPath2 + ":" + module.second.getTableName() + ":" +
				    filters.getParentLinkIndex() + ":" + filters.getParentLinkID() + "/" +
				    module.second.getValue();
				__COUTT__ << "Inserting filter @table parameters... " << localParentPath3
				          << __E__;
				insertParameters(out,
				                 tabStr,
				                 commentStr,
				                 localParentPath3,
				                 filterNodeParameterLink,
				                 "filterParameter" /*parameterType*/,
				                 true /*onlyInsertAtTableParameters*/,
				                 false /*includeAtTableParameters*/);
				if(module.second.status() &&
				   module.second.getNode("filterModuleType").getValue() == "")
				{
					std::string tmp  = localParentPath2;
					localParentPath2 = localParentPath3;
					OUTCL2F("# skipping '" << module.second.getValue()
					                       << "' with empty module type",
					        "" /* comment */,
					        "filterModuleType");
					localParentPath2 = tmp;
					continue;
				}

				OUT << module.second.getNode("filterKey").getValue() << ": {\n";
				PUSHTAB;

				insertModuleType(out,
				                 tabStr,
				                 commentStr,
				                 localParentPath3,
				                 module.second.getNode("filterModuleType"));

				//--------------------------------------
				// handle NOT @table:: filter parameters
				__COUTT__ << "Inserting filter not @table parameters... "
				          << localParentPath3 << __E__;
				insertParameters(out,
				                 tabStr,
				                 commentStr,
				                 localParentPath3,
				                 filterNodeParameterLink,
				                 "filterParameter" /*parameterType*/,
				                 false /*onlyInsertAtTableParameters*/,
				                 false /*includeAtTableParameters*/);

				POPTAB;
				OUT << "}\n";  // end filter module

				if(!module.second.status())
					POPCOMMENT;
			}  //end filter module loop
			POPTAB;
			OUT << "} # end physics.filters\n\n";  // end filters
		}
		else
		{
			__COUTS__(3) << "No filters found" << __E__;
			std::string localParentPath2 =
			    localParentPath + "/" + services.getParentLinkColumnName();
			OUTCL2("# no filters found", "" /* comment*/);
		}

		//--------------------------------------
		// handle NOT @table:: physics parameters
		__COUTT__ << "Inserting art.physics not @table parameters... " << localParentPath
		          << __E__;
		insertParameters(out,
		                 tabStr,
		                 commentStr,
		                 localParentPath,
		                 physics.getNode("physicsOtherParametersLink"),
		                 "physicsParameter" /*parameterType*/,
		                 false /*onlyInsertAtTableParameters*/,
		                 false /*includeAtTableParameters*/);

		POPTAB;
		OUT << "} # end physics\n\n";  // end physics
	}
	else
	{
		__COUTS__(3) << "No physics found" << __E__;
		std::string localParentPath =
		    parentPath + "/" + physics.getParentLinkColumnName();
		OUTCL("# no physics found", "" /* comment*/);
	}

	//--------------------------------------
	// handle source
	__COUTT__ << "Filling art.source" << __E__;
	auto source = art.getNode("sourceLink");
	if(!source.isDisconnected())
	{
		__COUTT__ << "Inserting source... " << parentPath << __E__;
		std::string localParentPath = parentPath + "/" +
		                              source.getParentLinkColumnName() + ":" +
		                              source.getTableName() + "/" +
		                              source.getValue();  //unique link so can go further
		OUTCL("source: {", source.hasComment() ? source.getComment() : "");
		PUSHTAB;
		insertModuleType(
		    out, tabStr, commentStr, parentPath, source.getNode("sourceModuleType"));
		POPTAB;
		OUT << "}\n\n";  // end source
	}
	else
	{
		std::string localParentPath = parentPath + "/" + source.getParentLinkColumnName();
		OUTCL("source: { # auto-generated default, to change provide a source link",
		      "" /* comment*/);
		PUSHTAB;
		OUT << "module_type: ArtdaqInput";
		POPTAB;
		OUT << "}\n\n";  // end source
	}

	//--------------------------------------
	// handle process_name
	__COUTT__ << "Writing art.process_name" << __E__;
	OUTCF("process_name: " << art.getNode(ARTDAQTableBase::colARTDAQArt_.colProcessName_),
	      "",
	      ARTDAQTableBase::colARTDAQArt_.colProcessName_);

	//--------------------------------------
	// handle art @table:: art add on parameters
	__COUTT__ << "Inserting art @table parameters... " << parentPath << __E__;
	insertParameters(out,
	                 tabStr,
	                 commentStr,
	                 parentPath,
	                 art.getNode("AddOnParametersLink"),
	                 "daqParameter" /*parameterType*/,
	                 false /*onlyInsertAtTableParameters*/,
	                 true /*includeAtTableParameters*/);

}  // end insertArtProcessBlock()

//==============================================================================
void ARTDAQTableBase::outputRoutingManagerFHICL(
    const ConfigurationTree& routingManagerNode,
    size_t                   routingTimeoutMs /* = DEFAULT_ROUTING_TIMEOUT_MS */,
    size_t                   routingRetryCount /* = DEFAULT_ROUTING_RETRY_COUNT */)
{
	if(ARTDAQ_DONOTWRITE_FCL)
	{
		__COUT__ << "Skipping fcl generation." << __E__;
		return;
	}

	std::string filename =
	    getFHICLFilename(ARTDAQAppType::RoutingManager, routingManagerNode.getValue());

	/////////////////////////
	// generate xdaq run parameter file
	std::fstream out;

	std::string tabStr     = "";
	std::string commentStr = "";

	__COUTV__(filename);
	out.open(filename, std::fstream::out | std::fstream::trunc);
	if(out.fail())
	{
		__SS__ << "Failed to open ARTDAQ RoutingManager fcl file: " << filename << __E__;
		__SS_THROW__;
	}

	try  //catch and give error in fcl file if issue!
	{
		//--------------------------------------
		// header
		OUT << "###########################################################" << __E__;
		OUT << "#" << __E__;
		OUT << "# artdaq " << getTypeString(ARTDAQAppType::RoutingManager)
		    << " fcl configuration file produced by otsdaq." << __E__;
		OUT << "# 	Creation time:                  \t"
		    << StringMacros::getTimestampString() << __E__;
		OUT << "# 	Original filename:              \t" << filename << __E__;
		OUT << "#	otsdaq-ARTDAQ RoutingManager UID:\t" << routingManagerNode.getValue()
		    << __E__;
		OUT << "#" << __E__;
		OUT << "###########################################################" << __E__;
		OUT << "\n\n";

		// no primary link to table tree for reader node!
		try
		{
			if(routingManagerNode.isDisconnected())
			{
				// create empty fcl
				OUT << "{}\n\n";
				out.close();
				return;
			}
		}
		catch(const std::runtime_error&)
		{
			//__COUT__ << "Ignoring error, assume this a valid UID node." << __E__;
			// error is expected here for UIDs.. so just ignore
			// this check is valuable if source node is a unique-Link node, rather than UID
		}

		//--------------------------------------
		// handle daq
		OUT << "daq: {\n";
		PUSHTAB;

		OUT << "policy: {\n";
		PUSHTAB;
		auto policyName =
		    routingManagerNode.getNode("routingPolicyPluginType").getValue();
		if(policyName == "DEFAULT")
			policyName = "NoOp";
		OUT << "policy: " << policyName << "\n";
		OUT << "receiver_ranks: []\n";

		// shared and unique parameters
		auto parametersLink = routingManagerNode.getNode("routingPolicyParametersLink");
		if(!parametersLink.isDisconnected())
		{
			auto parameters = parametersLink.getChildren();
			for(auto& parameter : parameters)
			{
				if(!parameter.second.status())
					PUSHCOMMENT;

				//				__COUT__ <<
				// parameter.second.getNode("daqParameterKey").getValue() <<
				//						": " <<
				//						parameter.second.getNode("daqParameterValue").getValue()
				//						<<
				//						"\n";

				auto comment =
				    parameter.second.getNode(TableViewColumnInfo::COL_NAME_COMMENT);
				OUT << parameter.second.getNode("daqParameterKey").getValue() << ": "
				    << parameter.second.getNode("daqParameterValue").getValue()
				    << (comment.isDefaultValue() ? "" : ("\t # " + comment.getValue()))
				    << "\n";

				if(!parameter.second.status())
					POPCOMMENT;
			}
		}

		POPTAB;
		OUT << "}\n";

		OUT << "use_routing_manager: true\n";

		auto routingManagerSubsystemID   = 1;
		auto routingManagerSubsystemLink = routingManagerNode.getNode("SubsystemLink");
		std::string rmHost               = "localhost";
		if(!routingManagerSubsystemLink.isDisconnected())
		{
			routingManagerSubsystemID = getSubsytemId(routingManagerSubsystemLink);
			rmHost = info_.subsystems[routingManagerSubsystemID].routingManagerHost;
		}
		if(rmHost == "localhost" || rmHost == "127.0.0.1")
		{
			char hostbuf[HOST_NAME_MAX + 1];
			gethostname(hostbuf, HOST_NAME_MAX);
			rmHost = std::string(hostbuf);
		}

		// Bookkept parameters
		OUT << "routing_manager_hostname: \"" << rmHost << "\"\n";
		OUT << "sender_ranks: []\n";
		OUT << "table_update_port: 0\n";
		OUT << "table_update_address: \"0.0.0.0\"\n";
		OUT << "table_acknowledge_port: 0\n";
		OUT << "token_receiver: {\n";
		PUSHTAB;

		OUT << "routing_token_port: 0\n";

		POPTAB;
		OUT << "}\n";

		// Optional parameters
		auto tableUpdateIntervalMs =
		    routingManagerNode.getNode("tableUpdateIntervalMs").getValue();
		if(tableUpdateIntervalMs != "DEFAULT")
		{
			OUT << "table_update_interval_ms: " << tableUpdateIntervalMs << "\n";
		}
		auto tableAckRetryCount =
		    routingManagerNode.getNode("tableAckRetryCount").getValue();
		if(tableAckRetryCount != "DEFAULT")
		{
			OUT << "table_ack_retry_count: " << tableAckRetryCount << "\n";
		}

		OUT << "routing_timeout_ms: " << routingTimeoutMs << "\n";
		OUT << "routing_retry_count: " << routingRetryCount << "\n";

		std::string parentPath = routingManagerNode.getParentTableName() + "/" +
		                         routingManagerNode.getParentRecordName() + "/" +
		                         routingManagerNode.getParentLinkColumnName() + ":" +
		                         routingManagerNode.getTableName() + "/" +
		                         routingManagerNode.getValue();
		insertMetricsBlock(OUT, tabStr, commentStr, parentPath, routingManagerNode);

		POPTAB;
		OUT << "}\n\n";  // end daq
		__COUTT__ << "outputReaderFHICL DONE" << __E__;
	}
	catch(...)
	{
		__SS__ << "\n\nError while generating FHiCL for "
		       << getTypeString(ARTDAQAppType::RoutingManager) << " node at filename '"
		       << filename << "'" << __E__;
		try
		{
			throw;
		}
		catch(const std::runtime_error& e)
		{
			ss << " Here is the error: " << e.what() << __E__;
		}
		catch(const std::exception& e)
		{
			ss << " Here is the error: " << e.what() << __E__;
		}
		out << ss.str();
		out.close();
		__SS_THROW__;
	}
	out.close();
}  // end outputReaderFHICL()

//==============================================================================
const ARTDAQTableBase::ARTDAQInfo& ARTDAQTableBase::extractARTDAQInfo(
    ConfigurationTree artdaqSupervisorNode,
    bool              getStatusFalseNodes /* = false */,
    bool              doWriteFHiCL /* = false */,
    size_t            maxFragmentSizeBytes /* = DEFAULT_MAX_FRAGMENT_SIZE*/,
    size_t            routingTimeoutMs /* = DEFAULT_ROUTING_TIMEOUT_MS */,
    size_t            routingRetryCount /* = DEFAULT_ROUTING_RETRY_COUNT */,
    ProgressBar*      progressBar /* = 0 */)
{
	if(progressBar)
		progressBar->step();

	// reset info every time, because it could be called after configuration manipulations
	info_.subsystems.clear();
	info_.processes.clear();

	if(progressBar)
		progressBar->step();

	info_.subsystems[NULL_SUBSYSTEM_DESTINATION].id    = NULL_SUBSYSTEM_DESTINATION;
	info_.subsystems[NULL_SUBSYSTEM_DESTINATION].label = NULL_SUBSYSTEM_DESTINATION_LABEL;

	// if no supervisor, then done
	if(artdaqSupervisorNode.isDisconnected())
	{
		__COUT__ << "artdaqSupervisorNode is disconnected." << __E__;
		return info_;
	}

	// We do RoutingManagers first so we can properly fill in routing tables later
	extractRoutingManagersInfo(artdaqSupervisorNode,
	                           getStatusFalseNodes,
	                           doWriteFHiCL,
	                           routingTimeoutMs,
	                           routingRetryCount);
	__COUT__ << "artdaqSupervisorNode RoutingManager size: "
	         << info_.processes.at(ARTDAQAppType::RoutingManager).size() << __E__;

	if(progressBar)
		progressBar->step();

	extractBoardReadersInfo(artdaqSupervisorNode,
	                        getStatusFalseNodes,
	                        doWriteFHiCL,
	                        maxFragmentSizeBytes,
	                        routingTimeoutMs,
	                        routingRetryCount);
	__COUT__ << "artdaqSupervisorNode BoardReader size: "
	         << info_.processes.at(ARTDAQAppType::BoardReader).size() << __E__;

	if(progressBar)
		progressBar->step();

	extractEventBuildersInfo(
	    artdaqSupervisorNode, getStatusFalseNodes, doWriteFHiCL, maxFragmentSizeBytes);
	__COUT__ << "artdaqSupervisorNode EventBuilder size: "
	         << info_.processes.at(ARTDAQAppType::EventBuilder).size() << __E__;

	if(progressBar)
		progressBar->step();

	extractDataLoggersInfo(
	    artdaqSupervisorNode, getStatusFalseNodes, doWriteFHiCL, maxFragmentSizeBytes);
	__COUT__ << "artdaqSupervisorNode DataLogger size: "
	         << info_.processes.at(ARTDAQAppType::DataLogger).size() << __E__;

	if(progressBar)
		progressBar->step();

	extractDispatchersInfo(
	    artdaqSupervisorNode, getStatusFalseNodes, doWriteFHiCL, maxFragmentSizeBytes);
	__COUT__ << "artdaqSupervisorNode Dispatcher size: "
	         << info_.processes.at(ARTDAQAppType::Dispatcher).size() << __E__;

	if(progressBar)
		progressBar->step();

	return info_;
}  // end extractARTDAQInfo()

//==============================================================================
void ARTDAQTableBase::extractRoutingManagersInfo(ConfigurationTree artdaqSupervisorNode,
                                                 bool              getStatusFalseNodes,
                                                 bool              doWriteFHiCL,
                                                 size_t            routingTimeoutMs,
                                                 size_t            routingRetryCount)
{
	__COUT__ << "Checking for Routing Managers..." << __E__;
	info_.processes[ARTDAQAppType::RoutingManager].clear();

	ConfigurationTree rmsLink =
	    artdaqSupervisorNode.getNode(colARTDAQSupervisor_.colLinkToRoutingManagers_);
	if(!rmsLink.isDisconnected() && rmsLink.getChildren().size() > 0)
	{
		std::vector<std::pair<std::string, ConfigurationTree>> routingManagers =
		    rmsLink.getChildren();

		__COUT__ << "There are " << routingManagers.size()
		         << " configured Routing Managers" << __E__;

		for(auto& routingManager : routingManagers)
		{
			const std::string& rmUID = routingManager.first;

			if(getStatusFalseNodes || routingManager.second.status())
			{
				std::string rmHost =
				    routingManager.second
				        .getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_HOSTNAME)
				        .getValueWithDefault("localhost");
				if(rmHost == "localhost" || rmHost == "127.0.0.1")
				{
					char hostbuf[HOST_NAME_MAX + 1];
					gethostname(hostbuf, HOST_NAME_MAX);
					rmHost = std::string(hostbuf);
				}

				std::string rmAP =
				    routingManager.second
				        .getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_ALLOWED_PROCESSORS)
				        .getValueWithDefault("");

				int               routingManagerSubsystemID = 1;
				ConfigurationTree routingManagerSubsystemLink =
				    routingManager.second.getNode(
				        ARTDAQTableBase::ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK);
				if(!routingManagerSubsystemLink.isDisconnected())
				{
					routingManagerSubsystemID =
					    getSubsytemId(routingManagerSubsystemLink);

					//__COUTV__(routingManagerSubsystemID);
					info_.subsystems[routingManagerSubsystemID].id =
					    routingManagerSubsystemID;

					const std::string& routingManagerSubsystemName =
					    routingManagerSubsystemLink.getUIDAsString();
					//__COUTV__(routingManagerSubsystemName);

					info_.subsystems[routingManagerSubsystemID].label =
					    routingManagerSubsystemName;

					if(info_.subsystems[routingManagerSubsystemID].hasRoutingManager)
					{
						__SS__ << "Error: You cannot have multiple Routing Managers in a "
						          "subsystem!";
						__SS_THROW__;
						return;
					}

					auto routingManagerSubsystemDestinationLink =
					    routingManagerSubsystemLink.getNode(
					        colARTDAQSubsystem_.colLinkToDestination_);
					if(routingManagerSubsystemDestinationLink.isDisconnected())
					{
						// default to no destination when no link
						info_.subsystems[routingManagerSubsystemID].destination = 0;
					}
					else
					{
						// get destination subsystem id
						info_.subsystems[routingManagerSubsystemID].destination =
						    getSubsytemId(routingManagerSubsystemDestinationLink);
					}
					//__COUTV__(info_.subsystems[routingManagerSubsystemID].destination);

					// add this subsystem to destination subsystem's sources, if not
					// there
					if(!info_.subsystems.count(
					       info_.subsystems[routingManagerSubsystemID].destination) ||
					   !info_
					        .subsystems[info_.subsystems[routingManagerSubsystemID]
					                        .destination]
					        .sources.count(routingManagerSubsystemID))
					{
						info_
						    .subsystems[info_.subsystems[routingManagerSubsystemID]
						                    .destination]
						    .sources.insert(routingManagerSubsystemID);
					}

				}  // end subsystem instantiation

				__COUT__ << "Found Routing Manager with UID " << rmUID
				         << ", DAQInterface Hostname " << rmHost << ", and Subsystem "
				         << routingManagerSubsystemID << __E__;
				info_.processes[ARTDAQAppType::RoutingManager].emplace_back(
				    rmUID,
				    rmHost,
				    rmAP,
				    routingManagerSubsystemID,
				    ARTDAQAppType::RoutingManager,
				    routingManager.second.status());

				info_.subsystems[routingManagerSubsystemID].hasRoutingManager  = true;
				info_.subsystems[routingManagerSubsystemID].routingManagerHost = rmHost;

				if(doWriteFHiCL)
				{
					outputRoutingManagerFHICL(
					    routingManager.second, routingTimeoutMs, routingRetryCount);

					flattenFHICL(ARTDAQAppType::RoutingManager,
					             routingManager.second.getValue());
				}
			}
			else  // disabled
			{
				__COUT__ << "Routing Manager " << rmUID << " is disabled." << __E__;
			}
		}  // end routing manager loop
	}
}  // end extractRoutingManagersInfo()

//==============================================================================
void ARTDAQTableBase::extractBoardReadersInfo(ConfigurationTree artdaqSupervisorNode,
                                              bool              getStatusFalseNodes,
                                              bool              doWriteFHiCL,
                                              size_t            maxFragmentSizeBytes,
                                              size_t            routingTimeoutMs,
                                              size_t            routingRetryCount)
{
	__COUT__ << "Checking for Board Readers..." << __E__;
	info_.processes[ARTDAQAppType::BoardReader].clear();

	ConfigurationTree readersLink =
	    artdaqSupervisorNode.getNode(colARTDAQSupervisor_.colLinkToBoardReaders_);
	if(!readersLink.isDisconnected() && readersLink.getChildren().size() > 0)
	{
		std::vector<std::pair<std::string, ConfigurationTree>> readers =
		    readersLink.getChildren();
		__COUT__ << "There are " << readers.size() << " configured Board Readers."
		         << __E__;

		for(auto& reader : readers)
		{
			const std::string& readerUID = reader.first;

			if(getStatusFalseNodes || reader.second.status())
			{
				std::string readerHost =
				    reader.second.getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_HOSTNAME)
				        .getValueWithDefault("localhost");
				std::string readerAP =
				    reader.second
				        .getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_ALLOWED_PROCESSORS)
				        .getValueWithDefault("");

				int               readerSubsystemID = 1;
				ConfigurationTree readerSubsystemLink =
				    reader.second.getNode(ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK);
				if(!readerSubsystemLink.isDisconnected())
				{
					readerSubsystemID = getSubsytemId(readerSubsystemLink);
					//__COUTV__(readerSubsystemID);
					info_.subsystems[readerSubsystemID].id = readerSubsystemID;

					const std::string& readerSubsystemName =
					    readerSubsystemLink.getUIDAsString();
					//__COUTV__(readerSubsystemName);

					info_.subsystems[readerSubsystemID].label = readerSubsystemName;

					auto readerSubsystemDestinationLink = readerSubsystemLink.getNode(
					    colARTDAQSubsystem_.colLinkToDestination_);
					if(readerSubsystemDestinationLink.isDisconnected())
					{
						// default to no destination when no link
						info_.subsystems[readerSubsystemID].destination = 0;
					}
					else
					{
						// get destination subsystem id
						info_.subsystems[readerSubsystemID].destination =
						    getSubsytemId(readerSubsystemDestinationLink);
					}
					//__COUTV__(info_.subsystems[readerSubsystemID].destination);

					// add this subsystem to destination subsystem's sources, if not
					// there
					if(!info_.subsystems.count(
					       info_.subsystems[readerSubsystemID].destination) ||
					   !info_.subsystems[info_.subsystems[readerSubsystemID].destination]
					        .sources.count(readerSubsystemID))
					{
						info_.subsystems[info_.subsystems[readerSubsystemID].destination]
						    .sources.insert(readerSubsystemID);
					}

				}  // end subsystem instantiation

				__COUT__ << "Found Board Reader with UID " << readerUID
				         << ", DAQInterface Hostname " << readerHost << ", and Subsystem "
				         << readerSubsystemID << __E__;
				info_.processes[ARTDAQAppType::BoardReader].emplace_back(
				    readerUID,
				    readerHost,
				    readerAP,
				    readerSubsystemID,
				    ARTDAQAppType::BoardReader,
				    reader.second.status());

				if(doWriteFHiCL)
				{
					outputBoardReaderFHICL(reader.second,
					                       maxFragmentSizeBytes,
					                       routingTimeoutMs,
					                       routingRetryCount);

					flattenFHICL(ARTDAQAppType::BoardReader, reader.second.getValue());
				}
			}
			else  // disabled
			{
				__COUT__ << "Board Reader " << readerUID << " is disabled." << __E__;
			}
		}  // end reader loop
	}
	else
	{
		__COUT_WARN__ << "There should be at least one Board Reader!";
		//__SS_THROW__;
		// return;
	}
}  // end extractBoardReadersInfo()

//==============================================================================
void ARTDAQTableBase::extractEventBuildersInfo(ConfigurationTree artdaqSupervisorNode,
                                               bool              getStatusFalseNodes,
                                               bool              doWriteFHiCL,
                                               size_t            maxFragmentSizeBytes)
{
	__COUT__ << "Checking for Event Builders..." << __E__;
	info_.processes[ARTDAQAppType::EventBuilder].clear();

	ConfigurationTree buildersLink =
	    artdaqSupervisorNode.getNode(colARTDAQSupervisor_.colLinkToEventBuilders_);
	if(!buildersLink.isDisconnected() && buildersLink.getChildren().size() > 0)
	{
		std::vector<std::pair<std::string, ConfigurationTree>> builders =
		    buildersLink.getChildren();

		std::string lastBuilderFcl[2],
		    flattenedLastFclParts
		        [2];  //same handling as otsdaq/otsdaq/TablePlugins/ARTDAQEventBuilderTable_table.cc:69
		for(auto& builder : builders)
		{
			const std::string& builderUID = builder.first;
			__COUTV__(builderUID);

			if(getStatusFalseNodes || builder.second.status())
			{
				std::string builderHost =
				    builder.second.getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_HOSTNAME)
				        .getValueWithDefault("localhost");
				std::string builderAP =
				    builder.second
				        .getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_ALLOWED_PROCESSORS)
				        .getValueWithDefault("");

				int               builderSubsystemID = 1;
				ConfigurationTree builderSubsystemLink =
				    builder.second.getNode(ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK);
				if(!builderSubsystemLink.isDisconnected())
				{
					builderSubsystemID = getSubsytemId(builderSubsystemLink);
					//__COUTV__(builderSubsystemID);

					info_.subsystems[builderSubsystemID].id = builderSubsystemID;

					const std::string& builderSubsystemName =
					    builderSubsystemLink.getUIDAsString();
					//__COUTV__(builderSubsystemName);

					info_.subsystems[builderSubsystemID].label = builderSubsystemName;

					auto builderSubsystemDestinationLink = builderSubsystemLink.getNode(
					    colARTDAQSubsystem_.colLinkToDestination_);
					if(builderSubsystemDestinationLink.isDisconnected())
					{
						// default to no destination when no link
						info_.subsystems[builderSubsystemID].destination = 0;
					}
					else
					{
						// get destination subsystem id
						info_.subsystems[builderSubsystemID].destination =
						    getSubsytemId(builderSubsystemDestinationLink);
					}
					//__COUTV__(info_.subsystems[builderSubsystemID].destination);

					// add this subsystem to destination subsystem's sources, if not
					// there
					if(!info_.subsystems.count(
					       info_.subsystems[builderSubsystemID].destination) ||
					   !info_.subsystems[info_.subsystems[builderSubsystemID].destination]
					        .sources.count(builderSubsystemID))
					{
						info_.subsystems[info_.subsystems[builderSubsystemID].destination]
						    .sources.insert(builderSubsystemID);
					}

				}  // end subsystem instantiation

				__COUT__ << "Found Event Builder with UID " << builderUID
				         << ", on Hostname " << builderHost << ", in Subsystem "
				         << builderSubsystemID << __E__;
				info_.processes[ARTDAQAppType::EventBuilder].emplace_back(
				    builderUID,
				    builderHost,
				    builderAP,
				    builderSubsystemID,
				    ARTDAQAppType::EventBuilder,
				    builder.second.status());

				if(doWriteFHiCL)
				{
					std::string returnFcl, processName;
					bool        needToFlatten = true;
					bool        captureAsLastFcl =
					    builders
					        .size() &&  //init to true if multiple builders left to handle
					    (&builder != &builders.back());
					outputDataReceiverFHICL(builder.second,
					                        ARTDAQAppType::EventBuilder,
					                        maxFragmentSizeBytes,
					                        DEFAULT_ROUTING_TIMEOUT_MS,
					                        DEFAULT_ROUTING_RETRY_COUNT,
					                        captureAsLastFcl ? &returnFcl : nullptr);

					//Speed-up Philosophy:
					// flattenFHICL is expensive, so try to identify multinodes with fcl that only differ by process_name,
					//	i.e., ignore starting comments and process name, then compare fcl.
					//	Note: not much gain for any other node types but Event Builders, which tend to only differ by process_name in their fcl

					auto cmi = returnFcl.find(
					    "#	otsdaq-ARTDAQ builder UID:");  //find starting comments
					if(cmi != std::string::npos)
						cmi = returnFcl.find('\n', cmi);
					if(cmi != std::string::npos)
					{
						size_t pnj = std::string::npos;
						auto   pni =
						    returnFcl.find("\tprocess_name: ", cmi);  //find process name
						if(pni != std::string::npos)
						{
							pni += std::string("\tprocess_name: ")
							           .size();  //move past field name
							pnj = returnFcl.find('\n', pni);
						}
						if(pnj != std::string::npos)
						{
							processName = returnFcl.substr(pni, pnj - pni);
							__COUT__ << "Found process name = " << processName << __E__;

							bool sameFirst = false;
							//check before process name (ignoring comments)
							std::string newPiece = returnFcl.substr(cmi, pni - cmi);
							if(flattenedLastFclParts[0].size() &&
							   lastBuilderFcl[0].size() && lastBuilderFcl[0] == newPiece)
							{
								__COUT__ << "Same first fcl" << __E__;
								sameFirst = true;
							}
							else if(TTEST(20))
							{
								__COUTVS__(20, lastBuilderFcl[0]);
								__COUTVS__(20, newPiece);
								for(size_t i = 0, j = 0;
								    i < lastBuilderFcl[0].size() && j < newPiece.size();
								    ++i, ++j)
								{
									if(lastBuilderFcl[0][i] != newPiece[j])
									{
										__COUTVS__(20, i);
										__COUTVS__(20, j);
										__COUTVS__(20, lastBuilderFcl[0].substr(i, 30));
										__COUTVS__(20, newPiece.substr(j, 30));
										break;
									}
								}
							}
							if(captureAsLastFcl)  //if more, save piece
								lastBuilderFcl[0] = newPiece;

							//check after process name
							newPiece = returnFcl.substr(pnj);
							if(lastBuilderFcl[0].size() && lastBuilderFcl[1] == newPiece)
							{
								__COUT__ << "Same second fcl" << __E__;
								if(sameFirst)  //found opportunity for shortcut-to-flatten!
								{
									std::chrono::steady_clock::time_point startClock =
									    std::chrono::steady_clock::now();
									__COUT__ << "Found fcl match! Reuse for "
									         << builderUID << __E__;
									captureAsLastFcl =
									    false;  //do not overwrite current last fcl now!
									needToFlatten = false;

									//do rapid flatten here
									std::string outFile = getFlatFHICLFilename(
									    ARTDAQAppType::EventBuilder, builderUID);
									__COUTVS__(3, outFile);
									std::ofstream ofs{outFile};
									if(!ofs)
									{
										__SS__ << "Failed to open fhicl output file '"
										       << outFile << "!'" << __E__;
										__SS_THROW__;
									}
									ofs << flattenedLastFclParts[0] << "process_name: \""
									    << processName << "\""
									    << flattenedLastFclParts[1];
									__COUTT__
									    << builderUID << " Flatten Clock time = "
									    << artdaq::TimeUtils::GetElapsedTime(startClock)
									    << __E__;
									continue;  //done with shortcut-to-flatten
								}              //end shortcut-to-flatten handling
							}
							if(captureAsLastFcl)  //if interesting for more, save piece
								lastBuilderFcl[1] = newPiece;
						}
					}

					if(needToFlatten)
						ARTDAQTableBase::flattenFHICL(
						    ARTDAQAppType::EventBuilder,
						    builderUID,
						    captureAsLastFcl ? &returnFcl : nullptr);
					else
						__COUT__ << "Skipping full flatten for " << builderUID << __E__;

					//save parts without process name
					__COUTV__(captureAsLastFcl);
					if(captureAsLastFcl)
					{
						size_t pnj = std::string::npos;
						auto   pni = returnFcl.find("process_name:");  //find process name
						if(pni != std::string::npos)
						{
							//enforce white space before process name
							if(pni &&
							   (returnFcl[pni - 1] == ' ' || returnFcl[pni - 1] == '\n' ||
							    returnFcl[pni - 1] == '\t'))
								pnj = returnFcl.find('\n', pni);
						}
						if(pnj != std::string::npos)
						{
							__COUT__
							    << "Found flattened '"  //Note: returnFcl.substr(pni, pnj - pni) includes "process_name:"
							    << returnFcl.substr(pni, pnj - pni) << "' at pos " << pni
							    << " of " << returnFcl.size() << __E__;
							flattenedLastFclParts[0] = returnFcl.substr(0, pni);
							flattenedLastFclParts[1] = returnFcl.substr(pnj);
						}
						else
						{
							__COUT_WARN__ << "Failed to capture fcl for " << processName
							              << "!" << __E__;
						}
					}
				}  //end doWriteFHiCL
			}
			else  // disabled
			{
				__COUT__ << "Event Builder " << builderUID << " is disabled." << __E__;
			}
		}  // end builder loop
	}
	else
	{
		__COUT_WARN__ << "There should be at least one Event Builder!";
		//__SS_THROW__;
		// return;
	}
}  // end extractEventBuildersInfo()

//==============================================================================
void ARTDAQTableBase::extractDataLoggersInfo(ConfigurationTree artdaqSupervisorNode,
                                             bool              getStatusFalseNodes,
                                             bool              doWriteFHiCL,
                                             size_t            maxFragmentSizeBytes)
{
	__COUT__ << "Checking for Data Loggers..." << __E__;
	info_.processes[ARTDAQAppType::DataLogger].clear();

	ConfigurationTree dataloggersLink =
	    artdaqSupervisorNode.getNode(colARTDAQSupervisor_.colLinkToDataLoggers_);
	if(!dataloggersLink.isDisconnected())
	{
		std::vector<std::pair<std::string, ConfigurationTree>> dataloggers =
		    dataloggersLink.getChildren();

		for(auto& datalogger : dataloggers)
		{
			const std::string& loggerUID = datalogger.first;

			if(getStatusFalseNodes || datalogger.second.status())
			{
				std::string loggerHost =
				    datalogger.second.getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_HOSTNAME)
				        .getValueWithDefault("localhost");
				std::string loggerAP =
				    datalogger.second
				        .getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_ALLOWED_PROCESSORS)
				        .getValueWithDefault("");

				int               loggerSubsystemID = 1;
				ConfigurationTree loggerSubsystemLink =
				    datalogger.second.getNode(ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK);
				if(!loggerSubsystemLink.isDisconnected())
				{
					loggerSubsystemID = getSubsytemId(loggerSubsystemLink);
					//__COUTV__(loggerSubsystemID);
					info_.subsystems[loggerSubsystemID].id = loggerSubsystemID;

					const std::string& loggerSubsystemName =
					    loggerSubsystemLink.getUIDAsString();
					//__COUTV__(loggerSubsystemName);

					info_.subsystems[loggerSubsystemID].label = loggerSubsystemName;

					auto loggerSubsystemDestinationLink = loggerSubsystemLink.getNode(
					    colARTDAQSubsystem_.colLinkToDestination_);
					if(loggerSubsystemDestinationLink.isDisconnected())
					{
						// default to no destination when no link
						info_.subsystems[loggerSubsystemID].destination = 0;
					}
					else
					{
						// get destination subsystem id
						info_.subsystems[loggerSubsystemID].destination =
						    getSubsytemId(loggerSubsystemDestinationLink);
					}
					//__COUTV__(info_.subsystems[loggerSubsystemID].destination);

					// add this subsystem to destination subsystem's sources, if not
					// there
					if(!info_.subsystems.count(
					       info_.subsystems[loggerSubsystemID].destination) ||
					   !info_.subsystems[info_.subsystems[loggerSubsystemID].destination]
					        .sources.count(loggerSubsystemID))
					{
						info_.subsystems[info_.subsystems[loggerSubsystemID].destination]
						    .sources.insert(loggerSubsystemID);
					}

				}  // end subsystem instantiation

				__COUT__ << "Found Data Logger with UID " << loggerUID
				         << ", DAQInterface Hostname " << loggerHost << ", and Subsystem "
				         << loggerSubsystemID << __E__;
				info_.processes[ARTDAQAppType::DataLogger].emplace_back(
				    loggerUID,
				    loggerHost,
				    loggerAP,
				    loggerSubsystemID,
				    ARTDAQAppType::DataLogger,
				    datalogger.second.status());

				if(doWriteFHiCL)
				{
					outputDataReceiverFHICL(datalogger.second,
					                        ARTDAQAppType::DataLogger,
					                        maxFragmentSizeBytes);

					flattenFHICL(ARTDAQAppType::DataLogger, datalogger.second.getValue());
				}
			}
			else  // disabled
			{
				__COUT__ << "Data Logger " << loggerUID << " is disabled." << __E__;
			}
		}  // end logger loop
	}
	else
	{
		__COUT_WARN__ << "There were no Data Loggers found!";
	}
}  // end extractDataLoggersInfo()

//==============================================================================
void ARTDAQTableBase::extractDispatchersInfo(ConfigurationTree artdaqSupervisorNode,
                                             bool              getStatusFalseNodes,
                                             bool              doWriteFHiCL,
                                             size_t            maxFragmentSizeBytes)
{
	__COUT__ << "Checking for Dispatchers..." << __E__;
	info_.processes[ARTDAQAppType::Dispatcher].clear();

	ConfigurationTree dispatchersLink =
	    artdaqSupervisorNode.getNode(colARTDAQSupervisor_.colLinkToDispatchers_);
	if(!dispatchersLink.isDisconnected())
	{
		std::vector<std::pair<std::string, ConfigurationTree>> dispatchers =
		    dispatchersLink.getChildren();

		for(auto& dispatcher : dispatchers)
		{
			const std::string& dispatcherUID = dispatcher.first;

			if(getStatusFalseNodes || dispatcher.second.status())
			{
				std::string dispatcherHost =
				    dispatcher.second.getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_HOSTNAME)
				        .getValueWithDefault("localhost");
				std::string dispatcherAP =
				    dispatcher.second
				        .getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_ALLOWED_PROCESSORS)
				        .getValueWithDefault("");
				int dispatcherPort =
				    dispatcher.second.getNode("DispatcherPort").getValue<int>();

				auto              dispatcherSubsystemID = 1;
				ConfigurationTree dispatcherSubsystemLink =
				    dispatcher.second.getNode(ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK);
				if(!dispatcherSubsystemLink.isDisconnected())
				{
					dispatcherSubsystemID = getSubsytemId(dispatcherSubsystemLink);
					//__COUTV__(dispatcherSubsystemID);
					info_.subsystems[dispatcherSubsystemID].id = dispatcherSubsystemID;

					const std::string& dispatcherSubsystemName =
					    dispatcherSubsystemLink.getUIDAsString();
					//__COUTV__(dispatcherSubsystemName);

					info_.subsystems[dispatcherSubsystemID].label =
					    dispatcherSubsystemName;

					auto dispatcherSubsystemDestinationLink =
					    dispatcherSubsystemLink.getNode(
					        colARTDAQSubsystem_.colLinkToDestination_);
					if(dispatcherSubsystemDestinationLink.isDisconnected())
					{
						// default to no destination when no link
						info_.subsystems[dispatcherSubsystemID].destination = 0;
					}
					else
					{
						// get destination subsystem id
						info_.subsystems[dispatcherSubsystemID].destination =
						    getSubsytemId(dispatcherSubsystemDestinationLink);
					}
					//__COUTV__(info_.subsystems[dispatcherSubsystemID].destination);

					// add this subsystem to destination subsystem's sources, if not
					// there
					if(!info_.subsystems.count(
					       info_.subsystems[dispatcherSubsystemID].destination) ||
					   !info_
					        .subsystems[info_.subsystems[dispatcherSubsystemID]
					                        .destination]
					        .sources.count(dispatcherSubsystemID))
					{
						info_
						    .subsystems[info_.subsystems[dispatcherSubsystemID]
						                    .destination]
						    .sources.insert(dispatcherSubsystemID);
					}
				}

				__COUT__ << "Found Dispatcher with UID " << dispatcherUID
				         << ", DAQInterface Hostname " << dispatcherHost
				         << ", and Subsystem " << dispatcherSubsystemID << __E__;
				info_.processes[ARTDAQAppType::Dispatcher].emplace_back(
				    dispatcherUID,
				    dispatcherHost,
				    dispatcherAP,
				    dispatcherSubsystemID,
				    ARTDAQAppType::Dispatcher,
				    dispatcher.second.status(),
				    dispatcherPort);

				if(doWriteFHiCL)
				{
					outputDataReceiverFHICL(dispatcher.second,
					                        ARTDAQAppType::Dispatcher,
					                        maxFragmentSizeBytes);

					flattenFHICL(ARTDAQAppType::Dispatcher, dispatcher.second.getValue());
				}
			}
			else  // disabled
			{
				__COUT__ << "Dispatcher " << dispatcherUID << " is disabled." << __E__;
			}
		}  // end dispatcher loop
	}
	else
	{
		__COUT_WARN__ << "There were no Dispatchers found!";
	}
}  // end extractDispatchersInfo()

//==============================================================================
///	isARTDAQEnabled
bool ARTDAQTableBase::isARTDAQEnabled(const ConfigurationManager* cfgMgr)
{
	auto contexts =
	    cfgMgr->getNode(ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME).getChildren();
	for(auto context : contexts)
	{
		if(!context.second.isEnabled())
			continue;

		auto apps = context.second
		                .getNode(XDAQContextTable::colContext_.colLinkToApplicationTable_)
		                .getChildren();
		for(auto app : apps)
		{
			// __COUTV__(app.second.getNode(XDAQContextTable::colApplication_.colClass_).getValue());
			if(app.second.getNode(XDAQContextTable::colApplication_.colClass_)
			           .getValue() == ARTDAQ_SUPERVISOR_CLASS &&
			   app.second.isEnabled())
				return true;
		}
	}
	return false;
}  // end isARTDAQEnabled()

//==============================================================================
///	getARTDAQSystem
///
///		static function to retrive the active ARTDAQ system configuration.
///
///	Subsystem map to destination subsystem name.
///	Node properties: {status,hostname,subsystemName,(nodeArrString),(hostnameArrString),(hostnameFixedWidth)}
///	artdaqSupervisoInfo: {name, status, context address, context port}
///
const ARTDAQTableBase::ARTDAQInfo& ARTDAQTableBase::getARTDAQSystem(
    ConfigurationManagerRW* cfgMgr,
    std::map<std::string /*type*/,
             std::map<std::string /*record*/, std::vector<std::string /*property*/>>>&
        nodeTypeToObjectMap,
    std::map<std::string /*subsystemName*/, std::string /*destinationSubsystemName*/>&
                                           subsystemObjectMap,
    std::vector<std::string /*property*/>& artdaqSupervisoInfo)
{
	__COUT__ << "getARTDAQSystem()" << __E__;

	artdaqSupervisoInfo.clear();  // init

	const XDAQContextTable* contextTable = cfgMgr->__GET_CONFIG__(XDAQContextTable);

	// for each artdaq context, output all artdaq apps

	const XDAQContextTable::XDAQContext* artdaqContext =
	    contextTable->getTheARTDAQSupervisorContext();

	// return empty info
	if(!artdaqContext)
		return ARTDAQTableBase::info_;

	__COUTV__(artdaqContext->contextUID_);
	__COUTV__(artdaqContext->applications_.size());

	// load artdaq node layout as multi-node printer-syntax guide
	std::map<std::string /*type*/, std::set<std::string /*node-names*/>> nodeLayoutNames;
	{  //copied from handleLoadArtdaqNodeLayoutXML() at otsdaq-utilities/otsdaq-utilities/ConfigurationGUI/ConfigurationGUISupervisor.cc:8054
		const std::string& finalContextGroupName =
		    cfgMgr->getActiveGroupName(ConfigurationManager::GroupType::CONTEXT_TYPE);
		const TableGroupKey& finalContextGroupKey =
		    cfgMgr->getActiveGroupKey(ConfigurationManager::GroupType::CONTEXT_TYPE);
		const std::string& finalConfigGroupName = cfgMgr->getActiveGroupName(
		    ConfigurationManager::GroupType::CONFIGURATION_TYPE);
		const TableGroupKey& finalConfigGroupKey = cfgMgr->getActiveGroupKey(
		    ConfigurationManager::GroupType::CONFIGURATION_TYPE);

		FILE* fp = nullptr;
		//first try context+config name only
		{
			std::stringstream layoutPath;
			layoutPath << ARTDAQTableBase::ARTDAQ_CONFIG_LAYOUTS_PATH
			           << finalContextGroupName << "_" << finalContextGroupKey << "."
			           << finalConfigGroupName << "_" << finalConfigGroupKey << ".dat";

			fp = fopen(layoutPath.str().c_str(), "r");
			if(!fp)
			{
				__COUT__ << "Layout file not found for '" << finalContextGroupName << "("
				         << finalContextGroupKey << ") + " << finalConfigGroupName << "("
				         << finalConfigGroupKey << ")': " << layoutPath.str() << __E__;
				// return; //try context only!
			}
			else
				__COUTV__(layoutPath.str());
		}
		//last try context name only
		{
			std::stringstream layoutPath;
			layoutPath << ARTDAQTableBase::ARTDAQ_CONFIG_LAYOUTS_PATH
			           << finalContextGroupName << "_" << finalContextGroupKey << ".dat";
			__COUTV__(layoutPath.str());

			fp = fopen(layoutPath.str().c_str(), "r");
			if(!fp)
			{
				__COUT__ << "Layout file not found for '" << finalContextGroupName << "("
				         << finalContextGroupKey << ")': " << layoutPath.str() << __E__;
			}
			else
				__COUTV__(layoutPath.str());
		}

		if(!fp)  //since exact context name was not found, see if there is a best match layout file
		{
			DIR*           pDIR;
			struct dirent* entry;
			bool           isDir;
			std::string    name;
			int            type;

			float       bestScore = 0, score;  //high score wins
			std::string bestName  = "";

			if(!(pDIR = opendir((ARTDAQTableBase::ARTDAQ_CONFIG_LAYOUTS_PATH).c_str())))
			{
				__SS__ << "Path '" << ARTDAQTableBase::ARTDAQ_CONFIG_LAYOUTS_PATH
				       << "' could not be opened!" << __E__;
				__SS_THROW__;
			}

			// else directory good, get all folders, .h, .cc, .txt files
			while((entry = readdir(pDIR)))
			{
				name = std::string(entry->d_name);
				type = int(entry->d_type);

				__COUTS__(2) << type << " " << name << "\n" << std::endl;

				if(name[0] != '.' &&
				   (type == 0 ||  // 0 == UNKNOWN (which can happen - seen in SL7 VM)
				    type == 4 ||  // directory type
				    type == 8 ||  // file type
				    type ==
				        10  // 10 == link (could be directory or file, treat as unknown)
				    ))
				{
					isDir = false;

					if(type == 0 || type == 10)
					{
						// unknown type .. determine if directory
						DIR* pTmpDIR = opendir(
						    (ARTDAQTableBase::ARTDAQ_CONFIG_LAYOUTS_PATH + "/" + name)
						        .c_str());
						if(pTmpDIR)
						{
							isDir = true;
							closedir(pTmpDIR);
						}
						else  //assume file
							__COUTS__(2) << "Unable to open path as directory: "
							             << (ARTDAQTableBase::ARTDAQ_CONFIG_LAYOUTS_PATH +
							                 "/" + name)
							             << __E__;
					}

					if(type == 4)
						isDir = true;  // flag directory types

					// handle directories and files

					if(isDir)
					{
						__COUTS__(2) << "Directory: " << type << " " << name << __E__;
					}
					else
					{
						__COUTS__(2) << "File: " << type << " " << name << "\n"
						             << std::endl;
						if(name.find(".dat") !=
						   name.size() - 4)  //skip if not proper file extension
							continue;

						__COUTS__(2) << "Contender: " << name << "\n" << std::endl;
						score = 0;  //reset for score calc

						auto nameSplit = StringMacros::getVectorFromString(name, {'.'});

						if(nameSplit.size() > 1)  //include config group in score
						{
							//match key to right of decimal and name to left of decimal
							auto keyi = nameSplit[1].rfind('_');
							if(keyi != std::string::npos)
							{
								int key =
								    atoi(nameSplit[1]
								             .substr(keyi, nameSplit[1].size() - 4 - keyi)
								             .c_str());
								__COUTVS__(2, key);
								float tmpscore =
								    finalConfigGroupKey.key() -
								    key;  //will be negative if comparing to newer key
								if(tmpscore < 0)
									tmpscore =
									    -1 * tmpscore -
									    1;  //give penalty for newer keys (favor older keys)
								__COUTVS__(2, tmpscore);
								tmpscore =
								    1.0 /
								    tmpscore;  //make high score be closest, and put value in decimal
								__COUTVS__(2, tmpscore);

								//now for each matching letter +1, for matching size +3
								std::string nameToCompare = nameSplit[1].substr(0, keyi);
								__COUTVS__(2, nameToCompare);
								size_t i = 0, j = 0;
								//match with both strings driving in case of jumps in the words
								for(; i < nameToCompare.size() &&
								      j < finalConfigGroupName.size();
								    ++i)
								{
									if(nameToCompare[i] == finalConfigGroupName[j])
									{
										tmpscore += 1.0;
										++j;
									}
								}
								__COUTVS__(2, tmpscore);
								i = 0, j = 0;
								for(; i < nameToCompare.size() &&
								      j < finalConfigGroupName.size();
								    ++j)
								{
									if(nameToCompare[i] == finalConfigGroupName[j])
									{
										tmpscore += 1.0;
										++i;
									}
								}
								__COUTVS__(2, tmpscore);
								score += tmpscore;
							}
							__COUTVS__(2, score);
						}                         //end config group score calc
						if(nameSplit.size() > 0)  //include context group in score
						{
							//match key to right of decimal and name to left of decimal
							auto keyi = nameSplit[0].rfind('_');
							if(keyi != std::string::npos)
							{
								int key =
								    atoi(nameSplit[0]
								             .substr(keyi, nameSplit[0].size() - 4 - keyi)
								             .c_str());
								__COUTVS__(2, key);
								float tmpscore =
								    finalContextGroupKey.key() -
								    key;  //will be negative if comparing to newer key
								if(tmpscore < 0)
									tmpscore =
									    -1 * tmpscore -
									    1;  //give penalty for newer keys (favor older keys)
								__COUTVS__(2, tmpscore);
								tmpscore =
								    1.0 /
								    tmpscore;  //make high score be closest, and put value in decimal
								__COUTVS__(2, tmpscore);

								//now for each matching letter +1, for matching size +3
								std::string nameToCompare = nameSplit[0].substr(0, keyi);
								__COUTVS__(2, nameToCompare);
								size_t i = 0, j = 0;
								//match with both strings driving in case of jumps in the words
								for(; i < nameToCompare.size() &&
								      j < finalContextGroupName.size();
								    ++i)
								{
									if(nameToCompare[i] == finalContextGroupName[j])
									{
										tmpscore += 1.0;
										++j;
									}
								}
								__COUTVS__(2, tmpscore);
								i = 0, j = 0;
								for(; i < nameToCompare.size() &&
								      j < finalContextGroupName.size();
								    ++j)
								{
									if(nameToCompare[i] == finalContextGroupName[j])
									{
										tmpscore += 1.0;
										++i;
									}
								}
								__COUTVS__(2, tmpscore);
								score += tmpscore;
							}
							__COUTVS__(2, score);
						}  //end context group score calc

						if(score > bestScore)
						{
							bestScore = score;
							bestName  = name;
							__COUTVS__(2, bestName);
						}
					}  //end score handling
				}      //end file handling
			}          //end directory search loop for best layout file

			if(bestName != "")
			{
				__COUT__ << "Found closest layout file name: " << bestName << ".dat"
				         << __E__;
				std::stringstream layoutPath;
				layoutPath << ARTDAQTableBase::ARTDAQ_CONFIG_LAYOUTS_PATH << bestName
				           << ".dat";
				__COUTV__(layoutPath.str());
				fp = fopen(layoutPath.str().c_str(), "r");
				if(!fp)
				{
					__COUT__ << "Closest layout file not found for '" << bestName << "'"
					         << __E__;
				}
			}

			//if(!fp) just ignore that file does not exist, and generate printer syntax from 1st principles
		}  //end no layout file handling

		if(fp)  //else if(!fp) just ignore that file does not exist, and generate printer syntax from 1st principles
		{
			__COUT__ << "Extract info from layout file.." << __E__;

			// file format is line by line
			// line 0 -- grid: <rows> <cols>
			// line 1-N -- node: <type> <name> <x-grid> <y-grid>

			const size_t maxLineSz = 1000;
			char         line[maxLineSz];
			if(!fgets(line, maxLineSz, fp))
			{
				fclose(fp);
				__COUT__ << "No layout naming info found." << __E__;
			}
			else
			{
				// ignore grid hint and extract grid

				char         name[maxLineSz];
				char         type[maxLineSz];
				unsigned int x, y;
				while(fgets(line, maxLineSz, fp))
				{
					// extract node
					sscanf(line, "%s %s %u %u", type, name, &x, &y);
					nodeLayoutNames[type].emplace(name);
				}  // end node extraction loop

				fclose(fp);
			}

			__COUTTV__(StringMacros::mapToString(nodeLayoutNames));
		}
	}  //end load node layout helper guide

	//Strategy:
	//	- Check for a count that match layout names
	//		-- if any match, then keep those matching with that node layout name
	//		-- otherwise allow auto-deduction of multinodes

	for(auto& artdaqApp : artdaqContext->applications_)
	{
		if(artdaqApp.class_ != ARTDAQ_SUPERVISOR_CLASS)
			continue;

		__COUTV__(artdaqApp.applicationUID_);
		artdaqSupervisoInfo.push_back(artdaqApp.applicationUID_);
		artdaqSupervisoInfo.push_back(
		    (artdaqContext->status_ && artdaqApp.status_) ? "1" : "0");
		artdaqSupervisoInfo.push_back(artdaqContext->address_);
		artdaqSupervisoInfo.push_back(std::to_string(artdaqContext->port_));

		const ARTDAQTableBase::ARTDAQInfo& info = ARTDAQTableBase::extractARTDAQInfo(
		    XDAQContextTable::getSupervisorConfigNode(/*artdaqSupervisorNode*/
		                                              cfgMgr,
		                                              artdaqContext->contextUID_,
		                                              artdaqApp.applicationUID_),
		    true /*getStatusFalseNodes*/);

		__COUT__ << "========== "
		         << "Found " << info.subsystems.size() << " subsystems." << __E__;

		// build subsystem desintation map
		for(auto& subsystem : info.subsystems)
			subsystemObjectMap.emplace(std::make_pair(
			    subsystem.second.label, std::to_string(subsystem.second.destination)));

		__COUT__ << "========== "
		         << "Found " << info.processes.size() << " process types." << __E__;

		for(auto& nameTypePair : ARTDAQTableBase::processTypes_.mapToType_)
		{
			const std::string& typeString = nameTypePair.first;
			__COUTV__(typeString);

			nodeTypeToObjectMap.emplace(
			    std::make_pair(typeString,
			                   std::map<std::string /*record*/,
			                            std::vector<std::string /*property*/>>()));

			auto it = info.processes.find(nameTypePair.second);
			if(it == info.processes.end())
			{
				__COUT__ << "\t"
				         << "Found 0 " << typeString << __E__;
				continue;
			}
			__COUT__ << "\t"
			         << "Found " << it->second.size() << " " << typeString << "(s)"
			         << __E__;

			auto tableIt = processTypes_.mapToTable_.find(typeString);
			if(tableIt == processTypes_.mapToTable_.end())
			{
				__SS__ << "Invalid artdaq node type '" << typeString << "' attempted!"
				       << __E__;
				__SS_THROW__;
			}
			__COUTV__(tableIt->second);

			{
				std::stringstream ss;
				cfgMgr->getTableByName(tableIt->second)->getView().print(ss);
				__COUT_MULTI__(1, ss.str());
			}

			auto allNodes = cfgMgr->getNode(tableIt->second).getChildren();

			std::set<
			    std::
			        string /* encodeURI nodeName */>  //use StringMacros::encodeURIComponent because dashes will confuse printer syntax later!
			    skipSet;  // use to skip nodes when constructing multi-nodes

			const std::set<std::string /*colName*/> skipColumns(
			    {ARTDAQ_TYPE_TABLE_HOSTNAME,
			     ARTDAQ_TYPE_TABLE_ALLOWED_PROCESSORS,
			     ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK,
			     colARTDAQReader_
			         .colDaqFragmentIDs_,  //for board readers, skip the 'unique' fragment IDs when considering for multinode
			     TableViewColumnInfo::COL_NAME_COMMENT,
			     TableViewColumnInfo::COL_NAME_AUTHOR,
			     TableViewColumnInfo::
			         COL_NAME_CREATION});  // note: also skip UID and Status

			if(TTEST(1) && nodeLayoutNames.find(typeString) != nodeLayoutNames.end())
			{
				__COUTTV__(StringMacros::setToString(nodeLayoutNames.at(typeString)));
			}

			// loop through all nodes of this type
			for(auto& artdaqNode : it->second)
			{
				// check skip set
				if(skipSet.find(StringMacros::encodeURIComponent(artdaqNode.label)) !=
				   skipSet.end())
					continue;

				__COUT__ << "\t\t"
				         << "Found '" << artdaqNode.label << "' " << typeString << __E__;

				std::string nodeName    = artdaqNode.label;
				bool        status      = artdaqNode.status;
				std::string hostname    = artdaqNode.hostname;
				std::string subsystemId = std::to_string(artdaqNode.subsystem);
				std::string subsystemName =
				    info.subsystems.at(artdaqNode.subsystem).label;

				ConfigurationTree thisNode =
				    cfgMgr->getNode(tableIt->second).getNode(nodeName);
				auto thisNodeColumns = thisNode.getChildren();

				// check for multi-node
				//	Steps:
				//		- search for other records to include with same values/links except hostname/name
				//		- if match to layout nodes, then maintain layout template

				std::vector<std::string> multiNodeNames, hostnameArray;
				// unsigned int             hostnameFixedWidth = 0;

				skipSet
				    .emplace(  //emplace self into skipset since this node is handled now (and should not be considered in future multinode instances)
				        StringMacros::encodeURIComponent(nodeName));

				__COUTV__(allNodes.size());
				for(auto& otherNode : allNodes)  // start multi-node search loop
				{
					if(skipSet.find(StringMacros::encodeURIComponent(otherNode.first)) !=
					       skipSet.end() ||
					   otherNode.second.status() != status)  // skip if status mismatch
						continue;  // skip unless 'other' and not in skip set

					//__COUTV__(subsystemName);
					//__COUTV__(otherNode.second.getNode(ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK_UID).getValue());

					if(subsystemName ==
					   otherNode.second.getNode(ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK_UID)
					       .getValue())
					{
						// possible multi-node situation
						//__COUT__ << "Checking for multi-node..." << __E__;

						//__COUTV__(thisNode.getNodeRow());
						//__COUTV__(otherNode.second.getNodeRow());

						auto otherNodeColumns = otherNode.second.getChildren();

						bool isMultiNode = true;
						for(unsigned int i = 0;
						    i < thisNodeColumns.size() && i < otherNodeColumns.size();
						    ++i)
						{
							// skip columns that do not need to be checked for multi-node consideration
							if(skipColumns.find(thisNodeColumns[i].first) !=
							       skipColumns.end() ||
							   thisNodeColumns[i].second.isLinkNode())
								continue;

							// at this point must match for multinode

							//__COUTV__(thisNodeColumns[i].first);
							//__COUTV__(otherNodeColumns[i].first);

							//__COUTV__(thisNodeColumns[i].second.getValue());
							//__COUTV__(otherNodeColumns[i].second.getValue());

							if(thisNodeColumns[i].second.getValue() !=
							   otherNodeColumns[i].second.getValue())
							{
								__COUT__ << "Mismatch, not multi-node member." << __E__;
								isMultiNode = false;
								break;
							}
						}

						if(isMultiNode)
						{
							__COUT__ << "Found '" << nodeName
							         << "' multi-node member candidate '"
							         << otherNode.first << "'" << __E__;

							//use StringMacros::encodeURIComponent because dashes will confuse printer syntax later!
							if(!multiNodeNames.size())  // add this node first!
							{
								multiNodeNames.push_back(
								    StringMacros::encodeURIComponent(nodeName));
								hostnameArray.push_back(
								    StringMacros::encodeURIComponent(hostname));
							}
							multiNodeNames.push_back(
							    StringMacros::encodeURIComponent(otherNode.first));
							hostnameArray.push_back(StringMacros::encodeURIComponent(
							    otherNode.second.getNode(ARTDAQ_TYPE_TABLE_HOSTNAME)
							        .getValue()));

							__COUTV__(hostnameArray.back());
							skipSet.emplace(
							    StringMacros::encodeURIComponent(otherNode.first));
						}
					}
				}  // end loop to search for multi-node members

				unsigned int nodeFixedWildcardLength = 0, hostFixedWildcardLength = 0;
				std::string  multiNodeString = "", hostArrayString = "";

				__COUTV__(nodeName);

				if(multiNodeNames.size() > 1)
				{
					__COUT__ << "Handling multi-node printer syntax" << __E__;

					__COUTTV__(StringMacros::vectorToString(multiNodeNames));
					__COUTTV__(StringMacros::vectorToString(hostnameArray));
					__COUTTV__(StringMacros::setToString(skipSet));

					//		- if match to layout nodes, then maintain layout template, and trim outliers from skipset
					if(nodeLayoutNames.find(typeString) != nodeLayoutNames.end())
					{
						__COUTTV__(
						    StringMacros::setToString(nodeLayoutNames.at(typeString)));

						// Strategy to use node layout as guide:
						//	- do two passes
						//	- find best node layout name based on narrowest match (i.e. smallest match count)
						std::string bestNodeLayoutName = "";
						size_t      bestNodeLayoutMatchCount =
						    multiNodeNames.size() + 1;  //init to 'infinite'
						//first pass
						for(const auto& layoutNameFull : nodeLayoutNames.at(typeString))
						{
							__COUTTV__(layoutNameFull);
							size_t      statusPos  = layoutNameFull.find(";status=");
							std::string layoutName = layoutNameFull.substr(0, statusPos);
							bool        layoutStatus = true;
							if(statusPos ==
							   std::string::
							       npos)  //not specified in layout, so take this status and hope!
								layoutStatus = status;
							else if("0" ==
							        layoutNameFull.substr(statusPos +
							                              std::string(";status=").size()))
								layoutStatus = false;

							__COUTTV__(layoutStatus);

							if(layoutStatus != status)
							{
								__COUTT__ << "Status mismatch for template" << __E__;
								break;
							}

							auto layoutSplit =
							    StringMacros::getVectorFromString(layoutName, {'*'});
							__COUTTV__(StringMacros::vectorToString(layoutSplit));

							bool   exactMatch = true;
							size_t pos        = 0;
							for(const auto& layoutSeg : layoutSplit)
								if((pos = nodeName.find(layoutSeg, pos)) ==
								   std::string::npos)
								{
									__COUTT__ << "Did not find '" << layoutSeg << "' in '"
									          << nodeName << "'" << __E__;
									exactMatch = false;
									break;
								}

							__COUTTV__(exactMatch);
							if(exactMatch)
							{
								size_t nodeLayoutMatchCount = 1;

								__COUT__ << "Found layout template name match! '"
								         << layoutName << "' for node '" << nodeName
								         << ".' Trimming multinode candidates to match..."
								         << __E__;

								for(unsigned int i = 1; i < multiNodeNames.size(); ++i)
								{
									__COUTTV__(multiNodeNames[i]);
									std::string multiNodeName =
									    StringMacros::decodeURIComponent(
									        multiNodeNames[i]);
									__COUTTV__(multiNodeName);
									bool   exactMatch = true;
									size_t pos        = 0;
									for(const auto& layoutSeg : layoutSplit)
										if((pos = multiNodeName.find(layoutSeg, pos)) ==
										   std::string::npos)
										{
											__COUTT__ << "Did not find '" << layoutSeg
											          << "' in '" << multiNodeName << "'"
											          << __E__;
											exactMatch = false;
											break;
										}

									if(exactMatch)
									{
										++nodeLayoutMatchCount;
										__COUTT__ << "Found '" << layoutName << "' in '"
										          << multiNodeName << "'" << __E__;
									}

								}  //end loop to trim multinode candidates

								__COUTTV__(nodeLayoutMatchCount);
								if(nodeLayoutMatchCount < bestNodeLayoutMatchCount)
								{
									bestNodeLayoutName       = layoutNameFull;
									bestNodeLayoutMatchCount = nodeLayoutMatchCount;
									__COUTTV__(bestNodeLayoutName);
									__COUTTV__(bestNodeLayoutMatchCount);
								}
							}
						}  //end first loop to find best layout node

						__COUTV__(nodeName);
						__COUTV__(StringMacros::vectorToString(multiNodeNames));
						__COUTV__(StringMacros::vectorToString(hostnameArray));
						__COUTV__(StringMacros::setToString(skipSet));

						//second pass, remove from skipSet
						if(bestNodeLayoutMatchCount > 0)
						{
							__COUTV__(bestNodeLayoutName);
							std::string layoutNameFull = bestNodeLayoutName;
							__COUTTV__(layoutNameFull);
							size_t      statusPos  = layoutNameFull.find(";status=");
							std::string layoutName = layoutNameFull.substr(0, statusPos);

							auto layoutSplit =
							    StringMacros::getVectorFromString(layoutName, {'*'});
							__COUTTV__(StringMacros::vectorToString(layoutSplit));

							__COUT__ << "Found layout template name match! '"
							         << layoutName << "' for node '" << nodeName
							         << ".' Trimming multinode candidates to match..."
							         << __E__;

							for(unsigned int i = 1; i < multiNodeNames.size(); ++i)
							{
								__COUTTV__(multiNodeNames[i]);
								std::string multiNodeName =
								    StringMacros::decodeURIComponent(multiNodeNames[i]);
								__COUTTV__(multiNodeName);
								bool   exactMatch = true;
								size_t pos        = 0;
								for(const auto& layoutSeg : layoutSplit)
									if((pos = multiNodeName.find(layoutSeg, pos)) ==
									   std::string::npos)
									{
										__COUTT__ << "Did not find '" << layoutSeg
										          << "' in '" << multiNodeName << "'"
										          << __E__;
										exactMatch = false;
										break;
									}

								if(!exactMatch)
								{
									__COUT__ << "Trimming multinode candidate '"
									         << multiNodeName << "'" << __E__;
									skipSet.erase(multiNodeNames[i]);
									multiNodeNames.erase(multiNodeNames.begin() + i);
									hostnameArray.erase(hostnameArray.begin() + i);
									--i;  //rewind for multiNodeNames[i] erase
								}
							}  //end loop to trim multinode candidates
						}      //end applying layout template name rule
					}          //end match to layout name templates

					__COUTV__(nodeName);
					__COUTV__(StringMacros::vectorToString(multiNodeNames));
					__COUTV__(StringMacros::vectorToString(hostnameArray));
					__COUTV__(StringMacros::setToString(skipSet));

					std::vector<std::string>
					    trimmedNodeNames;  // track trimmed nodes for collision check
					{
						// check for alpha-based similarity groupings (ignore numbers and special characters)
						unsigned int              maxScore = 0;
						unsigned int              score;
						unsigned int              minScore = -1;
						std::vector<unsigned int> scoreVector;
						scoreVector.push_back(-1);  // for 0 index (it's perfect)
						for(unsigned int i = 1; i < multiNodeNames.size(); ++i)
						{
							score = 0;

							__COUTS__(3) << multiNodeNames[0] << " vs "
							             << multiNodeNames[i] << __E__;

							// start forward score loop
							for(unsigned int j = 0, k = 0; j < multiNodeNames[0].size() &&
							                               k < multiNodeNames[i].size();
							    ++j, ++k)
							{
								while(j < multiNodeNames[0].size() &&
								      !(multiNodeNames[0][j] >= 'a' &&
								        multiNodeNames[0][j] <= 'z') &&
								      !(multiNodeNames[0][j] >= 'A' &&
								        multiNodeNames[0][j] <= 'Z'))
									++j;  // skip non-alpha characters
								while(k < multiNodeNames[i].size() &&
								      !(multiNodeNames[i][k] >= 'a' &&
								        multiNodeNames[i][k] <= 'z') &&
								      !(multiNodeNames[i][k] >= 'A' &&
								        multiNodeNames[i][k] <= 'Z'))
									++k;  // skip non-alpha characters

								while(k < multiNodeNames[i].size() &&
								      multiNodeNames[0][j] != multiNodeNames[i][k])
									++k;  // skip non-matching alpha characters

								__COUTS__(3)
								    << j << "-" << k << " of " << multiNodeNames[0].size()
								    << "-" << multiNodeNames[i].size() << __E__;

								if(j < multiNodeNames[0].size() &&
								   k < multiNodeNames[i].size())
									++score;  // found a matching letter!
							}                 // end forward score loop

							__COUTVS__(3, score);

							// start backward score loop
							for(unsigned int j = multiNodeNames[0].size() - 1,
							                 k = multiNodeNames[i].size() - 1;
							    j < multiNodeNames[0].size() &&
							    k < multiNodeNames[i].size();
							    --j, --k)
							{
								while(j < multiNodeNames[0].size() &&
								      !(multiNodeNames[0][j] >= 'a' &&
								        multiNodeNames[0][j] <= 'z') &&
								      !(multiNodeNames[0][j] >= 'A' &&
								        multiNodeNames[0][j] <= 'Z'))
									--j;  // skip non-alpha characters
								while(k < multiNodeNames[i].size() &&
								      !(multiNodeNames[i][k] >= 'a' &&
								        multiNodeNames[i][k] <= 'z') &&
								      !(multiNodeNames[i][k] >= 'A' &&
								        multiNodeNames[i][k] <= 'Z'))
									--k;  // skip non-alpha characters

								while(k < multiNodeNames[i].size() &&
								      multiNodeNames[0][j] != multiNodeNames[i][k])
									--k;  // skip non-matching alpha characters

								__COUTS__(3) << "BACK" << j << "-" << k << " of "
								             << multiNodeNames[0].size() << "-"
								             << multiNodeNames[i].size() << __E__;

								if(j < multiNodeNames[0].size() &&
								   k < multiNodeNames[i].size())
									++score;  // found a matching letter!
							}                 // end backward score loop

							__COUTVS__(3, score / 2.0);

							scoreVector.push_back(score);

							if(score > maxScore)
							{
								maxScore = score;
							}

							if(score < minScore)
							{
								minScore = score;
							}

						}  // end multi-node member scoring loop

						__COUTVS__(2, minScore);
						__COUTVS__(2, maxScore);

						__COUT__ << "Trimming multi-node members with low match score..."
						         << __E__;

						// go backwards, to not mess up indices as deleted
						//	do not delete index 0
						for(unsigned int i = multiNodeNames.size() - 1;
						    i > 0 && i < multiNodeNames.size();
						    --i)
						{
							//__COUTV__(scoreVector[i]);
							//__COUTV__(i);
							if(maxScore > multiNodeNames[0].size() &&
							   scoreVector[i] >= maxScore)
								continue;

							// else trim
							__COUT__ << "Trimming low score match " << multiNodeNames[i]
							         << " for node name " << nodeName << __E__;

							trimmedNodeNames.push_back(multiNodeNames[i]);
							skipSet.erase(multiNodeNames[i]);
							multiNodeNames.erase(multiNodeNames.begin() + i);
							hostnameArray.erase(hostnameArray.begin() + i);

						}  // end multi-node trim loop

					}  // done with multi-node member trim

					// Collision check: verify that the computed nodeName pattern
					//	does not also match trimmed nodes or existing map entries.
					//	If it does, re-score with full character matching and trim
					//	again until the pattern only matches the current node set.
					//	If trimming can't resolve the collision, abandon multi-node
					//	grouping so remaining members get processed individually.
					if(multiNodeNames.size() > 1)
					{
						// Lambda to check if a name matches the commonChunks pattern
						//	(i.e. all non-empty chunks appear in order, first chunk at position 0)
						auto matchesCommonChunksPattern =
						    [](const std::string&              name,
						       const std::vector<std::string>& chunks) -> bool {
							size_t pos = 0;
							for(unsigned int c = 0; c < chunks.size(); ++c)
							{
								if(chunks[c].empty())
									continue;
								size_t found;
								if(c == 0)
									found = (name.size() >= chunks[c].size() &&
									         name.compare(
									             0, chunks[c].size(), chunks[c]) == 0)
									            ? 0
									            : std::string::npos;
								else
									found = name.find(chunks[c], pos);
								if(found == std::string::npos)
									return false;
								pos = found + chunks[c].size();
							}
							return true;
						};

						bool collisionRetry = true;
						while(collisionRetry && multiNodeNames.size() > 1)
						{
							collisionRetry = false;

							// Trial extraction to get the current commonChunks pattern
							std::vector<std::string> trialCommonChunks;
							std::vector<std::string> trialWildcards;
							unsigned int             trialFixedLen = 0;
							StringMacros::extractCommonChunks(multiNodeNames,
							                                  trialCommonChunks,
							                                  trialWildcards,
							                                  trialFixedLen);

							__COUT__ << "Collision check: trialCommonChunks = "
							         << StringMacros::vectorToString(trialCommonChunks)
							         << __E__;

							// Check 1: trimmed nodes collision
							bool collisionFound = false;
							for(const auto& trimmedNode : trimmedNodeNames)
							{
								if(matchesCommonChunksPattern(trimmedNode,
								                              trialCommonChunks))
								{
									__COUT__ << "Collision detected: trimmed node '"
									         << trimmedNode
									         << "' matches base pattern from commonChunks"
									         << __E__;
									collisionFound = true;
									break;
								}
							}

							// Check 2: existing map entries collision
							if(!collisionFound)
							{
								for(const auto& existingEntry :
								    nodeTypeToObjectMap.at(typeString))
								{
									std::string existingBaseName = existingEntry.first;
									size_t statusPos = existingBaseName.find(";status=");
									if(statusPos != std::string::npos)
										existingBaseName =
										    existingBaseName.substr(0, statusPos);

									if(matchesCommonChunksPattern(existingBaseName,
									                              trialCommonChunks))
									{
										__COUT__
										    << "Collision detected: existing map entry '"
										    << existingEntry.first
										    << "' matches base pattern from commonChunks"
										    << __E__;
										collisionFound = true;
										break;
									}
								}
							}

							if(!collisionFound)
								break;  // no collision, we're done

							// Collision found! Re-score with full character matching
							__COUT__ << "Re-scoring remaining multi-node members with "
							            "full character matching to resolve collision..."
							         << __E__;

							unsigned int              fullMaxScore = 0;
							std::vector<unsigned int> fullScoreVector;
							fullScoreVector.push_back(-1);  // index 0 is perfect (self)

							for(unsigned int i = 1; i < multiNodeNames.size(); ++i)
							{
								unsigned int fscore = 0;
								// Simple forward character-by-character matching
								for(unsigned int j = 0; j < multiNodeNames[0].size() &&
								                        j < multiNodeNames[i].size();
								    ++j)
								{
									if(multiNodeNames[0][j] == multiNodeNames[i][j])
										++fscore;
									else
										break;
								}
								fullScoreVector.push_back(fscore);
								if(fscore > fullMaxScore)
									fullMaxScore = fscore;
							}

							__COUT__ << "Full char rescore: maxScore = " << fullMaxScore
							         << __E__;

							// Trim nodes below max score
							bool anyTrimmed = false;
							for(unsigned int i = multiNodeNames.size() - 1;
							    i > 0 && i < multiNodeNames.size();
							    --i)
							{
								if(fullScoreVector[i] >= fullMaxScore)
									continue;

								__COUT__ << "Collision trim: removing "
								         << multiNodeNames[i] << " (score "
								         << fullScoreVector[i] << " < " << fullMaxScore
								         << ") for node name " << nodeName << __E__;

								trimmedNodeNames.push_back(multiNodeNames[i]);
								skipSet.erase(multiNodeNames[i]);
								multiNodeNames.erase(multiNodeNames.begin() + i);
								hostnameArray.erase(hostnameArray.begin() + i);
								anyTrimmed     = true;
								collisionRetry = true;
							}

							// If no trimming was possible but still colliding,
							//	abandon multi-node grouping - remaining members
							//	will be processed individually by the main loop
							if(!anyTrimmed)
							{
								__COUT__ << "Cannot narrow multi-node group further to "
								            "resolve collision. Abandoning multi-node "
								            "grouping for '"
								         << nodeName
								         << "' - remaining members will be "
								            "processed individually."
								         << __E__;

								// Remove all members except [0] from skipSet
								for(unsigned int i = 1; i < multiNodeNames.size(); ++i)
									skipSet.erase(multiNodeNames[i]);
								multiNodeNames.resize(1);
								hostnameArray.resize(1);
								break;
							}
						}  // end collision resolution loop

						__COUT__ << "After collision resolution:" << __E__;
						__COUTV__(nodeName);
						__COUTV__(StringMacros::vectorToString(multiNodeNames));
						__COUTV__(StringMacros::vectorToString(hostnameArray));
						__COUTV__(StringMacros::setToString(skipSet));
					}  // end collision check

					__COUTV__(nodeName);
					__COUTV__(StringMacros::vectorToString(multiNodeNames));
					__COUTV__(StringMacros::vectorToString(hostnameArray));
					__COUTV__(StringMacros::setToString(skipSet));

					//set of names fully defined, reorder alphabettically
					{
						__COUT__ << "Reorganizing multinode '" << nodeName
						         << "' alphabetically..." << __E__;
						std::set<
						    std::pair<std::string /* node */, std::string /* host */>>
						    reorderSet;
						for(unsigned int i = 0; i < multiNodeNames.size(); ++i)
							reorderSet.emplace(
							    std::make_pair(multiNodeNames[i], hostnameArray[i]));

						__COUTV__(StringMacros::setToString(reorderSet));
						//skipset is unchanged, multiNodeNames and hostnameArray are reordered

						multiNodeNames.clear();
						hostnameArray.clear();
						for(const auto& orderedPair : reorderSet)
						{
							multiNodeNames.push_back(orderedPair.first);
							hostnameArray.push_back(orderedPair.second);
						}

					}  //end reorder alphabetically
					__COUTV__(nodeName);
					__COUTV__(StringMacros::vectorToString(multiNodeNames));
					__COUTV__(StringMacros::vectorToString(hostnameArray));
					__COUTV__(StringMacros::setToString(skipSet));

					// from set of nodename wildcards, make printer syntax
					if(multiNodeNames.size() > 1)
					{
						std::vector<std::string> commonChunks;
						std::vector<std::string> wildcards;

						//can not change the order of wildcards for node names! or the names will not keep pairing with host

						bool wildcardsNeeded =
						    StringMacros::extractCommonChunks(multiNodeNames,
						                                      commonChunks,
						                                      wildcards,
						                                      nodeFixedWildcardLength);

						if(!wildcardsNeeded || wildcards.size() != multiNodeNames.size())
						{
							__SS__
							    << "Impossible extractCommonChunks result! Please notify "
							       "admins or try to simplify record naming convention."
							    << __E__;
							__SS_THROW__;
						}

						__COUTV__(StringMacros::vectorToString(commonChunks));
						__COUTV__(StringMacros::vectorToString(wildcards));

						nodeName   = "";
						bool first = true;
						for(auto& commonChunk : commonChunks)
						{
							nodeName += (!first ? "*" : "") + commonChunk;
							if(first)
								first = false;
						}
						if(commonChunks.size() == 1)
							nodeName += '*';

						__COUTV__(nodeName);

						// steps:
						//	determine if all unsigned ints
						//	if int, then order and attempt to hyphenate
						//	if not ints, then comma separated

						bool allIntegers = true;
						for(auto& wildcard : wildcards)
							if(!allIntegers)
								break;
							else if(wildcard.size() == 0)  // emtpy string is not a number
							{
								allIntegers = false;
								break;
							}
							else
								for(unsigned int i = 0; i < wildcard.size(); ++i)
									if(!(wildcard[i] >= '0' && wildcard[i] <= '9'))
									{
										allIntegers = false;
										break;
									}

						__COUTV__(allIntegers);
						if(allIntegers)
						{
							__COUTV__(StringMacros::vectorToString(wildcards));

							// need ints in vector for random access to for hyphenating
							std::vector<unsigned int> intWildcards;
							for(auto& wildcard : wildcards)
								intWildcards.push_back(strtol(wildcard.c_str(), 0, 10));

							__COUTV__(StringMacros::vectorToString(intWildcards));

							unsigned int hyphenLo = -1;
							bool         isFirst  = true;
							for(unsigned int i = 0; i < intWildcards.size(); ++i)
							{
								if(i + 1 < intWildcards.size() &&
								   intWildcards[i] + 1 == intWildcards[i + 1])
								{
									if(i < hyphenLo)
										hyphenLo = i;  // start hyphen
										               //else continue hyphen
								}
								else  // new comma
								{
									if(i < hyphenLo)
									{
										// single number
										multiNodeString +=
										    (isFirst ? "" : ",") +
										    std::to_string(intWildcards[i]);
									}
									else
									{
										// if only 1 number apart, then comma
										if(intWildcards[hyphenLo] + 1 == intWildcards[i])
											multiNodeString +=
											    (isFirst ? "" : ",") +
											    std::to_string(intWildcards[hyphenLo]) +
											    "," + std::to_string(intWildcards[i]);
										else  // else hyphen numbers
											multiNodeString +=
											    (isFirst ? "" : ",") +
											    std::to_string(intWildcards[hyphenLo]) +
											    "-" + std::to_string(intWildcards[i]);
										hyphenLo = -1;  // reset for next
									}
									isFirst = false;
								}
							}
						}     // end all integer handling
						else  // not all integers, so csv
						{
							multiNodeString = StringMacros::vectorToString(wildcards);
							nodeFixedWildcardLength =
							    0;  //wipe out fixed length rule if not all numbers
						}           // end not-all integer handling

						__COUTV__(multiNodeString);
						__COUTV__(nodeFixedWildcardLength);
					}  // end node name printer syntax handling

					if(hostnameArray.size() > 1)
					{
						std::vector<std::string> commonChunks;
						std::vector<std::string> wildcards;

						//can not change the order of wildcards for hostname! or the names will not keep pairing with host

						bool wildcardsNeeded =
						    StringMacros::extractCommonChunks(hostnameArray,
						                                      commonChunks,
						                                      wildcards,
						                                      hostFixedWildcardLength);

						__COUTV__(wildcardsNeeded);
						__COUTV__(StringMacros::vectorToString(commonChunks));
						__COUTV__(StringMacros::vectorToString(wildcards));

						hostname   = "";
						bool first = true;
						for(auto& commonChunk : commonChunks)
						{
							hostname += (!first ? "*" : "") + commonChunk;
							if(first)
								first = false;
						}
						if(wildcardsNeeded && commonChunks.size() == 1)
							hostname += '*';

						__COUTV__(hostname);

						if(wildcardsNeeded)
						// else if not wildcards needed, then do not make hostname array string
						{
							// steps:
							//	determine if all unsigned ints
							//	if int, then order and attempt to hyphenate
							//	if not ints, then comma separated

							bool allIntegers = true;
							for(auto& wildcard : wildcards)
								for(unsigned int i = 0; i < wildcard.size(); ++i)
									if(!(wildcard[i] >= '0' && wildcard[i] <= '9'))
									{
										allIntegers = false;
										break;
									}

							__COUTV__(allIntegers);

							if(allIntegers)
							{
								__COUTV__(StringMacros::vectorToString(wildcards));

								// need ints in vector for random access to for hyphenating
								std::vector<unsigned int> intWildcards;
								for(auto& wildcard : wildcards)
									intWildcards.push_back(
									    strtol(wildcard.c_str(), 0, 10));

								__COUTV__(StringMacros::vectorToString(intWildcards));

								unsigned int hyphenLo = -1;
								bool         isFirst  = true;
								for(unsigned int i = 0; i < intWildcards.size(); ++i)
								{
									if(i + 1 < intWildcards.size() &&
									   intWildcards[i] + 1 == intWildcards[i + 1])
									{
										if(i < hyphenLo)
											hyphenLo = i;  // start hyphen
											               //else continue hyphen
									}
									else  // new comma
									{
										if(i < hyphenLo)
										{
											// single number
											hostArrayString +=
											    (isFirst ? "" : ",") +
											    std::to_string(intWildcards[i]);
										}
										else
										{
											// if only 1 number apart, then comma
											if(intWildcards[hyphenLo] + 1 ==
											   intWildcards[i])
												hostArrayString +=
												    (isFirst ? "" : ",") +
												    std::to_string(
												        intWildcards[hyphenLo]) +
												    "," + std::to_string(intWildcards[i]);
											else  // else hyphen numbers
												hostArrayString +=
												    (isFirst ? "" : ",") +
												    std::to_string(
												        intWildcards[hyphenLo]) +
												    "-" + std::to_string(intWildcards[i]);
											hyphenLo = -1;  // reset for next
										}
										isFirst = false;
									}
								}
							}     // end all integer handling
							else  // not all integers, so csv
							{
								hostArrayString = StringMacros::vectorToString(wildcards);
								hostFixedWildcardLength =
								    0;  //wipe out fixed length rule if not all numbers
							}           // end not-all integer handling
						}               // end wildcard need handling
						__COUTV__(hostArrayString);
						__COUTV__(hostFixedWildcardLength);
					}  // end node name printer syntax handling

				}  // end multi node printer syntax handling

				nodeName +=
				    ";status=" +
				    std::string(status
				                    ? "1"
				                    : "0");  //include status in name to avoid collissions
				auto result = nodeTypeToObjectMap.at(typeString)
				                  .emplace(std::make_pair(
				                      nodeName, std::vector<std::string /*property*/>()));

				if(TTEST(0))
				{
					__SS__ << "Here is the current nodeTypeToObjectMap:" << __E__;
					for(const auto& typePair : nodeTypeToObjectMap)
					{
						ss << "\tType: " << typePair.first << __E__;
						for(const auto& nodePair : typePair.second)
						{
							ss << "\t\tNode: " << nodePair.first << __E__;
							for(const auto& property : nodePair.second)
								ss << "\t\t\tProperty: " << property << __E__;
						}
					}
					__COUT__ << ss.str() << __E__;
				}

				if(!result.second)
				{
					__COUT__
					    << "Collision detected for node '" << nodeName << "' of type '"
					    << typeString
					    << "' when inserting into nodeTypeToObjectMap. This likely means "
					       "that two nodes have the same name and status, and if so, "
					       "they would be indistinguishable in printer syntax. "
					    << "Please notify admins or try to simplify record naming "
					       "convention."
					    << __E__;

					__SS__ << "Impossible printer syntax handling result! Collision of "
					          "base names. Please notify "
					          "admins or try to simplify record naming convention."
					       << __E__;
					__SS_THROW__;
				}

				nodeTypeToObjectMap.at(typeString)
				    .at(nodeName)
				    .push_back(status ? "1" : "0");

				nodeTypeToObjectMap.at(typeString).at(nodeName).push_back(hostname);

				nodeTypeToObjectMap.at(typeString).at(nodeName).push_back(subsystemId);
				if(multiNodeNames.size() > 1)
				{
					nodeTypeToObjectMap.at(typeString)
					    .at(nodeName)
					    .push_back(multiNodeString);

					nodeTypeToObjectMap.at(typeString)
					    .at(nodeName)
					    .push_back(std::to_string(nodeFixedWildcardLength));

					if(hostnameArray.size() > 1)
					{
						nodeTypeToObjectMap.at(typeString)
						    .at(nodeName)
						    .push_back(hostArrayString);

						nodeTypeToObjectMap.at(typeString)
						    .at(nodeName)
						    .push_back(std::to_string(hostFixedWildcardLength));
					}
				}  // done adding multinode parameters

				__COUTV__(multiNodeString);
				__COUTV__(StringMacros::decodeURIComponent(hostname));
				__COUTV__(hostArrayString);
				__COUT__ << "Done with extraction of node '" << nodeName << "'" << __E__;
			}  //end main node extraction loop
		}      // end processor type handling

	}  // end artdaq app loop

	__COUT__ << "Done getting artdaq nodes." << __E__;

	return ARTDAQTableBase::info_;
}  // end getARTDAQSystem()

//==============================================================================
///	setAndActivateARTDAQSystem
///
///		static function to modify the active configuration based on
///	node object and subsystem object.
///
///	Subsystem map to destination subsystem name.
///	Node properties: {originalName,status,hostname,subsystemName,(nodeArrString),(hostnameArrString),(hostnameFixedWidth)}
///
void ARTDAQTableBase::setAndActivateARTDAQSystem(
    ConfigurationManagerRW*                                          cfgMgr,
    const std::map<std::string /*type*/,
                   std::map<std::string /*record*/,
                            std::vector<std::string /*property*/>>>& nodeTypeToObjectMap,
    const std::map<std::string /*subsystemName*/,
                   std::string /*destinationSubsystemName*/>&        subsystemObjectMap)
{
	__COUT__ << "setAndActivateARTDAQSystem()" << __E__;

	const std::string& author = cfgMgr->getUsername();

	// Steps:
	//	0. Check for one and only artdaq Supervisor
	//	1. create/verify subsystems and destinations
	//	2. for each node
	//		create/verify records

	//------------------------
	// 0. Check for one and only artdaq Supervisor

	GroupEditStruct configGroupEdit(ConfigurationManager::GroupType::CONFIGURATION_TYPE,
	                                cfgMgr);

	unsigned int artdaqSupervisorRow = TableView::INVALID;

	const XDAQContextTable* contextTable = cfgMgr->__GET_CONFIG__(XDAQContextTable);

	const XDAQContextTable::XDAQContext* artdaqContext =
	    contextTable->getTheARTDAQSupervisorContext();

	bool needArtdaqSupervisorParents  = true;
	bool needArtdaqSupervisorCreation = false;

	__COUTV__(artdaqContext);
	if(artdaqContext)  // check for full connection to supervisor
	{
		try
		{
			const std::string& activeContextGroupName =
			    cfgMgr->getActiveGroupName(ConfigurationManager::GroupType::CONTEXT_TYPE);
			const TableGroupKey& activeContextGroupKey =
			    cfgMgr->getActiveGroupKey(ConfigurationManager::GroupType::CONTEXT_TYPE);
			const std::string& activeConfigGroupName = cfgMgr->getActiveGroupName(
			    ConfigurationManager::GroupType::CONFIGURATION_TYPE);
			const TableGroupKey& activeConfigGroupKey = cfgMgr->getActiveGroupKey(
			    ConfigurationManager::GroupType::CONFIGURATION_TYPE);

			__COUTV__(activeContextGroupName);
			__COUTV__(activeContextGroupKey);
			__COUTV__(activeConfigGroupName);
			__COUTV__(activeConfigGroupKey);
			__COUTV__(cfgMgr->getNode(ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME)
			              .getNode(artdaqContext->contextUID_)
			              .getValueAsString());
			__COUTV__(
			    cfgMgr->getNode(ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME)
			        .getNode(artdaqContext->contextUID_)
			        .getNode(XDAQContextTable::colContext_.colLinkToApplicationTable_)
			        .getValueAsString());
			__COUTV__(
			    cfgMgr->getNode(ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME)
			        .getNode(artdaqContext->contextUID_)
			        .getNode(XDAQContextTable::colContext_.colLinkToApplicationTable_)
			        .getNode(artdaqContext->applications_[0].applicationUID_)
			        .getValueAsString());
			__COUTV__(artdaqContext->applications_[0].applicationUID_);
			__COUTV__(XDAQContextTable::colApplication_.colLinkToSupervisorTable_);
			__COUTV__(
			    cfgMgr->getNode(ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME)
			        .getNode(artdaqContext->contextUID_)
			        .getNode(XDAQContextTable::colContext_.colLinkToApplicationTable_)
			        .getNode(artdaqContext->applications_[0].applicationUID_)
			        .getNode(XDAQContextTable::colApplication_.colLinkToSupervisorTable_)
			        .getValueAsString());

			ConfigurationTree artdaqSupervisorNode =
			    cfgMgr->getNode(ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME)
			        .getNode(artdaqContext->contextUID_)
			        .getNode(XDAQContextTable::colContext_.colLinkToApplicationTable_)
			        .getNode(artdaqContext->applications_[0].applicationUID_)
			        .getNode(XDAQContextTable::colApplication_.colLinkToSupervisorTable_);

			__COUTV__(artdaqSupervisorNode.isDisconnected());

			if(artdaqSupervisorNode.isDisconnected())
				needArtdaqSupervisorCreation = true;
			else
				artdaqSupervisorRow = artdaqSupervisorNode.getRow();

			needArtdaqSupervisorParents = false;
		}
		catch(...)  // parents are a problem if error
		{
			needArtdaqSupervisorCreation = true;
		}
		__COUTV__(needArtdaqSupervisorCreation);
	}

	if(!artdaqContext || needArtdaqSupervisorCreation)
	{
		__COUT__ << "No artdaq Supervisor found! Creating..." << __E__;
		__COUTV__(needArtdaqSupervisorParents);

		std::string  artdaqSupervisorUID;
		unsigned int row;

		// create record in ARTDAQ Supervisor table
		//	connect to an App in a Context

		// now create artdaq Supervisor in configuration group
		{
			TableEditStruct& artdaqSupervisorTable = configGroupEdit.getTableEditStruct(
			    ARTDAQ_SUPERVISOR_TABLE, true /*markModified*/);

			if(TTEST(0))
			{
				std::stringstream ss;
				artdaqSupervisorTable.tableView_->print(ss);
				__COUT_MULTI__(0, ss.str());
			}

			// create artdaq Supervisor context record
			row = artdaqSupervisorTable.tableView_->addRow(
			    author, true /*incrementUniqueData*/, "artdaqSupervisor");

			// get UID
			artdaqSupervisorUID =
			    artdaqSupervisorTable.tableView_
			        ->getDataView()[row][artdaqSupervisorTable.tableView_->getColUID()];
			artdaqSupervisorRow = row;

			__COUTV__(artdaqSupervisorRow);
			__COUTV__(artdaqSupervisorUID);

			// set DAQInterfaceDebugLevel
			artdaqSupervisorTable.tableView_->setValueAsString(
			    "1",
			    row,
			    artdaqSupervisorTable.tableView_->findCol(
			        colARTDAQSupervisor_.colDAQInterfaceDebugLevel_));
			// set DAQSetupScript
			artdaqSupervisorTable.tableView_->setValueAsString(
			    "${MRB_BUILDDIR}/../setup_ots.sh",
			    row,
			    artdaqSupervisorTable.tableView_->findCol(
			        colARTDAQSupervisor_.colDAQSetupScript_));

			// create group link to board readers
			artdaqSupervisorTable.tableView_->setValueAsString(
			    ARTDAQ_READER_TABLE,
			    row,
			    artdaqSupervisorTable.tableView_->findCol(
			        colARTDAQSupervisor_.colLinkToBoardReaders_));
			artdaqSupervisorTable.tableView_->setUniqueColumnValue(

			    row,
			    artdaqSupervisorTable.tableView_->findCol(
			        colARTDAQSupervisor_.colLinkToBoardReadersGroupID_),
			    artdaqSupervisorUID +
			        processTypes_.mapToGroupIDAppend_.at(processTypes_.READER));
			// create group link to event builders
			artdaqSupervisorTable.tableView_->setValueAsString(
			    ARTDAQ_BUILDER_TABLE,
			    row,
			    artdaqSupervisorTable.tableView_->findCol(
			        colARTDAQSupervisor_.colLinkToEventBuilders_));
			artdaqSupervisorTable.tableView_->setUniqueColumnValue(
			    row,
			    artdaqSupervisorTable.tableView_->findCol(
			        colARTDAQSupervisor_.colLinkToEventBuildersGroupID_),
			    artdaqSupervisorUID +
			        processTypes_.mapToGroupIDAppend_.at(processTypes_.BUILDER));
			// create group link to data loggers
			artdaqSupervisorTable.tableView_->setValueAsString(
			    ARTDAQ_LOGGER_TABLE,
			    row,
			    artdaqSupervisorTable.tableView_->findCol(
			        colARTDAQSupervisor_.colLinkToDataLoggers_));
			artdaqSupervisorTable.tableView_->setUniqueColumnValue(
			    row,
			    artdaqSupervisorTable.tableView_->findCol(
			        colARTDAQSupervisor_.colLinkToDataLoggersGroupID_),
			    artdaqSupervisorUID +
			        processTypes_.mapToGroupIDAppend_.at(processTypes_.LOGGER));
			// create group link to dispatchers
			artdaqSupervisorTable.tableView_->setValueAsString(
			    ARTDAQ_DISPATCHER_TABLE,
			    row,
			    artdaqSupervisorTable.tableView_->findCol(
			        colARTDAQSupervisor_.colLinkToDispatchers_));
			artdaqSupervisorTable.tableView_->setUniqueColumnValue(
			    row,
			    artdaqSupervisorTable.tableView_->findCol(
			        colARTDAQSupervisor_.colLinkToDispatchersGroupID_),
			    artdaqSupervisorUID +
			        processTypes_.mapToGroupIDAppend_.at(processTypes_.DISPATCHER));

			// create group link to routing managers
			artdaqSupervisorTable.tableView_->setValueAsString(
			    ARTDAQ_ROUTER_TABLE,
			    row,
			    artdaqSupervisorTable.tableView_->findCol(
			        colARTDAQSupervisor_.colLinkToRoutingManagers_));
			artdaqSupervisorTable.tableView_->setUniqueColumnValue(
			    row,
			    artdaqSupervisorTable.tableView_->findCol(
			        colARTDAQSupervisor_.colLinkToRoutingManagersGroupID_),
			    artdaqSupervisorUID +
			        processTypes_.mapToGroupIDAppend_.at(processTypes_.ROUTER));

			if(TTEST(0))
			{
				std::stringstream ss;
				artdaqSupervisorTable.tableView_->print(ss);
				__COUT_MULTI__(0, ss.str());
			}
		}  // end create artdaq Supervisor in configuration group

		// now create artdaq Supervisor parents in context group
		{
			GroupEditStruct contextGroupEdit(
			    ConfigurationManager::GroupType::CONTEXT_TYPE, cfgMgr);

			TableEditStruct& contextTable = contextGroupEdit.getTableEditStruct(
			    ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME, true /*markModified*/);
			TableEditStruct& appTable = contextGroupEdit.getTableEditStruct(
			    ConfigurationManager::XDAQ_APPLICATION_TABLE_NAME, true /*markModified*/);
			TableEditStruct& appPropertyTable = contextGroupEdit.getTableEditStruct(
			    ConfigurationManager::XDAQ_APP_PROPERTY_TABLE_NAME,
			    true /*markModified*/);

			// open try for decorating errors and for clean code scope
			std::string appUID;
			try
			{
				std::string contextUID;
				std::string contextAppGroupID;

				if(needArtdaqSupervisorParents)
				{
					// create artdaq Supervisor context record
					row = contextTable.tableView_->addRow(
					    author, true /*incrementUniqueData*/, "artdaqContext");
					// set context status true
					contextTable.tableView_->setValueAsString(
					    "1", row, contextTable.tableView_->getColStatus());

					contextUID =
					    contextTable.tableView_
					        ->getDataView()[row][contextTable.tableView_->getColUID()];

					__COUTV__(row);
					__COUTV__(contextUID);

					// set address/port
					contextTable.tableView_->setValueAsString(
					    "http://${HOSTNAME}",
					    row,
					    contextTable.tableView_->findCol(
					        XDAQContextTable::colContext_.colAddress_));
					contextTable.tableView_->setUniqueColumnValue(
					    row,
					    contextTable.tableView_->findCol(
					        XDAQContextTable::colContext_.colPort_),
					    "${OTS_MAIN_PORT}",
					    true /*doMathAppendStrategy*/);

					// create group link to artdaq Supervisor app
					contextTable.tableView_->setValueAsString(
					    ConfigurationManager::XDAQ_APPLICATION_TABLE_NAME,
					    row,
					    contextTable.tableView_->findCol(
					        XDAQContextTable::colContext_.colLinkToApplicationTable_));
					contextAppGroupID = contextTable.tableView_->setUniqueColumnValue(
					    row,
					    contextTable.tableView_->findCol(
					        XDAQContextTable::colContext_.colLinkToApplicationGroupID_),
					    "artdaqContextApps");

					__COUTV__(contextAppGroupID);

				}  // end create context entry

				std::string appPropertiesGroupID;

				// create artdaq Supervisor app
				{
					unsigned int row;

					if(needArtdaqSupervisorParents)
					{
						// first disable any existing artdaq supervisor apps
						{
							unsigned int c = appTable.tableView_->findCol(
							    XDAQContextTable::colApplication_.colClass_);
							for(unsigned int r = 0;
							    r < appTable.tableView_->getNumberOfRows();
							    ++r)
								if(appTable.tableView_->getDataView()[r][c] ==
								   ARTDAQ_SUPERVISOR_CLASS)
								{
									__COUT_WARN__
									    << "Found partially existing artdaq Supervisor "
									       "application '"
									    << appTable.tableView_->getDataView()
									           [r][appTable.tableView_->getColUID()]
									    << "'... Disabling it." << __E__;
									appTable.tableView_->setValueAsString(
									    "0", r, appTable.tableView_->getColStatus());
								}
						}

						// create artdaq Supervisor context record
						row = appTable.tableView_->addRow(
						    author, true /*incrementUniqueData*/, "artdaqSupervisor");
						// set app status true
						appTable.tableView_->setValueAsString(
						    "1", row, appTable.tableView_->getColStatus());

						appUID =
						    appTable.tableView_
						        ->getDataView()[row][appTable.tableView_->getColUID()];

						__COUTV__(row);
						__COUTV__(appUID);

						// set class
						appTable.tableView_->setValueAsString(
						    ARTDAQ_SUPERVISOR_CLASS,
						    row,
						    appTable.tableView_->findCol(
						        XDAQContextTable::colApplication_.colClass_));
						// set module
						appTable.tableView_->setValueAsString(
						    "${OTSDAQ_LIB}/libARTDAQSupervisor.so",
						    row,
						    appTable.tableView_->findCol(
						        XDAQContextTable::colApplication_.colModule_));
						// set groupid
						appTable.tableView_->setValueAsString(
						    contextAppGroupID,
						    row,
						    appTable.tableView_->findCol(XDAQContextTable::colApplication_
						                                     .colApplicationGroupID_));

						// create group link to artdaq Supervisor app properties
						appTable.tableView_->setValueAsString(
						    ConfigurationManager::XDAQ_APP_PROPERTY_TABLE_NAME,
						    row,
						    appTable.tableView_->findCol(XDAQContextTable::colApplication_
						                                     .colLinkToPropertyTable_));
						appPropertiesGroupID = appTable.tableView_->setUniqueColumnValue(
						    row,
						    appTable.tableView_->findCol(XDAQContextTable::colApplication_
						                                     .colLinkToPropertyGroupID_),
						    appUID + "Properties");

						__COUTV__(appPropertiesGroupID);
					}
					else  //! needArtdaqSupervisorParents
					{
						__COUT__ << "Getting row of existing parent supervisor." << __E__;

						// get row of current artdaq supervisor app
						row =
						    cfgMgr->getNode(ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME)
						        .getNode(artdaqContext->contextUID_)
						        .getNode(XDAQContextTable::colContext_
						                     .colLinkToApplicationTable_)
						        .getNode(artdaqContext->applications_[0].applicationUID_)
						        .getRow();
						__COUTV__(row);
					}

					// create group link to artdaq Supervisor app properties
					//		create link whether or not parents were created
					//		because, if here, then artdaq supervisor record was created.
					appTable.tableView_->setValueAsString(
					    ARTDAQ_SUPERVISOR_TABLE,
					    row,
					    appTable.tableView_->findCol(
					        XDAQContextTable::colApplication_.colLinkToSupervisorTable_));
					appTable.tableView_->setValueAsString(
					    artdaqSupervisorUID,
					    row,
					    appTable.tableView_->findCol(
					        XDAQContextTable::colApplication_.colLinkToSupervisorUID_));

				}  // end create app entry

				// create artdaq Supervisor properties
				if(needArtdaqSupervisorParents)
				{
					unsigned int row;

					const std::vector<std::string> propertyUIDs  = {"Partition0",
					                                                "ProductsDir",
					                                                "FragmentSize",
					                                                "BoardReaderTimeout",
					                                                "EventBuilderTimeout",
					                                                "DataLoggerTimeout",
					                                                "DispatcherTimeout"};
					const std::vector<std::string> propertyNames = {
					    "partition",                     //"Partition0",
					    "productsdir_for_bash_scripts",  //"ProductsDir",
					    "max_fragment_size_bytes",       //"FragmentSize",
					    "boardreader_timeout",           //"BoardReaderTimeout",
					    "eventbuilder_timeout",          //"EventBuilderTimeout",
					    "datalogger_timeout",            //"DataLoggerTimeout",
					    "dispatcher_timeout"             //"DispatcherTimeout"
					};
					const std::vector<std::string> propertyValues = {
					    "0",                //"Partition0",
					    "${OTS_PRODUCTS}",  //"ProductsDir",
					    "1284180560",       //"FragmentSize",
					    "600",              //"BoardReaderTimeout",
					    "600",              //"EventBuilderTimeout",
					    "600",              //"DataLoggerTimeout",
					    "600"               //"DispatcherTimeout"
					};

					for(unsigned int i = 0; i < propertyNames.size(); ++i)
					{
						// create artdaq Supervisor property record
						row = appPropertyTable.tableView_->addRow(
						    author,
						    true /*incrementUniqueData*/,
						    appUID + propertyUIDs[i]);
						// set app status true
						appPropertyTable.tableView_->setValueAsString(
						    "1", row, appPropertyTable.tableView_->getColStatus());

						// set type
						appPropertyTable.tableView_->setValueAsString(
						    "ots::SupervisorProperty",
						    row,
						    appPropertyTable.tableView_->findCol(
						        XDAQContextTable::colAppProperty_.colPropertyType_));
						// set name
						appPropertyTable.tableView_->setValueAsString(
						    propertyNames[i],
						    row,
						    appPropertyTable.tableView_->findCol(
						        XDAQContextTable::colAppProperty_.colPropertyName_));
						// set value
						appPropertyTable.tableView_->setValueAsString(
						    propertyValues[i],
						    row,
						    appPropertyTable.tableView_->findCol(
						        XDAQContextTable::colAppProperty_.colPropertyValue_));
						// set groupid
						appPropertyTable.tableView_->setValueAsString(
						    appPropertiesGroupID,
						    row,
						    appPropertyTable.tableView_->findCol(
						        XDAQContextTable::colAppProperty_.colPropertyGroupID_));
					}  // end property create loop
				}      // end create app property entries

				{
					std::stringstream ss;
					contextTable.tableView_->print(ss);
					__COUT_MULTI__(0, ss.str());
				}
				{
					std::stringstream ss;
					appTable.tableView_->print(ss);
					__COUT_MULTI__(0, ss.str());
				}
				{
					std::stringstream ss;
					appPropertyTable.tableView_->print(ss);
					__COUT_MULTI__(0, ss.str());
				}

				contextTable.tableView_
				    ->init();                 // verify new table (throws runtime_errors)
				appTable.tableView_->init();  // verify new table (throws runtime_errors)
				appPropertyTable.tableView_
				    ->init();  // verify new table (throws runtime_errors)
			}
			catch(...)
			{
				__COUT__
				    << "Table errors while creating ARTDAQ Supervisor. Erasing all newly "
				       "created table versions."
				    << __E__;
				throw;  // re-throw
			}           // end catch

			__COUT_INFO__ << "Edits complete for new artdaq Supervisor! Created '"
			              << appUID << "'" << __E__;

			if(0)  //keep for debugging save process
			{
				__SS__ << "DEBUG blocking artdaq supervisor save!" << __E__;
				__SS_THROW__;
			}
			TableGroupKey newContextGroupKey;
			contextGroupEdit.saveChanges(contextGroupEdit.originalGroupName_,
			                             newContextGroupKey,
			                             nullptr /*foundEquivalentGroupKey*/,
			                             true /*activateNewGroup*/,
			                             true /*updateGroupAliases*/,
			                             true /*updateTableAliases*/);

		}  // end create artdaq Supervisor in context group

	}  // end artdaq Supervisor verification
	else
	{
		artdaqSupervisorRow =
		    cfgMgr->getNode(ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME)
		        .getNode(artdaqContext->contextUID_)
		        .getNode(XDAQContextTable::colContext_.colLinkToApplicationTable_)
		        .getNode(artdaqContext->applications_[0].applicationUID_)
		        .getNode(XDAQContextTable::colApplication_.colLinkToSupervisorTable_)
		        .getRow();
	}

	__COUT__ << "------------------------- artdaq nodes to save:" << __E__;
	for(auto& subsystemPair : subsystemObjectMap)
	{
		__COUTV__(subsystemPair.first);

	}  // end subsystem loop

	for(auto& nodeTypePair : nodeTypeToObjectMap)
	{
		__COUTV__(nodeTypePair.first);

		for(auto& nodePair : nodeTypePair.second)
		{
			__COUTV__(nodePair.first);
		}

	}  // end node type loop
	__COUT__ << "------------------------- end artdaq nodes to save." << __E__;

	//==================================
	// at this point artdaqSupervisor is verified and we have row
	__COUTV__(artdaqSupervisorRow);
	if(artdaqSupervisorRow >= TableView::INVALID)
	{
		__SS__ << "Invalid artdaq Supervisor row " << artdaqSupervisorRow << " found!"
		       << __E__;
		__SS_THROW__;
	}

	// Remaining steps:
	// Step	1. create/verify subsystems and destinations
	// Step	2. for each node, create/verify records

	// open try for decorating configuration group errors and for clean code scope
	try
	{
		unsigned int row;

		TableEditStruct& artdaqSupervisorTable = configGroupEdit.getTableEditStruct(
		    ARTDAQ_SUPERVISOR_TABLE, true /*markModified*/);

		// for any NO_LINK links in artdaqSupervisor record, fix them
		{
			std::string artdaqSupervisorUID =
			    artdaqSupervisorTable.tableView_
			        ->getDataView()[artdaqSupervisorRow]
			                       [artdaqSupervisorTable.tableView_->getColUID()];

			// create group link to board readers
			if(artdaqSupervisorTable.tableView_
			       ->getDataView()[artdaqSupervisorRow]
			                      [artdaqSupervisorTable.tableView_->findCol(
			                          colARTDAQSupervisor_.colLinkToBoardReaders_)] ==
			   TableViewColumnInfo::DATATYPE_LINK_DEFAULT)
			{
				__COUT__ << "Fixing missing link to Readers" << __E__;
				artdaqSupervisorTable.tableView_->setValueAsString(
				    ARTDAQ_READER_TABLE,
				    artdaqSupervisorRow,
				    artdaqSupervisorTable.tableView_->findCol(
				        colARTDAQSupervisor_.colLinkToBoardReaders_));
				artdaqSupervisorTable.tableView_->setUniqueColumnValue(
				    artdaqSupervisorRow,
				    artdaqSupervisorTable.tableView_->findCol(
				        colARTDAQSupervisor_.colLinkToBoardReadersGroupID_),
				    artdaqSupervisorUID +
				        processTypes_.mapToGroupIDAppend_.at(processTypes_.READER));
			}

			// create group link to event builders
			if(artdaqSupervisorTable.tableView_
			       ->getDataView()[artdaqSupervisorRow]
			                      [artdaqSupervisorTable.tableView_->findCol(
			                          colARTDAQSupervisor_.colLinkToEventBuilders_)] ==
			   TableViewColumnInfo::DATATYPE_LINK_DEFAULT)
			{
				__COUT__ << "Fixing missing link to Builders" << __E__;
				artdaqSupervisorTable.tableView_->setValueAsString(
				    ARTDAQ_BUILDER_TABLE,
				    artdaqSupervisorRow,
				    artdaqSupervisorTable.tableView_->findCol(
				        colARTDAQSupervisor_.colLinkToEventBuilders_));
				artdaqSupervisorTable.tableView_->setUniqueColumnValue(
				    artdaqSupervisorRow,
				    artdaqSupervisorTable.tableView_->findCol(
				        colARTDAQSupervisor_.colLinkToEventBuildersGroupID_),
				    artdaqSupervisorUID +
				        processTypes_.mapToGroupIDAppend_.at(processTypes_.BUILDER));
			}

			// create group link to data loggers
			if(artdaqSupervisorTable.tableView_
			       ->getDataView()[artdaqSupervisorRow]
			                      [artdaqSupervisorTable.tableView_->findCol(
			                          colARTDAQSupervisor_.colLinkToDataLoggers_)] ==
			   TableViewColumnInfo::DATATYPE_LINK_DEFAULT)
			{
				__COUT__ << "Fixing missing link to Loggers" << __E__;
				artdaqSupervisorTable.tableView_->setValueAsString(
				    ARTDAQ_LOGGER_TABLE,
				    artdaqSupervisorRow,
				    artdaqSupervisorTable.tableView_->findCol(
				        colARTDAQSupervisor_.colLinkToDataLoggers_));
				artdaqSupervisorTable.tableView_->setUniqueColumnValue(
				    artdaqSupervisorRow,
				    artdaqSupervisorTable.tableView_->findCol(
				        colARTDAQSupervisor_.colLinkToDataLoggersGroupID_),
				    artdaqSupervisorUID +
				        processTypes_.mapToGroupIDAppend_.at(processTypes_.LOGGER));
			}

			// create group link to dispatchers
			if(artdaqSupervisorTable.tableView_
			       ->getDataView()[artdaqSupervisorRow]
			                      [artdaqSupervisorTable.tableView_->findCol(
			                          colARTDAQSupervisor_.colLinkToDispatchers_)] ==
			   TableViewColumnInfo::DATATYPE_LINK_DEFAULT)
			{
				__COUT__ << "Fixing missing link to Dispatchers" << __E__;
				artdaqSupervisorTable.tableView_->setValueAsString(
				    ARTDAQ_DISPATCHER_TABLE,
				    artdaqSupervisorRow,
				    artdaqSupervisorTable.tableView_->findCol(
				        colARTDAQSupervisor_.colLinkToDispatchers_));
				artdaqSupervisorTable.tableView_->setUniqueColumnValue(
				    artdaqSupervisorRow,
				    artdaqSupervisorTable.tableView_->findCol(
				        colARTDAQSupervisor_.colLinkToDispatchersGroupID_),
				    artdaqSupervisorUID +
				        processTypes_.mapToGroupIDAppend_.at(processTypes_.DISPATCHER));
			}

			// create group link to routing managers
			if(artdaqSupervisorTable.tableView_
			       ->getDataView()[artdaqSupervisorRow]
			                      [artdaqSupervisorTable.tableView_->findCol(
			                          colARTDAQSupervisor_.colLinkToRoutingManagers_)] ==
			   TableViewColumnInfo::DATATYPE_LINK_DEFAULT)
			{
				__COUT__ << "Fixing missing link to Routers" << __E__;
				artdaqSupervisorTable.tableView_->setValueAsString(
				    ARTDAQ_ROUTER_TABLE,
				    artdaqSupervisorRow,
				    artdaqSupervisorTable.tableView_->findCol(
				        colARTDAQSupervisor_.colLinkToRoutingManagers_));
				artdaqSupervisorTable.tableView_->setUniqueColumnValue(
				    artdaqSupervisorRow,
				    artdaqSupervisorTable.tableView_->findCol(
				        colARTDAQSupervisor_.colLinkToRoutingManagersGroupID_),
				    artdaqSupervisorUID +
				        processTypes_.mapToGroupIDAppend_.at(processTypes_.ROUTER));
			}

			{
				std::stringstream ss;
				artdaqSupervisorTable.tableView_->print(ss);
				__COUT_MULTI__(0, ss.str());
			}
		}  // end fixing links

		// Step	1. create/verify subsystems and destinations
		TableEditStruct& artdaqSubsystemTable = configGroupEdit.getTableEditStruct(
		    ARTDAQ_SUBSYSTEM_TABLE, true /*markModified*/);

		// clear all records
		artdaqSubsystemTable.tableView_->deleteAllRows();

		for(auto& subsystemPair : subsystemObjectMap)
		{
			__COUTV__(subsystemPair.first);
			__COUTV__(subsystemPair.second);

			// create artdaq Subsystem record
			row = artdaqSubsystemTable.tableView_->addRow(
			    author, true /*incrementUniqueData*/, subsystemPair.first);

			if(subsystemPair.second != "" &&
			   subsystemPair.second != TableViewColumnInfo::DATATYPE_STRING_DEFAULT &&
			   subsystemPair.second != TableViewColumnInfo::DATATYPE_STRING_ALT_DEFAULT &&
			   subsystemPair.second != NULL_SUBSYSTEM_DESTINATION_LABEL)
			{
				// set subsystem link
				artdaqSubsystemTable.tableView_->setValueAsString(
				    ARTDAQ_SUBSYSTEM_TABLE,
				    row,
				    artdaqSubsystemTable.tableView_->findCol(
				        colARTDAQSubsystem_.colLinkToDestination_));
				artdaqSubsystemTable.tableView_->setValueAsString(
				    subsystemPair.second,
				    row,
				    artdaqSubsystemTable.tableView_->findCol(
				        colARTDAQSubsystem_.colLinkToDestinationUID_));
			}
			// else leave disconnected link

		}  // end subsystem loop

		// Step	2. for each node, create/verify records
		for(auto& nodeTypePair : nodeTypeToObjectMap)
		{
			__COUTV__(nodeTypePair.first);

			//__COUTV__(StringMacros::mapToString(processTypes_.mapToTable_));

			auto it = processTypes_.mapToTable_.find(nodeTypePair.first);
			if(it == processTypes_.mapToTable_.end())
			{
				__SS__ << "Invalid artdaq node type '" << nodeTypePair.first
				       << "' attempted!" << __E__;
				__SS_THROW__;
			}
			__COUTV__(it->second);

			// test the table before getting for real
			try
			{
				/*	TableEditStruct& tmpTypeTable = */ configGroupEdit.getTableEditStruct(
				    it->second, true /*markModified*/);
			}
			catch(...)
			{
				if(nodeTypePair.second.size())
					throw;  // do not ignore if user was trying to save records

				__COUT__ << "Ignoring missing table '" << it->second
				         << "' since there were no user records attempted of type '"
				         << nodeTypePair.first << ".'" << __E__;
				continue;
			}
			TableEditStruct& typeTable =
			    configGroupEdit.getTableEditStruct(it->second, true /*markModified*/);

			TableEditStruct* artTable          = nullptr;
			bool             hasArtProcessName = false;
			unsigned int     artProcessNameCol = -1;
			if(nodeTypePair.first != ARTDAQTableBase::processTypes_.READER &&
			   nodeTypePair.first != ARTDAQTableBase::processTypes_.ROUTER)
			{
				__COUT__ << "Identified non-Reader, no-Router type '"
				         << nodeTypePair.first
				         << "' that has an art link and thus Process Name, so creating "
				            "table edit structure to ART table."
				         << __E__;
				artTable = &configGroupEdit.getTableEditStruct(
				    ARTDAQTableBase::ARTDAQ_ART_TABLE, true /*markModified*/);
				if(TTEST(1))
				{
					std::stringstream ss;
					artTable->tableView_->print(ss);
					__COUT_MULTI__(1, ss.str());
				}
				artProcessNameCol = artTable->tableView_->findCol(
				    ARTDAQTableBase::colARTDAQArt_.colProcessName_);
				__COUTTV__(artProcessNameCol);

				hasArtProcessName = true;
			}
			__COUTV__(hasArtProcessName);

			const unsigned int commentCol =
			    typeTable.tableView_->findColByType(TableViewColumnInfo::TYPE_COMMENT);
			const unsigned int authorCol =
			    typeTable.tableView_->findColByType(TableViewColumnInfo::TYPE_AUTHOR);
			const unsigned int timestampCol =
			    typeTable.tableView_->findColByType(TableViewColumnInfo::TYPE_TIMESTAMP);

			// keep track of records to delete, initialize to all in current table
			std::map<unsigned int /*type record row*/, bool /*doDelete*/> deleteRecordMap;
			unsigned int maxRowToDelete = typeTable.tableView_->getNumberOfRows();
			for(unsigned int r = 0; r < typeTable.tableView_->getNumberOfRows(); ++r)
				deleteRecordMap.emplace(std::make_pair(
				    r,  // typeTable.tableView_->getDataView()[i][typeTable.tableView_->getColUID()],
				    true));  // init to delete
			__COUTTV__(maxRowToDelete);

			// keep a map of original multinode values, to maintain node specific links
			//	(emplace when original node is deleted)
			// Note special (hierarchical) columns are defined as follows:
			//	 [-1] := ARTDAQTableBase::colARTDAQNotReader_.colLinkToArt_ / ARTDAQTableBase::colARTDAQArt_.colProcessName_
			const unsigned int ORIG_MAP_ART_PROC_NAME_COL = -1;
			std::map<std::string /*originalMultiNode name*/,
			         std::map<unsigned int /*col*/, std::string /*value*/>>
			    originalMultinodeValues;
			std::map<std::string /*multinode key*/,
			         std::map<unsigned int /*col*/,
			                  std::pair<bool /* all siblings have same value */,
			                            std::string /* sameValue */>>>
			    originalMultinodeSameSiblingValues;
			std::map<
			    std::string /*multinode key*/,
			    std::map<unsigned int /*col*/,
			             std::pair<bool /* all siblings have embedded name */,
			                       std::vector<std::string /* splitForEmbeddedValue */>>>>
			    originalMultinodeAllSiblingEmbeddedName;
			std::map<
			    std::string /*multinode key*/,
			    std::map<unsigned int /*col*/,
			             std::pair<bool /* all siblings have embedded printer index */,
			                       std::vector<std::string /* splitForEmbeddedIndex */>>>>
			    originalMultinodeAllSiblingEmbeddedPrinterIndex;

			// node instance loop
			for(auto& nodePair : nodeTypePair.second)
			{
				__COUTV__(nodePair.first);  //new name

				// default multi-node and array hostname info to empty
				std::vector<std::string> nodeIndices, hostnameIndices;
				unsigned int             hostnameFixedWidth = 0, nodeNameFixedWidth = 0;
				std::string              hostname;

				// if original record is found, then commandeer that record
				//	else create a new record
				// Node properties: {originalName,hostname,subsystemName,(nodeArrString),(nodeNameFixedWidth),(hostnameArrString),(hostnameFixedWidth)}

				// node parameter loop
				for(unsigned int i = 0; i < nodePair.second.size(); ++i)
				{
					__COUTV__(nodePair.second[i]);  //original name

					if(i == 0)  // original UID
					{
						std::string nodeName;
						// Steps:
						//	if original was multi-node,
						//		then delete all but one
						//	else
						//		take over the row, or create new
						if(nodePair.second[i][0] == ':')
						{
							__COUT__ << "Handling original multi-node." << __E__;

							// format:
							//	:<nodeNameFixedWidth>:<nodeVectorIndexString>:<nodeNameTemplate>

							std::string              lastOriginalName;
							std::vector<std::string> originalParameterArr =
							    StringMacros::getVectorFromString(
							        &(nodePair.second[i].c_str()[1]),
							        {':'} /*delimiter*/);

							if(originalParameterArr.size() != 3)
							{
								__SS__ << "Illegal original name parameter string '"
								       << nodePair.second[i] << "!'" << __E__;
								__SS_THROW__;
							}
							__COUTTV__(
							    StringMacros::vectorToString(originalParameterArr));

							unsigned int fixedWidth;
							sscanf(originalParameterArr[0].c_str(), "%u", &fixedWidth);
							__COUTV__(fixedWidth);

							std::vector<std::string> printerSyntaxArr =
							    StringMacros::getVectorFromString(originalParameterArr[1],
							                                      {','} /*delimiter*/);

							// unsigned int             count = 0;
							std::vector<std::string> originalNodeIndices;
							for(auto& printerSyntaxValue : printerSyntaxArr)
							{
								__COUTV__(printerSyntaxValue);

								std::vector<std::string> printerSyntaxRange =
								    StringMacros::getVectorFromString(
								        printerSyntaxValue, {'-'} /*delimiter*/);

								if(printerSyntaxRange.size() == 0 ||
								   printerSyntaxRange.size() > 2)
								{
									__SS__ << "Illegal multi-node printer syntax string '"
									       << printerSyntaxValue << "!'" << __E__;
									__SS_THROW__;
								}
								else if(printerSyntaxRange.size() == 1)
								{
									__COUTV__(printerSyntaxRange[0]);
									originalNodeIndices.push_back(printerSyntaxRange[0]);
								}
								else  // printerSyntaxRange.size() == 2
								{
									unsigned int lo, hi;
									sscanf(printerSyntaxRange[0].c_str(), "%u", &lo);
									sscanf(printerSyntaxRange[1].c_str(), "%u", &hi);
									if(hi < lo)  // swap
									{
										lo = hi;
										sscanf(printerSyntaxRange[0].c_str(), "%u", &hi);
									}
									for(; lo <= hi; ++lo)
									{
										__COUTTV__(lo);
										originalNodeIndices.push_back(std::to_string(lo));
									}
								}
							}  // end printer syntax loop

							__COUTTV__(originalParameterArr[2]);
							//remove ;status=
							originalParameterArr[2] = originalParameterArr[2].substr(
							    0, originalParameterArr[2].find(";status="));
							__COUTV__(originalParameterArr[2]);
							std::vector<std::string> originalNamePieces =
							    StringMacros::getVectorFromString(originalParameterArr[2],
							                                      {'*'} /*delimiter*/);
							__COUTV__(StringMacros::vectorToString(originalNamePieces));

							if(originalNamePieces.size() < 2)
							{
								__SS__ << "Illegal original multi-node name template - "
								          "please use * to indicate where the multi-node "
								          "index should be inserted!"
								       << __E__;
								__SS_THROW__;
							}

							if(TTEST(1))
							{
								std::stringstream ss;
								typeTable.tableView_->print(ss);
								__COUT_MULTI__(1, ss.str());
							}

							//create matching bools to decide copy stategy
							__COUT__
							    << "originalMultinodeSameSiblingValues init col map for "
							    << nodePair.first << __E__;
							originalMultinodeSameSiblingValues.emplace(std::make_pair(
							    nodePair.first,
							    std::map<
							        unsigned int /*col*/,
							        std::pair<bool /* all siblings have same value */,
							                  std::string /* sameValue */>>()));
							__COUT__ << "originalMultinodeAllSiblingEmbeddedName init "
							            "col map for "
							         << nodePair.first << __E__;
							originalMultinodeAllSiblingEmbeddedName.emplace(std::make_pair(
							    nodePair.first,
							    std::map<
							        unsigned int /*col*/,
							        std::pair<
							            bool /* all siblings have embedded name */,
							            std::vector<
							                std::
							                    string /* splitForEmbeddedValue */>>>()));
							__COUT__ << "originalMultinodeAllSiblingEmbeddedPrinterIndex "
							            "init col map for "
							         << nodePair.first << __E__;
							originalMultinodeAllSiblingEmbeddedPrinterIndex.emplace(
							    std::make_pair(
							        nodePair.first,
							        std::map<
							            unsigned int /*col*/,
							            std::pair<
							                bool /* all siblings have embedded printed index */
							                ,
							                std::vector<
							                    std::
							                        string /* splitForEmbeddedIndex */>>>()));

							// bool         isFirst     = true;
							unsigned int originalRow       = TableView::INVALID,
							             lastOriginalRow   = TableView::INVALID,
							             lastArtProcessRow = TableView::INVALID;
							for(unsigned int i = 0; i < originalNodeIndices.size(); ++i)
							{
								std::string originalName = originalNamePieces[0];
								std::string nodeNameIndex;
								for(unsigned int p = 1; p < originalNamePieces.size();
								    ++p)
								{
									nodeNameIndex = originalNodeIndices[i];
									if(fixedWidth > 1)
									{
										if(nodeNameIndex.size() > fixedWidth)
										{
											__SS__ << "Illegal original node name index '"
											       << nodeNameIndex
											       << "' - length is longer than fixed "
											          "width requirement of "
											       << fixedWidth << "!" << __E__;
											__SS_THROW__;
										}

										// 0 prepend as needed
										while(nodeNameIndex.size() < fixedWidth)
											nodeNameIndex = "0" + nodeNameIndex;
									}  // end fixed width handling

									originalName += nodeNameIndex + originalNamePieces[p];
								}
								__COUTTV__(originalName);
								originalRow = typeTable.tableView_->findRow(
								    typeTable.tableView_->getColUID(),
								    originalName,
								    0 /*offsetRow*/,
								    true /*doNotThrow*/);
								__COUTTV__(originalRow);

								// if have a new 'seed' valid row, then delete last valid row
								// before deleting, record all customizing values to draw from when creating new multinode records
								auto result = originalMultinodeValues.emplace(
								    std::make_pair(originalName,
								                   std::map<unsigned int /*col*/,
								                            std::string /*value*/>()));
								if(!result.second)
									__COUT__
									    << "originalName '" << originalName
									    << "' already in original multinode value cache."
									    << __E__;
								else  //keep original cache values
								{
									__COUT__ << "Saving multinode value " << originalName
									         << "[" << originalRow
									         << "][*] with row count = "
									         << typeTable.tableView_->getNumberOfRows()
									         << __E__;

									// save all link values
									for(unsigned int col = 0;
									    col < typeTable.tableView_->getNumberOfColumns();
									    ++col)
									{
										if(typeTable.tableView_->getColumnInfo(col)
										           .getName() ==
										       ARTDAQTableBase::
										           ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK ||
										   typeTable.tableView_->getColumnInfo(col)
										           .getName() ==
										       ARTDAQTableBase::
										           ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK_UID ||
										   typeTable.tableView_->getColumnInfo(col)
										           .getName() ==
										       ARTDAQTableBase::
										           ARTDAQ_TYPE_TABLE_HOSTNAME ||
										   typeTable.tableView_->getColumnInfo(col)
										       .isUID() ||
										   col == typeTable.tableView_->getColStatus() ||
										   typeTable.tableView_->getColumnInfo(col)
										       .isGroupID() ||
										   col == timestampCol ||
										   col ==
										       authorCol)  //always go with now author/timestamp on touched records (too easy to misidentify change vs nochange)
											continue;  // skip subsystem link, etc that is modified by fields maintained in the GUI
										else
										{
											__COUTT__
											    << "Caching node value: " << originalName
											    << "[" << originalRow << "][" << col
											    << "/"
											    << typeTable.tableView_
											           ->getColumnInfo(col)
											           .getName()
											    << "] = "
											    << typeTable.tableView_
											           ->getDataView()[originalRow][col]
											    << __E__;
											originalMultinodeValues.at(originalName)
											    .emplace(std::make_pair(
											        col,
											        typeTable.tableView_
											            ->getDataView()[originalRow]
											                           [col]));

											//the first time, set to true and then prove wrong

											for(const auto& pair :
											    originalMultinodeSameSiblingValues)
												__COUTT__ << "originalMultinodeSameSiblin"
												             "gValues["
												          << pair.first << "]" << __E__;
											auto result2 =
											    originalMultinodeSameSiblingValues
											        .at(nodePair.first)
											        .emplace(std::make_pair(
											            col,
											            //same value
											            std::make_pair(
											                true,
											                typeTable.tableView_
											                    ->getDataView()
											                        [originalRow][col])));

											for(const auto& pair :
											    originalMultinodeAllSiblingEmbeddedName)
												__COUTT__ << "originalMultinodeAllSibling"
												             "EmbeddedName["
												          << pair.first << "]" << __E__;
											originalMultinodeAllSiblingEmbeddedName
											    .at(nodePair.first)
											    .emplace(std::make_pair(
											        col,
											        std::make_pair(  //bool
											            typeTable.tableView_
											                    ->getDataView()
											                        [originalRow][col]
											                    .find(originalName) !=
											                std::string::npos,
											            //split string
											            std::vector<std::string>())));

											for(const auto& pair :
											    originalMultinodeAllSiblingEmbeddedPrinterIndex)
												__COUTT__ << "originalMultinodeAllSibling"
												             "EmbeddedPrinterIndex["
												          << pair.first << "]" << __E__;
											originalMultinodeAllSiblingEmbeddedPrinterIndex
											    .at(nodePair.first)
											    .emplace(std::make_pair(
											        col,
											        std::make_pair(  //bool
											            typeTable.tableView_
											                    ->getDataView()
											                        [originalRow][col]
											                    .find(nodeNameIndex) !=
											                std::string::npos,
											            //split string
											            std::vector<std::string>())));

											if(result2
											       .second)  //emplace always should work first time
											{
												__COUTTV__(
												    originalMultinodeSameSiblingValues
												        .at(nodePair.first)
												        .at(col)
												        .second);

												__COUTTV__(
												    originalMultinodeAllSiblingEmbeddedName
												        .at(nodePair.first)
												        .at(col)
												        .first);
												if(originalMultinodeAllSiblingEmbeddedName
												       .at(nodePair.first)
												       .at(col)
												       .first)
												{
													__COUTT__
													    << "Determine string splits for "
													       "embedded name"
													    << __E__;
													const std::string& val =
													    typeTable.tableView_
													        ->getDataView()[originalRow]
													                       [col];
													size_t pos = val.find(originalName);
													originalMultinodeAllSiblingEmbeddedName
													    .at(nodePair.first)
													    .at(col)
													    .second.push_back(
													        val.substr(0, pos));
													originalMultinodeAllSiblingEmbeddedName
													    .at(nodePair.first)
													    .at(col)
													    .second.push_back(val.substr(
													        pos + originalName.size()));
													__COUTTV__(StringMacros::vectorToString(
													    originalMultinodeAllSiblingEmbeddedName
													        .at(nodePair.first)
													        .at(col)
													        .second));
												}
												__COUTTV__(
												    originalMultinodeAllSiblingEmbeddedPrinterIndex
												        .at(nodePair.first)
												        .at(col)
												        .first);
												if(originalMultinodeAllSiblingEmbeddedPrinterIndex
												       .at(nodePair.first)
												       .at(col)
												       .first)
												{
													__COUTT__ << "Determine string "
													             "splits for embedded "
													             "printer syntax index: "
													          << nodeNameIndex << __E__;
													const std::string& val =
													    typeTable.tableView_
													        ->getDataView()[originalRow]
													                       [col];
													size_t pos = val.find(nodeNameIndex);
													originalMultinodeAllSiblingEmbeddedPrinterIndex
													    .at(nodePair.first)
													    .at(col)
													    .second.push_back(
													        val.substr(0, pos));
													originalMultinodeAllSiblingEmbeddedPrinterIndex
													    .at(nodePair.first)
													    .at(col)
													    .second.push_back(val.substr(
													        pos + nodeNameIndex.size()));
													__COUTTV__(StringMacros::vectorToString(
													    originalMultinodeAllSiblingEmbeddedPrinterIndex
													        .at(nodePair.first)
													        .at(col)
													        .second));
												}
											}
											else  //not first time, so prove wrong
											{
												if(originalMultinodeSameSiblingValues
												       .at(nodePair.first)
												       .at(col)
												       .first)
												{
													__COUTT__ << "Checking sibling same "
													             "values... for "
													          << nodePair.first << __E__;
													if(typeTable.tableView_
													       ->getDataView()[originalRow]
													                      [col] !=
													   typeTable.tableView_->getDataView()
													       [lastOriginalRow][col])
													{
														__COUT__
														    << "Found different sibling "
														       "values at col="
														    << col << " for "
														    << nodePair.first << __E__;
														originalMultinodeSameSiblingValues
														    .at(nodePair.first)
														    .at(col)
														    .first = false;
													}
												}
												if(originalMultinodeAllSiblingEmbeddedName
												       .at(nodePair.first)
												       .at(col)
												       .first)
												{
													__COUTT__ << "Checking sibling "
													             "embedded name... for "
													          << nodePair.first << ":"
													          << originalName << __E__;
													if(typeTable.tableView_
													       ->getDataView()[originalRow]
													                      [col]
													       .find(originalName) ==
													   std::string::npos)
													{
														__COUT__ << "Found no embedded "
														            "name at col="
														         << col << " looking for "
														         << originalName << __E__;
														originalMultinodeAllSiblingEmbeddedName
														    .at(nodePair.first)
														    .at(col)
														    .first = false;
													}
												}
												if(originalMultinodeAllSiblingEmbeddedPrinterIndex
												       .at(nodePair.first)
												       .at(col)
												       .first)
												{
													__COUTT__
													    << "Checking sibling embedded "
													       "printer syntax index... for "
													    << nodePair.first << ":"
													    << nodeNameIndex << __E__;
													if(typeTable.tableView_
													       ->getDataView()[originalRow]
													                      [col]
													       .find(nodeNameIndex) ==
													   std::string::npos)
													{
														__COUT__ << "Found no embedded "
														            "printer syntax "
														            "index at col="
														         << col << " looking for "
														         << nodeNameIndex
														         << __E__;
														originalMultinodeAllSiblingEmbeddedPrinterIndex
														    .at(nodePair.first)
														    .at(col)
														    .first = false;
													}
												}
											}

											__COUTT__
											    << "originalMultinodeSameSiblingValues["
											    << nodePair.first << "][" << col << "] = "
											    << originalMultinodeSameSiblingValues
											           .at(nodePair.first)
											           .at(col)
											           .first
											    << __E__;
											__COUTT__
											    << "originalMultinodeAllSiblingEmbeddedNa"
											       "me["
											    << nodePair.first << "][" << col << "] = "
											    << originalMultinodeAllSiblingEmbeddedName
											           .at(nodePair.first)
											           .at(col)
											           .first
											    << __E__;
											__COUTT__
											    << "originalMultinodeAllSiblingEmbeddedPr"
											       "interIndex["
											    << nodePair.first << "][" << col << "] = "
											    << originalMultinodeAllSiblingEmbeddedPrinterIndex
											           .at(nodePair.first)
											           .at(col)
											           .first
											    << __E__;

											if(hasArtProcessName && artTable &&
											   typeTable.tableView_->getColumnInfo(col)
											           .getName() ==
											       ARTDAQTableBase::colARTDAQNotReader_
											           .colLinkToArtUID_)
											{
												//note at this point, col = Link to art record
												__COUT__
												    << "Checking ART Process Name... for "
												       "originalName='"
												    << originalName << "' / "
												    << typeTable.tableView_
												           ->getDataView()[originalRow]
												                          [col]
												    << __E__;
												unsigned int artRow =
												    artTable->tableView_->findRow(
												        artTable->tableView_->getColUID(),
												        /* art UID record name */
												        typeTable.tableView_
												            ->getDataView()[originalRow]
												                           [col]);
												__COUTTV__(artRow);

												__COUTT__
												    << "Found ART Process Name = "
												    << artTable->tableView_->getDataView()
												           [artRow][artProcessNameCol]
												    << __E__;

												//original value tracking/emplace handling copied from above L4284
												originalMultinodeValues.at(originalName)
												    .emplace(std::make_pair(
												        ORIG_MAP_ART_PROC_NAME_COL,
												        artTable->tableView_
												            ->getDataView()
												                [artRow]
												                [artProcessNameCol]));
												__COUTTV__(
												    originalMultinodeValues
												        .at(originalName)
												        .at(ORIG_MAP_ART_PROC_NAME_COL));

												//the first time, set to true and then prove wrong
												originalMultinodeSameSiblingValues
												    .at(nodePair.first)
												    .emplace(std::make_pair(
												        ORIG_MAP_ART_PROC_NAME_COL,
												        //same value
												        std::make_pair(
												            true,
												            artTable->tableView_
												                ->getDataView()
												                    [artRow]
												                    [artProcessNameCol])));
												originalMultinodeAllSiblingEmbeddedName
												    .at(nodePair.first)
												    .emplace(std::make_pair(
												        ORIG_MAP_ART_PROC_NAME_COL,
												        std::make_pair(  //bool
												            artTable->tableView_
												                    ->getDataView()
												                        [artRow]
												                        [artProcessNameCol]
												                    .find(originalName) !=
												                std::string::npos,
												            //split string
												            std::vector<std::string>())));
												originalMultinodeAllSiblingEmbeddedPrinterIndex
												    .at(nodePair.first)
												    .emplace(std::make_pair(
												        ORIG_MAP_ART_PROC_NAME_COL,
												        std::make_pair(  //bool
												            artTable->tableView_
												                    ->getDataView()
												                        [artRow]
												                        [artProcessNameCol]
												                    .find(
												                        nodeNameIndex) !=
												                std::string::npos,
												            //split string
												            std::vector<std::string>())));

												if(result2
												       .second)  //emplace always should work first time
												{
													__COUTTV__(
													    originalMultinodeSameSiblingValues
													        .at(nodePair.first)
													        .at(ORIG_MAP_ART_PROC_NAME_COL)
													        .second);

													__COUTTV__(
													    originalMultinodeAllSiblingEmbeddedName
													        .at(nodePair.first)
													        .at(ORIG_MAP_ART_PROC_NAME_COL)
													        .first);
													if(originalMultinodeAllSiblingEmbeddedName
													       .at(nodePair.first)
													       .at(ORIG_MAP_ART_PROC_NAME_COL)
													       .first)
													{
														__COUTT__
														    << "Determine string splits "
														       "for embedded name"
														    << __E__;
														const std::string& val =
														    artTable->tableView_
														        ->getDataView()
														            [artRow]
														            [artProcessNameCol];
														size_t pos =
														    val.find(originalName);
														originalMultinodeAllSiblingEmbeddedName
														    .at(nodePair.first)
														    .at(ORIG_MAP_ART_PROC_NAME_COL)
														    .second.push_back(
														        val.substr(0, pos));
														originalMultinodeAllSiblingEmbeddedName
														    .at(nodePair.first)
														    .at(ORIG_MAP_ART_PROC_NAME_COL)
														    .second.push_back(val.substr(
														        pos +
														        originalName.size()));
														__COUTTV__(StringMacros::vectorToString(
														    originalMultinodeAllSiblingEmbeddedName
														        .at(nodePair.first)
														        .at(ORIG_MAP_ART_PROC_NAME_COL)
														        .second));
													}
													__COUTTV__(
													    originalMultinodeAllSiblingEmbeddedPrinterIndex
													        .at(nodePair.first)
													        .at(ORIG_MAP_ART_PROC_NAME_COL)
													        .first);
													if(originalMultinodeAllSiblingEmbeddedPrinterIndex
													       .at(nodePair.first)
													       .at(ORIG_MAP_ART_PROC_NAME_COL)
													       .first)
													{
														__COUTT__
														    << "Determine string splits "
														       "for embedded printer "
														       "syntax index: "
														    << nodeNameIndex << __E__;
														const std::string& val =
														    artTable->tableView_
														        ->getDataView()
														            [artRow]
														            [artProcessNameCol];
														size_t pos =
														    val.find(nodeNameIndex);
														originalMultinodeAllSiblingEmbeddedPrinterIndex
														    .at(nodePair.first)
														    .at(ORIG_MAP_ART_PROC_NAME_COL)
														    .second.push_back(
														        val.substr(0, pos));
														originalMultinodeAllSiblingEmbeddedPrinterIndex
														    .at(nodePair.first)
														    .at(ORIG_MAP_ART_PROC_NAME_COL)
														    .second.push_back(val.substr(
														        pos +
														        nodeNameIndex.size()));
														__COUTTV__(StringMacros::vectorToString(
														    originalMultinodeAllSiblingEmbeddedPrinterIndex
														        .at(nodePair.first)
														        .at(ORIG_MAP_ART_PROC_NAME_COL)
														        .second));
													}
												}
												else  //not first time, so prove wrong
												{
													if(originalMultinodeSameSiblingValues
													       .at(nodePair.first)
													       .at(ORIG_MAP_ART_PROC_NAME_COL)
													       .first)
													{
														__COUTT__ << "Checking sibling "
														             "same values... for "
														          << nodePair.first
														          << __E__;
														if(artTable->tableView_
														       ->getDataView()
														           [artRow]
														           [artProcessNameCol] !=
														   artTable->tableView_
														       ->getDataView()
														           [lastArtProcessRow]
														           [artProcessNameCol])
														{
															__COUT__
															    << "Found different "
															       "sibling values "
															       "at artProcessNameCol="
															    << artProcessNameCol
															    << " for "
															    << nodePair.first
															    << __E__;
															originalMultinodeSameSiblingValues
															    .at(nodePair.first)
															    .at(ORIG_MAP_ART_PROC_NAME_COL)
															    .first = false;
														}
													}
													if(originalMultinodeAllSiblingEmbeddedName
													       .at(nodePair.first)
													       .at(ORIG_MAP_ART_PROC_NAME_COL)
													       .first)
													{
														__COUTT__
														    << "Checking sibling "
														       "embedded name... for "
														    << nodePair.first << ":"
														    << originalName << __E__;
														if(artTable->tableView_
														       ->getDataView()
														           [artRow]
														           [artProcessNameCol]
														       .find(originalName) ==
														   std::string::npos)
														{
															__COUT__
															    << "Found no embedded "
															       "name at "
															       "artProcessNameCol="
															    << artProcessNameCol
															    << " looking for "
															    << originalName << __E__;
															originalMultinodeAllSiblingEmbeddedName
															    .at(nodePair.first)
															    .at(ORIG_MAP_ART_PROC_NAME_COL)
															    .first = false;
														}
													}
													if(originalMultinodeAllSiblingEmbeddedPrinterIndex
													       .at(nodePair.first)
													       .at(ORIG_MAP_ART_PROC_NAME_COL)
													       .first)
													{
														__COUTT__
														    << "Checking sibling "
														       "embedded printer syntax "
														       "index... for "
														    << nodePair.first << ":"
														    << nodeNameIndex << __E__;
														if(artTable->tableView_
														       ->getDataView()
														           [artRow]
														           [artProcessNameCol]
														       .find(nodeNameIndex) ==
														   std::string::npos)
														{
															__COUT__
															    << "Found no embedded "
															       "printer syntax index "
															       "at artProcessNameCol="
															    << artProcessNameCol
															    << " looking for "
															    << nodeNameIndex << __E__;
															originalMultinodeAllSiblingEmbeddedPrinterIndex
															    .at(nodePair.first)
															    .at(ORIG_MAP_ART_PROC_NAME_COL)
															    .first = false;
														}
													}
												}

												__COUTT__
												    << "originalMultinodeSameSiblingValue"
												       "s["
												    << nodePair.first << "]["
												    << ORIG_MAP_ART_PROC_NAME_COL
												    << "] = "
												    << originalMultinodeSameSiblingValues
												           .at(nodePair.first)
												           .at(ORIG_MAP_ART_PROC_NAME_COL)
												           .first
												    << __E__;
												__COUTT__
												    << "originalMultinodeAllSiblingEmbedd"
												       "edName["
												    << nodePair.first << "]["
												    << ORIG_MAP_ART_PROC_NAME_COL
												    << "] = "
												    << originalMultinodeAllSiblingEmbeddedName
												           .at(nodePair.first)
												           .at(ORIG_MAP_ART_PROC_NAME_COL)
												           .first
												    << __E__;
												__COUTT__
												    << "originalMultinodeAllSiblingEmbedd"
												       "edPrinterIndex["
												    << nodePair.first << "]["
												    << ORIG_MAP_ART_PROC_NAME_COL
												    << "] = "
												    << originalMultinodeAllSiblingEmbeddedPrinterIndex
												           .at(nodePair.first)
												           .at(ORIG_MAP_ART_PROC_NAME_COL)
												           .first
												    << __E__;

												__COUT__
												    << "Checking ART Process Name "
												       "complete for originalName='"
												    << originalName << "' / "
												    << typeTable.tableView_
												           ->getDataView()[originalRow]
												                          [col]
												    << __E__;
												lastArtProcessRow =
												    artRow;  //save for next comparison
											}  //end ART Process Name cache handling

										}  //end col caching handling
									}      //end col loop
								}          //end cache handling

								if(originalRow !=
								   TableView::
								       INVALID)  // save last original valid row for future cache/deletion
									lastOriginalRow = originalRow;

								__COUTTV__(lastOriginalRow);
								lastOriginalName = originalName;
							}  // end loop through multi-node instances

							for(const auto& pair :
							    originalMultinodeSameSiblingValues.at(nodePair.first))
								__COUTT__ << "originalMultinodeSameSiblingValues["
								          << nodePair.first << "][" << pair.first
								          << "]  = " << pair.second.first << __E__;
							for(const auto& pair :
							    originalMultinodeAllSiblingEmbeddedName.at(
							        nodePair.first))
								__COUTT__ << "originalMultinodeAllSiblingEmbeddedName["
								          << nodePair.first << "][" << pair.first
								          << "]  = " << pair.second.first << __E__;
							for(const auto& pair :
							    originalMultinodeAllSiblingEmbeddedPrinterIndex.at(
							        nodePair.first))
								__COUTT__
								    << "originalMultinodeAllSiblingEmbeddedPrinterIndex["
								    << nodePair.first << "][" << pair.first
								    << "]  = " << pair.second.first << __E__;

							__COUTTV__(lastOriginalRow);
							row = lastOriginalRow;  // take last valid row to proceed
							__COUTV__(row);
						}  // end handling of original multinode
						else
						{
							std::string originalName = nodePair.second[i].substr(
							    0, nodePair.second[i].find(";status="));
							__COUTV__(originalName);

							// attempt to find original 'single' node name
							row = typeTable.tableView_->findRow(
							    typeTable.tableView_->getColUID(),
							    originalName,
							    0 /*offsetRow*/,
							    true /*doNotThrow*/);
							__COUTV__(row);
						}

						//if no original nodes, there may be *'s in node name, so remove them
						{
							nodeName = nodePair.first;  // take new node name
							__COUTV__(nodeName);
							//remove ;status=
							nodeName = nodeName.substr(0, nodeName.find(";status="));

							//remove stars for seed nodename
							std::string tmpNodeName = nodeName;
							nodeName                = "";  //clear
							for(size_t c = 0; c < tmpNodeName.size(); ++c)
								if(tmpNodeName[c] != '*')
									nodeName += tmpNodeName[c];
						}  //end removing *'s from node name

						__COUTV__(nodeName);
						if(row == TableView::INVALID)
						{
							// No original record, so create artdaq type instance record
							row = typeTable.tableView_->addRow(
							    author, true /*incrementUniqueData*/, nodeName);

							// fill defaults properties/parameters here!
							if(nodeTypePair.first == processTypes_.READER)
							{
								__COUT__ << "Handling new " << nodeTypePair.first
								         << " defaults!" << __E__;
								TableEditStruct& daqParameterTable =
								    configGroupEdit.getTableEditStruct(
								        ARTDAQTableBase::ARTDAQ_DAQ_PARAMETER_TABLE,
								        true /*markModified*/);

								// create group link to daq parameter table
								typeTable.tableView_->setValueAsString(
								    ARTDAQTableBase::ARTDAQ_DAQ_PARAMETER_TABLE,
								    row,
								    typeTable.tableView_->findCol(
								        ARTDAQTableBase::colARTDAQReader_
								            .colLinkToDaqParameters_));
								std::string daqParameterGroupID =
								    typeTable.tableView_->setUniqueColumnValue(
								        row,
								        typeTable.tableView_->findCol(
								            ARTDAQTableBase::colARTDAQReader_
								                .colLinkToDaqParametersGroupID_),
								        nodeName + "DaqParameters");

								{
									std::stringstream ss;
									typeTable.tableView_->print(ss);
									__COUT_MULTI__(1, ss.str());
								}

								// now create parameters at target link
								const std::vector<std::string> parameterUIDs = {
								    "BoardID", "FragmentID"};

								const std::vector<std::string> parameterNames = {
								    "board_id",     //"BoardID",
								    "fragment_id",  //"FragmentID"
								};
								const std::vector<std::string> parameterValues = {
								    "0",  //"BoardID",
								    "0"   //"FragmentID",
								};

								unsigned int parameterRow;
								for(unsigned int i = 0; i < parameterNames.size(); ++i)
								{
									// create artdaq Reader property record
									parameterRow = daqParameterTable.tableView_->addRow(
									    author,
									    true /*incrementUniqueData*/,
									    nodeName + parameterUIDs[i]);

									// set app status true
									daqParameterTable.tableView_->setValueAsString(
									    "1",
									    parameterRow,
									    daqParameterTable.tableView_->getColStatus());
									// set key
									daqParameterTable.tableView_->setValueAsString(
									    parameterNames[i],
									    parameterRow,
									    daqParameterTable.tableView_->findCol(
									        ARTDAQTableBase::colARTDAQDaqParameter_
									            .colDaqParameterKey_));
									// set value
									daqParameterTable.tableView_->setValueAsString(
									    parameterValues[i],
									    parameterRow,
									    daqParameterTable.tableView_->findCol(
									        ARTDAQTableBase::colARTDAQDaqParameter_
									            .colDaqParameterValue_));
									// set groupid
									daqParameterTable.tableView_->setValueAsString(
									    daqParameterGroupID,
									    parameterRow,
									    daqParameterTable.tableView_->findCol(
									        ARTDAQTableBase::colARTDAQDaqParameter_
									            .colDaqParameterGroupID_));

								}  // end Reader default property create loop

								daqParameterTable.tableView_
								    ->init();  // verify new table (throws runtime_errors)

							}  // end Reader default property setup
							else if(nodeTypePair.first == processTypes_.BUILDER ||
							        nodeTypePair.first == processTypes_.LOGGER ||
							        nodeTypePair.first == processTypes_.DISPATCHER)
							{
								__COUT__ << "Handling new " << nodeTypePair.first
								         << " defaults!" << __E__;

								// goes through DAQ table
								TableEditStruct& daqTable =
								    configGroupEdit.getTableEditStruct(
								        ARTDAQTableBase::ARTDAQ_DAQ_TABLE,
								        true /*markModified*/);
								// create DAQ record
								unsigned int daqRecordRow = daqTable.tableView_->addRow(
								    author,
								    true /*incrementUniqueData*/,
								    nodeName + "Daq");
								std::string daqRecordUID =
								    daqTable.tableView_
								        ->getDataView()[daqRecordRow]
								                       [daqTable.tableView_->getColUID()];

								// create unique link to daq table
								typeTable.tableView_->setValueAsString(
								    ARTDAQTableBase::ARTDAQ_DAQ_TABLE,
								    row,
								    typeTable.tableView_->findCol(
								        ARTDAQTableBase::colARTDAQNotReader_
								            .colLinkToDaq_));
								typeTable.tableView_->setValueAsString(
								    daqRecordUID,
								    row,
								    typeTable.tableView_->findCol(
								        ARTDAQTableBase::colARTDAQNotReader_
								            .colLinkToDaqUID_));

								TableEditStruct& daqParameterTable =
								    configGroupEdit.getTableEditStruct(
								        ARTDAQTableBase::ARTDAQ_DAQ_PARAMETER_TABLE,
								        true /*markModified*/);
								// create group link to daq parameter table
								daqTable.tableView_->setValueAsString(
								    ARTDAQTableBase::ARTDAQ_DAQ_PARAMETER_TABLE,
								    daqRecordRow,
								    daqTable.tableView_->findCol(
								        ARTDAQTableBase::colARTDAQDaq_
								            .colLinkToDaqParameters_));
								std::string daqParameterGroupID =
								    daqTable.tableView_->setUniqueColumnValue(
								        daqRecordRow,
								        daqTable.tableView_->findCol(
								            ARTDAQTableBase::colARTDAQDaq_
								                .colLinkToDaqParametersGroupID_),
								        nodeName + "DaqParameters");

								// now create parameters at target link
								const std::vector<std::string> parameterUIDs = {
								    "BufferCount", "FragmentsPerEvent"};

								const std::vector<std::string> parameterNames = {
								    "buffer_count",                 //"BufferCount",
								    "expected_fragments_per_event"  //"FragmentsPerEvent"
								};
								const std::vector<std::string> parameterValues = {
								    "10",  //"BufferCount",
								    "0"    //"FragmentsPerEvent",
								};

								unsigned int parameterRow;
								for(unsigned int i = 0; i < parameterNames.size(); ++i)
								{
									// create artdaq Reader property record
									parameterRow = daqParameterTable.tableView_->addRow(
									    author,
									    true /*incrementUniqueData*/,
									    nodeName + parameterUIDs[i]);

									// set app status true
									daqParameterTable.tableView_->setValueAsString(
									    "1",
									    parameterRow,
									    daqParameterTable.tableView_->getColStatus());
									// set key
									daqParameterTable.tableView_->setValueAsString(
									    parameterNames[i],
									    parameterRow,
									    daqParameterTable.tableView_->findCol(
									        ARTDAQTableBase::colARTDAQDaqParameter_
									            .colDaqParameterKey_));
									// set value
									daqParameterTable.tableView_->setValueAsString(
									    parameterValues[i],
									    parameterRow,
									    daqParameterTable.tableView_->findCol(
									        ARTDAQTableBase::colARTDAQDaqParameter_
									            .colDaqParameterValue_));
									// set groupid
									daqParameterTable.tableView_->setValueAsString(
									    daqParameterGroupID,
									    parameterRow,
									    daqParameterTable.tableView_->findCol(
									        ARTDAQTableBase::colARTDAQDaqParameter_
									            .colDaqParameterGroupID_));

								}  // end Reader default property create loop

								daqTable.tableView_
								    ->init();  // verify new table (throws runtime_errors)
								daqParameterTable.tableView_
								    ->init();  // verify new table (throws runtime_errors)

							}  // end Builder, Logger, Dispatcher default property setup
						}
						else  // set UID
						{
							__COUT__
							    << "Reusing row " << row << " current-UID="
							    << typeTable.tableView_
							           ->getDataView()[row]
							                          [typeTable.tableView_->getColUID()]
							    << " as (temporarily to basename if multinode) new-UID="
							    << nodeName << __E__;
							typeTable.tableView_
							    ->setValueAsString(  //if single record, this renaming is final; if multi record, this renaming to basename is temporary
							        nodeName,
							        row,
							        typeTable.tableView_->getColUID());
						}
						__COUTV__(row);

						// remove from delete map
						if(row < maxRowToDelete)
							deleteRecordMap[row] = false;

						__COUTV__(StringMacros::mapToString(
						    processTypes_.mapToLinkGroupIDColumn_));

						// set GroupID
						typeTable.tableView_->setValueAsString(
						    artdaqSupervisorTable.tableView_
						        ->getDataView()[artdaqSupervisorRow]
						                       [artdaqSupervisorTable.tableView_->findCol(
						                           processTypes_.mapToLinkGroupIDColumn_
						                               .at(nodeTypePair.first))],
						    row,
						    typeTable.tableView_->findCol(
						        processTypes_.mapToGroupIDColumn_.at(
						            nodeTypePair.first)));
					}
					else if(i == 1)  // status
					{
						// enable/disable the target row
						typeTable.tableView_->setValueAsString(
						    nodePair.second[i],
						    row,
						    typeTable.tableView_->getColStatus());
					}
					else if(i == 2)  // hostname
					{
						// set hostname
						hostname = nodePair.second[i];
						typeTable.tableView_->setValueAsString(
						    hostname,
						    row,
						    typeTable.tableView_->findCol(ARTDAQ_TYPE_TABLE_HOSTNAME));
					}
					else if(i == 3)  // subsystemName
					{
						// set subsystemName
						if(nodePair.second[i] != "" &&
						   nodePair.second[i] !=
						       TableViewColumnInfo::DATATYPE_STRING_DEFAULT &&
						   nodePair.second[i] !=
						       TableViewColumnInfo::DATATYPE_STRING_ALT_DEFAULT)
						{
							// real subsystem?
							if(subsystemObjectMap.find(nodePair.second[i]) ==
							   subsystemObjectMap.end())
							{
								__SS__ << "Illegal subsystem '" << nodePair.second[i]
								       << "' mismatch!" << __E__;
								__SS_THROW__;
							}

							typeTable.tableView_->setValueAsString(
							    ARTDAQ_SUBSYSTEM_TABLE,
							    row,
							    typeTable.tableView_->findCol(
							        ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK));
							typeTable.tableView_->setValueAsString(
							    nodePair.second[i],
							    row,
							    typeTable.tableView_->findCol(
							        ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK_UID));
						}
						else  // no subsystem (i.e. default subsystem)
						{
							typeTable.tableView_->setValueAsString(
							    TableViewColumnInfo::DATATYPE_LINK_DEFAULT,
							    row,
							    typeTable.tableView_->findCol(
							        ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK));
						}
					}
					else if(
					    i == 4 || i == 5 || i == 6 ||
					    i ==
					        7)  //(nodeArrString),(nodeNameFixedWidth),(hostnameArrString),(hostnameFixedWidth)
					{
						// fill multi-node and array hostname info to empty
						// then handle after all parameters in hand.

						__COUT__ << "Handling printer syntax i=" << i << __E__;

						std::vector<std::string> printerSyntaxArr =
						    StringMacros::getVectorFromString(nodePair.second[i],
						                                      {','} /*delimiter*/);

						if(printerSyntaxArr.size() == 2)  // consider if fixed value
						{
							if(printerSyntaxArr[0] ==
							   "nnfw")  // then node name fixed width
							{
								sscanf(printerSyntaxArr[1].c_str(),
								       "%u",
								       &nodeNameFixedWidth);
								__COUTV__(nodeNameFixedWidth);
								continue;
							}
							else if(printerSyntaxArr[0] ==
							        "hnfw")  // then hostname fixed width
							{
								sscanf(printerSyntaxArr[1].c_str(),
								       "%u",
								       &hostnameFixedWidth);
								__COUTV__(hostnameFixedWidth);
								continue;
							}
						}

						// unsigned int count = 0;
						for(auto& printerSyntaxValue : printerSyntaxArr)
						{
							__COUTV__(printerSyntaxValue);

							std::vector<std::string> printerSyntaxRange =
							    StringMacros::getVectorFromString(printerSyntaxValue,
							                                      {'-'} /*delimiter*/);
							if(printerSyntaxRange.size() == 0 ||
							   printerSyntaxRange.size() > 2)
							{
								__SS__ << "Illegal multi-node printer syntax string '"
								       << printerSyntaxValue << "!'" << __E__;
								__SS_THROW__;
							}
							else if(printerSyntaxRange.size() == 1)
							{
								// unsigned int index;
								__COUTV__(printerSyntaxRange[0]);
								// sscanf(printerSyntaxRange[0].c_str(), "%u", &index);
								//__COUTV__(index);

								if(i == 4 /*nodeArrayString*/)
									nodeIndices.push_back(printerSyntaxRange[0]);
								else
									hostnameIndices.push_back(printerSyntaxRange[0]);
							}
							else  // printerSyntaxRange.size() == 2
							{
								unsigned int lo, hi;
								sscanf(printerSyntaxRange[0].c_str(), "%u", &lo);
								sscanf(printerSyntaxRange[1].c_str(), "%u", &hi);
								if(hi < lo)  // swap
								{
									lo = hi;
									sscanf(printerSyntaxRange[0].c_str(), "%u", &hi);
								}
								for(; lo <= hi; ++lo)
								{
									__COUTVS__(5, lo);
									if(i == 4 /*nodeArrayString*/)
										nodeIndices.push_back(std::to_string(lo));
									else
										hostnameIndices.push_back(std::to_string(lo));
								}
							}
						}
					}
					else
					{
						__SS__ << "Unexpected parameter[" << i << " '"
						       << nodePair.second[i] << "' for node " << nodePair.first
						       << "!" << __E__;
						__SS_THROW__;
					}
				}  // end node parameter loop

				__COUTV__(nodeIndices.size());
				__COUTV__(hostnameIndices.size());

				if(hostnameIndices.size())  // handle hostname array
				{
					if(hostnameIndices.size() != nodeIndices.size())
					{
						__SS__ << "Illegal associated hostname array has count "
						       << hostnameIndices.size()
						       << " which is not equal to the node count "
						       << nodeIndices.size() << "!" << __E__;
						__SS_THROW__;
					}
				}

				if(nodeIndices.size())  // handle multi-node instances
				{
					unsigned int hostnameCol =
					    typeTable.tableView_->findCol(ARTDAQ_TYPE_TABLE_HOSTNAME);
					// Steps:
					//	first instance takes current row,
					//	then copy for remaining instances

					std::vector<std::string> namePieces =
					    StringMacros::getVectorFromString(
					        nodePair.first.substr(0, nodePair.first.find(";status=")),
					        {'*'} /*delimiter*/);
					__COUTV__(StringMacros::vectorToString(namePieces));

					if(namePieces.size() < 2)
					{
						__SS__
						    << "Illegal multi-node name template - please use * to "
						       "indicate where the multi-node index should be inserted!"
						    << __E__;
						__SS_THROW__;
					}

					std::vector<std::string> hostnamePieces;
					if(hostnameIndices.size())  // handle hostname array
					{
						hostnamePieces = StringMacros::getVectorFromString(
						    hostname, {'*'} /*delimiter*/);
						__COUTV__(StringMacros::vectorToString(hostnamePieces));

						if(hostnamePieces.size() < 2)
						{
							__SS__
							    << "Illegal hostname array template - please use * to "
							       "indicate where the hostname index should be inserted!"
							    << __E__;
							__SS_THROW__;
						}
					}

					bool         isFirst    = true;
					unsigned int lastArtRow = TableView::INVALID;
					for(unsigned int i = 0; i < nodeIndices.size(); ++i)
					{
						std::string name = namePieces[0];
						std::string nodeNameIndex;
						for(unsigned int p = 1; p < namePieces.size(); ++p)
						{
							nodeNameIndex = nodeIndices[i];
							if(nodeNameFixedWidth > 1)
							{
								if(nodeNameIndex.size() > nodeNameFixedWidth)
								{
									__SS__ << "Illegal node name index '" << nodeNameIndex
									       << "' - length is longer than fixed width "
									          "requirement of "
									       << nodeNameFixedWidth << "!" << __E__;
									__SS_THROW__;
								}

								// 0 prepend as needed
								while(nodeNameIndex.size() < nodeNameFixedWidth)
									nodeNameIndex = "0" + nodeNameIndex;
							}  // end fixed width handling

							name += nodeNameIndex + namePieces[p];
						}
						__COUTV__(name);

						if(hostnamePieces.size())
						{
							hostname = hostnamePieces[0];
							std::string hostnameIndex;
							for(unsigned int p = 1; p < hostnamePieces.size(); ++p)
							{
								hostnameIndex = hostnameIndices[i];
								if(hostnameFixedWidth > 1)
								{
									if(hostnameIndex.size() > hostnameFixedWidth)
									{
										__SS__ << "Illegal hostname index '"
										       << hostnameIndex
										       << "' - length is longer than fixed width "
										          "requirement of "
										       << hostnameFixedWidth << "!" << __E__;
										__SS_THROW__;
									}

									// 0 prepend as needed
									while(hostnameIndex.size() < hostnameFixedWidth)
										hostnameIndex = "0" + hostnameIndex;
								}  // end fixed width handling

								hostname += hostnameIndex + hostnamePieces[p];
							}
							__COUTV__(hostname);
						}
						// else use hostname from above

						if(isFirst)  // take current row
						{
							__COUTT__
							    << author << "... Replacing row UID '"
							    << typeTable.tableView_
							           ->getDataView()[row]
							                          [typeTable.tableView_->getColUID()]
							    << "' with UID '" << name << "'" << __E__;

							// remove from delete map
							if(row < maxRowToDelete)
								deleteRecordMap[row] = false;
						}
						else  // copy row
						{
							__COUTT__
							    << author << "... Copying row UID '"
							    << typeTable.tableView_
							           ->getDataView()[row]
							                          [typeTable.tableView_->getColUID()]
							    << "' to UID '" << name << "'" << __E__;
							unsigned int copyRow = typeTable.tableView_->copyRows(
							    author,
							    *(typeTable.tableView_),
							    row,
							    1 /*srcRowsToCopy*/,
							    -1 /*destOffsetRow*/,
							    true /*generateUniqueDataColumns*/);

							// remove from delete map
							if(row < maxRowToDelete)
								deleteRecordMap[copyRow] = false;
							row = copyRow;
						}

						typeTable.tableView_->setValueAsString(
						    name, row, typeTable.tableView_->getColUID());
						typeTable.tableView_->setValueAsString(
						    hostname, row, hostnameCol);
						//NOTE: changing UID and copyRows does not change author or date! So change it if not an exact match; so change it now and fill with original archive if exact match
						typeTable.tableView_->setValueAsString(
						    TableViewColumnInfo::DATATYPE_COMMENT_DEFAULT,
						    row,
						    commentCol);
						typeTable.tableView_->setValueAsString(author, row, authorCol);
						typeTable.tableView_->setValue(time(0), row, timestampCol);

						__COUTTV__(typeTable
						               .tableView_  //comment
						               ->getDataView()[row][commentCol]);
						__COUTTV__(typeTable
						               .tableView_  //author
						               ->getDataView()[row][authorCol]);
						__COUTTV__(typeTable
						               .tableView_  //creation time
						               ->getDataView()[row][timestampCol]);
						// Strategy:
						// 	- Take values from best original node match
						//	- Then overwrite with same values
						//	- Then overwrite with embedded name values
						//	- If name matches exactly the original name, then keep Comment, Author, and CreationTime
						//i.e., Customize row based on original value map, originalMultinodeSameSiblingValues and originalMultinodeAllSiblingEmbeddedName and originalMultinodeAllSiblingEmbeddedPrinterIndex
						{
							//find highest score match to original node
							__COUT__
							    << "Looking for best original node match for row=" << row
							    << " UID='" << name << "'" << __E__;
							size_t      bestScore = 0;
							std::string bestOriginalNodeName;
							for(const auto& originalNodePair : originalMultinodeValues)
							{
								if(originalNodePair.second.find(
								       ORIG_MAP_ART_PROC_NAME_COL) !=
								   originalNodePair.second.end())
									__COUTTV__(originalNodePair.second.at(
									    ORIG_MAP_ART_PROC_NAME_COL));
								size_t score = 0;
								for(size_t c = 0, d = 0;
								    c < originalNodePair.first.size() && d < name.size();
								    ++c, ++d)
								{
									if(name[d] == originalNodePair.first[c])
										++score;
									else if(d + 1 < name.size() &&
									        name[d + 1] == originalNodePair.first[c])
										--c;  //rewind one for dropped character
									else if(c + 1 < originalNodePair.first.size() &&
									        name[d] == originalNodePair.first[c + 1])
										--d;  //rewind one for dropped character
								}
								if(originalNodePair.first.size() == name.size())
									++score;
								__COUTVS__(2, score);
								if(score > bestScore)
								{
									bestOriginalNodeName = originalNodePair.first;
									bestScore            = score;
									__COUTVS__(2, bestOriginalNodeName);
									__COUTVS__(2, bestScore);
								}
							}  //end scoring loop for best match in originalMultinodeValues

							bool        exactMatch = (bestOriginalNodeName == name);
							bool        needToHandleArtProcessName = false;
							std::string artProcessName;

							if(exactMatch ||
							   originalMultinodeValues.find(bestOriginalNodeName) !=
							       originalMultinodeValues.end())
							{
								__COUT__ << "Populating original multinode value from '"
								         << bestOriginalNodeName << "' into '" << name
								         << ".'" << __E__;

								for(const auto& valuePair :
								    originalMultinodeValues.at(bestOriginalNodeName))
								{
									//(keep new creation time always!) if not exact match then keep new meta info and skip Comment, Author, and CreationTime
									if(!exactMatch && (valuePair.first == commentCol ||
									                   valuePair.first == authorCol ||
									                   valuePair.first == timestampCol))
									{
										__COUTT__
										    << "Not exact node name match, so keeping "
										       "default meta info for node: "
										    << name << "[" << row << "]["
										    << valuePair.first
										    << "] /= " << valuePair.second << " keep= "
										    << typeTable.tableView_
										           ->getDataView()[row][valuePair.first]
										    << __E__;
										continue;
									}

									__COUTT__ << "Customizing node: " << name << "["
									          << row << "][" << valuePair.first
									          << "] = " << valuePair.second << __E__;
									//handle special columns, otherwise normal columns in type table
									if(valuePair.first == ORIG_MAP_ART_PROC_NAME_COL)
									{
										__COUTT__ << "NEED Special art Process Name "
										             "column value: "
										          << valuePair.second << __E__;
										needToHandleArtProcessName = true;
										artProcessName             = valuePair.second;
										continue;
										artTable->tableView_->setValueAsString(
										    valuePair.second, row, artProcessNameCol);
									}
									else
										typeTable.tableView_->setValueAsString(
										    valuePair.second, row, valuePair.first);
								}
							}
							else
								__COUT__ << "Did not find '" << name
								         << "' in original value cache. Looking for "
								            "bestOriginalNodeName="
								         << bestOriginalNodeName << __E__;

							__COUTV__(exactMatch);
							if(!exactMatch)  //not exact match, so apply sibling rules
							{
								if(originalMultinodeSameSiblingValues.find(
								       nodePair.first) !=
								   originalMultinodeSameSiblingValues.end())
								{
									__COUT__ << "Applying multinode sibling same value "
									            "rules for row="
									         << row << " UID='" << name << "'" << __E__;
									for(const auto& sameValuePair :
									    originalMultinodeSameSiblingValues.at(
									        nodePair.first))
									{
										if(!sameValuePair.second.first)
											continue;
										__COUTT__
										    << "Found originalMultinodeSameSiblingValues["
										    << nodePair.first << "]["
										    << sameValuePair.first /* col */ << "] = "
										    << sameValuePair.second.first << " --> "
										    << sameValuePair.second.second << __E__;

										//handle special columns, otherwise normal columns in type table
										if(sameValuePair.first ==
										   ORIG_MAP_ART_PROC_NAME_COL)
										{
											__COUTT__ << "NEED Special art Process Name "
											             "column value: "
											          << sameValuePair.second.second
											          << __E__;
											needToHandleArtProcessName = true;
											artProcessName = sameValuePair.second.second;
											continue;
											artTable->tableView_->setValueAsString(
											    sameValuePair.second.second,
											    row,
											    artProcessNameCol);
										}
										else
											typeTable.tableView_->setValueAsString(
											    sameValuePair.second.second,
											    row,
											    sameValuePair.first);
									}  //end loop to apply multinode same sibling values
								}

								//do originalMultinodeAllSiblingEmbeddedPrinterIndex before originalMultinodeAllSiblingEmbeddedName, so that originalMultinodeAllSiblingEmbeddedName has priority
								if(originalMultinodeAllSiblingEmbeddedPrinterIndex.find(
								       nodePair.first) !=
								   originalMultinodeAllSiblingEmbeddedPrinterIndex.end())
								{
									__COUT__ << "Applying multinode sibling embbeded "
									            "printer syntax index rules for row="
									         << row << " UID='" << name
									         << "' and printer index='" << nodeNameIndex
									         << "'" << __E__;
									for(const auto& embedValuePair :
									    originalMultinodeAllSiblingEmbeddedPrinterIndex
									        .at(nodePair.first))
									{
										if(!embedValuePair.second.first ||
										   embedValuePair.second.second.size() < 2)
											continue;
										__COUTT__
										    << "Found "
										       "originalMultinodeAllSiblingEmbeddedPrinte"
										       "rIndex["
										    << nodePair.first << "]["
										    << embedValuePair.first /* col */ << "] = "
										    << embedValuePair.second.first << " --> "
										    << StringMacros::vectorToString(
										           embedValuePair.second.second)
										    << __E__;
										std::string embedValue =
										    StringMacros::vectorToString(
										        embedValuePair.second.second,
										        nodeNameIndex);
										__COUTTV__(embedValue);

										//handle special columns, otherwise normal columns in type table
										if(embedValuePair.first ==
										   ORIG_MAP_ART_PROC_NAME_COL)
										{
											__COUTT__ << "NEED Special art Process Name "
											             "column value: "
											          << embedValue << __E__;
											needToHandleArtProcessName = true;
											artProcessName             = embedValue;
											continue;
											artTable->tableView_->setValueAsString(
											    embedValue, row, artProcessNameCol);
										}
										else
											typeTable.tableView_->setValueAsString(
											    embedValue, row, embedValuePair.first);
									}  //end loop to apply multinode same sibling values
								}

								if(originalMultinodeAllSiblingEmbeddedName.find(
								       nodePair.first) !=
								   originalMultinodeAllSiblingEmbeddedName.end())
								{
									__COUT__ << "Applying multinode sibling embbeded "
									            "name rules for row="
									         << row << " UID='" << name << "'" << __E__;
									for(const auto& embedValuePair :
									    originalMultinodeAllSiblingEmbeddedName.at(
									        nodePair.first))
									{
										if(!embedValuePair.second.first ||
										   embedValuePair.second.second.size() < 2)
											continue;
										__COUTT__
										    << "Found "
										       "originalMultinodeAllSiblingEmbeddedName["
										    << nodePair.first << "]["
										    << embedValuePair.first /* col */ << "] = "
										    << embedValuePair.second.first << " --> "
										    << StringMacros::vectorToString(
										           embedValuePair.second.second)
										    << __E__;
										std::string embedValue =
										    StringMacros::vectorToString(
										        embedValuePair.second.second, name);
										__COUTTV__(embedValue);

										//handle special columns, otherwise normal columns in type table
										if(embedValuePair.first ==
										   ORIG_MAP_ART_PROC_NAME_COL)
										{
											__COUTT__ << "NEED Special art Process Name "
											             "column value: "
											          << embedValue << __E__;
											needToHandleArtProcessName = true;
											artProcessName             = embedValue;
											continue;
											artTable->tableView_->setValueAsString(
											    embedValue, row, artProcessNameCol);
										}
										else
											typeTable.tableView_->setValueAsString(
											    embedValue, row, embedValuePair.first);
									}  //end loop to apply multinode same sibling values
								}

								__COUTV__(needToHandleArtProcessName);
								if(needToHandleArtProcessName)
								{
									__COUTT__ << "Special art Process Name column value: "
									          << artProcessName << __E__;
									//need to find row or make row for art record
									std::string artRecord =
									    typeTable.tableView_->getDataView()
									        [row][typeTable.tableView_->findCol(
									            ARTDAQTableBase::colARTDAQNotReader_
									                .colLinkToArtUID_)];
									__COUTTV__(artRecord);

									const unsigned int artCommentCol =
									    artTable->tableView_->findColByType(
									        TableViewColumnInfo::TYPE_COMMENT);
									const unsigned int artAuthorCol =
									    artTable->tableView_->findColByType(
									        TableViewColumnInfo::TYPE_AUTHOR);
									const unsigned int artTimestampCol =
									    artTable->tableView_->findColByType(
									        TableViewColumnInfo::TYPE_TIMESTAMP);

									unsigned int artRow = artTable->tableView_->findRow(
									    artTable->tableView_->getColUID(),
									    artRecord,
									    0 /* offsetRow */,
									    true /* doNotThrow*/);
									__COUTTV__(artRow);
									if(artRow == TableView::INVALID)  //need to make row!
									{
										__COUTT__ << "Need to make art Process record... "
										             "artRecord="
										          << artRecord << __E__;

										//change lastArtRow to best match's art row
										{
											__COUTTV__(bestOriginalNodeName);
											const unsigned int bestMatchRow =
											    typeTable.tableView_->findRow(
											        typeTable.tableView_->getColUID(),
											        bestOriginalNodeName);
											__COUTTV__(bestMatchRow);

											std::string bestMatchArtRecord =
											    typeTable.tableView_->getDataView()
											        [bestMatchRow]
											        [typeTable.tableView_->findCol(
											            ARTDAQTableBase::
											                colARTDAQNotReader_
											                    .colLinkToArtUID_)];
											__COUTTV__(bestMatchArtRecord);

											unsigned int bestMatchArtRow =
											    artTable->tableView_->findRow(
											        artTable->tableView_->getColUID(),
											        bestMatchArtRecord,
											        0 /* offsetRow */,
											        true /* doNotThrow*/);
											__COUTTV__(bestMatchArtRow);
											if(bestMatchArtRow !=
											   TableView::
											       INVALID)  //found best match's art record
												lastArtRow =
												    bestMatchArtRow;  //use best match's art record for copy
											__COUTTV__(lastArtRow);
										}

										if(lastArtRow != TableView::INVALID)
										{
											__COUTT__ << "Copying art Process record... "
											             "from lastArtRow="
											          << lastArtRow << __E__;
											unsigned int copyRow =
											    artTable->tableView_->copyRows(
											        author,
											        *(artTable->tableView_),
											        lastArtRow,
											        1 /*srcRowsToCopy*/,
											        -1 /*destOffsetRow*/,
											        true /*generateUniqueDataColumns*/);
											artTable->tableView_->setValueAsString(
											    artRecord,
											    copyRow,
											    artTable->tableView_->getColUID());
											artRow = copyRow;

											//NOTE: changing UID and copyRows does not change author or date! So change it if not an exact match; so change it now and fill with original archive if exact match
											artTable->tableView_->setValueAsString(
											    TableViewColumnInfo::
											        DATATYPE_COMMENT_DEFAULT,
											    artRow,
											    artCommentCol);
											artTable->tableView_->setValueAsString(
											    author, artRow, artAuthorCol);
											artTable->tableView_->setValue(
											    time(0), artRow, artTimestampCol);
										}
										else
										{
											__COUTT__ << "Creating art Process record... "
											             "artRecord="
											          << artRecord << __E__;

											artRow = artTable->tableView_->addRow(
											    author,
											    true /*incrementUniqueData*/,
											    artRecord);
										}
										__COUTT__
										    << "Made art Process record... artRecord="
										    << artRecord << __E__;
									}  //end making row

									__COUTT__ << "Modify art Process record based on "
									             "sibling rules... artRecord="
									          << artRecord
									          << " artProcessName=" << artProcessName
									          << __E__;

									artTable->tableView_->setValueAsString(
									    artProcessName, artRow, artProcessNameCol);
									lastArtRow = artRow;
								}
							}  //end applying sibling value rules
							else if(
							    needToHandleArtProcessName)  //get lastArtRow for future multirecord siblings
							{
								std::string artRecord =
								    typeTable.tableView_->getDataView()
								        [row][typeTable.tableView_->findCol(
								            ARTDAQTableBase::colARTDAQNotReader_
								                .colLinkToArtUID_)];
								__COUTTV__(artRecord);
								unsigned int artRow = artTable->tableView_->findRow(
								    artTable->tableView_->getColUID(),
								    artRecord,
								    0 /* offsetRow */,
								    true /* doNotThrow*/);
								__COUTTV__(artRow);
								if(artRow !=
								   TableView::INVALID)  //found valid art record row
									lastArtRow = artRow;
							}

							__COUTTV__(lastArtRow);

							if(TTEST(1))
							{
								__COUTTV__(row);
								if(row < maxRowToDelete)
									__COUTTV__(deleteRecordMap[row]);

								__COUTTV__(typeTable
								               .tableView_  //comment
								               ->getDataView()[row][commentCol]);
								__COUTTV__(typeTable
								               .tableView_  //author
								               ->getDataView()[row][authorCol]);
								__COUTTV__(typeTable
								               .tableView_  //creation time
								               ->getDataView()[row][timestampCol]);
							}
						}  // end copy and customize row handling

						isFirst = false;
					}  // end multi-node loop
				}      // end multi-node handling
			}          // end node record loop

			{  // delete record handling
				__COUT__ << "Deleting '" << nodeTypePair.first
				         << "' records not specified..." << __E__;

				// unsigned int           row;
				std::set<unsigned int> orderedRowSet;  // need to delete in reverse order
				for(auto& deletePair : deleteRecordMap)
				{
					if(!deletePair.second)
					{
						__COUTT__ << "Row keep = " << deletePair.first << __E__;
						continue;  // only delete if true
					}

					__COUTT__ << "Row delete = " << deletePair.first << __E__;
					orderedRowSet.emplace(deletePair.first);
				}

				// delete elements in reverse order
				for(std::set<unsigned int>::reverse_iterator rit = orderedRowSet.rbegin();
				    rit != orderedRowSet.rend();
				    rit++)
					typeTable.tableView_->deleteRow(*rit);

			}  // end delete record handling

			if(TTEST(1) && artTable)
			{
				std::stringstream ss;
				artTable->tableView_->print(ss);
				__COUT_MULTI__(1, ss.str());
			}

			if(hasArtProcessName && artTable)
				artTable->tableView_
				    ->init();  // verify new art table modifications (throws runtime_errors)

			if(TTEST(1))
			{
				std::stringstream ss;
				typeTable.tableView_->print(ss);
				__COUT_MULTI__(1, ss.str());
			}

			typeTable.tableView_->init();  // verify new table (throws runtime_errors)

		}  // end node type loop

		if(TTEST(1))
		{
			{
				std::stringstream ss;
				artdaqSupervisorTable.tableView_->print(ss);
				__COUT_MULTI__(1, ss.str());
			}
			{
				std::stringstream ss;
				artdaqSubsystemTable.tableView_->print(ss);
				__COUT_MULTI__(1, ss.str());
			}
		}

		artdaqSupervisorTable.tableView_
		    ->init();  // verify new table (throws runtime_errors)
		artdaqSubsystemTable.tableView_
		    ->init();  // verify new table (throws runtime_errors)
	}
	catch(...)
	{
		__COUT__ << "Table errors while creating ARTDAQ nodes. Erasing all newly "
		            "created table versions."
		         << __E__;
		throw;  // re-throw
	}           // end catch

	__COUT__ << "Edits complete for artdaq nodes and subsystems.. now save and activate "
	            "groups, and update aliases!"
	         << __E__;

	TableGroupKey newConfigurationGroupKey;
	if(0)  //keep for debugging save process
	{
		__SS__ << "DEBUG blocking save!" << __E__;
		__SS_THROW__;
	}
	{
		std::string localAccumulatedWarnings;
		configGroupEdit.saveChanges(configGroupEdit.originalGroupName_,
		                            newConfigurationGroupKey,
		                            nullptr /*foundEquivalentGroupKey*/,
		                            true /*activateNewGroup*/,
		                            true /*updateGroupAliases*/,
		                            true /*updateTableAliases*/,
		                            nullptr /*newBackboneKey*/,
		                            nullptr /*foundEquivalentBackboneKey*/,
		                            &localAccumulatedWarnings);
	}

}  // end setAndActivateARTDAQSystem()

//==============================================================================
int ARTDAQTableBase::getSubsytemId(ConfigurationTree subsystemNode)
{
	// using row forces a unique ID from 0 to rows-1
	//	note: default no defined subsystem link to id=1; so add 2

	return subsystemNode.getNodeRow() + 2;
}  // end getSubsytemId()

//==============================================================================
/// add whitespace so comments lineup in fcl for nicer presentation
void ARTDAQTableBase::addCommentWhitespace(std::ostream& os, size_t lineLength)
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
std::string ARTDAQTableBase::getStructureAsJSON(
    const ConfigurationManager* /* configManager */)
{
	if(fclMap_.size() == 0)  //assume was not generated (not first )
		genFlatFHiCL();
	std::stringstream oss;

	oss << "{" << __E__;

	if(fclMap_.size() > 1)
	{
		// Multiple types - keep grouped structure
		bool firstType = true;
		for(const auto& typePairMap : fclMap_)
		{
			if(!firstType)
				oss << ",";
			oss << "\t\"" << getTypeString(typePairMap.first) << "\": {" << __E__;

			bool firstEntry = true;
			for(const auto& fclPair : typePairMap.second)
			{
				if(!firstEntry)
					oss << ",";
				oss << "\t\t\"" << fclPair.first << "\": \""
				    << StringMacros::escapeJSONStringEntities(fclPair.second) << "\""
				    << __E__;
				firstEntry = false;
			}

			oss << "\t}" << __E__;
			firstType = false;
		}
	}
	else
	{
		// Single type (normal case) - flat structure
		bool firstEntry = true;
		for(const auto& typePairMap : fclMap_)
		{
			for(const auto& fclPair : typePairMap.second)
			{
				if(!firstEntry)
					oss << ",";
				oss << "\t\"" << fclPair.first << "\": \""
				    << StringMacros::escapeJSONStringEntities(fclPair.second) << "\""
				    << __E__;
				firstEntry = false;
			}
		}
	}

	oss << "}" << __E__;

	return oss.str();
}  //end getStructureAsJSON()

//==============================================================================
/// getBootFileContent
///		Generate boot.txt content as a string for a specific supervisor row
std::string ARTDAQTableBase::getBootFileContent(ConfigurationTree artdaqSupervisorNode,
                                                size_t            maxFragmentSizeBytes,
                                                size_t            routingTimeoutMs,
                                                size_t            routingRetryCount,
                                                ProgressBar*      progressBar)
{
	if(artdaqSupervisorNode.isDisconnected())
	{
		__SS__ << "ARTDAQ Supervisor node is disconnected while generating boot.txt "
		       << "content." << __E__;
		__SS_THROW__;
	}

	const ARTDAQInfo& info = extractARTDAQInfo(artdaqSupervisorNode,
	                                           false /*getStatusFalseNodes*/,
	                                           false /*doWriteFHiCL*/,
	                                           maxFragmentSizeBytes,
	                                           routingTimeoutMs,
	                                           routingRetryCount,
	                                           progressBar);

	int debugLevel =
	    artdaqSupervisorNode.getNode(colARTDAQSupervisor_.colDAQInterfaceDebugLevel_)
	        .getValue<int>();
	std::string setupScript =
	    artdaqSupervisorNode.getNode(colARTDAQSupervisor_.colDAQSetupScript_).getValue();

	return getBootFileContentFromInfo(info, setupScript, debugLevel);
}  //end getBootFileContent()

//==============================================================================
/// getBootFileContentFromInfo
///		Generate boot.txt content as a string
std::string ARTDAQTableBase::getBootFileContentFromInfo(const ARTDAQInfo&  info,
                                                        const std::string& setupScript,
                                                        int                debugLevel)
{
	std::stringstream o;

	o << "DAQ setup script: " << setupScript << std::endl;
	o << "debug level: " << debugLevel << std::endl;
	o << std::endl;

	if(info.subsystems.size() > 1)
	{
		for(auto& ss : info.subsystems)
		{
			if(ss.first == 0)
				continue;
			o << "Subsystem id: " << ss.first << std::endl;
			if(ss.second.destination != 0)
			{
				o << "Subsystem destination: " << ss.second.destination << std::endl;
			}
			for(auto& sss : ss.second.sources)
			{
				o << "Subsystem source: " << sss << std::endl;
			}
			if(ss.second.eventMode)
			{
				o << "Subsystem fragmentMode: False" << std::endl;
			}
			o << std::endl;
		}
	}

	for(auto& builder : info.processes.at(ARTDAQAppType::EventBuilder))
	{
		o << "EventBuilder host: " << builder.hostname << std::endl;
		o << "EventBuilder label: " << builder.label << std::endl;
		if(builder.subsystem != 1)
		{
			o << "EventBuilder subsystem: " << builder.subsystem << std::endl;
		}
		if(builder.allowed_processors != "")
		{
			o << "EventBuilder allowed_processors: " << builder.allowed_processors
			  << std::endl;
		}
		o << std::endl;
	}

	for(auto& logger : info.processes.at(ARTDAQAppType::DataLogger))
	{
		o << "DataLogger host: " << logger.hostname << std::endl;
		o << "DataLogger label: " << logger.label << std::endl;
		if(logger.subsystem != 1)
		{
			o << "DataLogger subsystem: " << logger.subsystem << std::endl;
		}
		if(logger.allowed_processors != "")
		{
			o << "DataLogger allowed_processors: " << logger.allowed_processors
			  << std::endl;
		}
		o << std::endl;
	}

	for(auto& dispatcher : info.processes.at(ARTDAQAppType::Dispatcher))
	{
		o << "Dispatcher host: " << dispatcher.hostname << std::endl;
		o << "Dispatcher label: " << dispatcher.label << std::endl;
		if(dispatcher.port != 0)
        {
            o << "Dispatcher port: " << dispatcher.port << std::endl;
		}
		if(dispatcher.subsystem != 1)
		{
			o << "Dispatcher subsystem: " << dispatcher.subsystem << std::endl;
		}
		if(dispatcher.allowed_processors != "")
		{
			o << "Dispatcher allowed_processors: " << dispatcher.allowed_processors
			  << std::endl;
		}
		o << std::endl;
	}

	for(auto& rm : info.processes.at(ARTDAQAppType::RoutingManager))
	{
		o << "RoutingManager host: " << rm.hostname << std::endl;
		o << "RoutingManager label: " << rm.label << std::endl;
		if(rm.subsystem != 1)
		{
			o << "RoutingManager subsystem: " << rm.subsystem << std::endl;
		}
		if(rm.allowed_processors != "")
		{
			o << "RoutingManager allowed_processors: " << rm.allowed_processors
			  << std::endl;
		}
		o << std::endl;
	}

	return o.str();
}  //end getBootFileContentFromInfo()
