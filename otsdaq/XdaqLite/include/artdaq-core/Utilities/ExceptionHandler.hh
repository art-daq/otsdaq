// OTS_LITE stand-in for artdaq-core/Utilities/ExceptionHandler.hh
#ifndef artdaq_core_Utilities_ExceptionHandler_hh
#define artdaq_core_Utilities_ExceptionHandler_hh

#include <string>

namespace artdaq
{
enum class ExceptionHandlerRethrow
{
	yes,
	no
};
// Call from inside a catch block: logs the in-flight exception and optionally rethrows.
void ExceptionHandler(ExceptionHandlerRethrow decision, const std::string& optional_message = "");
}  // namespace artdaq

#endif
