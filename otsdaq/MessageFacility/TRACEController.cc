#include "otsdaq/MessageFacility/TRACEController.h"

//==============================================================================
const ots::ITRACEController::HostTraceLevelMap& ots::TRACEController::getTraceLevels()
{
	TLOG(TLVL_DEBUG) << "Getting TRACE levels";
	traceLevelsMap_.clear();  // reset
	ots::ITRACEController::addTraceLevelsForThisHost();
	return traceLevelsMap_;
}  // end getTraceLevels()

//==============================================================================
void ots::TRACEController::setTraceLevelMask(std::string const& label, TraceMasks const& lvl, std::string const& hostname, std::string const& mode /*= "ALL"*/)
{
	if(hostname != "localhost" && hostname != getHostnameString())
	{
		TLOG(TLVL_WARNING) << "TRACEController asked to set TRACE levels for host " << hostname << ", but this is " << getHostnameString() << "!";
		return;
	}

	ots::ITRACEController::setTraceLevelsForThisHost(label, lvl, mode);

}  // end setTraceLevelMask()

//==============================================================================
bool ots::TRACEController::getIsTriggered()
{
	traceInit(NULL, 1);
	return static_cast<bool>(traceControl_rwp->triggered);
}  // end getIsTriggered()

//==============================================================================
void ots::TRACEController::setTriggerEnable(size_t entriesAfterTrigger) { TRACE_CNTL("trig", entriesAfterTrigger + 1); }  // end setTriggerEnable()

//==============================================================================
void ots::TRACEController::resetTraceBuffer() { TRACE_CNTL("reset"); }  // end resetTraceBuffer()

//==============================================================================
void ots::TRACEController::enableTrace(bool enable /* = true */)
{
	TRACE_CNTL("modeM", enable ? 1 : 0);
	TRACE_CNTL("modeS", enable ? 1 : 0);
}  // end enableTrace()
