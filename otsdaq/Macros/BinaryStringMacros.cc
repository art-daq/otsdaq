#include "otsdaq/Macros/BinaryStringMacros.h"

using namespace ots;

//==============================================================================
// binaryToHexString
//	convert a data buffer of <len> bytes to a hex string 2*len characters long
//	Note: no preamble is applied by default (but "0x" could be nice)
//
//	Note: this is used with defaults by VisualSupervisor
std::string BinaryStringMacros::binaryStringToHexString(const void*        binaryBuffer,
                                                        unsigned int       numberOfBytes,
                                                        const std::string& resultPreamble,
                                                        const std::string& resultDelimiter)
{
	std::string dest;
	dest.reserve(numberOfBytes * 2);
	char hexstr[3];

	for(unsigned int i = 0; i < numberOfBytes; ++i)
	{
		sprintf(hexstr, "%02X", (unsigned char)((const char*)binaryBuffer)[i]);
		if(i)
			dest += resultDelimiter;
		dest += hexstr;
	}
	return resultPreamble + dest;
}  // end binaryToHexString

//==============================================================================
// binaryNumberToHexString
//	convert a data buffer string a hex string
//		8 bytes at a time with the least significant byte last.
std::string BinaryStringMacros::binaryNumberToHexString(const std::string& binaryBuffer,
                                                        const std::string& resultPreamble /*"0x"*/,
                                                        const std::string& resultDelimiter /*" "*/)
{
	return binaryNumberToHexString(&binaryBuffer[0], binaryBuffer.size(), resultPreamble, resultDelimiter);
}  // end binaryNumberToHexString()

//==============================================================================
// binaryNumberToHexString
//	convert a data buffer string a hex string
//		8 bytes at a time with the least significant byte last.
std::string BinaryStringMacros::binaryNumberToHexString(const void*        binaryBuffer,
                                                        unsigned int       numberOfBytes,
                                                        const std::string& resultPreamble /*"0x"*/,
                                                        const std::string& resultDelimiter /*" "*/)
{
	std::string dest;
	dest.reserve(numberOfBytes * 2 + resultDelimiter.size() * (numberOfBytes / 8) + resultPreamble.size());
	char hexstr[3];

	dest += resultPreamble;

	unsigned int j = 0;
	for(; j + 8 < numberOfBytes; j += 8)
	{
		if(j)
			dest += resultDelimiter;
		for(unsigned int k = 0; k < 8; ++k)
		{
			sprintf(hexstr, "%02X", (unsigned char)((const char*)binaryBuffer)[7 - k + j * 8]);
			dest += hexstr;
		}
	}
	for(unsigned int k = numberOfBytes - 1; k >= j; --k)
	{
		sprintf(hexstr, "%02X", (unsigned char)((const char*)binaryBuffer)[k]);
		dest += hexstr;
		if(k == 0)
			break;  // to handle unsigned numbers when j is 0
	}

	return dest;
}  // end binaryNumberToHexString()

//==============================================================================
// insertValueInBinaryString
// 	static and specialized for string value
void BinaryStringMacros::insertValueInBinaryString(std::string& binaryBuffer, const std::string& value, unsigned int bitIndex /* = 0 */)
{
	std::string dataType = StringMacros::getNumberType(value);
	if(dataType == "nan")
	{
		__SS__ << "String value must be a valid number! Value was " << value << __E__;
		__SS_THROW__;
	}

	if(dataType == "double")
	{
		double v;
		if(!StringMacros::getNumber<double>(value, v))
		{
			__SS__ << "String double value must be a valid number! Value was " << value << __E__;
			__SS_THROW__;
		}
		BinaryStringMacros::insertValueInBinaryString<double>(binaryBuffer, v, bitIndex);
	}
	else  // assume unsigned long long
	{
		unsigned long long v;
		if(!StringMacros::getNumber<unsigned long long>(value, v))
		{
			__SS__ << "String unsigned long long value must be a valid number! Value was " << value << __E__;
			__SS_THROW__;
		}
		BinaryStringMacros::insertValueInBinaryString<unsigned long long>(binaryBuffer, v, bitIndex);
	}
}  // end insertValueInBinaryString()

//==============================================================================
// extractValueFromBinaryString
//	static template function
//	Extract value from buffer starting at bitIndex position
void BinaryStringMacros::extractValueFromBinaryString(
    const void* binaryBufferVoid, unsigned int bufferNumberOfBytes, void* valueVoid, unsigned int valueNumberOfBits, unsigned int bitIndex /* = 0 */)
{
	//__COUTV__(bufferNumberOfBytes);

	//__COUTV__(valueNumberOfBits);

	if(valueNumberOfBits == 0)
	{
		__SS__ << "Can not extract value of size 0!" << __E__;
		__SS_THROW__;
	}

	if(bitIndex + valueNumberOfBits > bufferNumberOfBytes * 8)
	{
		__SS__ << "Can not extract value of size " << valueNumberOfBits << ", at position " << bitIndex << ", from buffer of size " << bufferNumberOfBytes * 8
		       << "." << __E__;
		__SS_THROW__;
	}

	unsigned char*       value        = (unsigned char*)valueVoid;
	const unsigned char* binaryBuffer = (const unsigned char*)binaryBufferVoid;
	unsigned int         byteOffset   = bitIndex / 8;
	unsigned int         bitOffset    = bitIndex % 8;
	unsigned int         bitsLeft     = valueNumberOfBits;

	// unsigned int valueBytes = (valueNumberOfBits / 8) + ((valueNumberOfBits % 8) ? 1 : 0);

	unsigned int valuei = 0;

	while(bitsLeft)  // copy whole bytes
	{
		//		__COUT__ << "bitsLeft=" << bitsLeft <<
		//				" byteOffset=" << byteOffset <<
		//				" bitOffset=" << bitOffset <<
		//				" valuei=" << valuei <<
		//				__E__;

		// each byte is partial of buffer[n] + buffer[n+1] overlapping by bitOffset
		value[valuei] = binaryBuffer[byteOffset] >> bitOffset;
		if(bitOffset)
			value[valuei] |= binaryBuffer[byteOffset + 1] << (8 - bitOffset);

		if(bitsLeft > 7)
		{
			bitsLeft -= 8;
			++valuei;
			++byteOffset;
		}
		else  // only a partial byte left
		{
			unsigned char keepMask = ((unsigned char)(-1)) >> (8 - bitsLeft);
			value[valuei] &= keepMask;
			bitsLeft = 0;
		}

		//		__COUT__ << "value: " <<
		//						BinaryStringMacros::binaryToHexString(
		//								(char *)value,valueBytes,"0x"," ") << __E__;
	}

	//	__COUT__ << "value: " <<
	//					BinaryStringMacros::binaryToHexString(
	//							(char *)value,valueBytes,"0x"," ") << __E__;

}  // end extractValueFromBinaryString()

//==============================================================================
// extractValueFromBinaryString
//	static template function
//	Extract value from buffer starting at bitIndex position
void BinaryStringMacros::extractValueFromBinaryString(const std::string& binaryBuffer,
                                                      std::string&       value,
                                                      unsigned int       valueNumberOfBits,
                                                      unsigned int       bitIndex /* = 0 */)
{
	value.resize((valueNumberOfBits / 8) + ((valueNumberOfBits % 8) ? 1 : 0));
	extractValueFromBinaryString(&binaryBuffer[0], binaryBuffer.size(), &value[0], valueNumberOfBits, bitIndex);
}  // end extractValueFromBinaryString()
