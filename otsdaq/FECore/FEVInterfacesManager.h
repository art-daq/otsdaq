#ifndef _ots_FEVInterfacesManager_h_
#define _ots_FEVInterfacesManager_h_

#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include "otsdaq/Configurable/Configurable.h"
#include "otsdaq/FECore/FEVInterface.h"
#include "otsdaq/FiniteStateMachine/VStateMachine.h"

namespace ots
{
// FEVInterfacesManager
//	This class is a virtual class that handles a collection of front-end interface
// plugins.
class FEVInterfacesManager : public Configurable, public VStateMachine
{
  public:
	FEVInterfacesManager(const ConfigurationTree& theXDAQContextConfigTree, const std::string& supervisorConfigurationPath);
	virtual ~FEVInterfacesManager(void);

	// Methods
	void init(void);
	void destroy(void);
	void createInterfaces(void);

	// State Machine Methods
	virtual void        configure(void) override;
	virtual void        halt(void) override;
	virtual void        pause(void) override;
	virtual void        resume(void) override;
	virtual void        start(std::string runNumber) override;
	virtual void        stop(void) override;
	virtual std::string getStatusProgressDetail(void) override;  // overriding VStateMachine::getStatusProgressDetail

	void        universalRead(const std::string& interfaceID, char* address,
	                          char* returnValue);  // used by MacroMaker
	void        universalWrite(const std::string& interfaceID, char* address,
	                           char* writeValue);                   // used by MacroMaker
	std::string getFEListString(const std::string& supervisorLid);  // used by MacroMaker
	std::string getFEMacrosString(const std::string& supervisorName,
	                              const std::string& supervisorLid);  // used by MacroMaker
	void        runFEMacro(const std::string&                         interfaceID,
	                       const FEVInterface::frontEndMacroStruct_t& feMacro,
	                       const std::string&                         inputArgs,
	                       std::string&                               outputArgs);  // used by MacroMaker and FE calling indirectly
	void        runFEMacro(const std::string& interfaceID,
	                       const std::string& feMacroName,
	                       const std::string& inputArgs,
	                       std::string&       outputArgs);  // used by MacroMaker
	void        runMacro(const std::string& interfaceID,
	                     const std::string& macroObjectString,
	                     const std::string& inputArgs,
	                     std::string&       outputArgs);  // used by MacroMaker
	void        runFEMacroByFE(const std::string& callingInterfaceID,
	                           const std::string& interfaceID,
	                           const std::string& feMacroName,
	                           const std::string& inputArgs,
	                           std::string&       outputArgs);  // used by FE calling (i.e. FESupervisor)
	void        startFEMacroMultiDimensional(const std::string& requester,
	                                         const std::string& interfaceID,
	                                         const std::string& feMacroName,
	                                         const bool         enableSavingOutput,
	                                         const std::string& outputFilePath,
	                                         const std::string& outputFileRadix,
	                                         const std::string& inputArgs);  // used by iterator calling (i.e. FESupervisor)
	void        startMacroMultiDimensional(const std::string& requester,
	                                       const std::string& interfaceID,
	                                       const std::string& macroName,
	                                       const std::string& macroString,
	                                       const bool         enableSavingOutput,
	                                       const std::string& outputFilePath,
	                                       const std::string& outputFileRadix,
	                                       const std::string& inputArgs);  // used by iterator calling (i.e. FESupervisor)
	bool        checkMacroMultiDimensional(const std::string& interfaceID,
	                                       const std::string& macroName);  // used by iterator calling (i.e. FESupervisor)

	unsigned int        getInterfaceUniversalAddressSize(const std::string& interfaceID);  // used by MacroMaker
	unsigned int        getInterfaceUniversalDataSize(const std::string& interfaceID);     // used by MacroMaker
	bool                allFEWorkloopsAreDone(void);                                       // used by Iterator, e.g.
	const FEVInterface& getFEInterface(const std::string& interfaceID) const;

	const std::map<std::string /*name*/, std::unique_ptr<FEVInterface> >& getFEInterfaces(void) const { return theFEInterfaces_; }
	FEVInterface*                                                         getFEInterfaceP(const std::string& interfaceID);

	// FE communication helpers
	std::mutex frontEndCommunicationReceiveMutex_;
	std::map<std::string /*targetInterfaceID*/,  // map of target to buffers organized by
	                                             // source
	         std::map<std::string /*requester*/, std::queue<std::string /*value*/> > >
	    frontEndCommunicationReceiveBuffer_;

	// multi-dimensional FE Macro helpers
	std::mutex macroMultiDimensionalDoneMutex_;
	std::map<std::string /*targetInterfaceID*/,  // set of active multi-dimensional Macro
	                                             // launches
	         std::string /*status := Active, Done, Error: <message> */>
	    macroMultiDimensionalStatusMap_;

  private:
	std::map<std::string /*name*/, std::unique_ptr<FEVInterface> > theFEInterfaces_;
	std::vector<std::string /*name*/>                              theFENamesByPriority_;

	// for managing transition iterations
	std::map<std::string /*name*/, bool /*isDone*/> stateMachinesIterationDone_;
	unsigned int                                    stateMachinesIterationWorkCount_;
	unsigned int                                    subIterationWorkStateMachineIndex_;
	void                                            preStateMachineExecution(unsigned int i, const std::string& transitionName);
	bool                                            postStateMachineExecution(unsigned int i);
	void                                            preStateMachineExecutionLoop(void);
	void                                            postStateMachineExecutionLoop(void);
};

}  // namespace ots

#endif
