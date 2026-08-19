#include "otsdaq/CoreSupervisors/CoreSupervisorBase.h"

#include <iostream>

using namespace ots;

// XDAQ_INSTANTIATOR_IMPL(CoreSupervisorBase)

const std::string CoreSupervisorBase::WORK_LOOP_DONE    = "Done";
const std::string CoreSupervisorBase::WORK_LOOP_WORKING = "Working";

//==============================================================================
CoreSupervisorBase::CoreSupervisorBase(xdaq::ApplicationStub* stub)
    : xdaq::Application(stub)
    , SOAPMessenger(this)
    , CorePropertySupervisorBase(this)
    , RunControlStateMachine(
          CorePropertySupervisorBase::allSupervisorInfo_.isWizardMode()
              ?  // set state machine name
              CorePropertySupervisorBase::supervisorClassNoNamespace_
              : CorePropertySupervisorBase::supervisorClassNoNamespace_ + ":" +
                    CorePropertySupervisorBase::getSupervisorUID())
    , stateMachineWorkLoopManager_(toolbox::task::bind(
          this, &CoreSupervisorBase::stateMachineThread, "StateMachine"))
    , stateMachineSemaphore_(toolbox::BSem::FULL)
    , theRemoteWebUsers_(this,
                         CorePropertySupervisorBase::getGatewaySupervisorDescriptor())
{
	__SUP_COUT__ << "Constructor." << __E__;

	INIT_MF("." /*directory used is USER_DATA/LOG/.*/);

	xgi::bind(this, &CoreSupervisorBase::defaultPageWrapper, "Default");
	xgi::bind(this, &CoreSupervisorBase::requestWrapper, "Request");

	xgi::bind(
	    this, &CoreSupervisorBase::stateMachineXgiHandler, "StateMachineXgiHandler");

	xoap::bind(this,
	           &CoreSupervisorBase::stateMachineStateRequest,
	           "StateMachineStateRequest",
	           XDAQ_NS_URI);
	xoap::bind(this,
	           &CoreSupervisorBase::stateMachineErrorMessageRequest,
	           "StateMachineErrorMessageRequest",
	           XDAQ_NS_URI);
	xoap::bind(this,
	           &CoreSupervisorBase::workLoopStatusRequestWrapper,
	           "WorkLoopStatusRequest",
	           XDAQ_NS_URI);
	xoap::bind(this,
	           &CoreSupervisorBase::applicationStatusRequest,
	           "ApplicationStatusRequest",
	           XDAQ_NS_URI);
	xoap::bind(this,
	           &CoreSupervisorBase::TRACESupervisorRequest,
	           "TRACESupervisorRequest",
	           XDAQ_NS_URI);
	xoap::bind(this,
	           &CoreSupervisorBase::minReadyForEventGenerationStartIterationRequest,
	           "MinReadyForEventGenerationStartIterationRequest",
	           XDAQ_NS_URI);
	xoap::bind(this,
	           &CoreSupervisorBase::minReadyForEventGenerationStartIterationRequest,
	           "MinReadyForEventGenerationStartIterationRequest",
	           XDAQ_NS_URI);

	__SUP_COUT__ << "Constructed. getpid()=" << getpid() << " gettid()=" << gettid()
	             << __E__;
}  // end constructor

//==============================================================================
CoreSupervisorBase::~CoreSupervisorBase(void)
{
	__SUP_COUT__ << "Destructor." << __E__;
	destroy();
	__SUP_COUT__ << "Destructed." << __E__;
}  // end destructor()

//==============================================================================
void CoreSupervisorBase::destroy(void)
{
	__SUP_COUT__ << "Destroying..." << __E__;
	for(auto& it : theStateMachineImplementation_)
		delete it;

	theStateMachineImplementation_.clear();
}  // end destroy()

//==============================================================================
/// wrapper for inheritance call
void CoreSupervisorBase::defaultPageWrapper(xgi::Input* in, xgi::Output* out)
{
	return defaultPage(in, out);
}

//==============================================================================
void CoreSupervisorBase::defaultPage(xgi::Input* /*in*/, xgi::Output* out)
{
	__SUP_COUT__ << "Supervisor class " << supervisorClass_ << __E__;

	std::stringstream pagess;
	pagess << "/WebPath/html/" << supervisorClassNoNamespace_
	       << ".html?urn=" << this->getApplicationDescriptor()->getLocalId();

	__SUP_COUT__ << "Default page = " << pagess.str() << __E__;

	*out << "<!DOCTYPE HTML><html lang='en'><frameset col='100%' row='100%'><frame src='"
	     << pagess.str() << "'></frameset></html>";
}  // end defaultPage()

//==============================================================================
/// requestWrapper ~
///	wrapper for inheritance Supervisor request call
void CoreSupervisorBase::requestWrapper(xgi::Input* in, xgi::Output* out)
try
{
	out->getHTTPResponseHeader().addHeader(
	    "Access-Control-Allow-Origin",
	    "*");  // to avoid block by blocked by CORS policy of browser
	out->getHTTPResponseHeader().addHeader("Pragma", "no-cache");

	cgicc::Cgicc cgiIn(in);
	std::string  requestType = CgiDataUtilities::getData(cgiIn, "RequestType");

	__SUP_COUTT__ << "requestType " << requestType
	              << " files: " << cgiIn.getFiles().size() << __E__;

	HttpXmlDocument           xmlOut;
	WebUsers::RequestUserInfo userInfo(
	    requestType, CgiDataUtilities::getOrPostData(cgiIn, "CookieCode"));

	CorePropertySupervisorBase::getRequestUserInfo(userInfo);

	if(!theRemoteWebUsers_.xmlRequestToGateway(
	       cgiIn, out, &xmlOut, CorePropertySupervisorBase::allSupervisorInfo_, userInfo))
		return;  // access failed

	if(requestType == "GetUserDisplayName")
	{
		__COUTV__(userInfo.displayName_);
		xmlOut.addTextElementToData("DisplayName", userInfo.displayName_);
		xmlOut.outputXmlDocument((std::ostringstream*)out,
		                         false /*print to cout*/,
		                         !userInfo.NoXmlWhiteSpace_ /*allow whitespace*/);
		return;
	}

	// done checking cookieCode, sequence, userWithLock, and permissions access all in one
	// shot!
	//**** end LOGIN GATEWAY CODE ***//

	if(!userInfo.automatedCommand_)
		__SUP_COUT__ << "DIAG ms=" << StringMacros::nowEpochMs()
		             << " requestType: " << requestType << __E__;

	if(userInfo.NonXMLRequestType_)
	{
		try
		{
			nonXmlRequest(requestType, cgiIn, *out, userInfo);
		}
		catch(const std::runtime_error& e)
		{
			__SUP_SS__ << "An error was encountered handling requestType '" << requestType
			           << "':" << e.what() << __E__;
			__SUP_COUT_ERR__ << "\n" << ss.str();
		}
		catch(...)
		{
			__SUP_SS__ << "An unknown error was encountered handling requestType '"
			           << requestType << ".' "
			           << "Please check the printouts to debug." << __E__;
			try
			{
				throw;
			}  //one more try to printout extra info
			catch(const std::exception& e)
			{
				ss << "Exception message: " << e.what();
			}
			catch(...)
			{
			}
			__SUP_COUT_ERR__ << "\n" << ss.str();
		}
		return;
	}
	// else xml request type

	std::chrono::steady_clock::time_point requestStart = std::chrono::steady_clock::now();
	time_t                                requestStartTime = time(0);

	try
	{
		// call derived class' request()
		request(requestType, cgiIn, xmlOut, userInfo);
	}
	catch(const std::runtime_error& e)
	{
		__SUP_SS__ << "An error was encountered handling requestType '" << requestType
		           << "':" << e.what() << __E__;
		__SUP_COUT_ERR__ << "\n" << ss.str();
		xmlOut.addTextElementToData("Error", ss.str());
	}
	catch(...)
	{
		__SUP_SS__ << "An unknown error was encountered handling requestType '"
		           << requestType << ".' "
		           << "Please check the printouts to debug." << __E__;
		try
		{
			throw;
		}  //one more try to printout extra info
		catch(const std::exception& e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...)
		{
		}
		__SUP_COUT_ERR__ << "\n" << ss.str();
		xmlOut.addTextElementToData("Error", ss.str());
	}
	__SUP_COUTT__ << "Request '" << requestType
	              << "' time: " << artdaq::TimeUtils::GetElapsedTime(requestStart)
	              << __E__;

	// used to print xml errors only if there are a reasonable number of children
	if(TTEST(1))  //now just check child size if debugging
	{
		size_t numberOfChildren =
		    xmlOut.getRootDataElement()->getChildNodes()->getLength();
		if(numberOfChildren &&
		   xmlOut.getRootDataElement()->getChildNodes()->item(0)->getNodeType() !=
		       xercesc::DOMNode::TEXT_NODE)
			numberOfChildren += xmlOut.getRootDataElement()
			                        ->getChildNodes()
			                        ->item(0)
			                        ->getChildNodes()
			                        ->getLength();

		__SUP_COUTT__ << "Number of '" << requestType
		              << "' xml data element children: " << numberOfChildren << __E__;

		__SUP_COUTT__ << "Request '" << requestType
		              << "' time: " << artdaq::TimeUtils::GetElapsedTime(requestStart)
		              << __E__;
	}
	//Note: the above XML-children-count only looks at depth 1, if there are too many nodes (including deeper nodes) then outputing the xml will be slow (4K nodes takes ~2 seconds)

	// return xml doc holding server response to request
	xmlOut.outputXmlDocument((std::ostringstream*)out,
	                         false /*print to cout*/,
	                         !userInfo.NoXmlWhiteSpace_ /*allow whitespace*/,
	                         true /* printErrors */);  // report any errors encountered

	if(!userInfo.automatedCommand_)
		__SUP_COUT__ << "DIAG ms=" << StringMacros::nowEpochMs()
		             << " requestType: " << requestType << " response written, elapsed="
		             << artdaq::TimeUtils::GetElapsedTime(requestStart) << "s" << __E__;

	// __SUP_COUTT__ << "Request '" << requestType
	//               << "' time: " << artdaq::TimeUtils::GetElapsedTime(requestStart)
	//               << __E__;
	// std::stringstream oss;
	// xmlOut.outputXmlDocument((std::ostringstream*)&oss,
	//                          false /*print to cout*/,
	//                          !userInfo.NoXmlWhiteSpace_ /*allow whitespace*/,
	// 						 true /* printErrors */);
	// __SUP_COUTT__ << "Request '" << requestType
	//               << "' time: " << artdaq::TimeUtils::GetElapsedTime(requestStart)
	//               << __E__;
	// __SUP_COUTV__(oss.str());

	__SUP_COUTT__ << "Total '" << requestType << "' xml request time: "
	              << artdaq::TimeUtils::GetElapsedTime(requestStart) << " = "
	              << time(0) - requestStartTime << __E__;
}  // end requestWrapper()
catch(const std::runtime_error& e)
{
	__SUP_SS__ << "An error was encountered handling HTTP request:" << e.what() << __E__;
	__SUP_COUT_ERR__ << "\n" << ss.str();
	throw;
}
catch(...)
{
	__SUP_SS__ << "An unknown error was encountered HTTP request. "
	           << "Please check the printouts to debug." << __E__;
	try
	{
		throw;
	}  //one more try to printout extra info
	catch(const std::exception& e)
	{
		ss << "Exception message: " << e.what();
	}
	catch(...)
	{
	}
	__SUP_COUT_ERR__ << "\n" << ss.str();
	throw;
}  // end requestWrapper() error handling

//==============================================================================
/// request
///		Supervisors should override this function. It will be called after user access has
/// been verified 		according to the Supervisor Property settings. The
/// CoreSupervisorBase class provides consistent 		access, responses, and error
/// handling  across all inheriting supervisors that use ::request.
void CoreSupervisorBase::request(const std::string& /*requestType*/,
                                 cgicc::Cgicc& /*cgiIn*/,
                                 HttpXmlDocument& xmlOut,
                                 const WebUsers::RequestUserInfo& /*userInfo*/)
{
	__SUP_SS__ << "This is the empty Core Supervisor request. Supervisors should "
	              "override this function."
	           << __E__;
	__SUP_COUT__ << ss.str();
	xmlOut.addTextElementToData("Error", ss.str());
	return;

	// KEEP:
	//	here are some possibly interesting example lines of code for overriding
	// supervisors
	//
	// try
	//{
	//
	//  if(requestType == "savePlanCommandSequence")
	//	{
	//		std::string 	planName 		= CgiDataUtilities::getData(cgiIn,"planName");
	////from  GET 		std::string 	commands 		=
	// CgiDataUtilities::postData(cgiIn,"commands");
	////from POST
	//
	//		cgiIn.getFiles()
	//		__SUP_COUT__ << "planName: " << planName << __E__;
	//		__SUP_COUTV__(commands);
	//
	//
	//	}
	//	else
	//	{
	//		__SUP_SS__ << "requestType '" << requestType << "' request not recognized." <<
	// __E__;
	//		__SUP_SS_THROW__;
	//	}
	//	xmlOut.addTextElementToData("Error",
	//			"request encountered an error!");
	//}
	//	catch(const std::runtime_error& e)
	//	{
	//		__SUP_SS__ << "An error was encountered handling requestType '" << requestType
	//<< "':" << 				e.what() << __E__;
	//		__SUP_COUT_ERR__ << "\n" << ss.str();
	//		xmlOut.addTextElementToData("Error", ss.str());
	//	}
	//	catch(...)
	//	{
	//		__SUP_SS__ << "An unknown error was encountered handling requestType '" <<
	// requestType << ".' " << 				"Please check the printouts to debug." <<
	//__E__;
	//		__SUP_COUT_ERR__ << "\n" << ss.str();
	//		xmlOut.addTextElementToData("Error", ss.str());
	//	}
	// END KEEP.

}  // end request()

//==============================================================================
/// nonXmlRequest
///		Supervisors should override this function. It will be called after user access has
/// been verified 		according to the Supervisor Property settings. The
/// CoreSupervisorBase class provides consistent 		access, responses, and error
/// handling  across all inheriting supervisors that use ::request.
void CoreSupervisorBase::nonXmlRequest(const std::string& /*requestType*/,
                                       cgicc::Cgicc& /*cgiIn*/,
                                       std::ostream& out,
                                       const WebUsers::RequestUserInfo& /*userInfo*/)
{
	__SUP_COUT__ << "This is the empty Core Supervisor non-xml request. Supervisors "
	                "should override this function."
	             << __E__;
	out << "This is the empty Core Supervisor non-xml request. Supervisors should "
	       "override this function."
	    << __E__;
}  // end nonXmlRequest()

//==============================================================================
void CoreSupervisorBase::stateMachineXgiHandler(xgi::Input* /*in*/, xgi::Output* /*out*/)
{
}

//==============================================================================
xoap::MessageReference CoreSupervisorBase::stateMachineXoapHandler(
    xoap::MessageReference message)
{
	__SUP_COUT__ << "Soap Handler!" << __E__;
	stateMachineWorkLoopManager_.removeProcessedRequests();
	stateMachineWorkLoopManager_.processRequest(message);
	__SUP_COUT__ << "Done - Soap Handler!" << __E__;
	return message;
}  // end stateMachineXoapHandler()

//==============================================================================
/// indirection to allow for overriding handler
xoap::MessageReference CoreSupervisorBase::workLoopStatusRequestWrapper(
    xoap::MessageReference message)
{
	// this should have an override for monitoring work loops being done
	return workLoopStatusRequest(message);
}  // end workLoopStatusRequest()

//==============================================================================
xoap::MessageReference CoreSupervisorBase::workLoopStatusRequest(
    xoap::MessageReference /*message*/)
{
	// this should have an override for monitoring work loops being done
	return SOAPUtilities::makeSOAPMessageReference(CoreSupervisorBase::WORK_LOOP_DONE);
}  // end workLoopStatusRequest()

//==============================================================================
xoap::MessageReference CoreSupervisorBase::applicationStatusRequest(
    xoap::MessageReference /*message*/)
{
	// send back status and progress parameters

	__SUP_COUTVS__(20,
	               std::to_string(getpid()) + ":" + std::to_string(gettid()) + ":" +
	                   theStateMachine_.getCurrentStateName());

	const std::string& err = theStateMachine_.getErrorMessage();
	// std::string status = err == "" ? (theStateMachine_.isInTransition() ? theStateMachine_.getProvenanceStateName() : theStateMachine_.getCurrentStateName())
	//                                : (theStateMachine_.getCurrentStateName() == "Paused" ? "Soft-Error:::" : "Error:::") + err;

	// __SUP_COUTV__(theStateMachine_.isInTransition());
	// __SUP_COUTV__(theStateMachine_.getProvenanceStateName());
	// __SUP_COUTV__(theStateMachine_.getCurrentStateName());
	// __SUP_COUTV__(RunControlStateMachine::theProgressBar_.readPercentageString());

	SOAPParameters retParameters;
	if(err == "")
	{
		if(theStateMachine_.isInTransition() ||
		   RunControlStateMachine::theProgressBar_.read() < 100)
		{
			// attempt to get transition name, otherwise give provenance state
			try
			{
				retParameters.addParameter("Status",
				                           theStateMachine_.getCurrentTransitionName());
			}
			catch(...)
			{
				retParameters.addParameter("Status",
				                           theStateMachine_.getProvenanceStateName());
			}
		}
		else
			retParameters.addParameter("Status", theStateMachine_.getCurrentStateName());
	}
	else
		retParameters.addParameter(
		    "Status",
		    (theStateMachine_.getCurrentStateName() == "Paused" ? "Soft-Error:::"
		                                                        : "Error:::") +
		        err);

	retParameters.addParameter(
	    "Progress", RunControlStateMachine::theProgressBar_.readPercentageString());
	retParameters.addParameter(
	    "Detail",
	    getStatusProgressDetail());  // call virtual progress detail string generation

	//return available disk space if first app in context
	__SUP_COUTVS__(19, CorePropertySupervisorBase::isFirstAppInContext());
	if(CorePropertySupervisorBase::isFirstAppInContext())
	{
		uint64_t availableLogSpaceKB =
		    CorePropertySupervisorBase::getAvailableLogSpaceKB();
		uint64_t availableDataSpaceKB =
		    CorePropertySupervisorBase::getAvailableDataSpaceKB();

		retParameters.addParameter("AvailableLogSpaceKB",
		                           std::to_string(availableLogSpaceKB));
		retParameters.addParameter("AvailableDataSpaceKB",
		                           std::to_string(availableDataSpaceKB));
	}
	else  // just return 0 since not responsible for disk space
	{
		retParameters.addParameter("AvailableLogSpaceKB", "0");
		retParameters.addParameter("AvailableDataSpaceKB", "0");
	}

	auto subappInfo = getSubappInfo();
	retParameters.addParameter("Subapps",
	                           SupervisorInfo::serializeSubappInfos(subappInfo));

	return SOAPUtilities::makeSOAPMessageReference("applicationStatusRequestReply",
	                                               retParameters);
}  // end applicationStatusRequest()

//==============================================================================
xoap::MessageReference
CoreSupervisorBase::minReadyForEventGenerationStartIterationRequest(
    xoap::MessageReference /*message*/)
{
	unsigned int maxIteration = 0;
	for(const auto& fsm : theStateMachineImplementation_)
	{
		unsigned int val = fsm->getMinReadyForEventGenerationStartIteration();
		if(val > maxIteration)
			maxIteration = val;
	}

	__SUP_COUT__ << "MinReadyForEventGenerationStartIteration (aggregated max) = "
	             << maxIteration << __E__;

	SOAPParameters retParameters;
	retParameters.addParameter("MinIteration", std::to_string(maxIteration));
	return SOAPUtilities::makeSOAPMessageReference(
	    "MinReadyForEventGenerationStartIterationRequestReply", retParameters);
}  // end minReadyForEventGenerationStartIterationRequest()

//==============================================================================
/// virtual progress string that can be overridden with more info
///	e.g. steps and sub-steps
///	however integer 0-100 should be first number, then separated by : colons
///	e.g. 94:FE0:1:2
std::string CoreSupervisorBase::getStatusProgressDetail(void)
{
	std::string  detail;
	unsigned int cnt = 0;

	/*
	//__SUP_COUT__ << "Checking..." << CoreSupervisorBase::theStateMachineImplementation_.size() << __E__;
	for(const auto& fsm : CoreSupervisorBase::theStateMachineImplementation_)
	{
	    //__SUP_COUT__ << "Checking..." << __E__;
	    try
	    {
	        VStateMachine* testFSM = dynamic_cast<VStateMachine*>(fsm);
	    }
	    catch(...)
	    {
	        __SUP_COUT__ << "getStatusProgressDetail() VStateMachine testFSM Failed..." << __E__;
	        throw;
	    }
	}
	*/

	if(!theStateMachine_.isInTransition() &&
	   (theStateMachine_.getCurrentStateName() ==
	        RunControlStateMachine::HALTED_STATE_NAME ||
	    theStateMachine_.getCurrentStateName() ==
	        RunControlStateMachine::INITIAL_STATE_NAME))
	{
		detail = std::string("Uptime: ") +
		         StringMacros::encodeURIComponent(StringMacros::getTimeDurationString(
		             CorePropertySupervisorBase::getSupervisorUptime())) +
		         ", Time-in-state: " +
		         StringMacros::encodeURIComponent(StringMacros::getTimeDurationString(
		             theStateMachine_.getTimeInState()));
		return detail;
	}

	for(const auto& fsm : CoreSupervisorBase::theStateMachineImplementation_)
	{
		std::string fsmProgressDetail = fsm->getStatusProgressDetail();
		if(fsmProgressDetail.size())
			detail +=
			    ((cnt++) ? ":" : "") +
			    fsmProgressDetail;  // StringMacros::encodeURIComponent(fsmProgressDetail);
	}

	if(detail.size())
		__SUP_COUTVS__(20, detail);

	// if empty detail, give last command
	if(!detail.size() && RunControlStateMachine::getLastCommand() != "")
	{
		detail = "Last Command: " + RunControlStateMachine::getLastCommand();
		if(RunControlStateMachine::getLastCommand() ==
		   RunControlStateMachine::CONFIGURE_TRANSITION_NAME)
			detail += " w/" + RunControlStateMachine::getLastAttemptedConfigureGroup();
	}

	return detail;
}  // end getStatusProgressDetail()

//==============================================================================
bool CoreSupervisorBase::stateMachineThread(toolbox::task::WorkLoop* workLoop)
{
	stateMachineSemaphore_.take();
	__SUP_COUT__ << "Re-sending message..."
	             << SOAPUtilities::translate(
	                    stateMachineWorkLoopManager_.getMessage(workLoop))
	                    .getCommand()
	             << __E__;
	std::string reply = send(this->getApplicationDescriptor(),
	                         stateMachineWorkLoopManager_.getMessage(workLoop));
	stateMachineWorkLoopManager_.report(workLoop, reply, 100, true);
	__SUP_COUT__ << "Done with message" << __E__;
	stateMachineSemaphore_.give();
	return false;  // execute once and automatically remove the workloop so in
	               // WorkLoopManager the try workLoop->remove(job_) could be commented
	               // out return true;//go on and then you must do the
	               // workLoop->remove(job_) in WorkLoopManager
}  // end stateMachineThread()

//==============================================================================
xoap::MessageReference CoreSupervisorBase::stateMachineStateRequest(
    xoap::MessageReference /*message*/)
{
	__SUP_COUT__ << "theStateMachine_.getCurrentStateName() = "
	             << theStateMachine_.getCurrentStateName() << __E__;
	return SOAPUtilities::makeSOAPMessageReference(
	    theStateMachine_.getCurrentStateName());
}

//==============================================================================
xoap::MessageReference CoreSupervisorBase::stateMachineErrorMessageRequest(
    xoap::MessageReference /*message*/)

{
	__SUP_COUT__ << "theStateMachine_.getErrorMessage() = "
	             << theStateMachine_.getErrorMessage() << __E__;

	SOAPParameters retParameters;
	retParameters.addParameter("ErrorMessage", theStateMachine_.getErrorMessage());
	return SOAPUtilities::makeSOAPMessageReference("stateMachineErrorMessageRequestReply",
	                                               retParameters);
}  // end stateMachineErrorMessageRequest()

//==============================================================================
void CoreSupervisorBase::stateInitial(toolbox::fsm::FiniteStateMachine& /*fsm*/)
{
	__SUP_COUT__ << "CoreSupervisorBase::stateInitial" << __E__;
}

//==============================================================================
void CoreSupervisorBase::stateHalted(toolbox::fsm::FiniteStateMachine& /*fsm*/)
{
	__SUP_COUT__ << "CoreSupervisorBase::stateHalted" << __E__;
}

//==============================================================================
void CoreSupervisorBase::stateRunning(toolbox::fsm::FiniteStateMachine& /*fsm*/)
{
	__SUP_COUT__ << "CoreSupervisorBase::stateRunning" << __E__;
}

//==============================================================================
void CoreSupervisorBase::stateConfigured(toolbox::fsm::FiniteStateMachine& /*fsm*/)
{
	__SUP_COUT__ << "CoreSupervisorBase::stateConfigured" << __E__;
}

//==============================================================================
void CoreSupervisorBase::statePaused(toolbox::fsm::FiniteStateMachine& /*fsm*/)
{
	__SUP_COUT__ << "CoreSupervisorBase::statePaused" << __E__;
}

//==============================================================================
void CoreSupervisorBase::inError(toolbox::fsm::FiniteStateMachine& /*fsm*/)

{
	__SUP_COUT__ << "Fsm current state: " << theStateMachine_.getCurrentStateName()
	             << __E__;
	// rcmsStateNotifier_.stateChanged("Error", "");
}

//==============================================================================
void CoreSupervisorBase::enteringError(toolbox::Event::Reference event)

{
	//	__SUP_COUT__<< "Fsm current state: " << theStateMachine_.getCurrentStateName()
	//			<< "\n\nError Message: " <<
	//			theStateMachine_.getErrorMessage() << __E__;
	toolbox::fsm::FailedEvent& failedEvent =
	    dynamic_cast<toolbox::fsm::FailedEvent&>(*event);
	std::ostringstream error;
	error << "Failure performing transition from " << failedEvent.getFromState() << " to "
	      << failedEvent.getToState()
	      << " exception: " << failedEvent.getException().what();
	__SUP_COUT_ERR__ << error.str() << __E__;
	// diagService_->reportError(errstr.str(),DIAGERROR);
}

//==============================================================================
void CoreSupervisorBase::preStateMachineExecutionLoop(void)
{
	RunControlStateMachine::clearIterationWork();
	RunControlStateMachine::clearSubIterationWork();

	stateMachinesIterationWorkCount_ = 0;

	if(RunControlStateMachine::getIterationIndex() == 0 &&
	   RunControlStateMachine::getSubIterationIndex() == 0)
	{
		// reset vector for iterations done on first iteration

		subIterationWorkStateMachineIndex_ = -1;  // clear sub iteration work index

		stateMachinesIterationDone_.resize(theStateMachineImplementation_.size());
		for(unsigned int i = 0; i < stateMachinesIterationDone_.size(); ++i)
			stateMachinesIterationDone_[i] = false;
	}
	else
		__SUP_COUT__ << "Iteration " << RunControlStateMachine::getIterationIndex() << "."
		             << RunControlStateMachine::getSubIterationIndex() << "("
		             << subIterationWorkStateMachineIndex_ << ")" << __E__;
}

//==============================================================================
void CoreSupervisorBase::preStateMachineExecution(unsigned int i)
{
	if(i >= theStateMachineImplementation_.size())
	{
		__SUP_SS__ << "State Machine " << i << " not found!" << __E__;
		__SUP_SS_THROW__;
	}

	theStateMachineImplementation_[i]->VStateMachine::setIterationIndex(
	    RunControlStateMachine::getIterationIndex());
	theStateMachineImplementation_[i]->VStateMachine::setSubIterationIndex(
	    RunControlStateMachine::getSubIterationIndex());
	theStateMachineImplementation_[i]->VStateMachine::setSystemMinReadyForEventGenerationStartIteration(
	    RunControlStateMachine::getMinReadyForEventGenerationStartIteration());

	theStateMachineImplementation_[i]->VStateMachine::clearIterationWork();
	theStateMachineImplementation_[i]->VStateMachine::clearSubIterationWork();

	__SUP_COUT__
	    << "theStateMachineImplementation Iteration "
	    << theStateMachineImplementation_[i]->VStateMachine::getIterationIndex() << "."
	    << theStateMachineImplementation_[i]->VStateMachine::getSubIterationIndex()
	    << __E__;
}

//==============================================================================
void CoreSupervisorBase::postStateMachineExecution(unsigned int i)
{
	if(i >= theStateMachineImplementation_.size())
	{
		__SUP_SS__ << "State Machine " << i << " not found!" << __E__;
		__SUP_SS_THROW__;
	}

	// sub-iteration has priority
	if(theStateMachineImplementation_[i]->VStateMachine::getSubIterationWork())
	{
		subIterationWorkStateMachineIndex_ = i;
		RunControlStateMachine::indicateSubIterationWork();

		__SUP_COUT__ << "State machine " << i
		             << " is flagged for another sub-iteration..." << __E__;
	}
	else
	{
		stateMachinesIterationDone_[i] =
		    !theStateMachineImplementation_[i]->VStateMachine::getIterationWork();

		if(!stateMachinesIterationDone_[i])
		{
			__SUP_COUT__ << "State machine " << i
			             << " is flagged for another iteration..." << __E__;
			RunControlStateMachine::indicateIterationWork();  // mark not done at
			                                                  // CoreSupervisorBase level
			++stateMachinesIterationWorkCount_;  // increment still working count
		}
	}
}  // end postStateMachineExecution()

//==============================================================================
void CoreSupervisorBase::postStateMachineExecutionLoop(void)
{
	if(RunControlStateMachine::subIterationWorkFlag_)
		__SUP_COUT__ << "State machine implementation "
		             << subIterationWorkStateMachineIndex_
		             << " is flagged for another sub-iteration..." << __E__;
	else if(RunControlStateMachine::iterationWorkFlag_)
		__SUP_COUT__
		    << stateMachinesIterationWorkCount_
		    << " state machine implementation(s) flagged for another iteration..."
		    << __E__;
	else
		__SUP_COUT__ << "Done configuration all state machine implementations..."
		             << __E__;
}  // end postStateMachineExecutionLoop()

//==============================================================================
void CoreSupervisorBase::configureInit(void)
{
	// activate the configuration tree (only the first iteration)

	if(!(RunControlStateMachine::getIterationIndex() == 0 &&
	     RunControlStateMachine::getSubIterationIndex() == 0))
		return;

	__SUP_COUT__ << "configureInit()" << __E__;

	std::pair<std::string /*group name*/, TableGroupKey> theGroup(
	    SOAPUtilities::translate(theStateMachine_.getCurrentMessage())
	        .getParameters()
	        .getValue("ConfigurationTableGroupName"),
	    TableGroupKey(SOAPUtilities::translate(theStateMachine_.getCurrentMessage())
	                      .getParameters()
	                      .getValue("ConfigurationTableGroupKey")));

	__SUP_COUT__ << "Configuration table group name: " << theGroup.first
	             << " key: " << theGroup.second << __E__;

	//fill System Variables
	StringMacros::systemVariables_["ActiveStateMachine"]["name"] =
	    SOAPUtilities::translate(theStateMachine_.getCurrentMessage())
	        .getParameters()
	        .getValue("ActiveStateMachineName");
	StringMacros::systemVariables_["ActiveStateMachine"]["windowName"] =
	    SOAPUtilities::translate(theStateMachine_.getCurrentMessage())
	        .getParameters()
	        .getValue("ActiveStateMachineWindowName");
	StringMacros::systemVariables_["ActiveStateMachine"]["runAlias"] =
	    SOAPUtilities::translate(theStateMachine_.getCurrentMessage())
	        .getParameters()
	        .getValue("ActiveStateMachineRunAlias");
	__SUP_COUTV__(StringMacros::mapToString(StringMacros::systemVariables_));

	// Assemble Subsystem Common Table List ----------------
	std::map<std::string /* tableName */, TableVersion> mergeInTables, overrideTables;
	{
		{  //handle common merge-in list
			std::string subsystemCommonList;
			try
			{
				subsystemCommonList =
				    SOAPUtilities::translate(theStateMachine_.getCurrentMessage())
				        .getParameters()
				        .getValue("SubsystemCommonList");
			}
			catch(...)
			{
				__COUT__ << "Ignoring missing 'SubsystemCommonList' parameter." << __E__;
			}

			if(!subsystemCommonList.empty())
			{
				subsystemCommonList =
				    StringMacros::decodeURIComponent(subsystemCommonList);
				__COUT__ << "Transition parameter SubsystemCommonList: "
				         << subsystemCommonList << __E__;
				StringMacros::getMapFromString(subsystemCommonList, mergeInTables);
				__COUTV__(StringMacros::mapToString(mergeInTables));
			}
		}  //end handle common merge-in list

		{  //handle common override list
			std::string subsystemCommonOverrideList;
			try
			{
				subsystemCommonOverrideList =
				    SOAPUtilities::translate(theStateMachine_.getCurrentMessage())
				        .getParameters()
				        .getValue("SubsystemCommonOverrideList");
			}
			catch(...)
			{
				__COUT__ << "Ignoring missing 'SubsystemCommonOverrideList' parameter."
				         << __E__;
			}

			if(!subsystemCommonOverrideList.empty())
			{
				subsystemCommonOverrideList =
				    StringMacros::decodeURIComponent(subsystemCommonOverrideList);
				__COUT__ << "Transition parameter SubsystemCommonOverrideList: "
				         << subsystemCommonOverrideList << __E__;
				StringMacros::getMapFromString(subsystemCommonOverrideList,
				                               overrideTables);
				__COUTV__(StringMacros::mapToString(overrideTables));
			}
		}  //end handle common override list
	}      // end Assemble Subsystem Common Table List ----------------

	try
	{
		ConfigurationManager::ConfigureTransitionGuard configureGuard(
		    theConfigurationManager_);
		//disable version tracking to accept untracked versions to be selected by the FSM transition source
		theConfigurationManager_->loadTableGroup(
		    theGroup.first,
		    theGroup.second,
		    true /*doActivate*/,
		    0 /*groupMembers      */,
		    0 /*progressBar       */,
		    0 /*accumulateWarnings*/,
		    0 /*groupComment      */,
		    0 /*groupAuthor       */,
		    0 /*groupCreateTime   */,
		    false /*doNotLoadMember */,
		    0 /*groupTypeString */,
		    0 /*groupAliases */,
		    ConfigurationManager::LoadGroupType::ALL_TYPES,
		    true /*ignoreVersionTracking*/,
		    mergeInTables /* mergeInTables */,
		    overrideTables /* overrideTables */
		);
	}
	catch(const std::runtime_error& e)
	{
		__SS__ << "Error loading table group '" << theGroup.first << "("
		       << theGroup.second << ")! \n"
		       << e.what() << __E__;
		__SUP_COUT_ERR__ << ss.str();
		// ExceptionHandler(ExceptionHandlerRethrow::no, ss.str());

		//__SS_THROW_ONLY__;
		theStateMachine_.setErrorMessage(ss.str());
		throw toolbox::fsm::exception::Exception(
		    "Transition Error" /*name*/,
		    ss.str() /* message*/,
		    "CoreSupervisorBase::configureInit" /*module*/,
		    __LINE__ /*line*/,
		    __FUNCTION__ /*function*/
		);
	}
	catch(...)
	{
		__SS__ << "Unknown error loading table group '" << theGroup.first << "("
		       << theGroup.second << ")!" << __E__;
		try
		{
			throw;
		}  //one more try to printout extra info
		catch(const std::exception& e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...)
		{
		}
		__SUP_COUT_ERR__ << ss.str();
		// ExceptionHandler(ExceptionHandlerRethrow::no, ss.str());

		//__SS_THROW_ONLY__;
		theStateMachine_.setErrorMessage(ss.str());
		throw toolbox::fsm::exception::Exception(
		    "Transition Error" /*name*/,
		    ss.str() /* message*/,
		    "CoreSupervisorBase::configureInit" /*module*/,
		    __LINE__ /*line*/,
		    __FUNCTION__ /*function*/
		);
	}
}  //end configureInit()

//==============================================================================
void CoreSupervisorBase::transitionConfiguring(toolbox::Event::Reference /*event*/)
{
	__SUP_COUT__ << "transitionConfiguring()" << __E__;

	CoreSupervisorBase::configureInit();

	CoreSupervisorBase::transitionConfiguringFSMs();

	__SUP_COUT__ << "Configured." << __E__;
}  // end transitionConfiguring()

//==============================================================================
void CoreSupervisorBase::transitionConfiguringFSMs()
{
	// Now that the configuration manager has all the necessary configurations,
	//	create all objects that depend on the configuration (the first iteration)

	try
	{
		__SUP_COUT__ << "Configuring all state machine implementations..." << __E__;
		preStateMachineExecutionLoop();
		for(unsigned int i = 0; i < theStateMachineImplementation_.size(); ++i)
		{
			// if one state machine is doing a sub-iteration, then target that one
			if(subIterationWorkStateMachineIndex_ != (unsigned int)-1 &&
			   i != subIterationWorkStateMachineIndex_)
				continue;  // skip those not in the sub-iteration

			if(stateMachinesIterationDone_[i])
				continue;  // skip state machines already done

			preStateMachineExecution(i);
			theStateMachineImplementation_[i]->parentSupervisor_ =
			    this;  // for backwards compatibility, kept out of configure parameters
			theStateMachineImplementation_[i]->configure();  // e.g. for FESupervisor,
			                                                 // this is configure of
			                                                 // FEVInterfacesManager
			postStateMachineExecution(i);
		}
		postStateMachineExecutionLoop();
	}
	catch(const std::runtime_error& e)
	{
		__SUP_SS__ << "Error was caught while configuring: " << e.what() << __E__;
		__SUP_COUT_ERR__ << "\n" << ss.str();
		theStateMachine_.setErrorMessage(ss.str());
		throw toolbox::fsm::exception::Exception(
		    "Transition Error" /*name*/,
		    ss.str() /* message*/,
		    "CoreSupervisorBase::transitionConfiguringFSMs" /*module*/,
		    __LINE__ /*line*/,
		    __FUNCTION__ /*function*/
		);
	}
	catch(...)
	{
		__SUP_SS__
		    << "Unknown error was caught while configuring. Please checked the logs."
		    << __E__;
		try
		{
			throw;
		}  //one more try to printout extra info
		catch(const std::exception& e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...)
		{
		}
		__SUP_COUT_ERR__ << "\n" << ss.str();
		theStateMachine_.setErrorMessage(ss.str());
		throw toolbox::fsm::exception::Exception(
		    "Transition Error" /*name*/,
		    ss.str() /* message*/,
		    "CoreSupervisorBase::transitionConfiguringFSMs" /*module*/,
		    __LINE__ /*line*/,
		    __FUNCTION__ /*function*/
		);
	}
}  // end transitionConfiguringFSMs()

//==============================================================================
/// transitionHalting
///	Ignore errors if coming from Failed state
void CoreSupervisorBase::transitionHalting(toolbox::Event::Reference /*event*/)
{
	const std::string transitionName = "Halting";
	try
	{
		__SUP_COUT__ << transitionName << " all state machine implementations..."
		             << __E__;
		preStateMachineExecutionLoop();
		for(unsigned int i = 0; i < theStateMachineImplementation_.size(); ++i)
		{
			// if one state machine is doing a sub-iteration, then target that one
			if(subIterationWorkStateMachineIndex_ != (unsigned int)-1 &&
			   i != subIterationWorkStateMachineIndex_)
				continue;  // skip those not in the sub-iteration

			if(stateMachinesIterationDone_[i])
				continue;  // skip state machines already done

			preStateMachineExecution(i);
			theStateMachineImplementation_[i]->halt();  // e.g. for FESupervisor, this is
			                                            // transition of
			                                            // FEVInterfacesManager
			postStateMachineExecution(i);
		}
		postStateMachineExecutionLoop();
	}
	catch(const std::runtime_error& e)
	{
		// if halting from Failed or Halted state, then ignore errors
		if(theStateMachine_.getProvenanceStateName() ==
		       RunControlStateMachine::FAILED_STATE_NAME ||
		   theStateMachine_.getProvenanceStateName() ==
		       RunControlStateMachine::HALTED_STATE_NAME)
		{
			__SUP_COUT_INFO__ << "Error was caught while halting (but ignoring because "
			                     "previous state was '"
			                  << theStateMachine_.getProvenanceStateName()
			                  << "'): " << e.what() << __E__;
		}
		else  // if not previously in Failed state, then fail
		{
			__SUP_SS__ << "Error was caught while " << transitionName << ": " << e.what()
			           << __E__;
			__SUP_COUT_ERR__ << "\n" << ss.str();
			theStateMachine_.setErrorMessage(ss.str());
			throw toolbox::fsm::exception::Exception(
			    "Transition Error" /*name*/,
			    ss.str() /* message*/,
			    "CoreSupervisorBase::transition" + transitionName /*module*/,
			    __LINE__ /*line*/,
			    __FUNCTION__ /*function*/
			);
		}
	}
	catch(...)
	{
		// if halting from Failed or Halted state, then ignore errors
		if(theStateMachine_.getProvenanceStateName() ==
		       RunControlStateMachine::FAILED_STATE_NAME ||
		   theStateMachine_.getProvenanceStateName() ==
		       RunControlStateMachine::HALTED_STATE_NAME)
		{
			__SUP_COUT_INFO__ << "Unknown error was caught while halting (but ignoring "
			                     "because previous state was '"
			                  << theStateMachine_.getProvenanceStateName() << "')."
			                  << __E__;
		}
		else  // if not previously in Failed state, then fail
		{
			__SUP_SS__ << "Unknown error was caught while " << transitionName
			           << ". Please checked the logs." << __E__;
			try
			{
				throw;
			}  //one more try to printout extra info
			catch(const std::exception& e)
			{
				ss << "Exception message: " << e.what();
			}
			catch(...)
			{
			}
			__SUP_COUT_ERR__ << "\n" << ss.str();
			theStateMachine_.setErrorMessage(ss.str());
			throw toolbox::fsm::exception::Exception(
			    "Transition Error" /*name*/,
			    ss.str() /* message*/,
			    "CoreSupervisorBase::transition" + transitionName /*module*/,
			    __LINE__ /*line*/,
			    __FUNCTION__ /*function*/
			);
		}
	}
}  // end transitionHalting()

//==============================================================================
/// Inheriting supervisor classes should not override this function, or should at least
/// also call it in the override 	to maintain property functionality.
void CoreSupervisorBase::transitionInitializing(toolbox::Event::Reference /*event*/)
{
	__SUP_COUT__ << "transitionInitializing" << __E__;

	CorePropertySupervisorBase::resetPropertiesAreSetup();  // indicate need to re-load
	                                                        // user properties

	// Note: Do not initialize the state machine implementations...
	//	do any initializing in configure
	//	This allows re-instantiation at each configure time.
	// for(auto& it: theStateMachineImplementation_)
	// it->initialize();
}  // end transitionInitializing()

//==============================================================================
void CoreSupervisorBase::transitionPausing(toolbox::Event::Reference /*event*/)
{
	const std::string transitionName = "Pausing";
	try
	{
		__SUP_COUT__ << "Configuring all state machine implementations..." << __E__;
		preStateMachineExecutionLoop();
		for(unsigned int i = 0; i < theStateMachineImplementation_.size(); ++i)
		{
			// if one state machine is doing a sub-iteration, then target that one
			if(subIterationWorkStateMachineIndex_ != (unsigned int)-1 &&
			   i != subIterationWorkStateMachineIndex_)
				continue;  // skip those not in the sub-iteration

			if(stateMachinesIterationDone_[i])
				continue;  // skip state machines already done

			preStateMachineExecution(i);
			theStateMachineImplementation_[i]->pause();  // e.g. for FESupervisor, this is
			                                             // transition of
			                                             // FEVInterfacesManager
			postStateMachineExecution(i);
		}
		postStateMachineExecutionLoop();
	}
	catch(const std::runtime_error& e)
	{
		__SUP_SS__ << "Error was caught while " << transitionName << ": " << e.what()
		           << __E__;
		__SUP_COUT_ERR__ << "\n" << ss.str();
		theStateMachine_.setErrorMessage(ss.str());
		throw toolbox::fsm::exception::Exception(
		    "Transition Error" /*name*/,
		    ss.str() /* message*/,
		    "CoreSupervisorBase::transition" + transitionName /*module*/,
		    __LINE__ /*line*/,
		    __FUNCTION__ /*function*/
		);
	}
	catch(...)
	{
		__SUP_SS__ << "Unknown error was caught while " << transitionName
		           << ". Please checked the logs." << __E__;
		try
		{
			throw;
		}  //one more try to printout extra info
		catch(const std::exception& e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...)
		{
		}
		__SUP_COUT_ERR__ << "\n" << ss.str();
		theStateMachine_.setErrorMessage(ss.str());
		throw toolbox::fsm::exception::Exception(
		    "Transition Error" /*name*/,
		    ss.str() /* message*/,
		    "CoreSupervisorBase::transition" + transitionName /*module*/,
		    __LINE__ /*line*/,
		    __FUNCTION__ /*function*/
		);
	}
}  // end transitionPausing()

//==============================================================================
void CoreSupervisorBase::transitionResuming(toolbox::Event::Reference /*event*/)
{
	const std::string transitionName = "Resuming";
	try
	{
		__SUP_COUT__ << "Configuring all state machine implementations..." << __E__;
		preStateMachineExecutionLoop();
		for(unsigned int i = 0; i < theStateMachineImplementation_.size(); ++i)
		{
			// if one state machine is doing a sub-iteration, then target that one
			if(subIterationWorkStateMachineIndex_ != (unsigned int)-1 &&
			   i != subIterationWorkStateMachineIndex_)
				continue;  // skip those not in the sub-iteration

			if(stateMachinesIterationDone_[i])
				continue;  // skip state machines already done

			preStateMachineExecution(i);
			theStateMachineImplementation_[i]->resume();  // e.g. for FESupervisor, this
			                                              // is transition of
			                                              // FEVInterfacesManager
			postStateMachineExecution(i);
		}
		postStateMachineExecutionLoop();
	}
	catch(const std::runtime_error& e)
	{
		__SUP_SS__ << "Error was caught while " << transitionName << ": " << e.what()
		           << __E__;
		__SUP_COUT_ERR__ << "\n" << ss.str();
		theStateMachine_.setErrorMessage(ss.str());
		throw toolbox::fsm::exception::Exception(
		    "Transition Error" /*name*/,
		    ss.str() /* message*/,
		    "CoreSupervisorBase::transition" + transitionName /*module*/,
		    __LINE__ /*line*/,
		    __FUNCTION__ /*function*/
		);
	}
	catch(...)
	{
		__SUP_SS__ << "Unknown error was caught while " << transitionName
		           << ". Please checked the logs." << __E__;
		try
		{
			throw;
		}  //one more try to printout extra info
		catch(const std::exception& e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...)
		{
		}
		__SUP_COUT_ERR__ << "\n" << ss.str();
		theStateMachine_.setErrorMessage(ss.str());
		throw toolbox::fsm::exception::Exception(
		    "Transition Error" /*name*/,
		    ss.str() /* message*/,
		    "CoreSupervisorBase::transition" + transitionName /*module*/,
		    __LINE__ /*line*/,
		    __FUNCTION__ /*function*/
		);
	}
}  // end transitionResuming()

//==============================================================================
void CoreSupervisorBase::transitionStarting(toolbox::Event::Reference /*event*/)
{
	const std::string transitionName = "Starting";
	const std::string runNumber =
	    SOAPUtilities::translate(theStateMachine_.getCurrentMessage())
	        .getParameters()
	        .getValue("RunNumber");
	try
	{
		__SUP_COUT__ << "Configuring all state machine implementations..." << __E__;
		preStateMachineExecutionLoop();
		for(unsigned int i = 0; i < theStateMachineImplementation_.size(); ++i)
		{
			// if one state machine is doing a sub-iteration, then target that one
			if(subIterationWorkStateMachineIndex_ != (unsigned int)-1 &&
			   i != subIterationWorkStateMachineIndex_)
				continue;  // skip those not in the sub-iteration

			if(stateMachinesIterationDone_[i])
				continue;  // skip state machines already done

			preStateMachineExecution(i);
			theStateMachineImplementation_[i]->start(runNumber);  // e.g. for
			                                                      // FESupervisor, this is
			                                                      // transition of
			                                                      // FEVInterfacesManager
			postStateMachineExecution(i);
		}
		postStateMachineExecutionLoop();
	}
	catch(const std::runtime_error& e)
	{
		__SUP_SS__ << "Error was caught while " << transitionName << ": " << e.what()
		           << __E__;
		__SUP_COUT_ERR__ << "\n" << ss.str();
		theStateMachine_.setErrorMessage(ss.str());
		throw toolbox::fsm::exception::Exception(
		    "Transition Error" /*name*/,
		    ss.str() /* message*/,
		    "CoreSupervisorBase::transition" + transitionName /*module*/,
		    __LINE__ /*line*/,
		    __FUNCTION__ /*function*/
		);
	}
	catch(...)
	{
		__SUP_SS__ << "Unknown error was caught while " << transitionName
		           << ". Please checked the logs." << __E__;
		try
		{
			throw;
		}  //one more try to printout extra info
		catch(const std::exception& e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...)
		{
		}
		__SUP_COUT_ERR__ << "\n" << ss.str();
		theStateMachine_.setErrorMessage(ss.str());
		throw toolbox::fsm::exception::Exception(
		    "Transition Error" /*name*/,
		    ss.str() /* message*/,
		    "CoreSupervisorBase::transition" + transitionName /*module*/,
		    __LINE__ /*line*/,
		    __FUNCTION__ /*function*/
		);
	}
}  // end transitionStarting()

//==============================================================================
void CoreSupervisorBase::transitionStopping(toolbox::Event::Reference /*event*/)
{
	const std::string transitionName = "Stopping";
	try
	{
		__SUP_COUT__ << "Configuring all state machine implementations..." << __E__;
		preStateMachineExecutionLoop();
		for(unsigned int i = 0; i < theStateMachineImplementation_.size(); ++i)
		{
			// if one state machine is doing a sub-iteration, then target that one
			if(subIterationWorkStateMachineIndex_ != (unsigned int)-1 &&
			   i != subIterationWorkStateMachineIndex_)
				continue;  // skip those not in the sub-iteration

			if(stateMachinesIterationDone_[i])
				continue;  // skip state machines already done

			preStateMachineExecution(i);
			theStateMachineImplementation_[i]->stop();  // e.g. for FESupervisor, this is
			                                            // transition of
			                                            // FEVInterfacesManager
			postStateMachineExecution(i);
		}
		postStateMachineExecutionLoop();
	}
	catch(const std::runtime_error& e)
	{
		__SUP_SS__ << "Error was caught while " << transitionName << ": " << e.what()
		           << __E__;
		__SUP_COUT_ERR__ << "\n" << ss.str();
		theStateMachine_.setErrorMessage(ss.str());
		throw toolbox::fsm::exception::Exception(
		    "Transition Error" /*name*/,
		    ss.str() /* message*/,
		    "CoreSupervisorBase::transition" + transitionName /*module*/,
		    __LINE__ /*line*/,
		    __FUNCTION__ /*function*/
		);
	}
	catch(...)
	{
		__SUP_SS__ << "Unknown error was caught while " << transitionName
		           << ". Please checked the logs." << __E__;
		try
		{
			throw;
		}  //one more try to printout extra info
		catch(const std::exception& e)
		{
			ss << "Exception message: " << e.what();
		}
		catch(...)
		{
		}
		__SUP_COUT_ERR__ << "\n" << ss.str();
		theStateMachine_.setErrorMessage(ss.str());
		throw toolbox::fsm::exception::Exception(
		    "Transition Error" /*name*/,
		    ss.str() /* message*/,
		    "CoreSupervisorBase::transition" + transitionName /*module*/,
		    __LINE__ /*line*/,
		    __FUNCTION__ /*function*/
		);
	}
}  // end transitionStopping()

//==============================================================================
/// SendAsyncErrorToGateway
///	Static -- thread
///	Send async error or soft error to gateway
///	Call this as thread so that parent calling function (workloop) can end.
void CoreSupervisorBase::sendAsyncExceptionToGateway(const std::string& errorMessage,
                                                     bool               isPauseException,
                                                     bool               isStopException)
try
{
	if(isStopException)
		__SUP_COUT_ERR__ << "Sending Supervisor Async STOP Running Exception... \n"
		                 << errorMessage << __E__;
	else if(isPauseException)
		__SUP_COUT_ERR__ << "Sending Supervisor Async SOFT Running Error... \n"
		                 << errorMessage << __E__;
	else
		__SUP_COUT_ERR__ << "Sending Supervisor Async Running Error... \n"
		                 << errorMessage << __E__;

	theStateMachine_.setErrorMessage(errorMessage);

	XDAQ_CONST_CALL xdaq::ApplicationDescriptor* gatewaySupervisor =
	    allSupervisorInfo_.getGatewayInfo().getDescriptor();

	SOAPParameters parameters;
	parameters.addParameter("ErrorMessage", errorMessage);

	xoap::MessageReference replyMessage = SOAPMessenger::sendWithSOAPReply(
	    gatewaySupervisor,
	    isStopException ? "AsyncStopException"
	                    : (isPauseException ? "AsyncPauseException" : "AsyncError"),
	    parameters);

	std::stringstream replyMessageSStream;
	replyMessageSStream << SOAPUtilities::translate(replyMessage);
	__SUP_COUT__ << "Received... " << replyMessageSStream.str() << std::endl;

	if(replyMessageSStream.str().find("Fault") != std::string::npos)
	{
		__SUP_COUT_ERR__ << "Failure to indicate fault to Gateway..." << __E__;
		throw;
	}
}
catch(const xdaq::exception::Exception& e)
{
	if(isStopException)
		__SUP_COUT__
		    << "SOAP message failure indicating Supervisor asynchronous running STOP "
		       "exception back to Gateway: "
		    << e.what() << __E__;
	else if(isPauseException)
		__SUP_COUT__
		    << "SOAP message failure indicating Supervisor asynchronous running SOFT "
		       "exception back to Gateway: "
		    << e.what() << __E__;
	else
		__SUP_COUT__ << "SOAP message failure indicating Supervisor asynchronous running "
		                "error back to Gateway: "
		             << e.what() << __E__;
	throw;  // rethrow and hope error is noticed
}
catch(...)
{
	if(isStopException)
		__SUP_COUT__
		    << "Unknown error encounter indicating Supervisor asynchronous running "
		       "STOP exception back to Gateway."
		    << __E__;
	else if(isPauseException)
		__SUP_COUT__
		    << "Unknown error encounter indicating Supervisor asynchronous running "
		       "SOFT error back to Gateway."
		    << __E__;
	else
		__SUP_COUT__
		    << "Unknown error encounter indicating Supervisor asynchronous running "
		       "error back to Gateway."
		    << __E__;
	throw;  // rethrow and hope error is noticed
}  // end SendAsyncErrorToGateway()

//==============================================================================
xoap::MessageReference CoreSupervisorBase::TRACESupervisorRequest(
    xoap::MessageReference message)
{
	return CorePropertySupervisorBase::TRACESupervisorRequest(message);
}  // end TRACESupervisorRequest()
