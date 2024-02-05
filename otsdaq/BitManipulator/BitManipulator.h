#ifndef ots_BitManipulator_h
#define ots_BitManipulator_h

#include <stdint.h>
#include <string>

namespace ots
{
class BitManipulator
{
  public:
	BitManipulator(void);
	~BitManipulator(void);
	static uint64_t insertBits(uint64_t& data, uint64_t value, unsigned int startBit, unsigned int numberOfBits);
	static uint64_t insertBits(std::string& data, uint64_t value, unsigned int startBit, unsigned int numberOfBits);
	static uint64_t reverseBits(uint64_t data, unsigned int startBit, unsigned int numberOfBits);
	static uint32_t insertBits(uint32_t& data, uint32_t value, unsigned int startBit, unsigned int numberOfBits);
	static uint32_t insertBits(std::string& data, uint32_t value, unsigned int startBit, unsigned int numberOfBits);
	static uint32_t reverseBits(uint32_t data, unsigned int startBit, unsigned int numberOfBits);
	static uint32_t readBits(uint32_t data, unsigned int startBit, unsigned int numberOfBits);
};

}  // namespace ots

#endif
