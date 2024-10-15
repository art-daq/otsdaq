#include "otsdaq/SupervisorInfo/SupervisorInfo.h"

using namespace ots;

const std::string SupervisorInfo::APP_STATUS_UNKNOWN       = "UNKNOWN";
const std::string SupervisorInfo::APP_STATUS_NOT_MONITORED = "Not Monitored";

//=====================================================================================
void SupervisorInfo::setStatus(const std::string& status, const unsigned int progress, const std::string& detail)
{
	status_   = status;
	progress_ = progress;
	detail_   = detail;
	if(status != SupervisorInfo::APP_STATUS_UNKNOWN)  // if unknown, then do not consider it a status update
		lastStatusTime_ = time(0);
}  // end setStatus()

//=====================================================================================
void SupervisorInfo::setSubappStatus(const std::string& name, const std::string& status, const unsigned int progress, const std::string& detail)
{
	subapps_[name].name     = name;
	subapps_[name].status   = status;
	subapps_[name].progress = progress;
	subapps_[name].detail   = detail;
	if(status != SupervisorInfo::APP_STATUS_UNKNOWN)  // if unknown, then do not consider it a status update
		subapps_[name].lastStatusTime = time(0);
}  // end setSubappStatus()

//=====================================================================================
void SupervisorInfo::copySubappStatus(const SubappInfo& info)
{
	subapps_[info.name] = info;
}  // end setSubappStatus()

//=====================================================================================
void SupervisorInfo::clear(void)
{
	descriptor_        = 0;
	progress_          = 0;
	contextDescriptor_ = 0;
	name_              = "";
	id_                = 0;
	contextName_       = "";
	status_            = SupervisorInfo::APP_STATUS_UNKNOWN;
}  // end clear()

//=====================================================================================
std::string SupervisorInfo::extractHostname(const std::string& URL)
{
	//__COUTV__(URL);
	size_t i = URL.find("://");
	if(i == std::string::npos)
		i = 0;
	else
		i += 3;
	//__COUTV__(i);
	size_t j = URL.find(":", i);
	if(j != std::string::npos)
		j -= i;
	//__COUTV__(j);
	//__COUTV__(URL.substr(i,j));
	return URL.substr(i, j);
}  // end extractHostname

//=====================================================================================
std::string SupervisorInfo::serializeSubappInfos(std::vector<SubappInfo> infos)
{
	std::ostringstream ostr;
	for(auto& info : infos)
	{
		ostr << info.name << "\n";
		ostr << info.detail << "\n";
		ostr << info.progress << "\n";
		ostr << info.status << "\n";
		ostr << info.lastStatusTime << "\n";
		ostr << info.url << "\n";
		ostr << info.class_name << "\n";
	}
	return ostr.str();
}

//=====================================================================================
std::vector<SupervisorInfo::SubappInfo> SupervisorInfo::deserializeSubappInfos(std::string info_string)
{
	std::vector<SubappInfo> infos;
	std::istringstream      istr(info_string);
	std::string             line;
	while(std::getline(istr, line)) {
		SubappInfo thisInfo;
		thisInfo.name = line;
		std::getline(istr, line);
		thisInfo.detail = line;
		std::getline(istr, line);
		std::istringstream converter(line);
		converter >> thisInfo.progress;
		std::getline(istr, line);
		thisInfo.status = line;
		std::getline(istr, line);
		converter = std::istringstream(line);
		converter >> thisInfo.lastStatusTime;
		std::getline(istr, line);
		thisInfo.url = line;
		std::getline(istr, line);
		thisInfo.class_name = line;
		infos.push_back(thisInfo);
	}

	return infos;
}