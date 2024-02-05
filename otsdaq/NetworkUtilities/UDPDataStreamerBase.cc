#include "otsdaq/NetworkUtilities/UDPDataStreamerBase.h"
#include "otsdaq/Macros/CoutMacros.h"

#include <iostream>

using namespace ots;

// NOTE: if you want to inherit from this one you MUST initialize
// the Socket Constructor that is commented out here in your class
//==============================================================================
UDPDataStreamerBase::UDPDataStreamerBase(std::string IPAddress, unsigned int port, std::string toIPAddress, unsigned int toPort)
    : TransmitterSocket(IPAddress, port), streamToSocket_(toIPAddress, toPort)
{
	__COUT__ << "IPAddress " << IPAddress << std::endl;
	__COUT__ << "port " << port << std::endl;
	__COUT__ << "toIPAddress " << toIPAddress << std::endl;
	__COUT__ << "toPort " << toPort << std::endl;
	__COUT__ << std::endl;
	__COUT__ << std::endl;
	__COUT__ << std::endl;
	__COUT__ << std::endl;
	__COUT__ << std::endl;

	Socket::initialize();
	__COUT__ << "done!" << std::endl;
}

//==============================================================================
UDPDataStreamerBase::~UDPDataStreamerBase(void) {}
