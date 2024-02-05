#ifndef _ots_WorkLoopManager_h
#define _ots_WorkLoopManager_h

#include <toolbox/task/WorkLoop.h>
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunknown-pragmas"
#include <xgi/Method.h>  //for cgicc::Cgicc
#pragma GCC diagnostic pop
#include <xoap/MessageReference.h>

#include <ctime>
#include <map>
#include <string>

namespace ots
{
class HttpXmlDocument;
class SOAPCommand;

class WorkLoopManager
{
  public:
	typedef unsigned long long RequestNumber;

	WorkLoopManager(toolbox::task::ActionSignature* job);
	~WorkLoopManager(void);

	// All requests that can change the request map like
	// forceDeleteAllRequests, deleteAllInactiveRequests, deleteAllTimedOutRequests
	// forceThisRequest -> even if there are already maxWorkLoops this request must be
	// accepted!!
	// and more...
	// MUST BE HANDLED LIKE ALL THE OTHER REQUESTS!!!!!! THROUGH THE processRequest
	// Method!
	HttpXmlDocument processRequest(cgicc::Cgicc& cgi);
	HttpXmlDocument processRequest(const xoap::MessageReference& message);
	bool            report(toolbox::task::WorkLoop* workLoop, std::string result, float progress, bool status);
	bool            removeProcessedRequests(void);
	std::string     getWorkLoopRequest(toolbox::task::WorkLoop* workLoop);
	void            translateWorkLoopName(toolbox::task::WorkLoop* workLoop, SOAPCommand& soapCommand);

	// Getters
	bool                   getRequestResult(cgicc::Cgicc& cgi, HttpXmlDocument& xmldoc);
	xoap::MessageReference getMessage(toolbox::task::WorkLoop* workLoop);

  private:
	const std::string    cWorkLoopType_;
	static constexpr int maxWorkLoops     = 5;
	static constexpr int timeOutInSeconds = 20;
	struct WorkLoopStruct
	{
		toolbox::task::WorkLoop* workLoop;
		std::string              workLoopName;
		std::string              request;
		std::string              result;
		bool                     done;
		float                    progress;
		time_t                   requestStartTime;
		time_t                   requestLastTimeChecked;
		xoap::MessageReference   message;
	};
	HttpXmlDocument processRequest(std::string workLoopName, const xoap::MessageReference* message = 0);
	bool            removeWorkLoop(toolbox::task::WorkLoop* workLoop);
	bool            removeWorkLoop(RequestNumber requestNumber);
	bool            removeTimedOutRequests(void);
	std::string     composeWorkLoopName(RequestNumber requestNumber, cgicc::Cgicc& cgi);
	std::string     composeWorkLoopName(RequestNumber requestNumber, const xoap::MessageReference& message);
	RequestNumber   getWorkLoopRequestNumber(toolbox::task::WorkLoop* workLoop);
	RequestNumber   getWorkLoopRequestNumber(std::string workLoopName);  // This can only be
	                                                                     // used by the class
	                                                                     // because it is
	                                                                     // careful to use
	                                                                     // the right format
	std::string getWorkLoopRequest(std::string workLoopName);            // This can only be used by
	                                                                     // the class because it is
	                                                                     // careful to use the right
	                                                                     // format

	std::map<RequestNumber, WorkLoopStruct> workLoops_;
	toolbox::task::ActionSignature*         job_;
	unsigned long long                      requestNumber_;
	std::string                             requestName_;
};

}  // namespace ots

#endif
