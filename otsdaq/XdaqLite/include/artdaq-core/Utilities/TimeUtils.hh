// OTS_LITE stand-in for artdaq-core/Utilities/TimeUtils.hh (only what otsdaq uses)
#ifndef artdaq_core_Utilities_TimeUtils_hh
#define artdaq_core_Utilities_TimeUtils_hh

#include <chrono>
#include <ctime>
#include <string>

namespace artdaq
{
namespace TimeUtils
{
inline double GetElapsedTime(std::chrono::steady_clock::time_point const& then, std::chrono::steady_clock::time_point const& now = std::chrono::steady_clock::now())
{
	return std::chrono::duration_cast<std::chrono::duration<double>>(now - then).count();
}
inline double GetElapsedTimeMilliseconds(std::chrono::steady_clock::time_point const& then, std::chrono::steady_clock::time_point const& now = std::chrono::steady_clock::now())
{
	return std::chrono::duration_cast<std::chrono::duration<double, std::milli>>(now - then).count();
}
inline double GetElapsedTimeMicroseconds(std::chrono::steady_clock::time_point const& then, std::chrono::steady_clock::time_point const& now = std::chrono::steady_clock::now())
{
	return std::chrono::duration_cast<std::chrono::duration<double, std::micro>>(now - then).count();
}
inline uint64_t gettimeofday_us()
{
	return std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
}
inline std::string convertUnixTimeToString(time_t t)
{
	char buf[64];
	std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
	return buf;
}
}  // namespace TimeUtils
}  // namespace artdaq

#endif
