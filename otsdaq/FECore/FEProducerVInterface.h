#ifndef _ots_FEProducerVInterface_h_
#define _ots_FEProducerVInterface_h_

#include "otsdaq/DataManager/DataProducerBase.h"
#include "otsdaq/FECore/FEVInterface.h"

#include <array>
#include <iostream>
#include <string>
#include <vector>

namespace ots
{
// FEProducerVInterface
//	This class is a virtual class defining the features of front-end interface plugin
// class. 	The features include configuration hooks, finite state machine handlers,
// Front-end Macros for web accessible C++ handlers, slow controls hooks, as well as
// universal write and read for 	Macro Maker compatibility.
class FEProducerVInterface : public FEVInterface, public DataProducerBase
{
  public:
	FEProducerVInterface(const std::string& interfaceUID, const ConfigurationTree& theXDAQContextConfigTree, const std::string& interfaceConfigurationPath);

	virtual ~FEProducerVInterface(void);

	virtual void startProcessingData(std::string /*runNumber*/) { __FE_COUT__ << "Do nothing. The FE Manager starts the workloop." << __E__; }
	virtual void stopProcessingData(void) { __FE_COUT__ << "Do nothing. The FE Manager stops the workloop." << __E__; }

	virtual void         copyToNextBuffer(const std::string& dataToWrite);
	virtual std::string* getNextBuffer(void);
	virtual void         writeCurrentBuffer(void);

  protected:
	std::string*                        dataP_;
	std::map<std::string, std::string>* headerP_;
};

}  // namespace ots

#endif
