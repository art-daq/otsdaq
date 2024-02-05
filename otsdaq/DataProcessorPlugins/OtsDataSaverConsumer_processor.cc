#include "otsdaq/DataProcessorPlugins/OtsDataSaverConsumer.h"
#include "otsdaq/Macros/ProcessorPluginMacros.h"

using namespace ots;

//==============================================================================
OtsDataSaverConsumer::OtsDataSaverConsumer(std::string              supervisorApplicationUID,
                                           std::string              bufferUID,
                                           std::string              processorUID,
                                           const ConfigurationTree& theXDAQContextConfigTree,
                                           const std::string&       configurationPath)
    : WorkLoop(processorUID), RawDataSaverConsumerBase(supervisorApplicationUID, bufferUID, processorUID, theXDAQContextConfigTree, configurationPath)
{
}

//==============================================================================
OtsDataSaverConsumer::~OtsDataSaverConsumer(void) {}

//==============================================================================
void OtsDataSaverConsumer::writeHeader(void) {}

//==============================================================================
// add one byte quad-word count before each packet
void OtsDataSaverConsumer::writePacketHeader(const std::string& data)
{
	unsigned char quadWordsCount = (data.length() - 2) / 8;
	outFile_.write((char*)&quadWordsCount, 1);

	// packetTypes is data[0]
	// seqId is in data[1] position

	if(quadWordsCount)
	{
		unsigned char seqId = data[1];
		__CFG_COUT__ << "quadcount: " << quadWordsCount << " packetType: " << data[0] << " sequence id: " << seqId << " last: " << lastSeqId_ << __E__;
		if(!(lastSeqId_ + 1 == seqId || (lastSeqId_ == 255 && seqId == 0)))
		{
			__CFG_COUT__ << "?????? NOOOO Missing Packets: " << (unsigned int)lastSeqId_ << " v " << (unsigned int)seqId << __E__;
		}
		lastSeqId_ = seqId;
	}
}

DEFINE_OTS_PROCESSOR(OtsDataSaverConsumer)
