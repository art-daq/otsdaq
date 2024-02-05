#include "otsdaq/DataProcessorPlugins/TCPDataReceiverProducer.h"
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
TCPDataReceiverProducer::TCPDataReceiverProducer(std::string              supervisorApplicationUID,
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
    , dataType_(theXDAQContextConfigTree.getNode(configurationPath).getNode("DataType").getValue<std::string>())
{
}

//==============================================================================
TCPDataReceiverProducer::~TCPDataReceiverProducer(void) {}

//==============================================================================
void TCPDataReceiverProducer::startProcessingData(std::string runNumber)
{
	TCPSubscribeClient::connect(30, 1000);
	TCPSubscribeClient::setReceiveTimeout(1, 0);
	DataProducer::startProcessingData(runNumber);
}

//==============================================================================
void TCPDataReceiverProducer::stopProcessingData(void)
{
	DataProducer::stopProcessingData();
	TCPSubscribeClient::disconnect();
}

//==============================================================================
bool TCPDataReceiverProducer::workLoopThread(toolbox::task::WorkLoop* /*workLoop*/)
// bool TCPDataReceiverProducer::getNextFragment(void)
{
	// std::cout << __COUT_HDR_FL__ << __PRETTY_FUNCTION__ << DataProcessor::processorUID_
	// << " running, because workloop: " << WorkLoop::continueWorkLoop_ << std::endl;
	fastWrite();
	return WorkLoop::continueWorkLoop_;
}

//==============================================================================
void TCPDataReceiverProducer::slowWrite(void)
{
	// std::cout << __COUT_HDR_FL__ << __PRETTY_FUNCTION__ << name_ << " running!" <<
	// std::endl;

	try
	{
		if(dataType_ == "Packet")
			data_ = TCPSubscribeClient::receivePacket();         // Throws an exception if it fails
		else                                                     //"Raw" || DEFAULT
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
		std::this_thread::sleep_for(std::chrono::seconds(1));
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
void TCPDataReceiverProducer::fastWrite(void)
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
		if(dataType_ == "Packet")
			*dataP_ = TCPSubscribeClient::receivePacket();         // Throws an exception if it fails
		else                                                       //"Raw" || DEFAULT
			*dataP_ = TCPSubscribeClient::receive<std::string>();  // Throws an exception if it fails

		if(dataP_->size() == 0)  // When it goes in timeout
			return;
	}
	catch(const std::exception& e)
	{
		__COUT__ << "Error: " << e.what() << std::endl;
		std::this_thread::sleep_for(std::chrono::milliseconds(500));
		return;
	}
	(*headerP_)["IPAddress"] = ipAddress_;
	(*headerP_)["Port"]      = std::to_string(port_);

	DataProducer::setWrittenSubBuffer<std::string, std::map<std::string, std::string>>();
}

DEFINE_OTS_PROCESSOR(TCPDataReceiverProducer)
