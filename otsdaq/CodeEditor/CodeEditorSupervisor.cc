#include "otsdaq/CodeEditor/CodeEditorSupervisor.h"

using namespace ots;

XDAQ_INSTANTIATOR_IMPL(CodeEditorSupervisor)

//==============================================================================
CodeEditorSupervisor::CodeEditorSupervisor(xdaq::ApplicationStub* stub) : CoreSupervisorBase(stub)
{
	__SUP_COUT__ << "Constructed." << __E__;
}  // end constructor

//==============================================================================
CodeEditorSupervisor::~CodeEditorSupervisor(void)
{
	__SUP_COUT__ << "Destroying..." << __E__;
	// theStateMachineImplementation_ is reset and the object it points to deleted in
	// ~CoreSupervisorBase()
}  // end destructor

//==============================================================================
void CodeEditorSupervisor::defaultPage(xgi::Input* /*in*/, xgi::Output* out)
{
	__SUP_COUT__ << "ApplicationDescriptor LID=" << getApplicationDescriptor()->getLocalId() << __E__;
	*out << "<!DOCTYPE HTML><html lang='en'><frameset col='100%' row='100%'><frame "
	        "src='/WebPath/html/CodeEditor.html?urn="
	     << getApplicationDescriptor()->getLocalId() << "'></frameset></html>";
}  // end defaultPage()

//==============================================================================
// setSupervisorPropertyDefaults
//		override to set defaults for supervisor property values (before user settings
// override)
void CodeEditorSupervisor::setSupervisorPropertyDefaults()
{
	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.UserPermissionsThreshold,
	                                                  std::string() + "*=1 | codeEditor=-1");

}  // end setSupervisorPropertyDefaults()

//==============================================================================
// forceSupervisorPropertyValues
//		override to force supervisor property values (and ignore user settings)
void CodeEditorSupervisor::forceSupervisorPropertyValues()
{
	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.RequireUserLockRequestTypes, "codeEditor");
	CorePropertySupervisorBase::setSupervisorProperty(CorePropertySupervisorBase::SUPERVISOR_PROPERTIES.RequireSecurityRequestTypes, "codeEditor");

}  // end forceSupervisorPropertyValues()

//==============================================================================
//	Request
//		Handles Web Interface requests to Console supervisor.
//		Does not refresh cookie for automatic update checks.
void CodeEditorSupervisor::request(const std::string& requestType, cgicc::Cgicc& cgiIn, HttpXmlDocument& xmlOut, const WebUsers::RequestUserInfo& userInfo)
{
	// Commands:
	// 	codeEditor

	if(requestType == "codeEditor")
	{
		__SUP_COUT__ << "Code Editor" << __E__;

		codeEditor_.xmlRequest(CgiDataUtilities::getData(cgiIn, "option"),
		                       false,  // Indicates Edit mode
		                       cgiIn,
		                       &xmlOut,
		                       userInfo.username_);
	}
	else if(requestType == "readOnlycodeEditor")
	{
		codeEditor_.xmlRequest(CgiDataUtilities::getData(cgiIn, "option"),
		                       true,  // Indicates read only mode
		                       cgiIn,
		                       &xmlOut,
		                       userInfo.username_);
	}
	else
	{
		__SUP_SS__ << "requestType Request, " << requestType << ", not recognized." << __E__;
		__SUP_SS_THROW__;
	}
}  // end request()
