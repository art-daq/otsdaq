#ifndef _ots_ARTDAQReaderProcessorBase_h_
#define _ots_ARTDAQReaderProcessorBase_h_

#include "artdaq/Application/BoardReaderApp.hh"
#include "otsdaq/Configurable/Configurable.h"

#include <future>
#include <memory>
#include <string>

namespace ots
{
// ARTDAQReaderProcessorBase
//	This class is a Data Processor base class for the Consumer and Producer Plugin that
//	allows a single artdaq Board Reader to be
// instantiated on an otsdaq Buffer.
class ARTDAQReaderProcessorBase : public Configurable
{
  public:
	ARTDAQReaderProcessorBase(std::string              supervisorApplicationUID,
	                          std::string              bufferUID,
	                          std::string              processorUID,
	                          const ConfigurationTree& theXDAQContextConfigTree,
	                          const std::string&       configurationPath);
	virtual ~ARTDAQReaderProcessorBase(void);

	// void destroy          (void);
	// void configure         (fhicl::ParameterSet const& pset);

	// these functions are not inherited, they define the core reader functionality
	void initLocalGroup(int rank);
	void configure(int rank);
	void halt(void);
	void pause(void);
	void resume(void);
	void start(const std::string& runNumber);
	void stop(void);

	//	void pauseProcessingData(void);
	//	void resumeProcessingData(void);
	//	void startProcessingData(std::string runNumber) override;
	//	void stopProcessingData(void);

	// int universalRead	  (char *address, char *returnValue) override {;}
	// void universalWrite	  (char *address, char *writeValue) override {;}
	// artdaq::BoardReaderCore* getFragmentReceiverPtr(){return
	// fragment_receiver_ptr_.get();}  void ProcessData_(artdaq::FragmentPtrs & frags)
	// override;

  private:
	// bool workLoopThread(toolbox::task::WorkLoop* workLoop) { return false; }

	std::unique_ptr<artdaq::BoardReaderApp> fragment_receiver_ptr_;
	const std::string                       name_;
	fhicl::ParameterSet                     fhiclConfiguration_;
};

}  // namespace ots

#endif
