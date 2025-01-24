#ifndef _ots_DataManagerSingleton_h_
#define _ots_DataManagerSingleton_h_

#include <cassert>
#include <map>
#include <string>
#include "otsdaq/DataManager/DataManager.h"

namespace ots
{
class ConfigurationTree;

//====================================================================================
class DataManagerSingleton
{
  public:
	// There is no way I can realize that the singletonized class has been deleted!
	static void deleteInstance(std::string instanceUID)
	{
		// std::string instanceUID = composeUniqueName(supervisorContextUID,
		// supervisorApplicationUID);
		if(theInstances_.find(instanceUID) != theInstances_.end())
		{
			delete theInstances_[instanceUID];
			theInstances_.erase(theInstances_.find(instanceUID));
		}
	}

	template<class C>
	static C* getInstance(const ConfigurationTree& configurationTree,
	                      const std::string&       supervisorConfigurationPath,
	                      const std::string&       instanceUID)
	{
		if(theInstances_.find(instanceUID) == theInstances_.end())
		{
			__COUT__ << "Creating supervisor application UID: " << instanceUID
			         << " POINTER: " << theInstances_[instanceUID] << std::endl;
			theInstances_[instanceUID] = static_cast<DataManager*>(
			    new C(configurationTree, supervisorConfigurationPath));
			__COUT__ << "Creating supervisor application UID: " << instanceUID
			         << " POINTER: " << theInstances_[instanceUID] << std::endl;
		}
		else
			__COUT__ << "An instance of application UID " << instanceUID
			         << " already exists so your input parameters are ignored!"
			         << std::endl;

		return static_cast<C*>(theInstances_[instanceUID]);
	}

	static DataManager* getInstance(std::string instanceUID)
	{
		if(theInstances_.find(instanceUID) == theInstances_.end())
		{
			__COUT__ << "Can't find supervisor application UID " << instanceUID
			         << std::endl;

			__SS__ << "An instance of the class MUST already exists so I am crashing!"
			       << std::endl;
			__SS_THROW__;
		}
		else
			__COUT__ << "An instance of application UID " << instanceUID
			         << " already exists so your input parameters are ignored!"
			         << std::endl;

		return theInstances_[instanceUID];
	}

  private:
	DataManagerSingleton(void) { ; }
	~DataManagerSingleton(void) { ; }
	static std::map<std::string, DataManager*> theInstances_;
	// static std::string composeUniqueName(std::string supervisorContextUID, std::string
	// supervisorApplicationUID){return supervisorContextUID+supervisorApplicationUID;}
};

}  // namespace ots
#endif
