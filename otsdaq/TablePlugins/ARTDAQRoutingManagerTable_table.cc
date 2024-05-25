#include "otsdaq/Macros/TablePluginMacros.h"
#include "otsdaq/TablePlugins/ARTDAQRoutingManagerTable.h"

using namespace ots;

// clang-format off

#define SLOWCONTROL_PV_FILE_PATH \
		std::string( \
			getenv("OTSDAQ_EPICS_DATA")? \
				(std::string(getenv("OTSDAQ_EPICS_DATA")) + "/" + __ENV__("MU2E_OWNER") + "_otsdaq_artdaqRoutingManager-ai.dbg"): \
				(EPICS_CONFIG_PATH + "/_otsdaq_artdaqRoutingManager-ai.dbg")  )

// clang-format on

//==============================================================================
ARTDAQRoutingManagerTable::ARTDAQRoutingManagerTable(void)
    : TableBase("ARTDAQRoutingManagerTable"), ARTDAQTableBase("ARTDAQRoutingManagerTable"), SlowControlsTableBase("ARTDAQRoutingManagerTable")
{
	//////////////////////////////////////////////////////////////////////
	// WARNING: the names used in C++ MUST match the Table INFO 		//
	//////////////////////////////////////////////////////////////////////
	__COUT__ << "ARTDAQRoutingManagerTable Constructed." << __E__;
}  // end constructor()

//==============================================================================
ARTDAQRoutingManagerTable::~ARTDAQRoutingManagerTable(void) {}

//==============================================================================
void ARTDAQRoutingManagerTable::init(ConfigurationManager* configManager)
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

	auto routingManagers = lastConfigManager_->__SELF_NODE__.getChildren(
	    /*default filterMap*/ std::map<std::string /*relative-path*/, std::string /*value*/>(),
	    /*default byPriority*/ false,
	    /*TRUE! onlyStatusTrue*/ true);

	for(auto& routingManager : routingManagers)
	{
		ARTDAQTableBase::outputRoutingManagerFHICL(routingManager.second);
		ARTDAQTableBase::flattenFHICL(ARTDAQAppType::RoutingManager, routingManager.second.getValue());
	}

}  // end init()

//==============================================================================
unsigned int ARTDAQRoutingManagerTable::slowControlsHandlerConfig(
    std::stringstream&                                                             out,
    ConfigurationManager*                                                          configManager,
    std::vector<std::pair<std::string /*channelName*/, std::vector<std::string>>>* channelList /*= 0*/
) const
{
	/////////////////////////
	// generate xdaq run parameter file

	std::string tabStr     = "";
	std::string commentStr = "";

	// loop through ARTDAQ RoutingManager records starting at ARTDAQSupervisorTable
	std::vector<std::pair<std::string, ConfigurationTree>> artdaqRecords = configManager->getNode("ARTDAQSupervisorTable").getChildren();

	unsigned int numberOfRoutingManagerMetricParameters = 0;

	for(auto& artdaqPair : artdaqRecords)  // start main artdaq record loop
	{
		if(artdaqPair.second.getNode(colARTDAQSupervisor_.colLinkToRoutingManagers_).isDisconnected())
			continue;

		std::vector<std::pair<std::string, ConfigurationTree>> routingManagerRecords =
		    artdaqPair.second.getNode(colARTDAQSupervisor_.colLinkToRoutingManagers_).getChildren();

		for(auto& routingManagerPair : routingManagerRecords)  // start main routingManager record loop
		{
			if(!routingManagerPair.second.status())
				continue;

			try
			{
				if(routingManagerPair.second.getNode("daqMetricsLink").isDisconnected())
					continue;

                ConfigurationTree slowControlsLink = routingManagerPair.second.getNode("MetricAlarmThresholdsLink");

                // check if epics is present, if it is maybe extract subsystem?
                bool epcisFound = false;
                std::string subsystem = std::string("TDAQ_") + __ENV__("MU2E_OWNER");
                auto daqMetricsLinks = routingManagerPair.second.getNode("daqMetricsLink").getChildren();
                for(auto& daqMetricsLink : daqMetricsLinks)
                {
                    if(!daqMetricsLink.second.status())
                    continue;
                    if(daqMetricsLink.second.getNode("metricPluginType").getValue<std::string>() == "epics") {
                        epcisFound = true;

                        if(!daqMetricsLink.second.getNode("metricParametersLink").isDisconnected()) {
                            auto metricParametersLinks = daqMetricsLink.second.getNode("metricParametersLink").getChildren();
                            for(auto& metricParametersLink : metricParametersLinks) {
                                if(!metricParametersLink.second.status()) {
							        continue;
                                }
                                if( metricParametersLink.second.getNode("metricParameterKey").getValue<std::string>() == "subsystem" ) {
                                    subsystem = metricParametersLink.second.getNode("metricParameterValue").getValue<std::string>();
                                    break;
                                }
                            }
                        }
                    }
                }
                if(!epcisFound) continue;
                if(subsystem.find("Mu2e:") != std::string::npos)
				    subsystem = subsystem.replace(subsystem.find("Mu2e:"), 5, "");
				while(subsystem.find("\"") != std::string::npos)
					subsystem = subsystem.replace(subsystem.find("\""), 1, "");

				auto numberOfRoutingManagerMetricParametersThis =
					slowControlsHandler(out, tabStr, commentStr, subsystem, routingManagerPair.first, slowControlsLink, channelList);
			    __COUT__ << "Dispatcher '" << routingManagerPair.first << "' number of metrics for slow controls: " << numberOfRoutingManagerMetricParametersThis
					<< __E__;
                numberOfRoutingManagerMetricParameters += numberOfRoutingManagerMetricParametersThis;
			}
			catch(const std::runtime_error& e)
			{
				__COUT_ERR__ << "Ignoring RoutingManager error: " << e.what() << __E__;
			}
		}
	}

	return numberOfRoutingManagerMetricParameters;
}  // end slowControlsHandlerConfig()

//==============================================================================
// return out file path
std::string ARTDAQRoutingManagerTable::setFilePath() const { return SLOWCONTROL_PV_FILE_PATH; }

DEFINE_OTS_TABLE(ARTDAQRoutingManagerTable)
