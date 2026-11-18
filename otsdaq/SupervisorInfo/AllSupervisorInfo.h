#ifndef _ots_AllSupervisorInfo_h
#define _ots_AllSupervisorInfo_h

#include <map>
#include <mutex> /* for recursive_mutex */
#include <string>
#include <utility>
#include <vector>

#include "otsdaq/SupervisorInfo/SupervisorDescriptorInfoBase.h"
#include "otsdaq/SupervisorInfo/SupervisorInfo.h"

// clang-format off
namespace ots
{
class XDAQContextTable;

////// type define AllSupervisorInfoMap
typedef std::map<unsigned int, const SupervisorInfo&> SupervisorInfoMap;

////// class define
/// AllSupervisorInfo
///	xdaq Supervisors can use this class to gain access to
///	info for all supervisors in the xdaq Context. Supervisors
///	are organized by type/class. Note that if a supervisor is
///	encountered in the xdaq context that is of unknown type, then
///	it is ignored and not organized.
///
///	Supervisors should call init to setup data members of this class.
///
///	This class, when in normal mode, also interprets the active configuration
///	to associate configuration UID/names to the supervisors in the xdaq context.
///	In wizard mode, UID/name is taken from class name.
class AllSupervisorInfo : public SupervisorDescriptorInfoBase
{
  public:
	AllSupervisorInfo	(void);
	AllSupervisorInfo	(xdaq::ApplicationContext* applicationContext);
	~AllSupervisorInfo	(void);

	void 													init								(xdaq::ApplicationContext* applicationContext);
	void 													destroy								(void);
	void 													rebuildFromActiveContext			(xdaq::ApplicationContext* applicationContext); ///<Gateway only, at Startup: register apps new to the active Context, drop the ones that left, rebuild the lists

	/// BOOLs
	bool 													isWizardMode						(void) const { return theWizardInfo_ ? true : false; }
	bool 													isMacroMakerMode					(void) const { return AllSupervisorInfo::MACROMAKER_MODE; }

	/// SETTERs
	void 													setSupervisorStatus					(xdaq::Application* app, const std::string& status, const unsigned int progress = 100, const std::string& detail = "", std::vector<SupervisorInfo::SubappInfo> subapps = {}, const uint64_t availableLogSpaceKB = 0, const uint64_t availableDataSpaceKB = 0, const uint64_t availableVarSpaceKB = 0);
	void 													setSupervisorStatus					(const SupervisorInfo& appInfo, const std::string& status, const unsigned int progress = 100, const std::string& detail = "", std::vector<SupervisorInfo::SubappInfo> subapps = {}, const uint64_t availableLogSpaceKB = 0, const uint64_t availableDataSpaceKB = 0, const uint64_t availableVarSpaceKB = 0);
	void 													setSupervisorStatus					(const unsigned int& id, const std::string& status, const unsigned int progress = 100, const std::string& detail = "", std::vector<SupervisorInfo::SubappInfo> subapps = {}, const uint64_t availableLogSpaceKB = 0, const uint64_t availableDataSpaceKB = 0, const uint64_t availableVarSpaceKB = 0);
	void 													clearSupervisorSubappsStatus		(const SupervisorInfo& appInfo);

	/// GETTERs (so searching and iterating is easier)
	const std::map<unsigned int /* lid */, SupervisorInfo>& getAllSupervisorInfo				(void) const { return allSupervisorInfo_; }
	const SupervisorInfoMap&                                getAllFETypeSupervisorInfo			(void) const { return allFETypeSupervisorInfo_; }
	const SupervisorInfoMap&                                getAllDMTypeSupervisorInfo			(void) const { return allDMTypeSupervisorInfo_; }
	const SupervisorInfoMap&                                getAllLogbookTypeSupervisorInfo		(void) const { return allLogbookTypeSupervisorInfo_; }
	const SupervisorInfoMap&                                getAllMacroMakerTypeSupervisorInfo	(void) const { return allMacroMakerTypeSupervisorInfo_; }
	const std::map<std::string /*hostname*/, const SupervisorInfo&>&   getAllTraceControllerSupervisorInfo	(void) const { return allTraceControllerSupervisorInfo_; }

	const SupervisorInfo& 									getSupervisorInfo					(xdaq::Application* app) const;
	const SupervisorInfo& 									getGatewayInfo						(void) const;
	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* 			getGatewayDescriptor				(void) const;
	const SupervisorInfo&                        			getWizardInfo						(void) const;
	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* 			getWizardDescriptor					(void) const;
	const SupervisorInfo& 									getArtdaqSupervisorInfo				(void) const;

	std::vector<std::vector<const SupervisorInfo*>> 		getOrderedSupervisorDescriptors		(const std::string& stateMachineCommand, bool onlyGatewayContextSupervisors = false) const;
	std::recursive_mutex&									getSupervisorInfoMutex				(unsigned int lid) { return allSupervisorInfoMutex_[lid]; }

	void 													establishStateMachineApplicationSet	(void);
  private:
	typedef std::vector<std::pair<unsigned int /*appId*/, std::map<std::string /*FSM command*/, uint8_t /*priority*/>>> StateMachineApplicationSet;
	static StateMachineApplicationSet							extractStateMachineApplicationSet	(const XDAQContextTable* contextTable);
	void 														organizeSupervisors					(const XDAQContextTable* contextConfig); ///<builds the lists from SupervisorDescriptorInfoBase::allSupervisors_

	StateMachineApplicationSet 									stateMachineApplicationSet_; ///<apps targeted by state machine transitions; set at launch and at each Startup (with the rest of the lists), not when a Context group is activated
	bool 														stateMachineApplicationSetEstablished_ = false;
	mutable std::mutex 											stateMachineApplicationSetMutex_;

	SupervisorInfo* 											theSupervisorInfo_;
	SupervisorInfo* 											theWizardInfo_;
	SupervisorInfo* 											theARTDAQSupervisorInfo_;

	std::map<unsigned int /* lid */, SupervisorInfo>			allSupervisorInfo_;
	std::map<unsigned int /* lid */, std::recursive_mutex> 		allSupervisorInfoMutex_; ///<recursive_mutex so the same thread can lock multiple times (remember to unlock the same amount)
	SupervisorInfoMap 											allFETypeSupervisorInfo_, allDMTypeSupervisorInfo_, allLogbookTypeSupervisorInfo_, allMacroMakerTypeSupervisorInfo_;
	///,
	std::map<std::string /*hostname*/, const SupervisorInfo&> 	allTraceControllerSupervisorInfo_;

	static const bool 											MACROMAKER_MODE;

};

// clang-format off

}  // namespace ots

#endif
