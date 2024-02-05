#ifndef _ots_TCPDataReceiverProducer_h_
#define _ots_TCPDataReceiverProducer_h_

#include "otsdaq/Configurable/Configurable.h"
#include "otsdaq/DataManager/DataProducer.h"
#include "otsdaq/NetworkUtilities/TCPSubscribeClient.h"  // Make sure this is always first because <sys/types.h> (defined in Socket.h) must be first

#include <string>

namespace ots
{
class ConfigurationTree;

class TCPDataReceiverProducer : public DataProducer, public Configurable, public TCPSubscribeClient
{
  public:
	TCPDataReceiverProducer(std::string              supervisorApplicationUID,
	                        std::string              bufferUID,
	                        std::string              processorUID,
	                        const ConfigurationTree& theXDAQContextConfigTree,
	                        const std::string&       configurationPath);
	virtual ~TCPDataReceiverProducer(void);
	virtual void startProcessingData(std::string runNumber) override;
	virtual void stopProcessingData(void) override;

  protected:
	bool workLoopThread(toolbox::task::WorkLoop* workLoop) override;
	void slowWrite(void);
	void fastWrite(void);
	// For slow write
	std::string                        data_;
	std::map<std::string, std::string> header_;
	// For fast write
	std::string*                        dataP_;
	std::map<std::string, std::string>* headerP_;

	std::string    ipAddress_;
	unsigned short port_;
	std::string    dataType_;

	// bool getNextFragment(void);
};

}  // namespace ots

#endif
