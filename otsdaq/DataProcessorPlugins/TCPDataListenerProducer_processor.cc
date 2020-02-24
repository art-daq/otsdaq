#include "otsdaq/DataProcessorPlugins/TCPDataListenerProducer.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/ProcessorPluginMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"
#include "otsdaq/NetworkUtilities/NetworkConverters.h"

#include <string.h>
#include <cassert>
#include <chrono>
#include <iostream>
#include <thread>

using namespace ots;

//==============================================================================
TCPDataListenerProducer::TCPDataListenerProducer(std::string              supervisorApplicationUID,
                                                 std::string              bufferUID,
                                                 std::string              processorUID,
                                                 const ConfigurationTree& theXDAQContextConfigTree,
                                                 const std::string&       configurationPath)
    : WorkLoop(processorUID)
    //, Socket       ("192.168.133.100", 40000)
    , DataProducer(
          supervisorApplicationUID, bufferUID, processorUID, theXDAQContextConfigTree.getNode(configurationPath).getNode("BufferSize").getValue<unsigned int>())
    //, DataProducer (supervisorApplicationUID, bufferUID, processorUID, 100)
    , Configurable(theXDAQContextConfigTree, configurationPath)
    , TCPSubscribeClient(theXDAQContextConfigTree.getNode(configurationPath).getNode("ServerIPAddress").getValue<std::string>(),
                         theXDAQContextConfigTree.getNode(configurationPath).getNode("ServerPort").getValue<unsigned int>())
    , dataP_(nullptr)
    , headerP_(nullptr)
    , ipAddress_(theXDAQContextConfigTree.getNode(configurationPath).getNode("ServerIPAddress").getValue<std::string>())
    , port_(theXDAQContextConfigTree.getNode(configurationPath).getNode("ServerPort").getValue<unsigned int>())
{
}

//==============================================================================
TCPDataListenerProducer::~TCPDataListenerProducer(void) {}

//==============================================================================
void TCPDataListenerProducer::startProcessingData(std::string runNumber)
{
	TCPSubscribeClient::connect();
	TCPSubscribeClient::setReceiveTimeout(0, 1000);
	DataProducer::startProcessingData(runNumber);
}

//==============================================================================
void TCPDataListenerProducer::stopProcessingData(void)
{
	DataProducer::stopProcessingData();
	TCPSubscribeClient::disconnect();
}

//==============================================================================
bool TCPDataListenerProducer::workLoopThread(toolbox::task::WorkLoop* workLoop)
// bool TCPDataListenerProducer::getNextFragment(void)
{
	// std::cout << __COUT_HDR_FL__ << __PRETTY_FUNCTION__ << DataProcessor::processorUID_
	// << " running, because workloop: " << WorkLoop::continueWorkLoop_ << std::endl;
	fastWrite();
	return WorkLoop::continueWorkLoop_;
}

//==============================================================================
void TCPDataListenerProducer::slowWrite(void)
{
	// std::cout << __COUT_HDR_FL__ << __PRETTY_FUNCTION__ << name_ << " running!" <<
	// std::endl;

	try
	{
		data_ = TCPSubscribeClient::receive<std::string>();  // Throws an exception if it fails
		if(data_.size() == 0)
		{
			std::this_thread::sleep_for(std::chrono::microseconds(1000));
			return;
		}
	}
	catch(const std::exception& e)
	{
		__COUT__ << "Error: " << e.what() << std::endl;
		;
		return;
	}
	header_["IPAddress"] = ipAddress_;
	header_["Port"]      = std::to_string(port_);

	while(DataProducer::write(data_, header_) < 0)
	{
		__COUT__ << "There are no available buffers! Retrying...after waiting 10 "
		            "milliseconds!"
		         << std::endl;
		std::this_thread::sleep_for(std::chrono::microseconds(1000));
		return;
	}
}

//==============================================================================
void TCPDataListenerProducer::fastWrite(void)
{
	// std::cout << __COUT_HDR_FL__ << __PRETTY_FUNCTION__ << name_ << " running!" <<
	// std::endl;

	if(DataProducer::attachToEmptySubBuffer(dataP_, headerP_) < 0)
	{
		__COUT__ << "There are no available buffers! Retrying...after waiting 10 milliseconds!" << std::endl;
		std::this_thread::sleep_for(std::chrono::microseconds(1000));
		return;
	}

	try
	{
		*dataP_ = TCPSubscribeClient::receive<std::string>();  // Throws an exception if it fails
		if(dataP_->size() == 0)
			return;
	}
	catch(const std::exception& e)
	{
		__COUT__ << "Error: " << e.what() << std::endl;
		;
		return;
	}
	(*headerP_)["IPAddress"] = ipAddress_;
	(*headerP_)["Port"]      = std::to_string(port_);

	DataProducer::setWrittenSubBuffer<std::string, std::map<std::string, std::string>>();
}

DEFINE_OTS_PROCESSOR(TCPDataListenerProducer)