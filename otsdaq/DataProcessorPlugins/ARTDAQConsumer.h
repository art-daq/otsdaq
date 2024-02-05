#ifndef _ots_ARTDAQConsumer_h_
#define _ots_ARTDAQConsumer_h_

#include "otsdaq/ARTDAQReaderCore/ARTDAQReaderProcessorBase.h"

// #include "artdaq/Application/BoardReaderApp.hh"
// #include "otsdaq/Configurable/Configurable.h"
#include "otsdaq/DataManager/DataConsumer.h"

// #include <future>
// #include <memory>
// #include <string>

namespace ots
{
// ARTDAQConsumer
//	This class is a Data Consumer plugin that
//	allows a single artdaq Board Reader to be
// instantiated on the read side of an otsdaq Buffer.
class ARTDAQConsumer : public DataConsumer,
                       public ARTDAQReaderProcessorBase  // public DataConsumer, public Configurable
{
  public:
	ARTDAQConsumer(std::string              supervisorApplicationUID,
	               std::string              bufferUID,
	               std::string              processorUID,
	               const ConfigurationTree& theXDAQContextConfigTree,
	               const std::string&       configurationPath);
	virtual ~ARTDAQConsumer(void);

	//	void initLocalGroup(int rank);
	//	// void destroy          (void);
	//
	//	// void configure         (fhicl::ParameterSet const& pset);
	//	void configure(int rank);
	//	void halt(void);
	void pauseProcessingData(void) override;
	void resumeProcessingData(void) override;
	void startProcessingData(std::string runNumber) override;
	void stopProcessingData(void) override;
	//	// int universalRead	  (char *address, char *returnValue) override {;}
	//	// void universalWrite	  (char *address, char *writeValue) override {;}
	//	// artdaq::BoardReaderCore* getFragmentReceiverPtr(){return
	//	// fragment_receiver_ptr_.get();}  void ProcessData_(artdaq::FragmentPtrs & frags)
	//	// override;
	//
  private:
	bool workLoopThread(toolbox::task::WorkLoop* /*workLoop*/) override { return false; }
	//
	//	std::unique_ptr<artdaq::BoardReaderApp> fragment_receiver_ptr_;
	//	std::string                             name_;
	//
	//	// FIXME These should go...
	//	std::string         report_string_;
	//	bool                external_request_status_;
	//	fhicl::ParameterSet fhiclConfiguration_;
};

}  // namespace ots

#endif
