#include "otsdaq/DataProcessorPlugins/UDPDataListenerProducer.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/Macros/ProcessorPluginMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"
#include "otsdaq/NetworkUtilities/NetworkConverters.h"

#include <string.h>
#include <unistd.h>
#include <cassert>
#include <iostream>

using namespace ots;

//==============================================================================
UDPDataListenerProducer::UDPDataListenerProducer(std::string              supervisorApplicationUID,
                                                 std::string              bufferUID,
                                                 std::string              processorUID,
                                                 const ConfigurationTree& theXDAQContextConfigTree,
                                                 const std::string&       configurationPath)
    : WorkLoop(processorUID)
    , Socket(theXDAQContextConfigTree.getNode(configurationPath).getNode("HostIPAddress").getValue<std::string>(),
             theXDAQContextConfigTree.getNode(configurationPath).getNode("HostPort").getValue<unsigned int>())
    //, Socket       ("192.168.133.100", 40000)
    , DataProducer(
          supervisorApplicationUID, bufferUID, processorUID, theXDAQContextConfigTree.getNode(configurationPath).getNode("BufferSize").getValue<unsigned int>())
    //, DataProducer (supervisorApplicationUID, bufferUID, processorUID, 100)
    , Configurable(theXDAQContextConfigTree, configurationPath)
    , dataP_(nullptr)
    , headerP_(nullptr)
{
	unsigned int socketReceiveBufferSize;
	try  // if socketReceiveBufferSize is defined in configuration, use it
	{
		socketReceiveBufferSize = theXDAQContextConfigTree.getNode(configurationPath).getNode("SocketReceiveBufferSize").getValue<unsigned int>();
	}
	catch(...)
	{
		// for backwards compatibility, ignore
		socketReceiveBufferSize = 0x10000000;  // default to "large"
	}

	Socket::initialize(socketReceiveBufferSize);
}

//==============================================================================
UDPDataListenerProducer::~UDPDataListenerProducer(void) {}

//==============================================================================
bool UDPDataListenerProducer::workLoopThread(toolbox::task::WorkLoop* /*workLoop*/)
// bool UDPDataListenerProducer::getNextFragment(void)
{
	//__CFG_COUT__DataProcessor::processorUID_ << " running, because workloop: " <<
	// WorkLoop::continueWorkLoop_ << std::endl;
	fastWrite();
	return WorkLoop::continueWorkLoop_;
}

//==============================================================================
void UDPDataListenerProducer::slowWrite(void)
{
	//__CFG_COUT__name_ << " running!" << std::endl;

	if(ReceiverSocket::receive(data_, ipAddress_, port_) != -1)
	{
		//__CFG_COUT__name_ << " Buffer: " << message << std::endl;
		//__CFG_COUT__processorUID_ << " -> Got some data. From: " << std::hex <<
		// fromIPAddress << " port: " << fromPort << std::dec << std::endl;
		header_["IPAddress"] = NetworkConverters::networkToStringIP(ipAddress_);
		header_["Port"]      = NetworkConverters::networkToStringPort(port_);
		//        unsigned long long value;
		//        memcpy((void *)&value, (void *) data_.substr(2).data(),8); //make data
		//        counter
		//        __CFG_COUT__std::hex << value << std::dec << std::endl;

		while(DataProducer::write(data_, header_) < 0)
		{
			__CFG_COUT__ << "There are no available buffers! Retrying...after waiting 10 "
			                "milliseconds!"
			             << std::endl;
			usleep(10000);
			return;
		}
	}
}

//==============================================================================
void UDPDataListenerProducer::fastWrite(void)
{
	//__CFG_COUT__ << " running!" << std::endl;

	if(DataProducer::attachToEmptySubBuffer(dataP_, headerP_) < 0)
	{
		__CFG_COUT__ << "There are no available buffers! Retrying...after waiting 10 milliseconds!" << std::endl;
		usleep(10000);
		return;
	}

	//	if(0)  // test data buffers
	//	{
	//		sleep(1);
	//		unsigned long long value  = 0xA54321;  // this is 8-bytes
	//		std::string&       buffer = *dataP_;
	//		buffer.resize(
	//		    8);  // NOTE: this is inexpensive according to Lorenzo/documentation
	//		         // in C++11 (only increases size once and doesn't decrease size)
	//		memcpy((void*)&buffer[0] /*dest*/, (void*)&value /*src*/, 8 /*numOfBytes*/);
	//
	//		// size() and length() are equivalent
	//		__CFG_COUT__ << "Writing to buffer " << buffer.size() << " bytes!" << __E__;
	//		__CFG_COUT__ << "Writing to buffer length " << buffer.length() << " bytes!"
	//		             << __E__;
	//
	//		__CFG_COUT__ << "Buffer Data: "
	//		             << BinaryStringMacros::binaryNumberToHexString(buffer) << __E__;
	//
	//		__CFG_COUTV__(DataProcessor::theCircularBuffer_);
	//
	//		DataProducer::setWrittenSubBuffer<std::string,
	//		                                  std::map<std::string, std::string> >();
	//		return;
	//	}

	if(ReceiverSocket::receive(*dataP_, ipAddress_, port_, 1, 0, true) != -1)
	{
		header_["IPAddress"] = NetworkConverters::networkToStringIP(ipAddress_);
		header_["Port"]      = NetworkConverters::networkToStringPort(port_);
		__CFG_COUT__ << "Received data IP: " << header_["IPAddress"] << " port: " << header_["Port"] << __E__;
		DataProducer::setWrittenSubBuffer<std::string, std::map<std::string, std::string> >();
	}
}

DEFINE_OTS_PROCESSOR(UDPDataListenerProducer)
