#include "otsdaq/NetworkUtilities/NetworkConverters.h"
#include <arpa/inet.h>
#include <iostream>
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"

using namespace ots;

//==============================================================================
NetworkConverters::NetworkConverters() {}

//==============================================================================
NetworkConverters::~NetworkConverters() {}

//==============================================================================
std::string NetworkConverters::nameToStringIP(const std::string& value)
{
	struct sockaddr_in socketAddress;
	if(inet_aton(value.c_str(), &socketAddress.sin_addr) == 0)
		std::cout << __COUT_HDR_FL__ << "ERROR: Invalid IP address " << value << std::endl;
	return networkToStringIP(socketAddress.sin_addr.s_addr);
}

//==============================================================================
std::string NetworkConverters::stringToNameIP(const std::string& value)
{
	struct sockaddr_in socketAddress;
	socketAddress.sin_addr.s_addr = stringToNetworkIP(value);
	return std::string(inet_ntoa(socketAddress.sin_addr));
}
//==============================================================================
uint32_t NetworkConverters::stringToNetworkIP(const std::string& value) { return unsignedToNetworkIP(stringToUnsignedIP(value)); }

//==============================================================================
std::string NetworkConverters::networkToStringIP(uint32_t value) { return unsignedToStringIP(networkToUnsignedIP(value)); }

//==============================================================================
std::string NetworkConverters::unsignedToStringIP(uint32_t value)
{
	std::string returnValue;
	returnValue += (value >> 24) & 0xff;
	returnValue += (value >> 16) & 0xff;
	returnValue += (value >> 8) & 0xff;
	returnValue += (value)&0xff;
	return returnValue;
}

//==============================================================================
uint32_t NetworkConverters::stringToUnsignedIP(const std::string& value)
{
	return ((((uint32_t)value[0]) & 0xff) << 24) + ((((uint32_t)value[1]) & 0xff) << 16) + ((((uint32_t)value[2]) & 0xff) << 8) + (((uint32_t)value[3]) & 0xff);
}

//==============================================================================
uint32_t NetworkConverters::unsignedToNetworkIP(uint32_t value) { return htonl(value); }

//==============================================================================
uint32_t NetworkConverters::networkToUnsignedIP(uint32_t value) { return ntohl(value); }

//==============================================================================
uint16_t NetworkConverters::stringToNetworkPort(const std::string& value) { return unsignedToNetworkPort(stringToUnsignedPort(value)); }

//==============================================================================
std::string NetworkConverters::networkToStringPort(uint16_t value) { return unsignedToStringPort(networkToUnsignedPort(value)); }

//==============================================================================
std::string NetworkConverters::unsignedToStringPort(uint16_t value)
{
	std::string returnValue;
	returnValue += (value >> 8) & 0xff;
	returnValue += (value)&0xff;
	return returnValue;
}

//==============================================================================
uint16_t NetworkConverters::stringToUnsignedPort(const std::string& value) { return ((((uint32_t)value[0]) & 0xff) << 8) + (((uint32_t)value[1]) & 0xff); }

//==============================================================================
uint16_t NetworkConverters::unsignedToNetworkPort(uint16_t value) { return htons(value); }

//==============================================================================
uint16_t NetworkConverters::networkToUnsignedPort(uint16_t value) { return ntohs(value); }
