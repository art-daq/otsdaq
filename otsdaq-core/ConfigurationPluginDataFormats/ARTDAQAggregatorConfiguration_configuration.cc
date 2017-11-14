#include "otsdaq-core/ConfigurationPluginDataFormats/ARTDAQAggregatorConfiguration.h"
#include "otsdaq-core/Macros/ConfigurationPluginMacros.h"
#include "otsdaq-core/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq-core/ConfigurationPluginDataFormats/XDAQContextConfiguration.h"

#include <iostream>
#include <fstream>      // std::fstream
#include <stdio.h>
#include <sys/stat.h> 	//for mkdir

using namespace ots;


#define ARTDAQ_FCL_PATH			std::string(getenv("USER_DATA")) + "/"+ "ARTDAQConfigurations/"
#define ARTDAQ_FILE_PREAMBLE	"aggregator"

//helpers
#define OUT						out << tabStr << commentStr
#define PUSHTAB					tabStr += "\t"
#define POPTAB					tabStr.resize(tabStr.size()-1)
#define PUSHCOMMENT				commentStr += "# "
#define POPCOMMENT				commentStr.resize(commentStr.size()-2)


//========================================================================================================================
ARTDAQAggregatorConfiguration::ARTDAQAggregatorConfiguration(void)
: ConfigurationBase("ARTDAQAggregatorConfiguration")
{
	//////////////////////////////////////////////////////////////////////
	//WARNING: the names used in C++ MUST match the Configuration INFO  //
	//////////////////////////////////////////////////////////////////////

}

//========================================================================================================================
ARTDAQAggregatorConfiguration::~ARTDAQAggregatorConfiguration(void)
{}

//========================================================================================================================
void ARTDAQAggregatorConfiguration::init(ConfigurationManager* configManager)
{
	//make directory just in case
	mkdir((ARTDAQ_FCL_PATH).c_str(), 0755);

	__COUT__ << "*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*" << std::endl;
	__COUT__ << configManager->__SELF_NODE__ << std::endl;

	const XDAQContextConfiguration *contextConfig = configManager->__GET_CONFIG__(XDAQContextConfiguration);
	std::vector<const XDAQContextConfiguration::XDAQContext *> aggContexts =
			contextConfig->getAggregatorContexts();

	//for each aggregator context
	//	output associated fcl config file
	for(auto &aggContext: aggContexts)
	{
		ConfigurationTree aggConfigNode = contextConfig->getSupervisorConfigNode(configManager,
				aggContext->contextUID_, aggContext->applications_[0].applicationUID_);

		__COUT__ << "Path for this aggregator config is " <<
				aggContext->contextUID_ << "/" <<
				aggContext->applications_[0].applicationUID_ << "/" <<
				aggConfigNode.getValueAsString() <<
				std::endl;

		outputFHICL(aggConfigNode,
				contextConfig->getARTDAQAppRank(aggContext->contextUID_),
				contextConfig);
	}
}

//========================================================================================================================
std::string ARTDAQAggregatorConfiguration::getFHICLFilename(const ConfigurationTree &aggregatorNode)
{
	__COUT__ << "ARTDAQ Aggregator UID: " << aggregatorNode.getValue() << std::endl;
	std::string filename = ARTDAQ_FCL_PATH + ARTDAQ_FILE_PREAMBLE + "-";
	std::string uid = aggregatorNode.getValue();
	for(unsigned int i=0;i<uid.size();++i)
		if((uid[i] >= 'a' && uid[i] <= 'z') ||
				(uid[i] >= 'A' && uid[i] <= 'Z') ||
				(uid[i] >= '0' && uid[i] <= '9')) //only allow alpha numeric in file name
			filename += uid[i];

	filename += ".fcl";

	__COUT__ << "fcl: " << filename << std::endl;

	return filename;
}

//========================================================================================================================
void ARTDAQAggregatorConfiguration::outputFHICL(const ConfigurationTree &aggregatorNode,
		unsigned int selfRank,
		const XDAQContextConfiguration *contextConfig)
{
	/*
		the file will look something like this:

		services: {
		  scheduler: {
			fileMode: NOMERGE
			errorOnFailureToPut: false
		  }
		  NetMonTransportServiceInterface: {
			service_provider: NetMonTransportService
		  }

		  #SimpleMemoryCheck: { }
		}

		daq: {
		  aggregator: {
			expected_events_per_bunch: 1
			print_event_store_stats: true
			event_queue_depth: 20
			event_queue_wait_time: 5
			onmon_event_prescale: 1
			xmlrpc_client_list: ";http://ironwork.fnal.gov:5205/RPC2,3;http://ironwork.fnal.gov:5206/RPC2,3;http://ironwork.fnal.gov:5235/RPC2,4;http://ironwork.fnal.gov:5236/RPC2,4;http://ironwork.fnal.gov:5265/RPC2,5;http://ironwork.fnal.gov:5266/RPC2,5"
			file_size_MB: 0.0
			file_duration: 0
			file_event_count: 0
			is_data_logger: true

			sources: {
				s2: { transferPluginType: MPI source_rank: 2 max_fragment_size_words: 2097152}
		s3: { transferPluginType: MPI source_rank: 3 max_fragment_size_words: 2097152}

			}
		  }

		  metrics: {
			aggFile: {
			  metricPluginType: "file"
			  level: 3
			  fileName: "/tmp/aggregator/agg_%UID%_metrics.log"
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

		  transfer_to_dispatcher: {
			transferPluginType: Shmem
			source_rank: 4
			destination_rank: 5
			max_fragment_size_words: 2097152
		  }

		}

		source: {
		  module_type: NetMonInput
		}
		outputs: {
		  normalOutput: {
			module_type: RootOutput
			fileName: "/tmp/artdaqdemo_r%06r_sr%02s_%to.root"
		  }

		  #normalOutputMod2: {
		  #  module_type: RootOutput
		  #  fileName: "/tmp/artdaqdemo_r%06r_sr%02s_%to_mod2.root"
		  #  SelectEvents: { SelectEvents: [ pmod2 ] }
		  #}

		  #normalOutputMod3: {
		  #  module_type: RootOutput
		  #  fileName: "/tmp/artdaqdemo_r%06r_sr%02s_%to_mod3.root"
		  #  SelectEvents: { SelectEvents: [ pmod3 ] }
		  #}

		}
		physics: {
		  analyzers: {


		   checkintegrity: {
			 module_type: CheckIntegrity
			 raw_data_label: daq
			 frag_type: TOY1
		   }

		  }

		  producers: {

			 BuildInfo:
			 {
			   module_type: ArtdaqDemoBuildInfo
			   instance_name: ArtdaqDemo
			 }
		   }

		  filters: {

			prescaleMod2: {
			   module_type: NthEvent
			   nth: 2
			}

			prescaleMod3: {
			   module_type: NthEvent
			   nth: 3
			}
		  }

		  p2: [ BuildInfo ]
		  pmod2: [ prescaleMod2 ]
		  pmod3: [ prescaleMod3 ]

		  #a1: [ app, wf]

		  my_output_modules: [ normalOutput ]
		  #my_output_modules: [ normalOutputMod2, normalOutputMod3 ]
		}
		process_name: DAQAG

	 */

	std::string filename = getFHICLFilename(aggregatorNode);

	__COUT__ << "selfRank = " << selfRank << std::endl;


	/////////////////////////
	//generate xdaq run parameter file
	std::fstream out;

	std::string tabStr = "";
	std::string commentStr = "";

	out.open(filename, std::fstream::out | std::fstream::trunc);
	if(out.fail())
	{
		__SS__ << "Failed to open ARTDAQ Builder fcl file: " << filename << std::endl;
		throw std::runtime_error(ss.str());
	}

	//no primary link to configuration tree for aggregator node!
	if(aggregatorNode.isDisconnected())
	{
		//create empty fcl
		OUT << "{}\n\n";
		out.close();
		return;
	}

	//--------------------------------------
	//handle services
	auto services = aggregatorNode.getNode("servicesLink");
	if(!services.isDisconnected())
	{
		OUT << "services: {\n";

		//scheduler
		PUSHTAB;
		OUT << "scheduler: {\n";

		PUSHTAB;
		OUT << "fileMode: " << services.getNode("schedulerFileMode").getValue() <<
				"\n";
		OUT << "errorOnFailureToPut: " <<
				(services.getNode("schedulerErrorOnFailtureToPut").getValue<bool>()?"true":"false") <<
				"\n";
		POPTAB;

		OUT << "}\n\n";


		//NetMonTransportServiceInterface
		OUT << "NetMonTransportServiceInterface: {\n";

		PUSHTAB;
		OUT << "service_provider: " <<
				//services.getNode("NetMonTrasportServiceInterfaceServiceProvider").getEscapedValue()
				services.getNode("NetMonTrasportServiceInterfaceServiceProvider").getValue()
				<< "\n";

		POPTAB;
		OUT << "}\n\n";	//end NetMonTransportServiceInterface

		POPTAB;
		OUT << "}\n\n"; //end services

	}

	//--------------------------------------
	//handle daq
	auto daq = aggregatorNode.getNode("daqLink");
	if(!daq.isDisconnected())
	{
		OUT << "daq: {\n";

		//aggregator
		PUSHTAB;
		OUT << "aggregator: {\n";

		PUSHTAB;
		auto parametersLink = daq.getNode("daqAggregatorParametersLink");
		if(!parametersLink.isDisconnected())
		{

			auto parameters = parametersLink.getChildren();
			for(auto &parameter:parameters)
			{
				if(!parameter.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
					PUSHCOMMENT;

				auto comment = parameter.second.getNode("CommentDescription");
				OUT << parameter.second.getNode("daqParameterKey").getValue() <<
						": " <<
						parameter.second.getNode("daqParameterValue").getValue()
						<<
						(comment.isDefaultValue()?"":("\t # " + comment.getValue())) <<
						"\n";

				if(!parameter.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
					POPCOMMENT;
			}
		}
		OUT << "\n";	//end daq aggregator parameters

		OUT << "sources: {\n";

		PUSHTAB;
		auto sourcesGroup = daq.getNode("daqAggregatorSourcesLink");
		if(!sourcesGroup.isDisconnected())
		{
			try
			{
				auto sources = sourcesGroup.getChildren();
				for(auto &source:sources)
				{
					unsigned int sourceRank =
							contextConfig->getARTDAQAppRank(
									source.second.getNode("sourceARTDAQContextLink").getValue());

					OUT << source.second.getNode("sourceKey").getValue() <<
							": {" <<
							" transferPluginType: " <<
							source.second.getNode("transferPluginType").getValue() <<
							" source_rank: " <<
							sourceRank <<
							" max_fragment_size_words: " <<
							source.second.getNode("ARTDAQGlobalConfigurationLink/maxFragmentSizeWords").getValue<unsigned int>() <<
							"}\n";
				}
			}
			catch(const std::runtime_error& e)
			{
				__SS__ << "Are the DAQ sources valid? Error occurred looking for Aggregator DAQ sources: " << e.what() << std::endl;
				__COUT_ERR__ << ss.str() << std::endl;
				throw std::runtime_error(ss.str());
			}
		}
		POPTAB;
		OUT << "}\n\n";	//end sources

		POPTAB;
		OUT << "}\n\n";	//end aggregator


		OUT << "metrics: {\n";

		PUSHTAB;
		auto metricsGroup = daq.getNode("daqMetricsLink");
		if(!metricsGroup.isDisconnected())
		{
			auto metrics = metricsGroup.getChildren();

			for(auto &metric:metrics)
			{
				if(!metric.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
					PUSHCOMMENT;

				OUT << metric.second.getNode("metricKey").getValue() <<
						": {\n";
				PUSHTAB;

				OUT << "metricPluginType: " <<
						metric.second.getNode("metricPluginType").getValue()
						<< "\n";
				OUT << "level: " <<
						metric.second.getNode("metricLevel").getValue()
						<< "\n";

				auto metricParametersGroup = metric.second.getNode("metricParametersLink");
				if(!metricParametersGroup.isDisconnected())
				{
					auto metricParameters = metricParametersGroup.getChildren();
					for(auto &metricParameter:metricParameters)
					{
						if(!metricParameter.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
							PUSHCOMMENT;

						auto comment = metricParameter.second.getNode("CommentDescription");
						OUT << metricParameter.second.getNode("metricParameterKey").getValue() <<
								": " <<
								metricParameter.second.getNode("metricParameterValue").getValue()
								<<
								(comment.isDefaultValue()?"":("\t # " + comment.getValue())) <<
								"\n";

						if(!metricParameter.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
							POPCOMMENT;

					}
				}
				POPTAB;
				OUT << "}\n\n";	//end metric

				if(!metric.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
					POPCOMMENT;
			}
		}
		POPTAB;
		OUT << "}\n\n";	//end metrics

		//other destinations
		auto destinationsGroup = daq.getNode("daqAggregatorDestinationsLink");
		if(!destinationsGroup.isDisconnected())
		{
			try
			{
				auto destinations = destinationsGroup.getChildren();
				for(auto &destination:destinations)
				{
					unsigned int destinationRank = contextConfig->getARTDAQAppRank(
							destination.second.getNode("destinationARTDAQContextLink").getValueAsString());

					OUT << destination.second.getNode("destinationKey").getValue() <<
							": {" <<
							" transferPluginType: " <<
							destination.second.getNode("transferPluginType").getValue() <<
							" source_rank: " <<
							selfRank <<
							" destination_rank: " <<
							destinationRank <<
							" max_fragment_size_words: " <<
							destination.second.getNode("ARTDAQGlobalConfigurationLink/maxFragmentSizeWords").getValue<unsigned int>() <<
							"}\n";
				}
			}
			catch(const std::runtime_error& e)
			{
				__SS__ << "Are the DAQ destinations valid? Error occurred looking for Aggregator DAQ destinations: " << e.what() << std::endl;
				__COUT_ERR__ << ss.str() << std::endl;
				throw std::runtime_error(ss.str());
			}
		}

		POPTAB;
		OUT << "}\n\n"; //end daq
	}


	//--------------------------------------
	//handle source
	auto source = aggregatorNode.getNode("sourceLink");
	if(!source.isDisconnected())
	{
		OUT << "source: {\n";

		PUSHTAB;
		OUT << "module_type: " << source.getNode("sourceModuleType").getValue() <<
				"\n";
		POPTAB;
		OUT << "}\n\n";	//end source
	}

	//--------------------------------------
	//handle outputs
	auto outputs = aggregatorNode.getNode("outputsLink");
	if(!outputs.isDisconnected())
	{
		OUT << "outputs: {\n";

		PUSHTAB;

		auto outputPlugins = outputs.getChildren();
		for(auto &outputPlugin:outputPlugins)
		{
			if(!outputPlugin.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
				PUSHCOMMENT;

			OUT << outputPlugin.second.getNode("outputKey").getValue() <<
					": {\n";
			PUSHTAB;
			OUT << "module_type: " <<
					outputPlugin.second.getNode("outputModuleType").getValue() <<
					"\n";
			auto pluginParameterLink = outputPlugin.second.getNode("outputModuleParameterLink");
			if(!pluginParameterLink.isDisconnected())
			{
				auto pluginParameters = pluginParameterLink.getChildren();
				for(auto &pluginParameter:pluginParameters)
				{
					if(!pluginParameter.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
						PUSHCOMMENT;

					auto comment = pluginParameter.second.getNode("CommentDescription");
					OUT << pluginParameter.second.getNode("outputParameterKey").getValue() <<
							": " <<
							pluginParameter.second.getNode("outputParameterValue").getValue()
							<<
							(comment.isDefaultValue()?"":("\t # " + comment.getValue())) <<
							"\n";

					if(!pluginParameter.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
						POPCOMMENT;
				}
			}
			POPTAB;
			OUT << "}\n\n";	//end output module

			if(!outputPlugin.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
				POPCOMMENT;
		}

		POPTAB;
		OUT << "}\n\n";	//end outputs
	}


	//--------------------------------------
	//handle physics
	auto physics = aggregatorNode.getNode("physicsLink");
	if(!physics.isDisconnected())
	{
		///////////////////////
		OUT << "physics: {\n";

		PUSHTAB;

		auto analyzers = physics.getNode("analyzersLink");
		if(!analyzers.isDisconnected())
		{
			///////////////////////
			OUT << "analyzers: {\n";

			PUSHTAB;
			auto modules = analyzers.getChildren();
			for(auto &module:modules)
			{
				if(!module.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
					PUSHCOMMENT;

				OUT << module.second.getNode("analyzerKey").getValue() <<
						": {\n";
				PUSHTAB;
				OUT << "module_type: " <<
						module.second.getNode("analyzerModuleType").getValue() <<
						"\n";
				auto moduleParameterLink = module.second.getNode("analyzerModuleParameterLink");
				if(!moduleParameterLink.isDisconnected())
				{
					auto moduleParameters = moduleParameterLink.getChildren();
					for(auto &moduleParameter:moduleParameters)
					{
						if(!moduleParameter.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
							PUSHCOMMENT;

						auto comment = moduleParameter.second.getNode("CommentDescription");
						OUT << moduleParameter.second.getNode("analyzerParameterKey").getValue() <<
								": " <<
								moduleParameter.second.getNode("analyzerParameterValue").getValue()
								<<
								(comment.isDefaultValue()?"":("\t # " + comment.getValue())) <<
								"\n";

						if(!moduleParameter.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
							POPCOMMENT;
					}
				}
				POPTAB;
				OUT << "}\n\n";	//end analyzer module

				if(!module.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
					POPCOMMENT;
			}
			POPTAB;
			OUT << "}\n\n";	//end analyzer
		}

		auto producers = physics.getNode("producersLink");
		if(!producers.isDisconnected())
		{
			///////////////////////
			OUT << "producers: {\n";

			PUSHTAB;
			auto modules = producers.getChildren();
			for(auto &module:modules)
			{
				if(!module.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
					PUSHCOMMENT;

				OUT << module.second.getNode("producerKey").getValue() <<
						": {\n";
				PUSHTAB;
				OUT << "module_type: " <<
						module.second.getNode("producerModuleType").getValue() <<
						"\n";
				auto moduleParameterLink = module.second.getNode("producerModuleParameterLink");
				if(!moduleParameterLink.isDisconnected())
				{
					auto moduleParameters = moduleParameterLink.getChildren();
					for(auto &moduleParameter:moduleParameters)
					{
						if(!moduleParameter.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
							PUSHCOMMENT;

						auto comment = moduleParameter.second.getNode("CommentDescription");
						OUT << moduleParameter.second.getNode("producerParameterKey").getValue() <<
								":" <<
								moduleParameter.second.getNode("producerParameterValue").getValue()
								<<
								(comment.isDefaultValue()?"":("\t # " + comment.getValue())) <<
								"\n";

						if(!moduleParameter.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
							POPCOMMENT;
					}
				}
				POPTAB;
				OUT << "}\n\n";	//end producer module

				if(!module.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
					POPCOMMENT;
			}
			POPTAB;
			OUT << "}\n\n";	//end producer
		}


		auto filters = physics.getNode("filtersLink");
		if(!filters.isDisconnected())
		{
			///////////////////////
			OUT << "filters: {\n";

			PUSHTAB;
			auto modules = filters.getChildren();
			for(auto &module:modules)
			{
				if(!module.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
					PUSHCOMMENT;

				OUT << module.second.getNode("filterKey").getValue() <<
						": {\n";
				PUSHTAB;
				OUT << "module_type: " <<
						module.second.getNode("filterModuleType").getValue() <<
						"\n";
				auto moduleParameterLink = module.second.getNode("filterModuleParameterLink");
				if(!moduleParameterLink.isDisconnected())
				{
					auto moduleParameters = moduleParameterLink.getChildren();
					for(auto &moduleParameter:moduleParameters)
					{
						if(!moduleParameter.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
							PUSHCOMMENT;

						auto comment = moduleParameter.second.getNode("CommentDescription");
						OUT << moduleParameter.second.getNode("filterParameterKey").getValue()
							<< ": " <<
							moduleParameter.second.getNode("filterParameterValue").getValue()
							<<
							(comment.isDefaultValue()?"":("\t # " + comment.getValue())) <<
							"\n";

						if(!moduleParameter.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
							POPCOMMENT;
					}
				}
				POPTAB;
				OUT << "}\n\n";	//end filter module

				if(!module.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
					POPCOMMENT;
			}
			POPTAB;
			OUT << "}\n\n";	//end filter
		}


		auto otherParameterLink = physics.getNode("physicsOtherParametersLink");
		if(!otherParameterLink.isDisconnected())
		{
			///////////////////////
			auto physicsParameters = otherParameterLink.getChildren();
			for(auto &parameter:physicsParameters)
			{
				if(!parameter.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
					PUSHCOMMENT;

				auto comment = parameter.second.getNode("CommentDescription");
				OUT << parameter.second.getNode("physicsParameterKey").getValue() <<
						": " <<
						parameter.second.getNode("physicsParameterValue").getValue()
						<<
						(comment.isDefaultValue()?"":("\t # " + comment.getValue())) <<
						"\n";

				if(!parameter.second.getNode(ViewColumnInfo::COL_NAME_STATUS).getValue<bool>())
					POPCOMMENT;
			}
		}
		POPTAB;
		OUT << "}\n\n";	//end physics
	}


	//--------------------------------------
	//handle process_name
	OUT << "process_name: " <<
			aggregatorNode.getNode("ARTDAQGlobalConfigurationForProcessNameLink/processNameForAggregators")
			<< "\n";




	out.close();
}

DEFINE_OTS_CONFIGURATION(ARTDAQAggregatorConfiguration)
