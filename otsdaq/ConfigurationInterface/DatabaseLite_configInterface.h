#ifndef _ots_DatabaseLiteConfigurationInterface_h_
#define _ots_DatabaseLiteConfigurationInterface_h_

#include "otsdaq/ConfigurationInterface/ConfigurationInterface.h"

#include <map>
#include <set>
#include <string>

namespace ots
{
class TableBase;

// OTS_LITE stand-in for DatabaseConfigurationInterface (which needs artdaq-database).
// Reads the artdaq-database "filesystemdb" layout directly:
//     <root>/<TableName>/index.json            version -> document ouid
//     <root>/<TableName>/<ouid>.json           { "document": { "data": {...} }, ... }
// where <root> comes from ARTDAQ_DATABASE_URI = filesystemdb://<root>.
// Read-only for now: every save throws.
class DatabaseLiteConfigurationInterface : public ConfigurationInterface
{
  public:
	using table_version_map_t = std::map<std::string /*name*/, TableVersion /*version*/>;

	DatabaseLiteConfigurationInterface();
	~DatabaseLiteConfigurationInterface() { ; }

	void 									fill					(TableBase* table, TableVersion version) const override;
	void 									saveActiveVersion		(const TableBase* table, bool overwrite = false) const override;

	TableVersion 							findLatestVersion		(const TableBase* table) const noexcept override;
	std::set<std::string /*name*/> 			getAllTableNames		(void) const override;
	std::set<TableVersion> 					getVersions				(const TableBase* table) const noexcept override;

	std::set<std::string /*name+version*/> 	getAllTableGroupNames	(std::string const& filterString = "") const override;
	std::set<TableGroupKey> 				getKeys					(const std::string& groupName) const override;
	std::set<std::string /*group*/>			findGroupsWithTable		(std::string const& tableName, TableVersion version) const override;
	TableGroupKey							findLatestGroupKey		(const std::string& groupName) const noexcept override;
	table_version_map_t 					getTableGroupMembers	(std::string const& tableGroup, bool includeMetaDataTable = false) const override;
	void 									saveTableGroup			(table_version_map_t const& memberMap, std::string const& tableGroup) const override;

	std::pair<std::string, TableVersion> 	saveCustomJSON			(const std::string& JSON, const std::string& documentNameToSave) const override;
	std::string 							loadCustomJSON			(const std::string& documentNameToLoad, TableVersion documentVersionToLoad) const override;

  private:
	std::string                     tableDir				(const std::string& tableName) const { return dbRoot_ + "/" + tableName; }
	std::string                     readFile				(const std::string& path) const;
	std::map<TableVersion, std::string /*ouid*/> readVersionIndex	(const std::string& tableName) const;
	std::string                     loadDocumentData		(const std::string& tableName, TableVersion version) const;
	std::set<std::string>           readIndexKeys			(const std::string& tableName, const std::string& section) const;
	table_version_map_t             getCachedTableGroupMembers(std::string const& tableGroup) const;

	std::string dbRoot_;
};
}  // namespace ots

#endif
