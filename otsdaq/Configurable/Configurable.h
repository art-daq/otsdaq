#ifndef _ots_Configurable_h_
#define _ots_Configurable_h_

#include "otsdaq/ConfigurationInterface/ConfigurationTree.h"

namespace ots
{
class Configurable
{
  public:
	Configurable(const ConfigurationTree& theXDAQContextConfigTree, const std::string& theConfigurationPath);
	virtual ~Configurable();

	ConfigurationTree           getSelfNode() const;
	const ConfigurationManager* getConfigurationManager() const;

	const std::string& getContextUID() const;
	const std::string& getApplicationUID() const;

	unsigned int getApplicationLID() const;
	std::string  getContextAddress() const;
	unsigned int getContextPort() const;

  protected:
	const ConfigurationTree theXDAQContextConfigTree_;
	const std::string       theConfigurationPath_;
	const std::string       theConfigurationRecordName_;
};
}  // namespace ots

#endif
