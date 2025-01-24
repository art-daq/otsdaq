#ifndef _ots_FileConfigurationInterface_h_
#define _ots_FileConfigurationInterface_h_

#include "otsdaq/ConfigurationInterface/ConfigurationInterface.h"

#include <set>

namespace ots
{
class TableBase;

class FileConfigurationInterface : public ConfigurationInterface
{
  public:
	FileConfigurationInterface() { ; }
	virtual ~FileConfigurationInterface() { ; }

	// read configuration from database
	void fill(TableBase* /*configuration*/, TableVersion /*version*/) const;

	// write configuration to database
	void saveActiveVersion(const TableBase* /*configuration*/,
	                       bool overwrite = false) const;

	// find the latest configuration version by configuration type
	TableVersion findLatestVersion(const TableBase* /*configuration*/) const;

	// find all configuration versions by configuration type
	std::set<TableVersion> getVersions(const TableBase* /*configuration*/) const;

  private:
};
}  // namespace ots

#endif
