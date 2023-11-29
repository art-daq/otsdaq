#ifndef _ots_WizardSupervisor_h
#define _ots_WizardSupervisor_h

#include "otsdaq/SOAPUtilities/SOAPMessenger.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <xdaq/Application.h>
#pragma GCC diagnostic pop
#include "otsdaq/Macros/XDAQApplicationMacros.h"

#include <xoap/Method.h>
#include <xoap/SOAPBody.h>
#include <xoap/SOAPEnvelope.h>
#include <xoap/domutils.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#include <cgicc/HTMLClasses.h>
#include <cgicc/HTMLDoctype.h>
#include <cgicc/HTTPCookie.h>
#include <cgicc/HTTPHeader.h>
#include <xgi/Method.h>
#pragma GCC diagnostic pop

#include <map>
#include <string>

#include "otsdaq/SupervisorInfo/AllSupervisorInfo.h"

namespace ots
{
// clang-format off
class HttpXmlDocument;
class ITRACEController;

// WizardSupervisor
//	This class is a xdaq application.
//
//	It is instantiated by the xdaq context when otsdaq is in "Wiz Mode."
//
//	It is different from the "Normal Mode" Gateway Supervisor in that
//	it does not have a state machine and does not inherit properties
//	from CorePropertySupervisorBase. The assumption is that only admins
//	have access to wiz mode, and they have access to all features of it.
class WizardSupervisor : public xdaq::Application, public SOAPMessenger
{
  public:
	XDAQ_INSTANTIATOR();

	WizardSupervisor(xdaq::ApplicationStub*);
	virtual ~WizardSupervisor(void);

	void 				init(void);
	void 				destroy(void);

	void        		generateURL(void);
	static void 		printURL(WizardSupervisor* ptr, std::string securityCode);

	void 				Default(xgi::Input* in, xgi::Output* out);
	void 				verification(xgi::Input* in, xgi::Output* out);
	void 				request(xgi::Input* in, xgi::Output* out);
	void 				requestIcons(xgi::Input* in, xgi::Output* out);

	void        		editSecurity(xgi::Input* in, xgi::Output* out);
	void        		UserSettings(xgi::Input* in, xgi::Output* out);
	void        		tooltipRequest(xgi::Input* in, xgi::Output* out);
	void        		toggleSecurityCodeGeneration(xgi::Input* in, xgi::Output* out);
	std::string 		validateUploadFileType(const std::string fileType);
	void        		cleanUpPreviews();
	void        		savePostPreview(std::string&                        subject,
	                            std::string&                        text,
	                            const std::vector<cgicc::FormFile>& files,
	                            std::string                         creator,
	                            HttpXmlDocument*                    xmldoc = nullptr);

	// External Supervisor XOAP handlers
	xoap::MessageReference 		supervisorSequenceCheck			(xoap::MessageReference message);
	xoap::MessageReference 		supervisorLastTableGroupRequest	(xoap::MessageReference message);


  private:
	std::string              				securityCode_;
	bool                    				defaultSequence_;
	static const std::vector<std::string> 	allowedFileUploadTypes_, matchingFileUploadTypes_;
	static const std::string				WIZ_SUPERVISOR, WIZ_PORT, SERVICE_DATA_PATH;

	std::string 							supervisorClass_;
	std::string 							supervisorClassNoNamespace_;
	AllSupervisorInfo						allSupervisorInfo_;

	enum
	{
		ADMIN_PERMISSIONS_THRESHOLD = 255,
		EXPERIMENT_NAME_MIN_LENTH   = 3,
		EXPERIMENT_NAME_MAX_LENTH   = 25,
		USER_DATA_EXPIRATION_TIME   = 60 * 20,  // 20 minutes
	};
};
// clang-format on
}  // namespace ots

#endif
