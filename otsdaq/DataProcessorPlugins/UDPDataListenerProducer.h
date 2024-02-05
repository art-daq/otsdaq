#ifndef _ots_UDPDataListenerProducer_h_
#define _ots_UDPDataListenerProducer_h_

#include "otsdaq/Configurable/Configurable.h"
#include "otsdaq/DataManager/DataProducer.h"
#include "otsdaq/NetworkUtilities/ReceiverSocket.h"  // Make sure this is always first because <sys/types.h> (defined in Socket.h) must be first

#include <string>

namespace ots
{
class ConfigurationTree;

class UDPDataListenerProducer : public DataProducer, public Configurable, public ReceiverSocket
{
  public:
	UDPDataListenerProducer(std::string              supervisorApplicationUID,
	                        std::string              bufferUID,
	                        std::string              processorUID,
	                        const ConfigurationTree& theXDAQContextConfigTree,
	                        const std::string&       configurationPath);
	virtual ~UDPDataListenerProducer(void);

  protected:
	bool workLoopThread(toolbox::task::WorkLoop* workLoop);
	void slowWrite(void);
	void fastWrite(void);
	// For slow write
	std::string                        data_;
	std::map<std::string, std::string> header_;
	// For fast write
	std::string*                        dataP_;
	std::map<std::string, std::string>* headerP_;

	unsigned long  ipAddress_;
	unsigned short port_;

	unsigned char lastSeqId_;
	// bool getNextFragment(void);
};

}  // namespace ots

#endif
