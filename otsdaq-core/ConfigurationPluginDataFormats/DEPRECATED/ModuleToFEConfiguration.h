#ifndef _ots_ModuleToFEConfiguration_h_
#define _ots_ModuleToFEConfiguration_h_

#include "otsdaq-core/ConfigurationDataFormats/ConfigurationBase.h"

#include <list>
#include <string>

namespace ots
{
class ModuleToFEConfiguration : public ConfigurationBase
{
  public:
	ModuleToFEConfiguration(void);
	virtual ~ModuleToFEConfiguration(void);

	//Methods
	void init(ConfigurationManager *configManager);

	//Getters
	std::list<std::string> getFEWModulesList(unsigned int FEWNumber) const;
	std::list<std::string> getFERModulesList(unsigned int FERNumber) const;

  private:
	enum
	{
		ModuleName,
		ModuleType,
		FEWName,
		FEWType,
		FERName,
		FERType
	};
};
}  // namespace ots
#endif
