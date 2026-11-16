#include "otsdaq/ConfigurationInterface/ConfigurationInterface.h"
#include "otsdaq/ConfigurationInterface/MakeConfigurationInterface.h"

#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <dirent.h>
#include <cassert>
#include <cstdlib>
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

	std::string instanceType =
	    (mode == CONFIGURATION_MODE::XML_FILE) ? "File" : getDatabasePluginName();
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
/// getDatabasePluginName
///	The database mode serves both stores; the URI scheme picks the plugin:
///		textdb://<path>			-> "Text" (plain text files, Text_configInterface)
///		anything else			-> "Database" (artdaq-database, e.g. filesystemdb://<path>)
std::string ConfigurationInterface::getDatabasePluginName(void)
{
	const char* uri = getenv("ARTDAQ_DATABASE_URI");
	if(uri && std::string(uri).rfind("textdb://", 0) == 0)
		return "Text";
	return "Database";
}  // end getDatabasePluginName()

//==============================================================================
/// loadRawTableJSON
///	Returns the stored text of a table version exactly as the store holds it, without
///	needing the table schema. Uses a special JSONDoc_ table, whose views pass their
///	text through printJSON/fillFromJSON unchanged, renamed to the real table name after
///	its view exists so the store reads the real table's entry.
std::string ConfigurationInterface::loadRawTableJSON(const std::string& tableName,
                                                     TableVersion       version) const
{
	TableBase rawTable(true /*special table*/,
	                   TableBase::JSON_DOC_PREPEND + "RawTableCopy");
	rawTable.changeVersionAndActivateView(rawTable.createTemporaryView(), version);
	rawTable.setTableName(tableName);  // view keeps the pass-through name
	fill(&rawTable, version);
	return rawTable.getViewP()->getCustomStorageData();
}  // end loadRawTableJSON()

//==============================================================================
/// saveRawTableJSON
///	Stores text as the given table version, without needing the table schema
///	(see loadRawTableJSON).
void ConfigurationInterface::saveRawTableJSON(const std::string& tableName,
                                              TableVersion       version,
                                              const std::string& json,
                                              bool               overwrite) const
{
	TableBase rawTable(true /*special table*/,
	                   TableBase::JSON_DOC_PREPEND + "RawTableCopy");
	rawTable.changeVersionAndActivateView(rawTable.createTemporaryView(), version);
	rawTable.getViewP()->setCustomStorageData(json);
	rawTable.setTableName(tableName);  // view keeps the pass-through name
	saveActiveVersion(&rawTable, overwrite);
}  // end saveRawTableJSON()

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
