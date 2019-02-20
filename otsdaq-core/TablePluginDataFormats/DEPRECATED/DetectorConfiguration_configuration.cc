#include <iostream>
#include "../../Macros/TablePluginMacros.h"
#include "otsdaq-core/ConfigurationPluginDataFormats/DetectorTable.h"

using namespace ots;

//==============================================================================
DetectorConfiguration::DetectorConfiguration(void) : TableBase("DetectorConfiguration")
{
	//////////////////////////////////////////////////////////////////////
	// WARNING: the names and the order MUST match the ones in the enum  //
	//////////////////////////////////////////////////////////////////////
	// DetectorConfigurationInfo.xml
	//<?xml version="1.0" encoding="UTF-8" standalone="no" ?>
	//<ROOT xmlns:xsi="http://www.w3.org/2001/XMLSchema-instance"
	// xsi:noNamespaceSchemaLocation="TableInfo.xsd">
	//  <CONFIGURATION Name="DetectorConfiguration">
	//    <VIEW Name="DETECTOR_CONFIGURATION" Type="File,Database,DatabaseTest">
	//      <COLUMN Name="DetectorID"     StorageName="DETECTOR_ID" DataType="VARCHAR2"/>
	//      <COLUMN Name="DetectorType"   StorageName="DETECTOR_TYPE"
	//      DataType="VARCHAR2"/> <COLUMN Name="DetectorStatus"
	//      StorageName="DETECTOR_STATUS" DataType="VARCHAR2"/>
	//    </VIEW>
	//  </CONFIGURATION>
	//</ROOT>
}

//==============================================================================
DetectorConfiguration::~DetectorConfiguration(void) {}

//==============================================================================
void DetectorConfiguration::init(ConfigurationManager* configManager)
{
	nameToRow_.clear();
	detectorIDs_.clear();
	detectorTypes_.clear();
	std::string                 tmpDetectorID;
	std::map<std::string, bool> detectorTypes;
	for(unsigned int row = 0; row < TableBase::activeTableView_->getNumberOfRows(); row++)
	{
		TableBase::activeTableView_->getValue(tmpDetectorID, row, DetectorID);
		nameToRow_[tmpDetectorID]                                            = row;
		detectorTypes[TableBase::getView().getDataView()[row][DetectorType]] = true;
		detectorIDs_.push_back(tmpDetectorID);
	}
	for(auto& it : detectorTypes)
		detectorTypes_.push_back(it.first);
}

//==============================================================================
const std::vector<std::string>& DetectorConfiguration::getDetectorIDs() const
{
	return detectorIDs_;
}

//==============================================================================
const std::vector<std::string>& DetectorConfiguration::getDetectorTypes() const
{
	return detectorTypes_;
}

//==============================================================================
const std::string& DetectorConfiguration::getDetectorType(
    const std::string& detectorID) const
{
	return TableBase::getView()
	    .getDataView()[nameToRow_.find(detectorID)->second][DetectorType];
}

//==============================================================================
const std::string& DetectorConfiguration::getDetectorStatus(
    const std::string& detectorID) const
{
	return TableBase::getView()
	    .getDataView()[nameToRow_.find(detectorID)->second][DetectorStatus];
}

DEFINE_OTS_CONFIGURATION(DetectorConfiguration)