#include "otsdaq/TablePlugins/ARTDAQTableBase/ARTDAQTableBase.h"

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

const std::string 	ARTDAQTableBase::ARTDAQ_FCL_PATH = std::string(__ENV__("USER_DATA")) + "/" + "ARTDAQConfigurations/";

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

//Note!!!! processTypes_ must be instantiate after the static artdaq table names (to construct map in constructor in .h)
ARTDAQTableBase::ProcessTypes 			ARTDAQTableBase::processTypes_;

// clang-format on

//==============================================================================
// TableBase
//	If a valid string pointer is passed in accumulatedExceptions
//	then allowIllegalColumns is set for InfoReader
//	If accumulatedExceptions pointer = 0, then illegal columns throw std::runtime_error
// exception
ARTDAQTableBase::ARTDAQTableBase(std::string tableName, std::string* accumulatedExceptions /* =0 */) : TableBase(tableName, accumulatedExceptions)
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
// ARTDAQTableBase
//	Default constructor should never be used because table type is lost
ARTDAQTableBase::ARTDAQTableBase(void) : TableBase("ARTDAQTableBase")
{
	__SS__ << "Should not call void constructor, table type is lost!" << __E__;
	__SS_THROW__;
}  // end illegal default constructor()

//==============================================================================
ARTDAQTableBase::~ARTDAQTableBase(void) {}  // end destructor()

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
	__SS__ << "Illegal translation attempt for type '" << (unsigned int)type << "'" << __E__;
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
		if((uid[i] >= 'a' && uid[i] <= 'z') || (uid[i] >= 'A' && uid[i] <= 'Z') || (uid[i] >= '0' && uid[i] <= '9'))  // only allow alpha numeric in file name
			filename += uid[i];

	filename += ".fcl";

	//__COUT__ << "fcl: " << filename << __E__;

	return filename;
}  // end getFHICLFilename()

//==============================================================================
std::string ARTDAQTableBase::getFlatFHICLFilename(ARTDAQAppType type, const std::string& name)
{
	//__COUT__ << "Type: " << getTypeString(type) << " Name: " << name
	//         << __E__;
	std::string filename = ARTDAQ_FCL_PATH + getTypeString(type) + "-";
	std::string uid      = name;
	for(unsigned int i = 0; i < uid.size(); ++i)
		if((uid[i] >= 'a' && uid[i] <= 'z') || (uid[i] >= 'A' && uid[i] <= 'Z') || (uid[i] >= '0' && uid[i] <= '9'))  // only allow alpha numeric in file name
			filename += uid[i];

	filename += "_flattened.fcl";

	//__COUT__ << "fcl: " << filename << __E__;

	return filename;
}  // end getFlatFHICLFilename()

//==============================================================================
void ARTDAQTableBase::flattenFHICL(ARTDAQAppType type, const std::string& name)
{
	std::chrono::steady_clock::time_point startClock = std::chrono::steady_clock::now();
	//__COUT__ << "flattenFHICL()" << __ENV__("FHICL_FILE_PATH") << __E__;

	std::string inFile  = getFHICLFilename(type, name);
	std::string outFile = getFlatFHICLFilename(type, name);

	//__COUTV__(inFile);
	//__COUTV__(outFile);

	cet::filepath_lookup_nonabsolute policy("FHICL_FILE_PATH");
	fhicl::ParameterSet pset;

	try
	{
		TLOG(TLVL_INFO) << "parsing document: " << inFile;
		// tbl = fhicl::parse_document(inFile, policy);
		// pset = fhicl::ParameterSet::make(tbl);
		pset = fhicl::ParameterSet::make(inFile, policy);
		TLOG(TLVL_TRACE) << "document: " << inFile << " parsed";
		TLOG(TLVL_TRACE) << "got pset from table:";

		std::ofstream ofs{outFile};
		ofs << pset.to_indented_string(0);  // , fhicl::detail::print_mode::annotated); // Only really useful for debugging
		__COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__ << name << " Flatten Clock time = " << artdaq::TimeUtils::GetElapsedTime(startClock) << __E__; 
	}
	catch(cet::exception const& e)
	{
		__SS__ << "Failed to parse fhicl: " << e.what() << __E__;
		__SS_THROW__;
	}
}  // end flattenFHICL()

//==============================================================================
// insertParameters
//	Inserts parameters in FHiCL outputs stream.
//
// 	onlyInsertAtTableParameters allows @table:: parameters only,
//	so that calling code can do two passes (i.e. top of fcl block, @table:: only,
//	and bottom of fcl block, ignore/skip @table:: as default behavior).
void ARTDAQTableBase::insertParameters(std::ostream&      out,
                                       std::string&       tabStr,
                                       std::string&       commentStr,
                                       ConfigurationTree  parameterGroupLink,
                                       const std::string& parameterPreamble,
                                       bool               onlyInsertAtTableParameters /*=false*/,
                                       bool               includeAtTableParameters /*=false*/)
{
	// skip if link is disconnected
	if(!parameterGroupLink.isDisconnected())
	{
		///////////////////////
		auto otherParameters = parameterGroupLink.getChildren();

		std::string key;
		//__COUTV__(otherParameters.size());
		for(auto& parameter : otherParameters)
		{
			key = parameter.second.getNode(parameterPreamble + "Key").getValue();

			// handle special keyword @table:: (which imports full tables, usually as
			// defaults)
			if(key.find("@table::") != std::string::npos)
			{
				// include @table::
				if(onlyInsertAtTableParameters || includeAtTableParameters)
				{
					if(!parameter.second.status())
						PUSHCOMMENT;

					OUT << key;

					// skip connecting : if special keywords found
					OUT << parameter.second.getNode(parameterPreamble + "Value").getValue() << "\n";

					if(!parameter.second.status())
						POPCOMMENT;
				}
				// else skip it

				continue;
			}
			// else NOT @table:: keyword parameter

			if(onlyInsertAtTableParameters)
				continue;  // skip all other types

			if(!parameter.second.status())
				PUSHCOMMENT;

			OUT << key;

			// skip connecting : if special keywords found
			if(key.find("#include") == std::string::npos)
				OUT << ":";
			OUT << parameter.second.getNode(parameterPreamble + "Value").getValue() << "\n";

			if(!parameter.second.status())
				POPCOMMENT;
		}
	}
	// else
	//	__COUT__ << "No parameters found" << __E__;

}  // end insertParameters()

//==============================================================================
// insertModuleType
//	Inserts module type field, with consideration for @table::
std::string ARTDAQTableBase::insertModuleType(std::ostream& out, std::string& tabStr, std::string& commentStr, ConfigurationTree moduleTypeNode)
{
	std::string value = moduleTypeNode.getValue();

	OUT;
	if(value.find("@table::") == std::string::npos)
		out << "module_type: ";
	out << value << "\n";

	return value;
}  // end insertModuleType()

//==============================================================================
// insertMetricsBlock
void ARTDAQTableBase::insertMetricsBlock(std::ostream& out, std::string& tabStr, std::string& commentStr, ConfigurationTree daqNode)
{
	OUT << "\n\nmetrics: {\n";

	PUSHTAB;
	auto metricsGroup = daqNode.getNode("daqMetricsLink");
	if(!metricsGroup.isDisconnected())
	{
		auto metrics = metricsGroup.getChildren();

		for(auto& metric : metrics)
		{
			if(!metric.second.status())
				PUSHCOMMENT;

			OUT << metric.second.getNode("metricKey").getValue() << ": {\n";
			PUSHTAB;

			OUT << "metricPluginType: " << metric.second.getNode("metricPluginType").getValue() << "\n";
			OUT << "level: " << metric.second.getNode("metricLevel").getValue() << "\n";

			auto metricParametersGroup = metric.second.getNode("metricParametersLink");
			if(!metricParametersGroup.isDisconnected())
			{
				auto metricParameters = metricParametersGroup.getChildren();
				for(auto& metricParameter : metricParameters)
				{
					if(!metricParameter.second.status())
						PUSHCOMMENT;

					OUT << metricParameter.second.getNode("metricParameterKey").getValue() << ": "
					    << metricParameter.second.getNode("metricParameterValue").getValue() << "\n";

					if(!metricParameter.second.status())
						POPCOMMENT;
				}
			}
			POPTAB;
			OUT << "}\n\n";  // end metric

			if(!metric.second.status())
				POPCOMMENT;
		}
	}
	POPTAB;
	OUT << "}\n\n";  // end metrics
}  // end insertMetricsBlock()

//==============================================================================
void ARTDAQTableBase::outputBoardReaderFHICL(const ConfigurationTree& boardReaderNode,
                                             size_t /*maxFragmentSizeBytes */ /* = DEFAULT_MAX_FRAGMENT_SIZE */,
                                             size_t routingTimeoutMs /* = DEFAULT_ROUTING_TIMEOUT_MS */,
                                             size_t routingRetryCount /* = DEFAULT_ROUTING_RETRY_COUNT */)
{
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

	std::string filename = getFHICLFilename(ARTDAQAppType::BoardReader, boardReaderNode.getValue());

	/////////////////////////
	// generate xdaq run parameter file
	std::fstream out;

	std::string tabStr     = "";
	std::string commentStr = "";

	out.open(filename, std::fstream::out | std::fstream::trunc);
	if(out.fail())
	{
		__SS__ << "Failed to open ARTDAQ BoardReader fcl file: " << filename << __E__;
		__SS_THROW__;
	}

	//--------------------------------------
	// header
	OUT << "###########################################################" << __E__;
	OUT << "#" << __E__;
	OUT << "# artdaq reader fcl configuration file produced by otsdaq." << __E__;
	OUT << "# 	Creation time:           \t" << StringMacros::getTimestampString() << __E__;
	OUT << "# 	Original filename:       \t" << filename << __E__;
	OUT << "#	otsdaq-ARTDAQ Reader UID:\t" << boardReaderNode.getValue() << __E__;
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
		//__COUT__ << "Ignoring error, assume this a valid UID node." << __E__;
		// error is expected here for UIDs.. so just ignore
		// this check is valuable if source node is a unique-Link node, rather than UID
	}

	//--------------------------------------
	// handle preamble parameters
	//_COUT__ << "Inserting preamble parameters..." << __E__;
	insertParameters(out,
	                 tabStr,
	                 commentStr,
	                 boardReaderNode.getNode("preambleParametersLink"),
	                 "daqParameter" /*parameterType*/,
	                 false /*onlyInsertAtTableParameters*/,
	                 true /*includeAtTableParameters*/);

	//--------------------------------------
	// handle daq
	OUT << "daq: {\n";

	// fragment_receiver
	PUSHTAB;
	OUT << "fragment_receiver: {\n";

	PUSHTAB;
	{
		// plugin type and fragment data-type
		OUT << "generator"
		    << ": " << boardReaderNode.getNode("daqGeneratorPluginType").getValue() << ("\t #daq generator plug-in type") << "\n";
		OUT << "fragment_type"
		    << ": " << boardReaderNode.getNode("daqGeneratorFragmentType").getValue() << ("\t #generator data fragment type") << "\n\n";

		// shared and unique parameters
		auto parametersLink = boardReaderNode.getNode("daqParametersLink");
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

				auto comment = parameter.second.getNode(TableViewColumnInfo::COL_NAME_COMMENT);
				OUT << parameter.second.getNode("daqParameterKey").getValue() << ": " << parameter.second.getNode("daqParameterValue").getValue()
				    << (comment.isDefaultValue() ? "" : ("\t # " + comment.getValue())) << "\n";

				if(!parameter.second.status())
					POPCOMMENT;
			}
		}
		OUT << "\n";  // end daq board reader parameters
	}

	OUT << "destinations: {\n";

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
		OUT << "use_routing_manager: true\n";
		OUT << "routing_manager_hostname: \"" << info_.subsystems[readerSubsystemID].routingManagerHost << "\"\n";
		OUT << "table_update_port: 0\n";
		OUT << "table_update_address: \"0.0.0.0\"\n";
		OUT << "table_update_multicast_interface: \"0.0.0.0\"\n";
		OUT << "table_acknowledge_port : 0\n";
		OUT << "routing_timeout_ms: " << routingTimeoutMs << "\n";
		OUT << "routing_retry_count: " << routingRetryCount << "\n";
	}
	else
	{
		OUT << "use_routing_manager: false\n";
	}

	POPTAB;
	OUT << "}\n";  // end routing_table_config

	POPTAB;
	OUT << "}\n\n";  // end fragment_receiver

	insertMetricsBlock(OUT, tabStr, commentStr, boardReaderNode);

	POPTAB;
	OUT << "}\n\n";  // end daq

	//--------------------------------------
	// handle ALL add-on parameters
	//__COUT__ << "Inserting add-on parameters" << __E__;
	insertParameters(out,
	                 tabStr,
	                 commentStr,
	                 boardReaderNode.getNode("addOnParametersLink"),
	                 "daqParameter" /*parameterType*/,
	                 false /*onlyInsertAtTableParameters*/,
	                 true /*includeAtTableParameters*/);

	out.close();
}  // end outputReaderFHICL()

//==============================================================================
// outputDataReceiverFHICL
//	Note: currently selfRank and selfPort are unused by artdaq fcl
void ARTDAQTableBase::outputDataReceiverFHICL(const ConfigurationTree& receiverNode,
                                              ARTDAQAppType            appType,
                                              size_t /*maxFragmentSizeBytes */ /* = DEFAULT_MAX_FRAGMENT_SIZE */,
                                              size_t routingTimeoutMs /* = DEFAULT_ROUTING_TIMEOUT_MS */,
                                              size_t routingRetryCount /* = DEFAULT_ROUTING_RETRY_COUNT */)
{
	std::string filename = getFHICLFilename(appType, receiverNode.getValue());

	/////////////////////////
	// generate xdaq run parameter file
	std::fstream out;

	std::string tabStr     = "";
	std::string commentStr = "";

	out.open(filename, std::fstream::out | std::fstream::trunc);
	if(out.fail())
	{
		__SS__ << "Failed to open ARTDAQ fcl file: " << filename << __E__;
		__SS_THROW__;
	}

	//--------------------------------------
	// header
	OUT << "###########################################################" << __E__;
	OUT << "#" << __E__;
	OUT << "# artdaq " << getTypeString(appType) << " fcl configuration file produced by otsdaq." << __E__;
	OUT << "# 	Creation time:                  \t" << StringMacros::getTimestampString() << __E__;
	OUT << "# 	Original filename:              \t" << filename << __E__;
	OUT << "#	otsdaq-ARTDAQ " << getTypeString(appType) << " UID:\t" << receiverNode.getValue() << __E__;
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
	// handle preamble parameters
	//_COUT__ << "Inserting preamble parameters..." << __E__;
	insertParameters(out,
	                 tabStr,
	                 commentStr,
	                 receiverNode.getNode("preambleParametersLink"),
	                 "daqParameter" /*parameterType*/,
	                 false /*onlyInsertAtTableParameters*/,
	                 true /*includeAtTableParameters*/);

	//--------------------------------------
	// handle daq
	//__COUT__ << "Generating daq block..." << __E__;
	auto daq = receiverNode.getNode("daqLink");
	if(!daq.isDisconnected())
	{
		///////////////////////
		OUT << "daq: {\n";

		PUSHTAB;
		if(appType == ARTDAQAppType::EventBuilder)
		{
			// event_builder
			OUT << "event_builder: {\n";
		}
		else
		{
			// both datalogger and dispatcher use aggregator for now
			OUT << "aggregator: {\n";
		}

		PUSHTAB;

		if(appType == ARTDAQAppType::DataLogger)
		{
			OUT << "is_datalogger: true\n";
		}
		else if(appType == ARTDAQAppType::Dispatcher)
		{
			OUT << "is_dispatcher: true\n";
		}

		//--------------------------------------
		// handle ALL daq parameters
		//__COUT__ << "Inserting DAQ Parameters..." << __E__;
		insertParameters(out,
		                 tabStr,
		                 commentStr,
		                 daq.getNode("daqParametersLink"),
		                 "daqParameter" /*parameterType*/,
		                 false /*onlyInsertAtTableParameters*/,
		                 true /*includeAtTableParameters*/);

		if(appType == ARTDAQAppType::EventBuilder)
		{
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
				OUT << "use_routing_manager: true\n";
				OUT << "routing_manager_hostname: \"" << info_.subsystems[builderSubsystemID].routingManagerHost << "\"\n";
				OUT << "routing_token_port: 0\n";
			}
			else
			{
				OUT << "use_routing_manager: false\n";
			}
			POPTAB;
			OUT << "}\n";  // end routing_token_config
		}

		//__COUT__ << "Adding sources placeholder" << __E__;
		OUT << "sources: {\n"
		    << "}\n\n";  // end sources

		POPTAB;
		OUT << "}\n\n";  // end event builder

		insertMetricsBlock(OUT, tabStr, commentStr, daq);

		POPTAB;
		OUT << "}\n\n";  // end daq
	}

	//--------------------------------------
	// handle art
	//__COUT__ << "Filling art block..." << __E__;
	auto art = receiverNode.getNode("artLink");
	if(!art.isDisconnected())
	{
		OUT << "art: {\n";

		PUSHTAB;

		insertArtProcessBlock(out, tabStr, commentStr, art, receiverNode.getNode("SubsystemLink"), routingTimeoutMs, routingRetryCount);

		POPTAB;
		OUT << "}\n\n";  // end art
	}

	//--------------------------------------
	// handle ALL add-on parameters
	//__COUT__ << "Inserting add-on parameters" << __E__;
	insertParameters(out,
	                 tabStr,
	                 commentStr,
	                 receiverNode.getNode("addOnParametersLink"),
	                 "daqParameter" /*parameterType*/,
	                 false /*onlyInsertAtTableParameters*/,
	                 true /*includeAtTableParameters*/);

	//__COUT__ << "outputDataReceiverFHICL DONE" << __E__;
	out.close();
}  // end outputDataReceiverFHICL()

//==============================================================================
// outputOnlineMonitorFHICL
//	Note: currently selfRank and selfPort are unused by artdaq fcl
void ARTDAQTableBase::outputOnlineMonitorFHICL(const ConfigurationTree& monitorNode)
{
	std::string filename = getFHICLFilename(ARTDAQAppType::Monitor, monitorNode.getValue());

	/////////////////////////
	// generate xdaq run parameter file
	std::fstream out;

	std::string tabStr     = "";
	std::string commentStr = "";

	out.open(filename, std::fstream::out | std::fstream::trunc);
	if(out.fail())
	{
		__SS__ << "Failed to open ARTDAQ fcl file: " << filename << __E__;
		__SS_THROW__;
	}

	//--------------------------------------
	// header
	OUT << "###########################################################" << __E__;
	OUT << "#" << __E__;
	OUT << "# artdaq " << getTypeString(ARTDAQAppType::Monitor) << " fcl configuration file produced by otsdaq." << __E__;
	OUT << "# 	Creation time:                  \t" << StringMacros::getTimestampString() << __E__;
	OUT << "# 	Original filename:              \t" << filename << __E__;
	OUT << "#	otsdaq-ARTDAQ " << getTypeString(ARTDAQAppType::Monitor) << " UID:\t" << monitorNode.getValue() << __E__;
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
		//__COUT__ << "Ignoring error, assume this a valid UID node." << __E__;
		// error is expected here for UIDs.. so just ignore
		// this check is valuable if source node is a unique-Link node, rather than UID
	}

	//--------------------------------------
	// handle preamble parameters
	//_COUT__ << "Inserting preamble parameters..." << __E__;
	insertParameters(out,
	                 tabStr,
	                 commentStr,
	                 monitorNode.getNode("preambleParametersLink"),
	                 "daqParameter" /*parameterType*/,
	                 false /*onlyInsertAtTableParameters*/,
	                 true /*includeAtTableParameters*/);

	//--------------------------------------
	// handle art
	//__COUT__ << "Filling art block..." << __E__;
	auto art = monitorNode.getNode("artLink");
	if(!art.isDisconnected())
	{
		insertArtProcessBlock(out, tabStr, commentStr, art);
		OUT << "services.message: { " << artdaq::generateMessageFacilityConfiguration(mf::GetApplicationName().c_str(), true, false) << "}\n";
		OUT << "services.message.destinations.file: {type: \"GenFile\" threshold: \"INFO\" seperator: \"-\""
		    << " pattern: \"" << monitorNode.getValue() << "-%?H%t-%p.log"
		    << "\""
		    << " timestamp_pattern: \"%Y%m%d%H%M%S\""
		    << " directory: \"" << __ENV__("OTSDAQ_LOG_ROOT") << "/" << monitorNode.getValue() << "\""
		    << " append : false }\n";
	}

	auto dispatcherLink = monitorNode.getNode("dispatcherLink");
	if(!dispatcherLink.isDisconnected())
	{
		std::string monitorHost    = monitorNode.getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_HOSTNAME).getValueWithDefault("localhost");
		std::string dispatcherHost = dispatcherLink.getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_HOSTNAME).getValueWithDefault("localhost");
		OUT << "source.dispatcherHost: \"" << dispatcherHost << "\"\n";
		int dispatcherPort = dispatcherLink.getNode("DispatcherPort").getValue<int>();
		OUT << "source.dispatcherPort: " << dispatcherPort << "\n";
		OUT << "source.commanderPluginType: xmlrpc\n";

		int om_rank        = monitorNode.getNode("MonitorID").getValue<int>();
		int disp_fake_rank = dispatcherLink.getNode("DispatcherID").getValueWithDefault<int>(200);

		size_t      max_fragment_size    = monitorNode.getNode("max_fragment_size_words").getValueWithDefault(0x100000);
		std::string transfer_plugin_type = monitorNode.getNode("transfer_plugin_type").getValueWithDefault("Autodetect");

		OUT << "TransferPluginConfig: {\n";
		PUSHTAB;
		OUT << "transferPluginType: " << transfer_plugin_type << "\n";
		OUT << "host_map: [{ rank: " << disp_fake_rank << " host: \"" << dispatcherHost << "\"}, { rank: " << om_rank << " host: \"" << monitorHost << "\"}]\n";
		OUT << "max_fragment_size_words: " << max_fragment_size << "\n";
		OUT << "source_rank: " << disp_fake_rank << "\n";
		OUT << "destination_rank: " << om_rank << "\n";
		OUT << "unique_label: " << monitorNode.getValue() << "_to_" << dispatcherLink.getValue() << "\n";
		POPTAB;
		OUT << "}\n";
		OUT << "source.transfer_plugin: @local::TransferPluginConfig \n";
		auto dispatcherArt = monitorNode.getNode("dispatcherArtLink");
		if(!dispatcherArt.isDisconnected())
		{
			OUT << "source.dispatcher_config: {\n";

			PUSHTAB;

			OUT << "path: " << monitorNode.getNode("dispatcher_path").getValue() << "\n";
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

					OUT << "name: " << filterPath.second.getNode("Name").getValue() << " ";
					OUT << "path: " << filterPath.second.getNode("Path").getValue() << " ";

					OUT << "}\n";
					if(!filterPath.second.status())
						POPCOMMENT;
					first = false;
				}
			}

			POPTAB;

			OUT << "]\n";
			OUT << "unique_label: " << monitorNode.getValue() << "\n";
			insertArtProcessBlock(out, tabStr, commentStr, dispatcherArt);

			POPTAB;
			OUT << "}\n\n";  // end art
		}
	}

	//--------------------------------------
	// handle ALL add-on parameters
	//__COUT__ << "Inserting add-on parameters" << __E__;
	insertParameters(out,
	                 tabStr,
	                 commentStr,
	                 monitorNode.getNode("addOnParametersLink"),
	                 "daqParameter" /*parameterType*/,
	                 false /*onlyInsertAtTableParameters*/,
	                 true /*includeAtTableParameters*/);

	//__COUT__ << "outputDataReceiverFHICL DONE" << __E__;
	out.close();
}  // end outputDataReceiverFHICL()

//==============================================================================
// insertArtProcessBlock
//	Note: currently selfRank and selfPort are unused by artdaq fcl
void ARTDAQTableBase::insertArtProcessBlock(std::ostream&     out,
                                            std::string&      tabStr,
                                            std::string&      commentStr,
                                            ConfigurationTree art,
                                            ConfigurationTree subsystemLink,
                                            size_t            routingTimeoutMs,
                                            size_t            routingRetryCount)
{
	//--------------------------------------
	// handle services
	//__COUT__ << "Filling art.services" << __E__;
	auto services = art.getNode("servicesLink");
	if(!services.isDisconnected())
	{
		OUT << "services: {\n";

		PUSHTAB;

		//--------------------------------------
		// handle services @table:: parameters
		insertParameters(out,
		                 tabStr,
		                 commentStr,
		                 services.getNode("ServicesParametersLink"),
		                 "daqParameter" /*parameterType*/,
		                 true /*onlyInsertAtTableParameters*/,
		                 false /*includeAtTableParameters*/);

		OUT << "ArtdaqSharedMemoryServiceInterface: { service_provider: ArtdaqSharedMemoryService \n";

		OUT << "waiting_time: " << services.getNode("sharedMemoryWaitingTime").getValue() << "\n";
		OUT << "resume_after_timeout: " << (services.getNode("sharedMemoryResumeAfterTimeout").getValue<bool>() ? "true" : "false") << "\n";
		OUT << "}\n\n";

		OUT << "ArtdaqFragmentNamingServiceInterface: { service_provider: ArtdaqFragmentNamingService helper_plugin: "
		    << (services.getNode("fragmentNamingServiceProvider").getValue<std::string>()) << "}\n\n";

		//--------------------------------------
		// handle services NOT @table:: parameters
		insertParameters(out,
		                 tabStr,
		                 commentStr,
		                 services.getNode("ServicesParametersLink"),
		                 "daqParameter" /*parameterType*/,
		                 false /*onlyInsertAtTableParameters*/,
		                 false /*includeAtTableParameters*/);

		POPTAB;
		OUT << "}\n\n";  // end services
	}

	//--------------------------------------
	// handle outputs
	//__COUT__ << "Filling art.outputs" << __E__;
	auto outputs = art.getNode("outputsLink");
	if(!outputs.isDisconnected())
	{
		OUT << "outputs: {\n";

		PUSHTAB;

		auto outputPlugins = outputs.getChildren();
		for(auto& outputPlugin : outputPlugins)
		{
			if(!outputPlugin.second.status())
				PUSHCOMMENT;

			OUT << outputPlugin.second.getNode("outputKey").getValue() << ": {\n";
			PUSHTAB;

			std::string moduleType = insertModuleType(out, tabStr, commentStr, outputPlugin.second.getNode("outputModuleType"));

			//--------------------------------------
			// handle ALL output parameters
			insertParameters(out,
			                 tabStr,
			                 commentStr,
			                 outputPlugin.second.getNode("outputModuleParameterLink"),
			                 "outputParameter" /*parameterType*/,
			                 false /*onlyInsertAtTableParameters*/,
			                 true /*includeAtTableParameters*/);

			if(outputPlugin.second.getNode("outputModuleType").getValue() == "BinaryNetOutput" ||
			   outputPlugin.second.getNode("outputModuleType").getValue() == "RootNetOutput")
			{
				OUT << "destinations: {\n";
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
					OUT << "use_routing_manager: true\n";
					OUT << "routing_manager_hostname: \"" << info_.subsystems[destinationSubsystemID].routingManagerHost << "\"\n";
					OUT << "table_update_port: 0\n";
					OUT << "table_update_address: \"0.0.0.0\"\n";
					OUT << "table_update_multicast_interface: \"0.0.0.0\"\n";
					OUT << "table_acknowledge_port : 0\n";
					OUT << "routing_timeout_ms: " << routingTimeoutMs << "\n";
					OUT << "routing_retry_count: " << routingRetryCount << "\n";
				}
				else
				{
					OUT << "use_routing_manager: false\n";
				}

				if(outputPlugin.second.getNode("outputModuleType").getValue() == "RootNetOutput")
				{
					info_.subsystems[mySubsystemID].eventMode = true;
				}

				POPTAB;
				OUT << "}\n";  // end routing_table_config
			}
			if(outputPlugin.second.getNode("outputModuleType").getValue() == "TransferOutput")
			{
				OUT << "transfer_plugin: @local::TransferPluginConfig \n";
			}

			POPTAB;
			OUT << "}\n\n";  // end output module

			if(!outputPlugin.second.status())
				POPCOMMENT;
		}

		POPTAB;
		OUT << "}\n\n";  // end outputs
	}

	//--------------------------------------
	// handle physics
	//__COUT__ << "Filling art.physics" << __E__;
	auto physics = art.getNode("physicsLink");
	if(!physics.isDisconnected())
	{
		///////////////////////
		OUT << "physics: {\n";

		PUSHTAB;

		//--------------------------------------
		// handle only @table:: physics parameters
		insertParameters(out,
		                 tabStr,
		                 commentStr,
		                 physics.getNode("physicsOtherParametersLink"),
		                 "physicsParameter" /*parameterType*/,
		                 true /*onlyInsertAtTableParameters*/,
		                 false /*includeAtTableParameters*/);

		auto analyzers = physics.getNode("analyzersLink");
		if(!analyzers.isDisconnected())
		{
			///////////////////////
			OUT << "analyzers: {\n";

			PUSHTAB;

			auto modules = analyzers.getChildren();
			for(auto& module : modules)
			{
				if(!module.second.status())
					PUSHCOMMENT;

				//--------------------------------------
				// handle only @table:: analyzer parameters
				insertParameters(out,
				                 tabStr,
				                 commentStr,
				                 module.second.getNode("analyzerModuleParameterLink"),
				                 "analyzerParameter" /*parameterType*/,
				                 true /*onlyInsertAtTableParameters*/,
				                 false /*includeAtTableParameters*/);

				OUT << module.second.getNode("analyzerKey").getValue() << ": {\n";
				PUSHTAB;
				insertModuleType(out, tabStr, commentStr, module.second.getNode("analyzerModuleType"));

				//--------------------------------------
				// handle NOT @table:: producer parameters
				insertParameters(out,
				                 tabStr,
				                 commentStr,
				                 module.second.getNode("analyzerModuleParameterLink"),
				                 "analyzerParameter" /*parameterType*/,
				                 false /*onlyInsertAtTableParameters*/,
				                 false /*includeAtTableParameters*/);

				POPTAB;
				OUT << "}\n\n";  // end analyzer module

				if(!module.second.status())
					POPCOMMENT;
			}
			POPTAB;
			OUT << "}\n\n";  // end analyzer
		}

		auto producers = physics.getNode("producersLink");
		if(!producers.isDisconnected())
		{
			///////////////////////
			OUT << "producers: {\n";

			PUSHTAB;

			auto modules = producers.getChildren();
			for(auto& module : modules)
			{
				if(!module.second.status())
					PUSHCOMMENT;

				//--------------------------------------
				// handle only @table:: producer parameters
				insertParameters(out,
				                 tabStr,
				                 commentStr,
				                 module.second.getNode("producerModuleParameterLink"),
				                 "producerParameter" /*parameterType*/,
				                 true /*onlyInsertAtTableParameters*/,
				                 false /*includeAtTableParameters*/);

				OUT << module.second.getNode("producerKey").getValue() << ": {\n";
				PUSHTAB;

				insertModuleType(out, tabStr, commentStr, module.second.getNode("producerModuleType"));

				//--------------------------------------
				// handle NOT @table:: producer parameters
				insertParameters(out,
				                 tabStr,
				                 commentStr,
				                 module.second.getNode("producerModuleParameterLink"),
				                 "producerParameter" /*parameterType*/,
				                 false /*onlyInsertAtTableParameters*/,
				                 false /*includeAtTableParameters*/);

				POPTAB;
				OUT << "}\n\n";  // end producer module

				if(!module.second.status())
					POPCOMMENT;
			}
			POPTAB;
			OUT << "}\n\n";  // end producer
		}

		auto filters = physics.getNode("filtersLink");
		if(!filters.isDisconnected())
		{
			///////////////////////
			OUT << "filters: {\n";

			PUSHTAB;

			auto modules = filters.getChildren();
			for(auto& module : modules)
			{
				if(!module.second.status())
					PUSHCOMMENT;

				//--------------------------------------
				// handle only @table:: filter parameters
				insertParameters(out,
				                 tabStr,
				                 commentStr,
				                 module.second.getNode("filterModuleParameterLink"),
				                 "filterParameter" /*parameterType*/,
				                 true /*onlyInsertAtTableParameters*/,
				                 false /*includeAtTableParameters*/);

				OUT << module.second.getNode("filterKey").getValue() << ": {\n";
				PUSHTAB;

				insertModuleType(out, tabStr, commentStr, module.second.getNode("filterModuleType"));

				//--------------------------------------
				// handle NOT @table:: filter parameters
				insertParameters(out,
				                 tabStr,
				                 commentStr,
				                 module.second.getNode("filterModuleParameterLink"),
				                 "filterParameter" /*parameterType*/,
				                 false /*onlyInsertAtTableParameters*/,
				                 false /*includeAtTableParameters*/);

				POPTAB;
				OUT << "}\n\n";  // end filter module

				if(!module.second.status())
					POPCOMMENT;
			}
			POPTAB;
			OUT << "}\n\n";  // end filter
		}

		//--------------------------------------
		// handle NOT @table:: physics parameters
		insertParameters(out,
		                 tabStr,
		                 commentStr,
		                 physics.getNode("physicsOtherParametersLink"),
		                 "physicsParameter" /*parameterType*/,
		                 false /*onlyInsertAtTableParameters*/,
		                 false /*includeAtTableParameters*/);

		POPTAB;
		OUT << "}\n\n";  // end physics
	}

	//--------------------------------------
	// handle source
	//__COUT__ << "Filling art.source" << __E__;
	auto source = art.getNode("sourceLink");
	if(!source.isDisconnected())
	{
		OUT << "source: {\n";
		PUSHTAB;
		insertModuleType(out, tabStr, commentStr, source.getNode("sourceModuleType"));
		POPTAB;
		OUT << "}\n\n";  // end source
	}
	else
	{
		OUT << "source: {\n";
		PUSHTAB;
		OUT << "module_type: ArtdaqInput";
		POPTAB;
		OUT << "}\n\n";  // end source
	}

	//--------------------------------------
	// handle process_name
	//__COUT__ << "Writing art.process_name" << __E__;
	OUT << "process_name: " << art.getNode("ProcessName") << "\n";

	//--------------------------------------
	// handle art @table:: art add on parameters
	insertParameters(out,
	                 tabStr,
	                 commentStr,
	                 art.getNode("AddOnParametersLink"),
	                 "artParameter" /*parameterType*/,
	                 false /*onlyInsertAtTableParameters*/,
	                 true /*includeAtTableParameters*/);

}  // end insertArtProcessBlock()

//==============================================================================
void ARTDAQTableBase::outputRoutingManagerFHICL(const ConfigurationTree& routingManagerNode,
                                                size_t                   routingTimeoutMs /* = DEFAULT_ROUTING_TIMEOUT_MS */,
                                                size_t                   routingRetryCount /* = DEFAULT_ROUTING_RETRY_COUNT */)
{
	std::string filename = getFHICLFilename(ARTDAQAppType::RoutingManager, routingManagerNode.getValue());

	/////////////////////////
	// generate xdaq run parameter file
	std::fstream out;

	std::string tabStr     = "";
	std::string commentStr = "";

	out.open(filename, std::fstream::out | std::fstream::trunc);
	if(out.fail())
	{
		__SS__ << "Failed to open ARTDAQ RoutingManager fcl file: " << filename << __E__;
		__SS_THROW__;
	}

	//--------------------------------------
	// header
	OUT << "###########################################################" << __E__;
	OUT << "#" << __E__;
	OUT << "# artdaq routingManager fcl configuration file produced by otsdaq." << __E__;
	OUT << "# 	Creation time:                  \t" << StringMacros::getTimestampString() << __E__;
	OUT << "# 	Original filename:              \t" << filename << __E__;
	OUT << "#	otsdaq-ARTDAQ RoutingManager UID:\t" << routingManagerNode.getValue() << __E__;
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
	auto policyName = routingManagerNode.getNode("routingPolicyPluginType").getValue();
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

			auto comment = parameter.second.getNode(TableViewColumnInfo::COL_NAME_COMMENT);
			OUT << parameter.second.getNode("daqParameterKey").getValue() << ": " << parameter.second.getNode("daqParameterValue").getValue()
			    << (comment.isDefaultValue() ? "" : ("\t # " + comment.getValue())) << "\n";

			if(!parameter.second.status())
				POPCOMMENT;
		}
	}

	POPTAB;
	OUT << "}\n";

	OUT << "use_routing_manager: true\n";

	auto        routingManagerSubsystemID   = 1;
	auto        routingManagerSubsystemLink = routingManagerNode.getNode("SubsystemLink");
	std::string rmHost                      = "localhost";
	if(!routingManagerSubsystemLink.isDisconnected())
	{
		routingManagerSubsystemID = getSubsytemId(routingManagerSubsystemLink);
		rmHost                    = info_.subsystems[routingManagerSubsystemID].routingManagerHost;
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
	auto tableUpdateIntervalMs = routingManagerNode.getNode("tableUpdateIntervalMs").getValue();
	if(tableUpdateIntervalMs != "DEFAULT")
	{
		OUT << "table_update_interval_ms: " << tableUpdateIntervalMs << "\n";
	}
	auto tableAckRetryCount = routingManagerNode.getNode("tableAckRetryCount").getValue();
	if(tableAckRetryCount != "DEFAULT")
	{
		OUT << "table_ack_retry_count: " << tableAckRetryCount << "\n";
	}

	OUT << "routing_timeout_ms: " << routingTimeoutMs << "\n";
	OUT << "routing_retry_count: " << routingRetryCount << "\n";

	insertMetricsBlock(OUT, tabStr, commentStr, routingManagerNode);

	POPTAB;
	OUT << "}\n\n";  // end daq

	out.close();
}  // end outputReaderFHICL()

//==============================================================================
const ARTDAQTableBase::ARTDAQInfo& ARTDAQTableBase::extractARTDAQInfo(ConfigurationTree artdaqSupervisorNode,
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
		return info_;

	// We do RoutingManagers first so we can properly fill in routing tables later
	extractRoutingManagersInfo(artdaqSupervisorNode, getStatusFalseNodes, doWriteFHiCL, routingTimeoutMs, routingRetryCount);

	if(progressBar)
		progressBar->step();

	extractBoardReadersInfo(artdaqSupervisorNode, getStatusFalseNodes, doWriteFHiCL, maxFragmentSizeBytes, routingTimeoutMs, routingRetryCount);

	if(progressBar)
		progressBar->step();

	extractEventBuildersInfo(artdaqSupervisorNode, getStatusFalseNodes, doWriteFHiCL, maxFragmentSizeBytes);

	if(progressBar)
		progressBar->step();

	extractDataLoggersInfo(artdaqSupervisorNode, getStatusFalseNodes, doWriteFHiCL, maxFragmentSizeBytes);

	if(progressBar)
		progressBar->step();

	extractDispatchersInfo(artdaqSupervisorNode, getStatusFalseNodes, doWriteFHiCL, maxFragmentSizeBytes);

	if(progressBar)
		progressBar->step();

	return info_;
}  // end extractARTDAQInfo()

//==============================================================================
void ARTDAQTableBase::extractRoutingManagersInfo(
    ConfigurationTree artdaqSupervisorNode, bool getStatusFalseNodes, bool doWriteFHiCL, size_t routingTimeoutMs, size_t routingRetryCount)
{
	__COUT__ << "Checking for Routing Managers..." << __E__;
	ConfigurationTree rmsLink = artdaqSupervisorNode.getNode(colARTDAQSupervisor_.colLinkToRoutingManagers_);
	if(!rmsLink.isDisconnected() && rmsLink.getChildren().size() > 0)
	{
		std::vector<std::pair<std::string, ConfigurationTree>> routingManagers = rmsLink.getChildren();

		__COUT__ << "There are " << routingManagers.size() << " configured Routing Managers" << __E__;

		for(auto& routingManager : routingManagers)
		{
			const std::string& rmUID = routingManager.first;

			if(getStatusFalseNodes || routingManager.second.status())
			{
				std::string rmHost = routingManager.second.getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_HOSTNAME).getValueWithDefault("localhost");
				if(rmHost == "localhost" || rmHost == "127.0.0.1")
				{
					char hostbuf[HOST_NAME_MAX + 1];
					gethostname(hostbuf, HOST_NAME_MAX);
					rmHost = std::string(hostbuf);
				}

				std::string rmAP = routingManager.second.getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_ALLOWED_PROCESSORS).getValueWithDefault("");

				int               routingManagerSubsystemID   = 1;
				ConfigurationTree routingManagerSubsystemLink = routingManager.second.getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK);
				if(!routingManagerSubsystemLink.isDisconnected())
				{
					routingManagerSubsystemID = getSubsytemId(routingManagerSubsystemLink);

					//__COUTV__(routingManagerSubsystemID);
					info_.subsystems[routingManagerSubsystemID].id = routingManagerSubsystemID;

					const std::string& routingManagerSubsystemName = routingManagerSubsystemLink.getUIDAsString();
					//__COUTV__(routingManagerSubsystemName);

					info_.subsystems[routingManagerSubsystemID].label = routingManagerSubsystemName;

					if(info_.subsystems[routingManagerSubsystemID].hasRoutingManager)
					{
						__SS__ << "Error: You cannot have multiple Routing Managers in a subsystem!";
						__SS_THROW__;
						return;
					}

					auto routingManagerSubsystemDestinationLink = routingManagerSubsystemLink.getNode(colARTDAQSubsystem_.colLinkToDestination_);
					if(routingManagerSubsystemDestinationLink.isDisconnected())
					{
						// default to no destination when no link
						info_.subsystems[routingManagerSubsystemID].destination = 0;
					}
					else
					{
						// get destination subsystem id
						info_.subsystems[routingManagerSubsystemID].destination = getSubsytemId(routingManagerSubsystemDestinationLink);
					}
					//__COUTV__(info_.subsystems[routingManagerSubsystemID].destination);

					// add this subsystem to destination subsystem's sources, if not
					// there
					if(!info_.subsystems.count(info_.subsystems[routingManagerSubsystemID].destination) ||
					   !info_.subsystems[info_.subsystems[routingManagerSubsystemID].destination].sources.count(routingManagerSubsystemID))
					{
						info_.subsystems[info_.subsystems[routingManagerSubsystemID].destination].sources.insert(routingManagerSubsystemID);
					}

				}  // end subsystem instantiation

				__COUT__ << "Found Routing Manager with UID " << rmUID << ", DAQInterface Hostname " << rmHost << ", and Subsystem "
				         << routingManagerSubsystemID << __E__;
				info_.processes[ARTDAQAppType::RoutingManager].emplace_back(
				    rmUID, rmHost, rmAP, routingManagerSubsystemID, ARTDAQAppType::RoutingManager, routingManager.second.status());

				info_.subsystems[routingManagerSubsystemID].hasRoutingManager  = true;
				info_.subsystems[routingManagerSubsystemID].routingManagerHost = rmHost;

				if(doWriteFHiCL)
				{
					outputRoutingManagerFHICL(routingManager.second, routingTimeoutMs, routingRetryCount);

					flattenFHICL(ARTDAQAppType::RoutingManager, routingManager.second.getValue());
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
	ConfigurationTree readersLink = artdaqSupervisorNode.getNode(colARTDAQSupervisor_.colLinkToBoardReaders_);
	if(!readersLink.isDisconnected() && readersLink.getChildren().size() > 0)
	{
		std::vector<std::pair<std::string, ConfigurationTree>> readers = readersLink.getChildren();
		__COUT__ << "There are " << readers.size() << " configured Board Readers." << __E__;

		for(auto& reader : readers)
		{
			const std::string& readerUID = reader.first;

			if(getStatusFalseNodes || reader.second.status())
			{
				std::string readerHost = reader.second.getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_HOSTNAME).getValueWithDefault("localhost");
				std::string readerAP   = reader.second.getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_ALLOWED_PROCESSORS).getValueWithDefault("");

				int               readerSubsystemID   = 1;
				ConfigurationTree readerSubsystemLink = reader.second.getNode(ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK);
				if(!readerSubsystemLink.isDisconnected())
				{
					readerSubsystemID = getSubsytemId(readerSubsystemLink);
					//__COUTV__(readerSubsystemID);
					info_.subsystems[readerSubsystemID].id = readerSubsystemID;

					const std::string& readerSubsystemName = readerSubsystemLink.getUIDAsString();
					//__COUTV__(readerSubsystemName);

					info_.subsystems[readerSubsystemID].label = readerSubsystemName;

					auto readerSubsystemDestinationLink = readerSubsystemLink.getNode(colARTDAQSubsystem_.colLinkToDestination_);
					if(readerSubsystemDestinationLink.isDisconnected())
					{
						// default to no destination when no link
						info_.subsystems[readerSubsystemID].destination = 0;
					}
					else
					{
						// get destination subsystem id
						info_.subsystems[readerSubsystemID].destination = getSubsytemId(readerSubsystemDestinationLink);
					}
					//__COUTV__(info_.subsystems[readerSubsystemID].destination);

					// add this subsystem to destination subsystem's sources, if not
					// there
					if(!info_.subsystems.count(info_.subsystems[readerSubsystemID].destination) ||
					   !info_.subsystems[info_.subsystems[readerSubsystemID].destination].sources.count(readerSubsystemID))
					{
						info_.subsystems[info_.subsystems[readerSubsystemID].destination].sources.insert(readerSubsystemID);
					}

				}  // end subsystem instantiation

				__COUT__ << "Found Board Reader with UID " << readerUID << ", DAQInterface Hostname " << readerHost << ", and Subsystem " << readerSubsystemID
				         << __E__;
				info_.processes[ARTDAQAppType::BoardReader].emplace_back(
				    readerUID, readerHost, readerAP, readerSubsystemID, ARTDAQAppType::BoardReader, reader.second.status());

				if(doWriteFHiCL)
				{
					outputBoardReaderFHICL(reader.second, maxFragmentSizeBytes, routingTimeoutMs, routingRetryCount);

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
void ARTDAQTableBase::extractEventBuildersInfo(ConfigurationTree artdaqSupervisorNode, bool getStatusFalseNodes, bool doWriteFHiCL, size_t maxFragmentSizeBytes)
{
	__COUT__ << "Checking for Event Builders..." << __E__;
	ConfigurationTree buildersLink = artdaqSupervisorNode.getNode(colARTDAQSupervisor_.colLinkToEventBuilders_);
	if(!buildersLink.isDisconnected() && buildersLink.getChildren().size() > 0)
	{
		std::vector<std::pair<std::string, ConfigurationTree>> builders = buildersLink.getChildren();

		for(auto& builder : builders)
		{
			const std::string& builderUID = builder.first;

			if(getStatusFalseNodes || builder.second.status())
			{
				std::string builderHost = builder.second.getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_HOSTNAME).getValueWithDefault("localhost");
				std::string builderAP   = builder.second.getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_ALLOWED_PROCESSORS).getValueWithDefault("");

				int               builderSubsystemID   = 1;
				ConfigurationTree builderSubsystemLink = builder.second.getNode(ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK);
				if(!builderSubsystemLink.isDisconnected())
				{
					builderSubsystemID = getSubsytemId(builderSubsystemLink);
					//__COUTV__(builderSubsystemID);

					info_.subsystems[builderSubsystemID].id = builderSubsystemID;

					const std::string& builderSubsystemName = builderSubsystemLink.getUIDAsString();
					//__COUTV__(builderSubsystemName);

					info_.subsystems[builderSubsystemID].label = builderSubsystemName;

					auto builderSubsystemDestinationLink = builderSubsystemLink.getNode(colARTDAQSubsystem_.colLinkToDestination_);
					if(builderSubsystemDestinationLink.isDisconnected())
					{
						// default to no destination when no link
						info_.subsystems[builderSubsystemID].destination = 0;
					}
					else
					{
						// get destination subsystem id
						info_.subsystems[builderSubsystemID].destination = getSubsytemId(builderSubsystemDestinationLink);
					}
					//__COUTV__(info_.subsystems[builderSubsystemID].destination);

					// add this subsystem to destination subsystem's sources, if not
					// there
					if(!info_.subsystems.count(info_.subsystems[builderSubsystemID].destination) ||
					   !info_.subsystems[info_.subsystems[builderSubsystemID].destination].sources.count(builderSubsystemID))
					{
						info_.subsystems[info_.subsystems[builderSubsystemID].destination].sources.insert(builderSubsystemID);
					}

				}  // end subsystem instantiation

				__COUT__ << "Found Event Builder with UID " << builderUID << ", on Hostname " << builderHost << ", in Subsystem " << builderSubsystemID
				         << __E__;
				info_.processes[ARTDAQAppType::EventBuilder].emplace_back(
				    builderUID, builderHost, builderAP, builderSubsystemID, ARTDAQAppType::EventBuilder, builder.second.status());

				if(doWriteFHiCL)
				{
					outputDataReceiverFHICL(builder.second, ARTDAQAppType::EventBuilder, maxFragmentSizeBytes);

					flattenFHICL(ARTDAQAppType::EventBuilder, builder.second.getValue());
				}
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
void ARTDAQTableBase::extractDataLoggersInfo(ConfigurationTree artdaqSupervisorNode, bool getStatusFalseNodes, bool doWriteFHiCL, size_t maxFragmentSizeBytes)
{
	__COUT__ << "Checking for Data Loggers..." << __E__;
	ConfigurationTree dataloggersLink = artdaqSupervisorNode.getNode(colARTDAQSupervisor_.colLinkToDataLoggers_);
	if(!dataloggersLink.isDisconnected())
	{
		std::vector<std::pair<std::string, ConfigurationTree>> dataloggers = dataloggersLink.getChildren();

		for(auto& datalogger : dataloggers)
		{
			const std::string& loggerUID = datalogger.first;

			if(getStatusFalseNodes || datalogger.second.status())
			{
				std::string loggerHost = datalogger.second.getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_HOSTNAME).getValueWithDefault("localhost");
				std::string loggerAP   = datalogger.second.getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_ALLOWED_PROCESSORS).getValueWithDefault("");

				int               loggerSubsystemID   = 1;
				ConfigurationTree loggerSubsystemLink = datalogger.second.getNode(ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK);
				if(!loggerSubsystemLink.isDisconnected())
				{
					loggerSubsystemID = getSubsytemId(loggerSubsystemLink);
					//__COUTV__(loggerSubsystemID);
					info_.subsystems[loggerSubsystemID].id = loggerSubsystemID;

					const std::string& loggerSubsystemName = loggerSubsystemLink.getUIDAsString();
					//__COUTV__(loggerSubsystemName);

					info_.subsystems[loggerSubsystemID].label = loggerSubsystemName;

					auto loggerSubsystemDestinationLink = loggerSubsystemLink.getNode(colARTDAQSubsystem_.colLinkToDestination_);
					if(loggerSubsystemDestinationLink.isDisconnected())
					{
						// default to no destination when no link
						info_.subsystems[loggerSubsystemID].destination = 0;
					}
					else
					{
						// get destination subsystem id
						info_.subsystems[loggerSubsystemID].destination = getSubsytemId(loggerSubsystemDestinationLink);
					}
					//__COUTV__(info_.subsystems[loggerSubsystemID].destination);

					// add this subsystem to destination subsystem's sources, if not
					// there
					if(!info_.subsystems.count(info_.subsystems[loggerSubsystemID].destination) ||
					   !info_.subsystems[info_.subsystems[loggerSubsystemID].destination].sources.count(loggerSubsystemID))
					{
						info_.subsystems[info_.subsystems[loggerSubsystemID].destination].sources.insert(loggerSubsystemID);
					}

				}  // end subsystem instantiation

				__COUT__ << "Found Data Logger with UID " << loggerUID << ", DAQInterface Hostname " << loggerHost << ", and Subsystem " << loggerSubsystemID
				         << __E__;
				info_.processes[ARTDAQAppType::DataLogger].emplace_back(
				    loggerUID, loggerHost, loggerAP, loggerSubsystemID, ARTDAQAppType::DataLogger, datalogger.second.status());

				if(doWriteFHiCL)
				{
					outputDataReceiverFHICL(datalogger.second, ARTDAQAppType::DataLogger, maxFragmentSizeBytes);

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
void ARTDAQTableBase::extractDispatchersInfo(ConfigurationTree artdaqSupervisorNode, bool getStatusFalseNodes, bool doWriteFHiCL, size_t maxFragmentSizeBytes)
{
	__COUT__ << "Checking for Dispatchers..." << __E__;
	ConfigurationTree dispatchersLink = artdaqSupervisorNode.getNode(colARTDAQSupervisor_.colLinkToDispatchers_);
	if(!dispatchersLink.isDisconnected())
	{
		std::vector<std::pair<std::string, ConfigurationTree>> dispatchers = dispatchersLink.getChildren();

		for(auto& dispatcher : dispatchers)
		{
			const std::string& dispatcherUID = dispatcher.first;

			if(getStatusFalseNodes || dispatcher.second.status())
			{
				std::string dispatcherHost = dispatcher.second.getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_HOSTNAME).getValueWithDefault("localhost");
				std::string dispatcherAP   = dispatcher.second.getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_ALLOWED_PROCESSORS).getValueWithDefault("");
				int         dispatcherPort = dispatcher.second.getNode("DispatcherPort").getValue<int>();

				auto              dispatcherSubsystemID   = 1;
				ConfigurationTree dispatcherSubsystemLink = dispatcher.second.getNode(ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK);
				if(!dispatcherSubsystemLink.isDisconnected())
				{
					dispatcherSubsystemID = getSubsytemId(dispatcherSubsystemLink);
					//__COUTV__(dispatcherSubsystemID);
					info_.subsystems[dispatcherSubsystemID].id = dispatcherSubsystemID;

					const std::string& dispatcherSubsystemName = dispatcherSubsystemLink.getUIDAsString();
					//__COUTV__(dispatcherSubsystemName);

					info_.subsystems[dispatcherSubsystemID].label = dispatcherSubsystemName;

					auto dispatcherSubsystemDestinationLink = dispatcherSubsystemLink.getNode(colARTDAQSubsystem_.colLinkToDestination_);
					if(dispatcherSubsystemDestinationLink.isDisconnected())
					{
						// default to no destination when no link
						info_.subsystems[dispatcherSubsystemID].destination = 0;
					}
					else
					{
						// get destination subsystem id
						info_.subsystems[dispatcherSubsystemID].destination = getSubsytemId(dispatcherSubsystemDestinationLink);
					}
					//__COUTV__(info_.subsystems[dispatcherSubsystemID].destination);

					// add this subsystem to destination subsystem's sources, if not
					// there
					if(!info_.subsystems.count(info_.subsystems[dispatcherSubsystemID].destination) ||
					   !info_.subsystems[info_.subsystems[dispatcherSubsystemID].destination].sources.count(dispatcherSubsystemID))
					{
						info_.subsystems[info_.subsystems[dispatcherSubsystemID].destination].sources.insert(dispatcherSubsystemID);
					}
				}

				__COUT__ << "Found Dispatcher with UID " << dispatcherUID << ", DAQInterface Hostname " << dispatcherHost << ", and Subsystem "
				         << dispatcherSubsystemID << __E__;
				info_.processes[ARTDAQAppType::Dispatcher].emplace_back(
				    dispatcherUID, dispatcherHost, dispatcherAP, dispatcherSubsystemID, ARTDAQAppType::Dispatcher, dispatcher.second.status(), dispatcherPort);

				if(doWriteFHiCL)
				{
					outputDataReceiverFHICL(dispatcher.second, ARTDAQAppType::Dispatcher, maxFragmentSizeBytes);

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
//	isARTDAQEnabled
bool ARTDAQTableBase::isARTDAQEnabled(const ConfigurationManager* cfgMgr)
{
	auto contexts = cfgMgr->getNode(ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME).getChildren();
	for(auto context : contexts)
	{
		if(!context.second.isEnabled())
			continue;

		auto apps = context.second.getNode(XDAQContextTable::colContext_.colLinkToApplicationTable_).getChildren();		
		for(auto app : apps)
		{
			// __COUTV__(app.second.getNode(XDAQContextTable::colApplication_.colClass_).getValue());
			if(app.second.getNode(XDAQContextTable::colApplication_.colClass_).getValue() == ARTDAQ_SUPERVISOR_CLASS &&
				app.second.isEnabled())
				return true;
		}
	}
	return false;
}  // end isARTDAQEnabled()

//==============================================================================
//	getARTDAQSystem
//
//		static function to retrive the active ARTDAQ system configuration.
//
//	Subsystem map to destination subsystem name.
//	Node properties: {status,hostname,subsystemName,(nodeArrString),(hostnameArrString),(hostnameFixedWidth)}
//	artdaqSupervisoInfo: {name, status, context address, context port}
//
const ARTDAQTableBase::ARTDAQInfo& ARTDAQTableBase::getARTDAQSystem(
    ConfigurationManagerRW*                                                                                  cfgMgr,
    std::map<std::string /*type*/, std::map<std::string /*record*/, std::vector<std::string /*property*/>>>& nodeTypeToObjectMap,
    std::map<std::string /*subsystemName*/, std::string /*destinationSubsystemName*/>&                       subsystemObjectMap,
    std::vector<std::string /*property*/>&                                                                   artdaqSupervisoInfo)
{
	__COUT__ << "getARTDAQSystem()" << __E__;

	artdaqSupervisoInfo.clear();  // init

	const XDAQContextTable* contextTable = cfgMgr->__GET_CONFIG__(XDAQContextTable);

	// for each artdaq context, output all artdaq apps

	const XDAQContextTable::XDAQContext* artdaqContext = contextTable->getTheARTDAQSupervisorContext();

	// return empty info
	if(!artdaqContext)
		return ARTDAQTableBase::info_;

	__COUTV__(artdaqContext->contextUID_);
	__COUTV__(artdaqContext->applications_.size());

	for(auto& artdaqApp : artdaqContext->applications_)
	{
		if(artdaqApp.class_ != ARTDAQ_SUPERVISOR_CLASS)
			continue;

		__COUTV__(artdaqApp.applicationUID_);
		artdaqSupervisoInfo.push_back(artdaqApp.applicationUID_);
		artdaqSupervisoInfo.push_back((artdaqContext->status_ && artdaqApp.status_) ? "1" : "0");
		artdaqSupervisoInfo.push_back(artdaqContext->address_);
		artdaqSupervisoInfo.push_back(std::to_string(artdaqContext->port_));

		const ARTDAQTableBase::ARTDAQInfo& info = ARTDAQTableBase::extractARTDAQInfo(XDAQContextTable::getSupervisorConfigNode(/*artdaqSupervisorNode*/
		                                                                                                                       cfgMgr,
		                                                                                                                       artdaqContext->contextUID_,
		                                                                                                                       artdaqApp.applicationUID_),
		                                                                             true /*getStatusFalseNodes*/);

		__COUT__ << "========== "
		         << "Found " << info.subsystems.size() << " subsystems." << __E__;

		// build subsystem desintation map
		for(auto& subsystem : info.subsystems)
			subsystemObjectMap.emplace(std::make_pair(subsystem.second.label, std::to_string(subsystem.second.destination)));

		__COUT__ << "========== "
		         << "Found " << info.processes.size() << " process types." << __E__;

		for(auto& nameTypePair : ARTDAQTableBase::processTypes_.mapToType_)
		{
			const std::string& typeString = nameTypePair.first;
			__COUTV__(typeString);

			nodeTypeToObjectMap.emplace(std::make_pair(typeString, std::map<std::string /*record*/, std::vector<std::string /*property*/>>()));

			auto it = info.processes.find(nameTypePair.second);
			if(it == info.processes.end())
			{
				__COUT__ << "\t"
				         << "Found 0 " << typeString << __E__;
				continue;
			}
			__COUT__ << "\t"
			         << "Found " << it->second.size() << " " << typeString << "(s)" << __E__;

			auto tableIt = processTypes_.mapToTable_.find(typeString);
			if(tableIt == processTypes_.mapToTable_.end())
			{
				__SS__ << "Invalid artdaq node type '" << typeString << "' attempted!" << __E__;
				__SS_THROW__;
			}
			__COUTV__(tableIt->second);

			auto allNodes = cfgMgr->getNode(tableIt->second).getChildren();

			std::set<std::string /*nodeName*/> skipSet;  // use to skip nodes when constructing multi-nodes

			const std::set<std::string /*colName*/> skipColumns({ARTDAQ_TYPE_TABLE_HOSTNAME,
			                                                     ARTDAQ_TYPE_TABLE_ALLOWED_PROCESSORS,
			                                                     ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK,
			                                                     TableViewColumnInfo::COL_NAME_COMMENT,
			                                                     TableViewColumnInfo::COL_NAME_AUTHOR,
			                                                     TableViewColumnInfo::COL_NAME_CREATION});  // note: also skip UID and Status

			// loop through all nodes of this type
			for(auto& artdaqNode : it->second)
			{
				// check skip set
				if(skipSet.find(artdaqNode.label) != skipSet.end())
					continue;

				__COUT__ << "\t\t"
				         << "Found '" << artdaqNode.label << "' " << typeString << __E__;

				std::string nodeName      = artdaqNode.label;
				bool        status        = artdaqNode.status;
				std::string hostname      = artdaqNode.hostname;
				std::string subsystemId   = std::to_string(artdaqNode.subsystem);
				std::string subsystemName = info.subsystems.at(artdaqNode.subsystem).label;

				ConfigurationTree thisNode        = cfgMgr->getNode(tableIt->second).getNode(nodeName);
				auto              thisNodeColumns = thisNode.getChildren();

				// check for multi-node
				//	Steps:
				//		search for other records with same values/links except hostname/name

				std::vector<std::string> multiNodeNames, hostnameArray;
				// unsigned int             hostnameFixedWidth = 0;

				__COUTV__(allNodes.size());
				for(auto& otherNode : allNodes)  // start multi-node search loop
				{
					if(otherNode.first == nodeName || skipSet.find(otherNode.first) != skipSet.end() ||
					   otherNode.second.status() != status)  // skip if status mismatch
						continue;                            // skip unless 'other' and not in skip set

					//__COUTV__(subsystemName);
					//__COUTV__(otherNode.second.getNode(ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK_UID).getValue());

					if(subsystemName == otherNode.second.getNode(ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK_UID).getValue())
					{
						// possible multi-node situation
						//__COUT__ << "Checking for multi-node..." << __E__;

						//__COUTV__(thisNode.getNodeRow());
						//__COUTV__(otherNode.second.getNodeRow());

						auto otherNodeColumns = otherNode.second.getChildren();

						bool isMultiNode = true;
						for(unsigned int i = 0; i < thisNodeColumns.size() && i < otherNodeColumns.size(); ++i)
						{
							// skip columns that do not need to be checked for multi-node consideration
							if(skipColumns.find(thisNodeColumns[i].first) != skipColumns.end() || thisNodeColumns[i].second.isLinkNode())
								continue;

							// at this point must match for multinode

							//__COUTV__(thisNodeColumns[i].first);
							//__COUTV__(otherNodeColumns[i].first);

							//__COUTV__(thisNodeColumns[i].second.getValue());
							//__COUTV__(otherNodeColumns[i].second.getValue());

							if(thisNodeColumns[i].second.getValue() != otherNodeColumns[i].second.getValue())
							{
								__COUT__ << "Mismatch, not multi-node member." << __E__;
								isMultiNode = false;
								break;
							}
						}

						if(isMultiNode)
						{
							__COUT__ << "Found '" << nodeName << "' multi-node member candidate '" << otherNode.first << "'" << __E__;

							if(!multiNodeNames.size())  // add this node first!
							{
								multiNodeNames.push_back(nodeName);
								hostnameArray.push_back(hostname);
							}
							multiNodeNames.push_back(otherNode.first);
							hostnameArray.push_back(otherNode.second.getNode(ARTDAQ_TYPE_TABLE_HOSTNAME).getValue());
							skipSet.emplace(otherNode.first);
						}
					}
				}  // end loop to search for multi-node members

				unsigned int nodeFixedWildcardLength = 0, hostFixedWildcardLength = 0;
				std::string  multiNodeString = "", hostArrayString = "";

				if(multiNodeNames.size() > 1)
				{
					__COUT__ << "Handling multi-node printer syntax" << __E__;

					__COUTV__(StringMacros::vectorToString(multiNodeNames));
					__COUTV__(StringMacros::vectorToString(hostnameArray));
					__COUTV__(StringMacros::setToString(skipSet));

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

							//__COUT__ << multiNodeNames[0] << " vs " << multiNodeNames[i] << __E__;

							// start forward score loop
							for(unsigned int j = 0, k = 0; j < multiNodeNames[0].size() && k < multiNodeNames[i].size(); ++j, ++k)
							{
								while(j < multiNodeNames[0].size() && !(multiNodeNames[0][j] >= 'a' && multiNodeNames[0][j] <= 'z') &&
								      !(multiNodeNames[0][j] >= 'A' && multiNodeNames[0][j] <= 'Z'))
									++j;  // skip non-alpha characters
								while(k < multiNodeNames[i].size() && !(multiNodeNames[i][k] >= 'a' && multiNodeNames[i][k] <= 'z') &&
								      !(multiNodeNames[i][k] >= 'A' && multiNodeNames[i][k] <= 'Z'))
									++k;  // skip non-alpha characters

								while(k < multiNodeNames[i].size() && multiNodeNames[0][j] != multiNodeNames[i][k])
									++k;  // skip non-matching alpha characters

								//__COUT__ << j << "-" << k << " of " <<
								//		multiNodeNames[0].size() << "-" <<
								//		multiNodeNames[i].size() << __E__;

								if(j < multiNodeNames[0].size() && k < multiNodeNames[i].size())
									++score;  // found a matching letter!
							}                 // end forward score loop

							//__COUTV__(score);

							// start backward score loop
							for(unsigned int j = multiNodeNames[0].size() - 1, k = multiNodeNames[i].size() - 1;
							    j < multiNodeNames[0].size() && k < multiNodeNames[i].size();
							    --j, --k)
							{
								while(j < multiNodeNames[0].size() && !(multiNodeNames[0][j] >= 'a' && multiNodeNames[0][j] <= 'z') &&
								      !(multiNodeNames[0][j] >= 'A' && multiNodeNames[0][j] <= 'Z'))
									--j;  // skip non-alpha characters
								while(k < multiNodeNames[i].size() && !(multiNodeNames[i][k] >= 'a' && multiNodeNames[i][k] <= 'z') &&
								      !(multiNodeNames[i][k] >= 'A' && multiNodeNames[i][k] <= 'Z'))
									--k;  // skip non-alpha characters

								while(k < multiNodeNames[i].size() && multiNodeNames[0][j] != multiNodeNames[i][k])
									--k;  // skip non-matching alpha characters

								//__COUT__ << "BACK" << j << "-" << k << " of " <<
								//		multiNodeNames[0].size() << "-" <<
								//		multiNodeNames[i].size() << __E__;

								if(j < multiNodeNames[0].size() && k < multiNodeNames[i].size())
									++score;  // found a matching letter!
							}                 // end backward score loop

							//__COUTV__(score/2.0);

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

						//__COUTV__(minScore);
						//__COUTV__(maxScore);

						__COUT__ << "Trimming multi-node members with low match score..." << __E__;

						// go backwards, to not mess up indices as deleted
						//	do not delete index 0
						for(unsigned int i = multiNodeNames.size() - 1; i > 0 && i < multiNodeNames.size(); --i)
						{
							//__COUTV__(scoreVector[i]);
							//__COUTV__(i);
							if(maxScore > multiNodeNames[0].size() && scoreVector[i] >= maxScore)
								continue;

							// else trim
							__COUT__ << "Trimming " << multiNodeNames[i] << __E__;

							skipSet.erase(multiNodeNames[i]);
							multiNodeNames.erase(multiNodeNames.begin() + i);
							hostnameArray.erase(hostnameArray.begin() + i);

						}  // end multi-node trim loop

					}  // done with multi-node member trim

					__COUTV__(StringMacros::vectorToString(multiNodeNames));
					__COUTV__(StringMacros::vectorToString(hostnameArray));
					__COUTV__(StringMacros::setToString(skipSet));

					// from set of nodename wildcards, make printer syntax
					if(multiNodeNames.size() > 1)
					{
						std::vector<std::string> commonChunks;
						std::vector<std::string> wildcards;

						//can not change the order of wildcards for node names! or the names will not keep pairing with host

						bool wildcardsNeeded = StringMacros::extractCommonChunks(multiNodeNames, commonChunks, wildcards, nodeFixedWildcardLength);

						if(!wildcardsNeeded || wildcards.size() != multiNodeNames.size())
						{
							__SS__ << "Impossible extractCommonChunks result! Please notify admins or try to simplify record naming convention." << __E__;
							__SS_THROW__;
						}


						__COUTV__(StringMacros::vectorToString(commonChunks));

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
								if(i + 1 < intWildcards.size() && intWildcards[i] + 1 == intWildcards[i + 1])
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
										multiNodeString += (isFirst ? "" : ",") + std::to_string(intWildcards[i]);
									}
									else
									{
										// if only 1 number apart, then comma
										if(intWildcards[hyphenLo]+1 == intWildcards[i])
											multiNodeString +=
												(isFirst ? "" : ",") + std::to_string(intWildcards[hyphenLo]) + "," + std::to_string(intWildcards[i]);
										else // else hyphen numbers
											multiNodeString +=
												(isFirst ? "" : ",") + std::to_string(intWildcards[hyphenLo]) + "-" + std::to_string(intWildcards[i]);
										hyphenLo = -1;  // reset for next
									}
									isFirst = false;
								}
							}
						}     // end all integer handling
						else  // not all integers, so csv
						{
							multiNodeString = StringMacros::vectorToString(wildcards);
						}  // end not-all integer handling

						__COUTV__(multiNodeString);
						__COUTV__(nodeFixedWildcardLength);
					}  // end node name printer syntax handling

					if(hostnameArray.size() > 1)
					{
						std::vector<std::string> commonChunks;
						std::vector<std::string> wildcards;

						//can not change the order of wildcards for hostname! or the names will not keep pairing with host

						bool wildcardsNeeded = StringMacros::extractCommonChunks(hostnameArray, commonChunks, wildcards, hostFixedWildcardLength);


						__COUTV__(wildcardsNeeded);
						__COUTV__(StringMacros::vectorToString(commonChunks));

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
									intWildcards.push_back(strtol(wildcard.c_str(), 0, 10));

								__COUTV__(StringMacros::vectorToString(intWildcards));

								unsigned int hyphenLo = -1;
								bool         isFirst  = true;
								for(unsigned int i = 0; i < intWildcards.size(); ++i)
								{
									if(i + 1 < intWildcards.size() && intWildcards[i] + 1 == intWildcards[i + 1])
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
											hostArrayString += (isFirst ? "" : ",") + std::to_string(intWildcards[i]);
										}
										else
										{
											// if only 1 number apart, then comma
											if(intWildcards[hyphenLo]+1 == intWildcards[i])
												hostArrayString +=
													(isFirst ? "" : ",") + std::to_string(intWildcards[hyphenLo]) + "," + std::to_string(intWildcards[i]);
											else // else hyphen numbers
												hostArrayString +=
													(isFirst ? "" : ",") + std::to_string(intWildcards[hyphenLo]) + "-" + std::to_string(intWildcards[i]);
											hyphenLo = -1;  // reset for next
										}
										isFirst = false;
									}
								}
							}     // end all integer handling
							else  // not all integers, so csv
							{
								hostArrayString = StringMacros::vectorToString(wildcards);
							}  // end not-all integer handling
						}      // end wildcard need handling
						__COUTV__(hostArrayString);
						__COUTV__(hostFixedWildcardLength);
					}  // end node name printer syntax handling

				}  // end multi node printer syntax handling

				nodeTypeToObjectMap.at(typeString).emplace(std::make_pair(nodeName, std::vector<std::string /*property*/>()));

				nodeTypeToObjectMap.at(typeString).at(nodeName).push_back(status ? "1" : "0");

				nodeTypeToObjectMap.at(typeString).at(nodeName).push_back(hostname);

				nodeTypeToObjectMap.at(typeString).at(nodeName).push_back(subsystemId);
				if(multiNodeNames.size() > 1)
				{
					nodeTypeToObjectMap.at(typeString).at(nodeName).push_back(multiNodeString);

					nodeTypeToObjectMap.at(typeString).at(nodeName).push_back(std::to_string(nodeFixedWildcardLength));

					if(hostnameArray.size() > 1)
					{
						nodeTypeToObjectMap.at(typeString).at(nodeName).push_back(hostArrayString);

						nodeTypeToObjectMap.at(typeString).at(nodeName).push_back(std::to_string(hostFixedWildcardLength));
					}
				}  // done adding multinode parameters
			}
		}  // end processor type handling

	}  // end artdaq app loop

	__COUT__ << "Done getting artdaq nodes." << __E__;

	return ARTDAQTableBase::info_;
}  // end getARTDAQSystem()

//==============================================================================
//	setAndActivateARTDAQSystem
//
//		static function to modify the active configuration based on
//	node object and subsystem object.
//
//	Subsystem map to destination subsystem name.
//	Node properties: {originalName,status,hostname,subsystemName,(nodeArrString),(hostnameArrString),(hostnameFixedWidth)}
//
void ARTDAQTableBase::setAndActivateARTDAQSystem(
    ConfigurationManagerRW*                                                                                        cfgMgr,
    const std::map<std::string /*type*/, std::map<std::string /*record*/, std::vector<std::string /*property*/>>>& nodeTypeToObjectMap,
    const std::map<std::string /*subsystemName*/, std::string /*destinationSubsystemName*/>&                       subsystemObjectMap)
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

	GroupEditStruct configGroupEdit(ConfigurationManager::GroupType::CONFIGURATION_TYPE, cfgMgr);

	unsigned int artdaqSupervisorRow = TableView::INVALID;

	const XDAQContextTable* contextTable = cfgMgr->__GET_CONFIG__(XDAQContextTable);

	const XDAQContextTable::XDAQContext* artdaqContext = contextTable->getTheARTDAQSupervisorContext();

	bool needArtdaqSupervisorParents  = true;
	bool needArtdaqSupervisorCreation = false;

	if(artdaqContext)  // check for full connection to supervisor
	{
		try
		{
			ConfigurationTree artdaqSupervisorNode = cfgMgr->getNode(ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME)
			                                             .getNode(artdaqContext->contextUID_)
			                                             .getNode(XDAQContextTable::colContext_.colLinkToApplicationTable_)
			                                             .getNode(artdaqContext->applications_[0].applicationUID_)
			                                             .getNode(XDAQContextTable::colApplication_.colLinkToSupervisorTable_);

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
			TableEditStruct& artdaqSupervisorTable = configGroupEdit.getTableEditStruct(ARTDAQ_SUPERVISOR_TABLE, true /*markModified*/);

			// create artdaq Supervisor context record
			row = artdaqSupervisorTable.tableView_->addRow(author, true /*incrementUniqueData*/, "artdaqSupervisor");

			// get UID
			artdaqSupervisorUID = artdaqSupervisorTable.tableView_->getDataView()[row][artdaqSupervisorTable.tableView_->getColUID()];
			artdaqSupervisorRow = row;

			__COUTV__(artdaqSupervisorRow);
			__COUTV__(artdaqSupervisorUID);

			// set DAQInterfaceDebugLevel
			artdaqSupervisorTable.tableView_->setValueAsString(
			    "1", row, artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colDAQInterfaceDebugLevel_));
			// set DAQSetupScript
			artdaqSupervisorTable.tableView_->setValueAsString(
			    "${MRB_BUILDDIR}/../setup_ots.sh", row, artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colDAQSetupScript_));

			// create group link to board readers
			artdaqSupervisorTable.tableView_->setValueAsString(
			    ARTDAQ_READER_TABLE, row, artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToBoardReaders_));
			artdaqSupervisorTable.tableView_->setUniqueColumnValue(

			    row,
			    artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToBoardReadersGroupID_),
			    artdaqSupervisorUID + processTypes_.mapToGroupIDAppend_.at(processTypes_.READER));
			// create group link to event builders
			artdaqSupervisorTable.tableView_->setValueAsString(
			    ARTDAQ_BUILDER_TABLE, row, artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToEventBuilders_));
			artdaqSupervisorTable.tableView_->setUniqueColumnValue(
			    row,
			    artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToEventBuildersGroupID_),
			    artdaqSupervisorUID + processTypes_.mapToGroupIDAppend_.at(processTypes_.BUILDER));
			// create group link to data loggers
			artdaqSupervisorTable.tableView_->setValueAsString(
			    ARTDAQ_LOGGER_TABLE, row, artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToDataLoggers_));
			artdaqSupervisorTable.tableView_->setUniqueColumnValue(row,
			                                                       artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToDataLoggersGroupID_),
			                                                       artdaqSupervisorUID + processTypes_.mapToGroupIDAppend_.at(processTypes_.LOGGER));
			// create group link to dispatchers
			artdaqSupervisorTable.tableView_->setValueAsString(
			    ARTDAQ_DISPATCHER_TABLE, row, artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToDispatchers_));
			artdaqSupervisorTable.tableView_->setUniqueColumnValue(row,
			                                                       artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToDispatchersGroupID_),
			                                                       artdaqSupervisorUID + processTypes_.mapToGroupIDAppend_.at(processTypes_.DISPATCHER));

			// create group link to routing managers
			artdaqSupervisorTable.tableView_->setValueAsString(
			    ARTDAQ_ROUTER_TABLE, row, artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToRoutingManagers_));
			artdaqSupervisorTable.tableView_->setUniqueColumnValue(
			    row,
			    artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToRoutingManagersGroupID_),
			    artdaqSupervisorUID + processTypes_.mapToGroupIDAppend_.at(processTypes_.ROUTER));

			{
				std::stringstream ss;
				artdaqSupervisorTable.tableView_->print(ss);
				__COUT__ << ss.str();
			}
		}  // end create artdaq Supervisor in configuration group

		// now create artdaq Supervisor parents in context group
		{
			GroupEditStruct contextGroupEdit(ConfigurationManager::GroupType::CONTEXT_TYPE, cfgMgr);

			TableEditStruct& contextTable     = contextGroupEdit.getTableEditStruct(ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME, true /*markModified*/);
			TableEditStruct& appTable         = contextGroupEdit.getTableEditStruct(ConfigurationManager::XDAQ_APPLICATION_TABLE_NAME, true /*markModified*/);
			TableEditStruct& appPropertyTable = contextGroupEdit.getTableEditStruct(ConfigurationManager::XDAQ_APP_PROPERTY_TABLE_NAME, true /*markModified*/);

			// open try for decorating errors and for clean code scope
			try
			{
				std::string contextUID;
				std::string contextAppGroupID;

				if(needArtdaqSupervisorParents)
				{
					// create artdaq Supervisor context record
					row = contextTable.tableView_->addRow(author, true /*incrementUniqueData*/, "artdaqContext");
					// set context status true
					contextTable.tableView_->setValueAsString("1", row, contextTable.tableView_->getColStatus());

					contextUID = contextTable.tableView_->getDataView()[row][contextTable.tableView_->getColUID()];

					__COUTV__(row);
					__COUTV__(contextUID);

					// set address/port
					contextTable.tableView_->setValueAsString(
					    "http://${HOSTNAME}", row, contextTable.tableView_->findCol(XDAQContextTable::colContext_.colAddress_));
					contextTable.tableView_->setUniqueColumnValue(
					    row, contextTable.tableView_->findCol(XDAQContextTable::colContext_.colPort_), "${OTS_MAIN_PORT}", true /*doMathAppendStrategy*/);

					// create group link to artdaq Supervisor app
					contextTable.tableView_->setValueAsString(ConfigurationManager::XDAQ_APPLICATION_TABLE_NAME,
					                                          row,
					                                          contextTable.tableView_->findCol(XDAQContextTable::colContext_.colLinkToApplicationTable_));
					contextAppGroupID = contextTable.tableView_->setUniqueColumnValue(
					    row, contextTable.tableView_->findCol(XDAQContextTable::colContext_.colLinkToApplicationGroupID_), "artdaqContextApps");

					__COUTV__(contextAppGroupID);

				}  // end create context entry

				std::string appUID;
				std::string appPropertiesGroupID;

				// create artdaq Supervisor app
				{
					unsigned int row;

					if(needArtdaqSupervisorParents)
					{
						// first disable any existing artdaq supervisor apps
						{
							unsigned int c = appTable.tableView_->findCol(XDAQContextTable::colApplication_.colClass_);
							for(unsigned int r = 0; r < appTable.tableView_->getNumberOfRows(); ++r)
								if(appTable.tableView_->getDataView()[r][c] == ARTDAQ_SUPERVISOR_CLASS)
								{
									__COUT_WARN__ << "Found partially existing artdaq Supervisor application '"
									              << appTable.tableView_->getDataView()[r][appTable.tableView_->getColUID()] << "'... Disabling it." << __E__;
									appTable.tableView_->setValueAsString("0", r, appTable.tableView_->getColStatus());
								}
						}

						// create artdaq Supervisor context record
						row = appTable.tableView_->addRow(author, true /*incrementUniqueData*/, "artdaqSupervisor");
						// set app status true
						appTable.tableView_->setValueAsString("1", row, appTable.tableView_->getColStatus());

						appUID = appTable.tableView_->getDataView()[row][appTable.tableView_->getColUID()];

						__COUTV__(row);
						__COUTV__(appUID);

						// set class
						appTable.tableView_->setValueAsString(
						    ARTDAQ_SUPERVISOR_CLASS, row, appTable.tableView_->findCol(XDAQContextTable::colApplication_.colClass_));
						// set module
						appTable.tableView_->setValueAsString(
						    "${OTSDAQ_LIB}/libARTDAQSupervisor.so", row, appTable.tableView_->findCol(XDAQContextTable::colApplication_.colModule_));
						// set groupid
						appTable.tableView_->setValueAsString(
						    contextAppGroupID, row, appTable.tableView_->findCol(XDAQContextTable::colApplication_.colApplicationGroupID_));

						// create group link to artdaq Supervisor app properties
						appTable.tableView_->setValueAsString(ConfigurationManager::XDAQ_APP_PROPERTY_TABLE_NAME,
						                                      row,
						                                      appTable.tableView_->findCol(XDAQContextTable::colApplication_.colLinkToPropertyTable_));
						appPropertiesGroupID = appTable.tableView_->setUniqueColumnValue(
						    row, appTable.tableView_->findCol(XDAQContextTable::colApplication_.colLinkToPropertyGroupID_), appUID + "Properties");

						__COUTV__(appPropertiesGroupID);
					}
					else  //! needArtdaqSupervisorParents
					{
						__COUT__ << "Getting row of existing parent supervisor." << __E__;

						// get row of current artdaq supervisor app
						row = cfgMgr->getNode(ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME)
						          .getNode(artdaqContext->contextUID_)
						          .getNode(XDAQContextTable::colContext_.colLinkToApplicationTable_)
						          .getNode(artdaqContext->applications_[0].applicationUID_)
						          .getRow();
						__COUTV__(row);
					}

					// create group link to artdaq Supervisor app properties
					//		create link whether or not parents were created
					//		because, if here, then artdaq supervisor record was created.
					appTable.tableView_->setValueAsString(
					    ARTDAQ_SUPERVISOR_TABLE, row, appTable.tableView_->findCol(XDAQContextTable::colApplication_.colLinkToSupervisorTable_));
					appTable.tableView_->setValueAsString(
					    artdaqSupervisorUID, row, appTable.tableView_->findCol(XDAQContextTable::colApplication_.colLinkToSupervisorUID_));

				}  // end create app entry

				// create artdaq Supervisor properties
				if(needArtdaqSupervisorParents)
				{
					unsigned int row;

					const std::vector<std::string> propertyUIDs = {
					    "Partition0", "ProductsDir", "FragmentSize", "BoardReaderTimeout", "EventBuilderTimeout", "DataLoggerTimeout", "DispatcherTimeout"};
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
						row = appPropertyTable.tableView_->addRow(author, true /*incrementUniqueData*/, appUID + propertyUIDs[i]);
						// set app status true
						appPropertyTable.tableView_->setValueAsString("1", row, appPropertyTable.tableView_->getColStatus());

						// set type
						appPropertyTable.tableView_->setValueAsString(
						    "ots::SupervisorProperty", row, appPropertyTable.tableView_->findCol(XDAQContextTable::colAppProperty_.colPropertyType_));
						// set name
						appPropertyTable.tableView_->setValueAsString(
						    propertyNames[i], row, appPropertyTable.tableView_->findCol(XDAQContextTable::colAppProperty_.colPropertyName_));
						// set value
						appPropertyTable.tableView_->setValueAsString(
						    propertyValues[i], row, appPropertyTable.tableView_->findCol(XDAQContextTable::colAppProperty_.colPropertyValue_));
						// set groupid
						appPropertyTable.tableView_->setValueAsString(
						    appPropertiesGroupID, row, appPropertyTable.tableView_->findCol(XDAQContextTable::colAppProperty_.colPropertyGroupID_));
					}  // end property create loop
				}      // end create app property entries

				{
					std::stringstream ss;
					contextTable.tableView_->print(ss);
					__COUT__ << ss.str();
				}
				{
					std::stringstream ss;
					appTable.tableView_->print(ss);
					__COUT__ << ss.str();
				}
				{
					std::stringstream ss;
					appPropertyTable.tableView_->print(ss);
					__COUT__ << ss.str();
				}

				contextTable.tableView_->init();      // verify new table (throws runtime_errors)
				appTable.tableView_->init();          // verify new table (throws runtime_errors)
				appPropertyTable.tableView_->init();  // verify new table (throws runtime_errors)
			}
			catch(...)
			{
				__COUT__ << "Table errors while creating ARTDAQ Supervisor. Erasing all newly "
				            "created table versions."
				         << __E__;
				throw;  // re-throw
			}           // end catch

			__COUT__ << "Edits complete for new artdaq Supervisor!" << __E__;

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
		artdaqSupervisorRow = cfgMgr->getNode(ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME)
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
		__SS__ << "Invalid artdaq Supervisor row " << artdaqSupervisorRow << " found!" << __E__;
		__SS_THROW__;
	}

	// Remaining steps:
	// Step	1. create/verify subsystems and destinations
	// Step	2. for each node, create/verify records

	// open try for decorating configuration group errors and for clean code scope
	try
	{
		unsigned int row;

		TableEditStruct& artdaqSupervisorTable = configGroupEdit.getTableEditStruct(ARTDAQ_SUPERVISOR_TABLE, true /*markModified*/);

		// for any NO_LINK links in artdaqSupervisor record, fix them
		{
			std::string artdaqSupervisorUID =
			    artdaqSupervisorTable.tableView_->getDataView()[artdaqSupervisorRow][artdaqSupervisorTable.tableView_->getColUID()];

			// create group link to board readers
			if(artdaqSupervisorTable.tableView_
			       ->getDataView()[artdaqSupervisorRow][artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToBoardReaders_)] ==
			   TableViewColumnInfo::DATATYPE_LINK_DEFAULT)
			{
				__COUT__ << "Fixing missing link to Readers" << __E__;
				artdaqSupervisorTable.tableView_->setValueAsString(
				    ARTDAQ_READER_TABLE, artdaqSupervisorRow, artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToBoardReaders_));
				artdaqSupervisorTable.tableView_->setUniqueColumnValue(
				    artdaqSupervisorRow,
				    artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToBoardReadersGroupID_),
				    artdaqSupervisorUID + processTypes_.mapToGroupIDAppend_.at(processTypes_.READER));
			}

			// create group link to event builders
			if(artdaqSupervisorTable.tableView_
			       ->getDataView()[artdaqSupervisorRow][artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToEventBuilders_)] ==
			   TableViewColumnInfo::DATATYPE_LINK_DEFAULT)
			{
				__COUT__ << "Fixing missing link to Builders" << __E__;
				artdaqSupervisorTable.tableView_->setValueAsString(
				    ARTDAQ_BUILDER_TABLE, artdaqSupervisorRow, artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToEventBuilders_));
				artdaqSupervisorTable.tableView_->setUniqueColumnValue(
				    artdaqSupervisorRow,
				    artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToEventBuildersGroupID_),
				    artdaqSupervisorUID + processTypes_.mapToGroupIDAppend_.at(processTypes_.BUILDER));
			}

			// create group link to data loggers
			if(artdaqSupervisorTable.tableView_
			       ->getDataView()[artdaqSupervisorRow][artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToDataLoggers_)] ==
			   TableViewColumnInfo::DATATYPE_LINK_DEFAULT)
			{
				__COUT__ << "Fixing missing link to Loggers" << __E__;
				artdaqSupervisorTable.tableView_->setValueAsString(
				    ARTDAQ_LOGGER_TABLE, artdaqSupervisorRow, artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToDataLoggers_));
				artdaqSupervisorTable.tableView_->setUniqueColumnValue(
				    artdaqSupervisorRow,
				    artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToDataLoggersGroupID_),
				    artdaqSupervisorUID + processTypes_.mapToGroupIDAppend_.at(processTypes_.LOGGER));
			}

			// create group link to dispatchers
			if(artdaqSupervisorTable.tableView_
			       ->getDataView()[artdaqSupervisorRow][artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToDispatchers_)] ==
			   TableViewColumnInfo::DATATYPE_LINK_DEFAULT)
			{
				__COUT__ << "Fixing missing link to Dispatchers" << __E__;
				artdaqSupervisorTable.tableView_->setValueAsString(
				    ARTDAQ_DISPATCHER_TABLE, artdaqSupervisorRow, artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToDispatchers_));
				artdaqSupervisorTable.tableView_->setUniqueColumnValue(
				    artdaqSupervisorRow,
				    artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToDispatchersGroupID_),
				    artdaqSupervisorUID + processTypes_.mapToGroupIDAppend_.at(processTypes_.DISPATCHER));
			}

			// create group link to routing managers
			if(artdaqSupervisorTable.tableView_
			       ->getDataView()[artdaqSupervisorRow][artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToRoutingManagers_)] ==
			   TableViewColumnInfo::DATATYPE_LINK_DEFAULT)
			{
				__COUT__ << "Fixing missing link to Routers" << __E__;
				artdaqSupervisorTable.tableView_->setValueAsString(
				    ARTDAQ_ROUTER_TABLE, artdaqSupervisorRow, artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToRoutingManagers_));
				artdaqSupervisorTable.tableView_->setUniqueColumnValue(
				    artdaqSupervisorRow,
				    artdaqSupervisorTable.tableView_->findCol(colARTDAQSupervisor_.colLinkToRoutingManagersGroupID_),
				    artdaqSupervisorUID + processTypes_.mapToGroupIDAppend_.at(processTypes_.ROUTER));
			}

			{
				std::stringstream ss;
				artdaqSupervisorTable.tableView_->print(ss);
				__COUT__ << ss.str();
			}
		}  // end fixing links

		// Step	1. create/verify subsystems and destinations
		TableEditStruct& artdaqSubsystemTable = configGroupEdit.getTableEditStruct(ARTDAQ_SUBSYSTEM_TABLE, true /*markModified*/);

		// clear all records
		artdaqSubsystemTable.tableView_->deleteAllRows();

		for(auto& subsystemPair : subsystemObjectMap)
		{
			__COUTV__(subsystemPair.first);
			__COUTV__(subsystemPair.second);

			// create artdaq Subsystem record
			row = artdaqSubsystemTable.tableView_->addRow(author, true /*incrementUniqueData*/, subsystemPair.first);

			if(subsystemPair.second != "" && subsystemPair.second != TableViewColumnInfo::DATATYPE_STRING_DEFAULT &&
			   subsystemPair.second != NULL_SUBSYSTEM_DESTINATION_LABEL)
			{
				// set subsystem link
				artdaqSubsystemTable.tableView_->setValueAsString(
				    ARTDAQ_SUBSYSTEM_TABLE, row, artdaqSubsystemTable.tableView_->findCol(colARTDAQSubsystem_.colLinkToDestination_));
				artdaqSubsystemTable.tableView_->setValueAsString(
				    subsystemPair.second, row, artdaqSubsystemTable.tableView_->findCol(colARTDAQSubsystem_.colLinkToDestinationUID_));
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
				__SS__ << "Invalid artdaq node type '" << nodeTypePair.first << "' attempted!" << __E__;
				__SS_THROW__;
			}
			__COUTV__(it->second);

			// test the table before getting for real
			try
			{
				/*	TableEditStruct& tmpTypeTable = */ configGroupEdit.getTableEditStruct(it->second, true /*markModified*/);
			}
			catch(...)
			{
				if(nodeTypePair.second.size())
					throw;  // do not ignore if user was trying to save records

				__COUT__ << "Ignoring missing table '" << it->second << "' since there were no user records attempted of type '" << nodeTypePair.first << ".'"
				         << __E__;
				continue;
			}
			TableEditStruct& typeTable = configGroupEdit.getTableEditStruct(it->second, true /*markModified*/);

			// keep track of records to delete, initialize to all in current table
			std::map<unsigned int /*type record row*/, bool /*doDelete*/> deleteRecordMap;
			for(unsigned int r = 0; r < typeTable.tableView_->getNumberOfRows(); ++r)
				deleteRecordMap.emplace(std::make_pair(r,       // typeTable.tableView_->getDataView()[i][typeTable.tableView_->getColUID()],
				                                       true));  // init to delete

			// node instance loop
			for(auto& nodePair : nodeTypePair.second)
			{
				__COUTV__(nodePair.first);

				// default multi-node and array hostname info to empty
				std::vector<std::string> nodeIndices, hostnameIndices;
				unsigned int             hostnameFixedWidth = 0, nodeNameFixedWidth = 0;
				std::string              hostname;

				// keep a map original multinode values, to maintain node specific links
				//	(emplace when original node is delete)
				std::map<std::string /*originalMultiNode name*/, std::map<unsigned int /*col*/, std::string /*value*/>> originalMultinodeValues;

				// if original record is found, then commandeer that record
				//	else create a new record
				// Node properties: {originalName,hostname,subsystemName,(nodeArrString),(nodeNameFixedWidth),(hostnameArrString),(hostnameFixedWidth)}

				// node parameter loop
				for(unsigned int i = 0; i < nodePair.second.size(); ++i)
				{
					__COUTV__(nodePair.second[i]);

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

							std::vector<std::string> originalParameterArr =
							    StringMacros::getVectorFromString(&(nodePair.second[i].c_str()[1]), {':'} /*delimiter*/);

							if(originalParameterArr.size() != 3)
							{
								__SS__ << "Illegal original name parameter string '" << nodePair.second[i] << "!'" << __E__;
								__SS_THROW__;
							}

							unsigned int fixedWidth;
							sscanf(originalParameterArr[0].c_str(), "%u", &fixedWidth);
							__COUTV__(fixedWidth);

							std::vector<std::string> printerSyntaxArr = StringMacros::getVectorFromString(originalParameterArr[1], {','} /*delimiter*/);

							// unsigned int             count = 0;
							std::vector<std::string> originalNodeIndices;
							for(auto& printerSyntaxValue : printerSyntaxArr)
							{
								__COUTV__(printerSyntaxValue);

								std::vector<std::string> printerSyntaxRange = StringMacros::getVectorFromString(printerSyntaxValue, {'-'} /*delimiter*/);

								if(printerSyntaxRange.size() == 0 || printerSyntaxRange.size() > 2)
								{
									__SS__ << "Illegal multi-node printer syntax string '" << printerSyntaxValue << "!'" << __E__;
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
										__COUTV__(lo);
										originalNodeIndices.push_back(std::to_string(lo));
									}
								}
							}  // end printer syntax loop

							std::vector<std::string> originalNamePieces = StringMacros::getVectorFromString(originalParameterArr[2], {'*'} /*delimiter*/);
							__COUTV__(StringMacros::vectorToString(originalNamePieces));

							if(originalNamePieces.size() < 2)
							{
								__SS__ << "Illegal original multi-node name template - please use * to indicate where the multi-node index should be inserted!"
								       << __E__;
								__SS_THROW__;
							}

							// bool         isFirst     = true;
							unsigned int originalRow = TableView::INVALID, lastOriginalRow = TableView::INVALID;
							for(unsigned int i = 0; i < originalNodeIndices.size(); ++i)
							{
								std::string originalName = originalNamePieces[0];
								std::string nodeNameIndex;
								for(unsigned int p = 1; p < originalNamePieces.size(); ++p)
								{
									nodeNameIndex = originalNodeIndices[i];
									if(fixedWidth > 1)
									{
										if(nodeNameIndex.size() > fixedWidth)
										{
											__SS__ << "Illegal original node name index '" << nodeNameIndex
											       << "' - length is longer than fixed width requirement of " << fixedWidth << "!" << __E__;
											__SS_THROW__;
										}

										// 0 prepend as needed
										while(nodeNameIndex.size() < fixedWidth)
											nodeNameIndex = "0" + nodeNameIndex;
									}  // end fixed width handling

									originalName += nodeNameIndex + originalNamePieces[p];
								}
								__COUTV__(originalName);
								originalRow =
								    typeTable.tableView_->findRow(typeTable.tableView_->getColUID(), originalName, 0 /*offsetRow*/, true /*doNotThrow*/);
								__COUTV__(originalRow);

								// if have a new valid row, then delete last valid row
								if(originalRow != TableView::INVALID && lastOriginalRow != TableView::INVALID)
								{
									// before deleting, record all customizing values and maintain when saving
									originalMultinodeValues.emplace(std::make_pair(nodeName, std::map<unsigned int /*col*/, std::string /*value*/>()));

									__COUT__ << "Saving multinode value " << nodeName << "[" << lastOriginalRow
									         << "][*] with row count = " << typeTable.tableView_->getNumberOfRows() << __E__;

									// save all link values
									for(unsigned int col = 0; col < typeTable.tableView_->getNumberOfColumns(); ++col)
										if(typeTable.tableView_->getColumnInfo(col).getName() == ARTDAQTableBase::ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK ||
										   typeTable.tableView_->getColumnInfo(col).getName() == ARTDAQTableBase::ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK_UID)
											continue;  // skip subsystem link
										else if(typeTable.tableView_->getColumnInfo(col).isChildLink() ||
										        typeTable.tableView_->getColumnInfo(col).isChildLinkGroupID() ||
										        typeTable.tableView_->getColumnInfo(col).isChildLinkUID())
											originalMultinodeValues.at(nodeName).emplace(
											    std::make_pair(col, typeTable.tableView_->getDataView()[lastOriginalRow][col]));

									typeTable.tableView_->deleteRow(lastOriginalRow);
									if(originalRow > lastOriginalRow)
										--originalRow;  // modify after delete
								}

								if(originalRow != TableView::INVALID)
								{
									lastOriginalRow = originalRow;  // save last valid row for future deletion
									nodeName        = originalName;
								}

							}  // end loop through multi-node instances

							row = lastOriginalRow;  // take last valid row to proceed

							__COUTV__(nodeName);
							__COUTV__(row);

						}  // end handling of original multinode
						else
						{
							// attempt to find original 'single' node name
							row = typeTable.tableView_->findRow(typeTable.tableView_->getColUID(), nodePair.second[i], 0 /*offsetRow*/, true /*doNotThrow*/);
							__COUTV__(row);

							nodeName = nodePair.first;  // take new node name
						}

						__COUTV__(nodeName);
						if(row == TableView::INVALID)
						{
							// create artdaq type instance record
							row = typeTable.tableView_->addRow(author, true /*incrementUniqueData*/, nodeName);

							// fill defaults properties/parameters here!
							if(nodeTypePair.first == processTypes_.READER)
							{
								__COUT__ << "Handling new " << nodeTypePair.first << " defaults!" << __E__;
								TableEditStruct& daqParameterTable =
								    configGroupEdit.getTableEditStruct(ARTDAQTableBase::ARTDAQ_DAQ_PARAMETER_TABLE, true /*markModified*/);

								// create group link to daq parameter table
								typeTable.tableView_->setValueAsString(
								    ARTDAQTableBase::ARTDAQ_DAQ_PARAMETER_TABLE,
								    row,
								    typeTable.tableView_->findCol(ARTDAQTableBase::colARTDAQReader_.colLinkToDaqParameters_));
								std::string daqParameterGroupID = typeTable.tableView_->setUniqueColumnValue(
								    row,
								    typeTable.tableView_->findCol(ARTDAQTableBase::colARTDAQReader_.colLinkToDaqParametersGroupID_),
								    nodeName + "DaqParameters");

								typeTable.tableView_->print();

								// now create parameters at target link
								const std::vector<std::string> parameterUIDs = {"BoardID", "FragmentID"};

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
									parameterRow = daqParameterTable.tableView_->addRow(author, true /*incrementUniqueData*/, nodeName + parameterUIDs[i]);

									// set app status true
									daqParameterTable.tableView_->setValueAsString("1", parameterRow, daqParameterTable.tableView_->getColStatus());
									// set key
									daqParameterTable.tableView_->setValueAsString(
									    parameterNames[i],
									    parameterRow,
									    daqParameterTable.tableView_->findCol(ARTDAQTableBase::colARTDAQDaqParameter_.colDaqParameterKey_));
									// set value
									daqParameterTable.tableView_->setValueAsString(
									    parameterValues[i],
									    parameterRow,
									    daqParameterTable.tableView_->findCol(ARTDAQTableBase::colARTDAQDaqParameter_.colDaqParameterValue_));
									// set groupid
									daqParameterTable.tableView_->setValueAsString(
									    daqParameterGroupID,
									    parameterRow,
									    daqParameterTable.tableView_->findCol(ARTDAQTableBase::colARTDAQDaqParameter_.colDaqParameterGroupID_));

								}  // end Reader default property create loop

								daqParameterTable.tableView_->init();  // verify new table (throws runtime_errors)

							}  // end Reader default property setup
							else if(nodeTypePair.first == processTypes_.BUILDER || nodeTypePair.first == processTypes_.LOGGER ||
							        nodeTypePair.first == processTypes_.DISPATCHER)
							{
								__COUT__ << "Handling new " << nodeTypePair.first << " defaults!" << __E__;

								// goes through DAQ table
								TableEditStruct& daqTable = configGroupEdit.getTableEditStruct(ARTDAQTableBase::ARTDAQ_DAQ_TABLE, true /*markModified*/);
								// create DAQ record
								unsigned int daqRecordRow = daqTable.tableView_->addRow(author, true /*incrementUniqueData*/, nodeName + "Daq");
								std::string  daqRecordUID = daqTable.tableView_->getDataView()[daqRecordRow][daqTable.tableView_->getColUID()];

								// create unique link to daq table
								typeTable.tableView_->setValueAsString(
								    ARTDAQTableBase::ARTDAQ_DAQ_TABLE, row, typeTable.tableView_->findCol(ARTDAQTableBase::colARTDAQNotReader_.colLinkToDaq_));
								typeTable.tableView_->setValueAsString(
								    daqRecordUID, row, typeTable.tableView_->findCol(ARTDAQTableBase::colARTDAQNotReader_.colLinkToDaqUID_));

								TableEditStruct& daqParameterTable =
								    configGroupEdit.getTableEditStruct(ARTDAQTableBase::ARTDAQ_DAQ_PARAMETER_TABLE, true /*markModified*/);
								// create group link to daq parameter table
								daqTable.tableView_->setValueAsString(ARTDAQTableBase::ARTDAQ_DAQ_PARAMETER_TABLE,
								                                      daqRecordRow,
								                                      daqTable.tableView_->findCol(ARTDAQTableBase::colARTDAQDaq_.colLinkToDaqParameters_));
								std::string daqParameterGroupID = daqTable.tableView_->setUniqueColumnValue(
								    daqRecordRow,
								    daqTable.tableView_->findCol(ARTDAQTableBase::colARTDAQDaq_.colLinkToDaqParametersGroupID_),
								    nodeName + "DaqParameters");

								// now create parameters at target link
								const std::vector<std::string> parameterUIDs = {"BufferCount", "FragmentsPerEvent"};

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
									parameterRow = daqParameterTable.tableView_->addRow(author, true /*incrementUniqueData*/, nodeName + parameterUIDs[i]);

									// set app status true
									daqParameterTable.tableView_->setValueAsString("1", parameterRow, daqParameterTable.tableView_->getColStatus());
									// set key
									daqParameterTable.tableView_->setValueAsString(
									    parameterNames[i],
									    parameterRow,
									    daqParameterTable.tableView_->findCol(ARTDAQTableBase::colARTDAQDaqParameter_.colDaqParameterKey_));
									// set value
									daqParameterTable.tableView_->setValueAsString(
									    parameterValues[i],
									    parameterRow,
									    daqParameterTable.tableView_->findCol(ARTDAQTableBase::colARTDAQDaqParameter_.colDaqParameterValue_));
									// set groupid
									daqParameterTable.tableView_->setValueAsString(
									    daqParameterGroupID,
									    parameterRow,
									    daqParameterTable.tableView_->findCol(ARTDAQTableBase::colARTDAQDaqParameter_.colDaqParameterGroupID_));

								}  // end Reader default property create loop

								daqTable.tableView_->init();           // verify new table (throws runtime_errors)
								daqParameterTable.tableView_->init();  // verify new table (throws runtime_errors)

							}  // end Builder, Logger, Dispatcher default property setup
						}
						else  // set UID
						{
							typeTable.tableView_->setValueAsString(nodeName, row, typeTable.tableView_->getColUID());
						}
						__COUTV__(row);

						// remove from delete map
						deleteRecordMap[row] = false;

						__COUTV__(StringMacros::mapToString(processTypes_.mapToLinkGroupIDColumn_));

						// set GroupID
						typeTable.tableView_->setValueAsString(
						    artdaqSupervisorTable.tableView_->getDataView()[artdaqSupervisorRow][artdaqSupervisorTable.tableView_->findCol(
						        processTypes_.mapToLinkGroupIDColumn_.at(nodeTypePair.first))],
						    row,
						    typeTable.tableView_->findCol(processTypes_.mapToGroupIDColumn_.at(nodeTypePair.first)));
					}
					else if(i == 1)  // status
					{
						// enable/disable the target row
						typeTable.tableView_->setValueAsString(nodePair.second[i], row, typeTable.tableView_->getColStatus());
					}
					else if(i == 2)  // hostname
					{
						// set hostname
						hostname = nodePair.second[i];
						typeTable.tableView_->setValueAsString(hostname, row, typeTable.tableView_->findCol(ARTDAQ_TYPE_TABLE_HOSTNAME));
					}
					else if(i == 3)  // subsystemName
					{
						// set subsystemName
						if(nodePair.second[i] != "" && nodePair.second[i] != TableViewColumnInfo::DATATYPE_STRING_DEFAULT)
						{
							// real subsystem?
							if(subsystemObjectMap.find(nodePair.second[i]) == subsystemObjectMap.end())
							{
								__SS__ << "Illegal subsystem '" << nodePair.second[i] << "' mismatch!" << __E__;
								__SS_THROW__;
							}

							typeTable.tableView_->setValueAsString(
							    ARTDAQ_SUBSYSTEM_TABLE, row, typeTable.tableView_->findCol(ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK));
							typeTable.tableView_->setValueAsString(
							    nodePair.second[i], row, typeTable.tableView_->findCol(ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK_UID));
						}
						else  // no subsystem (i.e. default subsystem)
						{
							typeTable.tableView_->setValueAsString(
							    TableViewColumnInfo::DATATYPE_LINK_DEFAULT, row, typeTable.tableView_->findCol(ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK));
						}
					}
					else if(i == 4 || i == 5 || i == 6 || i == 7)  //(nodeArrString),(nodeNameFixedWidth),(hostnameArrString),(hostnameFixedWidth)
					{
						// fill multi-node and array hostname info to empty
						// then handle after all parameters in hand.

						__COUT__ << "Handling printer syntax i=" << i << __E__;

						std::vector<std::string> printerSyntaxArr = StringMacros::getVectorFromString(nodePair.second[i], {','} /*delimiter*/);

						if(printerSyntaxArr.size() == 2)  // consider if fixed value
						{
							if(printerSyntaxArr[0] == "nnfw")  // then node name fixed width
							{
								sscanf(printerSyntaxArr[1].c_str(), "%u", &nodeNameFixedWidth);
								__COUTV__(nodeNameFixedWidth);
								continue;
							}
							else if(printerSyntaxArr[0] == "hnfw")  // then hostname fixed width
							{
								sscanf(printerSyntaxArr[1].c_str(), "%u", &hostnameFixedWidth);
								__COUTV__(hostnameFixedWidth);
								continue;
							}
						}

						// unsigned int count = 0;
						for(auto& printerSyntaxValue : printerSyntaxArr)
						{
							__COUTV__(printerSyntaxValue);

							std::vector<std::string> printerSyntaxRange = StringMacros::getVectorFromString(printerSyntaxValue, {'-'} /*delimiter*/);
							if(printerSyntaxRange.size() == 0 || printerSyntaxRange.size() > 2)
							{
								__SS__ << "Illegal multi-node printer syntax string '" << printerSyntaxValue << "!'" << __E__;
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
									__COUTV__(lo);
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
						__SS__ << "Unexpected parameter[" << i << " '" << nodePair.second[i] << "' for node " << nodePair.first << "!" << __E__;
						__SS_THROW__;
					}
				}  // end node parameter loop

				__COUTV__(nodeIndices.size());
				__COUTV__(hostnameIndices.size());

				if(hostnameIndices.size())  // handle hostname array
				{
					if(hostnameIndices.size() != nodeIndices.size())
					{
						__SS__ << "Illegal associated hostname array has count " << hostnameIndices.size() << " which is not equal to the node count "
						       << nodeIndices.size() << "!" << __E__;
						__SS_THROW__;
					}
				}

				if(nodeIndices.size())  // handle multi-node instances
				{
					unsigned int hostnameCol = typeTable.tableView_->findCol(ARTDAQ_TYPE_TABLE_HOSTNAME);
					// Steps:
					//	first instance takes current row,
					//	then copy for remaining instances

					std::vector<std::string> namePieces = StringMacros::getVectorFromString(nodePair.first, {'*'} /*delimiter*/);
					__COUTV__(StringMacros::vectorToString(namePieces));

					if(namePieces.size() < 2)
					{
						__SS__ << "Illegal multi-node name template - please use * to indicate where the multi-node index should be inserted!" << __E__;
						__SS_THROW__;
					}

					std::vector<std::string> hostnamePieces;
					if(hostnameIndices.size())  // handle hostname array
					{
						hostnamePieces = StringMacros::getVectorFromString(hostname, {'*'} /*delimiter*/);
						__COUTV__(StringMacros::vectorToString(hostnamePieces));

						if(hostnamePieces.size() < 2)
						{
							__SS__ << "Illegal hostname array template - please use * to indicate where the hostname index should be inserted!" << __E__;
							__SS_THROW__;
						}
					}

					bool isFirst = true;
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
									__SS__ << "Illegal node name index '" << nodeNameIndex << "' - length is longer than fixed width requirement of "
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
										__SS__ << "Illegal hostname index '" << hostnameIndex << "' - length is longer than fixed width requirement of "
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
							typeTable.tableView_->setValueAsString(name, row, typeTable.tableView_->getColUID());

							typeTable.tableView_->setValueAsString(hostname, row, hostnameCol);

							// remove from delete map
							deleteRecordMap[row] = false;
						}
						else  // copy row
						{
							unsigned int copyRow = typeTable.tableView_->copyRows(
							    author, *(typeTable.tableView_), row, 1 /*srcRowsToCopy*/, -1 /*destOffsetRow*/, true /*generateUniqueDataColumns*/);
							typeTable.tableView_->setValueAsString(name, copyRow, typeTable.tableView_->getColUID());
							typeTable.tableView_->setValueAsString(hostname, copyRow, hostnameCol);

							// customize row if in original value map
							if(originalMultinodeValues.find(name) != originalMultinodeValues.end())
							{
								for(const auto& valuePair : originalMultinodeValues.at(name))
								{
									__COUT__ << "Customizing node: " << name << "[" << copyRow << "][" << valuePair.first << "] = " << valuePair.second
									         << __E__;
									typeTable.tableView_->setValueAsString(valuePair.second, copyRow, valuePair.first);
								}
							}

							// remove from delete map
							deleteRecordMap[copyRow] = false;
						}  // end copy and customize row handling

						isFirst = false;
					}  // end multi-node loop
				}      // end multi-node handling
			}          // end node record loop

			{  // delete record handling
				__COUT__ << "Deleting '" << nodeTypePair.first << "' records not specified..." << __E__;

				// unsigned int           row;
				std::set<unsigned int> orderedRowSet;  // need to delete in reverse order
				for(auto& deletePair : deleteRecordMap)
				{
					__COUTV__(deletePair.first);
					if(!deletePair.second)
						continue;  // only delete if true

					__COUTV__(deletePair.first);
					orderedRowSet.emplace(deletePair.first);
				}

				// delete elements in reverse order
				for(std::set<unsigned int>::reverse_iterator rit = orderedRowSet.rbegin(); rit != orderedRowSet.rend(); rit++)
					typeTable.tableView_->deleteRow(*rit);

			}  // end delete record handling

			{
				std::stringstream ss;
				typeTable.tableView_->print(ss);
				__COUT__ << ss.str();
			}

			typeTable.tableView_->init();  // verify new table (throws runtime_errors)

		}  // end node type loop

		{
			std::stringstream ss;
			artdaqSupervisorTable.tableView_->print(ss);
			__COUT__ << ss.str();
		}
		{
			std::stringstream ss;
			artdaqSubsystemTable.tableView_->print(ss);
			__COUT__ << ss.str();
		}

		artdaqSupervisorTable.tableView_->init();  // verify new table (throws runtime_errors)
		artdaqSubsystemTable.tableView_->init();   // verify new table (throws runtime_errors)
	}
	catch(...)
	{
		__COUT__ << "Table errors while creating ARTDAQ nodes. Erasing all newly "
		            "created table versions."
		         << __E__;
		throw;  // re-throw
	}           // end catch

	__COUT__ << "Edits complete for artdaq nodes and subsystems.. now save and activate groups, and update aliases!" << __E__;

	TableGroupKey newConfigurationGroupKey;
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
