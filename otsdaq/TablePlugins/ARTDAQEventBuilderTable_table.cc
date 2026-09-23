#include "otsdaq/Macros/TablePluginMacros.h"
#include "otsdaq/TablePlugins/ARTDAQEventBuilderTable.h"

#include <fstream>  // for std::ofstream

using namespace ots;

// clang-format off

#define SLOWCONTROL_PV_FILE_PATH \
		std::string( \
			getenv("OTSDAQ_EPICS_DATA")? \
				(std::string(getenv("OTSDAQ_EPICS_DATA")) + "/" + __ENV__("MU2E_OWNER") + "_otsdaq_artdaqEventBuilder-ai.dbg"): \
				(EPICS_CONFIG_PATH + "/_otsdaq_artdaqEventBuilder-ai.dbg")  )

// clang-format on

//==============================================================================
ARTDAQEventBuilderTable::ARTDAQEventBuilderTable(void)
    : TableBase("ARTDAQEventBuilderTable")
    , ARTDAQTableBase("ARTDAQEventBuilderTable")
    , SlowControlsTableBase("ARTDAQEventBuilderTable")
{
	//////////////////////////////////////////////////////////////////////
	// WARNING: the names used in C++ MUST match the Table INFO  		//
	//////////////////////////////////////////////////////////////////////
	__COUT__ << "ARTDAQEventBuilderTable Constructed." << __E__;
}  // end constructor()

//==============================================================================
ARTDAQEventBuilderTable::~ARTDAQEventBuilderTable(void) {}

//==============================================================================
void ARTDAQEventBuilderTable::init(ConfigurationManager* configManager)
{
	lastConfigManager_ = configManager;
	if(!ARTDAQTableBase::doGenFiles(configManager))
	{
		__COUTS__(3) << "ARTDAQTableBase indicates file generation can be skipped."
		             << __E__;
		return;
	}

	__COUTS__(3) << "*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*" << __E__;
	__COUTS__(3) << configManager->__SELF_NODE__ << __E__;

	genFlatFHiCL();
}  // end init()

//==============================================================================
void ARTDAQEventBuilderTable::genFlatFHiCL(void)
{
	// handle fcl file generation, wherever the level of this table

	auto builders = lastConfigManager_->getNode(ARTDAQTableBase::getTableName())
	                    .getChildren(
	                        /*default filterMap*/ std::map<std::string /*relative-path*/,
	                                                       std::string /*value*/>(),
	                        /*default byPriority*/ false,
	                        /*TRUE! onlyStatusTrue*/ true);

	// Builder fcl generation happens in two stages: outputDataReceiverFHICL
	// (config-tree access, sequential) inside the loop below, then the expensive
	// flattens run together in parallel after the loop, each capturing its
	// flattened fcl into fclMap_ (std::map nodes are stable, so the value
	// pointers taken below stay valid while the map grows)
	std::vector<std::pair<std::string, std::string*>> buildersToFlatten;
	for(auto& builder : builders)
	{
		const std::string& builderUID = builder.first;
		__COUTV__(builderUID);

		outputDataReceiverFHICL(builder.second,
		                        ARTDAQAppType::EventBuilder,
		                        DEFAULT_MAX_FRAGMENT_SIZE,
		                        DEFAULT_ROUTING_TIMEOUT_MS,
		                        DEFAULT_ROUTING_RETRY_COUNT);
		buildersToFlatten.emplace_back(
		    builderUID, &(fclMap_[ARTDAQAppType::EventBuilder][builderUID]));
	}  //end builder fcl handling loop

	if(buildersToFlatten.size())
		ARTDAQTableBase::flattenFHICLInParallel(ARTDAQAppType::EventBuilder,
		                                        buildersToFlatten);
	__COUTS__(3) << "*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*" << __E__;
}  // end init()

//==============================================================================
unsigned int ARTDAQEventBuilderTable::slowControlsHandlerConfig(
    std::stringstream&    out,
    ConfigurationManager* configManager,
    std::vector<std::pair<std::string /*channelName*/, std::vector<std::string>>>*
        channelList /*= 0*/
) const
{
	/////////////////////////
	// generate xdaq run parameter file

	std::string tabStr     = "";
	std::string commentStr = "";

	// loop through ARTDAQ EventBuilder records starting at ARTDAQSupervisorTable
	std::vector<std::pair<std::string, ConfigurationTree>> artdaqRecords =
	    configManager->getNode("ARTDAQSupervisorTable").getChildren();

	unsigned int numberOfEventBuiderMetricParameters = 0;

	for(auto& artdaqPair : artdaqRecords)  // start main artdaq record loop
	{
		if(artdaqPair.second.getNode(colARTDAQSupervisor_.colLinkToEventBuilders_)
		       .isDisconnected())
			continue;

		std::vector<std::pair<std::string, ConfigurationTree>> eventBuilderRecords =
		    artdaqPair.second.getNode(colARTDAQSupervisor_.colLinkToEventBuilders_)
		        .getChildren();

		for(auto& eventBuilderPair :
		    eventBuilderRecords)  // start main eventBuilder record loop
		{
			if(!eventBuilderPair.second.status())
				continue;

			try
			{
				if(eventBuilderPair.second.getNode("daqLink").isDisconnected())
					continue;

				auto daqLink = eventBuilderPair.second.getNode("daqLink");

				if(daqLink.getNode("daqMetricsLink").isDisconnected())
					continue;

				auto daqMetricsLinks = daqLink.getNode("daqMetricsLink").getChildren();
				for(auto& daqMetricsLink :
				    daqMetricsLinks)  // start daqMetricsLinks record loop
				{
					if(!daqMetricsLink.second.status())
						continue;

					if(daqMetricsLink.second.getNode("metricParametersLink")
					       .isDisconnected())
						continue;

					// ConfigurationTree slowControlsLink = configManager->getNode("ARTDAQMetricAlarmThresholdsTable");
					ConfigurationTree slowControlsLink =
					    eventBuilderPair.second.getNode("MetricAlarmThresholdsLink");

					auto metricParametersLinks =
					    daqMetricsLink.second.getNode("metricParametersLink")
					        .getChildren();
					for(auto& metricParametersLink :
					    metricParametersLinks)  // start daq MetricParametersLinks record loop
					{
						if(!metricParametersLink.second.status())
							continue;

						std::string subsystem =
						    metricParametersLink.second.getNode("metricParameterValue")
						        .getValueWithDefault<std::string>(std::string("TDAQ_") +
						                                          __ENV__("MU2E_OWNER"));
						if(subsystem.find("Mu2e:") != std::string::npos)
							subsystem = subsystem.replace(subsystem.find("Mu2e:"), 5, "");
						while(subsystem.find("\"") != std::string::npos)
							subsystem = subsystem.replace(subsystem.find("\""), 1, "");

						numberOfEventBuiderMetricParameters =
						    slowControlsHandler(out,
						                        tabStr,
						                        commentStr,
						                        subsystem,
						                        eventBuilderPair.first,
						                        slowControlsLink,
						                        channelList);

						__COUT__ << "EventBuilder '" << eventBuilderPair.first
						         << "' number of metrics for slow controls: "
						         << numberOfEventBuiderMetricParameters << __E__;
					}
				}
			}
			catch(const std::runtime_error& e)
			{
				__COUT_ERR__ << "Ignoring EventBuilder error: " << e.what() << __E__;
			}
		}
	}

	return numberOfEventBuiderMetricParameters;
}  // end slowControlsHandlerConfig()

//==============================================================================
/// return out file path
std::string ARTDAQEventBuilderTable::setFilePath() const
{
	return SLOWCONTROL_PV_FILE_PATH;
}

DEFINE_OTS_TABLE(ARTDAQEventBuilderTable)
