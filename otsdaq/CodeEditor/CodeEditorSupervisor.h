#ifndef _ots_CodeEditorSupervisor_h_
#define _ots_CodeEditorSupervisor_h_

#include "otsdaq/CodeEditor/CodeEditor.h"
#include "otsdaq/CoreSupervisors/CoreSupervisorBase.h"

namespace ots
{
// CodeEditorSupervisor
//	This class handles the Code Editor interface
class CodeEditorSupervisor : public CoreSupervisorBase
{
  public:
	XDAQ_INSTANTIATOR();

	CodeEditorSupervisor(xdaq::ApplicationStub* s);
	virtual ~CodeEditorSupervisor(void);

	// CorePropertySupervisorBase override functions
	virtual void defaultPage(xgi::Input* in, xgi::Output* out) override;
	virtual void request(const std::string& requestType, cgicc::Cgicc& cgiIn, HttpXmlDocument& xmlOut, const WebUsers::RequestUserInfo& userInfo) override;

	virtual void setSupervisorPropertyDefaults(void) override;  // override to control supervisor specific defaults
	virtual void forceSupervisorPropertyValues(void) override;  // override to force
	                                                            // supervisor property
	                                                            // values (and ignore user
	                                                            // settings)
  private:
	CodeEditor codeEditor_;
};

}  // namespace ots

#endif
