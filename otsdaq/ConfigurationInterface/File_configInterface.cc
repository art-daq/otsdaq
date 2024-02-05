#include "otsdaq/ConfigurationInterface/File_configInterface.h"
#include <dirent.h>
#include <errno.h>
#include <iostream>
#include <set>

#include "otsdaq/ConfigurationInterface/ConfigurationHandler.h"
#include "otsdaq/Macros/ConfigurationInterfacePluginMacros.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"
#include "otsdaq/TableCore/TableBase.h"

using namespace ots;

//==============================================================================
void FileConfigurationInterface::fill(TableBase* configuration, TableVersion version) const { ConfigurationHandler::readXML(configuration, version); }

//==============================================================================
// findLatestVersion
//	return INVALID if no existing versions
TableVersion FileConfigurationInterface::findLatestVersion(const TableBase* configuration) const
{
	auto versions = getVersions(configuration);
	if(!versions.size())
		return TableVersion();  // return INVALID
	return *(versions.rbegin());
}

//==============================================================================
// save active configuration view to file
void FileConfigurationInterface::saveActiveVersion(const TableBase* configuration, bool /*overwrite*/) const { ConfigurationHandler::writeXML(configuration); }

//==============================================================================
std::set<TableVersion> FileConfigurationInterface::getVersions(const TableBase* configuration) const
{
	std::string configDir = ConfigurationHandler::getXMLDir(configuration);
	std::cout << __COUT_HDR_FL__ << "ConfigurationDir: " << configDir << std::endl;
	DIR* dp;

	struct dirent* dirp;

	if((dp = opendir(configDir.c_str())) == 0)
	{
		__SS__ << "ERROR:(" << errno << ").  Can't open directory: " << configDir << std::endl;
		__COUT_ERR__ << ss.str();
		__SS_THROW__;
	}

	const unsigned char isDir = 0x4;
	// const std::string        pDir  = ".";
	// const std::string        ppDir = "..";
	// int                 dirVersion;
	std::string::const_iterator it;
	std::string                 dirName;
	std::set<TableVersion>      dirNumbers;

	while((dirp = readdir(dp)) != 0)
	{
		if(dirp->d_type == isDir && dirp->d_name[0] != '.')
		{
			dirName = dirp->d_name;
			// Check if there are non numeric directories
			it = dirName.begin();

			while(it != dirName.end() && std::isdigit(*it))
				++it;

			if(dirName.empty() || it != dirName.end())
			{
				std::cout << __COUT_HDR_FL__ << "WARNING: there is a non numeric directory in " << configDir << " called " << dirName
				          << " please remove it from there since only numeric "
				             "directories are considered."
				          << std::endl;
				continue;
			}

			dirNumbers.insert(TableVersion(strtol(dirp->d_name, 0, 10)));
		}
	}
	closedir(dp);
	return dirNumbers;
}

DEFINE_OTS_CONFIGURATION_INTERFACE(FileConfigurationInterface)
