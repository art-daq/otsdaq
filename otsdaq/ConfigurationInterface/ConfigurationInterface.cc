#include "otsdaq/ConfigurationInterface/ConfigurationInterface.h"
#include "otsdaq/ConfigurationInterface/MakeConfigurationInterface.h"

#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <dirent.h>
#include <cassert>
#include <iostream>
#include <typeinfo>

using namespace ots;

#define DEBUG_CONFIGURATION true

//==============================================================================
ConfigurationInterface* ConfigurationInterface::theInstance_               = nullptr;
bool                    ConfigurationInterface::theMode_                   = true;
bool                    ConfigurationInterface::theVersionTrackingEnabled_ = true;

const std::string ConfigurationInterface::GROUP_METADATA_TABLE_NAME = "TableGroupMetadata";

//==============================================================================
ConfigurationInterface::ConfigurationInterface() {}

//==============================================================================
ConfigurationInterface* ConfigurationInterface::getInstance(bool mode)
{
	auto instanceType = mode ? "File" : "Database";
	if(theMode_ != mode)
	{
		delete theInstance_;
		theInstance_ = nullptr;
	}
	if(theInstance_ == nullptr)
	{
		theInstance_ = makeConfigurationInterface(instanceType);
	}

	theMode_ = mode;
	return theInstance_;
} //end getInstance()

//==============================================================================
bool ConfigurationInterface::isVersionTrackingEnabled() { return ConfigurationInterface::theVersionTrackingEnabled_; }

//==============================================================================
void ConfigurationInterface::setVersionTrackingEnabled(bool setValue) { ConfigurationInterface::theVersionTrackingEnabled_ = setValue; }

//==============================================================================
// saveNewVersion
// 	If newVersion is 0, then save the temporaryVersion as the next positive version
// number,
//		save using the interface, and return the new version number
//	If newVersion is non 0, attempt to save as given newVersion number, else throw
// exception. 	return TableVersion::INVALID on failure
TableVersion ConfigurationInterface::saveNewVersion(TableBase* table, TableVersion temporaryVersion, TableVersion newVersion)
{
	if(!temporaryVersion.isTemporaryVersion() || !table->isStored(temporaryVersion))
	{
		std::cout << __COUT_HDR_FL__ << "Invalid temporary version number: " << temporaryVersion << std::endl;
		return TableVersion();  // return INVALID
	}

	if(!ConfigurationInterface::isVersionTrackingEnabled())  // tracking is OFF, so always
	                                                         // save to same version
		newVersion = TableVersion::SCRATCH;

	bool rewriteableExists = false;

	std::set<TableVersion> versions = getVersions(table);
	if(newVersion == TableVersion::INVALID)
	{
		if(versions.size() &&  // 1 more than last version, if any non-scratch versions exist
		   *(versions.rbegin()) != TableVersion(TableVersion::SCRATCH))
			newVersion = TableVersion::getNextVersion(*(versions.rbegin()));
		else if(versions.size() > 1)  // if scratch exists, take 1 more than second to last version
			newVersion = TableVersion::getNextVersion(*(--(versions.rbegin())));
		else
			newVersion = TableVersion::DEFAULT;
		std::cout << __COUT_HDR_FL__ << "Next available version number is " << newVersion << std::endl;
		//
		//		//for sanity check, compare with config's idea of next version
		//		TableVersion baseNextVersion = table->getNextVersion();
		//		if(newVersion <= baseNextVersion)
		//			newVersion = TableVersion::getNextVersion(baseNextVersion);
		//
		//		std::cout << __COUT_HDR_FL__ << "After considering baseNextVersion, " <<
		// baseNextVersion <<
		//				", next available version number is " << newVersion << std::endl;
	}
	else if(versions.find(newVersion) != versions.end())
	{
		std::cout << __COUT_HDR_FL__ << "newVersion(" << newVersion << ") already exists!" << std::endl;
		rewriteableExists = newVersion == TableVersion::SCRATCH;

		// throw error if version already exists and this is not the rewriteable version
		if(!rewriteableExists || ConfigurationInterface::isVersionTrackingEnabled())
		{
			__SS__ << ("New version already exists!") << std::endl;
			std::cout << __COUT_HDR_FL__ << ss.str();
			__SS_THROW__;
		}
	}

	std::cout << __COUT_HDR_FL__ << "Version number to save is " << newVersion << std::endl;

	// copy to new version
	table->changeVersionAndActivateView(temporaryVersion, newVersion);

	// save to disk
	//	only allow overwrite if version tracking is disabled AND the rewriteable version
	//		already exists.
	saveActiveVersion(table, !ConfigurationInterface::isVersionTrackingEnabled() && rewriteableExists);

	return newVersion;
} //end saveNewVersion()
