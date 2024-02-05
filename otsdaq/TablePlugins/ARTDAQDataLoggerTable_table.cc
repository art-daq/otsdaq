#include "otsdaq/Macros/TablePluginMacros.h"
#include "otsdaq/TablePlugins/ARTDAQDataLoggerTable.h"

using namespace ots;

// clang-format off

#define SLOWCONTROL_PV_FILE_PATH \
		std::string( \
			getenv("OTSDAQ_EPICS_DATA")? \
				(std::string(getenv("OTSDAQ_EPICS_DATA")) + "/" + __ENV__("MU2E_OWNER") + "_otsdaq_artdaqDataLogger-ai.dbg"): \
				(EPICS_CONFIG_PATH + "/_otsdaq_artdaqDataLogger-ai.dbg")  )

// clang-format on

//==============================================================================
ARTDAQDataLoggerTable::ARTDAQDataLoggerTable(void)
    : TableBase("ARTDAQDataLoggerTable"), ARTDAQTableBase("ARTDAQDataLoggerTable"), SlowControlsTableBase("ARTDAQDataLoggerTable")
{
	//////////////////////////////////////////////////////////////////////
	// WARNING: the names used in C++ MUST match the Table INFO  		//
	//////////////////////////////////////////////////////////////////////
	// December 2021 started seeing an issue where traceTID is found to be cleared to 0
	//	which crashes TRACE if __COUT__ is used in a Table plugin constructor
	//	This check and re-initialization seems to cover up the issue for now.
	//	Why it is cleared to 0 after the constructor sets it to -1 is still unknown.
	//		Note: it seems to only happen on the first alphabetially ARTDAQ Configure Table plugin.
	if(traceTID == 0)
	{
		std::cout << "ARTDAQBoardReaderTable Before traceTID=" << traceTID << __E__;
		char buf[40];
		traceInit(trace_name(TRACE_NAME, __TRACE_FILE__, buf, sizeof(buf)), 0);
		std::cout << "ARTDAQBoardReaderTable After traceTID=" << traceTID << __E__;
		__COUT__ << "ARTDAQDataLoggerTable TRACE reinit and Constructed." << __E__;
	}
	else
		__COUT__ << "ARTDAQDataLoggerTable Constructed." << __E__;
}  // end constructor()

//==============================================================================
ARTDAQDataLoggerTable::~ARTDAQDataLoggerTable(void) {}

//==============================================================================
void ARTDAQDataLoggerTable::init(ConfigurationManager* configManager)
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
	//	__COUT__ << lastConfigManager_->__SELF_NODE__ << __E__;

	// handle fcl file generation, wherever the level of this table

	auto dataloggers = lastConfigManager_->__SELF_NODE__.getChildren(
	    /*default filterMap*/ std::map<std::string /*relative-path*/, std::string /*value*/>(),
	    /*default byPriority*/ false,
	    /*TRUE! onlyStatusTrue*/ true);

	for(auto& datalogger : dataloggers)
	{
		ARTDAQTableBase::outputDataReceiverFHICL(datalogger.second, ARTDAQTableBase::ARTDAQAppType::DataLogger);

		ARTDAQTableBase::flattenFHICL(ARTDAQAppType::DataLogger, datalogger.second.getValue());
	}
} // end init()

//==============================================================================
unsigned int ARTDAQDataLoggerTable::slowControlsHandlerConfig(std::stringstream&                                                             out,
                                                              ConfigurationManager*                                                          configManager,
                                                              std::vector<std::pair<std::string /*channelName*/, std::vector<std::string>>>* channelList /*= 0*/
) const
{
	/////////////////////////
	// generate xdaq run parameter file

	std::string tabStr     = "";
	std::string commentStr = "";

	// loop through ARTDAQ DataLogger records starting at ARTDAQSupervisorTable
	std::vector<std::pair<std::string, ConfigurationTree>> artdaqRecords = configManager->getNode("ARTDAQSupervisorTable").getChildren();

	unsigned int numberOfDataLoggerMetricParameters = 0;

	for(auto& artdaqPair : artdaqRecords)  // start main artdaq record loop
	{
		if(artdaqPair.second.getNode(colARTDAQSupervisor_.colLinkToDataLoggers_).isDisconnected())
			continue;

		std::vector<std::pair<std::string, ConfigurationTree>> dataLoggerRecords =
		    artdaqPair.second.getNode(colARTDAQSupervisor_.colLinkToDataLoggers_).getChildren();

		for(auto& dataLoggerPair : dataLoggerRecords)  // start main dataLogger record loop
		{
			if(!dataLoggerPair.second.status())
				continue;

			try
			{
				if(dataLoggerPair.second.getNode("daqLink").isDisconnected())
					continue;

				auto daqLink = dataLoggerPair.second.getNode("daqLink");

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
					ConfigurationTree slowControlsLink = dataLoggerPair.second.getNode("MetricAlarmThresholdsLink");

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

						numberOfDataLoggerMetricParameters =
						    slowControlsHandler(out, tabStr, commentStr, subsystem, dataLoggerPair.first, slowControlsLink, channelList);

						__COUT__ << "DataLogger '" << dataLoggerPair.first << "' number of metrics for slow controls: " << numberOfDataLoggerMetricParameters
						         << __E__;
					}
				}
			}
			catch(const std::runtime_error& e)
			{
				__COUT_ERR__ << "Ignoring DataLogger error: " << e.what() << __E__;
			}
		}
	}

	return numberOfDataLoggerMetricParameters;
}  // end slowControlsHandlerConfig()

//==============================================================================
// return out file path
std::string ARTDAQDataLoggerTable::setFilePath() const { return SLOWCONTROL_PV_FILE_PATH; }

DEFINE_OTS_TABLE(ARTDAQDataLoggerTable)
