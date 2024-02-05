#include "otsdaq/DataProcessorPlugins/UDPDataStreamerConsumer.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/ProcessorPluginMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <unistd.h>
#include <cassert>
#include <iostream>

using namespace ots;

//==============================================================================
UDPDataStreamerConsumer::UDPDataStreamerConsumer(std::string              supervisorApplicationUID,
                                                 std::string              bufferUID,
                                                 std::string              processorUID,
                                                 const ConfigurationTree& theXDAQContextConfigTree,
                                                 const std::string&       configurationPath)
    : Socket(theXDAQContextConfigTree.getNode(configurationPath).getNode("HostIPAddress").getValue<std::string>(),
             theXDAQContextConfigTree.getNode(configurationPath).getNode("HostPort").getValue<unsigned int>())
    , WorkLoop(processorUID)
    , UDPDataStreamerBase(theXDAQContextConfigTree.getNode(configurationPath).getNode("HostIPAddress").getValue<std::string>(),
                          theXDAQContextConfigTree.getNode(configurationPath).getNode("HostPort").getValue<unsigned int>(),
                          theXDAQContextConfigTree.getNode(configurationPath).getNode("StreamToIPAddress").getValue<std::string>(),
                          theXDAQContextConfigTree.getNode(configurationPath).getNode("StreamToPort").getValue<unsigned int>())
    , DataConsumer(supervisorApplicationUID, bufferUID, processorUID, HighConsumerPriority)
    , Configurable(theXDAQContextConfigTree, configurationPath)
//, Socket         ("192.168.133.1", 47200)
//, DataConsumer   ("ARTDAQDataManager", 1, "ARTDAQBuffer", "ARTDAQDataStreamer0",
// HighConsumerPriority) , streamToSocket_("192.168.133.1", 50100)
{
	// Socket::initialize(); //dont call this! UDPDataStreamerBase() calls it
	__COUT__ << "done!" << std::endl;
}

//==============================================================================
UDPDataStreamerConsumer::~UDPDataStreamerConsumer(void) {}

//==============================================================================
bool UDPDataStreamerConsumer::workLoopThread(toolbox::task::WorkLoop* /*workLoop*/)
{
	fastRead();
	return WorkLoop::continueWorkLoop_;
}

//==============================================================================
void UDPDataStreamerConsumer::fastRead(void)
{
	//__COUT__ << processorUID_ << " running!" << std::endl;
	if(DataConsumer::read(dataP_, headerP_) < 0)
	{
		usleep(100);
		return;
	}
	// unsigned int reconverted = (((*headerP_)["IPAddress"][0]&0xff)<<24) +
	// (((*headerP_)["IPAddress"][1]&0xff)<<16) + (((*headerP_)["IPAddress"][2]&0xff)<<8)
	// +
	// ((*headerP_)["IPAddress"][3]&0xff);
	//__COUT__ << processorUID_ << " -> Got some data. From: "  << std::hex << reconverted
	//<< std::dec << std::endl;

	// std::cout << __COUT_HDR_FL__ << dataP_->length() << std::endl;
	TransmitterSocket::send(streamToSocket_, *dataP_);
	DataConsumer::setReadSubBuffer<std::string, std::map<std::string, std::string>>();
}

//==============================================================================
void UDPDataStreamerConsumer::slowRead(void)
{
	//__COUT__ << processorUID_ << " running!" << std::endl;
	// This is making a copy!!!
	if(DataConsumer::read(data_, header_) < 0)
	{
		usleep(1000);
		return;
	}
	// unsigned int reconverted = ((header_["IPAddress"][0]&0xff)<<24) +
	// ((header_["IPAddress"][1]&0xff)<<16) + ((header_["IPAddress"][2]&0xff)<<8) +
	// (header_["IPAddress"][3]&0xff);
	//__COUT__ << processorUID_ << " -> Got some data. From: "  << std::hex << reconverted
	//<< std::dec << std::endl;

	TransmitterSocket::send(streamToSocket_, data_);
}

DEFINE_OTS_PROCESSOR(UDPDataStreamerConsumer)
