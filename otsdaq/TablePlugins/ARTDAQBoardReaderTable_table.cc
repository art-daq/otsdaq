#include "otsdaq/Macros/TablePluginMacros.h"
#include "otsdaq/TablePlugins/ARTDAQBoardReaderTable.h"

using namespace ots;

// clang-format off

#define SLOWCONTROL_PV_FILE_PATH \
		std::string( \
			getenv("OTSDAQ_EPICS_DATA")? \
				(std::string(getenv("OTSDAQ_EPICS_DATA")) + "/" + __ENV__("MU2E_OWNER") + "_otsdaq_artdaqBoardReader-ai.dbg"): \
				(EPICS_CONFIG_PATH + "/_otsdaq_artdaqBoardReader-ai.dbg")  )

// clang-format on

//==============================================================================
ARTDAQBoardReaderTable::ARTDAQBoardReaderTable(void)
    : TableBase("ARTDAQBoardReaderTable"), ARTDAQTableBase("ARTDAQBoardReaderTable"), SlowControlsTableBase("ARTDAQBoardReaderTable")
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
		__COUT__ << "ARTDAQBoardReaderTable TRACE reinit and Constructed." << __E__;
	}
	else
		__COUT__ << "ARTDAQBoardReaderTable Constructed." << __E__;
}  // end constructor()

//==============================================================================
ARTDAQBoardReaderTable::~ARTDAQBoardReaderTable(void) {}

//==============================================================================
void ARTDAQBoardReaderTable::init(ConfigurationManager* configManager)
{
	lastConfigManager_ = configManager;

	// use isFirstAppInContext to only run once per context, for example to avoid
	//	generating files on local disk multiple times.
	isFirstAppInContext_ = configManager->isOwnerFirstAppInContext();

	//__COUTV__(isFirstAppInContext_);
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

	__COUT__ << "*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*&*" << __E__;
	__COUT__ << configManager->__SELF_NODE__ << __E__;

	// handle fcl file generation, wherever the level of this table

	// auto readers = lastConfigManager_->getNode(ARTDAQTableBase::getTableName()).getChildren(
	auto readers = lastConfigManager_->__SELF_NODE__.getChildren(
	    /*default filterMap*/ std::map<std::string /*relative-path*/, std::string /*value*/>(),
	    /*default byPriority*/ false,
	    /*TRUE! onlyStatusTrue*/ true);

	for(auto& reader : readers)
	{
		ARTDAQTableBase::outputBoardReaderFHICL(reader.second);
		ARTDAQTableBase::flattenFHICL(ARTDAQAppType::BoardReader, reader.second.getValue());
	}
}  // end init()

//==============================================================================
unsigned int ARTDAQBoardReaderTable::slowControlsHandlerConfig(
    std::stringstream&                                                             out,
    ConfigurationManager*                                                          configManager,
    std::vector<std::pair<std::string /*channelName*/, std::vector<std::string>>>* channelList /*= 0*/
) const
{
	/////////////////////////
	// generate xdaq run parameter file

	std::string tabStr     = "";
	std::string commentStr = "";

	// loop through ARTDAQ BoardReader records starting at ARTDAQSupervisorTable
	std::vector<std::pair<std::string, ConfigurationTree>> artdaqRecords = configManager->getNode("ARTDAQSupervisorTable").getChildren();

	unsigned int numberOfBoardReaderMetricParameters = 0;

	for(auto& artdaqPair : artdaqRecords)  // start main artdaq record loop
	{
		if(artdaqPair.second.getNode(colARTDAQSupervisor_.colLinkToBoardReaders_).isDisconnected())
			continue;

		std::vector<std::pair<std::string, ConfigurationTree>> boardReaderRecords =
		    artdaqPair.second.getNode(colARTDAQSupervisor_.colLinkToBoardReaders_).getChildren();

		for(auto& boardReaderPair : boardReaderRecords)  // start main boardReader record loop
		{
			if(!boardReaderPair.second.status())
				continue;

			try
			{
				if(boardReaderPair.second.getNode("daqMetricsLink").isDisconnected())
					continue;

                ConfigurationTree slowControlsLink = boardReaderPair.second.getNode("MetricAlarmThresholdsLink");

                // check if epics is present, if it is maybe extract subsystem?
                bool epcisFound = false;
                std::string subsystem = std::string("TDAQ_") + __ENV__("MU2E_OWNER");
                auto daqMetricsLinks = boardReaderPair.second.getNode("daqMetricsLink").getChildren();
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
                                //if( metricParametersLink.second.getNode("metricParameterKey").getValue<std::string>() == "channel_name_prefix" ) {
                                if( metricParametersLink.second.getNode("metricParameterKey").getValue<std::string>() == "subsystem" ) {
                                    subsystem = metricParametersLink.second.getNode("metricParameterValue").getValue<std::string>();
                                    break;
                                }
                            }
                        }
                    }
                }
                __COUT__ << "DEBUG: " << boardReaderPair.first << " - epcisFound: : " << epcisFound << __E__;
                if(!epcisFound) continue;
                if(subsystem.find("Mu2e:") != std::string::npos)
				    subsystem = subsystem.replace(subsystem.find("Mu2e:"), 5, "");
				while(subsystem.find("\"") != std::string::npos)
					subsystem = subsystem.replace(subsystem.find("\""), 1, "");

				auto numberOfBoardReaderMetricParametersThis =
					slowControlsHandler(out, tabStr, commentStr, subsystem, boardReaderPair.first, slowControlsLink, channelList);
                __COUT__ << "BoardReader '" << boardReaderPair.first << "' number of metrics for slow controls: " << numberOfBoardReaderMetricParametersThis
					<< __E__;
                numberOfBoardReaderMetricParameters += numberOfBoardReaderMetricParametersThis;
			}
			catch(const std::runtime_error& e)
			{
				__COUT_ERR__ << "Ignoring BoardReader error: " << e.what() << __E__;
			}
		}
	}

	return numberOfBoardReaderMetricParameters;
}  // end slowControlsHandlerConfig()

//==============================================================================
// return out file path
std::string ARTDAQBoardReaderTable::setFilePath() const { return SLOWCONTROL_PV_FILE_PATH; }

DEFINE_OTS_TABLE(ARTDAQBoardReaderTable)
