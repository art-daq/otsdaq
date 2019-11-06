#include <fhiclcpp/ParameterSet.h>
#include <fhiclcpp/detail/print_mode.h>
#include <fhiclcpp/intermediate_table.h>
#include <fhiclcpp/make_ParameterSet.h>
#include <fhiclcpp/parse.h>

#include "otsdaq/TablePlugins/ARTDAQTableBase/ARTDAQTableBase.h"
#include "otsdaq/TablePlugins/XDAQContextTable.h"

#include <fstream>   // for std::ofstream
#include <iostream>  // std::cout
#include <typeinfo>

#include "otsdaq/ProgressBar/ProgressBar.h"

using namespace ots;

// clang-format off

const std::string ARTDAQTableBase::ARTDAQ_FCL_PATH 				= std::string(__ENV__("USER_DATA")) + "/" + "ARTDAQConfigurations/";

const std::string ARTDAQTableBase::ARTDAQ_SUPERVISOR_TABLE 		= "ARTDAQSupervisorTable";

const std::string ARTDAQTableBase::ARTDAQ_READER_TABLE 			= "ARTDAQBoardReaderTable";
const std::string ARTDAQTableBase::ARTDAQ_BUILDER_TABLE 		= "ARTDAQEventBuilderTable";
const std::string ARTDAQTableBase::ARTDAQ_LOGGER_TABLE 			= "ARTDAQDataLoggerTable";
const std::string ARTDAQTableBase::ARTDAQ_DISPATCHER_TABLE 		= "ARTDAQDispatcherTable";
const std::string ARTDAQTableBase::ARTDAQ_MONITOR_TABLE 		= "ARTDAQMonitorTable";

const std::string ARTDAQTableBase::ARTDAQ_SUBSYSTEM_TABLE 		= "ARTDAQSubsystemTable";

const std::string ARTDAQTableBase::ARTDAQ_TYPE_TABLE_HOSTNAME 				= "ExecutionHostname";
const std::string ARTDAQTableBase::ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK 		= "SubsystemLink";
const std::string ARTDAQTableBase::ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK_UID 	= "SubsystemLinkUID";


const int ARTDAQTableBase::NULL_SUBSYSTEM_DESTINATION 						= 0;
const std::string ARTDAQTableBase::NULL_SUBSYSTEM_DESTINATION_LABEL			= "nullDestinationSubsystem";

ARTDAQTableBase::ColARTDAQSupervisor 	ARTDAQTableBase::colARTDAQSupervisor_ 		= ARTDAQTableBase::ColARTDAQSupervisor();
ARTDAQTableBase::ColARTDAQSubsystem 	ARTDAQTableBase::colARTDAQSubsystem_ 		= ARTDAQTableBase::ColARTDAQSubsystem();

//Note: processTypes_ must be instantiate after the static artdaq table names (to construct map in constructor)
ARTDAQTableBase::ProcessTypes ARTDAQTableBase::processTypes_	= ARTDAQTableBase::ProcessTypes();


// clang-format on


//==============================================================================
// TableBase
//	If a valid string pointer is passed in accumulatedExceptions
//	then allowIllegalColumns is set for InfoReader
//	If accumulatedExceptions pointer = 0, then illegal columns throw std::runtime_error
// exception
ARTDAQTableBase::ARTDAQTableBase(std::string  tableName,
                                 std::string* accumulatedExceptions /* =0 */)
    : TableBase(tableName, accumulatedExceptions)
{
	// make directory just in case
	mkdir((ARTDAQ_FCL_PATH).c_str(), 0755);
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

//========================================================================================================================
const std::string& ARTDAQTableBase::getTypeString(ARTDAQTableBase::ARTDAQAppType type)
{
	switch(type)
	{
	case ARTDAQTableBase::ARTDAQAppType::EventBuilder:
		return processTypes_.BUILDER;
	case ARTDAQTableBase::ARTDAQAppType::DataLogger:
		return processTypes_.LOGGER;
	case ARTDAQTableBase::ARTDAQAppType::Dispatcher:
		return processTypes_.DISPATCHER;
	case ARTDAQTableBase::ARTDAQAppType::BoardReader:
		return processTypes_.READER;
	case ARTDAQTableBase::ARTDAQAppType::Monitor:
		return processTypes_.MONITOR;
	}
	// return "UNKNOWN";
	__SS__ << "Illegal translation attempt for type '" << (unsigned int)type << "'"
	       << __E__;
	__SS_THROW__;
}  // end getTypeString()

//========================================================================================================================
std::string ARTDAQTableBase::getFHICLFilename(ARTDAQTableBase::ARTDAQAppType type,
                                              const std::string&             name)
{
	//__COUT__ << "Type: " << ARTDAQTableBase::getTypeString(type) << " Name: " << name
	         //<< __E__;
	std::string filename = ARTDAQ_FCL_PATH + ARTDAQTableBase::getTypeString(type) + "-";
	std::string uid      = name;
	for(unsigned int i = 0; i < uid.size(); ++i)
		if((uid[i] >= 'a' && uid[i] <= 'z') || (uid[i] >= 'A' && uid[i] <= 'Z') ||
		   (uid[i] >= '0' && uid[i] <= '9'))  // only allow alpha numeric in file name
			filename += uid[i];

	filename += ".fcl";

	//__COUT__ << "fcl: " << filename << __E__;

	return filename;
}  // end getFHICLFilename()

//========================================================================================================================
std::string ARTDAQTableBase::getFlatFHICLFilename(ARTDAQTableBase::ARTDAQAppType type,
                                                  const std::string&             name)
{
	//__COUT__ << "Type: " << ARTDAQTableBase::getTypeString(type) << " Name: " << name
	//         << __E__;
	std::string filename = ARTDAQ_FCL_PATH + ARTDAQTableBase::getTypeString(type) + "-";
	std::string uid      = name;
	for(unsigned int i = 0; i < uid.size(); ++i)
		if((uid[i] >= 'a' && uid[i] <= 'z') || (uid[i] >= 'A' && uid[i] <= 'Z') ||
		   (uid[i] >= '0' && uid[i] <= '9'))  // only allow alpha numeric in file name
			filename += uid[i];

	filename += "_flattened.fcl";

	//__COUT__ << "fcl: " << filename << __E__;

	return filename;
}  // end getFlatFHICLFilename()

//========================================================================================================================
void ARTDAQTableBase::flattenFHICL(ARTDAQTableBase::ARTDAQAppType type,
                                   const std::string&             name)
{
	__COUT__ << "flattenFHICL()" << __E__;
	std::string inFile  = getFHICLFilename(type, name);
	std::string outFile = getFlatFHICLFilename(type, name);

	__COUTV__(__ENV__("FHICL_FILE_PATH"));

	cet::filepath_lookup_nonabsolute policy("FHICL_FILE_PATH");

	fhicl::ParameterSet pset;

	try
	{
		fhicl::intermediate_table tbl;
		fhicl::parse_document(inFile, policy, tbl);
		fhicl::ParameterSet pset;
		fhicl::make_ParameterSet(tbl, pset);

		std::ofstream ofs{outFile};
		ofs << pset.to_indented_string(0, fhicl::detail::print_mode::annotated);
	}
	catch(cet::exception const& e)
	{
		__SS__ << "Failed to parse fhicl: " << e.what() << __E__;
		__SS_THROW__;
	}
}  // end flattenFHICL()

//========================================================================================================================
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
                                       bool onlyInsertAtTableParameters /*=false*/,
                                       bool includeAtTableParameters /*=false*/)
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
					if(!parameter.second.getNode(TableViewColumnInfo::COL_NAME_STATUS)
					        .getValue<bool>())
						PUSHCOMMENT;

					OUT << key;

					// skip connecting : if special keywords found
					OUT << parameter.second.getNode(parameterPreamble + "Value")
					           .getValue()
					    << "\n";

					if(!parameter.second.getNode(TableViewColumnInfo::COL_NAME_STATUS)
					        .getValue<bool>())
						POPCOMMENT;
				}
				// else skip it

				continue;
			}
			// else NOT @table:: keyword parameter

			if(onlyInsertAtTableParameters)
				continue;  // skip all other types

			if(!parameter.second.getNode(TableViewColumnInfo::COL_NAME_STATUS)
			        .getValue<bool>())
				PUSHCOMMENT;

			OUT << key;

			// skip connecting : if special keywords found
			if(key.find("#include") == std::string::npos)
				OUT << ":";
			OUT << parameter.second.getNode(parameterPreamble + "Value").getValue()
			    << "\n";

			if(!parameter.second.getNode(TableViewColumnInfo::COL_NAME_STATUS)
			        .getValue<bool>())
				POPCOMMENT;
		}
	}
	// else
	//	__COUT__ << "No parameters found" << __E__;

}  // end insertParameters()

//========================================================================================================================
// insertModuleType
//	Inserts module type field, with consideration for @table::
std::string ARTDAQTableBase::insertModuleType(std::ostream&     out,
                                              std::string&      tabStr,
                                              std::string&      commentStr,
                                              ConfigurationTree moduleTypeNode)
{
	std::string value = moduleTypeNode.getValue();

	OUT;
	if(value.find("@table::") == std::string::npos)
		out << "module_type: ";
	out << value << "\n";

	return value;
}  // end insertModuleType()

//========================================================================================================================
void ARTDAQTableBase::outputReaderFHICL(
    const ConfigurationTree& boardReaderNode,
    size_t maxFragmentSizeBytes /* = ARTDAQTableBase::DEFAULT_MAX_FRAGMENT_SIZE */)
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

	std::string filename = ARTDAQTableBase::getFHICLFilename(
	    ARTDAQTableBase::ARTDAQAppType::BoardReader, boardReaderNode.getValue());

	/////////////////////////
	// generate xdaq run parameter file
	std::fstream out;

	std::string tabStr     = "";
	std::string commentStr = "";

	out.open(filename, std::fstream::out | std::fstream::trunc);
	if(out.fail())
	{
		__SS__ << "Failed to open ARTDAQ Builder fcl file: " << filename << __E__;
		__SS_THROW__;
	}

	//--------------------------------------
	// header
	OUT << "###########################################################" << __E__;
	OUT << "#" << __E__;
	OUT << "# artdaq reader fcl configuration file produced by otsdaq." << __E__;
	OUT << "# 	Creation timestamp: " << StringMacros::getTimestampString() << __E__;
	OUT << "# 	Original filename: " << filename << __E__;
	OUT << "#	otsdaq-ARTDAQ Reader UID: " << boardReaderNode.getValue() << __E__;
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
	// handle daq
	OUT << "daq: {\n";

	// fragment_receiver
	PUSHTAB;
	OUT << "fragment_receiver: {\n";

	PUSHTAB;
	OUT << "max_fragment_size_bytes: " << maxFragmentSizeBytes << "\n";
	{
		// plugin type and fragment data-type
		OUT << "generator"
		    << ": " << boardReaderNode.getNode("daqGeneratorPluginType").getValue()
		    << ("\t #daq generator plug-in type") << "\n";
		OUT << "fragment_type"
		    << ": " << boardReaderNode.getNode("daqGeneratorFragmentType").getValue()
		    << ("\t #generator data fragment type") << "\n\n";

		// shared and unique parameters
		auto parametersLink = boardReaderNode.getNode("daqParametersLink");
		if(!parametersLink.isDisconnected())
		{
			auto parameters = parametersLink.getChildren();
			for(auto& parameter : parameters)
			{
				if(!parameter.second.getNode(TableViewColumnInfo::COL_NAME_STATUS)
				        .getValue<bool>())
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

				if(!parameter.second.getNode(TableViewColumnInfo::COL_NAME_STATUS)
				        .getValue<bool>())
					POPCOMMENT;
			}
		}
		OUT << "\n";  // end daq board reader parameters
	}

	OUT << "destinations: {\n";

	OUT << "}\n\n";  // end destinations

	POPTAB;
	OUT << "}\n\n";  // end fragment_receiver

	OUT << "metrics: {\n";

	PUSHTAB;
	auto metricsGroup = boardReaderNode.getNode("daqMetricsLink");
	if(!metricsGroup.isDisconnected())
	{
		auto metrics = metricsGroup.getChildren();

		for(auto& metric : metrics)
		{
			if(!metric.second.getNode(TableViewColumnInfo::COL_NAME_STATUS)
			        .getValue<bool>())
				PUSHCOMMENT;

			OUT << metric.second.getNode("metricKey").getValue() << ": {\n";
			PUSHTAB;

			OUT << "metricPluginType: "
			    << metric.second.getNode("metricPluginType").getValue() << "\n";
			OUT << "level: " << metric.second.getNode("metricLevel").getValue() << "\n";

			auto metricParametersGroup = metric.second.getNode("metricParametersLink");
			if(!metricParametersGroup.isDisconnected())
			{
				auto metricParameters = metricParametersGroup.getChildren();
				for(auto& metricParameter : metricParameters)
				{
					if(!metricParameter.second
					        .getNode(TableViewColumnInfo::COL_NAME_STATUS)
					        .getValue<bool>())
						PUSHCOMMENT;

					OUT << metricParameter.second.getNode("metricParameterKey").getValue()
					    << ": "
					    << metricParameter.second.getNode("metricParameterValue")
					           .getValue()
					    << "\n";

					if(!metricParameter.second
					        .getNode(TableViewColumnInfo::COL_NAME_STATUS)
					        .getValue<bool>())
						POPCOMMENT;
				}
			}
			POPTAB;
			OUT << "}\n\n";  // end metric

			if(!metric.second.getNode(TableViewColumnInfo::COL_NAME_STATUS)
			        .getValue<bool>())
				POPCOMMENT;
		}
	}
	POPTAB;
	OUT << "}\n\n";  // end metrics

	POPTAB;
	OUT << "}\n\n";  // end daq

	out.close();
}  // end outputReaderFHICL()

//========================================================================================================================
// outputDataReceiverFHICL
//	Note: currently selfRank and selfPort are unused by artdaq fcl
void ARTDAQTableBase::outputDataReceiverFHICL(const ConfigurationTree& receiverNode,
                                              ARTDAQTableBase::ARTDAQAppType appType,
                                              size_t maxFragmentSizeBytes)
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
	OUT << "# artdaq " << getTypeString(appType)
	    << " fcl configuration file produced by otsdaq." << __E__;
	OUT << "# 	Creation timestamp: " << StringMacros::getTimestampString() << __E__;
	OUT << "# 	Original filename: " << filename << __E__;
	OUT << "#	otsdaq-ARTDAQ " << getTypeString(appType)
	    << " UID: " << receiverNode.getValue() << __E__;
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
	ARTDAQTableBase::insertParameters(out,
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

		OUT << "max_fragment_size_bytes: " << maxFragmentSizeBytes << "\n";
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
		ARTDAQTableBase::insertParameters(out,
		                                  tabStr,
		                                  commentStr,
		                                  daq.getNode("daqParametersLink"),
		                                  "daqParameter" /*parameterType*/,
		                                  false /*onlyInsertAtTableParameters*/,
		                                  true /*includeAtTableParameters*/);

		//__COUT__ << "Adding sources placeholder" << __E__;
		OUT << "sources: {\n"
		    << "}\n\n";  // end sources

		POPTAB;
		OUT << "}\n\n";  // end event builder

		//__COUT__ << "Filling in metrics" << __E__;
		OUT << "metrics: {\n";

		PUSHTAB;
		auto metricsGroup = daq.getNode("daqMetricsLink");
		if(!metricsGroup.isDisconnected())
		{
			auto metrics = metricsGroup.getChildren();

			for(auto& metric : metrics)
			{
				if(!metric.second.getNode(TableViewColumnInfo::COL_NAME_STATUS)
				        .getValue<bool>())
					PUSHCOMMENT;

				OUT << metric.second.getNode("metricKey").getValue() << ": {\n";
				PUSHTAB;

				OUT << "metricPluginType: "
				    << metric.second.getNode("metricPluginType").getValue() << "\n";
				OUT << "level: " << metric.second.getNode("metricLevel").getValue()
				    << "\n";

				//--------------------------------------
				// handle ALL metric parameters
				ARTDAQTableBase::insertParameters(
				    out,
				    tabStr,
				    commentStr,
				    metric.second.getNode("metricParametersLink"),
				    "metricParameter" /*parameterType*/,
				    false /*onlyInsertAtTableParameters*/,
				    true /*includeAtTableParameters*/);

				POPTAB;
				OUT << "}\n\n";  // end metric

				if(!metric.second.getNode(TableViewColumnInfo::COL_NAME_STATUS)
				        .getValue<bool>())
					POPCOMMENT;
			}
		}
		POPTAB;
		OUT << "}\n\n";  // end metrics

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
			ARTDAQTableBase::insertParameters(out,
			                                  tabStr,
			                                  commentStr,
			                                  services.getNode("ServicesParametersLink"),
			                                  "daqParameter" /*parameterType*/,
			                                  true /*onlyInsertAtTableParameters*/,
			                                  false /*includeAtTableParameters*/);

			// scheduler
			OUT << "scheduler: {\n";

#if ART_HEX_VERSION < 0x30200
			PUSHTAB;
			//		OUT << "fileMode: " <<
			// services.getNode("schedulerFileMode").getValue()
			//<<
			//"\n";
			OUT << "errorOnFailureToPut: "
			    << (services.getNode("schedulerErrorOnFailtureToPut").getValue<bool>()
			            ? "true"
			            : "false")
			    << "\n";
			POPTAB;
#endif
			OUT << "}\n\n";

			// NetMonTransportServiceInterface
			OUT << "NetMonTransportServiceInterface: {\n";

			PUSHTAB;
			OUT << "service_provider: " <<
			    // services.getNode("NetMonTrasportServiceInterfaceServiceProvider").getEscapedValue()
			    services.getNode("NetMonTrasportServiceInterfaceServiceProvider")
			        .getValue()
			    << "\n";

			OUT << "destinations: {\n"
			    << "}\n\n";  // end destinations

			POPTAB;
			OUT << "}\n\n";  // end NetMonTransportServiceInterface

			//--------------------------------------
			// handle services NOT @table:: parameters
			ARTDAQTableBase::insertParameters(out,
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
				if(!outputPlugin.second.getNode(TableViewColumnInfo::COL_NAME_STATUS)
				        .getValue<bool>())
					PUSHCOMMENT;

				OUT << outputPlugin.second.getNode("outputKey").getValue() << ": {\n";
				PUSHTAB;

				std::string moduleType = ARTDAQTableBase::insertModuleType(
				    out,
				    tabStr,
				    commentStr,
				    outputPlugin.second.getNode("outputModuleType"));

				//--------------------------------------
				// handle ALL output parameters
				ARTDAQTableBase::insertParameters(
				    out,
				    tabStr,
				    commentStr,
				    outputPlugin.second.getNode("outputModuleParameterLink"),
				    "outputParameter" /*parameterType*/,
				    false /*onlyInsertAtTableParameters*/,
				    true /*includeAtTableParameters*/);

				if(outputPlugin.second.getNode("outputModuleType").getValue() ==
				   "BinaryNetOutput")
				{
					OUT << "destinations: {\n"
					    << "}\n\n";  // end destinations
				}

				POPTAB;
				OUT << "}\n\n";  // end output module

				if(!outputPlugin.second.getNode(TableViewColumnInfo::COL_NAME_STATUS)
				        .getValue<bool>())
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
			ARTDAQTableBase::insertParameters(
			    out,
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
					if(!module.second.getNode(TableViewColumnInfo::COL_NAME_STATUS)
					        .getValue<bool>())
						PUSHCOMMENT;

					//--------------------------------------
					// handle only @table:: analyzer parameters
					ARTDAQTableBase::insertParameters(
					    out,
					    tabStr,
					    commentStr,
					    module.second.getNode("analyzerModuleParameterLink"),
					    "analyzerParameter" /*parameterType*/,
					    true /*onlyInsertAtTableParameters*/,
					    false /*includeAtTableParameters*/);

					OUT << module.second.getNode("analyzerKey").getValue() << ": {\n";
					PUSHTAB;
					ARTDAQTableBase::insertModuleType(
					    out,
					    tabStr,
					    commentStr,
					    module.second.getNode("analyzerModuleType"));

					//--------------------------------------
					// handle NOT @table:: producer parameters
					ARTDAQTableBase::insertParameters(
					    out,
					    tabStr,
					    commentStr,
					    module.second.getNode("analyzerModuleParameterLink"),
					    "analyzerParameter" /*parameterType*/,
					    false /*onlyInsertAtTableParameters*/,
					    false /*includeAtTableParameters*/);

					POPTAB;
					OUT << "}\n\n";  // end analyzer module

					if(!module.second.getNode(TableViewColumnInfo::COL_NAME_STATUS)
					        .getValue<bool>())
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
					if(!module.second.getNode(TableViewColumnInfo::COL_NAME_STATUS)
					        .getValue<bool>())
						PUSHCOMMENT;

					//--------------------------------------
					// handle only @table:: producer parameters
					ARTDAQTableBase::insertParameters(
					    out,
					    tabStr,
					    commentStr,
					    module.second.getNode("producerModuleParameterLink"),
					    "producerParameter" /*parameterType*/,
					    true /*onlyInsertAtTableParameters*/,
					    false /*includeAtTableParameters*/);

					OUT << module.second.getNode("producerKey").getValue() << ": {\n";
					PUSHTAB;

					ARTDAQTableBase::insertModuleType(
					    out,
					    tabStr,
					    commentStr,
					    module.second.getNode("producerModuleType"));

					//--------------------------------------
					// handle NOT @table:: producer parameters
					ARTDAQTableBase::insertParameters(
					    out,
					    tabStr,
					    commentStr,
					    module.second.getNode("producerModuleParameterLink"),
					    "producerParameter" /*parameterType*/,
					    false /*onlyInsertAtTableParameters*/,
					    false /*includeAtTableParameters*/);

					POPTAB;
					OUT << "}\n\n";  // end producer module

					if(!module.second.getNode(TableViewColumnInfo::COL_NAME_STATUS)
					        .getValue<bool>())
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
					if(!module.second.getNode(TableViewColumnInfo::COL_NAME_STATUS)
					        .getValue<bool>())
						PUSHCOMMENT;

					//--------------------------------------
					// handle only @table:: filter parameters
					ARTDAQTableBase::insertParameters(
					    out,
					    tabStr,
					    commentStr,
					    module.second.getNode("filterModuleParameterLink"),
					    "filterParameter" /*parameterType*/,
					    true /*onlyInsertAtTableParameters*/,
					    false /*includeAtTableParameters*/);

					OUT << module.second.getNode("filterKey").getValue() << ": {\n";
					PUSHTAB;

					ARTDAQTableBase::insertModuleType(
					    out,
					    tabStr,
					    commentStr,
					    module.second.getNode("filterModuleType"));

					//--------------------------------------
					// handle NOT @table:: filter parameters
					ARTDAQTableBase::insertParameters(
					    out,
					    tabStr,
					    commentStr,
					    module.second.getNode("filterModuleParameterLink"),
					    "filterParameter" /*parameterType*/,
					    false /*onlyInsertAtTableParameters*/,
					    false /*includeAtTableParameters*/);

					POPTAB;
					OUT << "}\n\n";  // end filter module

					if(!module.second.getNode(TableViewColumnInfo::COL_NAME_STATUS)
					        .getValue<bool>())
						POPCOMMENT;
				}
				POPTAB;
				OUT << "}\n\n";  // end filter
			}

			//--------------------------------------
			// handle NOT @table:: physics parameters
			ARTDAQTableBase::insertParameters(
			    out,
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

			ARTDAQTableBase::insertModuleType(
			    out, tabStr, commentStr, source.getNode("sourceModuleType"));

			OUT << "waiting_time: " << source.getNode("sourceWaitingTime").getValue()
			    << "\n";
			OUT << "resume_after_timeout: "
			    << (source.getNode("sourceResumeAfterTimeout").getValue<bool>() ? "true"
			                                                                    : "false")
			    << "\n";
			POPTAB;
			OUT << "}\n\n";  // end source
		}

		//--------------------------------------
		// handle process_name
		//__COUT__ << "Writing art.process_name" << __E__;
		OUT << "process_name: " << art.getNode("ProcessName") << "\n";

		POPTAB;
		OUT << "}\n\n";  // end art
	}

	//--------------------------------------
	// handle ALL add-on parameters
	//__COUT__ << "Inserting add-on parameters" << __E__;
	ARTDAQTableBase::insertParameters(out,
	                                  tabStr,
	                                  commentStr,
	                                  receiverNode.getNode("addOnParametersLink"),
	                                  "daqParameter" /*parameterType*/,
	                                  false /*onlyInsertAtTableParameters*/,
	                                  true /*includeAtTableParameters*/);

	//__COUT__ << "outputDataReceiverFHICL DONE" << __E__;
	out.close();
}  // end outputDataReceiverFHICL()

//========================================================================================================================
void ARTDAQTableBase::extractArtdaqInfo(
    ConfigurationTree                                        artdaqSupervisorNode,
    std::map<int /*subsystem ID*/, ARTDAQTableBase::SubsystemInfo>& subsystems,
    std::map<ARTDAQTableBase::ARTDAQAppType, std::list<ARTDAQTableBase::ProcessInfo>>& processes,
    bool         doWriteFHiCL /* = false */,
    size_t       maxFragmentSizeBytes /* = ARTDAQTableBase::DEFAULT_MAX_FRAGMENT_SIZE*/,
    ProgressBar* progressBar /* =0 */)
{
	if(progressBar)
		progressBar->step();

	subsystems[ARTDAQTableBase::NULL_SUBSYSTEM_DESTINATION].id = ARTDAQTableBase::NULL_SUBSYSTEM_DESTINATION;
	subsystems[ARTDAQTableBase::NULL_SUBSYSTEM_DESTINATION].label = ARTDAQTableBase::NULL_SUBSYSTEM_DESTINATION_LABEL;

	std::list<ARTDAQTableBase::ProcessInfo>& readerInfo =
	    processes[ARTDAQTableBase::ARTDAQAppType::BoardReader];
	{
		__COUT__ << "Checking for BoardReaders" << __E__;
		auto readersLink = artdaqSupervisorNode.getNode(
				ARTDAQTableBase::colARTDAQSupervisor_.colLinkToBoardReaders_);
		if(!readersLink.isDisconnected() && readersLink.getChildren().size() > 0)
		{
			auto readers = readersLink.getChildren();
			__COUT__ << "There are " << readers.size() << " configured BoardReaders"
			         << __E__;

			for(auto& reader : readers)
			{
				if(reader.second.getNode(TableViewColumnInfo::COL_NAME_STATUS)
				       .getValue<bool>())
				{
					auto readerUID = reader.second.getNode("SupervisorUID").getValue();
					auto readerHost =
					    reader.second.getNode("ExecutionHostname").getValue();

					if(readerHost == TableViewColumnInfo::DATATYPE_STRING_DEFAULT)
						readerHost = "localhost";

					auto readerSubsystemID     = 1;
					auto readerSubsystemLink = reader.second.getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK);
					if(!readerSubsystemLink.isDisconnected())
					{
						readerSubsystemID = ARTDAQTableBase::getSubsytemId(readerSubsystemLink);
						__COUTV__(readerSubsystemID);
						subsystems[readerSubsystemID].id = readerSubsystemID;

						const std::string& readerSubsystemName =
								readerSubsystemLink.getUIDAsString();
						__COUTV__(readerSubsystemName);

						subsystems[readerSubsystemID].label = readerSubsystemName;

						auto readerSubsystemDestinationLink =
								readerSubsystemLink.getNode(ARTDAQTableBase::colARTDAQSubsystem_.colLinkToDestination_);
						if(readerSubsystemDestinationLink.isDisconnected())
						{
							//default to no destination when no link
							subsystems[readerSubsystemID].destination = 0;
						}
						else
						{
							//get destination subsystem id
							subsystems[readerSubsystemID].destination =
									ARTDAQTableBase::getSubsytemId(readerSubsystemDestinationLink);
						}
						__COUTV__(subsystems[readerSubsystemID].destination);

						//add this subsystem to destination subsystem's sources, if not there
						if(!subsystems.count(subsystems[readerSubsystemID].destination) ||
						   !subsystems[subsystems[readerSubsystemID].destination]
						        .sources.count(readerSubsystemID))
						{
							subsystems[subsystems[readerSubsystemID].destination]
							    .sources.insert(readerSubsystemID);
						}

					} //end subsystem instantiation

					__COUT__ << "Found BoardReader with UID " << readerUID
					         << ", DAQInterface Hostname " << readerHost
					         << ", and Subsystem " << readerSubsystemID << __E__;
					readerInfo.emplace_back(readerUID, readerHost, readerSubsystemID);

					if(doWriteFHiCL)
					{
						ARTDAQTableBase::outputReaderFHICL(
								reader.second,
								maxFragmentSizeBytes);

						ARTDAQTableBase::flattenFHICL(
								ARTDAQTableBase::ARTDAQAppType::BoardReader,
								reader.second.getValue());
					}
				}
				else
				{
					__COUT__ << "BoardReader "
					         << reader.second.getNode("SupervisorUID").getValue()
					         << " is disabled." << __E__;
				}
			}
		}
		else
		{
			__SS__ << "Error: There should be at least one BoardReader!";
			__SS_THROW__;
			return;
		}
	}

	if(progressBar)
		progressBar->step();

	std::list<ARTDAQTableBase::ProcessInfo>& builderInfo =
	    processes[ARTDAQTableBase::ARTDAQAppType::EventBuilder];
	{
		auto buildersLink = artdaqSupervisorNode.getNode(
				ARTDAQTableBase::colARTDAQSupervisor_.colLinkToEventBuilders_);
		if(!buildersLink.isDisconnected() && buildersLink.getChildren().size() > 0)
		{
			auto builders = buildersLink.getChildren();

			for(auto& builder : builders)
			{
				if(builder.second.getNode(TableViewColumnInfo::COL_NAME_STATUS)
				       .getValue<bool>())
				{
					auto builderUID = builder.second.getNode("SupervisorUID").getValue();
					auto builderHost =
					    builder.second.getNode("ExecutionHostname").getValue();

					if(builderHost == TableViewColumnInfo::DATATYPE_STRING_DEFAULT)
						builderHost = "localhost";

					auto builderSubsystemID     = 1;
					auto builderSubsystemLink = builder.second.getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK);
					if(!builderSubsystemLink.isDisconnected())
					{
						builderSubsystemID = ARTDAQTableBase::getSubsytemId(builderSubsystemLink);
						__COUTV__(builderSubsystemID);

						subsystems[builderSubsystemID].id = builderSubsystemID;

						const std::string& builderSubsystemName =
								builderSubsystemLink.getUIDAsString();
						__COUTV__(builderSubsystemName);

						subsystems[builderSubsystemID].label = builderSubsystemName;

						auto builderSubsystemDestinationLink =
								builderSubsystemLink.getNode(ARTDAQTableBase::colARTDAQSubsystem_.colLinkToDestination_);
						if(builderSubsystemDestinationLink.isDisconnected())
						{
							//default to no destination when no link
							subsystems[builderSubsystemID].destination = 0;
						}
						else
						{
							//get destination subsystem id
							subsystems[builderSubsystemID].destination =
									ARTDAQTableBase::getSubsytemId(builderSubsystemDestinationLink);
						}
						__COUTV__(subsystems[builderSubsystemID].destination);

						//add this subsystem to destination subsystem's sources, if not there
						if(!subsystems.count(subsystems[builderSubsystemID].destination) ||
						   !subsystems[subsystems[builderSubsystemID].destination]
						        .sources.count(builderSubsystemID))
						{
							subsystems[subsystems[builderSubsystemID].destination]
							    .sources.insert(builderSubsystemID);
						}

					} //end subsystem instantiation

					__COUT__ << "Found EventBuilder with UID " << builderUID
					         << ", on Hostname " << builderHost
					         << ", in Subsystem " << builderSubsystemID << __E__;
					builderInfo.emplace_back(builderUID, builderHost, builderSubsystemID);

					if(doWriteFHiCL)
					{
						ARTDAQTableBase::outputDataReceiverFHICL(
								builder.second,
								ARTDAQTableBase::ARTDAQAppType::EventBuilder,
								maxFragmentSizeBytes);

						ARTDAQTableBase::flattenFHICL(
								ARTDAQTableBase::ARTDAQAppType::EventBuilder,
								builder.second.getValue());
					}
				}
				else
				{
					__COUT__ << "EventBuilder "
					         << builder.second.getNode("SupervisorUID").getValue()
					         << " is disabled." << __E__;
				}
			}
		}
		else
		{
			__SS__ << "Error: There should be at least one EventBuilder!";
			__SS_THROW__;
			return;
		}
	}

	if(progressBar)
		progressBar->step();

	std::list<ARTDAQTableBase::ProcessInfo>& loggerInfo =
	    processes[ARTDAQTableBase::ARTDAQAppType::DataLogger];
	{
		auto dataloggersLink = artdaqSupervisorNode.getNode(
				ARTDAQTableBase::colARTDAQSupervisor_.colLinkToDataLoggers_);
		if(!dataloggersLink.isDisconnected())
		{
			auto dataloggers = dataloggersLink.getChildren();

			for(auto& datalogger : dataloggers)
			{
				if(datalogger.second.getNode(TableViewColumnInfo::COL_NAME_STATUS)
				       .getValue<bool>())
				{
					auto loggerHost =
					    datalogger.second.getNode("ExecutionHostname").getValue();
					auto loggerUID =
					    datalogger.second.getNode("SupervisorUID").getValue();

					if(loggerHost == TableViewColumnInfo::DATATYPE_STRING_DEFAULT)
						loggerHost = "localhost";

					auto loggerSubsystemID     = 1;
					auto loggerSubsystemLink = datalogger.second.getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK);
					if(!loggerSubsystemLink.isDisconnected())
					{
						loggerSubsystemID = ARTDAQTableBase::getSubsytemId(loggerSubsystemLink);
						__COUTV__(loggerSubsystemID);
						subsystems[loggerSubsystemID].id = loggerSubsystemID;

						const std::string& loggerSubsystemName =
								loggerSubsystemLink.getUIDAsString();
						__COUTV__(loggerSubsystemName);

						subsystems[loggerSubsystemID].label = loggerSubsystemName;

						auto loggerSubsystemDestinationLink =
								loggerSubsystemLink.getNode(ARTDAQTableBase::colARTDAQSubsystem_.colLinkToDestination_);
						if(loggerSubsystemDestinationLink.isDisconnected())
						{
							//default to no destination when no link
							subsystems[loggerSubsystemID].destination = 0;
						}
						else
						{
							//get destination subsystem id
							subsystems[loggerSubsystemID].destination =
									ARTDAQTableBase::getSubsytemId(loggerSubsystemDestinationLink);
						}
						__COUTV__(subsystems[loggerSubsystemID].destination);

						//add this subsystem to destination subsystem's sources, if not there
						if(!subsystems.count(subsystems[loggerSubsystemID].destination) ||
						   !subsystems[subsystems[loggerSubsystemID].destination]
						        .sources.count(loggerSubsystemID))
						{
							subsystems[subsystems[loggerSubsystemID].destination]
							    .sources.insert(loggerSubsystemID);
						}

					} //end subsystem instantiation

					__COUT__ << "Found DataLogger with UID " << loggerUID
					         << ", DAQInterface Hostname " << loggerHost
					         << ", and Subsystem " << loggerSubsystemID << __E__;
					loggerInfo.emplace_back(loggerUID, loggerHost, loggerSubsystemID);

					if(doWriteFHiCL)
					{
						ARTDAQTableBase::outputDataReceiverFHICL(
								datalogger.second,
								ARTDAQTableBase::ARTDAQAppType::DataLogger,
								maxFragmentSizeBytes);

						ARTDAQTableBase::flattenFHICL(
								ARTDAQTableBase::ARTDAQAppType::DataLogger,
								datalogger.second.getValue());
					}
				}
				else
				{
					__COUT__ << "DataLogger "
					         << datalogger.second.getNode("SupervisorUID").getValue()
					         << " is disabled." << __E__;
				}
			}
		}
	}

	std::list<ARTDAQTableBase::ProcessInfo>& dispatcherInfo =
	    processes[ARTDAQTableBase::ARTDAQAppType::Dispatcher];
	{
		auto dispatchersLink = artdaqSupervisorNode.getNode(
				ARTDAQTableBase::colARTDAQSupervisor_.colLinkToDispatchers_);
		if(!dispatchersLink.isDisconnected())
		{
			auto dispatchers = dispatchersLink.getChildren();

			for(auto& dispatcher : dispatchers)
			{
				if(dispatcher.second.getNode(TableViewColumnInfo::COL_NAME_STATUS)
				       .getValue<bool>())
				{
					auto dispatcherHost =
					    dispatcher.second.getNode("ExecutionHostname").getValue();
					auto dispatcherUID = dispatcher.second.getNode("SupervisorUID").getValue();

					if(dispatcherHost == TableViewColumnInfo::DATATYPE_STRING_DEFAULT)
						dispatcherHost = "localhost";

					auto dispatcherSubsystemID     = 1;
					auto dispatcherSubsystemLink = dispatcher.second.getNode(ARTDAQTableBase::ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK);
					if(!dispatcherSubsystemLink.isDisconnected())
					{
						dispatcherSubsystemID = ARTDAQTableBase::getSubsytemId(dispatcherSubsystemLink);
						__COUTV__(dispatcherSubsystemID);
						subsystems[dispatcherSubsystemID].id = dispatcherSubsystemID;

						const std::string& dispatcherSubsystemName =
								dispatcherSubsystemLink.getUIDAsString();
						__COUTV__(dispatcherSubsystemName);

						subsystems[dispatcherSubsystemID].label = dispatcherSubsystemName;

						auto dispatcherSubsystemDestinationLink =
								dispatcherSubsystemLink.getNode(ARTDAQTableBase::colARTDAQSubsystem_.colLinkToDestination_);
						if(dispatcherSubsystemDestinationLink.isDisconnected())
						{
							//default to no destination when no link
							subsystems[dispatcherSubsystemID].destination = 0;
						}
						else
						{
							//get destination subsystem id
							subsystems[dispatcherSubsystemID].destination =
									ARTDAQTableBase::getSubsytemId(dispatcherSubsystemDestinationLink);
						}
						__COUTV__(subsystems[dispatcherSubsystemID].destination);

						//add this subsystem to destination subsystem's sources, if not there
						if(!subsystems.count(subsystems[dispatcherSubsystemID].destination) ||
						   !subsystems[subsystems[dispatcherSubsystemID].destination]
						        .sources.count(dispatcherSubsystemID))
						{
							subsystems[subsystems[dispatcherSubsystemID].destination]
							    .sources.insert(dispatcherSubsystemID);
						}
					}

					__COUT__ << "Found Dispatcher with UID " << dispatcherUID
					         << ", DAQInterface Hostname " << dispatcherHost
					         << ", and Subsystem " << dispatcherSubsystemID << __E__;
					dispatcherInfo.emplace_back(dispatcherUID, dispatcherHost, dispatcherSubsystemID);

					if(doWriteFHiCL)
					{
						ARTDAQTableBase::outputDataReceiverFHICL(
								dispatcher.second,
								ARTDAQTableBase::ARTDAQAppType::Dispatcher,
								maxFragmentSizeBytes);

						ARTDAQTableBase::flattenFHICL(
								ARTDAQTableBase::ARTDAQAppType::Dispatcher,
								dispatcher.second.getValue());
					}
				}
				else
				{
					__COUT__ << "Dispatcher "
					         << dispatcher.second.getNode("SupervisorUID").getValue()
					         << " is disabled." << __E__;
				}
			}
		}
	}

}  // end extractArtdaqInfo()

//==============================================================================
//	setAndActivateArtdaqSystem
//
//		static function to modify the active configuration based on
//	node object and subsystem object.
//
//	Subsystem map to destination subsystem name.
//	Node properties: {originalName,hostname,subsystemName}
//
void ARTDAQTableBase::setAndActivateArtdaqSystem(
		ConfigurationManagerRW* 			cfgMgr,
		const std::map<std::string /*type*/,
			std::map<std::string /*record*/,
				std::vector<std::string /*property*/>>>& 			nodeTypeToObjectMap,
		const std::map<std::string /*subsystemName*/,
						std::string /*destinationSubsystemName*/>& 	subsystemObjectMap)
{

	__COUT__ << "setAndActivateArtdaqSystem()" << __E__;

	const std::string& author = cfgMgr->getUsername();

	//Steps:
	//	0. Check for one and only artdaq Supervisor
	//	1. create/verify subsystems and destinations
	//	2. for each node
	//		create/verify records


	//------------------------
	//0. Check for one and only artdaq Supervisor


	GroupEditStruct configGroupEdit(ConfigurationManager::GroupType::CONFIGURATION_TYPE,cfgMgr);

	unsigned int artdaqSupervisorRow;

	const XDAQContextTable* contextTable = cfgMgr->__GET_CONFIG__(XDAQContextTable);

	const XDAQContextTable::XDAQContext* artdaqContext =
	    contextTable->getTheARTDAQSupervisorContext();

	if(!artdaqContext)
	{
		__COUT__ << "No artdaq Supervisor found! Creating..." << __E__;

		std::string artdaqSupervisorUID;
		unsigned int row;

		//create record in ARTDAQ Supervisor table
		//	connect to an App in a Context

		//now create artdaq Supervisor in configuration group
		{
			TableEditStruct& artdaqSupervisorTable = configGroupEdit.getTableEditStruct(
					ARTDAQTableBase::ARTDAQ_SUPERVISOR_TABLE, true /*markModified*/);


			// create artdaq Supervisor context record
			row = artdaqSupervisorTable.tableView_->addRow(
					author, true /*incrementUniqueData*/,
					"artdaqSupervisor");

			//get UID
			artdaqSupervisorUID = artdaqSupervisorTable.tableView_->getDataView()[row][artdaqSupervisorTable.tableView_->getColUID()];
			artdaqSupervisorRow = row;

			__COUTV__(artdaqSupervisorRow);
			__COUTV__(artdaqSupervisorUID);

			//set DAQInterfaceDebugLevel
			artdaqSupervisorTable.tableView_->setValueAsString(
					"1",
					row,
					artdaqSupervisorTable.tableView_->findCol(
							ARTDAQTableBase::colARTDAQSupervisor_.colDAQInterfaceDebugLevel_));
			//set DAQSetupScript
			artdaqSupervisorTable.tableView_->setValueAsString(
					"${MRB_BUILDDIR}/../setup_ots.sh",
					row,
					artdaqSupervisorTable.tableView_->findCol(
							ARTDAQTableBase::colARTDAQSupervisor_.colDAQSetupScript_));

			//create group link to board readers
			artdaqSupervisorTable.tableView_->setValueAsString(
					ARTDAQTableBase::ARTDAQ_READER_TABLE,
					row,
					artdaqSupervisorTable.tableView_->findCol(
							ARTDAQTableBase::colARTDAQSupervisor_.colLinkToBoardReaders_));
			artdaqSupervisorTable.tableView_->setUniqueColumnValue(
					row,
					artdaqSupervisorTable.tableView_->findCol(
							ARTDAQTableBase::colARTDAQSupervisor_.colLinkToBoardReadersGroupID_),
							artdaqSupervisorUID +
							ARTDAQTableBase::processTypes_.mapToGroupIDAppend_.at(
									ARTDAQTableBase::processTypes_.READER));
			//create group link to event builders
			artdaqSupervisorTable.tableView_->setValueAsString(
					ARTDAQTableBase::ARTDAQ_BUILDER_TABLE,
					row,
					artdaqSupervisorTable.tableView_->findCol(
							ARTDAQTableBase::colARTDAQSupervisor_.colLinkToEventBuilders_));
			artdaqSupervisorTable.tableView_->setUniqueColumnValue(
					row,
					artdaqSupervisorTable.tableView_->findCol(
							ARTDAQTableBase::colARTDAQSupervisor_.colLinkToEventBuildersGroupID_),
							artdaqSupervisorUID +
							ARTDAQTableBase::processTypes_.mapToGroupIDAppend_.at(
									ARTDAQTableBase::processTypes_.BUILDER));
			//create group link to data loggers
			artdaqSupervisorTable.tableView_->setValueAsString(
					ARTDAQTableBase::ARTDAQ_LOGGER_TABLE,
					row,
					artdaqSupervisorTable.tableView_->findCol(
							ARTDAQTableBase::colARTDAQSupervisor_.colLinkToDataLoggers_));
			artdaqSupervisorTable.tableView_->setUniqueColumnValue(
					row,
					artdaqSupervisorTable.tableView_->findCol(
							ARTDAQTableBase::colARTDAQSupervisor_.colLinkToDataLoggersGroupID_),
							artdaqSupervisorUID +
							ARTDAQTableBase::processTypes_.mapToGroupIDAppend_.at(
									ARTDAQTableBase::processTypes_.LOGGER));
			//create group link to dispatchers
			artdaqSupervisorTable.tableView_->setValueAsString(
					ARTDAQTableBase::ARTDAQ_DISPATCHER_TABLE,
					row,
					artdaqSupervisorTable.tableView_->findCol(
							ARTDAQTableBase::colARTDAQSupervisor_.colLinkToDispatchers_));
			artdaqSupervisorTable.tableView_->setUniqueColumnValue(
					row,
					artdaqSupervisorTable.tableView_->findCol(
							ARTDAQTableBase::colARTDAQSupervisor_.colLinkToDispatchersGroupID_),
							artdaqSupervisorUID +
							ARTDAQTableBase::processTypes_.mapToGroupIDAppend_.at(
									ARTDAQTableBase::processTypes_.DISPATCHER));

			{
				std::stringstream ss;
				artdaqSupervisorTable.tableView_->print(ss);
				__COUT__ << ss.str();
			}
		} //end create artdaq Supervisor in configuration group

		//now create artdaq Supervisor parents in context group
		{
			GroupEditStruct contextGroupEdit(ConfigurationManager::GroupType::CONTEXT_TYPE,cfgMgr);

			TableEditStruct& contextTable = contextGroupEdit.getTableEditStruct(
					ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME, true /*markModified*/);
			TableEditStruct& appTable = contextGroupEdit.getTableEditStruct(
					ConfigurationManager::XDAQ_APPLICATION_TABLE_NAME, true /*markModified*/);
			TableEditStruct& appPropertyTable = contextGroupEdit.getTableEditStruct(
					ConfigurationManager::XDAQ_APP_PROPERTY_TABLE_NAME, true /*markModified*/);

			//open try for decorating errors and for clean code scope
			try
			{
				std::string  contextUID;
				std::string  contextAppGroupID;

				{

					// create artdaq Supervisor context record
					row = contextTable.tableView_->addRow(
							author, true /*incrementUniqueData*/, "artdaqContext");
					// set context status true
					contextTable.tableView_->setValueAsString(
							"1", row, contextTable.tableView_->getColStatus());

					contextUID = contextTable.tableView_->getDataView()[row][contextTable.tableView_->getColUID()];

					__COUTV__(row);
					__COUTV__(contextUID);

					//set address/port
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

					//create group link to artdaq Supervisor app
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

				} //end create context entry

				//create artdaq Supervisor app
				std::string  appUID;
				std::string  appPropertiesGroupID;

				{
					unsigned int row;

					// create artdaq Supervisor context record
					row = appTable.tableView_->addRow(
							author, true /*incrementUniqueData*/,
							"artdaqSupervisor");
					// set app status true
					appTable.tableView_->setValueAsString(
							"1", row, appTable.tableView_->getColStatus());

					appUID = appTable.tableView_->getDataView()[row][appTable.tableView_->getColUID()];

					__COUTV__(row);
					__COUTV__(appUID);

					//set class
					appTable.tableView_->setValueAsString(
							"ots::ARTDAQSupervisor",
							row,
							appTable.tableView_->findCol(
									XDAQContextTable::colApplication_.colClass_));
					//set module
					appTable.tableView_->setValueAsString(
							"${OTSDAQ_LIB}/libARTDAQSupervisor.so",
							row,
							appTable.tableView_->findCol(
									XDAQContextTable::colApplication_.colModule_));
					//set groupid
					appTable.tableView_->setValueAsString(
							contextAppGroupID,
							row,
							appTable.tableView_->findCol(
									XDAQContextTable::colApplication_.colApplicationGroupID_));

					//create group link to artdaq Supervisor app properties
					appTable.tableView_->setValueAsString(
							ConfigurationManager::XDAQ_APP_PROPERTY_TABLE_NAME,
							row,
							appTable.tableView_->findCol(
									XDAQContextTable::colApplication_.colLinkToPropertyTable_));
					appPropertiesGroupID = appTable.tableView_->setUniqueColumnValue(
							row,
							appTable.tableView_->findCol(
									XDAQContextTable::colApplication_.colLinkToPropertyGroupID_),
									appUID + "Properties");

					__COUTV__(appPropertiesGroupID);

					//create group link to artdaq Supervisor app properties
					appTable.tableView_->setValueAsString(
							ARTDAQTableBase::ARTDAQ_SUPERVISOR_TABLE,
							row,
							appTable.tableView_->findCol(
									XDAQContextTable::colApplication_.colLinkToSupervisorTable_));
					appTable.tableView_->setValueAsString(
							artdaqSupervisorUID,
							row,
							appTable.tableView_->findCol(
									XDAQContextTable::colApplication_.colLinkToSupervisorUID_));

				} //end create app entry

				//create artdaq Supervisor properties
				{
					unsigned int row;

					const std::vector<std::string> propertyUIDs = {
							"Partition0",
							"ProductsDir",
							"FragmentSize",
							"BoardReaderTimeout",
							"EventBuilderTimeout",
							"DataLoggerTimeout",
							"DispatcherTimeout"
					};
					const std::vector<std::string> propertyNames = {
							"partition",					//"Partition0",
							"productsdir_for_bash_scripts",	//"ProductsDir",
							"max_fragment_size_bytes",		//"FragmentSize",
							"boardreader_timeout",			//"BoardReaderTimeout",
							"eventbuilder_timeout",			//"EventBuilderTimeout",
							"datalogger_timeout",			//"DataLoggerTimeout",
							"dispatcher_timeout"			//"DispatcherTimeout"
					};
					const std::vector<std::string> propertyValues = {
							"0", 				//"Partition0",
							"${OTS_PRODUCTS}",	//"ProductsDir",
							"1284180560",		//"FragmentSize",
							"600",				//"BoardReaderTimeout",
							"600",				//"EventBuilderTimeout",
							"600",				//"DataLoggerTimeout",
							"600"				//"DispatcherTimeout"
					};

					for(unsigned int i=0;i<propertyNames.size();++i)
					{
						// create artdaq Supervisor context record
						row = appPropertyTable.tableView_->addRow(
								author, true /*incrementUniqueData*/,
								appUID + propertyUIDs[i]);
						// set app status true
						appPropertyTable.tableView_->setValueAsString(
								"1", row, appPropertyTable.tableView_->getColStatus());

						//set type
						appPropertyTable.tableView_->setValueAsString(
								"ots::SupervisorProperty",
								row,
								appPropertyTable.tableView_->findCol(
										XDAQContextTable::colAppProperty_.colPropertyType_));
						//set name
						appPropertyTable.tableView_->setValueAsString(
								propertyNames[i],
								row,
								appPropertyTable.tableView_->findCol(
										XDAQContextTable::colAppProperty_.colPropertyName_));
						//set value
						appPropertyTable.tableView_->setValueAsString(
								propertyValues[i],
								row,
								appPropertyTable.tableView_->findCol(
										XDAQContextTable::colAppProperty_.colPropertyValue_));
						//set groupid
						appPropertyTable.tableView_->setValueAsString(
								appPropertiesGroupID,
								row,
								appPropertyTable.tableView_->findCol(
										XDAQContextTable::colAppProperty_.colPropertyGroupID_));
					} //end property create loop
				} //end create app property entries


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

				contextTable.tableView_->init();  // verify new table (throws runtime_errors)
				appTable.tableView_->init();  // verify new table (throws runtime_errors)
				appPropertyTable.tableView_->init();  // verify new table (throws runtime_errors)
			}
			catch(...)
			{
				__COUT__ << "Table errors while creating ARTDAQ Supervisor. Erasing all newly "
						"created table versions."
						<< __E__;
				throw;  // re-throw
			}           // end catch


			__COUT__ << "Edits complete for new artdaq Supervisor!"
					<< __E__;

			TableGroupKey newContextGroupKey;
			contextGroupEdit.saveChanges(
					contextGroupEdit.originalGroupName_,
					newContextGroupKey,
					nullptr /*foundEquivalentGroupKey*/,
					true /*activateNewGroup*/,
					true /*updateGroupAliases*/,
					true /*updateTableAliases*/);

		} //end create artdaq Supervisor in context group


	} //end artdaq Supervisor verification
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


	for(auto& subsystemPair:subsystemObjectMap)
	{
		__COUTV__(subsystemPair.first);


	} //end subsystem loop

	for(auto& nodeTypePair:nodeTypeToObjectMap)
	{
		__COUTV__(nodeTypePair.first);

		for(auto& nodePair:nodeTypePair.second)
		{
			__COUTV__(nodePair.first);

		}

	} //end node type loop


	//==================================
	//at this point artdaqSupervisor is verified and we have row
	__COUTV__(artdaqSupervisorRow);

	//Remaining steps:
	//Step	1. create/verify subsystems and destinations
	//Step	2. for each node, create/verify records

	//open try for decorating configuration group errors and for clean code scope
	try
	{
		unsigned int row;

		TableEditStruct& artdaqSupervisorTable = configGroupEdit.getTableEditStruct(
				ARTDAQTableBase::ARTDAQ_SUPERVISOR_TABLE, true /*markModified*/);

		//Step	1. create/verify subsystems and destinations
		TableEditStruct& artdaqSubsystemTable = configGroupEdit.getTableEditStruct(
						ARTDAQTableBase::ARTDAQ_SUBSYSTEM_TABLE, true /*markModified*/);

		//clear all records
		artdaqSubsystemTable.tableView_->deleteAllRows();

		for(auto& subsystemPair:subsystemObjectMap)
		{
			__COUTV__(subsystemPair.first);
			__COUTV__(subsystemPair.second);


			// create artdaq Subsystem record
			row = artdaqSubsystemTable.tableView_->addRow(
					author, true /*incrementUniqueData*/,
					subsystemPair.first);

			if(subsystemPair.second != "" &&
					subsystemPair.second != TableViewColumnInfo::DATATYPE_STRING_DEFAULT &&
					subsystemPair.second != ARTDAQTableBase::NULL_SUBSYSTEM_DESTINATION_LABEL)
			{
				//set subsystem link
				artdaqSubsystemTable.tableView_->setValueAsString(
						ARTDAQTableBase::ARTDAQ_SUBSYSTEM_TABLE,
						row,
						artdaqSubsystemTable.tableView_->findCol(
								ARTDAQTableBase::colARTDAQSubsystem_.colLinkToDestination_));
				artdaqSubsystemTable.tableView_->setValueAsString(
						subsystemPair.second,
						row,
						artdaqSubsystemTable.tableView_->findCol(
								ARTDAQTableBase::colARTDAQSubsystem_.colLinkToDestinationUID_));
			}

		} //end subsystem loop

		//Step	2. for each node, create/verify records
		for(auto& nodeTypePair:nodeTypeToObjectMap)
		{
			if(nodeTypePair.second.size() == 0) continue; //skip empty types (table might not be included in config, which is not an error)

			__COUTV__(nodeTypePair.first);

			//__COUTV__(StringMacros::mapToString(ARTDAQTableBase::processTypes_.mapToTable_));

			auto it = ARTDAQTableBase::processTypes_.mapToTable_.find(nodeTypePair.first);
			if(it == ARTDAQTableBase::processTypes_.mapToTable_.end())
			{
				__SS__ << "Invalid artdaq node type '" <<
						nodeTypePair.first << "' attempted!" << __E__;
				__SS_THROW__;
			}
			__COUTV__(it->second);

			TableEditStruct& typeTable = configGroupEdit.getTableEditStruct(
					it->second, true /*markModified*/);


			for(auto& nodePair:nodeTypePair.second)
			{
				__COUTV__(nodePair.first);

				//if original record is found, then commandeer that record
				//	else create a new record
				// Node properties: {originalName,hostname,subsystemName}
				for(unsigned int i=0; i<nodePair.second.size(); ++i)
				{
					__COUTV__(nodePair.second[i]);

					if(i == 0) //original UID
					{
						row = typeTable.tableView_->findRow(
								typeTable.tableView_->getColUID(),
								nodePair.second[i],
								0 /*offsetRow*/,
								true /*doNotThrow*/
								);
						__COUTV__(row);

						if(row == TableView::INVALID)
						{
							// create artdaq type instance record
							row = typeTable.tableView_->addRow(
									author, true /*incrementUniqueData*/,
									nodePair.first);
							__COUTV__(row);
						}
						else //set UID
						{
							typeTable.tableView_->setValueAsString(
									nodePair.first,
									row,
									typeTable.tableView_->getColUID());
						}

						__COUTV__(StringMacros::mapToString(ARTDAQTableBase::processTypes_.mapToLinkGroupIDColumn_));

						//set GroupID
						typeTable.tableView_->setValueAsString(
								artdaqSupervisorTable.tableView_->getDataView()
								[artdaqSupervisorRow]
								 [artdaqSupervisorTable.tableView_->findCol(
										 ARTDAQTableBase::processTypes_.mapToLinkGroupIDColumn_.at(
												 nodeTypePair.first))],
								row,
								typeTable.tableView_->findCol(
										ARTDAQTableBase::processTypes_.mapToGroupIDColumn_.at(
												nodeTypePair.first)));

					}
					else if(i == 1) //hostname
					{
						//set hostname
						typeTable.tableView_->setValueAsString(
								nodePair.second[i],
								row,
								typeTable.tableView_->findCol(
										ARTDAQTableBase::ARTDAQ_TYPE_TABLE_HOSTNAME));
					}
					else if(i == 2) //subsystemName
					{
						//set subsystemName
						if(nodePair.second[i] != "" &&
								nodePair.second[i] != TableViewColumnInfo::DATATYPE_STRING_DEFAULT)
						{

							//real subsystem?
							if(subsystemObjectMap.find(nodePair.second[i]) ==
									subsystemObjectMap.end())
							{
								__SS__ << "Illegal subsystem '" <<
										nodePair.second[i] << "' mismatch!" << __E__;
								__SS_THROW__;
							}

							typeTable.tableView_->setValueAsString(
									ARTDAQTableBase::ARTDAQ_SUBSYSTEM_TABLE,
									row,
									typeTable.tableView_->findCol(
											ARTDAQTableBase::ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK));
							typeTable.tableView_->setValueAsString(
									nodePair.second[i],
									row,
									typeTable.tableView_->findCol(
											ARTDAQTableBase::ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK_UID));
						}
						else //no subsystem (i.e. default subsystem)
						{
							typeTable.tableView_->setValueAsString(
									TableViewColumnInfo::DATATYPE_LINK_DEFAULT,
									row,
									typeTable.tableView_->findCol(
											ARTDAQTableBase::ARTDAQ_TYPE_TABLE_SUBSYSTEM_LINK));
						}
					}
				} //end node parameter loop


			} //end node record loop

			{
				std::stringstream ss;
				typeTable.tableView_->print(ss);
				__COUT__ << ss.str();
			}

			typeTable.tableView_->init();  // verify new table (throws runtime_errors)

		} //end node type loop


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
		artdaqSubsystemTable.tableView_->init();  // verify new table (throws runtime_errors)
	}
	catch(...)
	{
		__COUT__ << "Table errors while creating ARTDAQ nodes. Erasing all newly "
				"created table versions."
				<< __E__;
		throw;  // re-throw
	}           // end catch

	__COUT__ << "Edits complete for artdaq nodes and subsystems!"
			<< __E__;

	TableGroupKey newConfigurationGroupKey;
	configGroupEdit.saveChanges(
			configGroupEdit.originalGroupName_,
			newConfigurationGroupKey,
			nullptr /*foundEquivalentGroupKey*/,
			true /*activateNewGroup*/,
			true /*updateGroupAliases*/,
			true /*updateTableAliases*/);

} //end setAndActivateArtdaqSystem()

//==============================================================================
int ARTDAQTableBase::getSubsytemId(ConfigurationTree subsystemNode)
{
	//using row forces a unique ID from 0 to rows-1
	//	note: default no defined subsystem link to id=1; so add 2

	return subsystemNode.getNodeRow() + 2;
} //end getSubsytemId()





