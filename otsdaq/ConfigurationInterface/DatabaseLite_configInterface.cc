#include "otsdaq/ConfigurationInterface/DatabaseLite_configInterface.h"
#include "otsdaq/Macros/ConfigurationInterfacePluginMacros.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"
#include "otsdaq/TableCore/TableBase.h"

#include <dirent.h>
#include <sys/stat.h>

#include <fstream>
#include <sstream>

using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "DatabaseLite"

namespace
{
// Minimal JSON helpers for the filesystemdb layout. The files are machine-written and
// regular, so a key search plus balanced-brace extraction is sufficient and avoids a
// JSON library dependency.

// Position just after the ':' following "key" at any depth, searching from `from`.
size_t findKey(const std::string& json, const std::string& key, size_t from = 0)
{
	const std::string quoted = "\"" + key + "\"";
	size_t            p      = json.find(quoted, from);
	while(p != std::string::npos)
	{
		size_t colon = json.find_first_not_of(" \t\r\n", p + quoted.size());
		if(colon != std::string::npos && json[colon] == ':')
			return colon + 1;
		p = json.find(quoted, p + quoted.size());
	}
	return std::string::npos;
}

// Extract the balanced {...} or [...] value starting at the first bracket at/after `from`.
std::string extractBalanced(const std::string& json, size_t from)
{
	size_t start = json.find_first_of("{[", from);
	if(start == std::string::npos)
		return "";
	char open = json[start], close = open == '{' ? '}' : ']';
	int  depth    = 0;
	bool inQuotes = false;
	for(size_t i = start; i < json.size(); ++i)
	{
		char c = json[i];
		if(inQuotes)
		{
			if(c == '\\')
				++i;
			else if(c == '"')
				inQuotes = false;
			continue;
		}
		if(c == '"')
			inQuotes = true;
		else if(c == open)
			++depth;
		else if(c == close && --depth == 0)
			return json.substr(start, i - start + 1);
	}
	return "";
}

// Top-level keys of a JSON object string.
std::set<std::string> objectKeys(const std::string& obj)
{
	std::set<std::string> keys;
	int                   depth    = 0;
	bool                  inQuotes = false;
	std::string           cur;
	bool                  expectKey = true;
	for(size_t i = 0; i < obj.size(); ++i)
	{
		char c = obj[i];
		if(inQuotes)
		{
			if(c == '\\')
			{
				cur += obj[++i];
				continue;
			}
			if(c == '"')
			{
				inQuotes = false;
				if(depth == 1 && expectKey)
					keys.insert(cur);
				continue;
			}
			cur += c;
			continue;
		}
		switch(c)
		{
		case '"': inQuotes = true; cur.clear(); break;
		case '{':
		case '[': ++depth; expectKey = (c == '{'); break;
		case '}':
		case ']': --depth; expectKey = (depth == 1); break;
		case ':': if(depth == 1) expectKey = false; break;
		case ',': if(depth == 1) expectKey = true; break;
		default: break;
		}
	}
	return keys;
}

// First string element of a JSON array string, e.g. ["abc"] -> abc.
std::string firstArrayString(const std::string& arr)
{
	size_t q1 = arr.find('"');
	if(q1 == std::string::npos)
		return "";
	size_t q2 = arr.find('"', q1 + 1);
	return q2 == std::string::npos ? "" : arr.substr(q1 + 1, q2 - q1 - 1);
}

bool isDir(const std::string& p)
{
	struct stat st;
	return ::stat(p.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}
}  // namespace

//==============================================================================
DatabaseLiteConfigurationInterface::DatabaseLiteConfigurationInterface()
{
	std::string uri = __ENV__("ARTDAQ_DATABASE_URI");  // e.g. filesystemdb:///path/filesystemdb/test_db
	const std::string scheme = "filesystemdb://";
	if(uri.compare(0, scheme.size(), scheme) != 0)
	{
		__SS__ << "OTS_LITE supports only ARTDAQ_DATABASE_URI=filesystemdb://<path>; got '" << uri << "'" << __E__;
		__SS_THROW__;
	}
	dbRoot_ = uri.substr(scheme.size());
	while(dbRoot_.size() > 1 && dbRoot_.back() == '/')
		dbRoot_.pop_back();
	if(!isDir(dbRoot_))
	{
		__SS__ << "Filesystem database root '" << dbRoot_ << "' does not exist." << __E__;
		__SS_THROW__;
	}
	__COUT__ << "Lite (read-only) filesystem database at " << dbRoot_ << __E__;
}  // end constructor()

//==============================================================================
std::string DatabaseLiteConfigurationInterface::readFile(const std::string& path) const
{
	std::ifstream in(path, std::ios::binary);
	if(!in)
	{
		__SS__ << "Cannot open '" << path << "'" << __E__;
		__SS_THROW__;
	}
	std::ostringstream ss;
	ss << in.rdbuf();
	return ss.str();
}

//==============================================================================
// index.json "version" section: { "2": ["<ouid>"], "3": ["<ouid>"] }
std::map<TableVersion, std::string> DatabaseLiteConfigurationInterface::readVersionIndex(const std::string& tableName) const
{
	std::map<TableVersion, std::string> result;
	std::string                         index = readFile(tableDir(tableName) + "/index.json");
	size_t                              vpos  = findKey(index, "version");
	if(vpos == std::string::npos)
		return result;
	std::string versions = extractBalanced(index, vpos);
	for(const std::string& key : objectKeys(versions))
	{
		char* end = nullptr;
		long  v   = std::strtol(key.c_str(), &end, 10);
		if(end == key.c_str() || *end != '\0')
			continue;
		size_t kpos = findKey(versions, key);
		if(kpos == std::string::npos)
			continue;
		std::string ouid = firstArrayString(extractBalanced(versions, kpos));
		if(!ouid.empty())
			result[TableVersion(v)] = ouid;
	}
	return result;
}

//==============================================================================
std::set<std::string> DatabaseLiteConfigurationInterface::readIndexKeys(const std::string& tableName, const std::string& section) const
{
	std::string index = readFile(tableDir(tableName) + "/index.json");
	size_t      pos   = findKey(index, section);
	if(pos == std::string::npos)
		return {};
	return objectKeys(extractBalanced(index, pos));
}

//==============================================================================
// Returns the "document.data" object of the stored document for (table, version).
std::string DatabaseLiteConfigurationInterface::loadDocumentData(const std::string& tableName, TableVersion version) const
{
	auto index = readVersionIndex(tableName);
	auto it    = index.find(version);
	if(it == index.end())
	{
		__SS__ << "Table '" << tableName << "' has no version " << version << " in " << tableDir(tableName) << "/index.json" << __E__;
		__SS_THROW__;
	}
	std::string doc  = readFile(tableDir(tableName) + "/" + it->second + ".json");
	size_t      dpos = findKey(doc, "document");
	if(dpos == std::string::npos)
	{
		__SS__ << "No 'document' in " << it->second << ".json for table '" << tableName << "'" << __E__;
		__SS_THROW__;
	}
	std::string document = extractBalanced(doc, dpos);
	size_t      datapos  = findKey(document, "data");
	if(datapos == std::string::npos)
	{
		__SS__ << "No 'document.data' in " << it->second << ".json for table '" << tableName << "'" << __E__;
		__SS_THROW__;
	}
	return extractBalanced(document, datapos);
}

//==============================================================================
void DatabaseLiteConfigurationInterface::fill(TableBase* table, TableVersion version) const
{
	const std::string& name = table->getTableName();
	__COUTT__ << "fill(" << name << ", v" << version << ")" << __E__;
	std::string data = loadDocumentData(name, version);
	// TableView::fillFromJSON parses NAME/COMMENT/AUTHOR/CREATION_TIME/DATA_SET; for the
	// special GroupCache_/JSONDoc_ tables it just stores the raw string.
	int rc = table->getViewP()->fillFromJSON(data);
	if(rc != 0)
	{
		__SS__ << "fillFromJSON failed (" << rc << ") for table '" << name << "' version " << version << __E__;
		__SS_THROW__;
	}
	table->getViewP()->setVersion(version);
}  // end fill()

//==============================================================================
void DatabaseLiteConfigurationInterface::saveActiveVersion(const TableBase* table, bool /*overwrite*/) const
{
	__SS__ << "OTS_LITE database is read-only: cannot save table '" << table->getTableName() << "'." << __E__;
	__SS_THROW__;
}

//==============================================================================
TableVersion DatabaseLiteConfigurationInterface::findLatestVersion(const TableBase* table) const noexcept
{
	auto versions = getVersions(table);
	if(versions.empty())
		return TableVersion();  // INVALID
	return *versions.rbegin();
}

//==============================================================================
std::set<TableVersion> DatabaseLiteConfigurationInterface::getVersions(const TableBase* table) const noexcept
try
{
	std::set<TableVersion> result;
	if(!isDir(tableDir(table->getTableName())))
		return result;
	for(const auto& p : readVersionIndex(table->getTableName()))
		result.insert(p.first);
	return result;
}
catch(const std::exception& e)
{
	__COUT_WARN__ << "getVersions(" << table->getTableName() << "): " << e.what() << __E__;
	return {};
}

//==============================================================================
std::set<std::string> DatabaseLiteConfigurationInterface::getAllTableNames() const
{
	std::set<std::string> names;
	DIR*                  dp = opendir(dbRoot_.c_str());
	if(!dp)
	{
		__SS__ << "Cannot list database root '" << dbRoot_ << "'" << __E__;
		__SS_THROW__;
	}
	while(dirent* e = readdir(dp))
	{
		std::string n = e->d_name;
		if(n.empty() || n[0] == '.' || !isDir(dbRoot_ + "/" + n))
			continue;
		if(n.compare(0, TableBase::GROUP_CACHE_PREPEND.size(), TableBase::GROUP_CACHE_PREPEND) == 0 ||
		   n.compare(0, TableBase::JSON_DOC_PREPEND.size(), TableBase::JSON_DOC_PREPEND) == 0)
			continue;
		names.insert(n);
	}
	closedir(dp);
	return names;
}

//==============================================================================
// Group names are the "configurations.name" keys of the TableGroupMetadata index, in the
// form <groupName>_v<key> (the artdaq-database "global configuration" names).
std::set<std::string> DatabaseLiteConfigurationInterface::getAllTableGroupNames(std::string const& filterString) const
{
	std::set<std::string> result;
	if(!isDir(tableDir(TableBase::GROUP_METADATA_TABLE_NAME)))
		return result;
	for(const std::string& n : readIndexKeys(TableBase::GROUP_METADATA_TABLE_NAME, "configurations.name"))
	{
		if(n.rfind("_v") == std::string::npos)
			continue;  // e.g. "notprovided"
		if(!filterString.empty() && n.compare(0, filterString.size(), filterString) != 0)
			continue;
		result.insert(n);
	}
	return result;
}

//==============================================================================
std::set<TableGroupKey> DatabaseLiteConfigurationInterface::getKeys(const std::string& groupName) const
{
	std::set<TableGroupKey> keys;
	for(const std::string& n : getAllTableGroupNames())
		if(n.find(groupName) == 0)
			keys.insert(TableGroupKey(n));
	return keys;
}

//==============================================================================
std::set<std::string> DatabaseLiteConfigurationInterface::findGroupsWithTable(std::string const& tableName, TableVersion version) const
{
	std::set<std::string> groups;
	for(const std::string& g : getAllTableGroupNames())
	{
		try
		{
			auto members = getTableGroupMembers(g, true);
			auto it      = members.find(tableName);
			if(it != members.end() && it->second == version)
				groups.insert(g);
		}
		catch(...)
		{
		}
	}
	return groups;
}

//==============================================================================
TableGroupKey DatabaseLiteConfigurationInterface::findLatestGroupKey(const std::string& groupName) const noexcept
{
	try
	{
		TableBase    cache(true /*special table*/, TableBase::GROUP_CACHE_PREPEND + groupName);
		TableVersion latest = findLatestVersion(&cache);
		if(!latest.isInvalid())
			return TableGroupKey(latest.version());
	}
	catch(...)
	{
	}
	std::set<TableGroupKey> keys = getKeys(groupName);
	if(!keys.empty())
		return *keys.crbegin();
	return TableGroupKey();
}

//==============================================================================
// Members come from the GroupCache_<group> document for the key, whose data block is
// {"TableName": "version", ...}. Same source the full plugin prefers.
DatabaseLiteConfigurationInterface::table_version_map_t DatabaseLiteConfigurationInterface::getCachedTableGroupMembers(std::string const& tableGroup) const
{
	std::size_t vi = tableGroup.rfind("_v");
	if(vi == std::string::npos)
	{
		__SS__ << "Table group '" << tableGroup << "' is not of the form <name>_v<key>" << __E__;
		__SS_THROW__;
	}
	std::string  groupName = tableGroup.substr(0, vi);
	TableVersion key(atoi(tableGroup.substr(vi + 2).c_str()));

	TableBase cache(true /*special table*/, TableBase::GROUP_CACHE_PREPEND + groupName);
	cache.changeVersionAndActivateView(cache.createTemporaryView(), key);
	fill(&cache, key);

	table_version_map_t retMap;
	std::string         jsonClean;
	for(char c : cache.getViewP()->getCustomStorageData())
		if(c != '{' && c != '}' && c != '"' && c != ' ' && c != '\n' && c != '\t' && c != '\r')
			jsonClean += c;
	StringMacros::getMapFromString(jsonClean, retMap);
	return retMap;
}

//==============================================================================
DatabaseLiteConfigurationInterface::table_version_map_t DatabaseLiteConfigurationInterface::getTableGroupMembers(std::string const& tableGroup, bool includeMetaDataTable) const
{
	table_version_map_t retMap = getCachedTableGroupMembers(tableGroup);
	if(!includeMetaDataTable)
		retMap.erase(TableBase::GROUP_METADATA_TABLE_NAME);
	return retMap;
}

//==============================================================================
void DatabaseLiteConfigurationInterface::saveTableGroup(table_version_map_t const& /*memberMap*/, std::string const& tableGroup) const
{
	__SS__ << "OTS_LITE database is read-only: cannot save table group '" << tableGroup << "'." << __E__;
	__SS_THROW__;
}

//==============================================================================
std::pair<std::string, TableVersion> DatabaseLiteConfigurationInterface::saveCustomJSON(const std::string& /*JSON*/, const std::string& documentNameToSave) const
{
	__SS__ << "OTS_LITE database is read-only: cannot save JSON document '" << documentNameToSave << "'." << __E__;
	__SS_THROW__;
}

//==============================================================================
std::string DatabaseLiteConfigurationInterface::loadCustomJSON(const std::string& documentNameToLoad, TableVersion documentVersionToLoad) const
{
	return loadDocumentData(TableBase::JSON_DOC_PREPEND + documentNameToLoad, documentVersionToLoad);
}
