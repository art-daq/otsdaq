#include "otsdaq/DataManager/RawDataSaverConsumerBase.h"
#include "otsdaq/Macros/BinaryStringMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

#include <unistd.h>
#include <cassert>
#include <iostream>
// #include <string.h> //memcpy
#include <fstream>
#include <sstream>

using namespace ots;

//==============================================================================
RawDataSaverConsumerBase::RawDataSaverConsumerBase(std::string              supervisorApplicationUID,
                                                   std::string              bufferUID,
                                                   std::string              processorUID,
                                                   const ConfigurationTree& theXDAQContextConfigTree,
                                                   const std::string&       configurationPath)
    : WorkLoop(processorUID)
    , DataConsumer(supervisorApplicationUID, bufferUID, processorUID, HighConsumerPriority)
    , Configurable(theXDAQContextConfigTree, configurationPath)
    , filePath_(theXDAQContextConfigTree.getNode(configurationPath).getNode("FilePath").getValue<std::string>())
    , fileRadix_(theXDAQContextConfigTree.getNode(configurationPath).getNode("RadixFileName").getValue<std::string>())
    , maxFileSize_(theXDAQContextConfigTree.getNode(configurationPath).getNode("MaxFileSize").getValue<long>() * 1000000)  // Instead of 2^6=1048576
    , currentSubRunNumber_(0)

{
	// FILE *fp = fopen( "/home/otsdaq/tsave.txt","w");
	// if(fp)fclose(fp);
}

//==============================================================================
RawDataSaverConsumerBase::~RawDataSaverConsumerBase(void) {}

//==============================================================================
void RawDataSaverConsumerBase::startProcessingData(std::string runNumber)
{
	if(runNumber != "")  // If there is no number it means it was paused
	{
		currentSubRunNumber_ = 0;
		openFile(runNumber);
	}
	DataConsumer::startProcessingData(runNumber);
}

//==============================================================================
void RawDataSaverConsumerBase::stopProcessingData(void)
{
	DataConsumer::stopProcessingData();
	closeFile();
}

//==============================================================================
void RawDataSaverConsumerBase::openFile(std::string runNumber)
{
	currentRunNumber_ = runNumber;
	//	std::string fileName =   "Run" + runNumber + "_" + processorUID_ + "_Raw.dat";
	std::stringstream fileName;
	fileName << filePath_ << "/" << fileRadix_ << "_Run" << runNumber;
	// if split file is there then subrunnumber must be set!
	if(maxFileSize_ > 0)
		fileName << "_" << currentSubRunNumber_;
	fileName << "_Raw.dat";
	__CFG_COUT__ << "Saving file: " << fileName.str() << std::endl;
	outFile_.open(fileName.str().c_str(), std::ios::out | std::ios::binary);
	if(!outFile_.is_open())
	{
		__CFG_SS__ << "Can't open file " << fileName.str() << std::endl;
		__CFG_SS_THROW__;
	}

	writeHeader();  // write start of file header
}

//==============================================================================
void RawDataSaverConsumerBase::closeFile(void)
{
	if(outFile_.is_open())
	{
		writeFooter();  // write end of file footer
		outFile_.close();
	}
}

//==============================================================================
void RawDataSaverConsumerBase::save(const std::string& data)
{
	std::ofstream output;

	//	std::string outputPath = "/home/otsdaq/tsave.txt";
	//	if(0)
	//	{
	//		output.open(outputPath, std::ios::out | std::ios::app | std::ios::binary);
	//		output << data;
	//	}
	//	else
	//	{
	//		output.open(outputPath, std::ios::out | std::ios::app);
	//		output << data;
	//
	//		char str[5];
	//		for(unsigned int j=0;j<data.length();++j)
	//		{
	//			sprintf(str,"%2.2x",((unsigned int)data[j]) & ((unsigned int)(0x0FF)));
	//
	//			if(j%64 == 0) std::cout << "SAVE " << j << "\t: 0x\t";
	//			std::cout << str;
	//			if(j%8 == 7) std::cout << " ";
	//			if(j%64 == 63) std::cout << std::endl;
	//		}
	//		std::cout << std::endl;
	//		std::cout << std::endl;
	//	}
	//
	//	if(1)
	//	{
	//		char str[5];
	//		for(unsigned int j=0;j<data.length();++j)
	//		{
	//			sprintf(str,"%2.2x",((unsigned int)data[j]) & ((unsigned int)(0x0FF)));
	//
	//			if(j%64 == 0) std::cout << "SAVE " << j << "\t: 0x\t";
	//			std::cout << str;
	//			if(j%8 == 7) std::cout << " ";
	//			if(j%64 == 63) std::cout << std::endl;
	//		}
	//		std::cout << std::endl;
	//		std::cout << std::endl;
	//	}

	if(maxFileSize_ > 0)
	{
		long length = outFile_.tellp();
		if(length >= maxFileSize_ / 1000)
		{
			closeFile();
			++currentSubRunNumber_;
			openFile(currentRunNumber_);
		}
	}

	writePacketHeader(data);  // write start of packet header
	outFile_.write((char*)&data[0], data.length());
	writePacketFooter(data);  // write start of packet footer
}

//==============================================================================
bool RawDataSaverConsumerBase::workLoopThread(toolbox::task::WorkLoop* /*workLoop*/)
{
	//__CFG_COUT__ << DataProcessor::processorUID_ << " running, because workloop: "
	//<< WorkLoop::continueWorkLoop_ << std::endl;
	fastRead();
	return WorkLoop::continueWorkLoop_;
}

//==============================================================================
void RawDataSaverConsumerBase::fastRead(void)
{
	//__CFG_COUT__ << processorUID_ << " running!" << std::endl;
	if(DataConsumer::read(dataP_, headerP_) < 0)
	{
		usleep(100);
		return;
	}
	//__CFG_COUTV__(dataP_->length());
	// std::string& buffer = *dataP_;

	//__CFG_COUT__ << "Reading from buffer length " << buffer.length() << " bytes!" <<
	//__E__;
	//__CFG_COUT__ << "Buffer Data: " <<
	// BinaryStringMacros::binaryNumberToHexString(buffer) << __E__;

	save(*dataP_);
	DataConsumer::setReadSubBuffer<std::string, std::map<std::string, std::string> >();
}

//==============================================================================
void RawDataSaverConsumerBase::slowRead(void)
{
	//__CFG_COUT__ << processorUID_ << " running!" << std::endl;
	// This is making a copy!!!
	if(DataConsumer::read(data_, header_) < 0)
	{
		usleep(1000);
		return;
	}
	save(data_);
}
