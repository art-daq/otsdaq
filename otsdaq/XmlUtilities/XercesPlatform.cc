#include "otsdaq/XmlUtilities/XercesPlatform.h"

#include <xercesc/util/PlatformUtils.hpp>

#include <mutex>

namespace
{
std::mutex& platformMutex(void)
{
	static std::mutex m;  // one lock for every Initialize/Terminate in the process
	return m;
}
}  // namespace

void ots::XercesPlatform::initialize(void)
{
	std::lock_guard<std::mutex> lock(platformMutex());
	xercesc::XMLPlatformUtils::Initialize();
}

void ots::XercesPlatform::terminate(void)
{
	std::lock_guard<std::mutex> lock(platformMutex());
	xercesc::XMLPlatformUtils::Terminate();
}
