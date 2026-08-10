#include "otsdaq/SupervisorInfo/SupervisorInfo.h"

using namespace ots;

const std::string SupervisorInfo::APP_STATUS_UNKNOWN       = "UNKNOWN";
const std::string SupervisorInfo::APP_STATUS_NOT_MONITORED = "Not Monitored";

//=====================================================================================
void SupervisorInfo::setStatus(const std::string& status,
                               const unsigned int progress,
                               const std::string& detail,
                               const int64_t      availableLogSpaceKB,
                               const int64_t      availableDataSpaceKB)
{
	/// Note: be careful accessing status_ in multithreaded code (need higher level lock, a la getSupervisorInfoMutex)
	if(status != SupervisorInfo::APP_STATUS_UNKNOWN)
	{
		lastStatusTime_ = time(0);
		if(status_ != status)
			lastStatusChangeTime_ = time(0);
	}
	status_   = status;
	progress_ = progress;
	detail_   = detail;
	SupervisorInfo::emplaceAvailableSpace(availableLogSpaceKB, availableLogSpaceKB_);
	SupervisorInfo::emplaceAvailableSpace(availableDataSpaceKB, availableDataSpaceKB_);
}  // end setStatus()

//=====================================================================================
void SupervisorInfo::setSubappStatus(const std::string& name,
                                     const std::string& status,
                                     const unsigned int progress,
                                     const std::string& detail,
                                     const int64_t      availableLogSpaceKB,
                                     const int64_t      availableDataSpaceKB)
{
	subapps_[name].name        = name;
	if(status !=
	   SupervisorInfo::
	       APP_STATUS_UNKNOWN)  // if unknown, then do not consider it a status update
	{
		subapps_.at(name).lastStatusTime = time(0);
		if(subapps_.at(name).status != status)
			subapps_.at(name).lastStatusChangeTime = time(0);
	}
	subapps_.at(name).status   = status;
	subapps_.at(name).progress = progress;
	subapps_.at(name).detail   = detail;
	SupervisorInfo::emplaceAvailableSpace(availableLogSpaceKB,
	                                      subapps_.at(name).availableLogSpaceKB_);
	SupervisorInfo::emplaceAvailableSpace(availableDataSpaceKB,
	                                      subapps_.at(name).availableDataSpaceKB_);
}  // end setSubappStatus()

//=====================================================================================
void SupervisorInfo::copySubappStatus(const SubappInfo& info)
{
	subapps_[info.name] = info;
}  // end setSubappStatus()

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
		ostr << info.lastStatusChangeTime << "\n";
		ostr << info.url << "\n";
		ostr << info.class_name << "\n";
	}
	return ostr.str();
}  //end serializeSubappInfos()

//=====================================================================================
std::vector<SupervisorInfo::SubappInfo> SupervisorInfo::deserializeSubappInfos(
    std::string info_string)
{
	std::vector<SubappInfo> infos;
	std::istringstream      istr(info_string);
	std::string             line;
	while(std::getline(istr, line))
	{
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
		converter = std::istringstream(line);
		converter >> thisInfo.lastStatusChangeTime;
		std::getline(istr, line);
		thisInfo.url = line;
		std::getline(istr, line);
		thisInfo.class_name = line;
		infos.push_back(thisInfo);
	}

	return infos;
}  //end deserializeSubappInfos()

//=====================================================================================
/// Keep only closest times to series of now, 3.75 minutes, 7.5, 15, 30, 60 minutes
/// Respective index in deque: now=[0], 3.75=[3], 15=[5], 30=[7], 60=[9]
void SupervisorInfo::emplaceAvailableSpace(
    const int64_t                           availableSpaceKB,
    std::deque<std::pair<time_t, int64_t>>& availableSpaceDeque)
{
	if(availableSpaceKB > 0)  //only insert valid values
	{
		__GEN_COUTVS__(40, availableSpaceKB);
		time_t now = time(0);
		//newer values at front
		availableSpaceDeque.emplace_front(now, availableSpaceKB);  // to position 0

		//keep closest to 3.75 without going over for next 2 values
		if(availableSpaceDeque.size() > 2)
		{
			if(now - availableSpaceDeque.at(2).first > 225)  //3.75 minutes
				availableSpaceDeque.erase(availableSpaceDeque.begin() + 2);
			else  //erase newer value
				availableSpaceDeque.erase(availableSpaceDeque.begin() + 1);

			// fill with values up to 10 to init the deque
			while(availableSpaceDeque.size() < 10)
				availableSpaceDeque.emplace_back(availableSpaceDeque.at(1));
		}

		//now starting at position 2, keep 2 values per interval:
		//	the +1 is always best, if over interval by too much, then replace with newer value
		//  and take next best value from shorter interval
		for(size_t i = 8; i > 0 && i + 1 < availableSpaceDeque.size(); i -= 2)
		{
			if(now - availableSpaceDeque.at(i + 1).first >
			   (1 << (i / 2)) * 225 + (1 << (i / 2)) * 225 / 2)  //1.5x interval
			{
				//too old, so replace with newer value
				availableSpaceDeque[i + 1] = availableSpaceDeque[i];
				availableSpaceDeque[i]     = availableSpaceDeque[i - 1];
			}
			if(availableSpaceDeque[i + 1].first == availableSpaceDeque[i].first &&
			   availableSpaceDeque[i].first != availableSpaceDeque[i - 1].first)
			{
				//same time, so shift up newer value
				availableSpaceDeque[i] = availableSpaceDeque[i - 1];
			}
		}  //end main loop

		if(TTEST(1))
		{
			__SS__ << "Available space deque: ";
			size_t i = 0;
			for(auto& val : availableSpaceDeque)
			{
				ss << i << ":(t=" << val.first << ", KBs=" << val.second
				   << ", dt=" << now - val.first << " < "
				   << ((1 << (i / 2)) * 225 + (1 << (i / 2)) * 225 / 2) << ", KBps="
				   << (val.second - availableSpaceKB) * 1.0f / (1 + now - val.first)
				   << ") ";
				++i;
			}
			__COUTS__(40) << mfSubject_ << " " << &availableSpaceDeque << " " << ss.str()
			              << __E__ << getLogUsageRateLastHourKBps() << " KB/s";
		}
	}
}  //end emplaceAvailableSpace()
