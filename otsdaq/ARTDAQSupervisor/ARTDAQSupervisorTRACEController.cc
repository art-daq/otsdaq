#include "otsdaq/ARTDAQSupervisor/ARTDAQSupervisorTRACEController.h"
#include <string>

ots::ARTDAQSupervisorTRACEController::ARTDAQSupervisorTRACEController() {}

const ots::ITRACEController::HostTraceLevelMap& ots::ARTDAQSupervisorTRACEController::getTraceLevels()
{
	traceLevelsMap_.clear();  // reset

	ots::ITRACEController::addTraceLevelsForThisHost();
	if(theSupervisor_)
	{
		auto commanders = theSupervisor_->makeCommandersFromProcessInfo();

		for(auto& comm : commanders)
		{
			if(traceLevelsMap_.count(comm.first.host) == 0)
			{
				auto lvlstring = comm.second->send_trace_get("ALL");
				auto lvls      = ARTDAQSupervisor::tokenize_(lvlstring);

				for(auto& lvl : lvls)
				{
					std::istringstream iss(lvl);
					std::string        name;
					uint64_t           lvlM, lvlS, lvlT;

					iss >> name >> lvlM >> lvlS >> lvlT;

					// PREPEND special artdaq signature, so that normal TRACE on those hosts can be modified independently of artdaq processes that are handled
					// by ARTDAQ supervisor.
					traceLevelsMap_["artdaq.." + comm.first.host][name].M = lvlM;
					traceLevelsMap_["artdaq.." + comm.first.host][name].S = lvlS;
					traceLevelsMap_["artdaq.." + comm.first.host][name].T = lvlT;
				}
			}
		}
	}

	return traceLevelsMap_;
}  // end getTraceLevels()

void ots::ARTDAQSupervisorTRACEController::setTraceLevelMask(const std::string& label,
                                                             TraceMasks const&  lvl,
                                                             const std::string& host /*=localhost*/,
                                                             std::string const& mode /*= "ALL"*/)
{
	if(theSupervisor_)
	{
		auto commanders = theSupervisor_->makeCommandersFromProcessInfo();

		bool allMode = mode == "ALL";

		for(auto& comm : commanders)
		{
			// PREPEND special artdaq signature, so that normal TRACE on those hosts can be modified independently of artdaq processes that are handled by
			// ARTDAQ supervisor.
			if("artdaq.." + comm.first.host == host)
			{
				if(allMode || mode == "FAST")
					comm.second->send_trace_set(label, "M", std::to_string(lvl.M));
				if(allMode || mode == "SLOW")
					comm.second->send_trace_set(label, "S", std::to_string(lvl.S));
				if(allMode || mode == "TRIGGER")
					comm.second->send_trace_set(label, "T", std::to_string(lvl.T));
				return;
			}
		}
	}

	ots::ITRACEController::setTraceLevelsForThisHost(label, lvl, mode);
}  // end setTraceLevelMask()
