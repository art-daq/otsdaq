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
ConfigurationInterface*                    ConfigurationInterface::theInstance_ = nullptr;
ConfigurationInterface::CONFIGURATION_MODE ConfigurationInterface::theMode_ =
    ConfigurationInterface::CONFIGURATION_MODE::DO_NOT_CREATE;
bool ConfigurationInterface::theVersionTrackingEnabled_ = true;

//==============================================================================
ConfigurationInterface::ConfigurationInterface() {}

//==============================================================================
ConfigurationInterface* ConfigurationInterface::getInstance(
    ConfigurationInterface::CONFIGURATION_MODE mode /* = DO_NOT_CREATE */)
{
	if(mode == CONFIGURATION_MODE::DO_NOT_CREATE)
	{
		if(theInstance_ == nullptr)
			std::cout
			    << __COUT_HDR_FL__
			    << "WARNING -- returning a nullptr ConfigurationInterface::theInstance_"
			    << __E__;
		return theInstance_;
	}

	auto instanceType = (mode == CONFIGURATION_MODE::XML_FILE) ? "File" : "Database";
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
}  //end getInstance()

//==============================================================================
bool ConfigurationInterface::isVersionTrackingEnabled()
{
	return ConfigurationInterface::theVersionTrackingEnabled_;
}

//==============================================================================
void ConfigurationInterface::setVersionTrackingEnabled(bool setValue)
{
	ConfigurationInterface::theVersionTrackingEnabled_ = setValue;
}

//==============================================================================
const ConfigurationInterface::CONFIGURATION_MODE& ConfigurationInterface::getMode()
{
	return ConfigurationInterface::theMode_;
}

//==============================================================================
/// saveNewVersion
/// 	If newVersion is 0, then save the temporaryVersion as the next positive version
/// number,
///		save using the interface, and return the new version number
///	If newVersion is non 0, attempt to save as given newVersion number, else throw
/// exception. 	return TableVersion::INVALID on failure
TableVersion ConfigurationInterface::saveNewVersion(TableBase*   table,
                                                    TableVersion temporaryVersion,
                                                    TableVersion newVersion)
{
	if(!temporaryVersion.isTemporaryVersion() || !table->isStored(temporaryVersion))
	{
		__COUT__ << "Invalid temporary version number: " << temporaryVersion << std::endl;
		return TableVersion();  // return INVALID
	}

	if(!ConfigurationInterface::isVersionTrackingEnabled())  // tracking is OFF, so always
	                                                         // save to same version
		newVersion = TableVersion::SCRATCH;

	bool rewriteableExists = false;

	std::set<TableVersion> versions = getVersions(table);
	__COUTTV__(StringMacros::setToString(versions));
	if(newVersion == TableVersion::INVALID)
	{
		if(versions.size())
			__COUTTV__(*(versions.rbegin()));
		if(versions.size() > 1)
			__COUTTV__(*(++versions.rbegin()));

		if(versions
		       .size() &&  // 1 more than last version, if any non-scratch versions exist
		   *(versions.rbegin()) != TableVersion(TableVersion::SCRATCH))
			newVersion = TableVersion::getNextVersion(*(versions.rbegin()));
		else if(versions.size() >
		        1)  // if scratch exists, take 1 more than second to last version
			newVersion = TableVersion::getNextVersion(
			    *(++(versions.rbegin())));  //NOTE: ++ is reverse for a reverse_iterator!!
		else
			newVersion = TableVersion::DEFAULT;
		__COUT__ << "Next available version number is " << newVersion << std::endl;
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
		__COUT__ << "newVersion(" << newVersion << ") already exists!" << std::endl;
		rewriteableExists = newVersion == TableVersion::SCRATCH;

		// throw error if version already exists and this is not the rewriteable version
		if(!rewriteableExists || ConfigurationInterface::isVersionTrackingEnabled())
		{
			__SS__ << ("New version already exists!") << std::endl;
			__SS_THROW__;
		}
	}

	__COUT__ << "Version number to save is " << newVersion << std::endl;

	// copy to new version
	table->changeVersionAndActivateView(temporaryVersion, newVersion);

	// save to disk
	//	only allow overwrite if version tracking is disabled AND the rewriteable version
	//		already exists.
	bool overwrite =
	    !ConfigurationInterface::isVersionTrackingEnabled() && rewriteableExists;
	uint16_t retries   = overwrite ? 4 : 0;  //only allow retries if not overwriting
	auto     tableView = table->getViewP();
	while(1)
	{
		try
		{
			saveActiveVersion(table, overwrite);
		}
		catch(const std::runtime_error& e)
		{
			__COUT__ << "Caught runtime_error exception during table save." << __E__;
			if(std::string(e.what()).find("there was a collision") != std::string::npos)
			{
				__COUT_WARN__ << "There was a collision saving the new table "
				              << *tableView << "(" << newVersion
				              << "), trying incremented table version... retries="
				              << retries << __E__;
				if(++retries > 3)  //give up
					throw;
				newVersion =
				    TableVersion::getNextVersion(newVersion);  //increment table version
				tableView->setVersion(newVersion);
				__COUT__ << "New version for table: " << *tableView << " found as "
				         << newVersion << __E__;
				continue;
			}
			else
				throw;
		}

		__COUT__ << "Created table: " << *tableView << "-v" << newVersion << __E__;
		break;
	}  //end collission retry loop

	return newVersion;
}  //end saveNewVersion()

//==============================================================================
/// getVersionsWithMetadata
///		Returns a vector of TableVersionMetadata containing version numbers
///		along with their creation time, author, and comment.
///		This allows for filtering versions by date without loading full table data.
std::vector<TableVersionMetadata> ConfigurationInterface::getVersionsWithMetadata(
    const TableBase* table) const
{
	std::vector<TableVersionMetadata> result;
	std::set<TableVersion>            versions = getVersions(table);
	
	// Create a temporary table to load metadata for each version
	TableBase tempTable(table->getTableName());
	
	for(const auto& version : versions)
	{
		try
		{
			// Load the table to get metadata
			tempTable.reset();
			tempTable.init();
			tempTable.setTableVersion(version);
			fill(&tempTable, version);
			
			const TableView& view = tempTable.getView();
			TableVersionMetadata metadata(
			    version,
			    view.getCreationTime(),
			    view.getAuthor(),
			    view.getComment()
			);
			result.push_back(metadata);
		}
		catch(const std::exception& e)
		{
			__COUT_WARN__ << "Failed to load metadata for table '" << table->getTableName() 
			              << "' version " << version << ": " << e.what() << __E__;
			// Add entry with unknown metadata
			result.push_back(TableVersionMetadata(version, 0, "Unknown", ""));
		}
	}
	
	return result;
}  //end getVersionsWithMetadata()

//==============================================================================
/// filterVersionsByDateRange
///		Filters versions to only include those created within the specified time range.
///		startTime and endTime are Unix timestamps (seconds since epoch).
///		Use startTime=0 for no lower bound, endTime=0 or very large value for no upper bound.
std::vector<TableVersionMetadata> ConfigurationInterface::filterVersionsByDateRange(
    const std::vector<TableVersionMetadata>& versions,
    time_t                                   startTime,
    time_t                                   endTime) const
{
	std::vector<TableVersionMetadata> filtered;
	
	for(const auto& versionMeta : versions)
	{
		// If creation time is 0, it's unknown, so include it to be safe
		if(versionMeta.creationTime == 0)
		{
			filtered.push_back(versionMeta);
			continue;
		}
		
		// Check if within date range
		bool afterStart = (startTime == 0 || versionMeta.creationTime >= startTime);
		bool beforeEnd  = (endTime == 0 || versionMeta.creationTime <= endTime);
		
		if(afterStart && beforeEnd)
		{
			filtered.push_back(versionMeta);
		}
	}
	
	return filtered;
}  //end filterVersionsByDateRange()

//==============================================================================
/// filterVersionsLastNDays
///		Convenience method to filter versions created in the last N days.
///		For example, numDays=7 returns versions from the last week.
std::vector<TableVersionMetadata> ConfigurationInterface::filterVersionsLastNDays(
    const std::vector<TableVersionMetadata>& versions,
    unsigned int                             numDays) const
{
	time_t now       = time(0);
	time_t startTime = now - (numDays * 24 * 60 * 60);  // N days ago
	
	return filterVersionsByDateRange(versions, startTime, now);
}  //end filterVersionsLastNDays()
