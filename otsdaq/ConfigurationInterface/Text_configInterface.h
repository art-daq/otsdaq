#ifndef _ots_TextConfigurationInterface_h_
#define _ots_TextConfigurationInterface_h_

#include "otsdaq/ConfigurationInterface/ConfigurationInterface.h"

#include <map>
#include <set>
#include <string>

// clang-format off
namespace ots
{
class TableBase;

/// TextConfigurationInterface
///	Configuration store made of plain text files, one file per table version and one
///	file per group key, with no index files. Selected when ARTDAQ_DATABASE_URI starts
///	with textdb:// (e.g. textdb:///home/user/databases/textdb/test_db).
///
///	Layout under the root path:
///		tables/<TableName>/v<version>.json	the table version as written by TableView::printJSON
///											(the same text artdaq-database stores as document.data)
///		groups/<GroupName>/k<key>.txt		one "<TableName> <version>" line per member table
///
///	Special tables (GroupCache_*, JSONDoc_*, TableGroupMetadata) are ordinary tables here.
///	Use otsdaq_convert_config_db to copy a store to or from the artdaq-database format.
class TextConfigurationInterface : public ConfigurationInterface
{
  public:
	using table_version_map_t = std::map<std::string /*name*/, TableVersion /*version*/>;

	TextConfigurationInterface();
	~TextConfigurationInterface() { ; }

	static const std::string				URI_SCHEME;  ///< "textdb://"

	/// read table version from file
	void 									fill					(TableBase* table, TableVersion version) const override;

	/// write active table version to file; refuses an existing version unless overwrite
	void 									saveActiveVersion		(const TableBase* table, bool overwrite = false) const override;

	TableVersion 							findLatestVersion		(const TableBase* table) const noexcept override;
	std::set<std::string /*name*/> 			getAllTableNames		(void) const override;
	std::set<TableVersion> 					getVersions				(const TableBase* table) const noexcept override;

	/// group names are returned as <GroupName>_v<key>
	std::set<std::string /*name+version*/> 	getAllTableGroupNames	(std::string const& filterString = "") const override;
	std::set<TableGroupKey> 				getKeys					(const std::string& groupName) const override;
	std::set<std::string /*group*/>			findGroupsWithTable		(std::string const& tableName, TableVersion version) const override;
	TableGroupKey							findLatestGroupKey		(const std::string& groupName) const noexcept override;

	table_version_map_t 					getTableGroupMembers	(std::string const& tableGroup, bool includeMetaDataTable = false) const override;
	void 									saveTableGroup			(table_version_map_t const& memberMap, std::string const& tableGroup) const override;

	std::pair<std::string, TableVersion> 	saveCustomJSON			(const std::string& JSON, const std::string& documentNameToSave) const override;
	std::string 							loadCustomJSON			(const std::string& documentNameToLoad, TableVersion documentVersionToLoad) const override;

	const std::string&						getRootPath				(void) const { return rootPath_; }

  private:
	std::string								tablePath				(const std::string& tableName) const;
	std::string								tableFile				(const std::string& tableName, TableVersion version) const;
	std::string								groupPath				(const std::string& groupName) const;
	std::string								groupFile				(const std::string& groupName, TableGroupKey key) const;

	std::string								rootPath_;
};
}  // namespace ots

// clang-format on
#endif
