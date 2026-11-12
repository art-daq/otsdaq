#include "otsdaq/ConfigurationInterface/Text_configInterface.h"
#include "otsdaq/Macros/ConfigurationInterfacePluginMacros.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"
#include "otsdaq/TableCore/TableBase.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>

using namespace ots;

using table_version_map_t = ots::TextConfigurationInterface::table_version_map_t;

namespace fs = std::filesystem;

const std::string TextConfigurationInterface::URI_SCHEME = "textdb://";

namespace
{
const std::string TABLES_DIR = "tables";
const std::string GROUPS_DIR = "groups";

//==============================================================================
/// names become directory names: allow only A-Z a-z 0-9 _ - (no path characters)
void checkName(const std::string& name, const char* what)
{
	bool ok = !name.empty() && name[0] != '-';
	for(char c : name)
		if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
		     c == '_' || c == '-'))
		{
			ok = false;
			break;
		}
	if(!ok)
	{
		__SS__ << "Text configuration store: illegal " << what << " name '" << name
		       << "' (allowed characters: A-Z a-z 0-9 _ -)." << __E__;
		__SS_THROW__;
	}
}  // end checkName()

//==============================================================================
bool readFile(const std::string& path, std::string& content)
{
	std::ifstream in(path, std::ios::in | std::ios::binary);
	if(!in)
		return false;
	std::ostringstream ss;
	ss << in.rdbuf();
	content = ss.str();
	return true;
}  // end readFile()

//==============================================================================
/// write content to path through a temporary file in the same directory, so a reader
///	never sees a partial file. Without overwrite, an existing file is a collision: the
///	message contains "there was a collision", which the callers' retry loops look for.
void writeFile(const std::string& path, const std::string& content, bool overwrite)
{
	fs::path        p(path);
	std::error_code ec;
	fs::create_directories(p.parent_path(), ec);
	if(ec)
	{
		__SS__ << "Text configuration store: could not create directory '"
		       << p.parent_path().string() << "': " << ec.message() << __E__;
		__SS_THROW__;
	}

	std::stringstream tmpName;
	tmpName << path << ".tmp." << getpid() << "."
	        << std::hash<std::thread::id>{}(std::this_thread::get_id());
	const std::string tmp = tmpName.str();

	int fd = open(tmp.c_str(), O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0644);
	if(fd < 0)
	{
		__SS__ << "Text configuration store: could not create '" << tmp
		       << "': " << strerror(errno) << __E__;
		__SS_THROW__;
	}
	size_t done = 0;
	while(done < content.size())
	{
		ssize_t n = write(fd, content.data() + done, content.size() - done);
		if(n < 0)
		{
			if(errno == EINTR)
				continue;
			int err = errno;
			close(fd);
			unlink(tmp.c_str());
			__SS__ << "Text configuration store: write failed for '" << tmp
			       << "': " << strerror(err) << __E__;
			__SS_THROW__;
		}
		done += (size_t)n;
	}
	fsync(fd);  // flash storage on embedded targets: make the version durable
	close(fd);

	if(overwrite)
	{
		if(rename(tmp.c_str(), path.c_str()) != 0)
		{
			int err = errno;
			unlink(tmp.c_str());
			__SS__ << "Text configuration store: could not replace '" << path
			       << "': " << strerror(err) << __E__;
			__SS_THROW__;
		}
		return;
	}

	// no overwrite: link() refuses an existing target atomically
	if(link(tmp.c_str(), path.c_str()) == 0)
	{
		unlink(tmp.c_str());
		return;
	}
	int err = errno;
	if(err == EEXIST)
	{
		unlink(tmp.c_str());
		__SS__ << "Text configuration store: '" << path
		       << "' already exists, there was a collision with an existing entry."
		       << __E__;
		__SS_ONLY_THROW__;
	}
	if(err == EPERM || err == ENOTSUP || err == EOPNOTSUPP || err == ENOSYS)
	{
		// file system without hard links (e.g. FAT on an SD card): check, then rename
		if(fs::exists(path, ec))
		{
			unlink(tmp.c_str());
			__SS__ << "Text configuration store: '" << path
			       << "' already exists, there was a collision with an existing entry."
			       << __E__;
			__SS_ONLY_THROW__;
		}
		if(rename(tmp.c_str(), path.c_str()) == 0)
			return;
		err = errno;
	}
	unlink(tmp.c_str());
	__SS__ << "Text configuration store: could not create '" << path
	       << "': " << strerror(err) << __E__;
	__SS_THROW__;
}  // end writeFile()

//==============================================================================
/// collect the numbers from file names <prefix><number><suffix> in dir
std::set<long> listNumbered(const std::string& dir,
                            char               prefix,
                            const std::string& suffix)
{
	std::set<long>  ret;
	std::error_code ec;
	if(!fs::is_directory(dir, ec))
		return ret;
	for(const auto& entry : fs::directory_iterator(dir, ec))
	{
		const std::string name = entry.path().filename().string();
		if(name.size() <= suffix.size() + 1 || name[0] != prefix ||
		   name.compare(name.size() - suffix.size(), suffix.size(), suffix) != 0)
			continue;
		const std::string number = name.substr(1, name.size() - 1 - suffix.size());
		bool              digits = !number.empty();
		for(size_t i = 0; i < number.size(); ++i)
			if(!(number[i] >= '0' && number[i] <= '9') && !(i == 0 && number[i] == '-'))
				digits = false;
		if(digits && number != "-")
			ret.insert(std::stol(number));
	}
	return ret;
}  // end listNumbered()

//==============================================================================
std::set<std::string> listSubdirectories(const std::string& dir)
{
	std::set<std::string> ret;
	std::error_code       ec;
	if(!fs::is_directory(dir, ec))
		return ret;
	for(const auto& entry : fs::directory_iterator(dir, ec))
		if(entry.is_directory(ec))
			ret.insert(entry.path().filename().string());
	return ret;
}  // end listSubdirectories()

//==============================================================================
/// parse "<TableName> <version>" lines; blank lines and # comments are ignored
table_version_map_t parseGroupFile(const std::string& content, const std::string& path)
{
	table_version_map_t retMap;
	std::istringstream  in(content);
	std::string         line;
	while(std::getline(in, line))
	{
		size_t start = line.find_first_not_of(" \t\r");
		if(start == std::string::npos || line[start] == '#')
			continue;
		std::istringstream fields(line.substr(start));
		std::string        name;
		long               version;
		if(!(fields >> name >> version))
		{
			__SS__ << "Text configuration store: bad member line '" << line << "' in '"
			       << path << "'. Expected: <TableName> <version>" << __E__;
			__SS_THROW__;
		}
		retMap[name] = TableVersion((unsigned int)version);
	}
	return retMap;
}  // end parseGroupFile()

}  // namespace

//==============================================================================
TextConfigurationInterface::TextConfigurationInterface()
{
	const char* uri = getenv("ARTDAQ_DATABASE_URI");
	std::string uriStr(uri ? uri : "");
	if(uriStr.compare(0, URI_SCHEME.size(), URI_SCHEME) != 0 ||
	   uriStr.size() == URI_SCHEME.size())
	{
		__SS__ << "The text configuration store needs ARTDAQ_DATABASE_URI=" << URI_SCHEME
		       << "<path> (e.g. " << URI_SCHEME
		       << "/home/user/databases/textdb/test_db), found '" << uriStr << "'."
		       << __E__;
		__SS_THROW__;
	}
	rootPath_ = uriStr.substr(URI_SCHEME.size());
	while(rootPath_.size() > 1 && rootPath_.back() == '/')
		rootPath_.pop_back();

	std::error_code ec;
	fs::create_directories(rootPath_ + "/" + TABLES_DIR, ec);
	if(!ec)
		fs::create_directories(rootPath_ + "/" + GROUPS_DIR, ec);
	if(ec)
	{
		__SS__ << "Text configuration store: could not create '" << rootPath_
		       << "': " << ec.message() << __E__;
		__SS_THROW__;
	}
	__COUT__ << "Text configuration store at " << rootPath_ << __E__;
}  // end constructor()

//==============================================================================
std::string TextConfigurationInterface::tablePath(const std::string& tableName) const
{
	checkName(tableName, "table");
	return rootPath_ + "/" + TABLES_DIR + "/" + tableName;
}
std::string TextConfigurationInterface::tableFile(const std::string& tableName,
                                                  TableVersion       version) const
{
	return tablePath(tableName) + "/v" + version.toString() + ".json";
}
std::string TextConfigurationInterface::groupPath(const std::string& groupName) const
{
	checkName(groupName, "group");
	return rootPath_ + "/" + GROUPS_DIR + "/" + groupName;
}
std::string TextConfigurationInterface::groupFile(const std::string& groupName,
                                                  TableGroupKey      key) const
{
	return groupPath(groupName) + "/k" + key.toString() + ".txt";
}

//==============================================================================
/// read table version from file
void TextConfigurationInterface::fill(TableBase* table, TableVersion version) const
{
	const std::string path = tableFile(table->getTableName(), version);
	std::string       json;
	if(!readFile(path, json))
	{
		__SS__ << "\n\n======> Text configuration store error while filling '"
		       << table->getTableName() << "' version '" << version
		       << "' - are you sure this version exists? File not found: " << path
		       << __E__;
		__SS_ONLY_THROW__;
	}
	if(table->getViewP()->fillFromJSON(json) < 0)
	{
		__SS__ << "Text configuration store: could not parse '" << path << "'." << __E__;
		__SS_THROW__;
	}
	table->getViewP()->setVersion(version);  // same as the Database plugin
}  // end fill()

//==============================================================================
/// write active table version to file
void TextConfigurationInterface::saveActiveVersion(const TableBase* table,
                                                   bool             overwrite) const
{
	TableVersion version = table->getView().getVersion();
	if(version.isInvalid() || version.isTemporaryVersion())
	{
		__SS__ << "Text configuration store: refusing to save '" << table->getTableName()
		       << "' with invalid or temporary version " << version << "." << __E__;
		__SS_THROW__;
	}
	std::stringstream json;
	table->getView().printJSON(json);
	try
	{
		writeFile(tableFile(table->getTableName(), version), json.str(), overwrite);
	}
	catch(const std::runtime_error& e)
	{
		__SS__ << "Error saving table '" << table->getTableName() << "'-v" << version
		       << ": " << e.what();
		__SS_ONLY_THROW__;  // keep "there was a collision" visible to the retry loops
	}
	__COUTT__ << "Saved " << table->getTableName() << "-v" << version << __E__;
}  // end saveActiveVersion()

//==============================================================================
TableVersion TextConfigurationInterface::findLatestVersion(
    const TableBase* table) const noexcept
{
	auto versions = getVersions(table);
	if(!versions.size())
		return TableVersion();  // INVALID
	return *(versions.rbegin());
}  // end findLatestVersion()

//==============================================================================
std::set<TableVersion> TextConfigurationInterface::getVersions(
    const TableBase* table) const noexcept
try
{
	std::set<TableVersion> retSet;
	for(long v : listNumbered(tablePath(table->getTableName()), 'v', ".json"))
		retSet.insert(TableVersion((unsigned int)v));
	return retSet;
}  // end getVersions()
catch(...)
{
	return {};
}  // end getVersions() catch

//==============================================================================
std::set<std::string /*name*/> TextConfigurationInterface::getAllTableNames() const
{
	return listSubdirectories(rootPath_ + "/" + TABLES_DIR);
}  // end getAllTableNames()

//==============================================================================
/// returns <GroupName>_v<key> for every group key; filterString is a name prefix
std::set<std::string /*name*/> TextConfigurationInterface::getAllTableGroupNames(
    std::string const& filterString) const
{
	std::set<std::string> retSet;
	for(const auto& groupName : listSubdirectories(rootPath_ + "/" + GROUPS_DIR))
		for(long key :
		    listNumbered(rootPath_ + "/" + GROUPS_DIR + "/" + groupName, 'k', ".txt"))
		{
			std::string fullName = groupName + "_v" + std::to_string(key);
			if(filterString.empty() ||
			   fullName.compare(0, filterString.size(), filterString) == 0)
				retSet.insert(fullName);
		}
	return retSet;
}  // end getAllTableGroupNames()

//==============================================================================
std::set<TableGroupKey> TextConfigurationInterface::getKeys(
    const std::string& groupName) const
{
	std::set<TableGroupKey> retSet;
	for(long key : listNumbered(groupPath(groupName), 'k', ".txt"))
		retSet.insert(TableGroupKey((unsigned int)key));
	return retSet;
}  // end getKeys()

//==============================================================================
TableGroupKey TextConfigurationInterface::findLatestGroupKey(
    const std::string& groupName) const noexcept
try
{
	std::set<TableGroupKey> keys = getKeys(groupName);
	if(keys.size())
		return *(keys.crbegin());
	return TableGroupKey();  // INVALID
}  // end findLatestGroupKey()
catch(...)
{
	return TableGroupKey();
}  // end findLatestGroupKey() catch

//==============================================================================
/// tableGroup format: <GroupName>_v<key>
table_version_map_t TextConfigurationInterface::getTableGroupMembers(
    std::string const& tableGroup, bool includeMetaDataTable /* = false */) const
{
	std::string   groupName;
	TableGroupKey key;
	TableGroupKey::getGroupNameAndKey(tableGroup, groupName, key);

	const std::string path = groupFile(groupName, key);
	std::string       content;
	if(!readFile(path, content))
	{
		__SS__ << "Text configuration store: group '" << tableGroup
		       << "' not found (no file " << path << ")." << __E__;
		__SS_ONLY_THROW__;
	}
	table_version_map_t retMap = parseGroupFile(content, path);

	if(!includeMetaDataTable)
		retMap.erase(TableBase::GROUP_METADATA_TABLE_NAME);
	return retMap;
}  // end getTableGroupMembers()

//==============================================================================
/// tableGroup format: <GroupName>_v<key>
void TextConfigurationInterface::saveTableGroup(table_version_map_t const& memberMap,
                                                std::string const& tableGroup) const
{
	if(memberMap.size() == 0)
	{
		__SS__ << "Error: Attempting to save table group '" << tableGroup
		       << "' with empty member map! Please provide at least one member table for "
		          "the group."
		       << __E__;
		__SS_THROW__;
	}

	std::string   groupName;
	TableGroupKey key;
	TableGroupKey::getGroupNameAndKey(tableGroup, groupName, key);
	if(key.isInvalid())
	{
		__SS__ << "Text configuration store: invalid group key in '" << tableGroup << "'."
		       << __E__;
		__SS_THROW__;
	}

	std::stringstream content;
	for(const auto& member : memberMap)
	{
		checkName(member.first, "table");
		content << member.first << " " << member.second.toString() << "\n";
	}

	try
	{
		writeFile(groupFile(groupName, key), content.str(), false /*overwrite*/);
	}
	catch(const std::runtime_error& e)
	{
		__SS__ << "Error saving group '" << tableGroup << "': " << e.what();
		__SS_ONLY_THROW__;  // keep "there was a collision" visible to the retry loop
	}
	__COUTT__ << "Saved group " << tableGroup << __E__;
}  // end saveTableGroup()

//==============================================================================
/// returns the <GroupName>_v<key> of every group that has tableName at version
std::set<std::string /*group*/> TextConfigurationInterface::findGroupsWithTable(
    std::string const& tableName, TableVersion version) const
{
	std::set<std::string> retSet;
	for(const auto& fullName : getAllTableGroupNames())
	{
		try
		{
			table_version_map_t members = getTableGroupMembers(fullName, true);
			auto                it      = members.find(tableName);
			if(it != members.end() && it->second == version)
				retSet.insert(fullName);
		}
		catch(const std::runtime_error& e)
		{
			__COUT_WARN__ << "Skipping unreadable group " << fullName << ": " << e.what()
			              << __E__;
		}
	}
	return retSet;
}  // end findGroupsWithTable()

//==============================================================================
/// Save a json string as the next version of document JSONDoc_<name>
///	(same behavior as the Database plugin)
std::pair<std::string, TableVersion> TextConfigurationInterface::saveCustomJSON(
    const std::string& json, const std::string& documentNameToSave) const
{
	TableBase localDocSaver(true /*special table*/,
	                        TableBase::JSON_DOC_PREPEND + documentNameToSave);

	std::set<TableVersion> versions = getVersions(&localDocSaver);
	TableVersion           version;
	if(versions.size())
		version = TableVersion::getNextVersion(*versions.rbegin());
	else
		version = TableVersion::DEFAULT;

	localDocSaver.changeVersionAndActivateView(localDocSaver.createTemporaryView(),
	                                           version);
	localDocSaver.getViewP()->setCustomStorageData(json);
	saveActiveVersion(&localDocSaver, false /* overwrite */);

	return std::make_pair(localDocSaver.getTableName(),
	                      localDocSaver.getView().getVersion());
}  // end saveCustomJSON()

//==============================================================================
/// Load document JSONDoc_<name> at a version and return its content
std::string TextConfigurationInterface::loadCustomJSON(
    const std::string& documentNameToLoad, TableVersion documentVersionToLoad) const
{
	TableBase localDocLoader(true /*special table*/,
	                         TableBase::JSON_DOC_PREPEND + documentNameToLoad);
	localDocLoader.changeVersionAndActivateView(localDocLoader.createTemporaryView(),
	                                            documentVersionToLoad);
	fill(&localDocLoader, documentVersionToLoad);
	return localDocLoader.getViewP()->getCustomStorageData();
}  // end loadCustomJSON()

DEFINE_OTS_CONFIGURATION_INTERFACE(TextConfigurationInterface)
