#ifndef _ots_DQMHistosBase_h_
#define _ots_DQMHistosBase_h_

#include <map>
#include <string>

class TFile;
class TDirectory;
class TObject;

namespace ots
{
class VisualDataManager;
class DQMHistosBase
{
  public:
	DQMHistosBase(void);
	virtual ~DQMHistosBase(void);

	virtual void book(void) { ; }
	virtual void fill(std::string& /*buffer*/, std::map<std::string, std::string> /*header*/) { ; }
	virtual void load(std::string /*fileName*/) { ; }

	void setDataManager(VisualDataManager* dataManager) { theDataManager_ = dataManager; }

	TObject* get(std::string name);
	TFile*   getFile(void) { return theFile_; }

  protected:
	void         setAutoSave(bool autoSave) { autoSave_ = autoSave; }  // Default is true
	bool         isFileOpen(void);
	virtual void save(void);
	virtual void openFile(std::string fileName);
	virtual void closeFile(void);
	virtual void autoSave(bool force = false);  // The file will be saved if force == true or currentTime - beginTimeTime_ is >= autoSaveInterval_
	virtual void setAutoSaveInterval(unsigned int interval) { autoSaveInterval_ = interval; }  // Default is in the protected variables = 300

	TFile*      theFile_          = nullptr;
	TDirectory* myDirectory_      = nullptr;
	bool        autoSave_         = true;
	bool        autoSaveInterval_ = 300;
	time_t      beginTime_        = 0;

  private:
	VisualDataManager* theDataManager_;
};
}  // namespace ots

#endif
