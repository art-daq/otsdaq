#ifndef _ots_ARTDAQProducer_h_
#define _ots_ARTDAQProducer_h_

#include "otsdaq/ARTDAQReaderCore/ARTDAQReaderProcessorBase.h"
//
// #include "artdaq/Application/BoardReaderApp.hh"
// #include "otsdaq/Configurable/Configurable.h"
#include "otsdaq/DataManager/DataProducer.h"
//
// #include <future>
// #include <memory>
// #include <string>

namespace ots
{
// ARTDAQProducer
//	This class is a Data Producer plugin that
//	allows a single artdaq Board Reader to be
// instantiated on the write side of an otsdaq Buffer.
class ARTDAQProducer : public DataProducer,
                       public ARTDAQReaderProcessorBase  // public DataProducer, public Configurable
{
  public:
	ARTDAQProducer(std::string              supervisorApplicationUID,
	               std::string              bufferUID,
	               std::string              processorUID,
	               const ConfigurationTree& theXDAQContextConfigTree,
	               const std::string&       configurationPath);
	virtual ~ARTDAQProducer(void);

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
