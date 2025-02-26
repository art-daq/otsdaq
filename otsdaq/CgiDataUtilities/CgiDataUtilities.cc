#include "otsdaq/CgiDataUtilities/CgiDataUtilities.h"
#include "otsdaq/Macros/CoutMacros.h"

using namespace ots;

//==============================================================================
/// getOrPostData
/// 	return std::string value of needle from get or post std::string
///		post format is expected to be: needle1=value1&needle2=value2...
///	if not found, return ""
std::string CgiDataUtilities::getOrPostData(cgicc::Cgicc& cgi, const std::string& needle)
{
	std::string postData = "";
	if((postData = CgiDataUtilities::postData(cgi, needle)) == "")
		postData = CgiDataUtilities::getData(
		    cgi, needle);  // get command from form, if PreviewEntry
	return postData;
}

//==============================================================================
/// getPostData
/// 	return std::string value of needle from post std::string
///		post format is expected to be: needle1=value1&needle2=value2...
///	if not found, return ""
std::string CgiDataUtilities::postData(cgicc::Cgicc& cgi, const std::string& needle)
{
	std::string postData = "&" + cgi.getEnvironment().getPostData();
	//__COUT__ << "PostData: " + postData << std::endl;
	size_t start_pos = postData.find(
	    "&" + needle +
	    "=");  // add & and = to make sure found field and not part of a value
	if(start_pos == std::string::npos)
		return "";  // needle not found

	size_t end_pos = postData.find('=', start_pos);  // verify = sign
	if(end_pos == std::string::npos)
		return "";  //= not found

	start_pos += needle.length() + 2;         // get past & and field
	end_pos = postData.find('&', start_pos);  // skip needle and = sign
	if(end_pos == std::string::npos)
		end_pos = postData.length();  // not found, so take data to end

	//__COUT__ << "start_pos=" << start_pos
	//		<< "end_pos=" << end_pos << std::endl;
	return postData.substr(start_pos, end_pos - start_pos);  // return value
}

//==============================================================================
/// getData
///	returns "" if not found
///		get query data format is expected to be: needle1=value1&needle2=value2...
std::string CgiDataUtilities::getData(cgicc::Cgicc& cgi, const std::string& needle)
{
	std::string getData = "&" + cgi.getEnvironment().getQueryString();
	//__COUT__ << "getData: " + getData << std::endl;

	size_t start_pos = getData.find(
	    "&" + needle +
	    "=");  // add & and = to make sure found field and not part of a value
	if(start_pos == std::string::npos)
		return "";  // needle not found

	size_t end_pos = getData.find('=', start_pos);  // verify = sign
	if(end_pos == std::string::npos)
		return "";  //= not found

	start_pos += needle.length() + 2;        // get past & and field
	end_pos = getData.find('&', start_pos);  // skip needle and = sign
	if(end_pos != std::string::npos)
		end_pos -= start_pos;  // found, so determine sz of field

	//__COUT__ << "start_pos=" << start_pos << " '" << getData[start_pos] <<
	//			"' end_pos=" << end_pos << " := " << getData.substr(start_pos,end_pos) <<
	// std::endl;

	return getData.substr(start_pos, end_pos);  // return value
}

//==============================================================================
int CgiDataUtilities::getOrPostDataAsInt(cgicc::Cgicc& cgi, const std::string& needle)
{
	return atoi(getOrPostData(cgi, needle).c_str());
}
int CgiDataUtilities::postDataAsInt(cgicc::Cgicc& cgi, const std::string& needle)
{
	return atoi(postData(cgi, needle).c_str());
}
int CgiDataUtilities::getDataAsInt(cgicc::Cgicc& cgi, const std::string& needle)
{
	return atoi(getData(cgi, needle).c_str());
}

//==============================================================================
uint64_t CgiDataUtilities::getOrPostDataAsUint64_t(cgicc::Cgicc& cgi, const std::string& needle)
{
	__COUT_TYPE__(TLVL_DEBUG+20) << __COUT_HDR__ << "getDataAsUint64_t() " << getData(cgi, needle).c_str() << __E__;
	return std::strtoull(getOrPostData(cgi, needle).c_str(),nullptr,10);
}
uint64_t CgiDataUtilities::postDataAsUint64_t(cgicc::Cgicc& cgi, const std::string& needle)
{
	__COUT_TYPE__(TLVL_DEBUG+20) << __COUT_HDR__ << "getDataAsUint64_t() " << getData(cgi, needle).c_str() << __E__;
	return std::strtoull(postData(cgi, needle).c_str(),nullptr,10);
}
uint64_t CgiDataUtilities::getDataAsUint64_t(cgicc::Cgicc& cgi, const std::string& needle)
{
	__COUT_TYPE__(TLVL_DEBUG+20) << __COUT_HDR__ << "getDataAsUint64_t() " << getData(cgi, needle).c_str() << __E__;
	return std::strtoull(getData(cgi, needle).c_str(),nullptr,10);
}


