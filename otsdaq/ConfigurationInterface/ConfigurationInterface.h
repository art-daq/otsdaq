#ifndef _ots_ConfigurationInterface_h_
#define _ots_ConfigurationInterface_h_

#include <memory>
#include <set>
#include <sstream>
#include <mutex>
#include "otsdaq/Macros/CoutMacros.h"

#include "otsdaq/TableCore/MakeTable.h"
#include "otsdaq/TableCore/TableBase.h"
#include "otsdaq/TableCore/TableGroupKey.h"
#include "otsdaq/TableCore/TableVersion.h"
#include "otsdaq/TableCore/TableView.h"

namespace ots
{
// clang-format off

class ConfigurationHandlerBase;

class ConfigurationInterface
{
	friend class ConfigurationManagerRW;  // because need access to latestVersion() call for group metadata
	friend class ConfigurationManager;    // because need access to fill() call for group metadata

protected:
	ConfigurationInterface(void);  // Protected constructor
public:
	virtual ~ConfigurationInterface() { ; }

	enum class CONFIGURATION_MODE 
	{
		DO_NOT_CREATE,
		XML_FILE,
		ARTDAQ_DATABASE
	};

	static ConfigurationInterface* 			getInstance						(CONFIGURATION_MODE mode = CONFIGURATION_MODE::DO_NOT_CREATE);
	static bool                    			isVersionTrackingEnabled		(void);
	static void                    			setVersionTrackingEnabled		(bool setValue);

	static const std::string GROUP_METADATA_TABLE_NAME;

	// table handling
	#include "otsdaq/ConfigurationInterface/ConfigurationInterface.icc"  	//define ConfigurationInterface::get() source code
	virtual std::set<std::string /*name*/> 	getAllTableNames				(void) const { __SS__; __THROW__(ss.str() + "ConfigurationInterface::... Must only call getAllTableNames in a mode with this functionality implemented (e.g. DatabaseConfigurationInterface)."); }
	virtual std::set<TableVersion> 			getVersions						(const TableBase* configuration) const = 0;
	static const CONFIGURATION_MODE&		getMode							(void);
	TableVersion                   			saveNewVersion					(TableBase*   configuration, TableVersion temporaryVersion, TableVersion newVersion = TableVersion());

	// group handling
	virtual std::set<std::string /*name*/> 	getAllTableGroupNames			(const std::string& /*filterString*/ = "") const { __SS__; __THROW__(ss.str() + "ConfigurationInterface::... Must only call getAllTableGroupNames in a mode with this functionality implemented (e.g. DatabaseConfigurationInterface)."); }
	virtual std::set<TableGroupKey> 		getKeys							(const std::string& /*groupName*/) const { __SS__; __THROW__(ss.str() + "ConfigurationInterface::... Must only call getKeys in a mode with this functionality implemented (e.g. DatabaseConfigurationInterface)."); }


	// Caution: getTableGroupMembers must be carefully used.. the table versions
	// are as initially defined for table versions aliases, i.e. not converted according
	// to the metadata groupAliases!
	virtual std::map<std::string /*name*/,
	TableVersion /*version*/> 				getTableGroupMembers			(std::string const& /*groupName*/, bool /*includeMetaDataTable*/ = false) const { __SS__; __THROW__(ss.str() + "ConfigurationInterface::... Must only call getTableGroupMembers in a mode with this functionality implemented (e.g. DatabaseConfigurationInterface)."); }
	virtual void 							saveTableGroup					(std::map<std::string /*name*/,TableVersion /*version*/> const& /*tableToVersionMap*/, std::string const& /*groupName*/) const { __SS__; __THROW__(ss.str() + "ConfigurationInterface::... Must only call saveTableGroup in a mode with this functionality implemented (e.g. DatabaseConfigurationInterface)."); }

	virtual std::pair<std::string, TableVersion>	saveCustomJSON			(const std::string& JSON, const std::string& documentNameToSave) const { __COUTV__(JSON);  __COUTV__(documentNameToSave); return std::make_pair("",TableVersion());}
	virtual std::string 					loadCustomJSON					(const std::string& documentNameToLoad, TableVersion documentVersionToLoad) const {__COUTV__(documentNameToLoad); __COUTV__(documentVersionToLoad); return "{}";};


protected:
	virtual void 							fill							(TableBase* configuration, TableVersion version) const = 0;

public:  // was protected,.. unfortunately, must be public to allow
	// otsdaq_database_migrate and otsdaq_import_system_aliases to compile
	virtual TableGroupKey 					findLatestGroupKey				(const std::string& /*groupName*/) const /* return INVALID if no existing versions */ { __SS__; __THROW__(ss.str() + "ConfigurationInterface::... Must only call findLatestGroupKey in a mode with this functionality implemented (e.g. DatabaseConfigurationInterface)."); }
	virtual TableVersion 					findLatestVersion				(const TableBase* configuration) const = 0;  // return INVALID if no existing versions
	virtual void 							saveActiveVersion				(const TableBase* configuration, bool overwrite = false) const = 0;

protected:
	ConfigurationHandlerBase* 				theConfigurationHandler_;

private:
	static ConfigurationInterface* 			theInstance_;
	static CONFIGURATION_MODE     			theMode_;  				
	static bool								theVersionTrackingEnabled_;  	// tracking versions 1 is enabled, 0 is disabled

	std::mutex								tableReaderMutex_;

};

// clang-format on
}  // namespace ots
#endif
