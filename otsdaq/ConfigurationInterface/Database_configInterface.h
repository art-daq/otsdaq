#ifndef _ots_DatabaseConfigurationInterface_h_
#define _ots_DatabaseConfigurationInterface_h_

#include "otsdaq/ConfigurationInterface/ConfigurationInterface.h"

// #include "ConfigurationInterface.h"

#include <list>
#include <map>
#include <set>
#include <stdexcept>

namespace ots
{
class TableBase;

class DatabaseConfigurationInterface : public ConfigurationInterface
{
  public:
	using table_version_map_t = std::map<std::string /*name*/, TableVersion /*version*/>;

	DatabaseConfigurationInterface();
	~DatabaseConfigurationInterface() { ; }

	bool 									IS_FILESYSTEM_DB = true;

	// read table from database
	void 									fill						(TableBase* table, TableVersion version) const;

	// write table to database	
	void 									saveActiveVersion			(const TableBase* table, bool overwrite = false) const;

	// find the latest table version by table type	
	TableVersion 							findLatestVersion			(const TableBase* table) const noexcept;

	// returns a list of all table names	
	std::set<std::string /*name*/> 			getAllTableNames			(void) const;
	// find all table versions by table type	
	std::set<TableVersion> 					getVersions					(const TableBase* table) const noexcept;

	// find all table groups in database	
	std::set<std::string /*name+version*/> 	getAllTableGroupNames		(std::string const& filterString = "") const;
	std::set<TableGroupKey>                	getKeys						(const std::string& groupName) const;
	TableGroupKey                          	findLatestGroupKey			(const std::string& groupName) const noexcept;

	// return the contents of a table group	
	table_version_map_t 					getTableGroupMembers		(std::string const& tableGroup, bool includeMetaDataTable = false) const;

	// create a new table group from the contents map	
	void 									saveTableGroup				(table_version_map_t const& memberMap, std::string const& tableGroup) const;


	std::pair<std::string, TableVersion>	saveCustomJSON				(const std::string& JSON, const std::string& documentNameToSave) const override;
	std::string		 						loadCustomJSON				(const std::string& documentNameToLoad, TableVersion documentVersionToLoad) const override;

  private:
	table_version_map_t 					getCachedTableGroupMembers	(std::string const& tableGroup) const;
	void 									saveTableGroupMemberCache	(table_version_map_t const& memberMap, std::string const& tableGroup) const;
};
}  // namespace ots

#endif
