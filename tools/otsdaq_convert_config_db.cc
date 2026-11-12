/// otsdaq_convert_config_db
///	Copies a configuration store between the artdaq-database format (filesystemdb://)
///	and the plain text format (textdb://), in either direction. Every table version and
///	every group key is copied with its number unchanged, so active group keys and
///	aliases stay valid. Table text is copied as stored (no table schema involved).
///
///	usage:
///		otsdaq_convert_config_db <sourceURI> <destinationURI> [--verify] [--dry-run]
///	examples:
///		otsdaq_convert_config_db filesystemdb:///path/filesystemdb/test_db textdb:///path/textdb/test_db
///		otsdaq_convert_config_db textdb:///path/textdb/test_db filesystemdb:///path/filesystemdb/new_db
///
///	Exactly one side must be textdb://. Entries already present in the destination are
///	skipped, so an interrupted run can be repeated. GroupCache_* tables (artdaq-database
///	lookup caches) are not copied; the artdaq-database plugin rebuilds them as groups are
///	saved. Reading groups from an artdaq-database source may add such cache entries to it.

#include "otsdaq/ConfigurationInterface/ConfigurationInterface.h"
#include "otsdaq/ConfigurationInterface/MakeConfigurationInterface.h"
#include "otsdaq/TableCore/TableBase.h"

#include <cstdlib>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>

using namespace ots;

namespace
{
const std::string TEXT_SCHEME = "textdb://";

bool isText(const std::string& uri) { return uri.rfind(TEXT_SCHEME, 0) == 0; }

/// table names are alphanumeric; JSONDoc_ documents may also hold _ (as TableBase allows)
bool isTableName(const std::string& name)
{
	if(name.empty())
		return false;
	bool isDoc = name.rfind(TableBase::JSON_DOC_PREPEND, 0) == 0;
	for(char c : name)
		if(!((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
		     (isDoc && c == '_')))
			return false;
	return true;
}

/// remove white space so artdaq-database reformatting does not count as a difference
std::string squeeze(const std::string& s)
{
	std::string out;
	out.reserve(s.size());
	for(char c : s)
		if(c != ' ' && c != '\t' && c != '\n' && c != '\r')
			out += c;
	return out;
}

void usage(const char* exe)
{
	std::cout << "usage: " << exe
	          << " <sourceURI> <destinationURI> [--verify] [--dry-run]\n"
	             "  URIs: filesystemdb:///<path> (artdaq-database) or textdb:///<path> "
	             "(plain text); exactly one must be textdb://\n"
	             "  --verify   read every copied entry back from the destination and "
	             "compare\n"
	             "  --dry-run  list what would be copied, write nothing\n";
}
}  // namespace

int main(int argc, char* argv[])
{
	std::string srcURI, dstURI;
	bool        verify = false, dryRun = false;
	for(int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];
		if(arg == "--verify")
			verify = true;
		else if(arg == "--dry-run")
			dryRun = true;
		else if(arg == "-h" || arg == "--help")
		{
			usage(argv[0]);
			return 0;
		}
		else if(srcURI.empty())
			srcURI = arg;
		else if(dstURI.empty())
			dstURI = arg;
		else
		{
			usage(argv[0]);
			return 1;
		}
	}
	if(srcURI.empty() || dstURI.empty() || isText(srcURI) == isText(dstURI))
	{
		usage(argv[0]);
		return 1;
	}

	// The text plugin reads ARTDAQ_DATABASE_URI once, at construction. The artdaq-database
	// plugin reads it on every call. So: build the text side first, then leave the
	// variable set to the artdaq-database side for the rest of the run.
	const std::string& textURI = isText(srcURI) ? srcURI : dstURI;
	const std::string& dbURI   = isText(srcURI) ? dstURI : srcURI;

	std::unique_ptr<ConfigurationInterface> textIfc, dbIfc;
	try
	{
		setenv("ARTDAQ_DATABASE_URI", textURI.c_str(), 1);
		textIfc.reset(makeConfigurationInterface("Text"));
	}
	catch(const std::exception& e)
	{
		std::cout << "ERROR creating the Text configuration store plugin: " << e.what()
		          << "\n(is setup_ots.sh sourced, so CET_PLUGIN_PATH finds the plugins?)"
		          << std::endl;
		return 1;
	}
	try
	{
		setenv("ARTDAQ_DATABASE_URI", dbURI.c_str(), 1);
		dbIfc.reset(makeConfigurationInterface("Database"));
	}
	catch(const std::exception& e)
	{
		std::cout
		    << "ERROR creating the Database (artdaq-database) configuration store "
		       "plugin: "
		    << e.what()
		    << "\n(it needs an ots lite or full build; ots maxlite does not build it)"
		    << std::endl;
		return 1;
	}
	ConfigurationInterface* src = isText(srcURI) ? textIfc.get() : dbIfc.get();
	ConfigurationInterface* dst = isText(srcURI) ? dbIfc.get() : textIfc.get();

	std::cout << "Source:      " << srcURI << "\nDestination: " << dstURI
	          << (dryRun ? "\n(dry run: nothing is written)" : "") << std::endl;

	size_t tablesCopied = 0, tablesSkipped = 0, groupsCopied = 0, groupsSkipped = 0,
	       verified = 0, errors = 0, notTables = 0;

	//==============================================================================
	// table versions (including TableGroupMetadata, aliases tables and JSONDoc_* documents)
	std::set<std::string> tableNames;
	try
	{
		tableNames = src->getAllTableNames();
	}
	catch(const std::exception& e)
	{
		std::cout << "ERROR listing source tables: " << e.what() << std::endl;
		return 1;
	}

	for(const auto& tableName : tableNames)
	{
		if(tableName.rfind(TableBase::GROUP_CACHE_PREPEND, 0) == 0)
			continue;  // lookup cache, rebuilt by the artdaq-database plugin

		if(!isTableName(tableName))
		{
			std::cout << "Skipping '" << tableName
			          << "': not a table name (stray directory in the store?)"
			          << std::endl;
			++notTables;
			continue;
		}

		std::set<TableVersion> srcVersions, dstVersions;
		try
		{
			TableBase versionLister(true /*special table*/, tableName);
			srcVersions = src->getVersions(&versionLister);
			dstVersions = dst->getVersions(&versionLister);
		}
		catch(const std::exception& e)
		{
			std::cout << "ERROR listing versions of " << tableName << ": " << e.what()
			          << std::endl;
			++errors;
			continue;
		}

		size_t copiedHere = 0;
		for(const auto& version : srcVersions)
		{
			if(dstVersions.count(version))
			{
				++tablesSkipped;
				continue;
			}
			if(dryRun)
			{
				std::cout << "would copy " << tableName << "-v" << version << std::endl;
				++tablesCopied;
				continue;
			}
			try
			{
				std::string json = src->loadRawTableJSON(tableName, version);
				dst->saveRawTableJSON(tableName, version, json);
				++tablesCopied;
				++copiedHere;
				if(verify)
				{
					if(squeeze(dst->loadRawTableJSON(tableName, version)) ==
					   squeeze(json))
						++verified;
					else
					{
						std::cout << "VERIFY MISMATCH " << tableName << "-v" << version
						          << std::endl;
						++errors;
					}
				}
			}
			catch(const std::exception& e)
			{
				std::cout << "ERROR copying " << tableName << "-v" << version << ": "
				          << e.what() << std::endl;
				++errors;
			}
		}
		if(copiedHere)
			std::cout << tableName << ": " << copiedHere << " version(s) copied"
			          << std::endl;
	}  // end table loop

	//==============================================================================
	// group keys (after tables: artdaq-database needs the member versions to exist)
	std::set<std::string> groups;
	try
	{
		groups = src->getAllTableGroupNames();
	}
	catch(const std::exception& e)
	{
		std::cout << "ERROR listing source groups: " << e.what() << std::endl;
		return 1;
	}

	for(const auto& fullGroupName : groups)
	{
		std::string   groupName;
		TableGroupKey key;
		TableGroupKey::getGroupNameAndKey(fullGroupName, groupName, key);
		try
		{
			std::set<TableGroupKey> dstKeys;
			try
			{
				dstKeys = dst->getKeys(groupName);
			}
			catch(...)
			{
			}  // no keys yet
			if(dstKeys.count(key))
			{
				++groupsSkipped;
				continue;
			}
			std::map<std::string, TableVersion> members =
			    src->getTableGroupMembers(fullGroupName, true /*includeMetaDataTable*/);
			if(dryRun)
			{
				std::cout << "would copy group " << fullGroupName << " ("
				          << members.size() << " members)" << std::endl;
				++groupsCopied;
				continue;
			}
			dst->saveTableGroup(members, fullGroupName);
			++groupsCopied;
			if(verify)
			{
				if(dst->getTableGroupMembers(fullGroupName, true) == members)
					++verified;
				else
				{
					std::cout << "VERIFY MISMATCH group " << fullGroupName << std::endl;
					++errors;
				}
			}
		}
		catch(const std::exception& e)
		{
			std::cout << "ERROR copying group " << fullGroupName << ": " << e.what()
			          << std::endl;
			++errors;
		}
	}  // end group loop

	std::cout << "\nTable versions " << (dryRun ? "to copy: " : "copied: ")
	          << tablesCopied << " (already present: " << tablesSkipped << ")"
	          << "\nGroup keys " << (dryRun ? "to copy: " : "copied: ") << groupsCopied
	          << " (already present: " << groupsSkipped << ")";
	if(notTables)
		std::cout << "\nSkipped, not table names: " << notTables;
	if(verify)
		std::cout << "\nVerified: " << verified;
	std::cout << "\nErrors: " << errors << std::endl;
	return errors ? 1 : 0;
}  // end main()
