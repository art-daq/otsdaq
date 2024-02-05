#ifndef _ots_VisualDataManager_h_
#define _ots_VisualDataManager_h_

#include "otsdaq/DataManager/DataManager.h"
// #include "otsdaq/MonicelliInterface/Visual3DEvent.h"
// #include "otsdaq/MonicelliInterface/Visual3DGeometry.h"
// #include "otsdaq/MonicelliInterface/MonicelliEventAnalyzer.h"
// #include "otsdaq/MonicelliInterface/MonicelliGeometryConverter.h"
#include "otsdaq/RootUtilities/DQMHistosBase.h"

#include <map>
#include <string>
#include <vector>

namespace ots
{
class ConfigurationManager;

class VisualDataManager : public DataManager
{
  public:
	VisualDataManager(const ConfigurationTree& theXDAQContextConfigTree, const std::string& supervisorConfigurationPath);
	virtual ~VisualDataManager(void);

	void configure(void) override;
	void halt(void) override;
	void pause(void) override;
	void resume(void) override;
	void start(std::string runNumber) override;
	void stop(void) override;

	void load(std::string fileName, std::string type);
	// Getters
	const std::vector<DQMHistosBase*>& getLiveDQMs(void) { return theLiveDQMs_; };

	void   setDoNotStop(bool doNotStop) { doNotStop_ = doNotStop; }
	bool   isReady(void) { return ready_; }
	TFile* openFile(std::string fileName);

	bool               getLiveDQMHistos(void);
	DQMHistosBase&     getFileDQMHistos(void);  // TO BE DELETED
	const std::string& getRawData(void);        // TO BE DELETED
	                                            // const Visual3DEvents&   getVisual3DEvents   (void);
	                                            // const Visual3DGeometry& getVisual3DGeometry (void);

  private:
	std::vector<DQMHistosBase*>   theLiveDQMs_;
	bool                          doNotStop_;
	bool                          ready_;
	std::map<std::string, TFile*> fileMap_;
	// MonicelliEventAnalyzer     theMonicelliEventAnalyzer_;
	// MonicelliGeometryConverter theMonicelliGeometryConverter_;
	// Visual3DData           the3DData_;

	bool          theLiveDQMHistos_;  // TO BE DELETED
	DQMHistosBase theFileDQMHistos_;  // TO BE DELETED
};
}  // namespace ots

#endif
