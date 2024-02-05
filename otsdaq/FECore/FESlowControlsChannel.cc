#include "otsdaq/FECore/FESlowControlsChannel.h"
#include "otsdaq/Macros/BinaryStringMacros.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/FECore/FEVInterface.h"

#include <iostream>
#include <sstream>
#include <stdexcept> /*runtime_error*/

using namespace ots;

#undef __MF_SUBJECT__
#define __MF_SUBJECT__ "SlowControls"
#define mfSubject_ (interface_->getInterfaceUID() + "-" + channelName_)

////////////////////////////////////
// Packet Types sent in txBuffer:
//
//			create value packet:
//			  1B type (0: value, 1: loloalarm, 2: loalarm, 3: hioalarm, 4: hihialarm)
//				1B sequence count from channel
//				8B time
//				4B sz of name
//				name
//				1B sz of value in bytes
//				1B sz of value in bits
//				value or alarm threshold value
//
////////////////////////////////////

//==============================================================================
FESlowControlsChannel::FESlowControlsChannel(FEVInterface* interface,
                                             const std::string& channelName,
                                             const std::string& dataType,
                                             const std::string& universalAddress,
                                             unsigned int       universalDataBitOffset,
                                             bool               readAccess,
                                             bool               writeAccess,
                                             bool               monitoringEnabled,
                                             bool               recordChangesOnly,
                                             time_t             delayBetweenSamples,
                                             bool               saveEnabled,
                                             const std::string& savePath,
                                             const std::string& saveFileRadix,
                                             bool               saveBinaryFormat,
                                             bool               alarmsEnabled,
                                             bool               latchAlarms,
                                             const std::string& lolo,
                                             const std::string& lo,
                                             const std::string& hi,
                                             const std::string& hihi)
    : interface_(interface)
    , channelName_(channelName)
    //, fullChannelName_(interface->getInterfaceUID() + "/" + channelName_)
    , fullChannelName_(interface->getInterfaceUID() + ":" + channelName_)
    , dataType_(dataType)
    , universalDataBitOffset_(universalDataBitOffset)
    , txPacketSequenceNumber_(0)
    , readAccess_(readAccess)
    , writeAccess_(writeAccess)
    , monitoringEnabled_(monitoringEnabled)
    , recordChangesOnly_(recordChangesOnly)
    , delayBetweenSamples_(delayBetweenSamples < 1 ? 1 : delayBetweenSamples)  // units of seconds, with 1 minimum
    , saveEnabled_(saveEnabled)
    , savePath_(savePath)
    , saveFileRadix_(saveFileRadix)
    , saveBinaryFormat_(saveBinaryFormat)
    , alarmsEnabled_(alarmsEnabled)
    , latchAlarms_(latchAlarms)
    , lastSampleTime_(0)
    , loloAlarmed_(false)
    , loAlarmed_(false)
    , hiAlarmed_(false)
    , hihiAlarmed_(false)
    , saveFullFileName_(savePath_ + "/" + saveFileRadix_ + "-" + underscoreString(fullChannelName_) + "-" + std::to_string(time(0)) +
                        (saveBinaryFormat_ ? ".dat" : ".txt"))
{
	__GEN_COUTV__(dataType_);
	__GEN_COUTV__(interface->getUniversalAddressSize());
	__GEN_COUTV__(universalAddress);

	if(interface->getUniversalAddressSize() == 0 || 
		interface->getUniversalDataSize() == 0)
	{
		__GEN_SS__ << "The front-end interface must have a non-zero universal address and data size. Current address size = " 
			<< interface->getUniversalAddressSize() << ", data size = " << interface->getUniversalDataSize() << __E__;
		__GEN_SS_THROW__;
	}

	sizeOfReadBytes_ = 0;

	// check for valid types:
	//	if(dataType_ != "char" &&
	//				dataType_ != "short" &&
	//				dataType_ != "int" &&
	//				dataType_ != "unsigned int" &&
	//				dataType_ != "long long " &&
	//				dataType_ != "unsigned long long" &&
	//				dataType_ != "float" &&
	//				dataType_ != "double")
	//		{
	if(dataType_.size() > 1 && dataType_[dataType_.size() - 1] == 'b')  // if ends in 'b' then take that many bits
	{
		//if dataType_ leads with xB then yb, the use x as the number of read bytes (e.g. for a block read) 

		//search for 'B'
		for(unsigned int i = 0; i<dataType_.size()-1; ++i)
			if(dataType_[i] == 'B') //then treat as number of read bytes
			{
				sscanf(&dataType_[0], "%uB", &sizeOfReadBytes_);
				++i; //to get past 'B'
				sscanf(&dataType_[i], "%u", &sizeOfDataTypeBits_);
				break;
			}
			else if(i == dataType_.size()-2) //else no 'B'
				sscanf(&dataType_[0], "%u", &sizeOfDataTypeBits_);
	}
	else if(dataType_ == "char" || dataType_ == "unsigned char")
		sizeOfDataTypeBits_ = sizeof(char) * 8;
	else if(dataType_ == "short" || dataType_ == "unsigned short")
		sizeOfDataTypeBits_ = sizeof(short) * 8;
	else if(dataType_ == "int" || dataType_ == "unsigned int")
		sizeOfDataTypeBits_ = sizeof(int) * 8;
	else if(dataType_ == "long long" || dataType_ == "unsigned long long")
		sizeOfDataTypeBits_ = sizeof(long long) * 8;
	else if(dataType_ == "float")
		sizeOfDataTypeBits_ = sizeof(float) * 8;
	else if(dataType_ == "double")
		sizeOfDataTypeBits_ = sizeof(double) * 8;
	else
	{
		__GEN_SS__ << "ChannelDataType '" << dataType_ << "' is invalid. "
		           << "Valid data types (w/size in bytes) are as follows: "
		           << "#b (# bits)"
		           << ", char (" << sizeof(char) << "B), unsigned char (" << sizeof(unsigned char) << "B), short (" << sizeof(short) << "B), unsigned short ("
		           << sizeof(unsigned short) << "B), int (" << sizeof(int) << "B), unsigned int (" << sizeof(unsigned int) << "B), long long ("
		           << sizeof(long long) << "B), unsigned long long (" << sizeof(unsigned long long) << "B), float (" << sizeof(float) << "B), double ("
		           << sizeof(double) << "B)." << __E__;
		__GEN_COUT_ERR__ << "\n" << ss.str();
		__GEN_SS_THROW__;
	}

	//calculate number of bytes to read
	if(!sizeOfReadBytes_)
		sizeOfReadBytes_ = (
			(universalDataBitOffset_ + sizeOfDataTypeBits_) / 8 + 
			(((universalDataBitOffset_ + sizeOfDataTypeBits_) % 8) ? 1 : 0));
	sizeOfDataTypeBytes_ = (
		(sizeOfDataTypeBits_) / 8 + 
		(((sizeOfDataTypeBits_) % 8) ? 1 : 0));


	universalAddress_.resize(interface->getUniversalAddressSize());
	try
	{
		convertStringToBuffer(universalAddress, universalAddress_);
		__GEN_COUTV__(BinaryStringMacros::binaryNumberToHexString(universalAddress_, "0x", " "));
	}
	catch(const std::runtime_error& e)
	{
		__GEN_SS__ << "Failed to extract universalAddress '" << universalAddress << "'..." << __E__;
		ss << e.what();
		__GEN_SS_THROW__;
	}

	__GEN_COUTV__(sizeOfReadBytes_);
	__GEN_COUTV__(interface->getUniversalDataSize());
	if(sizeOfReadBytes_ > interface->getUniversalDataSize())
	{
		//check if FE supports Block Reads by using a test read (because the compiler does not allow this)
		// if(interface->*(&FEVInterface::universalBlockRead) != (&FEVInterface::universalBlockRead))
		// {
		// 	__GEN_COUT__ << "This FE interface does implement block reads." << __E__;
		// }		
		try //check if FE supports Block Reads by using a test read
		{
			std::string readValue;
			readValue.resize(sizeOfReadBytes_);
			interface->universalBlockRead(&universalAddress_[0], &readValue[0],sizeOfReadBytes_);
		}
		catch(const std::runtime_error& e)
		{
			__GEN_COUTV__(StringMacros::demangleTypeName(typeid(*interface).name()));
			if(strcmp(e.what(),"UNDEFINED BLOCK READ") == 0)
			{
				__GEN_SS__ << "Invalid Data Type '" << dataType_ << "' (offset:" <<
					universalDataBitOffset_ << " + " << sizeOfDataTypeBits_
		           << "-bits) = " << sizeOfReadBytes_ <<
		              "-bytes. Data Type size must be less than or equal to Universal Data Size = " << 
					  interface->getUniversalDataSize() << "-bytes. (Or the FEInterface must implement the virtual function universalBlockRead() for larger read sizes)" << __E__;
				__GEN_COUT_ERR__ << "\n" << ss.str();
				__GEN_SS_THROW__;
			}
			// else ignore  error for test read (assume things are not setup yet)
		}
		__GEN_COUT__ << "Block read was found to be implemented!" << __E__;
	}

	
	lolo_.resize(sizeOfDataTypeBytes_);
	lo_.resize(sizeOfDataTypeBytes_);
	hi_.resize(sizeOfDataTypeBytes_);
	hihi_.resize(sizeOfDataTypeBytes_);

	if(alarmsEnabled_)
	{
		try
		{
			convertStringToBuffer(lolo, lolo_, true);
		}
		catch(const std::runtime_error& e)
		{
			__GEN_SS__ << "Failed to extract lolo '" << lolo << "'..." << __E__;
			ss << e.what();
			__GEN_SS_THROW__;
		}
		try
		{
			convertStringToBuffer(lo, lo_, true);
		}
		catch(const std::runtime_error& e)
		{
			__GEN_SS__ << "Failed to extract lo '" << lo << "'..." << __E__;
			ss << e.what();
			__GEN_SS_THROW__;
		}
		try
		{
			convertStringToBuffer(hi, hi_, true);
		}
		catch(const std::runtime_error& e)
		{
			__GEN_SS__ << "Failed to extract hi '" << hi << "'..." << __E__;
			ss << e.what();
			__GEN_SS_THROW__;
		}
		try
		{
			convertStringToBuffer(hihi, hihi_, true);
		}
		catch(const std::runtime_error& e)
		{
			__GEN_SS__ << "Failed to extract hihi '" << hihi << "'..." << __E__;
			ss << e.what();
			__GEN_SS_THROW__;
		}
	}

	// prepare for data to come
	sample_.resize(sizeOfDataTypeBytes_);
	lastSample_.resize(sizeOfDataTypeBytes_);

	__GEN_COUT__;
	print();
	__GEN_COUT__ << "Constructed." << __E__;
}  // end constructor

//==============================================================================
FESlowControlsChannel::~FESlowControlsChannel(void) {}


//==============================================================================
// virtual in case read should be different than universalread
void FESlowControlsChannel::getSample(std::string& readValue)
{
	if(getReadSizeBytes() > interface_->getUniversalDataSize())
	{
		//block read!
		readValue.resize(getReadSizeBytes());
		interface_->universalBlockRead(&universalAddress_[0], &readValue[0], getReadSizeBytes());
	}
	else //normal read
	{
		readValue.resize(interface_->getUniversalDataSize());
		interface_->universalRead(&universalAddress_[0], &readValue[0]);
	}
}  // end getSample()

//==============================================================================
void FESlowControlsChannel::print(std::ostream& out) const
{
	out << "Slow Controls Channel '" << mfSubject_ << "'" << __E__;

	out << "\t"
	    << "dataType_: " << dataType_ << __E__;
	out << "\t"
	    << "sizeOfDataTypeBits_: " << sizeOfDataTypeBits_ << __E__;
	out << "\t"
	    << "sizeOfDataTypeBytes_: " << sizeOfDataTypeBytes_ << __E__;
	out << "\t"
	    << "universalDataBitOffset_: " << universalDataBitOffset_ << __E__;
	out << "\t"
	    << "sizeOfReadBytes_: " << sizeOfReadBytes_ << __E__;
	out << "\t"
	    << "universalAddress_: " << BinaryStringMacros::binaryNumberToHexString(universalAddress_, "0x", " ") << __E__;	
	out << "\t"
	    << "readAccess_: " << readAccess_ << __E__;
	out << "\t"
	    << "writeAccess_: " << writeAccess_ << __E__;
	out << "\t"
	    << "monitoringEnabled_: " << monitoringEnabled_ << __E__;
	out << "\t"
	    << "recordChangesOnly_: " << recordChangesOnly_ << __E__;
	out << "\t"
	    << "delayBetweenSamples_: " << delayBetweenSamples_ << __E__;
	out << "\t"
	    << "saveEnabled_: " << saveEnabled_ << __E__;
	out << "\t"
	    << "savePath_: " << savePath_ << __E__;
	out << "\t"
	    << "saveFileRadix_: " << saveFileRadix_ << __E__;
	out << "\t"
	    << "saveBinaryFormat_: " << saveBinaryFormat_ << __E__;
	out << "\t"
	    << "alarmsEnabled_: " << alarmsEnabled_ << __E__;
	out << "\t"
	    << "latchAlarms_: " << latchAlarms_ << __E__;
	out << "\t"
	    << "savePath_: " << savePath_ << __E__;
	out << "\t"
	    << "saveFullFileName_: " << saveFullFileName_ << __E__;

}  // end print()

//==============================================================================
// underscoreString
//	replace all non-alphanumeric with underscore
std::string FESlowControlsChannel::underscoreString(const std::string& str)
{
	std::string retStr;
	retStr.reserve(str.size());
	for(unsigned int i = 0; i < str.size(); ++i)
		if((str[i] >= 'a' && str[i] <= 'z') || (str[i] >= 'A' && str[i] <= 'Z') || (str[i] >= '0' && str[i] <= '9'))
			retStr.push_back(str[i]);
		else
			retStr.push_back('_');
	return retStr;
}  // end underscoreString()

//==============================================================================
// convertStringToBuffer
//	if useDataType == false, then assume unsigned long long
//
// 	Note: buffer is expected to sized properly in advance, e.g. buffer.resize(#)
void FESlowControlsChannel::convertStringToBuffer(const std::string& inString, std::string& buffer, bool useDataType /*  = false */)
{
	if(useDataType && (dataType_ == "float" || dataType_ == "double"))
	{
		__GEN_COUT__ << "Floating point spec'd" << __E__;
		if(dataType_ == "float" && buffer.size() == sizeof(float))
		{
			sscanf(&inString[0], "%f", (float*)&buffer[0]);
			__GEN_COUT__ << "float: " << *((float*)&buffer[0]) << __E__;
		}
		else if(dataType_ == "double" && buffer.size() == sizeof(double))
		{
			sscanf(&inString[0], "%lf", (double*)&buffer[0]);
			__GEN_COUT__ << "double: " << *((double*)&buffer[0]) << __E__;
		}
		else
		{
			__GEN_SS__ << "Invalid floating point spec! "
			           << "dataType_=" << dataType_ << " buffer.size()=" << buffer.size() << __E__;
			__GEN_COUT_ERR__ << "\n" << ss.str();
			__GEN_SS_THROW__;
		}

		{  // print
			__GEN_SS__ << "0x ";
			for(int i = (int)buffer.size() - 1; i >= 0; --i)
				ss << std::hex << (int)((buffer[i] >> 4) & 0xF) << (int)((buffer[i]) & 0xF) << " " << std::dec;
			ss << __E__;
			__GEN_COUT__ << "\n" << ss.str();
		}
		return;
	}

	// at this point assume unsigned number that will be matched to buffer size
	unsigned long long val;
	if(!StringMacros::getNumber(inString, val))
	{
		__GEN_SS__ << "Invalid unsigned number format in string " << inString << __E__;
		ss << __E__;
		print(ss);
		__GEN_SS_THROW__;
	}
	// transfer the long long to the buffer
	unsigned int i = 0;
	for(; i < sizeof(long long) && i < buffer.size(); ++i)
		buffer[i] = ((char*)&val)[i];

	// clear remaining buffer
	for(; i < buffer.size(); ++i)
		buffer[i] = 0;

	__GEN_COUT_TYPE__(TLVL_DEBUG+12) << __COUT_HDR__ << "Resulting Number Buffer: " << BinaryStringMacros::binaryNumberToHexString(buffer, "0x", " ") << __E__;
}  // end convertStringToBuffer()

//==============================================================================
// handleSample
//	adds to txBuffer if sample should be sent to monitor server
void FESlowControlsChannel::handleSample(const std::string& universalReadValue, std::string& txBuffer, FILE* fpAggregate, bool aggregateIsBinaryFormat)
{
	// __GEN_COUT__ << "txBuffer size=" << txBuffer.size() << __E__;

	// first copy the read value to the channel, then extract sample from universalReadValue_
	universalReadValue_ = universalReadValue;
	//	considering bit size and offset
	extractSample();  // sample_ = universalReadValue_

	// behavior:
	//	if recordChangesOnly
	//		if no change and not first value
	//			return
	//
	//	for interesting value
	//		if monitoringEnabled
	//			add packet to buffer
	//			if alarmsEnabled
	//				for each alarm add packet to buffer
	//		if localSavingEnabled
	//			append to file

	//////////////////////////////////////////////////

	lastSampleTime_ = time(0);
	
	if(recordChangesOnly_)
	{
		if(lastSampleTime_ && lastSample_ == sample_)
		{
			__GEN_COUT__ << "no change." << __E__;
			return;  // no change
		}
	}

	__GEN_COUT__ << "new value!" << __E__;

	// else we have an interesting value!
	lastSample_     = sample_;

	char alarmMask = 0;

	/////////////////////////////////////////////
	/////////////////////////////////////////////
	/////////////////////////////////////////////
	if(monitoringEnabled_)
	{
		// create value packet:
		//  1B type (0: value, 1: loloalarm, 2: loalarm, 3: hioalarm, 4: hihialarm)
		//	1B sequence count from channel
		//	8B time
		//	4B sz of name
		//	name
		//	1B sz of value in bytes
		//	1B sz of value in bits
		//	value

		__GEN_COUT__ << "before txBuffer sz=" << txBuffer.size() << __E__;
		txBuffer.push_back(0);                          // value type
		txBuffer.push_back(txPacketSequenceNumber_++);  // sequence counter and increment

		txBuffer.resize(txBuffer.size() + sizeof(lastSampleTime_));
		memcpy(&txBuffer[txBuffer.size() - sizeof(lastSampleTime_)] /*dest*/, &lastSampleTime_ /*src*/, sizeof(lastSampleTime_));

		unsigned int tmpSz = fullChannelName_.size();

		txBuffer.resize(txBuffer.size() + sizeof(tmpSz));
		memcpy(&txBuffer[txBuffer.size() - sizeof(tmpSz)] /*dest*/, &tmpSz /*src*/, sizeof(tmpSz));

		txBuffer += fullChannelName_;

		txBuffer.push_back((unsigned char)sample_.size());       // size in bytes
		txBuffer.push_back((unsigned char)sizeOfDataTypeBits_);  // size in bits

		txBuffer += sample_;
		__GEN_COUT__ << "after txBuffer sz=" << txBuffer.size() << __E__;

		{  // print
			__GEN_SS__ << "txBuffer: \n";
			for(unsigned int i = 0; i < txBuffer.size(); ++i)
			{
				ss << std::hex << (int)((txBuffer[i] >> 4) & 0xF) << (int)((txBuffer[i]) & 0xF) << " " << std::dec;
				if(i % 8 == 7)
					ss << __E__;
			}
			ss << __E__;
			__GEN_COUT__ << "\n" << ss.str();
		}
	}

	// check alarms
	if(alarmsEnabled_)
		alarmMask = checkAlarms(txBuffer);

	// create array helper for saving
	std::string* alarmValueArray[] = {&lolo_, &lo_, &hi_, &hihi_};

	/////////////////////////////////////////////
	/////////////////////////////////////////////
	/////////////////////////////////////////////
	if(fpAggregate)  // aggregate file means saving enabled at FE level
	{
		// append to file
		if(aggregateIsBinaryFormat)
		{
			// save binary format:
			//	8B time (save any alarms by marking with time = 1, 2, 3, 4)
			//	4B sz of name
			//	name
			//	1B sz of value in bytes
			//	1B sz of value in bits
			//	value or alarm threshold

			__GEN_COUT__ << "Aggregate Binary File Format: " << sizeof(lastSampleTime_) << " " << sample_.size() << __E__;

			{
				fwrite(&lastSampleTime_, sizeof(lastSampleTime_), 1,
				       fpAggregate);  //	8B time

				unsigned int tmpSz = fullChannelName_.size();
				fwrite(&tmpSz, sizeof(tmpSz), 1, fpAggregate);  // 4B sz of name
				fwrite(&fullChannelName_[0], fullChannelName_.size(), 1,
				       fpAggregate);  // name

				unsigned char tmpChar = (unsigned char)sample_.size();
				fwrite(&tmpChar, 1, 1, fpAggregate);  // size in bytes

				tmpChar = (unsigned char)sizeOfDataTypeBits_;
				fwrite(&tmpChar, 1, 1, fpAggregate);                  // size in bits
				fwrite(&sample_[0], sample_.size(), 1, fpAggregate);  // value
			}

			// save any alarms by marking with time 1, 2, 3, 4
			if(alarmMask)  // if any alarms
				for(time_t i = 1; i < 5; ++i, alarmMask >>= 1)
					if(alarmMask & 1)
					{
						{
							fwrite(&i,
							       sizeof(lastSampleTime_),
							       1,
							       fpAggregate);  //	8B save any alarms by marking with
							                      // time = 1, 2, 3, 4

							unsigned int tmpSz = fullChannelName_.size();
							fwrite(&tmpSz, sizeof(tmpSz), 1, fpAggregate);  // 4B sz of name
							fwrite(&fullChannelName_[0], fullChannelName_.size(), 1,
							       fpAggregate);  // name

							unsigned char tmpChar = (unsigned char)sample_.size();
							fwrite(&tmpChar, 1, 1, fpAggregate);  // size in bytes

							tmpChar = (unsigned char)sizeOfDataTypeBits_;
							fwrite(&tmpChar, 1, 1, fpAggregate);  // size in bits
							fwrite(&(*alarmValueArray[i - 1])[0], (*alarmValueArray[i - 1]).size(), 1,
							       fpAggregate);  // alarm threshold
						}
					}
		}
		else
		{
			// save text format:
			//	timestamp (save any alarms by marking with time 1, 2, 3, 4)
			//	name
			//	value

			__GEN_COUT__ << "Aggregate Text File Format: " << dataType_ << __E__;

			fprintf(fpAggregate, "%lu\n", lastSampleTime_);
			fprintf(fpAggregate, "%s\n", fullChannelName_.c_str());

			if(dataType_[dataType_.size() - 1] == 'b')  // if ends in 'b' then take that many bits
			{
				std::stringstream ss;
				ss << "0x";
				for(unsigned int i = 0; i < sample_.size(); ++i)
					ss << std::hex << (int)((sample_[i] >> 4) & 0xF) << (int)((sample_[i]) & 0xF) << std::dec;
				fprintf(fpAggregate, "%s\n", ss.str().c_str());
			}
			else if(dataType_ == "char")
				fprintf(fpAggregate, "%d\n", *((char*)(&sample_[0])));
			else if(dataType_ == "unsigned char")
				fprintf(fpAggregate, "%u\n", *((unsigned char*)(&sample_[0])));
			else if(dataType_ == "short")
				fprintf(fpAggregate, "%d\n", *((short*)(&sample_[0])));
			else if(dataType_ == "unsigned short")
				fprintf(fpAggregate, "%u\n", *((unsigned short*)(&sample_[0])));
			else if(dataType_ == "int")
				fprintf(fpAggregate, "%d\n", *((int*)(&sample_[0])));
			else if(dataType_ == "unsigned int")
				fprintf(fpAggregate, "%u\n", *((unsigned int*)(&sample_[0])));
			else if(dataType_ == "long long")
				fprintf(fpAggregate, "%lld\n", *((long long*)(&sample_[0])));
			else if(dataType_ == "unsigned long long")
				fprintf(fpAggregate, "%llu\n", *((unsigned long long*)(&sample_[0])));
			else if(dataType_ == "float")
				fprintf(fpAggregate, "%f\n", *((float*)(&sample_[0])));
			else if(dataType_ == "double")
				fprintf(fpAggregate, "%f\n", *((double*)(&sample_[0])));

			// save any alarms by marking with time 1, 2, 3, 4
			if(alarmMask)  // if any alarms
			{
				char checkMask = 1;  // use mask to maintain alarm mask
				for(time_t i = 1; i < 5; ++i, checkMask <<= 1)
					if(alarmMask & checkMask)
					{
						fprintf(fpAggregate, "%lu\n", i);
						fprintf(fpAggregate, "%s\n", fullChannelName_.c_str());

						if(dataType_[dataType_.size() - 1] == 'b')  // if ends in 'b' then take that many bits
						{
							std::stringstream ss;
							ss << "0x";
							for(unsigned int j = 0; j < (*alarmValueArray[i - 1]).size(); ++i)
								ss << std::hex << (int)(((*alarmValueArray[i - 1])[j] >> 4) & 0xF) << (int)(((*alarmValueArray[i - 1])[j]) & 0xF) << std::dec;
							fprintf(fpAggregate, "%s\n", ss.str().c_str());
						}
						else if(dataType_ == "char")
							fprintf(fpAggregate, "%d\n", *((char*)(&(*alarmValueArray[i - 1])[0])));
						else if(dataType_ == "unsigned char")
							fprintf(fpAggregate, "%u\n", *((unsigned char*)(&(*alarmValueArray[i - 1])[0])));
						else if(dataType_ == "short")
							fprintf(fpAggregate, "%d\n", *((short*)(&(*alarmValueArray[i - 1])[0])));
						else if(dataType_ == "unsigned short")
							fprintf(fpAggregate, "%u\n", *((unsigned short*)(&(*alarmValueArray[i - 1])[0])));
						else if(dataType_ == "int")
							fprintf(fpAggregate, "%d\n", *((int*)(&(*alarmValueArray[i - 1])[0])));
						else if(dataType_ == "unsigned int")
							fprintf(fpAggregate, "%u\n", *((unsigned int*)(&(*alarmValueArray[i - 1])[0])));
						else if(dataType_ == "long long")
							fprintf(fpAggregate, "%lld\n", *((long long*)(&(*alarmValueArray[i - 1])[0])));
						else if(dataType_ == "unsigned long long")
							fprintf(fpAggregate, "%llu\n", *((unsigned long long*)(&(*alarmValueArray[i - 1])[0])));
						else if(dataType_ == "float")
							fprintf(fpAggregate, "%f\n", *((float*)(&(*alarmValueArray[i - 1])[0])));
						else if(dataType_ == "double")
							fprintf(fpAggregate, "%f\n", *((double*)(&(*alarmValueArray[i - 1])[0])));
					}
			}
		}
	}

	/////////////////////////////////////////////
	/////////////////////////////////////////////
	/////////////////////////////////////////////
	if(saveEnabled_)
	{
		FILE* fp = fopen(saveFullFileName_.c_str(), saveBinaryFormat_ ? "ab" : "a");
		if(!fp)
		{
			__GEN_COUT_ERR__ << "Failed to open slow controls channel file: " << saveFullFileName_ << __E__;
			return;
		}

		// append to file
		if(saveBinaryFormat_)
		{
			__GEN_COUT__ << "Binary File Format: " << sizeof(lastSampleTime_) << " " << sample_.size() << __E__;
			fwrite(&lastSampleTime_, sizeof(lastSampleTime_), 1, fp);
			fwrite(&sample_[0], sample_.size(), 1, fp);

			// save any alarms by marking with time 1, 2, 3, 4
			if(alarmMask)  // if any alarms
				for(time_t i = 1; i < 5; ++i, alarmMask >>= 1)
					if(alarmMask & 1)
					{
						fwrite(&i, sizeof(lastSampleTime_), 1, fp);
						fwrite(&(*alarmValueArray[i - 1])[0], (*alarmValueArray[i - 1]).size(), 1, fp);
					}
		}
		else
		{
			__GEN_COUT__ << "Text File Format: " << dataType_ << __E__;

			fprintf(fp, "%lu\n", lastSampleTime_);

			if(dataType_[dataType_.size() - 1] == 'b')  // if ends in 'b' then take that many bits
			{
				std::stringstream ss;
				ss << "0x";
				for(unsigned int i = 0; i < sample_.size(); ++i)
					ss << std::hex << (int)((sample_[i] >> 4) & 0xF) << (int)((sample_[i]) & 0xF) << std::dec;
				fprintf(fp, "%s\n", ss.str().c_str());
			}
			else if(dataType_ == "char")
				fprintf(fp, "%d\n", *((char*)(&sample_[0])));
			else if(dataType_ == "unsigned char")
				fprintf(fp, "%u\n", *((unsigned char*)(&sample_[0])));
			else if(dataType_ == "short")
				fprintf(fp, "%d\n", *((short*)(&sample_[0])));
			else if(dataType_ == "unsigned short")
				fprintf(fp, "%u\n", *((unsigned short*)(&sample_[0])));
			else if(dataType_ == "int")
				fprintf(fp, "%d\n", *((int*)(&sample_[0])));
			else if(dataType_ == "unsigned int")
				fprintf(fp, "%u\n", *((unsigned int*)(&sample_[0])));
			else if(dataType_ == "long long")
				fprintf(fp, "%lld\n", *((long long*)(&sample_[0])));
			else if(dataType_ == "unsigned long long")
				fprintf(fp, "%llu\n", *((unsigned long long*)(&sample_[0])));
			else if(dataType_ == "float")
				fprintf(fp, "%f\n", *((float*)(&sample_[0])));
			else if(dataType_ == "double")
				fprintf(fp, "%f\n", *((double*)(&sample_[0])));

			// save any alarms by marking with time 1, 2, 3, 4
			if(alarmMask)  // if any alarms
			{
				char checkMask = 1;  // use mask to maintain alarm mask
				for(time_t i = 1; i < 5; ++i, checkMask <<= 1)
					if(alarmMask & checkMask)
					{
						fprintf(fp, "%lu\n", i);

						if(dataType_[dataType_.size() - 1] == 'b')  // if ends in 'b' then take that many bits
						{
							std::stringstream ss;
							ss << "0x";
							for(unsigned int j = 0; j < (*alarmValueArray[i - 1]).size(); ++i)
								ss << std::hex << (int)(((*alarmValueArray[i - 1])[j] >> 4) & 0xF) << (int)(((*alarmValueArray[i - 1])[j]) & 0xF) << std::dec;
							fprintf(fp, "%s\n", ss.str().c_str());
						}
						else if(dataType_ == "char")
							fprintf(fp, "%d\n", *((char*)(&(*alarmValueArray[i - 1])[0])));
						else if(dataType_ == "unsigned char")
							fprintf(fp, "%u\n", *((unsigned char*)(&(*alarmValueArray[i - 1])[0])));
						else if(dataType_ == "short")
							fprintf(fp, "%d\n", *((short*)(&(*alarmValueArray[i - 1])[0])));
						else if(dataType_ == "unsigned short")
							fprintf(fp, "%u\n", *((unsigned short*)(&(*alarmValueArray[i - 1])[0])));
						else if(dataType_ == "int")
							fprintf(fp, "%d\n", *((int*)(&(*alarmValueArray[i - 1])[0])));
						else if(dataType_ == "unsigned int")
							fprintf(fp, "%u\n", *((unsigned int*)(&(*alarmValueArray[i - 1])[0])));
						else if(dataType_ == "long long")
							fprintf(fp, "%lld\n", *((long long*)(&(*alarmValueArray[i - 1])[0])));
						else if(dataType_ == "unsigned long long")
							fprintf(fp, "%llu\n", *((unsigned long long*)(&(*alarmValueArray[i - 1])[0])));
						else if(dataType_ == "float")
							fprintf(fp, "%f\n", *((float*)(&(*alarmValueArray[i - 1])[0])));
						else if(dataType_ == "double")
							fprintf(fp, "%f\n", *((double*)(&(*alarmValueArray[i - 1])[0])));
					}
			}
		}

		fclose(fp);
	}
}

//==============================================================================
// extractSample
//	extract sample from universalReadValue
//		considering bit size and offset
void FESlowControlsChannel::extractSample()
{
	const std::string& universalReadValue = universalReadValue_; //do not modify

	{  // print
		__GEN_SS__ << "Universal Read: ";
		for(unsigned int i = 0; i < universalReadValue.size(); ++i)
			ss << std::hex << (int)((universalReadValue[i] >> 4) & 0xF) << (int)((universalReadValue[i]) & 0xF) << " " << std::dec;
		ss << __E__;
		__GEN_COUT__ << "\n" << ss.str();
		__GEN_COUT__ << "Universal Read: " << BinaryStringMacros::binaryNumberToHexString(universalReadValue, "0x", " ") << " at t=" << time(0) << __E__;
	}

	sample_.resize(0);  // clear a la sample_ = "";
	BinaryStringMacros::extractValueFromBinaryString(universalReadValue, sample_, 
		sizeOfDataTypeBits_, universalDataBitOffset_);

	__GEN_COUT__ << "Sample: " << BinaryStringMacros::binaryNumberToHexString(sample_, "0x", " ") << 
		", from address: " << 
		BinaryStringMacros::binaryNumberToHexString(universalAddress_, "0x", " ") << 
		", sample size in bytes: " << sample_.size() << 
		", in bits: " << sizeOfDataTypeBits_ << 
		", at bit-offset: " << universalDataBitOffset_ << __E__;
	
}  // end extractSample()

//==============================================================================
// clearAlarms
//	clear target alarm
// 	if -1 clear all
void FESlowControlsChannel::clearAlarms(int targetAlarm)
{
	if(targetAlarm == -1 || targetAlarm == 0)
		loloAlarmed_ = false;
	if(targetAlarm == -1 || targetAlarm == 1)
		loAlarmed_ = false;
	if(targetAlarm == -1 || targetAlarm == 2)
		hiAlarmed_ = false;
	if(targetAlarm == -1 || targetAlarm == 3)
		hihiAlarmed_ = false;
}  // end clearAlarms()

//==============================================================================
// checkAlarms
//	if current value is a new alarm, set alarmed and add alarm packet to buffer
//
//	return mask of alarms that fired during this check.
char FESlowControlsChannel::checkAlarms(std::string& txBuffer)
{
	// procedure:
	//	for each alarm type
	//		determine type and get value as proper type
	//			i.e.:
	//				0 -- unsigned long long
	//				1 -- long long
	//				2 -- float
	//				3 -- double
	//			do comparison with the type
	//				alarm alarms

	int  useType          = -1;
	char createPacketMask = 0;

	if(dataType_[dataType_.size() - 1] == 'b')  // if ends in 'b' then take that many bits
		useType = 0;
	else if(dataType_ == "char")
		useType = 1;
	else if(dataType_ == "unsigned char")
		useType = 0;
	else if(dataType_ == "short")
		useType = 1;
	else if(dataType_ == "unsigned short")
		useType = 0;
	else if(dataType_ == "int")
		useType = 1;
	else if(dataType_ == "unsigned int")
		useType = 0;
	else if(dataType_ == "long long")
		useType = 1;
	else if(dataType_ == "unsigned long long")
		useType = 0;
	else if(dataType_ == "float")
		useType = 2;
	else if(dataType_ == "double")
		useType = 3;

	if(useType == 0)  // unsigned long long
	{
		__GEN_COUT__ << "Using unsigned long long for alarms." << __E__;
		// lolo
		if((!loloAlarmed_ || !latchAlarms_) && *((unsigned long long*)&sample_[0]) <= *((unsigned long long*)&lolo_[0]))
		{
			loloAlarmed_ = true;
			createPacketMask |= 1 << 0;
		}
		// lo
		if((!loAlarmed_ || !latchAlarms_) && *((unsigned long long*)&sample_[0]) <= *((unsigned long long*)&lo_[0]))
		{
			loAlarmed_ = true;
			createPacketMask |= 1 << 1;
		}
		// hi
		if((!hiAlarmed_ || !latchAlarms_) && *((unsigned long long*)&sample_[0]) >= *((unsigned long long*)&hi_[0]))
		{
			hiAlarmed_ = true;
			createPacketMask |= 1 << 2;
		}
		// hihi
		if((!hihiAlarmed_ || !latchAlarms_) && *((unsigned long long*)&sample_[0]) >= *((unsigned long long*)&hihi_[0]))
		{
			hihiAlarmed_ = true;
			createPacketMask |= 1 << 3;
		}
	}
	else if(useType == 1)  // long long
	{
		__GEN_COUT__ << "Using long long for alarms." << __E__;
		// lolo
		if((!loloAlarmed_ || !latchAlarms_) && *((long long*)&sample_[0]) <= *((long long*)&lolo_[0]))
		{
			loloAlarmed_ = true;
			createPacketMask |= 1 << 0;
		}
		// lo
		if((!loAlarmed_ || !latchAlarms_) && *((long long*)&sample_[0]) <= *((long long*)&lo_[0]))
		{
			loAlarmed_ = true;
			createPacketMask |= 1 << 1;
		}
		// hi
		if((!hiAlarmed_ || !latchAlarms_) && *((long long*)&sample_[0]) >= *((long long*)&hi_[0]))
		{
			hiAlarmed_ = true;
			createPacketMask |= 1 << 2;
		}
		// hihi
		if((!hihiAlarmed_ || !latchAlarms_) && *((long long*)&sample_[0]) >= *((long long*)&hihi_[0]))
		{
			hihiAlarmed_ = true;
			createPacketMask |= 1 << 3;
		}
	}
	else if(useType == 2)  // float
	{
		__GEN_COUT__ << "Using float for alarms." << __E__;
		// lolo
		if((!loloAlarmed_ || !latchAlarms_) && *((float*)&sample_[0]) <= *((float*)&lolo_[0]))
		{
			loloAlarmed_ = true;
			createPacketMask |= 1 << 0;
		}
		// lo
		if((!loAlarmed_ || !latchAlarms_) && *((float*)&sample_[0]) <= *((float*)&lo_[0]))
		{
			loAlarmed_ = true;
			createPacketMask |= 1 << 1;
		}
		// hi
		if((!hiAlarmed_ || !latchAlarms_) && *((float*)&sample_[0]) >= *((float*)&hi_[0]))
		{
			hiAlarmed_ = true;
			createPacketMask |= 1 << 2;
		}
		// hihi
		if((!hihiAlarmed_ || !latchAlarms_) && *((float*)&sample_[0]) >= *((float*)&hihi_[0]))
		{
			hihiAlarmed_ = true;
			createPacketMask |= 1 << 3;
		}
	}
	else if(useType == 3)  // double
	{
		__GEN_COUT__ << "Using double for alarms." << __E__;
		// lolo
		if((!loloAlarmed_ || !latchAlarms_) && *((double*)&sample_[0]) <= *((double*)&lolo_[0]))
		{
			loloAlarmed_ = true;
			createPacketMask |= 1 << 0;
		}
		// lo
		if((!loAlarmed_ || !latchAlarms_) && *((double*)&sample_[0]) <= *((double*)&lo_[0]))
		{
			loAlarmed_ = true;
			createPacketMask |= 1 << 1;
		}
		// hi
		if((!hiAlarmed_ || !latchAlarms_) && *((double*)&sample_[0]) >= *((double*)&hi_[0]))
		{
			hiAlarmed_ = true;
			createPacketMask |= 1 << 2;
		}
		// hihi
		if((!hihiAlarmed_ || !latchAlarms_) && *((double*)&sample_[0]) >= *((double*)&hihi_[0]))
		{
			hihiAlarmed_ = true;
			createPacketMask |= 1 << 3;
		}
	}

	// create array helper for packet gen
	std::string* alarmValueArray[] = {&lolo_, &lo_, &hi_, &hihi_};

	if(monitoringEnabled_)  // only create packet if monitoring
	{
		// create a packet for each bit in mask
		char checkMask = 1;  // use mask to maintain return mask
		for(int i = 0; i < 4; ++i, checkMask <<= 1)
			if(createPacketMask & checkMask)
			{
				// create alarm packet:
				//  1B type (0: value, 1: loloalarm, 2: loalarm, 3: hioalarm, 4:
				//  hihialarm)
				//	1B sequence count from channel
				//	8B time
				//	4B sz of name
				//	name
				//	1B sz of alarm value in bytes
				//	1B sz of alarm value in bits
				//	alarm value

				__GEN_COUT__ << "Create packet type " << i + 1 << " alarm value = " << *alarmValueArray[i] << __E__;

				__GEN_COUT__ << "before txBuffer sz=" << txBuffer.size() << __E__;
				txBuffer.push_back(i + 1);                      // alarm packet type
				txBuffer.push_back(txPacketSequenceNumber_++);  // sequence counter and increment

				txBuffer.resize(txBuffer.size() + sizeof(lastSampleTime_));
				memcpy(&txBuffer[txBuffer.size() - sizeof(lastSampleTime_)] /*dest*/, &lastSampleTime_ /*src*/, sizeof(lastSampleTime_));

				unsigned int tmpSz = fullChannelName_.size();

				txBuffer.resize(txBuffer.size() + sizeof(tmpSz));
				memcpy(&txBuffer[txBuffer.size() - sizeof(tmpSz)] /*dest*/, &tmpSz /*src*/, sizeof(tmpSz));

				txBuffer += fullChannelName_;

				txBuffer.push_back((unsigned char)(*alarmValueArray[i]).size());  // size in bytes
				txBuffer.push_back((unsigned char)sizeOfDataTypeBits_);           // size in bits

				txBuffer += (*alarmValueArray[i]);
				__GEN_COUT__ << "after txBuffer sz=" << txBuffer.size() << __E__;

				{  // print
					__GEN_SS__ << "txBuffer: \n";
					for(unsigned int i = 0; i < txBuffer.size(); ++i)
					{
						ss << std::hex << (int)((txBuffer[i] >> 4) & 0xF) << (int)((txBuffer[i]) & 0xF) << " " << std::dec;
						if(i % 8 == 7)
							ss << __E__;
					}
					ss << __E__;
					__GEN_COUT__ << "\n" << ss.str();
				}
			}
	}

	return createPacketMask;
}  // end checkAlarms()
