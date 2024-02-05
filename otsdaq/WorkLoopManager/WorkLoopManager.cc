#include "otsdaq/WorkLoopManager/WorkLoopManager.h"
#include "otsdaq/Macros/CoutMacros.h"
#include "otsdaq/MessageFacility/MessageFacility.h"
#include "otsdaq/SOAPUtilities/SOAPCommand.h"
#include "otsdaq/XmlUtilities/HttpXmlDocument.h"

#include <toolbox/task/WorkLoopFactory.h>

#include <cassert>
#include <cstdlib>
#include <iostream>
#include <sstream>

using namespace ots;

//==============================================================================
WorkLoopManager::WorkLoopManager(toolbox::task::ActionSignature* job) : cWorkLoopType_("waiting"), job_(job), requestNumber_(0), requestName_(job_->name())
{
	__COUT__ << "Request name: " << requestName_ << " jobName: " << job_->name() << std::endl;
}

//==============================================================================
WorkLoopManager::~WorkLoopManager(void) {}

//==============================================================================
HttpXmlDocument WorkLoopManager::processRequest(cgicc::Cgicc& cgi) { return processRequest(composeWorkLoopName(requestNumber_++, cgi)); }

//==============================================================================
HttpXmlDocument WorkLoopManager::processRequest(const xoap::MessageReference& message)
{
	return processRequest(composeWorkLoopName(requestNumber_++, message), &message);
}

//==============================================================================
HttpXmlDocument WorkLoopManager::processRequest(std::string workLoopName, const xoap::MessageReference* message)
{
	HttpXmlDocument   xmlDocument;
	std::stringstream requestNumberStream;
	RequestNumber     requestNumber = getWorkLoopRequestNumber(workLoopName);
	requestNumberStream << requestNumber;
	std::stringstream reportStream;

	__COUT__ << "Processing request! WorkLoops: " << workLoops_.size() << " req: " << requestNumber << std::endl;
	if(workLoops_.size() >= maxWorkLoops)
	{
		if(!removeTimedOutRequests())
		{
			reportStream << "Too many running requests (" << workLoops_.size() << "), try later!" << std::endl;
			__COUT__ << "ERROR: " << reportStream.str() << std::endl;

			xmlDocument.addTextElementToData("RequestStatus", "ERROR");
			xmlDocument.addTextElementToData("RequestName", getWorkLoopRequest(workLoopName));
			xmlDocument.addTextElementToData("RequestNumber", requestNumberStream.str());
			xmlDocument.addTextElementToData("ErrorReport", reportStream.str());
			return xmlDocument;
		}
	}
	time(&(workLoops_[requestNumber].requestStartTime));
	workLoops_[requestNumber].requestLastTimeChecked = workLoops_[requestNumber].requestStartTime;
	workLoops_[requestNumber].request                = getWorkLoopRequest(workLoopName);
	workLoops_[requestNumber].result                 = "EMPTY";
	workLoops_[requestNumber].progress               = 0;
	workLoops_[requestNumber].done                   = false;
	workLoops_[requestNumber].workLoopName           = workLoopName;
	if(message != 0)
		workLoops_[requestNumber].message = *message;
	try
	{
		workLoops_[requestNumber].workLoop = toolbox::task::getWorkLoopFactory()->getWorkLoop(workLoopName, cWorkLoopType_);
	}
	catch(xcept::Exception& e)
	{
		reportStream << "Can't create workloop, try again." << std::endl;
		__COUT__ << "ERROR: " << reportStream.str() << std::endl;
		xmlDocument.addTextElementToData("RequestStatus", "ERROR");
		xmlDocument.addTextElementToData("RequestName", getWorkLoopRequest(workLoopName));
		xmlDocument.addTextElementToData("RequestNumber", requestNumberStream.str());
		xmlDocument.addTextElementToData("ErrorReport", reportStream.str());
		removeWorkLoop(requestNumber);
		return xmlDocument;
	}

	try
	{
		workLoops_[requestNumber].workLoop->submit(job_);
	}
	catch(xcept::Exception& e)
	{
		reportStream << "Can't submit workloop job, try again." << std::endl;
		__COUT__ << "ERROR: " << reportStream.str() << std::endl;
		xmlDocument.addTextElementToData("RequestStatus", "ERROR");
		xmlDocument.addTextElementToData("RequestName", getWorkLoopRequest(workLoopName));
		xmlDocument.addTextElementToData("RequestNumber", requestNumberStream.str());
		xmlDocument.addTextElementToData("ErrorReport", reportStream.str());
		removeWorkLoop(requestNumber);
		return xmlDocument;
	}
	try
	{
		workLoops_[requestNumber].workLoop->activate();
	}
	catch(xcept::Exception& e)
	{
		reportStream << "Can't activate workloop, try again." << std::endl;
		__COUT__ << "ERROR: " << reportStream.str() << std::endl;
		xmlDocument.addTextElementToData("RequestStatus", "ERROR");
		xmlDocument.addTextElementToData("RequestName", getWorkLoopRequest(workLoopName));
		xmlDocument.addTextElementToData("RequestNumber", requestNumberStream.str());
		xmlDocument.addTextElementToData("ErrorReport", reportStream.str());
		removeWorkLoop(requestNumber);
		return xmlDocument;
	}

	__COUT__ << "SUCCESS: Request is being processed!" << std::endl;

	xmlDocument.addTextElementToData("RequestStatus", "SUCCESS");
	xmlDocument.addTextElementToData("RequestName", getWorkLoopRequest(workLoopName));
	xmlDocument.addTextElementToData("RequestNumber", requestNumberStream.str());

	return xmlDocument;
}

//==============================================================================
bool WorkLoopManager::report(toolbox::task::WorkLoop* workLoop, std::string result, float progress, bool status)
{
	RequestNumber requestNumber = getWorkLoopRequestNumber(workLoop);

	/*
	    __COUT__ << "Reporting result for request " << getWorkLoopRequest(workLoop) << "
	   with req#: " << requestNumber << std::endl; if(workLoops_.find(requestNumber) ==
	   workLoops_.end())
	    {
	        __COUT__ << "This MUST NEVER happen. You must find out what is wrong!" <<
	   std::endl; assert(0);
	    }
	*/
	workLoops_[requestNumber].result   = result;
	workLoops_[requestNumber].progress = progress;
	workLoops_[requestNumber].done     = status;
	return true;
}

//==============================================================================
xoap::MessageReference WorkLoopManager::getMessage(toolbox::task::WorkLoop* workLoop)
{
	RequestNumber requestNumber = getWorkLoopRequestNumber(workLoop);
	return workLoops_[requestNumber].message;
}

//==============================================================================
bool WorkLoopManager::getRequestResult(cgicc::Cgicc& cgi, HttpXmlDocument& xmlDocument)
{
	std::stringstream reportStream;
	std::string       requestNumberString = cgi.getElement("RequestNumber")->getValue();
	RequestNumber     requestNumber       = strtoull(requestNumberString.c_str(), 0, 10);

	__COUT__ << "Request: " << requestName_ << " RequestNumber=" << requestNumberString << " assigned # " << requestNumber << std::endl;

	if(workLoops_.find(requestNumber) == workLoops_.end())
	{
		reportStream << "Can't find request " << requestNumber << " within the currently active " << workLoops_.size() << " requests!";
		__COUT__ << "WARNING: " << reportStream.str() << std::endl;
		xmlDocument.addTextElementToData("RequestStatus", "WARNING");
		xmlDocument.addTextElementToData("RequestName", "UNKNOWN");
		xmlDocument.addTextElementToData("RequestNumber", requestNumberString);
		xmlDocument.addTextElementToData("ErrorReport", reportStream.str());
		return false;
	}

	if(!workLoops_[requestNumber].done)
	{
		reportStream << "Still processing request " << requestNumber;
		__COUT__ << "WARNING: " << reportStream.str() << std::endl;
		// Resetting timer since there is a listener
		time(&(workLoops_[requestNumber].requestLastTimeChecked));

		xmlDocument.addTextElementToData("RequestStatus", "MESSAGE");
		xmlDocument.addTextElementToData("RequestName", workLoops_[requestNumber].request);
		xmlDocument.addTextElementToData("RequestNumber", requestNumberString);
		xmlDocument.addTextElementToData("ErrorReport", reportStream.str());
		std::stringstream progress;
		progress << workLoops_[requestNumber].progress;
		xmlDocument.addTextElementToData("RequestProgress", progress.str());
		return false;
	}

	// Request done and ready to be retrieved so I can remove it from the queue
	xmlDocument.addTextElementToData("RequestStatus", workLoops_[requestNumber].result);
	xmlDocument.addTextElementToData("RequestName", workLoops_[requestNumber].request);
	xmlDocument.addTextElementToData("RequestNumber", requestNumberString);
	std::stringstream progress;
	progress << workLoops_[requestNumber].progress;
	xmlDocument.addTextElementToData("RequestProgress", progress.str());

	removeWorkLoop(requestNumber);
	return true;
}

//==============================================================================
bool WorkLoopManager::removeWorkLoop(toolbox::task::WorkLoop* workLoop) { return removeWorkLoop(getWorkLoopRequestNumber(workLoop)); }

//==============================================================================
bool WorkLoopManager::removeWorkLoop(RequestNumber requestNumber)
{
	if(workLoops_.find(requestNumber) == workLoops_.end())
	{
		__COUT__ << "WorkLoop " << requestNumber << " is not in the WorkLoops list!" << std::endl;
		return false;
	}

	if(workLoops_[requestNumber].workLoop == 0)
	{
		__COUT__ << "WorkLoop " << requestNumber << " was not created at all!" << std::endl;
		workLoops_.erase(requestNumber);
		return false;
	}

	try
	{
		workLoops_[requestNumber].workLoop->cancel();
	}
	catch(xcept::Exception& e)
	{
		__COUT__ << "Can't cancel WorkLoop " << requestNumber << std::endl;
		// diagService_->reportError("Failed to start Job Control monitoring. Exception:
		// "+string(e.what()),DIAGWARN);
		return false;
	}
	try
	{
		workLoops_[requestNumber].workLoop->remove(job_);
	}
	catch(xcept::Exception& e)
	{
		// ATTENTION!
		// If the workloop job thread returns false, then the workloop job is
		// automatically removed and it can't be removed again Leaving this try catch
		// allows me to be general in the job threads so I can return true (repeat loop)
		// or false ( loop only once) without crashing
		// __COUT__ << "WARNING: Can't remove request WorkLoop: " << requestNumber <<
		// std::endl;
	}
	__COUT__ << "Deleting WorkLoop " << requestNumber << std::endl;
	toolbox::task::getWorkLoopFactory()->removeWorkLoop(workLoops_[requestNumber].workLoopName,
	                                                    cWorkLoopType_);  // delete workLoops_[requestNumber].workLoop; is done by the
	                                                                      // factory
	workLoops_.erase(requestNumber);
	return true;
}

//==============================================================================
bool WorkLoopManager::removeProcessedRequests(void)
{
	std::map<RequestNumber, WorkLoopStruct>::iterator it = workLoops_.begin();
	while(it != workLoops_.end())
		if(it->second.done)
			removeWorkLoop((it++)->first);
		else
			++it;
	return true;
}

//==============================================================================
bool WorkLoopManager::removeTimedOutRequests(void)
{
	time_t now;
	time(&now);
	std::map<RequestNumber, WorkLoopStruct>::iterator it = workLoops_.begin();
	__COUT__ << "Removing timed out " << std::endl;
	for(; it != workLoops_.end(); it++)
		if(it->second.done && difftime(now, it->second.requestLastTimeChecked) > timeOutInSeconds)
		{
			__COUT__ << "Removing timed out request #" << it->first << " after " << difftime(now, it->second.requestLastTimeChecked) << " seconds."
			         << std::endl;
			removeWorkLoop(it->first);
			return true;  // since I return I don't care if the iterator pointer is
			              // screwed after I erase the element
		}
	__COUT__ << "Done Removing timed out " << std::endl;
	return false;
}

// WorkLoopName Format:
// RequestNumber-CommandToExecute<Argument1Name:Argument1Value,Argument2Name:Argument2Value...>
// Then the WorkLoop adds at the end /waiting
//==============================================================================
std::string WorkLoopManager::composeWorkLoopName(RequestNumber requestNumber, cgicc::Cgicc& cgi)
{
	std::stringstream name;
	name << requestNumber << "-" << cgi.getElement(requestName_)->getValue();
	__COUT__ << "Request: " << requestName_ << " Value=" << cgi.getElement(requestName_)->getValue() << " WLName: " << name.str() << std::endl;
	return name.str();
}

//==============================================================================
std::string WorkLoopManager::composeWorkLoopName(RequestNumber requestNumber, const xoap::MessageReference& message)
{
	SOAPCommand       soapCommand(message);
	std::stringstream name;
	name << requestNumber << "-" << soapCommand.getCommand();
	__COUT__ << "Request: " << requestName_ << " Value=" << soapCommand.getCommand() << " WLName: " << name.str() << std::endl;
	if(soapCommand.hasParameters())
	{
		name << '<';
		char separator = ',';
		for(SOAPParameters::const_iterator it = soapCommand.getParameters().begin(); it != soapCommand.getParameters().end(); it++)
		{
			if(it != soapCommand.getParameters().begin())
				name << separator;
			name << it->first << "|" << it->second;
		}
		name << '>';
	}
	return name.str();
}

//==============================================================================
WorkLoopManager::RequestNumber WorkLoopManager::getWorkLoopRequestNumber(toolbox::task::WorkLoop* workLoop)
{
	return getWorkLoopRequestNumber(workLoop->getName());
}

//==============================================================================
WorkLoopManager::RequestNumber WorkLoopManager::getWorkLoopRequestNumber(std::string workLoopName)
{
	workLoopName = workLoopName.substr(0, workLoopName.find('-'));
	return strtoull(workLoopName.c_str(), 0, 10);
}

//==============================================================================
std::string WorkLoopManager::getWorkLoopRequest(toolbox::task::WorkLoop* workLoop) { return getWorkLoopRequest(workLoop->getName()); }

//==============================================================================
std::string WorkLoopManager::getWorkLoopRequest(std::string workLoopName)
{
	return workLoopName.substr(workLoopName.find('-') + 1, workLoopName.find(std::string("/") + cWorkLoopType_) - workLoopName.find('-') - 1);
}

//==============================================================================
void WorkLoopManager::translateWorkLoopName(toolbox::task::WorkLoop* workLoop, SOAPCommand& soapCommand)
{
	std::string request = getWorkLoopRequest(workLoop);
	if(request.find('<') == std::string::npos)
	{
		__COUT__ << "Simple request" << std::endl;
		soapCommand.setCommand(request);
	}
	else
	{
		size_t ltPosition = request.find('<');
		size_t gtPosition = request.find('>');
		size_t begin      = ltPosition + 1;
		size_t orPosition;
		size_t commaPosition;

		soapCommand.setCommand(request.substr(0, ltPosition));
		while((commaPosition = request.find(',', begin)) != std::string::npos)
		{
			orPosition = request.find('|', begin);
			soapCommand.setParameter(request.substr(begin, orPosition - begin), request.substr(orPosition + 1, commaPosition - orPosition - 1));
			begin = commaPosition + 1;
			__COUT__ << "Comma: " << commaPosition << std::endl;
		}
		orPosition = request.find('|', begin);
		soapCommand.setParameter(request.substr(begin, orPosition - begin), request.substr(orPosition + 1, gtPosition - orPosition - 1));
	}
	__COUT__ << soapCommand << std::endl;
	//__COUT__ << name.substr(name.find(',')+1,name.find('/')-name.find(',')-1) <<
	// std::endl;
}
