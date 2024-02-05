#ifndef _ots_NetworkConverters_h_
#define _ots_NetworkConverters_h_

#include <stdint.h>
#include <string>

namespace ots
{
class NetworkConverters
{
  public:
	NetworkConverters(void);
	~NetworkConverters(void);

	static std::string nameToStringIP(const std::string& value);
	static std::string stringToNameIP(const std::string& value);
	static uint32_t    stringToNetworkIP(const std::string& value);
	static std::string networkToStringIP(uint32_t value);
	static uint32_t    stringToUnsignedIP(const std::string& value);
	static std::string unsignedToStringIP(uint32_t value);
	static uint32_t    unsignedToNetworkIP(uint32_t value);
	static uint32_t    networkToUnsignedIP(uint32_t value);

	static uint16_t    stringToNetworkPort(const std::string& value);
	static std::string networkToStringPort(uint16_t value);
	static uint16_t    stringToUnsignedPort(const std::string& value);
	static std::string unsignedToStringPort(uint16_t value);
	static uint16_t    unsignedToNetworkPort(uint16_t value);
	static uint16_t    networkToUnsignedPort(uint16_t value);
};

}  // namespace ots

#endif
