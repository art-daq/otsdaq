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

	// use isFirstAppInContext to only run once per context, for example to avoid
	//	generating files on local disk multiple times.
	isFirstAppInContext_ = configManager->isOwnerFirstAppInContext();

	__COUTVS__(4, isFirstAppInContext_);
	if(!isFirstAppInContext_)
		return;

	//if artdaq supervisor is disabled, skip fcl handling
	if(!ARTDAQTableBase::isARTDAQEnabled(configManager))
	{
		__COUT_INFO__ << "ARTDAQ Supervisor is disabled, so skipping fcl handling."
		              << __E__;
		return;
	}

	// make directory just in case
	mkdir((ARTDAQTableBase::ARTDAQ_FCL_PATH).c_str(), 0755);

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

	std::string lastBuilderFcl[2],
	    flattenedLastFclParts
	        [2];  //same handling as otsdaq/otsdaq/TablePlugins/ARTDAQTableBase/ARTDAQTableBase.cc:1986
	for(auto& builder : builders)
	{
		const std::string& builderUID = builder.first;
		__COUTV__(builderUID);

		std::string returnFcl, processName;
		bool        needToFlatten = true;
		bool        captureAsLastFcl =
		    builders.size() &&  //init to true if multiple builders left to handle
		    (&builder != &builders.back());
		outputDataReceiverFHICL(builder.second,
		                        ARTDAQAppType::EventBuilder,
		                        DEFAULT_MAX_FRAGMENT_SIZE,
		                        DEFAULT_ROUTING_TIMEOUT_MS,
		                        DEFAULT_ROUTING_RETRY_COUNT,
		                        captureAsLastFcl ? &returnFcl : nullptr);

		//Speed-up Philosophy:
		// flattenFHICL is expensive, so try to identify multinodes with fcl that only differ by process_name,
		//	i.e., ignore starting comments and process name, then compare fcl.
		//	Note: not much gain for any other node types but Event Builders, which tend to only differ by process_name in their fcl

		auto cmi =
		    returnFcl.find("#	otsdaq-ARTDAQ builder UID:");  //find starting comments
		if(cmi != std::string::npos)
			cmi = returnFcl.find('\n', cmi);
		if(cmi != std::string::npos)
		{
			size_t pnj = std::string::npos;
			auto   pni = returnFcl.find("\tprocess_name: ", cmi);  //find process name
			if(pni != std::string::npos)
			{
				pni += std::string("\tprocess_name: ").size();  //move past field name
				pnj = returnFcl.find('\n', pni);
			}
			if(pnj != std::string::npos)
			{
				processName = returnFcl.substr(pni, pnj - pni);
				__COUT__ << "Found process name = " << processName << __E__;

				bool sameFirst = false;
				//check before process name (ignoring comments)
				std::string newPiece = returnFcl.substr(cmi, pni - cmi);
				if(flattenedLastFclParts[0].size() && lastBuilderFcl[0].size() &&
				   lastBuilderFcl[0] == newPiece)
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
						__COUT__ << "Found fcl match! Reuse for " << builderUID << __E__;
						captureAsLastFcl =
						    false;  //do not overwrite current last fcl now!
						needToFlatten = false;

						//do rapid flatten here
						std::string outFile =
						    getFlatFHICLFilename(ARTDAQAppType::EventBuilder, builderUID);
						__COUTVS__(3, outFile);
						std::ofstream ofs{outFile};
						if(!ofs)
						{
							__SS__ << "Failed to open fhicl output file '" << outFile
							       << "!'" << __E__;
							__SS_THROW__;
						}
						std::ostringstream out;
						out << flattenedLastFclParts[0] << "process_name: \""
						    << processName << "\"" << flattenedLastFclParts[1];

						ofs << out.str();
						fclMap_[ARTDAQAppType::EventBuilder][builder.first] = out.str();

						__COUTT__ << builderUID << " Flatten Clock time = "
						          << artdaq::TimeUtils::GetElapsedTime(startClock)
						          << __E__;
						continue;  //done with shortcut-to-flatten
					}              //end shortcut-to-flatten handling
				}
				if(captureAsLastFcl)  //if interesting for more, save piece
					lastBuilderFcl[1] = newPiece;
			}
		}

		std::string& returnFclRef = returnFcl;
		if(needToFlatten)
		{
			ARTDAQTableBase::flattenFHICL(
			    ARTDAQAppType::EventBuilder,
			    builderUID,
			    &(fclMap_[ARTDAQAppType::EventBuilder]
			             [builder.first]));  //captureAsLastFcl ? &returnFcl : nullptr);

			if(captureAsLastFcl)
				returnFclRef = fclMap_[ARTDAQAppType::EventBuilder][builder.first];
		}
		else
			__COUT__ << "Skipping full flatten for " << builderUID << __E__;

		//save parts without process name
		__COUTV__(captureAsLastFcl);
		if(captureAsLastFcl)
		{
			size_t pnj = std::string::npos;
			auto   pni = returnFclRef.find("process_name:");  //find process name
			if(pni != std::string::npos)
			{
				//enforce white space before process name
				if(pni &&
				   (returnFclRef[pni - 1] == ' ' || returnFclRef[pni - 1] == '\n' ||
				    returnFclRef[pni - 1] == '\t'))
					pnj = returnFclRef.find('\n', pni);
			}
			if(pnj != std::string::npos)
			{
				__COUT__
				    << "Found flattened '"  //Note: returnFclRef.substr(pni, pnj - pni) includes "process_name:"
				    << returnFclRef.substr(pni, pnj - pni) << "' at pos " << pni << " of "
				    << returnFclRef.size() << __E__;
				flattenedLastFclParts[0] = returnFclRef.substr(0, pni);
				flattenedLastFclParts[1] = returnFclRef.substr(pnj);
			}
			else
			{
				__COUT_WARN__ << "Failed to capture fcl for " << processName << "!"
				              << __E__;
			}
		}
	}  //end builder fcl handling loop
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
