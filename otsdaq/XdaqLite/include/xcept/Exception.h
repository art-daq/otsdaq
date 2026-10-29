// OTS_LITE stand-in for xdaq's xcept/Exception.h.
// Derives from std::exception (not std::runtime_error), as real xcept does, so that
// otsdaq's catch(std::runtime_error) / catch(toolbox::fsm::exception::Exception) ordering holds.
#ifndef _xcept_Exception_h_
#define _xcept_Exception_h_

#include <exception>
#include <string>

namespace xcept
{
class Exception : public std::exception
{
  public:
	Exception(const std::string& name, const std::string& message, const std::string& module = "", int line = 0, const std::string& function = "")
	    : name_(name)
	    , message_(message)
	    , module_(module)
	    , line_(line)
	    , function_(function)
	{
	}
	Exception(const std::string& name, const std::string& message, const std::string& module, int line, const std::string& function, Exception& previous)
	    : name_(name)
	    , message_(message + " <- " + previous.message())
	    , module_(module)
	    , line_(line)
	    , function_(function)
	{
	}
	virtual ~Exception() noexcept {}

	const char* what() const noexcept override { return message_.c_str(); }
	std::string name() const { return name_; }
	std::string message() const { return message_; }
	std::string module() const { return module_; }
	int         line() const { return line_; }
	std::string function() const { return function_; }

  private:
	std::string name_, message_, module_;
	int         line_;
	std::string function_;
};
}  // namespace xcept

#define XCEPT_DECLARE(NAMESPACE, NAME)     \
	namespace NAMESPACE                    \
	{                                      \
	class NAME : public xcept::Exception   \
	{                                      \
	  public:                              \
		using xcept::Exception::Exception; \
	};                                     \
	}

#define XCEPT_RAISE(EXCEPTION, MSG)             throw EXCEPTION(#EXCEPTION, MSG, __FILE__, __LINE__, __FUNCTION__)
#define XCEPT_RETHROW(EXCEPTION, MSG, PREVIOUS) throw EXCEPTION(#EXCEPTION, MSG, __FILE__, __LINE__, __FUNCTION__, PREVIOUS)
#define XCEPT_ASSERT(COND, EXCEPTION, MSG) \
	if(!(COND))                            \
	XCEPT_RAISE(EXCEPTION, MSG)

#endif
