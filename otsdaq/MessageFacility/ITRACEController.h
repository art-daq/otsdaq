#ifndef OTSDAQ_MESSAGEFACILITY_ITRACECONTROLLER_H
#define OTSDAQ_MESSAGEFACILITY_ITRACECONTROLLER_H

#include <limits.h>
#include <unistd.h>
#include <deque>
#include <list>
#include <string>
#include <unordered_map>
#include "TRACE/trace.h"
#include "otsdaq/Macros/StringMacros.h"

#if TRACE_REVNUM < 1394
#define traceLvls_p traceNamLvls_p
#define TRACE_TID2NAME(idx) traceNamLvls_p[idx].name
#endif

namespace ots
{
class ITRACEController
{
  public:
	struct TraceMasks
	{
		uint64_t             M;
		uint64_t             S;
		uint64_t             T;
		friend std::ostream& operator<<(std::ostream& out, const TraceMasks& traceMask)
		{
			out << "Memory:" << traceMask.M << ",Slow:" << traceMask.S << ",Trigger:" << traceMask.T;
			return out;
		}
	};
	typedef std::unordered_map<std::string, TraceMasks>    TraceLevelMap;
	typedef std::unordered_map<std::string, TraceLevelMap> HostTraceLevelMap;

	ITRACEController() {}
	virtual ~ITRACEController() = default;

	virtual const HostTraceLevelMap& getTraceLevels(void)                               = 0;  // pure virtual
	virtual void                     setTraceLevelMask(std::string const& name,
	                                                   TraceMasks const&  lvl,
	                                                   std::string const& hostname = "localhost",
	                                                   std::string const& mode     = "ALL") = 0;  // pure virtual

	virtual bool getIsTriggered(void)                         = 0;  // pure virtual
	virtual void setTriggerEnable(size_t entriesAfterTrigger) = 0;  // pure virtual

	virtual void resetTraceBuffer(void)          = 0;  // pure virtual
	virtual void enableTrace(bool enable = true) = 0;  // pure virtual

	//=====================================
	std::string getTraceBufferDump(std::string const& filterFor = "", std::string const& filterOut = "")
	{
		std::string command = "";  //"trace_cntl show";

		std::vector<std::string> grepArr;
		StringMacros::getVectorFromString(filterFor, grepArr, {','});

		std::string safeGrep = "";
		for(const auto& grepVal : grepArr)
		{
			std::cout << "grepVal = " << grepVal << std::endl;

			if(grepVal.size() < 3)
				continue;

			safeGrep += " | grep \" ";
			for(unsigned int i = 0; i < grepVal.size(); ++i)
				if((grepVal[i] >= 'a' && grepVal[i] <= 'z') || (grepVal[i] >= 'A' && grepVal[i] <= 'Z') || (grepVal[i] >= '0' && grepVal[i] <= '9') ||
				   (grepVal[i] == '.' && i && grepVal[i - 1] != '.') || (grepVal[i] == '-' || grepVal[i] == '_'))
					safeGrep += grepVal[i];
			safeGrep += " \"";
		}
		std::cout << "safeGrep = " << safeGrep << std::endl;

		grepArr.clear();  // reset
		StringMacros::getVectorFromString(filterOut, grepArr, {','});

		for(const auto& grepVal : grepArr)
		{
			std::cout << "grepVal = " << grepVal << std::endl;

			if(grepVal.size() < 3)
				continue;

			safeGrep += " | grep -v \" ";
			for(unsigned int i = 0; i < grepVal.size(); ++i)
				if((grepVal[i] >= 'a' && grepVal[i] <= 'z') || (grepVal[i] >= 'A' && grepVal[i] <= 'Z') || (grepVal[i] >= '0' && grepVal[i] <= '9') ||
				   (grepVal[i] == '.' && i && grepVal[i - 1] != '.') || (grepVal[i] == '-' || grepVal[i] == '_'))
					safeGrep += grepVal[i];
			safeGrep += " \"";
		}
		std::cout << "safeGrep = " << safeGrep << std::endl;

		// command += " | grep \"" + safeGrep + "\"";
		// command += " | tdelta";
		// command += " | test -n \"${PAGER-}\" && trace_delta \"$@\" | $PAGER || trace_delta \"$@\";";
		//  try source $TRACE_BIN/trace_functions.sh; tshow | tdelta
		command += " source $TRACE_BIN/trace_functions.sh; tshow ";
		// command += " | grep Console ";// 2>/dev/null ;
		command += safeGrep;
		command += " | tdelta -d 1 ";
		TLOG(TLVL_DEBUG) << "getTraceBufferDump command: " << command;
		return StringMacros::exec(command.c_str());
	}  // end getTraceBufferDump()

	//=====================================
	std::string getHostnameString(void)
	{
		char hostname_c[HOST_NAME_MAX];
		gethostname(hostname_c, HOST_NAME_MAX);
		return std::string(hostname_c);
	}  // end getHostnameString()

  protected:
	//=====================================
	void addTraceLevelsForThisHost(void)
	{
		auto hostname = getHostnameString();
		TLOG(TLVL_DEBUG) << "Adding TRACE levels [" << hostname << "]";

		unsigned ee = traceControl_p->num_namLvlTblEnts;
		for(unsigned ii = 0; ii < ee; ++ii)
		{
			if(TRACE_TID2NAME(ii)[0])
			{
				std::string name                  = std::string(TRACE_TID2NAME(ii));
				traceLevelsMap_[hostname][name].M = traceLvls_p[ii].M;
				traceLevelsMap_[hostname][name].S = traceLvls_p[ii].S;
				traceLevelsMap_[hostname][name].T = traceLvls_p[ii].T;
			}
		}
	}  // end addTraceLevelsForThisHost()

	//=====================================
	void setTraceLevelsForThisHost(std::string const& name, TraceMasks const& lvl, std::string const& mode = "ALL")
	{
		auto hostname = getHostnameString();
		TLOG(TLVL_DEBUG) << "Setting TRACE levels [" << hostname << "]";

		bool allMode = mode == "ALL";
		TLOG(TLVL_DEBUG) << "Setting " << mode << " levels for name '" << name << "' to " << std::hex << std::showbase << lvl.M << " " << lvl.S << " " << lvl.T;
		if(name != "ALL")
		{
			if(allMode || mode == "FAST")
				TRACE_CNTL("lvlmsknM", name.c_str(), lvl.M);
			if(allMode || mode == "SLOW")
				TRACE_CNTL("lvlmsknS", name.c_str(), lvl.S);
			if(allMode || mode == "TRIGGER")
				TRACE_CNTL("lvlmsknT", name.c_str(), lvl.T);
		}
		else
		{
			if(allMode || mode == "FAST")
				TRACE_CNTL("lvlmskMg", lvl.M);
			if(allMode || mode == "SLOW")
				TRACE_CNTL("lvlmskSg", lvl.S);
			if(allMode || mode == "TRIGGER")
				TRACE_CNTL("lvlmskTg", lvl.T);
		}
	}  // end setTraceLevelsForThisHost()

	HostTraceLevelMap traceLevelsMap_;
};

}  // namespace ots

#endif  // OTSDAQ_MESSAGEFACILITY_ITRACECONTROLLER_H
