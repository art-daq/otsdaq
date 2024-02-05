#include "otsdaq/BitManipulator/BitManipulator.h"
#include <cstdlib>
#include <iostream>
#include <sstream>
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

using namespace ots;

//==============================================================================
BitManipulator::BitManipulator() {}

//==============================================================================
BitManipulator::~BitManipulator() {}

//==============================================================================
uint64_t BitManipulator::insertBits(uint64_t& data, uint64_t value, unsigned int startBit, unsigned int numberOfBits)
{
	//    std::cout << __COUT_HDR_FL__ << "Before: " << std::hex << data << "-<-" << value
	//    << std::dec << std::endl;
	for(unsigned int i = 0; i < numberOfBits; i++)
		data &= ~((uint64_t)1 << (startBit + i));

	data |= (value << startBit);
	//    std::cout << __COUT_HDR_FL__ << "After:  " << std::hex << data << "-<-" << value
	//    << std::dec << std::endl;
	return data;
}

//==============================================================================
uint64_t BitManipulator::insertBits(std::string& data, uint64_t value, unsigned int startBit, unsigned int numberOfBits)
{
	uint8_t            toWrite     = 0;
	const unsigned int bitsInAByte = 8;

	uint8_t overWritten    = 0;
	int     startByte      = startBit / bitsInAByte;
	int     finalByte      = (startBit + numberOfBits - 1) / bitsInAByte;
	int     startBitInByte = startBit % bitsInAByte;
	int     finalBitInByte = (startBit + numberOfBits - 1) % bitsInAByte;
	int     tmp;
	// int     firstByteLength = bitsInAByte;
	int lastByteLength = (startBit + numberOfBits) % bitsInAByte;

	//
	// std::cout << __COUT_HDR_FL__ << " start from byte : " << startByte << ", finish in
	// " << finalByte << "\n" << std::endl;

	for(int j = 0; j <= finalByte - startByte; ++j)
	{
		if(j != 0)
			startBitInByte = 0;
		if(j != finalByte - startByte)
			finalBitInByte = 7;
		else
			finalBitInByte = (startBit + numberOfBits - 1) % 8;

		tmp            = finalBitInByte;
		finalBitInByte = 7 - startBitInByte;
		startBitInByte = 7 - tmp;

		// std::cout << __COUT_HDR_FL__ << "in byte : " << startByte + j << ", start bit:
		// " << startBitInByte << ", finish in bit " << finalBitInByte << "\n" <<
		// std::endl;

		overWritten = data.substr(startByte + j, 1).data()[0];
		// std::cout << __COUT_HDR_FL__ << "value overwritten: " << hex <<
		// (uint64_t)overWritten << "\n" << std::endl;
		//    	toWrite = (uint8_t)value; //FIXME you can declare value as uint8_t from
		//    the beginning 30-0600000000000000
		toWrite = (uint8_t)0;
		for(int y = 0; y <= finalBitInByte - startBitInByte; ++y)
		{
			if(finalByte - startByte > 1)
			{
				if(j != finalByte - startByte)
					toWrite |= ((value >> (lastByteLength + (finalByte - startByte - 1 - j) * 8 + y)) & 1) << y;
				else
					toWrite |= ((value >> (lastByteLength + y)) & 1) << y;
			}
			else if(finalByte - startByte == 1)
				toWrite |= ((value >> (lastByteLength * (1 - j) + y)) & 1) << y;
			else if(finalByte - startByte == 0)
				toWrite |= ((value >> y) & 1) << y;
			//    		toWrite |= ((value >> (firstByteLength + (j-1)*8 + y))&1) << (j*8
			//    + y);
		}

		// std::cout << __COUT_HDR_FL__ << "value to manipulate: " << hex <<
		// (uint64_t)toWrite << " obtained from " << tmpVal << "\n" << std::endl;

		for(int n = 0; n < finalBitInByte - startBitInByte + 1; n++)
		{
			overWritten &= ~((uint64_t)1 << (startBitInByte + n));
		}
		// std::cout << __COUT_HDR_FL__ << "value in intermediate step: " << hex <<
		// (uint64_t)overWritten << "\n" << std::endl;

		overWritten |= (toWrite << startBitInByte);

		// std::cout << __COUT_HDR_FL__ << "value to overwrite: " << hex <<
		// (uint64_t)overWritten << "\n" << std::endl;
		data[startByte + j] = overWritten;

		//	if(j == 0)
		//		firstByteLength = finalBitInByte - startBitInByte + 1;
	}

	return (uint64_t)overWritten;
}

//==============================================================================
uint64_t BitManipulator::reverseBits(uint64_t data, unsigned int startBit, unsigned int numberOfBits)
{
	uint64_t reversedData = 0;
	for(unsigned int r = startBit; r < numberOfBits; r++)
		reversedData |= ((data >> r) & 1) << (numberOfBits - 1 - r);
	return reversedData;
}

//==============================================================================
uint32_t BitManipulator::insertBits(uint32_t& data, uint32_t value, unsigned int startBit, unsigned int numberOfBits)
{
	//    std::cout << __COUT_HDR_FL__ << "Before: " << hex << data << "-<-" << value <<
	//    std::endl;
	value = value << startBit;
	for(unsigned int i = 0; i < 32; i++)
	{
		if(i >= startBit && i < startBit + numberOfBits)
			data &= ~((uint32_t)1 << i);
		else
			value &= ~((uint32_t)1 << i);
	}
	data += value;
	// std::cout << __COUT_HDR_FL__ << "After:  " << hex << data << "-<-" << value <<
	// std::endl;
	return data;
}

//==============================================================================
uint32_t BitManipulator::insertBits(std::string& data, uint32_t value, unsigned int startBit, unsigned int numberOfBits)
{
	uint8_t            toWrite     = 0;
	const unsigned int bitsInAByte = 8;

	uint8_t overWritten    = 0;
	int     startByte      = startBit / bitsInAByte;
	int     finalByte      = (startBit + numberOfBits - 1) / bitsInAByte;
	int     startBitInByte = startBit % 8;
	int     finalBitInByte = (startBit + numberOfBits - 1) % bitsInAByte;
	int     tmp;
	// int     firstByteLength = bitsInAByte;
	int lastByteLength = (startBit + numberOfBits) % bitsInAByte;

	//
	// std::cout << __COUT_HDR_FL__ << " start from byte : " << startByte << ", finish in
	// " << finalByte << "\n" << std::endl;

	for(int j = 0; j <= finalByte - startByte; ++j)
	{
		if(j != 0)
			startBitInByte = 0;
		if(j != finalByte - startByte)
			finalBitInByte = 7;
		else
			finalBitInByte = (startBit + numberOfBits - 1) % 8;

		tmp            = finalBitInByte;
		finalBitInByte = 7 - startBitInByte;
		startBitInByte = 7 - tmp;

		// std::cout << __COUT_HDR_FL__ << "in byte : " << startByte + j << ", start bit:
		// " << startBitInByte << ", finish in bit " << finalBitInByte << "\n" <<
		// std::endl;

		overWritten = data.substr(startByte + j, 1).data()[0];
		// std::cout << __COUT_HDR_FL__ << "value overwritten: " << hex <<
		// (uint64_t)overWritten << "\n" << std::endl;
		//    	toWrite = (uint8_t)value; //FIXME you can declare value as uint8_t from
		//    the beginning 30-0600000000000000
		toWrite = (uint8_t)0;
		for(int y = 0; y <= finalBitInByte - startBitInByte; ++y)
		{
			if(finalByte - startByte > 1)
			{
				if(j != finalByte - startByte)
					toWrite |= ((value >> (lastByteLength + (finalByte - startByte - 1 - j) * 8 + y)) & 1) << y;
				else
					toWrite |= ((value >> (lastByteLength + y)) & 1) << y;
			}
			else if(finalByte - startByte == 1)
				toWrite |= ((value >> (lastByteLength * (1 - j) + y)) & 1) << y;
			else if(finalByte - startByte == 0)
				toWrite |= ((value >> y) & 1) << y;
			//    		toWrite |= ((value >> (firstByteLength + (j-1)*8 + y))&1) << (j*8
			//    + y);
		}

		// std::cout << __COUT_HDR_FL__ << "value to manipulate: " << hex <<
		// (uint64_t)toWrite << " obtained from " << tmpVal << "\n" << std::endl;

		for(int n = 0; n < finalBitInByte - startBitInByte + 1; n++)
		{
			overWritten &= ~((uint32_t)1 << (startBitInByte + n));
		}
		// std::cout << __COUT_HDR_FL__ << "value in intermediate step: " << hex <<
		// (uint64_t)overWritten << "\n" << std::endl;

		overWritten |= (toWrite << startBitInByte);

		// std::cout << __COUT_HDR_FL__ << "value to overwrite: " << hex <<
		// (uint64_t)overWritten << "\n" << std::endl;
		data[startByte + j] = overWritten;

		// if(j == 0)
		//	firstByteLength = finalBitInByte - startBitInByte + 1;
	}

	return (uint32_t)overWritten;
}

//==============================================================================
uint32_t BitManipulator::reverseBits(uint32_t data, unsigned int startBit, unsigned int numberOfBits)
{
	uint32_t reversedData = 0;
	for(unsigned int r = startBit; r < startBit + numberOfBits; r++)
		reversedData |= ((data >> r) & 1) << (numberOfBits - 1 - r);
	return reversedData;
}

//==============================================================================
uint32_t BitManipulator::readBits(uint32_t data, unsigned int startBit, unsigned int numberOfBits)
{
	uint32_t returnData = 0;
	for(unsigned int r = startBit; r < startBit + numberOfBits; r++)
		returnData += ((data >> r) & 0x1) << (r - startBit);
	return returnData;
}
