#include "otsdaq/Macros/TablePluginMacros.h"
#include "otsdaq/TablePlugins/ARTDAQDispatcherTable.h"

using namespace ots;

// clang-format off

#define SLOWCONTROL_PV_FILE_PATH \
		std::string( \
			getenv("OTSDAQ_EPICS_DATA")? \
				(std::string(getenv("OTSDAQ_EPICS_DATA")) + "/" + __ENV__("MU2E_OWNER") + "_otsdaq_artdaqDispatcher-ai.dbg"): \
				(EPICS_CONFIG_PATH + "/_otsdaq_artdaqDispatcher-ai.dbg")  )

// clang-format on

//==============================================================================
ARTDAQDispatcherTable::ARTDAQDispatcherTable(void)
    : TableBase("ARTDAQDispatcherTable"), ARTDAQTableBase("ARTDAQDispatcherTable"), SlowControlsTableBase("ARTDAQDispatcherTable")

{
	//////////////////////////////////////////////////////////////////////
	// WARNING: the names used in C++ MUST match the Table INFO  		//
	//////////////////////////////////////////////////////////////////////
	__COUT__ << "ARTDAQDispatcherTable Constructed." << __E__;
}  // end constructor()

//==============================================================================
ARTDAQDispatcherTable::~ARTDAQDispatcherTable(void) {}

//==============================================================================
void ARTDAQDispatcherTable::init(ConfigurationManager* configManager)
{
	lastConfigManager_ = configManager;

	// use isFirstAppInContext to only run once per context, for example to avoid
	//	generating files on local disk multiple times.
	isFirstAppInContext_ = configManager->isOwnerFirstAppInContext();

	//__COUTV__(isFirstAppInContext);
	if(!isFirstAppInContext_)
		return;

	//if artdaq supervisor is disabled, skip fcl handling
	if(!ARTDAQTableBase::isARTDAQEnabled(configManager))
	{
		__COUT_INFO__ << "ARTDAQ Supervisor is disabled, so skipping fcl handling." << __E__;
		return;
	}

	// make directory just in case
	mkdir((ARTDAQTableBase::ARTDAQ_FCL_PATH).c_str(), 0755);

	//	__COUT__ << "*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*" << __E__;
	//	__COUT__ << configManager->__SELF_NODE__ << __E__;

	// handle fcl file generation, wherever the level of this table

	auto dispatchers = lastConfigManager_->__SELF_NODE__.getChildren(
	    /*default filterMap*/ std::map<std::string /*relative-path*/, std::string /*value*/>(),
	    /*default byPriority*/ false,
	    /*TRUE! onlyStatusTrue*/ true);

	for(auto& dispatcher : dispatchers)
	{
		ARTDAQTableBase::outputDataReceiverFHICL(dispatcher.second, ARTDAQTableBase::ARTDAQAppType::Dispatcher);
		ARTDAQTableBase::flattenFHICL(ARTDAQAppType::Dispatcher, dispatcher.second.getValue());
	}

}  // end init()

//==============================================================================
unsigned int ARTDAQDispatcherTable::slowControlsHandlerConfig(std::stringstream&                                                             out,
                                                              ConfigurationManager*                                                          configManager,
                                                              std::vector<std::pair<std::string /*channelName*/, std::vector<std::string>>>* channelList /*= 0*/
) const
{
	/////////////////////////
	// generate xdaq run parameter file

	std::string tabStr     = "";
	std::string commentStr = "";

	// loop through ARTDAQ Dispatcher records starting at ARTDAQSupervisorTable
	std::vector<std::pair<std::string, ConfigurationTree>> artdaqRecords = configManager->getNode("ARTDAQSupervisorTable").getChildren();

	unsigned int numberOfDispatcherMetricParameters = 0;

	for(auto& artdaqPair : artdaqRecords)  // start main artdaq record loop
	{
		if(artdaqPair.second.getNode(colARTDAQSupervisor_.colLinkToDispatchers_).isDisconnected())
			continue;

		std::vector<std::pair<std::string, ConfigurationTree>> dispatcherRecords =
		    artdaqPair.second.getNode(colARTDAQSupervisor_.colLinkToDispatchers_).getChildren();

		for(auto& dispatcherPair : dispatcherRecords)  // start main dispatcher record loop
		{
			if(!dispatcherPair.second.status())
				continue;

			try
			{
				if(dispatcherPair.second.getNode("daqLink").isDisconnected())
					continue;

				auto daqLink = dispatcherPair.second.getNode("daqLink");

				if(daqLink.getNode("daqMetricsLink").isDisconnected())
					continue;

				auto daqMetricsLinks = daqLink.getNode("daqMetricsLink").getChildren();
				for(auto& daqMetricsLink : daqMetricsLinks)  // start daqMetricsLinks record loop
				{
					if(!daqMetricsLink.second.status())
						continue;

					if(daqMetricsLink.second.getNode("metricParametersLink").isDisconnected())
						continue;

					// ConfigurationTree slowControlsLink = configManager->getNode("ARTDAQMetricAlarmThresholdsTable");
					ConfigurationTree slowControlsLink = dispatcherPair.second.getNode("MetricAlarmThresholdsLink");

					auto metricParametersLinks = daqMetricsLink.second.getNode("metricParametersLink").getChildren();
					for(auto& metricParametersLink : metricParametersLinks)  // start daq MetricParametersLinks record loop
					{
						if(!metricParametersLink.second.status())
							continue;

						std::string subsystem = metricParametersLink.second.getNode("metricParameterValue")
						                            .getValueWithDefault<std::string>(std::string("TDAQ_") + __ENV__("MU2E_OWNER"));
						if(subsystem.find("Mu2e:") != std::string::npos)
							subsystem = subsystem.replace(subsystem.find("Mu2e:"), 5, "");
						while(subsystem.find("\"") != std::string::npos)
							subsystem = subsystem.replace(subsystem.find("\""), 1, "");

						numberOfDispatcherMetricParameters =
						    slowControlsHandler(out, tabStr, commentStr, subsystem, dispatcherPair.first, slowControlsLink, channelList);

						__COUT__ << "Dispatcher '" << dispatcherPair.first << "' number of metrics for slow controls: " << numberOfDispatcherMetricParameters
						         << __E__;
					}
				}
			}
			catch(const std::runtime_error& e)
			{
				__COUT_ERR__ << "Ignoring Dispatcher error: " << e.what() << __E__;
			}
		}
	}

	return numberOfDispatcherMetricParameters;
}  // end slowControlsHandlerConfig()

//==============================================================================
// return out file path
std::string ARTDAQDispatcherTable::setFilePath() const { return SLOWCONTROL_PV_FILE_PATH; }

DEFINE_OTS_TABLE(ARTDAQDispatcherTable)
