// #include <otsdaq_demo/otsdaq-demo/FEInterfaces/FEWROtsUDPFSSRInterface.h>
// #include
//<otsdaq_demo/otsdaq-demo/UserConfigurationDataFormats/FEWROtsUDPFSSRInterfaceConfiguration.h>
#include "otsdaq/ConfigurationInterface/ConfigurationManager.h"
#include "otsdaq/FECore/FEVInterfacesManager.h"
// #include "otsdaq/TableDataFormats/TableGroupKey.h"

// #include "otsdaq-demo/FEInterfaces/FEWOtsGenericInterface.h"
#include "otsdaq/FECore/FEVInterface.h"

#include <iostream>
#include <memory>

#include "otsdaq/ConfigurationInterface/ConfigurationInterface.h"
#include "otsdaq/FECore/MakeInterface.h"
#include "otsdaq/TableCore/MakeTable.h"

// #include "otsdaq-components/FEInterfaces/FEWOtsUDPFSSRInterface.h"
// #include "otsdaq-components/FEInterfaces/FEWOtsUDPHCALInterface.h"

using namespace ots;

int main()
{
	// Variables
	std::string supervisorContextUID_           = "MainContext";
	std::string supervisorApplicationUID_       = "FeSupervisor0";
	std::string ConfigurationAlias_             = "Physics";
	std::string theSupervisorConfigurationPath_ = supervisorContextUID_ + "/LinkToApplicationTable/" + supervisorApplicationUID_ + "/LinkToSupervisorTable";
	// const int TableGroupKeyValue_ = 0;
	// std::shared_ptr<TableGroupKey> theConfigurationTableGroupKey_(new
	// TableGroupKey(TableGroupKeyValue_));

	////////////////////////////////////////////////////////////////
	// INSERTED GLOBALLY IN THE CODE
	ConfigurationManager* theConfigurationManager_ = new ConfigurationManager;
	FEVInterfacesManager  theFEVInterfacesManager_(theConfigurationManager_->getNode(ConfigurationManager::XDAQ_CONTEXT_TABLE_NAME),
                                                  theSupervisorConfigurationPath_);

	std::pair<std::string /*group name*/, TableGroupKey> theGroup = theConfigurationManager_->getTableGroupFromAlias(ConfigurationAlias_);

	theConfigurationManager_->loadTableGroup(theGroup.first, theGroup.second, true);

	theFEVInterfacesManager_.configure();
	////////////////////////////////////////////////////////////////
	exit(0);
	////////////////////////////////////////////////////////////////
	// Getting just the informations about the FEWInterface

	//	const std::string feId_ = "0";
	//	ConfigurationInterface* theInterface_;
	//	theInterface_ = ConfigurationInterface::getInstance(true);//FIXME This will be
	// variable because if false it takes it from the database 	Configurations*
	// configurations = 0;
	//	theInterface_->get((TableBase*&)configurations,"Configurations");
	//	TableBase* frontEndConfiguration = 0;
	//	theInterface_->get(frontEndConfiguration, "FEConfiguration",
	// theConfigurationTableGroupKey_,
	// configurations);
	//
	//	const std::string interfaceName     =  "FEOtsUDPFSSRInterface";
	//	const std::string configurationName =  interfaceName + "Table";
	//
	//	//FEWOtsUDPHardwareConfiguration* interfaceConfiguration_ = 0;
	//
	//	TableBase* interfaceConfiguration_ =
	// 0;//makeConfigurationInterface(configurationName);
	//
	//	if(configurations->findKOC(*theConfigurationTableGroupKey_,configurationName))
	//	{
	//		theInterface_->get(interfaceConfiguration_, configurationName,
	// theConfigurationTableGroupKey_, configurations);
	//	}
	//
	//	std::unique_ptr<FEVInterface> theFEWInterface  = makeInterface(interfaceName,
	// feId_, (FEInterfaceTableBase*)interfaceConfiguration_);

	////////////////////////////////////////////////////////////////

	return 0;
}
