#ifndef _ots_ARTDAQDataManager_h_
#define _ots_ARTDAQDataManager_h_

#include "otsdaq/DataManager/DataManager.h"

namespace ots
{
class ConfigurationManager;

// ARTDAQDataManager
//	This class provides the otsdaq interface to a single artdaq Board Reader.
class ARTDAQDataManager : public DataManager
{
  public:
	ARTDAQDataManager(const ConfigurationTree& theXDAQContextConfigTree, const std::string& supervisorConfigurationPath);
	virtual ~ARTDAQDataManager(void);
	void configure(void);
	void stop(void);

  private:
	int rank_;
};

}  // namespace ots

#endif
