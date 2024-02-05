#ifndef ots_CgiDataUtilities_h
#define ots_CgiDataUtilities_h

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#include <xgi/Method.h>  //for cgicc::Cgicc
#pragma GCC diagnostic pop
#include <string>

namespace ots
{
class CgiDataUtilities
{
  public:
	CgiDataUtilities(){};
	~CgiDataUtilities(){};

	static std::string getOrPostData(cgicc::Cgicc& cgi, const std::string& needle);
	static std::string postData(cgicc::Cgicc& cgi, const std::string& needle);
	static std::string getData(cgicc::Cgicc& cgi, const std::string& needle);

	static int getOrPostDataAsInt(cgicc::Cgicc& cgi, const std::string& needle);
	static int postDataAsInt(cgicc::Cgicc& cgi, const std::string& needle);
	static int getDataAsInt(cgicc::Cgicc& cgi, const std::string& needle);

	// decodeURIComponent moved to StringMacros::
	// static std::string decodeURIComponent(const std::string& data);
};

}  // namespace ots

#endif  // ots_CgiDataUtilities_h
